#pragma once
#include <QString>
#include <QStringList>

class Commands
{
public:
    static QString parse(const QString &input);

    // shutdown
    static void runShutdown(int minutes);
    static void cancelShutdown();

    // process management
    static void killProcess(const QString &processName);
    static void killProcessTree(const QString &processName);
    static bool isProcessRunning(const QString &processName);
    static void launchApp(const QString &path, const QStringList &args = {});

    // windows services
    static void stopService(const QString &serviceName);
    static void startService(const QString &serviceName);

    // display
    static void switchToSingleMonitor();
    static void restoreExtendedDisplay();
    static void changeResolution(int width, int height);
    static void setHighestRefreshRate();
    static void setRefreshRate(int hz);

    // package management
    static void uninstallApps(const QStringList &appNames);

    // blackout
    static void runBlackout(const QStringList &exeNames);

    // maintenance
    static void cleanTempFiles();
};