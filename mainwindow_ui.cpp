#include "mainwindow.h"
#include "aibridge.h"
#include "webbrowser.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QScrollBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QPainter>
#include <QApplication>
#include <QCursor>
#include <QSystemTrayIcon>
#include <QFont>
#include <QSizePolicy>
#include <QSplitter>
#include <QPropertyAnimation>
#include <QDialog>
#include <QStandardPaths>
#include <QProcess>
#include "theme.h"
#include "toast.h"
#include <QButtonGroup>
#include <QAbstractButton>

// ── Nav-button style helpers ──────────────────────────────────────────────────
// Called at click-time (not setup-time) so they always use the current theme.

QString MainWindow::navActiveStyle()
{
    const Theme &T = Theme::instance();
    return QString(
        "QPushButton { color: %1; font-size: 12px; background: %2;"
        " border-radius: 6px; padding: 10px 10px; text-align: left; }"
        "QPushButton:hover { background: %2; }")
        .arg(T.text().name(), T.surfaceAlt().name());
}

QString MainWindow::navInactiveStyle()
{
    const Theme &T = Theme::instance();
    return QString(
        "QPushButton { color: %1; font-size: 12px; background: transparent;"
        " border: none; padding: 10px 10px; text-align: left; }"
        "QPushButton:hover { background: %2; color: %3; border-radius: 6px; }")
        .arg(T.textMuted().name(), T.surfaceAlt().name(), T.textDim().name());
}

QWidget *MainWindow::setupTopBar()
{
    QWidget *topBar = new QWidget();
    topBar->setFixedHeight(42);
    topBar->setStyleSheet("background-color: #000000; border-bottom: 1px solid #1e2130;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 0, 16, 0);

    m_taskRunnerLabel = new QLabel("Personal Task Runner");
    m_taskRunnerLabel->setStyleSheet("color: #9098b0; font-size: 11px;");
    QLabel *subLeft = m_taskRunnerLabel;

    QPushButton *title = new QPushButton("A L I C E");
    title->setFlat(true);
    title->setCursor(Qt::PointingHandCursor);
    title->setStyleSheet("QPushButton { color: #ffffff; font-size: 15px; letter-spacing: 6px;"
                         "background: transparent; border: none; }");
    connect(title, &QPushButton::clicked, this, [this]() {
        stack->setCurrentIndex(0);
        if (activeNavBtn) {
            activeNavBtn->setStyleSheet(navInactiveStyle());
            activeNavBtn = nullptr;
        }
    });

    statusLabel = new QPushButton("● Ready");
    statusLabel->setFlat(true);
    statusLabel->setCursor(Qt::PointingHandCursor);
    statusLabel->setToolTip("Click to refresh");
    statusLabel->setStyleSheet(
        "QPushButton { color: #50C878; font-size: 12px; background: transparent;"
        "border: none; padding: 0px; }");
    connect(statusLabel, &QPushButton::clicked, this, [this]() {
        statusLabel->setText("● Refreshing...");
        statusLabel->setStyleSheet(
            "QPushButton { color: #9098b0; font-size: 12px; background: transparent;"
            "border: none; padding: 0px; }");
        QTimer::singleShot(1000, this, [this]() { setStatus("ready"); });
    });

    // GPT drawer toggle button
    gptDrawerBtn = new QPushButton("⊞");
    gptDrawerBtn->setFlat(true);
    gptDrawerBtn->setFixedSize(36, 36);
    gptDrawerBtn->setCursor(Qt::PointingHandCursor);
    gptDrawerBtn->setToolTip("Toggle ChatGPT panel");
    gptDrawerBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 16px; background: transparent; border: none; }"
        "QPushButton:hover { color: #4a9eff; }"
        "QPushButton:checked { color: #4a9eff; }");
    gptDrawerBtn->setCheckable(true);
    connect(gptDrawerBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!gpuDrawer || !gpuAnim) return;
        const int topH = 42;
        const int inpH = inputArea ? inputArea->height() : 54;
        const int h    = height() - topH - inpH;
        const int gw   = gpuDrawer->width();
        gptDrawerOpen  = on;
        if (on) {
            gpuDrawer->raise();
            // Snap to just off right, then slide in
            gpuDrawer->setGeometry(width() + gw, topH, gw, h);
            gpuAnim->setStartValue(QRect(width() + gw, topH, gw, h));
            gpuAnim->setEndValue  (QRect(width() - gw, topH, gw, h));
        } else {
            gpuAnim->setStartValue(QRect(width() - gw, topH, gw, h));
            gpuAnim->setEndValue  (QRect(width() + gw, topH, gw, h));
        }
        gpuAnim->start();
    });

    // Left panel — expands equally with right panel so ALICE stays centred
    // regardless of whether the task-runner label is visible or not.
    QWidget *leftPanel = new QWidget();
    leftPanel->setStyleSheet("background: transparent;");
    QHBoxLayout *leftLay = new QHBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->addWidget(subLeft);
    leftLay->addStretch();

    // Right panel — mirrors left panel's stretch weight
    QWidget *rightPanel = new QWidget();
    rightPanel->setStyleSheet("background: transparent;");
    QHBoxLayout *rightLay = new QHBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->addStretch();
    rightLay->addWidget(statusLabel);
    rightLay->addWidget(gptDrawerBtn);

    topLayout->addWidget(leftPanel,  1);   // stretch = 1
    topLayout->addWidget(title,      0);   // fixed width, perfectly centred
    topLayout->addWidget(rightPanel, 1);   // stretch = 1  ← balances left

    return topBar;
}

QWidget *MainWindow::setupSidebar()
{
    QWidget *sidebar = new QWidget();
    sidebar->setMinimumWidth(80);
    sidebar->setStyleSheet("background-color: #13161f; border-right: 1px solid #1e2130;");
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 14, 10, 14);
    sideLayout->setSpacing(2);

    auto makeNavItem = [this](const QString &text, int index, bool active = false) {
        QPushButton *item = new QPushButton(text);
        item->setContentsMargins(10, 7, 10, 7);
        item->setFlat(true);
        item->setCursor(Qt::PointingHandCursor);
        item->setObjectName("navItem");
        item->setStyleSheet(active ? navActiveStyle() : navInactiveStyle());
        connect(item, &QPushButton::clicked, this, [this, index, item]() {
            if (stack->currentIndex() == index) {
                stack->setCurrentIndex(0);
                if (activeNavBtn) {
                    activeNavBtn->setStyleSheet(navInactiveStyle());
                }
                activeNavBtn = nullptr;
            } else {
                if (activeNavBtn) {
                    activeNavBtn->setStyleSheet(navInactiveStyle());
                }
                activeNavBtn = item;
                item->setStyleSheet(navActiveStyle());
                stack->setCurrentIndex(index);
                if (index == 1) refreshHistory();
                if (index == 3) refreshAppLogs();
            }
        });
        return item;
    };

    auto makeSectionLabel = [](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setStyleSheet("color: #787ea1; font-size: 10px; letter-spacing: 2px;");
        lbl->setContentsMargins(6, 0, 6, 4);
        return lbl;
    };

    QPushButton *newSessionBtn = makeNavItem("New Session", 0, true);
    connect(newSessionBtn, &QPushButton::clicked, this, &MainWindow::onNewSession);
    sideLayout->addWidget(newSessionBtn);
    sideLayout->addSpacing(6);

    QFrame *div1 = new QFrame();
    div1->setFrameShape(QFrame::HLine);
    div1->setStyleSheet("color: #1a1d27;");
    sideLayout->addWidget(div1);
    sideLayout->addSpacing(4);

    sideLayout->addWidget(makeSectionLabel("WORKSPACE"));
    sideLayout->addWidget(makeNavItem("History", 1));
    sideLayout->addWidget(makeNavItem("Logs", 3));
    sideLayout->addWidget(makeNavItem("Tasks", 4));
    sideLayout->addSpacing(6);

    QFrame *div2 = new QFrame();
    div2->setFrameShape(QFrame::HLine);
    div2->setStyleSheet("color: #1a1d27;");
    sideLayout->addWidget(div2);
    sideLayout->addSpacing(4);

    sideLayout->addWidget(makeSectionLabel("SYSTEM"));
    sideLayout->addWidget(makeNavItem("App Profiles", 5));
    sideLayout->addWidget(makeNavItem("Sequences", 7));
    sideLayout->addWidget(makeNavItem("Settings", 6));
    sideLayout->addStretch();

    QPushButton *commandsBtn = new QPushButton("Commands");
    commandsBtn->setFlat(true);
    commandsBtn->setCursor(Qt::PointingHandCursor);
    commandsBtn->setObjectName("navItem");
    commandsBtn->setStyleSheet(navInactiveStyle());
    connect(commandsBtn, &QPushButton::clicked, this, [this, commandsBtn]() {
        if (stack->currentIndex() == 8) {
            // toggle off
            commandsBtn->setStyleSheet(navInactiveStyle());
            activeNavBtn = nullptr;
            stack->setCurrentIndex(0);
        } else {
            if (activeNavBtn) {
                activeNavBtn->setStyleSheet(navInactiveStyle());
            }
            activeNavBtn = commandsBtn;
            commandsBtn->setStyleSheet(navActiveStyle());
            stack->setCurrentIndex(8);
        }
    });
    sideLayout->addWidget(commandsBtn);

    return sidebar;
}

QWidget *MainWindow::setupChatPage()
{
    QWidget *chatView = new QWidget();
    chatView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *chatViewLayout = new QVBoxLayout(chatView);
    chatViewLayout->setContentsMargins(0,0,0,0);
    chatViewLayout->setSpacing(0);

    chatScroll = new QScrollArea();
    chatScroll->setWidgetResizable(true);
    chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chatScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                              "QScrollBar:vertical { width: 0px; }");

    QWidget *outputWidget = new QWidget();
    outputWidget->setStyleSheet("background-color: #0c0e15;");
    outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(20, 20, 20, 10);
    outputLayout->setSpacing(14);

    // Watermark — shown when chat is empty
    QWidget *watermark = new QWidget();
    watermark->setStyleSheet("background: transparent;");
    QVBoxLayout *watermarkLayout = new QVBoxLayout(watermark);
    watermarkLayout->setContentsMargins(0, 0, 0, 0);
    watermarkLayout->setSpacing(40);
    watermarkLayout->setAlignment(Qt::AlignCenter);

    watermarkGreeting = new QLabel("Hello, User");
    watermarkGreeting->setStyleSheet("color: #c8d0e8; font-size: 22px; font-weight: 400;");
    watermarkGreeting->setAlignment(Qt::AlignHCenter);
    watermarkGreeting->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Commands guide — two-column grid (command | description)
    QWidget *guideWidget = new QWidget();
    guideWidget->setStyleSheet("background: transparent;");
    QGridLayout *guideGrid = new QGridLayout(guideWidget);
    guideGrid->setContentsMargins(0, 0, 0, 0);
    guideGrid->setVerticalSpacing(6);
    guideGrid->setHorizontalSpacing(40);

    const QString cmdStyle  = "color: #ffffff; font-size: 13px; font-family: monospace;";
    const QString descStyle = "color: #6b7494; font-size: 13px;";

    auto addGuideRow = [&](int row, const QString &cmd, const QString &desc) {
        QLabel *cmdLbl  = new QLabel(cmd);
        QLabel *descLbl = new QLabel(desc);
        cmdLbl->setStyleSheet(cmdStyle);
        descLbl->setStyleSheet(descStyle);
        cmdLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        descLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        guideGrid->addWidget(cmdLbl,  row, 0);
        guideGrid->addWidget(descLbl, row, 1);
    };

    addGuideRow(0, "shutdown <minutes>", "schedule a shutdown");
    addGuideRow(1, "update apps",        "check and install updates");
    addGuideRow(2, "download <app>",     "install an app via winget");
    addGuideRow(3, "uninstall <app>",    "uninstall one or more apps");
    addGuideRow(4, "run <task>",         "ask AI to run a command  (AI button on)");
    addGuideRow(5, "note <message>",     "save a note");
    addGuideRow(6, "schedule <task>",    "schedule a reminder");
    addGuideRow(7, "exit / quit",        "close Alice");

    // Centre the grid horizontally inside the watermark
    QHBoxLayout *guideCentreLayout = new QHBoxLayout();
    guideCentreLayout->setContentsMargins(0, 0, 0, 0);
    guideCentreLayout->addStretch();
    guideCentreLayout->addWidget(guideWidget);
    guideCentreLayout->addStretch();

    watermarkLayout->addWidget(watermarkGreeting);
    watermarkLayout->addLayout(guideCentreLayout);
    outputLayout->addStretch();
    outputLayout->addWidget(watermark);
    outputLayout->addStretch();

    chatScroll->setWidget(outputWidget);
    connect(chatScroll->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int max) {
        chatScroll->verticalScrollBar()->setValue(max);
    });

    // Middle-click scroll + selection edge-scroll
    chatScroll->viewport()->installEventFilter(this);
    m_edgeScrollTimer = new QTimer(this);
    m_edgeScrollTimer->setInterval(25);
    connect(m_edgeScrollTimer, &QTimer::timeout, this, [this]() {
        if (!QApplication::mouseButtons().testFlag(Qt::LeftButton) && !m_midScrollActive) {
            m_edgeScrollTimer->stop();
            return;
        }
        QPoint local = chatScroll->viewport()->mapFromGlobal(QCursor::pos());
        int h = chatScroll->viewport()->height();
        const int edge = 50;
        int speed = 0;
        if (local.y() < edge)
            speed = -((edge - local.y()) * 3 / edge + 1) * 6;
        else if (local.y() > h - edge)
            speed = ((local.y() - (h - edge)) * 3 / edge + 1) * 6;
        if (speed != 0)
            chatScroll->verticalScrollBar()->setValue(
                chatScroll->verticalScrollBar()->value() + speed);
    });

    // Input area — height grows with content (up to ~5 lines)
    inputArea = new QWidget();
    inputArea->setStyleSheet("background-color: #0c0e15; border-top: 1px solid #1e2130;");
    QHBoxLayout *inputLayout = new QHBoxLayout(inputArea);
    inputLayout->setContentsMargins(14, 8, 14, 8);
    inputLayout->setSpacing(8);

    inputBox = new QPlainTextEdit();
    inputBox->setMinimumHeight(38);
    inputBox->setMaximumHeight(140);
    inputBox->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputBox->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    inputBox->setStyleSheet("QPlainTextEdit { background: #13161f; border: 1px solid #1e2130;"
                            "border-radius: 8px; padding: 8px 12px; font-size: 12px; color: #ffffff; }"
                            "QPlainTextEdit:focus { border-color: #252a3a; }");
    inputBox->installEventFilter(this);

    // Auto-expand height as text wraps
    connect(inputBox->document(), &QTextDocument::contentsChanged, this, [this]() {
        int lineH  = inputBox->fontMetrics().lineSpacing();
        int blocks = inputBox->document()->blockCount();
        int newH   = qBound(38, lineH * blocks + 20, 140);
        inputBox->setFixedHeight(newH);
        inputArea->setFixedHeight(newH + 16);
    });
    // AI mode toggle button
    aiModeBtn = new QPushButton("AI");
    aiModeBtn->setFixedSize(36, 36);
    aiModeBtn->setCheckable(true);
    aiModeBtn->setCursor(Qt::PointingHandCursor);
    aiModeBtn->setStyleSheet(
        "QPushButton { background: #13161f; color: #9098b0; border: 1px solid #1e2130;"
        "border-radius: 18px; font-size: 12px; font-weight: bold; }"
        "QPushButton:checked { background: #1a2a3a; color: #4a9eff; border-color: #4a9eff; }");
    connect(aiModeBtn, &QPushButton::toggled, this, [this](bool on) {
        aiModeOn = on;
        if (m_hud) m_hud->setAiMode(on);   // keep HUD AI button in sync
    });

    // Set initial height
    inputArea->setFixedHeight(54);
    inputLayout->addWidget(inputBox);
    inputLayout->addWidget(aiModeBtn);

    chatViewLayout->addWidget(chatScroll);
    chatViewLayout->addWidget(inputArea);

    return chatView;
}

QWidget *MainWindow::setupHistoryPage()
{
    QWidget *historyView = new QWidget();
    historyView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *historyLayout = new QVBoxLayout(historyView);
    historyLayout->setContentsMargins(20, 20, 20, 20);
    historyLayout->setSpacing(0);

    QHBoxLayout *historyHeaderLayout = new QHBoxLayout();
    QLabel *historyTitle = new QLabel("History");
    historyTitle->setStyleSheet("color: #c8d0e8; font-size: 28px; font-weight: 500;");
    QPushButton *historyRefreshBtn = new QPushButton("Refresh");
    historyRefreshBtn->setFlat(true);
    historyRefreshBtn->setCursor(Qt::PointingHandCursor);
    historyRefreshBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 4px 10px;"
                                     "border: 1px solid #1e2130; border-radius: 5px; }"
                                     "QPushButton:hover { color: #ffffff; background: #181c28; }");
    connect(historyRefreshBtn, &QPushButton::clicked, this, &MainWindow::refreshHistory);

    QPushButton *historyClearBtn = new QPushButton("Clear");
    historyClearBtn->setFlat(true);
    historyClearBtn->setCursor(Qt::PointingHandCursor);
    historyClearBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 4px 10px;"
                                   "border: 1px solid #1e2130; border-radius: 5px; }"
                                   "QPushButton:hover { color: #ff4444; background: #1a1010; }");
    connect(historyClearBtn, &QPushButton::clicked, this, [this]() {
        QFile f(historyFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.close();
            refreshHistory();
            logAppEvent("History cleared");
        }
    });

    historyHeaderLayout->addWidget(historyTitle);
    historyHeaderLayout->addStretch();
    historyHeaderLayout->addWidget(historyRefreshBtn);
    historyHeaderLayout->addSpacing(6);
    historyHeaderLayout->addWidget(historyClearBtn);
    historyLayout->addLayout(historyHeaderLayout);
    historyLayout->addSpacing(14);

    historyScroll = new QScrollArea();
    historyScroll->setWidgetResizable(true);
    historyScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                                 "QScrollBar:vertical { width: 4px; background: #0c0e15; }"
                                 "QScrollBar::handle:vertical { background: #1e2130; border-radius: 2px; }");

    QWidget *historyContent = new QWidget();
    historyContent->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *historyLogLayout = new QVBoxLayout(historyContent);
    historyLogLayout->setContentsMargins(0, 0, 0, 0);

    historyTextLabel = new QLabel();
    historyTextLabel->setWordWrap(true);
    historyTextLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    historyTextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    historyTextLabel->setStyleSheet("color: #9098b0; font-size: 12px; font-family: monospace; line-height: 1.6;");
    historyLogLayout->addWidget(historyTextLabel);
    historyLogLayout->addStretch();

    historyScroll->setWidget(historyContent);
    historyLayout->addWidget(historyScroll);

    return historyView;
}

QWidget *MainWindow::setupHistoryEntryPage()
{
    QWidget *historyEntryView = new QWidget();
    historyEntryView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *historyEntryLayout = new QVBoxLayout(historyEntryView);
    historyEntryLayout->setContentsMargins(20, 20, 20, 20);
    historyEntryLayout->setSpacing(0);

    historyViewTitle = new QLabel("Session");
    historyViewTitle->setStyleSheet("color: #c8d0e8; font-size: 14px; font-weight: 500; padding-bottom: 14px;");
    historyEntryLayout->addWidget(historyViewTitle);

    historyEntryScroll = new QScrollArea();
    historyEntryScroll->setWidgetResizable(true);
    historyEntryScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                                      "QScrollBar:vertical { width: 4px; background: #0c0e15; }"
                                      "QScrollBar::handle:vertical { background: #1e2130; border-radius: 2px; }");

    QWidget *historyEntryContent = new QWidget();
    historyEntryContent->setStyleSheet("background: #0c0e15;");
    historyViewLayout = new QVBoxLayout(historyEntryContent);
    historyViewLayout->setContentsMargins(0, 0, 0, 0);
    historyViewLayout->setSpacing(14);
    historyViewLayout->addStretch();

    historyEntryScroll->setWidget(historyEntryContent);
    historyEntryLayout->addWidget(historyEntryScroll);

    return historyEntryView;
}

QWidget *MainWindow::setupLogsPage()
{
    QWidget *logsView = new QWidget();
    logsView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *logsLayout = new QVBoxLayout(logsView);
    logsLayout->setContentsMargins(20, 20, 20, 20);
    logsLayout->setSpacing(0);

    QHBoxLayout *logsHeaderLayout = new QHBoxLayout();
    QLabel *logsTitle = new QLabel("Logs");
    logsTitle->setStyleSheet("color: #c8d0e8; font-size: 28px; font-weight: 500;");
    QPushButton *logsRefreshBtn = new QPushButton("Refresh");
    logsRefreshBtn->setFlat(true);
    logsRefreshBtn->setCursor(Qt::PointingHandCursor);
    logsRefreshBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 4px 10px;"
                                  "border: 1px solid #1e2130; border-radius: 5px; }"
                                  "QPushButton:hover { color: #ffffff; background: #181c28; }");
    connect(logsRefreshBtn, &QPushButton::clicked, this, &MainWindow::refreshAppLogs);

    QPushButton *logsClearBtn = new QPushButton("Clear");
    logsClearBtn->setFlat(true);
    logsClearBtn->setCursor(Qt::PointingHandCursor);
    logsClearBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 4px 10px;"
                                "border: 1px solid #1e2130; border-radius: 5px; }"
                                "QPushButton:hover { color: #ff4444; background: #1a1010; }");
    connect(logsClearBtn, &QPushButton::clicked, this, [this]() {
        QFile f(appLogFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.close();
            refreshAppLogs();
            logAppEvent("Logs cleared");
        }
    });

    logsHeaderLayout->addWidget(logsTitle);
    logsHeaderLayout->addStretch();
    logsHeaderLayout->addWidget(logsRefreshBtn);
    logsHeaderLayout->addSpacing(6);
    logsHeaderLayout->addWidget(logsClearBtn);
    logsLayout->addLayout(logsHeaderLayout);
    logsLayout->addSpacing(14);

    logsScroll = new QScrollArea();
    logsScroll->setWidgetResizable(true);
    logsScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                              "QScrollBar:vertical { width: 4px; background: #0c0e15; }"
                              "QScrollBar::handle:vertical { background: #1e2130; border-radius: 2px; }");

    QWidget *logsContent = new QWidget();
    logsContent->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *logsContentLayout = new QVBoxLayout(logsContent);
    logsContentLayout->setContentsMargins(0, 0, 0, 0);

    appLogsTextLabel = new QLabel();
    appLogsTextLabel->setWordWrap(true);
    appLogsTextLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    appLogsTextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    appLogsTextLabel->setStyleSheet("color: #9098b0; font-size: 12px; font-family: monospace; line-height: 1.6;");
    logsContentLayout->addWidget(appLogsTextLabel);
    logsContentLayout->addStretch();

    logsScroll->setWidget(logsContent);
    logsLayout->addWidget(logsScroll);

    return logsView;
}

QWidget *MainWindow::setupTasksPage()
{
    QWidget *tasksView = new QWidget();
    tasksView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *tasksLayout = new QVBoxLayout(tasksView);
    tasksLayout->setContentsMargins(20, 20, 20, 20);
    tasksLayout->setSpacing(0);

    // ── Header: title + clear button ─────────────────────────────────────────
    QWidget *tasksHeader = new QWidget();
    tasksHeader->setStyleSheet("background: transparent;");
    QHBoxLayout *tasksHeaderLay = new QHBoxLayout(tasksHeader);
    tasksHeaderLay->setContentsMargins(0, 0, 0, 14);
    tasksHeaderLay->setSpacing(8);
    QLabel *tasksTitle = new QLabel("Tasks");
    tasksTitle->setStyleSheet("color: #c8d0e8; font-size: 14px; font-weight: 500;");
    tasksHeaderLay->addWidget(tasksTitle);
    tasksHeaderLay->addStretch();
    QPushButton *clearTasksBtn = new QPushButton("Clear All");
    clearTasksBtn->setFlat(true);
    clearTasksBtn->setCursor(Qt::PointingHandCursor);
    clearTasksBtn->setStyleSheet(
        "QPushButton { color: #ff5577; font-size: 12px; padding: 3px 10px;"
        " border: 1px solid #ff5577; border-radius: 4px; }"
        "QPushButton:hover { background: #2a0a12; }");
    connect(clearTasksBtn, &QPushButton::clicked, this, [this]() {
        while (tasksContentLayout->count() > 0) {
            QLayoutItem *item = tasksContentLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        while (scheduledTasksLayout->count() > 0) {
            QLayoutItem *item = scheduledTasksLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        updateStatus();
        Toast::show(this, "All tasks cleared", Toast::Success);
    });
    tasksHeaderLay->addWidget(clearTasksBtn);
    tasksLayout->addWidget(tasksHeader);

    QScrollArea *tasksScroll = new QScrollArea();
    tasksScroll->setWidgetResizable(true);
    tasksScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                               "QScrollBar:vertical { width: 4px; background: #0c0e15; }"
                               "QScrollBar::handle:vertical { background: #1e2130; border-radius: 2px; }");

    QWidget *tasksContent = new QWidget();
    tasksContent->setStyleSheet("background: #0c0e15;");
    tasksContentLayout = new QVBoxLayout(tasksContent);
    tasksContentLayout->setContentsMargins(0, 0, 0, 0);
    tasksContentLayout->setSpacing(10);

    QLabel *noTasks = new QLabel("No active tasks.");
    noTasks->setStyleSheet("color: #9098b0; font-size: 12px;");
    tasksContentLayout->addWidget(noTasks);
    tasksContentLayout->addStretch();

    // Scheduled section
    QLabel *scheduledTitle = new QLabel("Scheduled");
    scheduledTitle->setObjectName("scheduledSectionTitle");
    scheduledTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; padding-top: 14px; padding-bottom: 6px;");
    tasksContentLayout->addWidget(scheduledTitle);

    scheduledTasksLayout = new QVBoxLayout();
    scheduledTasksLayout->setContentsMargins(0, 0, 0, 0);
    scheduledTasksLayout->setSpacing(6);
    tasksContentLayout->addLayout(scheduledTasksLayout);

    QLabel *noScheduled = new QLabel("No scheduled tasks.");
    noScheduled->setObjectName("noScheduledLabel");
    noScheduled->setStyleSheet("color: #9098b0; font-size: 12px;");
    scheduledTasksLayout->addWidget(noScheduled);
    tasksContentLayout->addStretch();

    tasksScroll->setWidget(tasksContent);
    tasksLayout->addWidget(tasksScroll);

    return tasksView;
}

QWidget *MainWindow::setupAppProfilesPage()
{
    QWidget *appProfilesView = new QWidget();
    appProfilesView->setStyleSheet("background: #0c0e15;");
    QHBoxLayout *appProfilesMainLayout = new QHBoxLayout(appProfilesView);
    appProfilesMainLayout->setContentsMargins(0, 0, 0, 0);
    appProfilesMainLayout->setSpacing(0);

    // Left panel - app list
    QWidget *appListPanel = new QWidget();
    appListPanel->setFixedWidth(200);
    appListPanel->setStyleSheet("background: #11131a; border-right: 1px solid #1e2130;");
    QVBoxLayout *appListLayout = new QVBoxLayout(appListPanel);
    appListLayout->setContentsMargins(0, 0, 0, 0);
    appListLayout->setSpacing(0);

    QLabel *appListTitle = new QLabel("APP PROFILES");
    appListTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; padding: 16px 14px 8px;");
    appListLayout->addWidget(appListTitle);

    appListEntriesLayout = new QVBoxLayout();
    appListEntriesLayout->setContentsMargins(0, 0, 0, 0);
    appListEntriesLayout->setSpacing(0);
    appListLayout->addLayout(appListEntriesLayout);

    QPushButton *addAppBtn = new QPushButton("+ Add App");
    addAppBtn->setFlat(true);
    addAppBtn->setCursor(Qt::PointingHandCursor);
    addAppBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px;"
                             "text-align: left; border: 1px dashed #1e2130; margin: 10px; border-radius: 6px; }"
                             "QPushButton:hover { color: #9098b0; border-color: #9098b0; }");
    connect(addAppBtn, &QPushButton::clicked, this, &MainWindow::onAddAppProfile);
    appListLayout->addWidget(addAppBtn);
    appListLayout->addStretch();

    // Right panel - profile detail
    appProfileDetail = new QStackedWidget();
    appProfileDetail->setStyleSheet("background: #0c0e15;");

    QWidget *noAppSelected = new QWidget();
    noAppSelected->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *noAppLayout = new QVBoxLayout(noAppSelected);
    QLabel *noAppLabel = new QLabel("Select an app to configure");
    noAppLabel->setStyleSheet("color: #9098b0; font-size: 13px;");
    noAppLabel->setAlignment(Qt::AlignCenter);
    noAppLayout->addWidget(noAppLabel);
    appProfileDetail->addWidget(noAppSelected);

    appProfilesMainLayout->addWidget(appListPanel);
    appProfilesMainLayout->addWidget(appProfileDetail);

    return appProfilesView;
}

QScrollArea *MainWindow::setupSettingsPage()
{
    QScrollArea *settingsScroll = new QScrollArea();
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsScroll->setStyleSheet("background: #0c0e15; border: none;");

    QWidget *settingsView = new QWidget();
    settingsView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsView);
    settingsLayout->setContentsMargins(20, 20, 20, 20);
    settingsLayout->setSpacing(20);

    QLabel *settingsTitle = new QLabel("Settings");
    settingsTitle->setStyleSheet("color: #c8d0e8; font-size: 28px; font-weight: 500;");
    settingsLayout->addWidget(settingsTitle);

    QLabel *nameLabel = new QLabel("YOUR NAME");
    nameLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(nameLabel);

    QWidget *nameRow = new QWidget();
    nameRow->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QHBoxLayout *nameRowLayout = new QHBoxLayout(nameRow);
    nameRowLayout->setContentsMargins(14, 10, 14, 10);
    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("Enter your name");
    nameEdit->setStyleSheet("QLineEdit { background: transparent; border: none; color: #ffffff; font-size: 13px; }"
                            "QLineEdit:focus { border: none; }");
    QPushButton *nameSaveBtn = new QPushButton("Save");
    nameSaveBtn->setFlat(true);
    nameSaveBtn->setCursor(Qt::PointingHandCursor);
    nameSaveBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
                               "border: 1px solid #1e2130; border-radius: 4px; }"
                               "QPushButton:hover { color: #ffffff; background: #181c28; }");
    connect(nameSaveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        updateWatermark();
    });
    nameRowLayout->addWidget(nameEdit, 1);
    nameRowLayout->addWidget(nameSaveBtn);
    settingsLayout->addWidget(nameRow);

    // ── APPEARANCE ───────────────────────────────────────────────────────────
    QLabel *appearanceLabel = new QLabel("APPEARANCE");
    appearanceLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(appearanceLabel);

    QWidget *appearanceCard = new QWidget();
    appearanceCard->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *appLay = new QVBoxLayout(appearanceCard);
    appLay->setContentsMargins(14, 14, 14, 14);
    appLay->setSpacing(14);

    // ── Theme grid (3 columns × 3 rows = 9 themes) ──────────────────────────
    QLabel *themeSectionLbl = new QLabel("Theme");
    themeSectionLbl->setStyleSheet("color: #ffffff; font-size: 13px; border: none;");
    appLay->addWidget(themeSectionLbl);

    QWidget *themeGrid = new QWidget();
    themeGrid->setObjectName("themeGrid");
    QGridLayout *themeGridLay = new QGridLayout(themeGrid);
    themeGridLay->setContentsMargins(0, 0, 0, 0);
    themeGridLay->setSpacing(8);

    auto restyleThemePills = [themeGrid]() {
        for (QPushButton *p : themeGrid->findChildren<QPushButton*>("themePill")) {
            bool on = p->isChecked();
            p->setStyleSheet(on
                ? QString("QPushButton { background: %1; color: white; border: none;"
                          "border-radius: 8px; padding: 7px 14px; font-size: 12px; font-weight: 600; }")
                      .arg(Theme::instance().accent().name())
                : "QPushButton { background: transparent; color: #c8d0e8; border: 1px solid #2a2e40;"
                  "border-radius: 8px; padding: 7px 14px; font-size: 12px; }"
                  "QPushButton:hover { border-color: #ffffff30; color: #ffffff; }");
        }
    };

    auto *themePillGroup = new QButtonGroup(themeGrid);
    themePillGroup->setExclusive(true);
    const QList<QPair<QString, Theme::Mode>> themeDefs = {
        {"Midnight", Theme::Midnight}, {"Dark",   Theme::Dark},   {"Light",  Theme::Light},
        {"Ocean",    Theme::Ocean},    {"Forest", Theme::Forest},  {"Sunset", Theme::Sunset},
        {"Slate",    Theme::Slate},    {"Rose",   Theme::Rose},    {"Neon",   Theme::Neon}};
    for (int i = 0; i < themeDefs.size(); ++i) {
        auto &[lbl, mode] = themeDefs[i];
        QPushButton *b = new QPushButton(lbl);
        b->setObjectName("themePill");
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(true);
        b->setChecked(Theme::instance().mode() == mode);
        b->setProperty("themeMode", static_cast<int>(mode));
        themeGridLay->addWidget(b, i / 3, i % 3);
        themePillGroup->addButton(b);
    }
    restyleThemePills();
    connect(themePillGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, [this, restyleThemePills](QAbstractButton *btn) {
                Theme::instance().setMode(static_cast<Theme::Mode>(btn->property("themeMode").toInt()));
                restyleThemePills();
                saveSettings();
                Toast::show(this, QString("Theme: %1").arg(btn->text()), Toast::Success);
            });
    appLay->addWidget(themeGrid);

    // ── Animations toggle ────────────────────────────────────────────────────
    QWidget *animRow = new QWidget();
    QHBoxLayout *animLay = new QHBoxLayout(animRow);
    animLay->setContentsMargins(0, 0, 0, 0);
    animLay->setSpacing(8);
    QLabel *animLbl = new QLabel("Animations");
    animLbl->setStyleSheet("color: #ffffff; font-size: 13px; border: none;");
    animLbl->setMinimumWidth(120);
    animLay->addWidget(animLbl);
    QPushButton *animToggle = new QPushButton(Theme::instance().animated() ? "ON" : "OFF");
    animToggle->setCursor(Qt::PointingHandCursor);
    animToggle->setProperty("on", Theme::instance().animated());
    auto restyleAnim = [animToggle]() {
        bool on = animToggle->property("on").toBool();
        animToggle->setText(on ? "ON" : "OFF");
        animToggle->setStyleSheet(on
            ? "QPushButton { color: #50C878; font-size: 12px; padding: 4px 14px;"
              "border: 1px solid #50C878; border-radius: 14px; }"
              "QPushButton:hover { background: #0a1a10; }"
            : "QPushButton { color: #9098b0; font-size: 12px; padding: 4px 14px;"
              "border: 1px solid #2a2e40; border-radius: 14px; }"
              "QPushButton:hover { color: #ffffff; }");
    };
    restyleAnim();
    connect(animToggle, &QPushButton::clicked, this, [this, animToggle, restyleAnim]() {
        bool on = !animToggle->property("on").toBool();
        animToggle->setProperty("on", on);
        Theme::instance().setAnimated(on);
        restyleAnim();
        saveSettings();
    });
    animLay->addWidget(animToggle);
    animLay->addStretch();
    appLay->addWidget(animRow);

    settingsLayout->addWidget(appearanceCard);

    QLabel *apiLabel = new QLabel("API KEYS");
    apiLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(apiLabel);

    apiSectionWidget = new QWidget();
    apiSectionWidget->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QWidget *apiTableWidget = apiSectionWidget;
    QVBoxLayout *apiTableLayout = new QVBoxLayout(apiTableWidget);
    apiTableLayout->setContentsMargins(0, 0, 0, 0);
    apiTableLayout->setSpacing(0);

    QWidget *apiHeader = new QWidget();
    apiHeader->setObjectName("apiHeader");
    apiHeader->setStyleSheet("background: #0c0e15; border-radius: 8px;");
    QHBoxLayout *apiHeaderLayout = new QHBoxLayout(apiHeader);
    apiHeaderLayout->setContentsMargins(14, 10, 14, 10);
    QLabel *h1 = new QLabel("Name"); h1->setStyleSheet("color: #9098b0; font-size: 12px;");
    QLabel *h2 = new QLabel("Model"); h2->setStyleSheet("color: #9098b0; font-size: 12px;");
    QLabel *h3 = new QLabel("Active"); h3->setStyleSheet("color: #9098b0; font-size: 12px;");
    apiHeaderLayout->addWidget(h1, 2);
    apiHeaderLayout->addWidget(h2, 2);
    apiHeaderLayout->addWidget(h3, 1);
    apiTableLayout->addWidget(apiHeader);

    apiEntriesLayout = new QVBoxLayout();
    apiEntriesLayout->setContentsMargins(0, 0, 0, 0);
    apiEntriesLayout->setSpacing(0);
    apiTableLayout->addLayout(apiEntriesLayout);

    QPushButton *addApiBtn = new QPushButton("+ Add API Key");
    connect(addApiBtn, &QPushButton::clicked, this, [this]() {
        QDialog *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Add API Key");
        dialog->setStyleSheet("background: #13161f; color: #ffffff;");
        QVBoxLayout *dl = new QVBoxLayout(dialog);
        dl->setSpacing(10);
        QLineEdit *nameEdit = new QLineEdit(); nameEdit->setPlaceholderText("Name (e.g. Claude main)");
        QLineEdit *keyEdit = new QLineEdit(); keyEdit->setPlaceholderText("API Key");
        QLineEdit *modelEdit = new QLineEdit(); modelEdit->setPlaceholderText("Model (e.g. claude-sonnet-4-6)");
        QString inputStyle = "QLineEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; color: #ffffff; }";
        nameEdit->setStyleSheet(inputStyle);
        keyEdit->setStyleSheet(inputStyle);
        modelEdit->setStyleSheet(inputStyle);
        QPushButton *saveBtn = new QPushButton("Add");
        saveBtn->setStyleSheet("QPushButton { background: #181c28; color: #ffffff; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; }"
                               "QPushButton:hover { background: #252a3a; }");
        connect(saveBtn, &QPushButton::clicked, this, [this, dialog, nameEdit, keyEdit, modelEdit]() {
            if (nameEdit->text().isEmpty() || keyEdit->text().isEmpty()) return;
            addApiEntry(nameEdit->text(), keyEdit->text(), modelEdit->text(), false);
            saveSettings();
            dialog->accept();
        });
        dl->addWidget(nameEdit);
        dl->addWidget(keyEdit);
        dl->addWidget(modelEdit);
        dl->addWidget(saveBtn);
        dialog->exec();
    });
    addApiBtn->setFlat(true);
    addApiBtn->setCursor(Qt::PointingHandCursor);
    addApiBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; border-top: 1px solid #1e2130; }"
                             "QPushButton:hover { color: #ffffff; }");
    apiTableLayout->addWidget(addApiBtn);
    settingsLayout->addWidget(apiTableWidget);

    const QString comboStyle =
        "QComboBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
        "color: #ffffff; font-size: 12px; padding: 3px 10px; min-width: 110px; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox QAbstractItemView { background: #13161f; border: 1px solid #1e2130;"
        "color: #ffffff; selection-background-color: #181c28; }";

    // --- INTEGRATED WEB AI ---
    QLabel *intWebAiLabel = new QLabel("INTEGRATED WEB AI");
    intWebAiLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(intWebAiLabel);

    QWidget *intWebAiRow = new QWidget();
    intWebAiRow->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QHBoxLayout *intWebAiRowLayout = new QHBoxLayout(intWebAiRow);
    intWebAiRowLayout->setContentsMargins(14, 12, 14, 12);

    QLabel *intWebAiDesc = new QLabel("Embedded browser — no external browser required");
    intWebAiDesc->setStyleSheet("color: #ffffff; font-size: 13px;");

    integratedWebAiProviderCombo = new QComboBox();
    integratedWebAiProviderCombo->addItems({"Claude", "ChatGPT", "Gemini"});
    integratedWebAiProviderCombo->setStyleSheet(comboStyle);
    integratedWebAiProviderCombo->setFont(QFont("Segoe UI", 10));
    integratedWebAiProviderCombo->setEnabled(false);

    integratedWebAiToggleBtn = new QPushButton("OFF");
    integratedWebAiToggleBtn->setFlat(true);
    integratedWebAiToggleBtn->setCursor(Qt::PointingHandCursor);
    integratedWebAiToggleBtn->setProperty("toggled", false);
    integratedWebAiToggleBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
        "border: 1px solid #1e2130; border-radius: 4px; }"
        "QPushButton:hover { color: #ffffff; background: #181c28; }");

    connect(integratedWebAiToggleBtn, &QPushButton::clicked, this, [this]() {
        bool on = !integratedWebAiToggleBtn->property("toggled").toBool();
        integratedWebAiToggleBtn->setProperty("toggled", on);
        integratedWebAiToggleBtn->setText(on ? "ON" : "OFF");
        integratedWebAiToggleBtn->setStyleSheet(on
            ? "QPushButton { color: #50C878; font-size: 12px; padding: 3px 10px;"
              "border: 1px solid #50C878; border-radius: 4px; }"
              "QPushButton:hover { background: #0a1a10; }"
            : "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
              "border: 1px solid #1e2130; border-radius: 4px; }"
              "QPushButton:hover { color: #ffffff; background: #181c28; }");

        integratedWebAiProviderCombo->setEnabled(on);
        apiSectionWidget->setEnabled(!on);
        saveSettings();
    });

    connect(integratedWebAiProviderCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        saveSettings();
    });

    QPushButton *webAiLoginBtn = new QPushButton("Login");
    webAiLoginBtn->setFlat(true);
    webAiLoginBtn->setCursor(Qt::PointingHandCursor);
    webAiLoginBtn->setToolTip("Open a browser window to log in — Alice remembers your session");
    webAiLoginBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
        "border: 1px solid #1e2130; border-radius: 4px; }"
        "QPushButton:hover { color: #ffffff; background: #181c28; }");
    connect(webAiLoginBtn, &QPushButton::clicked, this, &MainWindow::showWebAiLogin);

    intWebAiRowLayout->addWidget(intWebAiDesc, 1);
    intWebAiRowLayout->addWidget(integratedWebAiProviderCombo);
    intWebAiRowLayout->addSpacing(8);
    intWebAiRowLayout->addWidget(webAiLoginBtn);
    intWebAiRowLayout->addSpacing(8);
    intWebAiRowLayout->addWidget(integratedWebAiToggleBtn);
    settingsLayout->addWidget(intWebAiRow);

    // --- BLACKOUT APPS ---
    QLabel *blackoutLabel = new QLabel("BLACKOUT APPS");
    blackoutLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(blackoutLabel);

    QWidget *blackoutTable = new QWidget();
    blackoutTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *blackoutTableLayout = new QVBoxLayout(blackoutTable);
    blackoutTableLayout->setContentsMargins(0, 0, 0, 0);
    blackoutTableLayout->setSpacing(0);

    blackoutAppsLayout = new QVBoxLayout();
    blackoutAppsLayout->setContentsMargins(0, 0, 0, 0);
    blackoutAppsLayout->setSpacing(0);
    blackoutTableLayout->addLayout(blackoutAppsLayout);

    QPushButton *addBlackoutBtn = new QPushButton("+ Add App");
    addBlackoutBtn->setFlat(true);
    addBlackoutBtn->setCursor(Qt::PointingHandCursor);
    addBlackoutBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; border-top: 1px solid #1e2130; }"
        "QPushButton:hover { color: #ffffff; }");
    connect(addBlackoutBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select App", "C:/", "Executables (*.exe)");
        if (path.isEmpty()) return;
        QString exe = QFileInfo(path).fileName();

        QWidget *row = new QWidget();
        row->setObjectName("blackoutRow");
        row->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 10, 14, 10);

        QLabel *nameLabel = new QLabel(QFileInfo(path).baseName());
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
        saveSettings();
    });
    blackoutTableLayout->addWidget(addBlackoutBtn);
    settingsLayout->addWidget(blackoutTable);

    QLabel *folderLabel = new QLabel("Maintenance Folder");
    folderLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(folderLabel);

    QWidget *folderRow = new QWidget();
    folderRow->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QHBoxLayout *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(14, 10, 14, 10);
    folderPath = new QLabel();
    folderPath->setStyleSheet("color: #ffffff; font-size: 12px;");
    folderPath->setText(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/maintenance");
    QPushButton *showFolderBtn = new QPushButton("Show Folder");
    showFolderBtn->setFlat(true);
    showFolderBtn->setCursor(Qt::PointingHandCursor);
    showFolderBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 4px 10px; border: 1px solid #1e2130; border-radius: 6px; }"
                                 "QPushButton:hover { color: #ffffff; }");
    connect(showFolderBtn, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath->text()));
    });
    folderLayout->addWidget(folderPath, 1);
    folderLayout->addWidget(showFolderBtn);
    settingsLayout->addWidget(folderRow);

    QLabel *startupLabel = new QLabel("STARTUP");
    startupLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(startupLabel);

    QWidget *startupRow = new QWidget();
    startupRow->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QHBoxLayout *startupRowLayout = new QHBoxLayout(startupRow);
    startupRowLayout->setContentsMargins(14, 10, 14, 10);
    QLabel *startupDesc = new QLabel("Run Alice automatically when Windows starts");
    startupDesc->setStyleSheet("color: #ffffff; font-size: 13px;");
    startupToggleBtn = new QPushButton();
    startupToggleBtn->setFlat(true);
    startupToggleBtn->setCursor(Qt::PointingHandCursor);
    startupToggleBtn->setFixedWidth(90);
    connect(startupToggleBtn, &QPushButton::clicked, this, [this]() {
        // Disable during the blocking schtasks call so the user can't double-click.
        startupToggleBtn->setEnabled(false);
        startupToggleBtn->setText("...");

        const bool wantEnabled = !isAutoStartupEnabled();   // one schtasks /query
        const bool ok          = setAutoStartup(wantEnabled);

        // For admin builds setAutoStartup is synchronous — verify reality.
        // For non-admin (debug) it spawns an elevated process and returns immediately,
        // so we trust the intent if ShellExecute succeeded (ok == true).
        bool actuallyEnabled;
        if (isRunningAsAdmin()) {
            actuallyEnabled = isAutoStartupEnabled();       // one more schtasks /query
        } else {
            // Elevated helper is still running; optimistically reflect intent.
            // The 500 ms timer in loadSettings will correct the button on next open.
            actuallyEnabled = ok && wantEnabled;
        }

        auto enabledStyle = QStringLiteral(
            "QPushButton { color: #50C878; font-size: 12px; padding: 3px 10px; border: 1px solid #1e2130; border-radius: 4px; }"
            "QPushButton:hover { background: #181c28; }");
        auto disabledStyle = QStringLiteral(
            "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px; border: 1px solid #1e2130; border-radius: 4px; }"
            "QPushButton:hover { color: #ffffff; background: #181c28; }");

        startupToggleBtn->setEnabled(true);
        startupToggleBtn->setText(actuallyEnabled ? "● Enabled" : "Disabled");
        startupToggleBtn->setStyleSheet(actuallyEnabled ? enabledStyle : disabledStyle);

        if (!ok) {
            Toast::show(this, "Startup registration failed", Toast::Error);
        } else {
            Toast::show(this,
                actuallyEnabled ? "Startup task registered (runs at logon)" : "Auto-startup disabled",
                Toast::Success);
        }
    });
    // Initial state shown as placeholder — loadSettings() queries schtasks asynchronously
    // (500 ms after startup) and updates this button then.
    startupToggleBtn->setText("...");
    startupToggleBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px; border: 1px solid #1e2130; border-radius: 4px; }"
        "QPushButton:hover { color: #ffffff; background: #181c28; }");
    startupRowLayout->addWidget(startupDesc, 1);
    startupRowLayout->addWidget(startupToggleBtn);
    settingsLayout->addWidget(startupRow);

    // ── INTERFACE ─────────────────────────────────────────────────────────────
    QLabel *interfaceLabel = new QLabel("INTERFACE");
    interfaceLabel->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    settingsLayout->addWidget(interfaceLabel);

    QWidget *interfaceCard = new QWidget();
    interfaceCard->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *ifLay = new QVBoxLayout(interfaceCard);
    ifLay->setContentsMargins(0, 0, 0, 0);
    ifLay->setSpacing(0);

    // ── Row 1: Personal Task Runner label toggle ─────────────────────────────
    {
        QWidget *row = new QWidget();
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 10, 14, 10);
        QLabel *desc = new QLabel("Show \"Personal Task Runner\" in top bar");
        desc->setStyleSheet("color: #ffffff; font-size: 13px;");
        rl->addWidget(desc, 1);

        QPushButton *btn = new QPushButton();
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedWidth(60);
        bool ptrOn = m_taskRunnerLabel ? m_taskRunnerLabel->isVisible() : true;
        auto stylePtrBtn = [btn](bool on) {
            btn->setText(on ? "ON" : "OFF");
            btn->setStyleSheet(on
                ? "QPushButton { color: #50C878; font-size: 12px; padding: 3px 10px;"
                  "border: 1px solid #50C878; border-radius: 4px; }"
                  "QPushButton:hover { background: #0a1a10; }"
                : "QPushButton { color: #9098b0; font-size: 12px; padding: 3px 10px;"
                  "border: 1px solid #1e2130; border-radius: 4px; }"
                  "QPushButton:hover { color: #ffffff; background: #181c28; }");
        };
        stylePtrBtn(ptrOn);
        connect(btn, &QPushButton::clicked, this, [this, btn, stylePtrBtn]() {
            bool nowOn = !m_taskRunnerLabel->isVisible();
            m_taskRunnerLabel->setVisible(nowOn);
            stylePtrBtn(nowOn);
            saveSettings();
        });
        rl->addWidget(btn);
        ifLay->addWidget(row);
    }

    settingsLayout->addWidget(interfaceCard);

    settingsLayout->addStretch();
    settingsScroll->setWidget(settingsView);

    return settingsScroll;
}

QWidget *MainWindow::setupSequencesPage()
{
    QWidget *sequencesView = new QWidget();
    sequencesView->setStyleSheet("background: #0c0e15;");
    QHBoxLayout *seqMainLayout = new QHBoxLayout(sequencesView);
    seqMainLayout->setContentsMargins(0, 0, 0, 0);
    seqMainLayout->setSpacing(0);

    QWidget *seqListPanel = new QWidget();
    seqListPanel->setFixedWidth(200);
    seqListPanel->setStyleSheet("background: #11131a; border-right: 1px solid #1e2130;");
    QVBoxLayout *seqListLayout = new QVBoxLayout(seqListPanel);
    seqListLayout->setContentsMargins(0, 0, 0, 0);
    seqListLayout->setSpacing(0);

    QLabel *seqListTitle = new QLabel("SEQUENCES");
    seqListTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; padding: 16px 14px 8px;");
    seqListLayout->addWidget(seqListTitle);

    seqListEntriesLayout = new QVBoxLayout();
    seqListEntriesLayout->setContentsMargins(0, 0, 0, 0);
    seqListEntriesLayout->setSpacing(0);
    seqListLayout->addLayout(seqListEntriesLayout);

    QPushButton *addSeqBtn = new QPushButton("+ Add Sequence");
    addSeqBtn->setFlat(true);
    addSeqBtn->setCursor(Qt::PointingHandCursor);
    addSeqBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px;"
                             "text-align: left; border: 1px dashed #1e2130; margin: 10px; border-radius: 6px; }"
                             "QPushButton:hover { color: #9098b0; border-color: #9098b0; }");
    connect(addSeqBtn, &QPushButton::clicked, this, [this]() {
        addSequenceEntry("New Sequence", "Manual", "", QJsonArray());
        saveSequences();
    });
    seqListLayout->addWidget(addSeqBtn);
    seqListLayout->addStretch();

    seqDetail = new QStackedWidget();
    seqDetail->setStyleSheet("background: #0c0e15;");
    QWidget *noSeqSelected = new QWidget();
    noSeqSelected->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *noSeqLayout = new QVBoxLayout(noSeqSelected);
    QLabel *noSeqLabel = new QLabel("Select a sequence to configure");
    noSeqLabel->setStyleSheet("color: #9098b0; font-size: 13px;");
    noSeqLabel->setAlignment(Qt::AlignCenter);
    noSeqLayout->addWidget(noSeqLabel);
    seqDetail->addWidget(noSeqSelected);

    seqMainLayout->addWidget(seqListPanel);
    seqMainLayout->addWidget(seqDetail);

    return sequencesView;
}

QWidget *MainWindow::setupCommandsPage()
{
    QWidget *guideView = new QWidget();
    guideView->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *guideViewLayout = new QVBoxLayout(guideView);
    guideViewLayout->setContentsMargins(28, 28, 28, 28);
    guideViewLayout->setSpacing(0);

    QLabel *guideTitle = new QLabel("Guide");
    guideTitle->setStyleSheet("color: #c8d0e8; font-size: 28px; font-weight: 500;");
    guideViewLayout->addWidget(guideTitle);
    guideViewLayout->addSpacing(24);

    QWidget *guideGridWidget = new QWidget();
    guideGridWidget->setStyleSheet("background: transparent;");
    QGridLayout *guideViewGrid = new QGridLayout(guideGridWidget);
    guideViewGrid->setContentsMargins(0, 0, 0, 0);
    guideViewGrid->setVerticalSpacing(10);
    guideViewGrid->setHorizontalSpacing(40);

    const QString gCmdStyle  = "color: #ffffff; font-size: 13px; font-family: monospace;";
    const QString gDescStyle = "color: #6b7494; font-size: 13px;";
    auto addGuideViewRow = [&](int row, const QString &cmd, const QString &desc) {
        QLabel *c = new QLabel(cmd);  c->setStyleSheet(gCmdStyle);
        QLabel *d = new QLabel(desc); d->setStyleSheet(gDescStyle);
        c->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        d->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        guideViewGrid->addWidget(c, row, 0);
        guideViewGrid->addWidget(d, row, 1);
    };
    // ── AI ──────────────────────────────────────────────────────────────────
    addGuideViewRow(0,  "run <task>",          "ask AI to run a command  (AI button on)");

    // ── System ──────────────────────────────────────────────────────────────
    addGuideViewRow(1,  "shutdown <minutes>",  "schedule a shutdown");
    addGuideViewRow(2,  "cancel shutdown",     "cancel scheduled shutdown");
    addGuideViewRow(3,  "clean temp",          "delete temp files, cache & old updates");
    addGuideViewRow(4,  "windows log",         "open Windows Reliability Monitor");
    addGuideViewRow(5,  "rammap",              "open RAMMap  (RamMap.exe must be in Alice folder)");

    // ── Apps ────────────────────────────────────────────────────────────────
    addGuideViewRow(6,  "update apps",         "check and install winget updates");
    addGuideViewRow(7,  "download <app>",      "install an app via winget");
    addGuideViewRow(8,  "uninstall <app>",     "uninstall one or more apps");

    // ── Privacy ─────────────────────────────────────────────────────────────
    addGuideViewRow(9,  "blackout",            "kill all apps in the blackout list");
    addGuideViewRow(10, "kill <app>",          "find and kill a process + its tree");

    // ── Notes ───────────────────────────────────────────────────────────────
    addGuideViewRow(11, "note <message>",      "save a note");
    addGuideViewRow(12, "open notes",          "show all notes");
    addGuideViewRow(13, "clear notes",         "delete all notes");

    // ── Scheduling ──────────────────────────────────────────────────────────
    addGuideViewRow(14, "schedule <task>",     "schedule a task with date and time");

    // ── General ─────────────────────────────────────────────────────────────
    addGuideViewRow(15, "exit / quit",         "close Alice");

    guideViewLayout->addWidget(guideGridWidget);
    guideViewLayout->addStretch();

    return guideView;
}

void MainWindow::setupTrayIcon()
{
    QPixmap trayPix(16, 16);
    trayPix.fill(Qt::transparent);
    {
        QPainter p(&trayPix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(0x7b, 0x8c, 0xde));
        p.setPen(Qt::NoPen);
        p.drawEllipse(2, 2, 12, 12);
    }
    setWindowIcon(QIcon(trayPix));
    trayIcon = new QSystemTrayIcon(QIcon(trayPix), this);
    trayIcon->setToolTip("Alice");

    QMenu *trayMenu = new QMenu(this);
    trayMenu->setStyleSheet(
        "QMenu { background: #13161f; border: 1px solid #1e2130; border-radius: 6px; padding: 6px 0; }"
        "QMenu::item { color: #9098b0; font-size: 12px; padding: 5px 16px; }"
        "QMenu::item:selected { background: #181c28; color: #ffffff; }");
    QAction *showAction = new QAction("Show Alice", this);
    QAction *hudAction  = new QAction("Show Chat Box", this);
    QAction *quitAction = new QAction("Quit", this);
    trayMenu->addAction(showAction);
    trayMenu->addAction(hudAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(showAction, &QAction::triggered, this, [this]() {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show(); raise(); activateWindow();
    });
    connect(hudAction, &QAction::triggered, this, [this, hudAction]() {
        if (!m_hud) return;
        const bool nowVisible = !m_hud->isVisible();
        m_hud->setVisible(nowVisible);
        hudAction->setText(nowVisible ? "Hide Chat Box" : "Show Chat Box");
    });
    // Must set m_forceQuit first so closeEvent doesn't intercept and hide to tray
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_forceQuit = true;
        QCoreApplication::quit();
    });
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            setWindowState(windowState() & ~Qt::WindowMinimized);
            show(); raise(); activateWindow();
        }
    });
}

void MainWindow::setupAiBridge()
{
    aiBridge = new AiBridge(this);
    connect(aiBridge, &AiBridge::responseReceived, this, [this](const QString &resp) {
        // Remove the "Thinking..." bubble posted by sendToAI().
        bool removed = false;
        for (int i = outputLayout->count() - 2; i >= 0 && !removed; i--) {
            QLayoutItem *li = outputLayout->itemAt(i);
            if (!li || !li->widget()) continue;
            const auto editors = li->widget()->findChildren<QTextEdit*>();
            for (QTextEdit *ed : editors) {
                if (ed->toPlainText() == "Thinking...") {
                    delete outputLayout->takeAt(i)->widget();
                    removed = true;
                    break;
                }
            }
        }
        setStatus("ready");

        // ── AI run mode: parse response as a command and execute it ──────────
        if (m_webAiRunPending) {
            m_webAiRunPending = false;
            QString cmd = resp.trimmed();

            // Strip code fences that GPT might still wrap around the command
            if (cmd.startsWith("```")) {
                int nl = cmd.indexOf('\n');
                if (nl != -1) cmd = cmd.mid(nl + 1).trimmed();
                if (cmd.endsWith("```")) cmd.chop(3);
                cmd = cmd.trimmed();
            }

            // Detect shell prefix
            QString shell = "cmd";
            if (cmd.startsWith("powershell:", Qt::CaseInsensitive)) {
                shell = "powershell";
                cmd = cmd.mid(11).trimmed();
            } else if (cmd.startsWith("cmd:", Qt::CaseInsensitive)) {
                cmd = cmd.mid(4).trimmed();
            }

            logAppEvent("AI run [" + shell + "]: " + cmd);
            addMessage("▶ **Running** (" + shell + "):\n```\n" + cmd + "\n```", "alice");

            QProcess *proc = new QProcess(this);
            connect(proc, &QProcess::finished,      proc, &QProcess::deleteLater);
            connect(proc, &QProcess::errorOccurred, proc, &QProcess::deleteLater); // leak guard if start() fails
            if (shell == "powershell") {
                proc->start("powershell.exe",
                            {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", cmd});
            } else {
                proc->start("cmd.exe", {"/c", cmd});
            }
            return;
        }

        logAppEvent("Web AI response received (" + QString::number(resp.length()) + " chars)");
        addMessage(resp, "alice");
    });
}

QWidget *MainWindow::setupWebAiPanel()
{
    QWidget *panel = new QWidget();
    panel->setMinimumWidth(280);
    panel->setStyleSheet("background: #0c0e15; border-left: 1px solid #1e2130;");
    panel->setObjectName("webAiPanel");

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Top bar
    QWidget *bar = new QWidget();
    bar->setFixedHeight(36);
    bar->setStyleSheet("background: #13161f; border-bottom: 1px solid #1e2130;");
    QHBoxLayout *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(12, 0, 12, 0);
    QLabel *barLabel = new QLabel("ChatGPT");
    barLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
    barLayout->addWidget(barLabel);
    barLayout->addStretch();

    // The browser view — visible so JS/layout runs properly
    webAiPanel = new QWebEngineView();
    // Pass nullptr parent — setPage() will adopt the page (sets parent = view).
    // Passing webAiPanel as parent AND calling setPage would double-free on destruction.
    auto *page = new QWebEnginePage(aiBridge->browser()->profile(), nullptr);
    webAiPanel->setPage(page);

    // Hand the visible view to AliceBrowser so it polls THIS view
    aiBridge->browser()->setView(webAiPanel);
    webAiPanel->load(QUrl("https://chatgpt.com/"));

    layout->addWidget(bar);
    layout->addWidget(webAiPanel, 1);
    return panel;
}
