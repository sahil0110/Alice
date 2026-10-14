// AliceLoader — placed in shell:startup via a .lnk shortcut.
// Connects to \\.\pipe\AliceLauncherPipe, sends "launch", logs result, exits.
// Log format is identical to Alice's structured log.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTime>
#include <QTextStream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ─── Constants ───────────────────────────────────────────────────────────────

static const wchar_t *PIPE_NAME    = L"\\\\.\\pipe\\AliceLauncherPipe";
static const int      PIPE_RETRIES = 5;
static const DWORD    RETRY_MS     = 500;

// ─── Logging ─────────────────────────────────────────────────────────────────

// Log file: %APPDATA%\Alice\Alice\applog.txt
// (same location Alice uses; both apps share AppName=Alice / OrgName=Alice)
static QString logPath()
{
    // Must match Alice's appLogFilePath: %APPDATA%\Alice\Alice\applog.txt
    return QString::fromLocal8Bit(qgetenv("APPDATA"))
           + "/Alice/Alice/applog.txt";
}

static void appendLog(const QString &line)
{
    const QString path = logPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << line << '\n';
        out.flush();
        f.flush();
    }
}

// Emits one structured info line matching Alice's format:
//   [HH:mm:ss.zzz] [I] loader_main.cpp:LINE (main) message
#define LOG_INFO(msg)                                                    \
    appendLog(QStringLiteral("[%1] [I] loader_main.cpp:%2 (main) %3")   \
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))) \
        .arg(__LINE__)                                                   \
        .arg(msg))

#define LOG_ERR(msg)                                                     \
    appendLog(QStringLiteral("[%1] [C] loader_main.cpp:%2 (main) %3")   \
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))) \
        .arg(__LINE__)                                                   \
        .arg(msg))

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    a.setApplicationName("Alice");
    a.setOrganizationName("Alice");

    // Session-start marker
    appendLog(QStringLiteral("\n[%1] ── Loader Start ──")
                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));

    LOG_INFO(QStringLiteral("AliceLoader started — PID %1").arg(GetCurrentProcessId()));
    LOG_INFO(QStringLiteral("Pipe target: %1").arg(QString::fromWCharArray(PIPE_NAME)));

    // ── Connect to AliceLauncherPipe ─────────────────────────────────────────
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int attempt = 1; attempt <= PIPE_RETRIES && pipe == INVALID_HANDLE_VALUE; ++attempt) {
        LOG_INFO(QStringLiteral("Connect attempt %1/%2 ...").arg(attempt).arg(PIPE_RETRIES));
        pipe = CreateFileW(PIPE_NAME, GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            LOG_INFO(QStringLiteral("  -> failed, GetLastError=%1").arg(err));
            if (err == ERROR_PIPE_BUSY) {
                LOG_INFO("  -> pipe busy, waiting 2000 ms");
                WaitNamedPipeW(PIPE_NAME, 2000);
            } else if (err == ERROR_FILE_NOT_FOUND) {
                LOG_ERR("  -> pipe not found (service may not be running)");
                Sleep(RETRY_MS);
            } else {
                Sleep(RETRY_MS);
            }
        } else {
            LOG_INFO("  -> connected");
        }
    }

    if (pipe == INVALID_HANDLE_VALUE) {
        LOG_ERR(QStringLiteral("All %1 attempts failed — giving up").arg(PIPE_RETRIES));
        appendLog(QStringLiteral("[%1] ── Loader End (failed) ──")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));
        Sleep(200);
        return 1;
    }

    // ── Send "launch" command ─────────────────────────────────────────────────
    LOG_INFO("Sending \"launch\" command...");
    static const char CMD[] = "launch";
    DWORD written = 0;
    BOOL wok = WriteFile(pipe, CMD, static_cast<DWORD>(sizeof(CMD) - 1), &written, nullptr);
    CloseHandle(pipe);

    if (wok && written == sizeof(CMD) - 1) {
        LOG_INFO(QStringLiteral("Command sent (%1 bytes) — Alice should start shortly").arg(written));
    } else {
        LOG_ERR(QStringLiteral("WriteFile returned %1, written=%2, error=%3")
                    .arg(wok ? "TRUE" : "FALSE").arg(written).arg(GetLastError()));
    }

    appendLog(QStringLiteral("[%1] ── Loader End ──")
                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));

    // Brief pause so the OS finishes flushing the log file before the process exits.
    Sleep(200);
    return (wok && written > 0) ? 0 : 1;
}
