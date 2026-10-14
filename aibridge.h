#pragma once
#include <QObject>
#include <QString>

class AliceBrowser;

class AiBridge : public QObject
{
    Q_OBJECT
public:
    explicit AiBridge(QObject *parent = nullptr);

    void sendPrompt(const QString &prompt, const QString &provider);
    void newChat(const QString &provider);
    AliceBrowser *browser() const { return m_browser; }

signals:
    void responseReceived(const QString &response);

private:
    AliceBrowser *m_browser         = nullptr;
    QString       m_currentProvider;
    bool          m_pageReady       = false;
    QString       m_pendingPrompt;
    QString       m_pendingProvider;

    static QString providerUrl(const QString &provider);
    static QString inputSelector(const QString &provider);
    static QString responseSelector(const QString &provider);

    void doSend(const QString &prompt, const QString &provider);
};
