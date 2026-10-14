#include "mainwindow.h"
#include "aibridge.h"
#include "webbrowser.h"
#include "bubbleedit.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTime>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>
#include <QScrollBar>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>
#include "commands.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSplitter>
#include <QComboBox>
#include <QSpinBox>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QDebug>
#include <functional>
#include <memory>
#include "theme.h"
#include "toast.h"
#include "mediactrl.h"

// Windows shell-hook integration (match-found detection)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

// AliceShellFilter — intercepts WM_SHELLHOOKMESSAGE, routes window-flash events
// back to MainWindow::onWindowFlash on the Qt main thread.
class AliceShellFilter : public QAbstractNativeEventFilter
{
public:
    explicit AliceShellFilter(MainWindow *w, unsigned int msg)
        : m_win(w), m_msg(msg) {}

    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override
    {
        MSG *m = static_cast<MSG *>(message);
        if (m->message != m_msg) return false;

        // HSHELL_FLASH == HSHELL_REDRAW (6) | HSHELL_HIGHBIT (0x8000) == 0x8006
        constexpr WPARAM HSHELL_FLASH_VAL = 0x8006u;
        if (m->wParam != HSHELL_FLASH_VAL) return false;

        HWND flashHwnd = reinterpret_cast<HWND>(m->lParam);
        if (!flashHwnd) return false;

        DWORD pid = 0;
        GetWindowThreadProcessId(flashHwnd, &pid);
        if (!pid) return false;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return false;
        wchar_t buf[MAX_PATH] = {};
        DWORD sz = MAX_PATH;
        QueryFullProcessImageNameW(hProc, 0, buf, &sz);
        CloseHandle(hProc);
        QString fullPath = QString::fromWCharArray(buf);
        int slash = fullPath.lastIndexOf(QLatin1Char('\\'));
        QString exeName = (slash >= 0 ? fullPath.mid(slash + 1) : fullPath);

        // Always queued — safe cross-thread call back into Qt
        QMetaObject::invokeMethod(m_win, "onWindowFlash",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, exeName));
        return false; // don't consume — let Qt process the message too
    }

private:
    MainWindow   *m_win;
    unsigned int  m_msg;
};

// --- Global qDebug → structured log bridge ---
static std::function<void(const QString &)> s_logHandler;

// Strip return type / parameters from a raw function signature, keeping ClassName::method.
// MSVC:  "void __cdecl MainWindow::foo(void)"  → "MainWindow::foo"
// GCC:   "void MainWindow::foo()"               → "MainWindow::foo"
static QString cleanFuncName(const char *raw)
{
    if (!raw) return QStringLiteral("?");
    QString f = QString::fromLatin1(raw);
    int paren = f.indexOf(QLatin1Char('('));
    if (paren > 0) {
        int space = f.lastIndexOf(QLatin1Char(' '), paren - 1);
        f = f.mid(space + 1, paren - space - 1);
    }
    return f;
}

static void aliceMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    // Messages with no file context come from Qt internals or QtWebEngine / Chromium
    // (JavaScript console, display subsystem, etc.) — not from Alice's own code.
    // Drop Debug and Warning noise from those sources; keep Critical and Fatal.
    if (!ctx.file && (type == QtDebugMsg || type == QtWarningMsg))
        return;

    const char *levelStr = "?";
    switch (type) {
        case QtDebugMsg:    levelStr = "D"; break;
        case QtInfoMsg:     levelStr = "I"; break;
        case QtWarningMsg:  levelStr = "W"; break;
        case QtCriticalMsg: levelStr = "C"; break;
        case QtFatalMsg:    levelStr = "F"; break;
    }

    QString file = ctx.file
        ? QFileInfo(QString::fromLatin1(ctx.file)).fileName()
        : QStringLiteral("?");

    QString formatted = QStringLiteral("[%1] [%2] %3:%4 (%5) %6")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")))
        .arg(QLatin1String(levelStr))
        .arg(file)
        .arg(ctx.line)
        .arg(cleanFuncName(ctx.function))
        .arg(msg);

    if (s_logHandler) s_logHandler(formatted);
    // For QtFatalMsg Qt aborts automatically after the handler returns.
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    initSession();   // sets appLogFilePath and writes the session-start marker directly
    networkManager = new QNetworkAccessManager(this);
    // Point the bridge at writeAppLog — NOT at the logAppEvent macro (that would recurse)
    s_logHandler = [this](const QString &formatted) { writeAppLog(formatted); };
    qInstallMessageHandler(aliceMessageHandler);
    logAppEvent("Application started"); // first structured log line after handler is live

    setWindowTitle("Alice");
    setMinimumSize(900, 600);
    resize(1200, 760);
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(Theme::instance().windowBg().name()));
    if (auto *app = qobject_cast<QApplication*>(QCoreApplication::instance()))
        app->setStyleSheet(Theme::instance().globalStyleSheet());
    connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::applyTheme);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // stack must exist before setupSidebar (sidebar buttons reference it)
    stack = new AnimatedStack();
    stack->setStyleSheet(QString("background: %1;").arg(Theme::instance().inputBg().name()));
    stack->addWidget(setupChatPage());         // 0
    stack->addWidget(setupHistoryPage());      // 1
    stack->addWidget(setupHistoryEntryPage()); // 2
    stack->addWidget(setupLogsPage());         // 3
    stack->addWidget(setupTasksPage());        // 4
    stack->addWidget(setupAppProfilesPage());  // 5
    stack->addWidget(setupSettingsPage());     // 6
    stack->addWidget(setupSequencesPage());    // 7
    stack->addWidget(setupCommandsPage());     // 8

    setupTrayIcon();
    setupAiBridge();

    // ── Layout: permanent sidebar + main content (resizable splitter) ───────
    QWidget *topBar = setupTopBar();
    m_topBar = topBar;

    bodySplitter = new QSplitter(Qt::Horizontal, central);
    bodySplitter->setHandleWidth(4);
    bodySplitter->setStyleSheet(QString(
        "QSplitter::handle { background: %1; }"
        "QSplitter::handle:hover { background: %2; }")
        .arg(Theme::instance().border().name(), Theme::instance().accent().name()));
    m_sidebar = setupSidebar();
    bodySplitter->addWidget(m_sidebar);   // permanent left sidebar

    QWidget *mainArea = new QWidget();
    mainArea->setObjectName("mainArea");
    mainArea->setStyleSheet(QString("QWidget#mainArea { background-color: %1; }").arg(Theme::instance().inputBg().name()));
    QVBoxLayout *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(stack);
    bodySplitter->addWidget(mainArea);

    bodySplitter->setCollapsible(0, false);   // sidebar never collapses
    bodySplitter->setCollapsible(1, false);   // main area never collapses
    connect(bodySplitter, &QSplitter::splitterMoved, this, &MainWindow::saveSettings);

    rootLayout->addWidget(topBar);
    rootLayout->addWidget(bodySplitter, 1);

    // ── GPT drawer — floats over content, always shown (off-screen when closed) ──
    gpuDrawer = setupWebAiPanel();
    gpuDrawer->setParent(central);
    gpuDrawer->setFixedWidth(480);
    gpuDrawer->show();    // NEVER hide — must stay shown so innerText works
    gpuDrawer->raise();
    // start off-screen right (positioned in resizeEvent once window has size)

    gpuAnim = new QPropertyAnimation(gpuDrawer, "geometry", this);
    gpuAnim->setDuration(220);
    gpuAnim->setEasingCurve(QEasingCurve::OutQuart);

    loadSettings();   // stores m_savedHudX / m_savedHudY for setupHud to use
    loadProfiles();
    loadSequences();
    setupHud();       // positions HUD before show() using saved values — no race
    applyTheme();     // apply theme to stored surfaces after they exist

    centralWatcher = new QTimer(this);
    connect(centralWatcher, &QTimer::timeout, this, [this]() {
        if (watcherCallbacks.isEmpty()) {
            centralWatcher->stop();   // nothing to watch — go fully idle
            return;
        }
        for (auto it = watcherCallbacks.begin(); it != watcherCallbacks.end(); ++it) {
            bool nowRunning = Commands::isProcessRunning(it.key());
            bool wasRunning = watcherRunState.value(it.key(), false);
            if (nowRunning != wasRunning) {
                watcherRunState[it.key()] = nowRunning;
                for (auto &cb : it.value())
                    cb(nowRunning);
            }
        }
    });
    // Do NOT start the timer here — registerWatcher() starts it on demand.

    // ── Shell hook: detect window-flash events (e.g. OW match found) ──────────
    m_shellMsg = RegisterWindowMessage(L"SHELLHOOK");
    if (RegisterShellHookWindow(reinterpret_cast<HWND>(winId()))) {
        m_shellFilter = new AliceShellFilter(this, m_shellMsg);
        qApp->installNativeEventFilter(m_shellFilter);
    }
}

void MainWindow::onWindowFlash(const QString &exeName)
{
    // Ignore while cooldown is active (OW keeps flashing until match loads)
    if (m_matchCooldownActive) return;

    // Walk app profiles to find one whose exe matches the flashing window
    // and has match detection enabled.
    for (int i = 0; i < appListEntriesLayout->count(); i++) {
        QPushButton *btn = qobject_cast<QPushButton *>(appListEntriesLayout->itemAt(i)->widget());
        if (!btn) continue;

        QString profileExe = QFileInfo(btn->property("exePath").toString()).fileName();
        if (profileExe.compare(exeName, Qt::CaseInsensitive) != 0) continue;

        // Found the matching profile — look at tab 4 (Match Detect)
        if (i + 1 >= appProfileDetail->count()) break;
        QWidget *detail     = appProfileDetail->widget(i + 1);
        QStackedWidget *tabs = detail ? detail->findChild<QStackedWidget *>() : nullptr;
        if (!tabs || tabs->count() < 5) break;

        QWidget    *matchTab = tabs->widget(4);
        QPushButton *toggle  = matchTab ? matchTab->findChild<QPushButton *>("matchDetectToggle") : nullptr;
        if (!toggle || !toggle->property("toggled").toBool()) break;   // feature off

        // ── Read cooldown ────────────────────────────────────────────────────
        auto *coolSb = matchTab->findChild<QSpinBox*>("matchCooldown");
        int cooldownSec = coolSb ? coolSb->value() : 60;

        if (cooldownSec > 0) {
            m_matchCooldownActive = true;
            if (!m_matchCooldownTimer) {
                m_matchCooldownTimer = new QTimer(this);
                m_matchCooldownTimer->setSingleShot(true);
                connect(m_matchCooldownTimer, &QTimer::timeout, this,
                        [this]() { m_matchCooldownActive = false; });
            }
            m_matchCooldownTimer->start(cooldownSec * 1000);
        }

        // ── Read steps in display order from stepsContainer ──────────────────
        struct Step { QString action; int delayMs; };
        QList<Step> steps;
        QWidget *sc = matchTab->findChild<QWidget*>("stepsContainer");
        if (sc) {
            auto *scl = qobject_cast<QVBoxLayout*>(sc->layout());
            if (scl) {
                for (int j = 0; j < scl->count(); ++j) {
                    QWidget *row = scl->itemAt(j) ? scl->itemAt(j)->widget() : nullptr;
                    if (!row) continue;
                    auto *sb = row->findChild<QSpinBox*>("stepDelay");
                    steps.append({ row->property("stepAction").toString(),
                                   (sb ? sb->value() : 0) * 1000 });
                }
            }
        }

        // ── Execute steps in order, each after its own delay ─────────────────
        // Use a shared_ptr so the recursive lambda stays alive across timer callbacks.
        auto runner = std::make_shared<std::function<void(int)>>();
        *runner = [this, steps, profileExe, runner](int idx) {
            if (idx >= steps.size()) return;
            QTimer::singleShot(steps[idx].delayMs, this,
                               [this, steps, profileExe, idx, runner]() {
                const QString &act = steps[idx].action;
                if      (act == "pauseBrowser") MediaCtrl::pauseActiveBrowsers();
                else if (act == "playSpotify")  MediaCtrl::ensureSpotifyPlaying();
                else if (act == "focusWindow")  MediaCtrl::focusWindow(profileExe);
                (*runner)(idx + 1);
            });
        };
        (*runner)(0);
        break;
    }
}

void MainWindow::setupHud()
{
    m_hud = new ChatHud();

    connect(m_hud, &ChatHud::hudMoved, this, [this](const QPoint &) { saveSettings(); });

    connect(m_hud, &ChatHud::showAliceRequested, this, [this]() {
        if (isVisible()) {
            hide();   // already on screen — send to tray
        } else {
            setWindowState(windowState() & ~Qt::WindowMinimized);
            show(); raise(); activateWindow();
        }
    });

    connect(m_hud, &ChatHud::aiModeChanged, this, [this](bool on) {
        aiModeOn = on;
        if (aiModeBtn) aiModeBtn->setChecked(on);
    });

    connect(m_hud, &ChatHud::commandEntered, this, [this](const QString &text, bool aiMode) {
        // Route everything through Alice's normal pipeline (chat, tasks, quit, AI).
        if (aiModeOn != aiMode) {
            aiModeOn = aiMode;
            if (aiModeBtn) aiModeBtn->setChecked(aiMode);
        }
        inputBox->setPlainText(text);
        onSendMessage();
    });

    // Position BEFORE show() so the HUD appears at the saved spot on first frame
    if (m_savedHudX >= 0)
        m_hud->setHudPos({m_savedHudX, m_savedHudY});

    m_hud->show();
}

MainWindow::~MainWindow()
{
    // Deregister shell hook before window handle is destroyed
    if (m_shellFilter) {
        qApp->removeNativeEventFilter(m_shellFilter);
        DeregisterShellHookWindow(reinterpret_cast<HWND>(winId()));
        delete m_shellFilter;
        m_shellFilter = nullptr;
    }
    qInstallMessageHandler(nullptr); // restore Qt default before object is destroyed
    s_logHandler = nullptr;
}

void MainWindow::addMessage(const QString &text, const QString &sender)
{
    QWidget *msgWidget = new QWidget();
    QVBoxLayout *msgLayout = new QVBoxLayout(msgWidget);
    msgLayout->setContentsMargins(0, 0, 0, 0);
    msgLayout->setSpacing(3);
    QString time = QDateTime::currentDateTime().toString("HH:mm");

    if (sender == "user") {
        QLabel *label = new QLabel(userName() + "  " + time);
        label->setStyleSheet("color: #9098b0; font-size: 12px; letter-spacing: 1px;");
        label->setAlignment(Qt::AlignRight);
        msgLayout->addWidget(label);
    } else {
        QLabel *label = new QLabel(sender == "alice" ? "Alice  " + time : sender + "  " + time);
        label->setStyleSheet("color: #9098b0; font-size: 12px; letter-spacing: 1px;");
        msgLayout->addWidget(label);
    }

    BubbleEdit *bubble = new BubbleEdit();
    // Render markdown for alice/system messages (handles bold, headers,
    // lists, code blocks, emojis — plain text falls through unchanged)
    if (sender == "alice" || sender == "system") {
        bubble->setMarkdown(text);
    } else {
        bubble->setPlainText(text);
    }

    if (sender == "user") {
        bubble->setStyleSheet("QTextEdit { background: #161924; color: #ffffff; border: 1px solid #252a3a;"
                              "border-radius: 8px; font-size: 14px; padding: 0px 4px; }");
        msgLayout->setAlignment(Qt::AlignRight);
    } else if (sender == "system") {
        bubble->setStyleSheet("QTextEdit { background: #0f1a14; color: #ffffff; border: 1px solid #1a2e20;"
                              "border-radius: 8px; font-size: 13px; padding: 0px 4px; }");
    } else {
        bubble->setStyleSheet("QTextEdit { background: #13161f; color: #ffffff; border: 1px solid #1e2130;"
                              "border-radius: 8px; font-size: 14px; padding: 0px 4px; }");
        msgWidget->setContentsMargins(0, 0, 100, 0);
        msgLayout->setAlignment(Qt::AlignLeft);
    }

    msgLayout->addWidget(bubble);
    // Remove leading stretch (vertical centering) on first message
    if (outputLayout->count() > 0 && outputLayout->itemAt(0)->spacerItem()) {
        delete outputLayout->takeAt(0);
    }
    watermarkGreeting->parentWidget()->setVisible(false);
    outputLayout->insertWidget(outputLayout->count() - 1, msgWidget);
    saveMessage(text, sender);
}

void MainWindow::onSendMessage()
{
    QString text = inputBox->toPlainText().trimmed();
    if (text.isEmpty()) return;
    inputBox->clear();
    addMessage(text, "user");

    QString lower = text.toLower();

    // Exit commands — must be handled here so closeEvent flag is set before quit fires
    if (lower == "exit" || lower == "quit" || lower == "close alice") {
        m_forceQuit = true;
        QCoreApplication::quit();
        return;
    }

    // Intercept yes/no when waiting for winget upgrade confirmation
    if (wingetPending) {
        if (lower == "yes") {
            wingetPending = false;
            addMessage("Starting upgrade...", "alice");
            logAppEvent("Winget upgrade --all started");
            if (wingetOutput) {
                wingetOutput->appendPlainText("\n--- Running winget upgrade --all ---\n");
                wingetOutput->verticalScrollBar()->setValue(wingetOutput->verticalScrollBar()->maximum());
            }
            QProcess *proc = new QProcess(this);
            // Filter out winget's spinner/progress animation lines before appending.
            // A spinner line contains only whitespace and/or the characters \, |, /, -.
            auto appendFiltered = [this](const QByteArray &raw) {
                if (!wingetOutput) return;
                static const QRegularExpression spinnerRe(R"(^[\s\\|/\-]*$)");
                for (const QString &line : QString::fromLocal8Bit(raw).split('\n')) {
                    if (!spinnerRe.match(line).hasMatch())
                        wingetOutput->appendPlainText(line);
                }
                wingetOutput->verticalScrollBar()->setValue(wingetOutput->verticalScrollBar()->maximum());
            };
            connect(proc, &QProcess::readyReadStandardOutput, this, [proc, appendFiltered]() {
                appendFiltered(proc->readAllStandardOutput());
            });
            connect(proc, &QProcess::readyReadStandardError, this, [proc, appendFiltered]() {
                appendFiltered(proc->readAllStandardError());
            });
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc](int, QProcess::ExitStatus) {
                if (wingetOutput) {
                    wingetOutput->appendPlainText("\n--- Upgrade complete ---");
                    wingetOutput->verticalScrollBar()->setValue(wingetOutput->verticalScrollBar()->maximum());
                }
                addMessage("Upgrade complete.", "alice");
                logAppEvent("Winget upgrade --all finished");
                proc->deleteLater();
            });
            proc->start("winget", QStringList()
                << "upgrade" << "--all"
                << "--silent"
                << "--accept-source-agreements"
                << "--accept-package-agreements"
                << "--disable-interactivity"
                << "--include-unknown");
            return;
        } else if (lower == "no") {
            wingetPending = false;
            if (wingetTaskCard) {
                wingetTaskCard->deleteLater();
                wingetTaskCard = nullptr;
                wingetOutput   = nullptr;
            }
            addMessage("Upgrade cancelled.", "alice");
            logAppEvent("Winget upgrade cancelled");
            return;
        }
    }

    if (lower.startsWith("schedule ")) {
        QString request = text.mid(9).trimmed();
        if (request.isEmpty()) {
            addMessage("Usage: schedule <task description and time>", "alice");
            return;
        }
        addMessage("Parsing schedule...", "alice");
        QString today = QDate::currentDate().toString("yyyy-MM-dd");
        QString sysPrompt = QString(
            "Extract the task description and datetime from the user request. "
            "Today's date is %1. "
            "Reply ONLY in this exact format with no other text: TASK|||DATETIME "
            "where DATETIME is in format YYYY-MM-DD HH:MM. "
            "If only a time is given assume today. "
            "If a day name is given (e.g. Saturday) find the next occurrence. "
            "If a date like '25th' is given assume the current month unless it has passed. "
            "Example reply: restart computer|||2026-04-15 22:00"
        ).arg(today);
        sendToAIWithCallback(sysPrompt, request, [this](const QString &response) {
            // Remove "Parsing schedule..." bubble
            for (int i = outputLayout->count() - 2; i >= 0; i--) {
                QLayoutItem *li = outputLayout->itemAt(i);
                if (!li || !li->widget()) continue;
                bool removed = false;
                const auto editors = li->widget()->findChildren<QTextEdit*>();
                for (QTextEdit *ed : editors) {
                    if (ed->toPlainText() == "Parsing schedule...") {
                        delete outputLayout->takeAt(i)->widget();
                        removed = true;
                        break;
                    }
                }
                if (removed) break;
            }

            QStringList parts = response.split("|||");
            if (parts.size() != 2) {
                addMessage("Couldn't parse the schedule. Try: schedule restart at 10pm", "alice");
                return;
            }
            QString task     = parts[0].trimmed();
            QString dtString = parts[1].trimmed();
            QDateTime when   = QDateTime::fromString(dtString, "yyyy-MM-dd HH:mm");
            if (!when.isValid() || when <= QDateTime::currentDateTime()) {
                addMessage("Invalid or past datetime: " + dtString, "alice");
                return;
            }
            addScheduledTask(task, when);
            addMessage("Scheduled: \"" + task + "\" at " + when.toString("ddd dd MMM, HH:mm"), "alice");
            logAppEvent("Scheduled task: " + task + " at " + dtString);
        });
        return;
    }

    if (lower == "blackout" || lower == "private" || lower == "ghost" ||
        lower == "focus"   || lower == "stealth") {
        QStringList exes;
        for (int i = 0; i < blackoutAppsLayout->count(); ++i) {
            QWidget *w = blackoutAppsLayout->itemAt(i)->widget();
            if (!w) continue;
            QLabel *lbl = w->findChild<QLabel*>("blackoutExe");
            if (lbl && !lbl->text().isEmpty()) exes << lbl->text();
        }
        if (exes.isEmpty()) {
            addMessage("No apps in blackout list. Add some in Settings.", "alice");
            return;
        }
        for (const QString &exe : std::as_const(exes)) {
            QProcess::startDetached("taskkill", {"/F", "/T", "/IM", exe});
            logAppEvent("Blackout kill: " + exe);
        }
        addMessage("Blackout mode activated.", "alice");

    } else if (lower.startsWith("kill ")) {
        const QString query = text.mid(5).trimmed().toLower();
        if (query.isEmpty()) {
            addMessage("Usage: kill <app name>", "alice");
            return;
        }

        // Snapshot all running processes, collect matches (substring, case-insensitive,
        // no .exe required from the user).
        QStringList matched;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    QString exe = QString::fromWCharArray(pe.szExeFile);
                    // Strip .exe for comparison, keep original for taskkill
                    QString baseLower = exe.left(exe.lastIndexOf('.')).toLower();
                    if (baseLower.contains(query) || exe.toLower().contains(query)) {
                        if (!matched.contains(exe, Qt::CaseInsensitive))
                            matched << exe;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }

        if (matched.isEmpty()) {
            addMessage("No running process found matching **" + query + "**.", "alice");
            return;
        }

        if (matched.size() > 4) {
            addMessage("Too many matches for **" + query + "** — be more specific:\n" +
                       matched.mid(0, 6).join(", ") + (matched.size() > 6 ? " …" : ""), "alice");
            return;
        }

        if (matched.size() > 1) {
            addMessage("Found multiple matches:\n" + matched.join(", ") +
                       "\nType the exact name to kill one, or they will all be killed now.", "alice");
        }

        QStringList killed, failed;
        for (const QString &exe : std::as_const(matched)) {
            QProcess proc;
            proc.start("taskkill", QStringList() << "/F" << "/T" << "/IM" << exe);
            proc.waitForFinished(3000);
            if (proc.exitCode() == 0)
                killed << exe;
            else
                failed << exe;
            logAppEvent("Kill " + exe + " (exit " + QString::number(proc.exitCode()) + ")");
        }

        QString reply;
        if (!killed.isEmpty())
            reply += "Killed: " + killed.join(", ");
        if (!failed.isEmpty()) {
            if (!reply.isEmpty()) reply += "\n";
            reply += "Failed to kill: " + failed.join(", ");
        }
        addMessage(reply, "alice");

    } else if (lower.startsWith("note ")) {
        QString msg = text.mid(5).trimmed();
        if (!msg.isEmpty()) {
            bool hasContent = QFileInfo(notesFilePath).size() > 0;
            QFile f(notesFilePath);
            if (f.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&f);
                if (hasContent) out << "\n";
                out << msg << "\n";
                f.close();
                addMessage("Noted.", "alice");
                logAppEvent("Note added");
            }
        }
    } else if (lower == "open notes" || lower == "open note") {
        QFile f(notesFilePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QTextStream(&f).readAll().trimmed();
            f.close();
            addMessage(content.isEmpty() ? "No notes yet." : content, "alice");
        } else {
            addMessage("No notes yet.", "alice");
        }
    } else if (lower == "clear notes" || lower == "clear note" || lower == "delete notes" || lower == "delete note") {
        QFile f(notesFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.close();
            addMessage("Notes cleared.", "alice");
            logAppEvent("Notes cleared");
        }
    } else if (lower.startsWith("download ") || lower.startsWith("install ")) {
        const int off = lower.startsWith("download ") ? 9 : 8;
        const QString appName = text.mid(off).trimmed();
        if (appName.isEmpty()) {
            addMessage("Usage: install <app>  e.g. install vlc", "alice");
            return;
        }

        // Runs winget with the resolved package ID and shows a task card
        auto doInstall = [this](const QString &pkgId, const QString &displayName) {
            logAppEvent("Install: " + pkgId);
            addMessage("Installing \"" + displayName + "\"... check Tasks for progress.", "alice");
            setStatus("working");
            stack->setCurrentIndex(4);

            QWidget *taskCard = new QWidget();
            taskCard->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
            QHBoxLayout *tcl = new QHBoxLayout(taskCard);
            tcl->setContentsMargins(14, 10, 14, 10);
            QLabel *titleLbl  = new QLabel("Installing: " + displayName);
            titleLbl->setStyleSheet("color: #ffffff; font-size: 13px;");
            QLabel *statusLbl = new QLabel("● Working");
            statusLbl->setStyleSheet("color: #f0c040; font-size: 12px;");
            statusLbl->setAlignment(Qt::AlignRight);
            tcl->addWidget(titleLbl, 1);
            tcl->addWidget(statusLbl);
            tasksContentLayout->insertWidget(qMax(0, tasksContentLayout->count() - 1), taskCard);

            QProcess *proc = new QProcess(this);
            QPointer<QWidget> cardPtr   = taskCard;
            QPointer<QLabel>  statusPtr = statusLbl;

            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, pkgId, displayName, cardPtr, statusPtr](int code, QProcess::ExitStatus) {
                const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed();
                setStatus("ready"); updateStatus();
                QString msg;
                if (out.contains("already installed", Qt::CaseInsensitive)) {
                    msg = displayName + " is already installed.";
                    if (statusPtr) { statusPtr->setText("Already installed"); statusPtr->setStyleSheet("color:#9098b0;font-size:12px;"); }
                } else if (code == 0) {
                    msg = "✓ " + displayName + " installed successfully.";
                    if (statusPtr) { statusPtr->setText("● Done"); statusPtr->setStyleSheet("color:#50C878;font-size:12px;"); }
                    logAppEvent("Installed: " + pkgId);
                } else {
                    msg = "Could not install \"" + displayName + "\". Package ID: " + pkgId;
                    if (statusPtr) { statusPtr->setText("● Failed"); statusPtr->setStyleSheet("color:#ff4444;font-size:12px;"); }
                }
                addMessage(msg, "alice");
                if (cardPtr) QTimer::singleShot(5000, cardPtr, &QWidget::deleteLater);
                proc->deleteLater();
            });
            proc->start("winget", {"install", pkgId,
                "--accept-source-agreements", "--accept-package-agreements",
                "--silent", "--disable-interactivity"});
        };

        // Ask AI for the exact winget package ID, then install
        const QString apiKey = getActiveApiKey();
        if (!apiKey.isEmpty()) {
            addMessage("Looking up \"" + appName + "\" on winget...", "alice");
            setStatus("working");
            sendToAIWithCallback(
                "You are a Windows package manager assistant. Reply with ONLY the exact "
                "winget package ID. If the app is not on winget, reply: NOT_AVAILABLE",
                "Winget package ID for: " + appName,
                [this, appName, doInstall](const QString &resp) {
                    QString pkgId = resp.trimmed().remove('"').remove('`')
                                        .split('\n').first().trimmed();
                    if (pkgId.isEmpty() || pkgId.contains("NOT_AVAILABLE", Qt::CaseInsensitive)) {
                        addMessage("\"" + appName + "\" is not available on winget.", "alice");
                        setStatus("ready");
                        return;
                    }
                    doInstall(pkgId, appName);
                });
        } else {
            // No API key — run `winget search` to resolve the exact package ID first
            addMessage("Searching for \"" + appName + "\"...", "alice");
            setStatus("working");
            QProcess *searchProc = new QProcess(this);
            connect(searchProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, appName, doInstall, searchProc](int, QProcess::ExitStatus) {
                const QString out = QString::fromLocal8Bit(searchProc->readAllStandardOutput());
                searchProc->deleteLater();

                // winget search output after the "---" separator:
                //   Name          Id                  Version   Source
                //   Mozilla Firefox  Mozilla.Firefox  134.0.1  winget
                // Columns are separated by 2+ spaces.
                QString pkgId;
                bool pastSep = false;
                static const QRegularExpression colSplit(R"(\s{2,})");
                for (const QString &raw : out.split('\n')) {
                    QString line = raw.trimmed();
                    if (line.startsWith("---")) { pastSep = true; continue; }
                    if (!pastSep || line.isEmpty()) continue;
                    QStringList cols = line.split(colSplit, Qt::SkipEmptyParts);
                    if (cols.size() >= 2) {
                        pkgId = cols[1].trimmed();
                        break;
                    }
                }

                if (pkgId.isEmpty()) {
                    addMessage("\"" + appName + "\" was not found on winget.", "alice");
                    setStatus("ready");
                    return;
                }
                doInstall(pkgId, appName);
            });
            searchProc->start("winget", {"search", "--query", appName,
                                         "--limit", "1",
                                         "--accept-source-agreements"});
        }
        return;

    } else if (lower.startsWith("uninstall ") || lower.startsWith("remove ")) {
        const int off = lower.startsWith("uninstall ") ? 10 : 7;
        const QString appName = text.mid(off).trimmed();
        if (appName.isEmpty()) {
            addMessage("Usage: uninstall <app>  e.g. uninstall vlc", "alice");
            return;
        }

        auto doUninstall = [this](const QString &pkgId, const QString &displayName) {
            addMessage("Uninstalling \"" + displayName + "\"...", "alice");
            logAppEvent("Uninstall: " + pkgId);
            QProcess *proc = new QProcess(this);
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, pkgId, displayName](int code, QProcess::ExitStatus) {
                if (code == 0)
                    addMessage("✓ \"" + displayName + "\" uninstalled.", "alice");
                else
                    addMessage("Could not uninstall \"" + displayName + "\". Is it installed?", "alice");
                logAppEvent("Uninstall exit " + QString::number(code) + ": " + pkgId);
                proc->deleteLater();
            });
            proc->start("winget", {"uninstall", pkgId,
                "--silent", "--accept-source-agreements", "--disable-interactivity"});
        };

        const QString apiKey = getActiveApiKey();
        if (!apiKey.isEmpty()) {
            addMessage("Looking up \"" + appName + "\"...", "alice");
            sendToAIWithCallback(
                "You are a Windows package manager assistant. Reply with ONLY the exact "
                "winget package ID. If the app is not on winget, reply: NOT_AVAILABLE",
                "Winget package ID for: " + appName,
                [this, appName, doUninstall](const QString &resp) {
                    QString pkgId = resp.trimmed().remove('"').remove('`')
                                        .split('\n').first().trimmed();
                    if (pkgId.isEmpty() || pkgId.contains("NOT_AVAILABLE", Qt::CaseInsensitive)) {
                        addMessage("\"" + appName + "\" was not found on winget.", "alice");
                        return;
                    }
                    doUninstall(pkgId, appName);
                });
        } else {
            // No API key — search winget for the exact package ID first
            addMessage("Searching for \"" + appName + "\"...", "alice");
            QProcess *searchProc = new QProcess(this);
            connect(searchProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, appName, doUninstall, searchProc](int, QProcess::ExitStatus) {
                const QString out = QString::fromLocal8Bit(searchProc->readAllStandardOutput());
                searchProc->deleteLater();
                QString pkgId;
                bool pastSep = false;
                static const QRegularExpression colSplitU(R"(\s{2,})");
                for (const QString &raw : out.split('\n')) {
                    QString line = raw.trimmed();
                    if (line.startsWith("---")) { pastSep = true; continue; }
                    if (!pastSep || line.isEmpty()) continue;
                    QStringList cols = line.split(colSplitU, Qt::SkipEmptyParts);
                    if (cols.size() >= 2) { pkgId = cols[1].trimmed(); break; }
                }
                if (pkgId.isEmpty()) {
                    addMessage("\"" + appName + "\" was not found on winget.", "alice");
                    return;
                }
                doUninstall(pkgId, appName);
            });
            searchProc->start("winget", {"search", "--query", appName,
                                         "--limit", "1", "--accept-source-agreements"});
        }
        return;

    } else if (aiModeOn && lower.startsWith("run ")) {
        // AI mode + "run <task>" → ask GPT for the exact command, then execute it
        QString runTask = text.mid(4).trimmed();
        if (integratedWebAiToggleBtn->property("toggled").toBool()) {
            // Web AI (GPT in browser) path
            m_webAiRunPending = true;
            const QString cmdPrompt =
                "Reply with ONLY the Windows command to execute for this task. "
                "No explanation, no markdown, no code fences, no backticks. "
                "If PowerShell is required, start your reply with 'powershell: '. "
                "If a regular CMD command is fine, just give the raw command. "
                "Task: " + runTask;
            addMessage("Thinking...", "alice");
            setStatus("working");
            aiBridge->sendPrompt(cmdPrompt, integratedWebAiProviderCombo->currentText());
        } else {
            // Gemini API path — execute=true already handles command-only extraction and running
            sendToAI(runTask, true);
        }
    } else if (aiModeOn) {
        sendToAI(text, false);   // AI button is on — whole message goes to AI
    } else if (lower == "update apps") {
        logAppEvent("Command: " + text);
        startWingetListing();
    } else {
        QString result = Commands::parse(text);
        if (!result.isEmpty()) {
            logAppEvent("Command: " + text);
            addMessage(result, "alice");
            if (text.toLower().startsWith("shutdown ")) {
                int minutes = text.mid(9).trimmed().toInt();
                if (minutes > 0) addTask("Shutdown", minutes * 60, true);
            }
            if (text.toLower() == "cancel shutdown") {
                for (int i = 0; i < tasksContentLayout->count(); ++i) {
                    QLayoutItem *item = tasksContentLayout->itemAt(i);
                    if (item && item->widget() && item->widget()->objectName() == "Shutdown") {
                        QWidget *w = tasksContentLayout->takeAt(i)->widget();
                        w->deleteLater();
                        QTimer::singleShot(0, this, &MainWindow::updateStatus);
                        break;
                    }
                }
            }
        } else {
            // Check manual-trigger sequences
            bool ranSequence = false;
            for (int i = 0; i < seqListEntriesLayout->count(); i++) {
                QPushButton *seqBtn = qobject_cast<QPushButton*>(seqListEntriesLayout->itemAt(i)->widget());
                if (!seqBtn) continue;
                if (seqBtn->text().toLower() == lower && i + 1 < seqDetail->count()) {
                    QWidget *seqDetailWidget = seqDetail->widget(i + 1);
                    QComboBox *tCombo = seqDetailWidget->findChild<QComboBox*>("triggerTypeCombo");
                    if (tCombo && tCombo->currentText() == "Manual") {
                        executeSequenceActions(getSequenceActions(i + 1), 0);
                        addMessage("Running sequence: " + seqBtn->text(), "alice");
                        logAppEvent("Manual sequence: " + seqBtn->text());
                        ranSequence = true;
                        break;
                    }
                }
            }
            if (!ranSequence)
                addMessage("Unknown command.", "alice");
        }
    }
}

void MainWindow::onNewSession()
{
    // If the watermark is still visible the chat is already empty — nothing to do.
    QWidget *watermark = watermarkGreeting->parentWidget();
    if (watermark->isVisible()) {
        stack->setCurrentIndex(0);
        return;
    }

    // Remove all message widgets.  Iterate backward so indices stay valid.
    // Skip spacer items and the watermark container — never delete those.
    for (int i = outputLayout->count() - 1; i >= 0; --i) {
        QLayoutItem *it = outputLayout->itemAt(i);
        if (!it || it->spacerItem()) continue;
        if (it->widget() == watermark)   continue;
        QLayoutItem *taken = outputLayout->takeAt(i);
        delete taken->widget();
        delete taken;
    }
    // Re-add leading stretch if addMessage() removed it (it does on the first message).
    if (outputLayout->count() == 0 || !outputLayout->itemAt(0)->spacerItem())
        outputLayout->insertStretch(0);
    watermark->setVisible(true);
    stack->setCurrentIndex(0);

    // Write session-start marker to history.txt
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    QFile hf(historyFilePath);
    if (hf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&hf);
        out << "\n[" << timestamp << "] \u2500\u2500 Session Start \u2500\u2500\n";
        hf.close();
    }

    // Write session-start marker into app log
    QFile alf(appLogFilePath);
    if (alf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&alf);
        out << "\n[" << timestamp << "] \u2500\u2500 Session Start \u2500\u2500\n";
        alf.close();
    }
    logAppEvent("New session started");

    // Integrated Web AI: navigate the embedded browser to a fresh chat page
    if (integratedWebAiToggleBtn->property("toggled").toBool()) {
        aiBridge->newChat(integratedWebAiProviderCombo->currentText());
        logAppEvent("Integrated Web AI: new chat requested");
    }
}

void MainWindow::initSession()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base + "/sessions");
    QDir().mkpath(base + "/logs");
    QDir().mkpath(base + "/config");
    configFilePath = base + "/config/settings.json";
    profilesFilePath = base + "/config/profiles.json";
    sequencesFilePath = base + "/config/sequences.json";
    historyFilePath = base + "/history.txt";
    appLogFilePath  = base + "/applog.txt";
    notesFilePath       = base + "/notes.txt";

    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    sessionFilePath = base + "/sessions/" + date + ".json";
    logFilePath = base + "/logs/" + date + ".txt";

    // Write session separator into session file if it already exists
    if (QFile::exists(sessionFilePath)) {
        QFile f(sessionFilePath);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&f);
            out << "\n---\n";
            f.close();
        }
    }

    // Write session-start marker into persistent history file
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    QFile hf(historyFilePath);
    if (hf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&hf);
        out << "\n[" << timestamp << "] \u2500\u2500 Session Start \u2500\u2500\n";
        hf.close();
    }

    // Write session-start marker into app log (written directly \u2014 not through logAppEvent
    // because the structured handler isn't installed yet at this point in the constructor)
    QFile alf(appLogFilePath);
    if (alf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&alf);
        out << "\n[" << timestamp << "] \u2500\u2500 Session Start \u2500\u2500\n";
        alf.close();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_forceQuit) {
        event->accept();   // real quit — let it close
        return;
    }
    // X button — minimize to tray
    event->ignore();
    hide();
    trayIcon->showMessage("Alice", "Alice is running in the background.", QSystemTrayIcon::NoIcon, 2000);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!gpuDrawer) return;

    const int topH  = 42;
    const int inpH  = inputArea ? inputArea->height() : 54;
    const int h     = height() - topH - inpH;
    const int gw    = gpuDrawer->width();

    if (gptDrawerOpen)
        gpuDrawer->setGeometry(width() - gw, topH, gw, h);
    else
        gpuDrawer->setGeometry(width() + gw, topH, gw, h); // off-screen right, but shown
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // ── Input box: Enter sends, Shift+Enter inserts newline ─────────────────
    if (obj == inputBox && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && !(ke->modifiers() & Qt::ShiftModifier)) {
            onSendMessage();
            return true;
        }
    }

    // ── Chat scroll: middle-click auto-scroll + selection edge-scroll ───────
    if (chatScroll && obj == chatScroll->viewport()) {
        auto *sb = chatScroll->verticalScrollBar();

        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::MiddleButton) {
                if (m_midScrollActive) {
                    // Second click stops
                    m_midScrollActive = false;
                    m_edgeScrollTimer->stop();
                    chatScroll->viewport()->setCursor(Qt::ArrowCursor);
                } else {
                    m_midScrollActive = true;
                    m_midScrollOrigin = me->globalPosition().toPoint();
                    chatScroll->viewport()->setCursor(Qt::SizeVerCursor);
                    m_edgeScrollTimer->start();
                }
                return true;
            }
            if (me->button() == Qt::LeftButton) {
                m_edgeScrollTimer->start(); // start edge-scroll watch for selection
            }
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !m_midScrollActive)
                m_edgeScrollTimer->stop();
            if (me->button() == Qt::RightButton && m_midScrollActive) {
                m_midScrollActive = false;
                m_edgeScrollTimer->stop();
                chatScroll->viewport()->setCursor(Qt::ArrowCursor);
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_midScrollActive) {
            auto *me = static_cast<QMouseEvent*>(event);
            int dy = me->globalPosition().toPoint().y() - m_midScrollOrigin.y();
            // Dead zone ±8px, then proportional speed
            if (qAbs(dy) > 8)
                sb->setValue(sb->value() + dy / 5);
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setStatus(const QString &status)
{
    if (status == "ready") {
        statusLabel->setText("● Ready");
        statusLabel->setStyleSheet("color: #50C878; font-size: 12px;");
    } else if (status == "working") {
        statusLabel->setText("● Working");
        statusLabel->setStyleSheet("color: #f0c040; font-size: 12px;");
    } else if (status == "error") {
        statusLabel->setText("● Error");
        statusLabel->setStyleSheet("color: #ff4444; font-size: 12px;");
    }
}

void MainWindow::updateWatermark()
{
    watermarkGreeting->setText("Hello, " + userName());
}

void MainWindow::updateStatus()
{
    for (int i = 0; i < tasksContentLayout->count(); i++) {
        QLayoutItem *item = tasksContentLayout->itemAt(i);
        if (!item || !item->widget()) continue;                          // skip spacers / sub-layouts
        if (qobject_cast<QLabel*>(item->widget())) continue;             // skip section labels
        if (item->widget()->property("activeTask").toBool() == false) continue; // skip non-active cards
        setStatus("working");
        return;
    }
    setStatus("ready");
}

QString MainWindow::userName()
{
    QString n = nameEdit->text().trimmed();
    return n.isEmpty() ? "User" : n;
}

void MainWindow::registerWatcher(const QString &exe, std::function<void(bool)> cb)
{
    if (exe.isEmpty()) return;
    watcherCallbacks[exe].append(std::move(cb));
    // Start the timer only when there is actually something to watch.
    if (centralWatcher && !centralWatcher->isActive())
        centralWatcher->start(5000);
}

// Custom page that allows OAuth popup windows (e.g. "Sign in with Google")
class LoginPage : public QWebEnginePage
{
public:
    explicit LoginPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent) {}

protected:
    QWebEnginePage *createWindow(WebWindowType) override
    {
        // OAuth flows open a popup — give it its own visible view
        auto *popup     = new LoginPage(this->profile(), this);
        auto *popupView = new QWebEngineView();
        popupView->setAttribute(Qt::WA_DeleteOnClose);
        popupView->setPage(popup);
        popupView->resize(520, 640);
        popupView->setWindowTitle("Login");
        popupView->show();
        // When the popup closes, delete its page too
        connect(popupView, &QWebEngineView::destroyed, popup, &QObject::deleteLater);
        return popup;
    }
};

void MainWindow::applyTheme()
{
    Theme &T   = Theme::instance();
    const QString bg    = T.inputBg().name();
    const QString wbg   = T.windowBg().name();
    const QString surf  = T.surface().name();
    const QString surfa = T.surfaceAlt().name();
    const QString brd   = T.border().name();
    const QString txt   = T.text().name();
    const QString muted = T.textMuted().name();
    const QString acc   = T.accent().name();
    const QString side  = T.sidebarBg().name();
    const QString top   = T.topBarBg().name();

    // ── Named surfaces ───────────────────────────────────────────────────────
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(wbg));
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        app->setStyleSheet(T.globalStyleSheet());

    if (m_topBar)
        m_topBar->setStyleSheet(
            QString("background-color: %1; border-bottom: 1px solid %2;").arg(top, brd));
    if (m_taskRunnerLabel)
        m_taskRunnerLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(muted));
    if (m_sidebar)
        m_sidebar->setStyleSheet(
            QString("QWidget { background-color: %1; border-right: 1px solid %2; }").arg(side, brd));
    if (stack)
        stack->setStyleSheet(QString("background: %1;").arg(bg));
    if (bodySplitter)
        bodySplitter->setStyleSheet(QString(
            "QSplitter::handle { background: %1; }"
            "QSplitter::handle:hover { background: %2; }").arg(brd, acc));
    if (auto *main = findChild<QWidget *>("mainArea"))
        main->setStyleSheet(QString("QWidget#mainArea { background-color: %1; }").arg(bg));

    // ── QScrollArea pages ────────────────────────────────────────────────────
    const QString scrollQss = QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollArea > QWidget > QWidget { background: %1; }").arg(bg);
    for (QScrollArea *sa : findChildren<QScrollArea *>()) {
        sa->setStyleSheet(scrollQss);
        if (sa->widget())
            sa->widget()->setStyleSheet(QString("background: %1;").arg(bg));
    }

    // ── Comprehensive palette replacement across ALL 9 themes ────────────────
    //
    // Two-stage approach:
    //   Stage 1 — global replace for dark-theme palette values.
    //     These hex codes never appear as foreground text colors, so a simple
    //     string replace is safe.
    //   Stage 2 — background/border-only replace for Light-theme palette values.
    //     #ffffff etc. are also used as "color: #ffffff" on dark themes, so we
    //     must only replace them when they appear in background/border CSS
    //     properties, not in standalone "color:" declarations.

    struct ColorMap { QStringList from; QString to; };

    // Stage 1: dark-theme colors — safe to replace everywhere
    const QList<ColorMap> darkPalette = {
        // surface (card / panel backgrounds)
        {{ "#13161f","#1d2030","#0d1a28","#0d1a0f","#1a0d18","#161820","#1a0d16","#071214" }, surf },
        // surfaceAlt (hover, active-nav)
        {{ "#181c28","#262a3a","#122030","#122014","#20121e","#1e2028","#20121a","#091618" }, surfa },
        // border
        {{ "#1e2130","#2a2e40","#1a2e42","#1a2e1e","#2e1a2a","#262830","#2e1a24","#0e2428","#1a1d27" }, brd },
        // inputBg / page background
        {{ "#0c0e15","#08121e","#08120a","#120810","#10121a","#12080e","#040a0c" }, bg },
        // windowBg
        {{ "#0a0c12","#15171f","#060e18","#060e09","#100810","#0e1014","#050c0e" }, wbg },
        // sidebarBg
        {{ "#0d0f17","#081018","#08100a","#10080e","#0c0e12","#10080c","#04090b" }, side },
        // nav inactive text / sidebar section labels
        {{ "#6b7494","#787ea1" }, muted },
        // topBar
        {{ "#000000","#070a10","#10131c","#040c14","#040c06","#0c040c","#0a0c10","#0c0408","#02070a" }, top },
    };

    // Stage 2: Light-theme colors — context-aware (background/border only).
    // Uses a regex that matches the color only when it follows a background
    // or border CSS property keyword, so "color: #ffffff" is left untouched
    // while "background: #ffffff" and "border: 1px solid #d8dbe8" are updated.
    const QList<ColorMap> lightPalette = {
        {{ "#ffffff"  }, surf  },   // Light surface
        {{ "#eceef5"  }, surfa },   // Light surfaceAlt
        {{ "#d8dbe8"  }, brd   },   // Light border
        {{ "#f7f8fc"  }, bg    },   // Light inputBg
        {{ "#f3f4f8"  }, wbg   },   // Light windowBg
        {{ "#e8eaf2"  }, side  },   // Light sidebarBg
    };

    // Regex template: matches <color> only after a background/border keyword
    // e.g.  "background: #ffffff"  ✓     "color: #ffffff"  ✗
    auto bgOnlyReplace = [](const QString &css,
                             const QString &from, const QString &to) -> QString {
        QRegularExpression re(
            "((?:background(?:-color)?|border(?:-[a-z]+)?)\\s*:[^;{}]*?)"
            + QRegularExpression::escape(from),
            QRegularExpression::CaseInsensitiveOption);
        return QString(css).replace(re, "\\1" + to);
    };

    for (QWidget *w : findChildren<QWidget *>()) {
        const QString s = w->styleSheet();
        if (s.isEmpty()) continue;
        QString ns = s;

        // Stage 1: global replace (dark themes)
        for (const ColorMap &cm : darkPalette)
            for (const QString &old : cm.from)
                ns.replace(old, cm.to, Qt::CaseInsensitive);

        // Stage 2: background-only replace (Light theme leftovers)
        for (const ColorMap &cm : lightPalette)
            for (const QString &old : cm.from)
                ns = bgOnlyReplace(ns, old, cm.to);

        if (ns != s) w->setStyleSheet(ns);
    }

    // ── Explicitly re-style the API section (may be disabled, which makes Qt's
    //    disabled-palette overlay fight the string-replace pass above) ─────────
    if (apiSectionWidget)
        apiSectionWidget->setStyleSheet(
            QString("background: %1; border-radius: 8px; border: 1px solid %2;")
            .arg(surf, brd));
    if (auto *ah = findChild<QWidget *>("apiHeader"))
        ah->setStyleSheet(
            QString("background: %1; border-radius: 8px;").arg(bg));

    // ── Theme pills: sync checked state + restyle ───────────────────────────
    // setupSettingsPage() runs before loadSettings(), so pills are initialised
    // with the default mode.  applyTheme() (called after loadSettings()) fixes
    // both the checked state and the visual style in one pass.
    const Theme::Mode curMode = T.mode();
    for (QPushButton *pill : findChildren<QPushButton *>("themePill")) {
        const bool active =
            static_cast<Theme::Mode>(pill->property("themeMode").toInt()) == curMode;
        if (pill->isChecked() != active)
            pill->setChecked(active);
        if (active) {
            pill->setStyleSheet(QString(
                "QPushButton { background: %1; color: white; border: none;"
                " border-radius: 8px; padding: 7px 14px; font-size: 12px; font-weight: 600; }")
                .arg(acc));
        } else {
            pill->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                " border-radius: 8px; padding: 7px 14px; font-size: 12px; }"
                "QPushButton:hover { border-color: %3; color: %3; }")
                .arg(T.textDim().name(), brd, txt));
        }
    }

    // ── Nav buttons: re-apply live-colour styles so theme switches keep them
    //    consistent.  Active button uses navActiveStyle(), all others use
    //    navInactiveStyle() — both read live Theme::instance() colours. ─────────
    for (QPushButton *btn : findChildren<QPushButton *>("navItem"))
        btn->setStyleSheet(btn == activeNavBtn ? navActiveStyle() : navInactiveStyle());
}

void MainWindow::showWebAiLogin()
{
    const QString provider = integratedWebAiProviderCombo->currentText();
    QString url;
    if (provider == "Claude")      url = "https://claude.ai/new";
    else if (provider == "Gemini") url = "https://gemini.google.com/";
    else                           url = "https://chatgpt.com/";

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Login — " + provider);
    dlg->resize(1100, 750);
    dlg->setStyleSheet("background: #0c0e15;");
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Info bar at the top
    QWidget *infoBar = new QWidget();
    infoBar->setFixedHeight(36);
    infoBar->setStyleSheet("background: #13161f; border-bottom: 1px solid #1e2130;");
    auto *infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(14, 0, 14, 0);
    QLabel *infoLabel = new QLabel(
        "Log in to " + provider + " — Alice remembers your session. Close this window when done.");
    infoLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
    infoLayout->addWidget(infoLabel);

    // Visible browser using the same persistent profile as AliceBrowser
    auto *view = new QWebEngineView(dlg);
    auto *page = new LoginPage(aiBridge->browser()->profile(), view);
    view->setPage(page);
    view->load(QUrl(url));

    layout->addWidget(infoBar);
    layout->addWidget(view);

    dlg->setWindowFlags(Qt::Window);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    view->setFocus();

    connect(dlg, &QDialog::finished, this, [this, provider]() {
        logAppEvent("Web AI login window closed for " + provider);
    });
}
