#pragma once
#include <QMainWindow>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QComboBox>
#include <QSpinBox>
#include <QJsonArray>
#include <QMap>
#include <functional>
#include <QPointer>
#include <QPlainTextEdit>
#include "aibridge.h"
#include "chathud.h"
#include "animatedstack.h"
#include <QWebEngineView>
#include <QSplitter>
#include <QPropertyAnimation>
#include <QAbstractNativeEventFilter>
#include <QDebug>


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendMessage();
    void onNewSession();
    void loadHistorySession(const QString &path, const QString &name);
    void refreshHistory();
    void onAddAppProfile();
    void onWindowFlash(const QString &exeName);  // fired by AliceShellFilter

private:
    void addMessage(const QString &text, const QString &sender);
    QPlainTextEdit *inputBox;
    QVBoxLayout *outputLayout;
    AnimatedStack *stack;
    QString sessionFilePath;
    QString logFilePath;
    QString historyFilePath;
    QString appLogFilePath;
    QString notesFilePath;
    void writeAppLog(const QString &formatted); // low-level: writes pre-formatted line to file
    void initSession();
    void saveMessage(const QString &text, const QString &sender);
    QPushButton *activeNavBtn = nullptr;
    QWidget *apiSectionWidget = nullptr;
    QVBoxLayout *historyViewLayout;
    QLabel *historyViewTitle;
    QLabel *historyTextLabel;
    QLabel *appLogsTextLabel;
    void refreshAppLogs();
    QVBoxLayout *tasksContentLayout;
    QScrollArea *chatScroll         = nullptr;
    QScrollArea *historyScroll      = nullptr;
    QScrollArea *logsScroll         = nullptr;
    QScrollArea *historyEntryScroll = nullptr;
    QVBoxLayout *apiEntriesLayout;
    QVBoxLayout *blackoutAppsLayout;
    QLabel *folderPath;
    void loadSettings();
    void saveSettings();
    void addApiEntry(const QString &name, const QString &key, const QString &model, bool active);
    QString configFilePath;
    QNetworkAccessManager *networkManager;
    QString getActiveApiKey();
    QString getActiveModel();
    void sendToAI(const QString &prompt, bool execute);
    QPushButton *statusLabel;
    void setStatus(const QString &status);
    QLabel *watermarkGreeting;
    QLineEdit *nameEdit;
    void updateWatermark();
    QString userName();
    void addTask(const QString &title, int seconds, bool cancellable);
    void addScheduledTask(const QString &task, const QDateTime &when);
    void sendToAIWithCallback(const QString &systemPrompt, const QString &userPrompt,
                              std::function<void(const QString &)> callback);
    QVBoxLayout *scheduledTasksLayout;
    QVBoxLayout *appListEntriesLayout;
    QStackedWidget *appProfileDetail;
    void addAppProfileEntry(const QString &name, const QString &exePath);
    QString profilesFilePath;
    void saveProfiles();
    void loadProfiles();
    void updateStatus();
    void addAppWatcher(const QString &appName, const QString &exeName, QPushButton *profileBtn);

    // Central process watcher
    QTimer *centralWatcher = nullptr;
    QMap<QString, QList<std::function<void(bool)>>> watcherCallbacks;
    QMap<QString, bool> watcherRunState;
    void registerWatcher(const QString &exe, std::function<void(bool)> cb);
    QSystemTrayIcon *trayIcon;
    QPushButton *startupToggleBtn;
    bool setAutoStartup(bool enabled);   // returns true if schtasks succeeded
    bool isAutoStartupEnabled() const;
    bool isRunningAsAdmin() const;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;  // keeps GPT drawer pinned to right edge

    // Sequences
    QVBoxLayout *seqListEntriesLayout;
    QStackedWidget *seqDetail;
    QString sequencesFilePath;
    void addSequenceEntry(const QString &name, const QString &triggerType,
                          const QString &triggerExe, const QJsonArray &actions);
    void addActionRow(QWidget *container, const QJsonObject &data);
    QJsonArray getSequenceActions(int detailIdx) const;
    void executeSequenceActions(const QJsonArray &actions, int idx);
    void saveSequences();
    void loadSequences();
    void startSequenceWatcher(QPushButton *btn);

    // Winget upgrade flow
    bool wingetPending = false;
    QPointer<QWidget> wingetTaskCard;
    QPointer<QPlainTextEdit> wingetOutput;
    int wingetUpgradeCount = 0;
    void startWingetListing();

    // Web AI bridge (QtWebEngine)
    AiBridge *aiBridge = nullptr;
    void showWebAiLogin();

    // Web AI
    QWebEngineView *webAiPanel    = nullptr;
    QPushButton    *aiModeBtn     = nullptr;
    bool            aiModeOn      = false;
    QWidget*        setupWebAiPanel();

    // GPT drawer (right side — floats over content, always shown off-screen when closed)
    QWidget            *gpuDrawer     = nullptr;
    QPropertyAnimation *gpuAnim       = nullptr;
    bool                gptDrawerOpen = false;
    QPushButton        *gptDrawerBtn  = nullptr;

    // Body splitter (sidebar | main content)
    QSplitter *bodySplitter = nullptr;

    // Scroll helpers
    QWidget  *inputArea         = nullptr;   // kept for dynamic resize
    bool      m_midScrollActive = false;
    bool      m_webAiRunPending = false;     // true while waiting for AI "run" command response
    bool      m_forceQuit      = false;     // set before quit so closeEvent doesn't intercept
    QPoint    m_midScrollOrigin;
    QTimer   *m_edgeScrollTimer = nullptr;

    // Integrated Web AI (embedded browser — distinct from extension-based Web AI)
    QPushButton *integratedWebAiToggleBtn = nullptr;
    QComboBox   *integratedWebAiProviderCombo = nullptr;

    // Floating desktop HUD
    ChatHud *m_hud       = nullptr;
    int      m_savedHudX = -1;   // loaded before setupHud so no timer race
    int      m_savedHudY = -1;
    void     setupHud();

    // Per-sequence watcher tokens — invalidates old callback when Save re-registers
    QMap<QPushButton*, std::shared_ptr<bool>> m_seqWatcherTokens;

    // Shell hook — match-found detection
    unsigned int                  m_shellMsg    = 0;
    QAbstractNativeEventFilter   *m_shellFilter = nullptr;
    bool                          m_matchCooldownActive = false;
    QTimer                       *m_matchCooldownTimer  = nullptr;

    // UI page builders (implemented in mainwindow_ui.cpp)
    QWidget*      setupTopBar();
    QWidget*      setupSidebar();
    QWidget*      setupChatPage();
    QWidget*      setupHistoryPage();
    QWidget*      setupHistoryEntryPage();
    QWidget*      setupLogsPage();
    QWidget*      setupTasksPage();
    QWidget*      setupAppProfilesPage();
    QScrollArea*  setupSettingsPage();
    void          applyTheme();              // refresh themed surfaces (top bar, sidebar, bg)
    QWidget      *m_topBar            = nullptr;  // kept for theme refresh
    QWidget      *m_sidebar           = nullptr;
    QWidget      *m_navIndicator      = nullptr;  // animated active-nav pill
    QLabel       *m_taskRunnerLabel   = nullptr;  // "Personal Task Runner" in top bar
    QWidget*      setupSequencesPage();
    QWidget*      setupCommandsPage();
    void          setupTrayIcon();
    void          setupAiBridge();

    // Returns a stylesheet for a sidebar nav button in the given state,
    // always using the live theme colours so click handlers stay correct.
    static QString navActiveStyle();
    static QString navInactiveStyle();
};

// Convenience macro: routes through qInfo() so the message handler automatically
// captures __FILE__, __LINE__ and Q_FUNC_INFO at the call site — same as Qt's own logs.
#define logAppEvent(msg) qInfo().noquote() << (msg)
