#include "mainwindow.h"
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProcess>
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include "theme.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>     // FOLDERID_Startup, SHGetKnownFolderPath, IShellLinkW
#include <objbase.h>    // CoInitializeEx, CoCreateInstance, CoUninitialize

void MainWindow::loadSettings()
{
    qDebug() << "[loadSettings] path:" << configFilePath;
    QFile f(configFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[loadSettings] file not found or could not open — skipping";
        return;
    }
    QByteArray raw = f.readAll();
    f.close();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (doc.isNull()) {
        qDebug() << "[loadSettings] JSON parse error:" << parseErr.errorString();
        return;
    }
    QJsonObject root = doc.object();
    QString loadedName = root["name"].toString("User");
    qDebug() << "[loadSettings] name from file:" << loadedName;
    nameEdit->setText(loadedName);

    // Theme prefs (loaded BEFORE applyTheme runs)
    if (root.contains("themeMode"))
        Theme::instance().setMode(static_cast<Theme::Mode>(root["themeMode"].toInt(Theme::Midnight)));
    if (root.contains("themeAnimated"))
        Theme::instance().setAnimated(root["themeAnimated"].toBool(true));
    if (root.contains("taskRunnerVisible") && m_taskRunnerLabel)
        m_taskRunnerLabel->setVisible(root["taskRunnerVisible"].toBool(true));

    QJsonArray apis = root["apis"].toArray();
    for (const QJsonValue &v : std::as_const(apis)) {
        QJsonObject api = v.toObject();
        addApiEntry(api["name"].toString(), api["key"].toString(), api["model"].toString(), api["active"].toBool());
    }

    const QJsonArray blackoutApps = root["blackoutApps"].toArray();
    for (const QJsonValue &v : blackoutApps) {
        QString exe = v.toString();
        if (exe.isEmpty()) continue;
        QWidget *row = new QWidget();
        row->setObjectName("blackoutRow");
        row->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 10, 14, 10);
        QLabel *nameLabel = new QLabel(exe.section('.', 0, 0));
        nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
        QLabel *exeLabel = new QLabel(exe);
        exeLabel->setObjectName("blackoutExe");
        exeLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
        QPushButton *delBtn = new QPushButton("✕");
        delBtn->setFlat(true);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                              "QPushButton:hover { color: #ff4444; }");
        connect(delBtn, &QPushButton::clicked, this, [this, row]() {
            row->deleteLater();
            saveSettings();
        });
        rl->addWidget(nameLabel, 2);
        rl->addWidget(exeLabel, 2);
        rl->addWidget(delBtn);
        blackoutAppsLayout->addWidget(row);
    }

    // Integrated Web AI
    bool intOn = root["integratedWebAiEnabled"].toBool(false);
    integratedWebAiToggleBtn->setProperty("toggled", intOn);
    integratedWebAiToggleBtn->setText(intOn ? "ON" : "OFF");
    integratedWebAiToggleBtn->setStyleSheet(intOn
        ? "QPushButton { color: #50C878; font-size: 12px; padding: 3px 10px;"
          "border: 1px solid #50C878; border-radius: 4px; }"
          "QPushButton:hover { background: #0a1a10; }"
        : "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
          "border: 1px solid #1e2130; border-radius: 4px; }"
          "QPushButton:hover { color: #ffffff; background: #181c28; }");
    QString intProvider = root["integratedWebAiProvider"].toString("ChatGPT");
    int intProviderIdx = integratedWebAiProviderCombo->findText(intProvider);
    integratedWebAiProviderCombo->blockSignals(true);
    if (intProviderIdx >= 0) integratedWebAiProviderCombo->setCurrentIndex(intProviderIdx);
    integratedWebAiProviderCombo->blockSignals(false);
    integratedWebAiProviderCombo->setEnabled(intOn);

    apiSectionWidget->setEnabled(!intOn);

    // Store HUD position — applied in setupHud() before show() so there's no timer race
    if (root.contains("hudX")) {
        m_savedHudX = root["hudX"].toInt(-1);
        m_savedHudY = root["hudY"].toInt(-1);
    }

    // Restore sidebar width after window layout is finalised
    // Use singleShot(50) so the window is fully shown before setSizes fires,
    // and save both pane sizes so we don't depend on current width().
    int sidebarW = root["sidebarWidth"].toInt(160);
    int mainW    = root["mainAreaWidth"].toInt(900);
    QTimer::singleShot(50, this, [this, sidebarW, mainW]() {
        if (bodySplitter)
            bodySplitter->setSizes({sidebarW, mainW});
    });

    updateWatermark();
    // isAutoStartupEnabled() does a file-system check + SCM query — defer off UI thread.
    // setupSettingsPage() sets a "..." placeholder; we update the real state once shown.
    QTimer::singleShot(500, this, [this]() {
        bool startup = isAutoStartupEnabled();
        startupToggleBtn->setText(startup ? "● Enabled" : "Disabled");
        startupToggleBtn->setStyleSheet(startup
            ? "QPushButton { color: #50C878; font-size: 12px; padding: 3px 10px; border: 1px solid #1e2130; border-radius: 4px; }"
              "QPushButton:hover { background: #181c28; }"
            : "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px; border: 1px solid #1e2130; border-radius: 4px; }"
              "QPushButton:hover { color: #ffffff; background: #181c28; }");
    });
}

void MainWindow::saveSettings()
{
    QJsonArray apis;
    for (int i = 0; i < apiEntriesLayout->count(); i++) {
        QWidget *w = apiEntriesLayout->itemAt(i)->widget();
        if (!w) continue;
        QJsonObject api;
        api["name"] = w->property("apiName").toString();
        api["key"] = w->property("apiKey").toString();
        api["model"] = w->property("apiModel").toString();
        api["active"] = w->property("apiActive").toBool();
        apis.append(api);
    }
    QJsonArray blackoutApps;
    for (int i = 0; i < blackoutAppsLayout->count(); ++i) {
        QWidget *w = blackoutAppsLayout->itemAt(i)->widget();
        if (!w) continue;
        QLabel *exeLbl = w->findChild<QLabel*>("blackoutExe");
        if (exeLbl && !exeLbl->text().isEmpty())
            blackoutApps.append(exeLbl->text());
    }

    QJsonObject root;
    root["name"]          = nameEdit->text().isEmpty() ? "User" : nameEdit->text();
    root["apis"]          = apis;
    root["blackoutApps"]  = blackoutApps;
    root["integratedWebAiEnabled"]  = integratedWebAiToggleBtn->property("toggled").toBool();
    root["integratedWebAiProvider"] = integratedWebAiProviderCombo->currentText();
    root["themeMode"]          = static_cast<int>(Theme::instance().mode());
    root["themeAnimated"]      = Theme::instance().animated();
    root["taskRunnerVisible"]  = m_taskRunnerLabel ? m_taskRunnerLabel->isVisible() : true;
    if (bodySplitter && bodySplitter->sizes().size() >= 2) {
        int sw = bodySplitter->sizes().at(0);
        int mw = bodySplitter->sizes().at(1);
        if (sw > 0 && mw > 0) {   // don't persist zeros from pre-show state
            root["sidebarWidth"]  = sw;
            root["mainAreaWidth"] = mw;
        }
    }
    if (m_hud) {
        root["hudX"] = m_hud->hudPos().x();
        root["hudY"] = m_hud->hudPos().y();
    }
    QFile f(configFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

void MainWindow::addApiEntry(const QString &name, const QString &key, const QString &model, bool active)
{
    QWidget *row = new QWidget();
    row->setProperty("apiName", name);
    row->setProperty("apiKey", key);
    row->setProperty("apiModel", model);
    row->setProperty("apiActive", active);
    row->setStyleSheet("QWidget { border-top: 1px solid #1e2130; }");
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(14, 10, 14, 10);

    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
    QLabel *modelLabel = new QLabel(model);
    modelLabel->setStyleSheet("color: #9098b0; font-size: 12px;");

    QPushButton *activeBtn = new QPushButton(active ? "● Active" : "Set Active");
    activeBtn->setFlat(true);
    activeBtn->setCursor(Qt::PointingHandCursor);
    activeBtn->setStyleSheet(active
                                 ? "QPushButton { color: #3a7a4a; font-size: 12px; }"
                                 : "QPushButton { color: #9098b0; font-size: 12px; }"
                                   "QPushButton:hover { color: #ffffff; }");
    connect(activeBtn, &QPushButton::clicked, this, [this, row, activeBtn]() {
        bool wasActive = row->property("apiActive").toBool();
        // deactivate all
        for (int i = 0; i < apiEntriesLayout->count(); i++) {
            QWidget *w = apiEntriesLayout->itemAt(i)->widget();
            if (!w) continue;
            w->setProperty("apiActive", false);
            QPushButton *btn = w->findChild<QPushButton*>();
            if (btn) {
                btn->setText("Set Active");
                btn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                   "QPushButton:hover { color: #ffffff; }");
            }
        }
        // if it wasn't active, activate it; if it was, leave everything deactivated (toggle off)
        if (!wasActive) {
            row->setProperty("apiActive", true);
            activeBtn->setText("● Active");
            activeBtn->setStyleSheet("QPushButton { color: #3a7a4a; font-size: 12px; }");
        }
        saveSettings();
    });

    QPushButton *deleteBtn = new QPushButton("✕");
    deleteBtn->setFlat(true);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                             "QPushButton:hover { color: #ff4444; }");
    connect(deleteBtn, &QPushButton::clicked, this, [this, row]() {
        apiEntriesLayout->removeWidget(row);
        row->deleteLater();
        saveSettings();
    });

    rowLayout->addWidget(nameLabel, 2);
    rowLayout->addWidget(modelLabel, 2);
    rowLayout->addWidget(activeBtn, 1);
    rowLayout->addWidget(deleteBtn);
    apiEntriesLayout->addWidget(row);
}

// ─── Startup helpers (file-static) ───────────────────────────────────────────

// Full path to AliceLoader.lnk inside the per-user shell:startup folder.
static QString startupLnkPath()
{
    PWSTR path = nullptr;
    SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_CREATE, nullptr, &path);
    if (!path) return {};
    QString dir = QString::fromWCharArray(path);
    CoTaskMemFree(path);
    return dir + "\\AliceLoader.lnk";
}

// Full path to AliceService.exe (same directory as Alice.exe).
static QString serviceExePath()
{
    return QDir::toNativeSeparators(
        QFileInfo(QApplication::applicationFilePath())
            .dir().filePath("AliceService.exe"));
}

// Full path to AliceLoader.exe (same directory as Alice.exe).
static QString loaderExePath()
{
    return QDir::toNativeSeparators(
        QFileInfo(QApplication::applicationFilePath())
            .dir().filePath("AliceLoader.exe"));
}

// Returns true if AliceLauncherService is registered with the SCM.
static bool isServiceInstalled()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceW(scm, L"AliceLauncherService", SERVICE_QUERY_STATUS);
    bool installed = (svc != nullptr);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return installed;
}

// Creates AliceLoader.lnk in shell:startup pointing at loaderExe.
static bool createStartupLnk(const QString &loaderExe)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // RPC_E_CHANGED_MODE means COM is already initialised on this thread — that's fine.
    if (FAILED(hr) && hr != static_cast<HRESULT>(RPC_E_CHANGED_MODE)) return false;

    IShellLinkW *link = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, reinterpret_cast<void **>(&link));
    if (FAILED(hr)) { CoUninitialize(); return false; }

    link->SetPath(reinterpret_cast<const wchar_t *>(loaderExe.utf16()));
    link->SetWorkingDirectory(reinterpret_cast<const wchar_t *>(
        QDir::toNativeSeparators(QFileInfo(loaderExe).absolutePath()).utf16()));
    link->SetDescription(L"Start Alice at logon");

    IPersistFile *pf = nullptr;
    hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&pf));

    bool ok = false;
    if (SUCCEEDED(hr)) {
        const QString lnk = startupLnkPath();
        ok = SUCCEEDED(pf->Save(reinterpret_cast<const wchar_t *>(lnk.utf16()), TRUE));
        pf->Release();
    }

    link->Release();
    CoUninitialize();
    return ok;
}

// ─── MainWindow members ───────────────────────────────────────────────────────

bool MainWindow::isRunningAsAdmin() const
{
    BOOL elevated = FALSE;
    HANDLE token  = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD sz = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sz, &sz))
            elevated = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated;
}

bool MainWindow::isAutoStartupEnabled() const
{
    // Both must be true: AliceLoader.lnk present in shell:startup
    //                AND AliceLauncherService registered in SCM.
    return QFile::exists(startupLnkPath()) && isServiceInstalled();
}

// Runs exePath with a single argument, waits for it to finish, returns exit code.
// Returns -1 if the process could not be started at all (file not found, etc.).
static int runProcess(const QString &exePath, const QString &arg)
{
    if (!QFile::exists(exePath)) return -1;
    QProcess proc;
    proc.start(exePath, {arg});
    if (!proc.waitForStarted(5000)) return -1;   // failed to launch
    proc.waitForFinished(15000);
    return (proc.exitStatus() == QProcess::NormalExit) ? proc.exitCode() : -1;
}

bool MainWindow::setAutoStartup(bool enabled)
{
    if (enabled) {
        // ── 1. Install AliceLauncherService if not already present ────────────
        if (!isServiceInstalled()) {
            const QString svc = serviceExePath();
            bool svcOk = false;

            if (isRunningAsAdmin()) {
                // Release builds: already admin — run AliceService.exe --install directly.
                int code = runProcess(svc, "--install");
                svcOk = (code == 0);
                if (code == -1)
                    logAppEvent("AliceService.exe not found or failed to start: " + svc);
            } else {
                // Debug builds: elevate AliceService.exe so it can call CreateService.
                HINSTANCE r = ShellExecuteW(
                    nullptr, L"runas",
                    reinterpret_cast<const wchar_t *>(
                        QDir::toNativeSeparators(svc).utf16()),
                    L"--install", nullptr, SW_HIDE);
                svcOk = (reinterpret_cast<INT_PTR>(r) > 32);
                if (svcOk) Sleep(3000); // wait for elevated process to register the service
            }

            if (!svcOk) {
                logAppEvent("AliceLauncherService install failed");
                return false;
            }
            logAppEvent("AliceLauncherService installed");
        }

        // ── 2. Guard: AliceLoader.exe must exist next to Alice.exe ────────────
        const QString loader = loaderExePath();
        if (!QFile::exists(loader)) {
            logAppEvent("AliceLoader.exe not found: " + loader);
            return false;
        }

        // ── 3. Place AliceLoader.lnk in shell:startup ─────────────────────────
        bool lnkOk = createStartupLnk(loader);
        logAppEvent(lnkOk ? "AliceLoader.lnk created in shell:startup"
                           : "AliceLoader.lnk creation failed");
        return lnkOk;

    } else {
        // ── 1. Remove AliceLoader.lnk ─────────────────────────────────────────
        QFile::remove(startupLnkPath());
        logAppEvent("AliceLoader.lnk removed from shell:startup");

        // ── 2. Uninstall AliceLauncherService ─────────────────────────────────
        const QString svc = serviceExePath();
        bool svcOk = false;

        if (isRunningAsAdmin()) {
            int code = runProcess(svc, "--uninstall");
            svcOk = (code == 0);
        } else {
            HINSTANCE r = ShellExecuteW(
                nullptr, L"runas",
                reinterpret_cast<const wchar_t *>(
                    QDir::toNativeSeparators(svc).utf16()),
                L"--uninstall", nullptr, SW_HIDE);
            svcOk = (reinterpret_cast<INT_PTR>(r) > 32);
        }

        logAppEvent(svcOk ? "AliceLauncherService uninstalled"
                           : "AliceLauncherService uninstall failed");
        return svcOk;
    }
}
