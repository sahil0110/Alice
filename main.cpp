#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QFileInfo>
#include <QTimer>

int main(int argc, char *argv[])
{
    // Chromium flags — must be set before QApplication.
    // Disables FedCM (crashes renderer in embedded WebEngine with Google login)
    // and increases memory/process stability.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-features=FedCm,FedCmIdpSigninStatus "
            "--disable-blink-features=FedCM "
            "--memory-pressure-off "
            "--renderer-process-limit=1 "
            "--disable-dev-shm-usage "
            "--js-flags=--max-old-space-size=1024 "
            "--in-process-gpu "           // run GPU in main process — avoids GPU process
            "--disable-gpu-sandbox");     // startup crash at logon (display not ready yet)

    QApplication a(argc, argv);
    a.setApplicationName("Alice");
    a.setOrganizationName("Alice");
    a.setStyleSheet(Theme::instance().globalStyleSheet());

    // Load from .ico next to the executable so Qt and Windows use the same icon.
    // Falls back to a programmatically-drawn circle if the file is missing.
    QString icoPath = QFileInfo(QApplication::applicationFilePath()).dir().filePath("Alice.ico");
    QIcon appIcon;
    if (QFileInfo::exists(icoPath)) {
        appIcon = QIcon(icoPath);
    } else {
        QPixmap px(256, 256);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(0x7b, 0x8c, 0xde));
        p.setPen(Qt::NoPen);
        p.drawEllipse(16, 16, 224, 224);
        p.end();
        appIcon = QIcon(px);
    }
    a.setWindowIcon(appIcon);

    // --startup: injected by AliceLoader (placed in shell:startup) at logon.
    // Delay window creation so the system tray, GPU driver, and display are ready.
    const bool isStartup = QCoreApplication::arguments().contains("--startup");

    MainWindow *w = nullptr;
    auto createWindow = [&w, isStartup]() {
        w = new MainWindow();
        if (!isStartup) {
            w->show();  // normal launch → show window immediately
        }
        // startup launch → stay hidden in tray; user opens via tray icon or double-click
    };

    if (isStartup) {
        QTimer::singleShot(7000, createWindow);
    } else {
        createWindow();
    }

    int ret = QCoreApplication::exec();
    delete w;
    return ret;
}
