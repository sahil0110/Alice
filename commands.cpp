#include "commands.h"
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

QString Commands::parse(const QString &input)
{
    QString cmd = input.toLower().trimmed();

    if (cmd.startsWith("shutdown ")) {
        QString rest = input.mid(9).trimmed();
        int minutes = rest.toInt();
        if (minutes > 0) {
            runShutdown(minutes);
            return "Shutdown scheduled in " + QString::number(minutes) + " minutes.";
        }
        return "Usage: shutdown <minutes>";
    }

    if (cmd == "cancel shutdown") {
        cancelShutdown();
        return "Shutdown cancelled.";
    }

    // ── RAMMap ──────────────────────────────────────────────────────────────
    if (cmd == "rammap" || cmd == "empty standby" || cmd == "ram map" || cmd == "ram clean") {
        // Search common locations
        QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/RamMap.exe",
            QCoreApplication::applicationDirPath() + "/RamMap64.exe",
            "C:/SysinternalsSuite/RamMap.exe",
            "C:/SysinternalsSuite/RamMap64.exe",
            "C:/tools/RamMap.exe",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/RamMap.exe",
        };
        QString found;
        for (const QString &p : candidates)
            if (QFile::exists(p)) { found = p; break; }

        if (found.isEmpty()) {
            // Last resort: check PATH via 'where'
            QProcess where;
            where.start("where", {"RamMap.exe"});
            where.waitForFinished(1000);
            if (where.exitCode() == 0)
                found = QString::fromLocal8Bit(where.readLine()).trimmed();
        }

        if (found.isEmpty())
            return "RamMap.exe not found.\n"
                   "Put RamMap.exe in the same folder as Alice (or in C:\\SysinternalsSuite\\).";

        QProcess::startDetached(found, {"-accepteula"});
        return "Opened RAMMap — go to Empty → Empty Standby List.";
    }

    // ── Reliability Monitor (Windows Logs) ──────────────────────────────────
    if (cmd == "windows log" || cmd == "windows logs" ||
        cmd == "reliability monitor" || cmd == "reliability") {
        QProcess::startDetached("perfmon.exe", {"/rel"});
        return "Opening Windows Reliability Monitor...";
    }

    // ── Clean temp / cache / old updates ────────────────────────────────────
    if (cmd == "clean temp" || cmd == "clean cache" || cmd == "clean junk" ||
        cmd == "cleanup"    || cmd == "clean"       || cmd == "clean all") {
        cleanTempFiles();
        return "Cleaning temp files, browser cache leftovers, and old Windows Update data...\n"
               "A UAC prompt will appear for system folders.";
    }

    if (cmd.startsWith("uninstall ") || cmd.startsWith("remove ")) {
        QString rest = cmd.startsWith("uninstall ") ? input.mid(10).trimmed()
                                                     : input.mid(7).trimmed();
        QStringList apps;
        for (const QString &a : rest.split(',', Qt::SkipEmptyParts))
            if (!a.trimmed().isEmpty()) apps << a.trimmed();
        if (apps.isEmpty()) return "Usage: uninstall <app> [, <app2>, ...]";
        uninstallApps(apps);
        return "Uninstalling " + apps.join(", ") + "...";
    }

    return "";
}

void Commands::runShutdown(int minutes)
{
    int seconds = minutes * 60;
    QProcess::startDetached("shutdown", QStringList() << "/s" << "/t" << QString::number(seconds));
}

void Commands::cancelShutdown()
{
    QProcess::startDetached("shutdown", QStringList() << "/a");
}

void Commands::killProcess(const QString &processName)
{
    QProcess::startDetached("taskkill", QStringList() << "/f" << "/im" << processName);
}

void Commands::killProcessTree(const QString &processName)
{
    QProcess::startDetached("taskkill", QStringList() << "/f" << "/t" << "/im" << processName);
}

void Commands::switchToSingleMonitor()
{
    QProcess::startDetached("DisplaySwitch.exe", QStringList() << "/internal");
}

void Commands::restoreExtendedDisplay()
{
    QProcess::startDetached("DisplaySwitch.exe", QStringList() << "/extend");
}

void Commands::changeResolution(int width, int height)
{
    // Uses inline C# via PowerShell to call ChangeDisplaySettings
    QString script = QString(
        "& { Add-Type @'\n"
        "using System; using System.Runtime.InteropServices;\n"
        "public class Res {\n"
        "  [DllImport(\"user32.dll\")] public static extern int ChangeDisplaySettings(ref DEVMODE dm, int flags);\n"
        "  [StructLayout(LayoutKind.Sequential)] public struct DEVMODE {\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmDeviceName;\n"
        "    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;\n"
        "    public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;\n"
        "    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmFormName;\n"
        "    public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;\n"
        "  }\n"
        "}\n"
        "'@;\n"
        "$dm = New-Object Res+DEVMODE;\n"
        "$dm.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($dm);\n"
        "$dm.dmPelsWidth = %1; $dm.dmPelsHeight = %2; $dm.dmFields = 0x180000;\n"
        "[Res]::ChangeDisplaySettings([ref]$dm, 0) }"
    ).arg(width).arg(height);
    QProcess::startDetached("powershell",
        QStringList() << "-WindowStyle" << "Hidden" << "-Command" << script);
}

void Commands::setHighestRefreshRate()
{
    // Enumerate all display modes for the current resolution, pick the highest Hz
    QString script =
        "& { Add-Type @'\n"
        "using System; using System.Runtime.InteropServices;\n"
        "public class HzFinder {\n"
        "  [DllImport(\"user32.dll\")] public static extern bool EnumDisplaySettings(string dev, int mode, ref DEVMODE dm);\n"
        "  [DllImport(\"user32.dll\")] public static extern int ChangeDisplaySettings(ref DEVMODE dm, int flags);\n"
        "  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)] public struct DEVMODE {\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmDeviceName;\n"
        "    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;\n"
        "    public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;\n"
        "    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmFormName;\n"
        "    public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;\n"
        "  }\n"
        "}\n"
        "'@;\n"
        "$cur = New-Object HzFinder+DEVMODE; $cur.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($cur);\n"
        "[HzFinder]::EnumDisplaySettings($null, -1, [ref]$cur);\n"
        "$w = $cur.dmPelsWidth; $h = $cur.dmPelsHeight;\n"
        "$maxHz = 0; $i = 0;\n"
        "$best = New-Object HzFinder+DEVMODE; $best.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($best);\n"
        "$dm = New-Object HzFinder+DEVMODE; $dm.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($dm);\n"
        "while ([HzFinder]::EnumDisplaySettings($null, $i, [ref]$dm)) {\n"
        "  if ($dm.dmPelsWidth -eq $w -and $dm.dmPelsHeight -eq $h -and $dm.dmDisplayFrequency -gt $maxHz) {\n"
        "    $maxHz = $dm.dmDisplayFrequency; $best = $dm; }\n"
        "  $i++ }\n"
        "if ($maxHz -gt 0) { $best.dmFields = 0x400000; [HzFinder]::ChangeDisplaySettings([ref]$best, 0) } }";
    QProcess::startDetached("powershell",
        QStringList() << "-WindowStyle" << "Hidden" << "-Command" << script);
}

void Commands::setRefreshRate(int hz)
{
    if (hz <= 0) return;
    QString script = QString(
        "& { Add-Type @'\n"
        "using System; using System.Runtime.InteropServices;\n"
        "public class HzRestore {\n"
        "  [DllImport(\"user32.dll\")] public static extern int ChangeDisplaySettings(ref DEVMODE dm, int flags);\n"
        "  [StructLayout(LayoutKind.Sequential)] public struct DEVMODE {\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmDeviceName;\n"
        "    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;\n"
        "    public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;\n"
        "    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;\n"
        "    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmFormName;\n"
        "    public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;\n"
        "  }\n"
        "}\n"
        "'@;\n"
        "$dm = New-Object HzRestore+DEVMODE;\n"
        "$dm.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($dm);\n"
        "$dm.dmDisplayFrequency = %1; $dm.dmFields = 0x400000;\n"
        "[HzRestore]::ChangeDisplaySettings([ref]$dm, 0) }"
    ).arg(hz);
    QProcess::startDetached("powershell",
        QStringList() << "-WindowStyle" << "Hidden" << "-Command" << script);
}

bool Commands::isProcessRunning(const QString &processName)
{
    // Use a Win32 process snapshot — no subprocess spawn, runs entirely in-process.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile)
                    .compare(processName, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

void Commands::launchApp(const QString &path, const QStringList &args)
{
    if (path.isEmpty()) return;

    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "reg") {
        // Import registry file — must be elevated (HKLM keys require admin rights).
        // Use PowerShell Start-Process -Verb RunAs so Windows shows the UAC prompt.
        QString native = QDir::toNativeSeparators(path);
        native.replace("'", "''");   // escape single quotes for PS string
        QProcess::startDetached("powershell.exe",
            QStringList() << "-WindowStyle" << "Hidden" << "-Command"
            << QString("Start-Process regedit.exe -Verb RunAs -ArgumentList '/s','\"%1\"'")
               .arg(native));
    } else if (ext == "lnk" || ext == "url") {
        // Shortcut — let explorer resolve it
        QProcess::startDetached("explorer.exe", QStringList() << QDir::toNativeSeparators(path));
    } else if (ext == "exe" || ext.isEmpty()) {
        // Executable — if no extension, try appending .exe
        QString resolved = path;
        if (ext.isEmpty() && !QFile::exists(path))
            resolved = path + ".exe";
        QProcess::startDetached(resolved, args);
    } else if (ext == "ps1") {
        // PowerShell script — run elevated so scripts that touch HKLM work
        QString native = QDir::toNativeSeparators(path);
        native.replace("'", "''");
        QProcess::startDetached("powershell.exe",
            QStringList() << "-WindowStyle" << "Hidden" << "-Command"
            << QString("Start-Process powershell -Verb RunAs "
                       "-ArgumentList '-ExecutionPolicy Bypass -File \"%1\"'").arg(native));
    } else {
        // Any other file type — open with default associated app via cmd start
        QProcess::startDetached("cmd.exe",
            QStringList() << "/c" << "start" << "" << QDir::toNativeSeparators(path));
    }
}

void Commands::stopService(const QString &serviceName)
{
    QProcess::startDetached("sc", QStringList() << "stop" << serviceName);
}

void Commands::startService(const QString &serviceName)
{
    QProcess::startDetached("sc", QStringList() << "start" << serviceName);
}

void Commands::runBlackout(const QStringList &exeNames)
{
    for (const QString &exe : exeNames)
        killProcessTree(exe);
}

void Commands::cleanTempFiles()
{
    // Write a .ps1 script to a temp location to avoid all quote-nesting nightmares,
    // then run it elevated via Start-Process -Verb RunAs.
    const QString scriptPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/alice_cleanup.ps1";

    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[cleanTemp] Could not write script to" << scriptPath;
        return;
    }
    QTextStream out(&f);
    out << "# Alice cleanup script — runs elevated\n"
        << "$ErrorActionPreference = 'SilentlyContinue'\n\n"
        // User temp (safe even without elevation)
        << "Remove-Item \"$env:TEMP\\*\"            -Recurse -Force\n"
        << "Remove-Item \"$env:LOCALAPPDATA\\Temp\\*\" -Recurse -Force\n\n"
        // System temp + prefetch (need elevation)
        << "Remove-Item 'C:\\Windows\\Temp\\*'      -Recurse -Force\n"
        << "Remove-Item 'C:\\Windows\\Prefetch\\*'  -Force\n\n"
        // Windows Update download cache
        << "Stop-Service  -Name wuauserv -Force\n"
        << "Remove-Item 'C:\\Windows\\SoftwareDistribution\\Download\\*' -Recurse -Force\n"
        << "Start-Service -Name wuauserv\n\n"
        // Windows Error Reporting cache
        << "Remove-Item \"$env:LOCALAPPDATA\\Microsoft\\Windows\\WER\\*\" -Recurse -Force\n\n"
        << "Write-Host 'Alice cleanup complete.'\n";
    f.close();

    // Launch an elevated PowerShell that runs the script
    const QString nativePath = QDir::toNativeSeparators(scriptPath);
    QProcess::startDetached("powershell.exe", QStringList()
        << "-NoProfile" << "-WindowStyle" << "Hidden"
        << "-Command"
        << QString("Start-Process powershell -Verb RunAs "
                   "-ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%1\"'")
                   .arg(nativePath));
}

void Commands::uninstallApps(const QStringList &appNames)
{
    // Launch each uninstall sequentially with a 3-second gap between them.
    // stdout/stderr are piped to qDebug() so the Logs view receives all output.
    for (int i = 0; i < appNames.size(); ++i) {
        const QString name = appNames[i];
        QTimer::singleShot(i * 3000, [name]() {
            QProcess *proc = new QProcess();
            QObject::connect(proc, &QProcess::readyReadStandardOutput, [proc, name]() {
                const QString out = QString::fromLocal8Bit(
                    proc->readAllStandardOutput()).trimmed();
                if (!out.isEmpty())
                    qDebug().noquote() << "[uninstall" << name << "]" << out;
            });
            QObject::connect(proc, &QProcess::readyReadStandardError, [proc, name]() {
                const QString err = QString::fromLocal8Bit(
                    proc->readAllStandardError()).trimmed();
                if (!err.isEmpty())
                    qDebug().noquote() << "[uninstall" << name << "]" << err;
            });
            QObject::connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                proc, &QObject::deleteLater);
            proc->start("winget", QStringList()
                << "uninstall"
                << "--name"                     << name
                << "--silent"
                << "--accept-source-agreements"
                << "--disable-interactivity");
        });
    }
}

