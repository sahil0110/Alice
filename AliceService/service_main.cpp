// AliceService — Win32 service (no Qt).
//
// Modes:
//   AliceService.exe --install    register + start the service (requires admin)
//   AliceService.exe --uninstall  stop + delete the service   (requires admin)
//   AliceService.exe              started by SCM; runs permanently
//
// On every "launch" command received via \\.\pipe\AliceLauncherPipe:
//   Launches Alice.exe in the active user's session using CreateProcessAsUserW
//   with the user's elevated token so the requireAdministrator manifest is
//   satisfied without showing a UAC prompt.
//
// Only errors are written to alice.log.

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW
#include <wtsapi32.h>
#include <userenv.h>
#include <string>
#include <cstdio>

// ─── Constants ───────────────────────────────────────────────────────────────

static const wchar_t *SERVICE_NAME    = L"AliceLauncherService";
static const wchar_t *SERVICE_DISPLAY = L"Alice Launcher Service";
static const wchar_t *PIPE_NAME       = L"\\\\.\\pipe\\AliceLauncherPipe";

// ─── Globals ─────────────────────────────────────────────────────────────────

static SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
static SERVICE_STATUS        g_status       = {};
static HANDLE                g_stopEvent    = nullptr;

// ─── Forward declarations ─────────────────────────────────────────────────────

static void WINAPI ServiceMain(DWORD argc, LPWSTR *argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrl);
static void        PipeLoop();
static bool        LaunchAlice();
static std::wstring GetLogPath();
static void        MkPath(const std::wstring &path);
static void        LogError(const wchar_t *file, int line, const wchar_t *func,
                             const std::wstring &msg);
static int         InstallService();
static int         UninstallService();

// ─── Logging (errors only) ────────────────────────────────────────────────────

// Resolves %APPDATA%\Alice\Alice\logs\alice.log for the active console user.
static std::wstring GetLogPath()
{
    DWORD sid = WTSGetActiveConsoleSessionId();
    if (sid == 0xFFFFFFFF) return {};

    HANDLE tok = nullptr;
    if (!WTSQueryUserToken(sid, &tok)) return {};

    wchar_t prof[MAX_PATH] = {};
    DWORD   sz             = MAX_PATH;
    bool    ok             = GetUserProfileDirectoryW(tok, prof, &sz) != FALSE;
    CloseHandle(tok);

    if (!ok || prof[0] == L'\0') return {};
    // Must match Alice's appLogFilePath: %APPDATA%\Alice\Alice\applog.txt
    return std::wstring(prof) + L"\\AppData\\Roaming\\Alice\\Alice\\applog.txt";
}

// Creates every component of `path` (like mkdir -p).
static void MkPath(const std::wstring &path)
{
    std::wstring cur;
    for (wchar_t c : path) {
        cur += c;
        if (c == L'\\' || c == L'/') CreateDirectoryW(cur.c_str(), nullptr);
    }
    CreateDirectoryW(path.c_str(), nullptr);
}

// Writes one error line matching Alice's structured log format:
//   [HH:mm:ss.zzz] [E] service_main.cpp:LINE (func) message
static void LogError(const wchar_t *file, int line, const wchar_t *func,
                     const std::wstring &msg)
{
    std::wstring logPath = GetLogPath();
    if (logPath.empty()) return;

    // Ensure log directory exists
    MkPath(logPath.substr(0, logPath.rfind(L'\\')));

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[32];
    swprintf_s(ts, L"%02d:%02d:%02d.%03d",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    // Format: [HH:mm:ss.zzz] [E] file:line (func) message\n
    std::wstring line_str = std::wstring(L"[") + ts + L"] [E] "
                          + file + L":" + std::to_wstring(line)
                          + L" (" + func + L") " + msg + L"\n";

    // Convert to UTF-8 (Alice's QTextStream writes UTF-8)
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0,
                                      line_str.c_str(), -1,
                                      nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 1) return;
    std::string utf8(static_cast<size_t>(utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, line_str.c_str(), -1,
                        &utf8[0], utf8Len, nullptr, nullptr);

    HANDLE fh = CreateFileW(logPath.c_str(),
                            FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(fh, utf8.c_str(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(fh);
}

#define LOG_ERR(msg) LogError(L"service_main.cpp", __LINE__, __FUNCTIONW__, msg)

// ─── Alice launcher ───────────────────────────────────────────────────────────

// Returns the directory that contains AliceService.exe.
static std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    size_t slash = p.rfind(L'\\');
    return (slash != std::wstring::npos) ? p.substr(0, slash) : p;
}

// Creates Alice.exe in the active user's interactive session using their
// elevated token, so requireAdministrator is satisfied without a UAC prompt.
static bool LaunchAlice()
{
    // ── Get active console session ────────────────────────────────────────────
    DWORD sid = WTSGetActiveConsoleSessionId();
    if (sid == 0xFFFFFFFF) {
        LOG_ERR(L"No active console session");
        return false;
    }

    // ── Get the user's token ──────────────────────────────────────────────────
    HANDLE userToken = nullptr;
    if (!WTSQueryUserToken(sid, &userToken)) {
        LOG_ERR(L"WTSQueryUserToken failed (error " + std::to_wstring(GetLastError()) + L")");
        return false;
    }

    // ── Pick elevated token when UAC is active ────────────────────────────────
    // TokenElevationTypeLimited  → standard user token; linked token is elevated
    // TokenElevationTypeFull     → already elevated (UAC off or built-in admin)
    // TokenElevationTypeDefault  → no UAC split (local admin, UAC fully disabled)
    HANDLE tokenToUse = userToken;
    TOKEN_ELEVATION_TYPE elevType = TokenElevationTypeDefault;
    DWORD sz = 0;
    if (GetTokenInformation(userToken, TokenElevationType, &elevType, sizeof(elevType), &sz)
        && elevType == TokenElevationTypeLimited)
    {
        TOKEN_LINKED_TOKEN linked = {};
        sz = sizeof(linked);
        if (GetTokenInformation(userToken, TokenLinkedToken, &linked, sz, &sz))
            tokenToUse = linked.LinkedToken;
    }

    // ── Create environment block for the user ─────────────────────────────────
    LPVOID envBlock = nullptr;
    CreateEnvironmentBlock(&envBlock, tokenToUse, FALSE); // failure is non-fatal

    // ── Build command line ────────────────────────────────────────────────────
    std::wstring dir     = GetExeDir();
    std::wstring exePath = dir + L"\\Alice.exe";
    std::wstring cmdLine = L"\"" + exePath + L"\" --startup";

    // ── Launch in the user's desktop ──────────────────────────────────────────
    STARTUPINFOW si   = {};
    si.cb             = sizeof(si);
    si.lpDesktop      = const_cast<wchar_t *>(L"winsta0\\default");

    PROCESS_INFORMATION pi = {};

    DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
    bool ok = CreateProcessAsUserW(
        tokenToUse,
        exePath.c_str(),
        cmdLine.data(),
        nullptr, nullptr,
        FALSE,
        creationFlags,
        envBlock,
        dir.c_str(),
        &si, &pi) != FALSE;

    if (!ok)
        LOG_ERR(L"CreateProcessAsUser failed (error " + std::to_wstring(GetLastError()) + L")");

    // ── Cleanup ───────────────────────────────────────────────────────────────
    if (envBlock) DestroyEnvironmentBlock(envBlock);
    if (tokenToUse != userToken) CloseHandle(tokenToUse);
    CloseHandle(userToken);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok;
}

// ─── Service status helper ────────────────────────────────────────────────────

static void SetStatus(DWORD state, DWORD waitHint = 0)
{
    static DWORD checkpoint = 1;
    g_status.dwCurrentState  = state;
    g_status.dwWaitHint      = waitHint;
    g_status.dwCheckPoint    = (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
                               ? 0 : checkpoint++;
    SetServiceStatus(g_statusHandle, &g_status);
}

// ─── Pipe server loop ─────────────────────────────────────────────────────────

// Runs on the ServiceMain thread.
// Creates a named pipe instance, waits for a client (using overlapped I/O so
// the stop event can interrupt the wait), reads the command, acts on it, loops.
static void PipeLoop()
{
    while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {

        // Create one pipe instance for the next client.
        // NULL DACL is required: the service runs as LocalSystem, but the
        // client (AliceLoader) runs as the interactive user.  Without an
        // explicit security descriptor the default ACL denies non-admin
        // processes write access, so AliceLoader gets ERROR_ACCESS_DENIED.
        SECURITY_DESCRIPTOR sd = {};
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE); // nullptr DACL = allow everyone
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength              = sizeof(sa);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle       = FALSE;

        HANDLE pipe = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            0, 256, 0, &sa);

        if (pipe == INVALID_HANDLE_VALUE) {
            LOG_ERR(L"CreateNamedPipe failed (error " + std::to_wstring(GetLastError()) + L")");
            Sleep(1000);
            continue;
        }

        // Wait for a client to connect — or for the stop event
        OVERLAPPED ov    = {};
        ov.hEvent        = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL connected   = ConnectNamedPipe(pipe, &ov);
        DWORD connectErr = GetLastError();

        bool clientReady = false;
        if (!connected) {
            if (connectErr == ERROR_IO_PENDING) {
                // Normal overlapped path — wait on the event or stop signal
                HANDLE waitHandles[] = { ov.hEvent, g_stopEvent };
                DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (result == WAIT_OBJECT_0) {
                    DWORD transferred = 0;
                    clientReady = GetOverlappedResult(pipe, &ov, &transferred, FALSE) != FALSE;
                }
                // result == WAIT_OBJECT_0 + 1 → stop event; clientReady stays false
            } else if (connectErr == ERROR_PIPE_CONNECTED) {
                // Client connected before ConnectNamedPipe was called
                clientReady = true;
            }
        } else {
            clientReady = true; // already connected (synchronous)
        }

        if (clientReady) {
            char   buf[32] = {};
            DWORD  read    = 0;
            if (ReadFile(pipe, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
                if (strncmp(buf, "launch", 6) == 0)
                    LaunchAlice();
            }
        }

        CloseHandle(ov.hEvent);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

// ─── Service control handler ──────────────────────────────────────────────────

static void WINAPI ServiceCtrlHandler(DWORD ctrl)
{
    switch (ctrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            SetStatus(SERVICE_STOP_PENDING, 3000);
            SetEvent(g_stopEvent);
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break; // SCM will re-read g_status
    }
}

// ─── ServiceMain ─────────────────────────────────────────────────────────────

static void WINAPI ServiceMain(DWORD /*argc*/, LPWSTR * /*argv*/)
{
    // Register control handler
    g_statusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_statusHandle) return;

    // Describe capabilities
    g_status.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwServiceSpecificExitCode = 0;

    SetStatus(SERVICE_START_PENDING, 1000);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        g_status.dwWin32ExitCode = GetLastError();
        SetStatus(SERVICE_STOPPED);
        return;
    }

    SetStatus(SERVICE_RUNNING);

    // Run the pipe listener — blocks until stop event is signaled
    PipeLoop();

    CloseHandle(g_stopEvent);
    g_stopEvent = nullptr;

    SetStatus(SERVICE_STOPPED);
}

// ─── Install / Uninstall ──────────────────────────────────────────────────────

static int InstallService()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return static_cast<int>(GetLastError());

    SC_HANDLE svc = CreateServiceW(
        scm,
        SERVICE_NAME,
        SERVICE_DISPLAY,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,         // start automatically at boot
        SERVICE_ERROR_NORMAL,
        exePath,
        nullptr, nullptr, nullptr,  // load group, tag, dependencies
        nullptr, nullptr);          // account (LocalSystem), password

    if (!svc) {
        DWORD err = GetLastError();
        CloseServiceHandle(scm);
        if (err == ERROR_SERVICE_EXISTS) return 0; // idempotent
        return static_cast<int>(err);
    }

    // Start the service immediately so it's ready without a reboot
    StartServiceW(svc, 0, nullptr);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

static int UninstallService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return static_cast<int>(GetLastError());

    SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME,
                                 SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!svc) {
        DWORD err = GetLastError();
        CloseServiceHandle(scm);
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) return 0; // idempotent
        return static_cast<int>(err);
    }

    // Ask it to stop
    SERVICE_STATUS ss = {};
    ControlService(svc, SERVICE_CONTROL_STOP, &ss);

    // Wait up to 10 s for the service to reach STOPPED
    for (int i = 0; i < 20; ++i) {
        if (!QueryServiceStatus(svc, &ss)) break;
        if (ss.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(500);
    }

    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int     argc  = 0;
    LPWSTR *argv  = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc >= 2 && _wcsicmp(argv[1], L"--install") == 0) {
        LocalFree(argv);
        return InstallService();
    }
    if (argc >= 2 && _wcsicmp(argv[1], L"--uninstall") == 0) {
        LocalFree(argv);
        return UninstallService();
    }
    LocalFree(argv);

    // No arguments → we were started by SCM
    SERVICE_TABLE_ENTRYW table[] = {
        { const_cast<wchar_t *>(SERVICE_NAME), ServiceMain },
        { nullptr, nullptr }
    };
    StartServiceCtrlDispatcherW(table);
    return 0;
}
