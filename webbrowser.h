#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <functional>

class QWebEngineView;
class QWebEngineProfile;

class AliceBrowser : public QObject
{
    Q_OBJECT
public:
    explicit AliceBrowser(QObject *parent = nullptr);
    ~AliceBrowser();

    void loadUrl(const QString &url);
    void runJavaScript(const QString &script,
                       std::function<void(const QVariant &)> callback = {});
    void injectPrompt(const QString &prompt,
                      const QString &inputSelector = "#prompt-textarea");

    // Phase 1: capture baseline BEFORE prompt is injected
    void captureBaseline(const QString &responseSelector,
                         std::function<void()> done = {});

    // Phase 2: start polling for response AFTER prompt is injected
    void startPolling(const QString &responseSelector);

    // Combined legacy helper (baseline + delayed poll start)
    void extractResponse(const QString &responseSelector,
                         std::function<void(const QString &)> callback = {});

    QWebEngineProfile *profile() const { return m_profile; }
    void setView(QWebEngineView *view);

signals:
    void pageLoaded();
    void responseReady(const QString &response);

private slots:
    void pollTick();

private:
    QWebEngineProfile *m_profile   = nullptr;
    QWebEngineView    *m_view      = nullptr;
    QTimer            *m_pollTimer = nullptr;

    QString m_responseSelector;
    QString m_baseline;
    QString m_previousText;
    int     m_stableCount = 0;
    int     m_pollCount   = 0;
    bool    m_ownsView    = true;  // false once setView() hands ownership to a panel widget
    std::function<void(const QString &)> m_responseCallback;

    static constexpr int POLL_INTERVAL_MS = 400;
    static constexpr int STABLE_NEEDED    = 4;   // 1.6s of stability
    static constexpr int MAX_POLLS        = 300;  // 2 minutes

    void stopPolling();
    QString buildQueryJs(const QString &sel) const;
};
