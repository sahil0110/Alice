#include "mediactrl.h"

// Pull in Windows headers before any Qt headers that may redefine things.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

// Core Audio (WASAPI)
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>

#include <QString>
#include <QStringList>
#include <QSet>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Return the lowercase filename (e.g. "chrome.exe") for a process ID.
static QString exeNameForPid(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD sz = MAX_PATH;
    QueryFullProcessImageNameW(hProc, 0, buf, &sz);
    CloseHandle(hProc);
    QString full = QString::fromWCharArray(buf);
    int slash = full.lastIndexOf(QLatin1Char('\\'));
    return (slash >= 0 ? full.mid(slash + 1) : full).toLower();
}

// EnumWindows callback data — finds the first visible, unowned window for a PID.
struct FindWndData { DWORD pid; HWND hwnd; };

static BOOL CALLBACK findWindowForPid(HWND hwnd, LPARAM lp)
{
    auto *d = reinterpret_cast<FindWndData *>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == d->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        d->hwnd = hwnd;
        return FALSE; // stop enumeration
    }
    return TRUE;
}

// Return the parent PID of a given process using an existing snapshot.
static DWORD parentPidForPid(DWORD pid, HANDLE snap)
{
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid)
                return pe.th32ParentProcessID;
        } while (Process32NextW(snap, &pe));
    }
    return 0;
}

// Return the main visible window handle for a named executable (case-insensitive).
static HWND windowForExe(const QString &exeName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    HWND found = nullptr;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile).toLower() == exeName.toLower()) {
                FindWndData d{pe.th32ProcessID, nullptr};
                EnumWindows(findWindowForPid, reinterpret_cast<LPARAM>(&d));
                if (d.hwnd) { found = d.hwnd; break; }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

// Iterate every session on the default render endpoint, calling fn(ctrl2, state, pid).
// COM must already be initialised on the calling thread.
template <typename Fn>
static void forEachAudioSession(Fn &&fn)
{
    IMMDeviceEnumerator *pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void **>(&pEnum));
    if (FAILED(hr) || !pEnum) return;

    IMMDevice *pDevice = nullptr;
    pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (!pDevice) { pEnum->Release(); return; }

    IAudioSessionManager2 *pMgr = nullptr;
    pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                      reinterpret_cast<void **>(&pMgr));
    if (!pMgr) { pDevice->Release(); pEnum->Release(); return; }

    IAudioSessionEnumerator *pSessEnum = nullptr;
    pMgr->GetSessionEnumerator(&pSessEnum);
    if (!pSessEnum) { pMgr->Release(); pDevice->Release(); pEnum->Release(); return; }

    int count = 0;
    pSessEnum->GetCount(&count);
    for (int i = 0; i < count; ++i) {
        IAudioSessionControl *pCtrl = nullptr;
        pSessEnum->GetSession(i, &pCtrl);
        if (!pCtrl) continue;

        IAudioSessionControl2 *pCtrl2 = nullptr;
        pCtrl->QueryInterface(__uuidof(IAudioSessionControl2),
                              reinterpret_cast<void **>(&pCtrl2));
        if (pCtrl2) {
            AudioSessionState state = AudioSessionStateInactive;
            pCtrl2->GetState(&state);
            DWORD pid = 0;
            pCtrl2->GetProcessId(&pid);
            fn(pCtrl2, state, pid);
            pCtrl2->Release();
        }
        pCtrl->Release();
    }
    pSessEnum->Release();
    pMgr->Release();
    pDevice->Release();
    pEnum->Release();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

namespace MediaCtrl {

QStringList pauseActiveBrowsers()
{
    // Known browser main-process exe names (all lowercase).
    // Chromium-family browsers (Chrome, Edge, Brave, Opera) spawn renderer
    // subprocesses that own the WASAPI audio sessions.  Those renderers share
    // the same exe name but have no visible window — we must find the browser's
    // *main* process window instead.
    static const QStringList browsers = {
        "chrome.exe", "msedge.exe", "firefox.exe",
        "brave.exe",  "opera.exe",  "opera_gx.exe"
    };

    // S_OK  → we initialised COM; we must call CoUninitialize.
    // S_FALSE → COM was already initialised on this thread (e.g. by QtWebEngine);
    //           do NOT call CoUninitialize or we decrement the ref-count and break it.
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comOwned = (comHr == S_OK);

    // ── Stage 1: collect the set of browser exe names that are actively playing ──
    // Take one process snapshot upfront so we can resolve parent PIDs cheaply.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    QSet<QString> activeBrowsers;

    forEachAudioSession([&](IAudioSessionControl2 * /*ctrl*/, AudioSessionState state, DWORD pid) {
        if (state != AudioSessionStateActive) return;

        QString exe = exeNameForPid(pid);

        if (browsers.contains(exe, Qt::CaseInsensitive)) {
            // Direct match — this PID is the browser main process (e.g. Firefox).
            activeBrowsers.insert(exe.toLower());
        } else if (snap != INVALID_HANDLE_VALUE) {
            // Renderer subprocess (Chromium family): look up the parent process.
            DWORD parentPid = parentPidForPid(pid, snap);
            if (parentPid) {
                QString parentExe = exeNameForPid(parentPid);
                if (browsers.contains(parentExe, Qt::CaseInsensitive))
                    activeBrowsers.insert(parentExe.toLower());
            }
        }
    });

    if (snap != INVALID_HANDLE_VALUE)
        CloseHandle(snap);

    // ── Stage 2: for each active browser, find its main window and send pause ──
    QStringList paused;
    for (const QString &exe : std::as_const(activeBrowsers)) {
        HWND hwnd = windowForExe(exe);
        if (hwnd) {
            SendMessageW(hwnd, WM_APPCOMMAND,
                         reinterpret_cast<WPARAM>(hwnd),
                         MAKELPARAM(0, APPCOMMAND_MEDIA_PAUSE));
            paused << exe;
        }
    }

    if (comOwned) CoUninitialize();
    return paused;
}

void ensureSpotifyPlaying()
{
    HWND hwnd = windowForExe(QStringLiteral("Spotify.exe"));
    if (!hwnd) return;
    SendMessageW(hwnd, WM_APPCOMMAND,
                 reinterpret_cast<WPARAM>(hwnd),
                 MAKELPARAM(0, APPCOMMAND_MEDIA_PLAY));
}

void focusWindow(const QString &exeName)
{
    HWND hwnd = windowForExe(exeName);
    if (!hwnd) return;

    // Restore from minimised first.
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

    // Windows blocks SetForegroundWindow unless the calling thread "owns" focus.
    // Temporarily attaching input queues grants that permission.
    DWORD targetTid  = GetWindowThreadProcessId(hwnd, nullptr);
    DWORD currentTid = GetCurrentThreadId();
    bool  attached   = (targetTid != currentTid) &&
                       AttachThreadInput(currentTid, targetTid, TRUE);

    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);

    if (attached)
        AttachThreadInput(currentTid, targetTid, FALSE);
}

} // namespace MediaCtrl
