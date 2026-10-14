#include "aibridge.h"
#include "webbrowser.h"
#include <QTimer>
#include <QDebug>

AiBridge::AiBridge(QObject *parent)
    : QObject(parent)
    , m_browser(new AliceBrowser(this))
{
    connect(m_browser, &AliceBrowser::responseReady,
            this,      &AiBridge::responseReceived);

    connect(m_browser, &AliceBrowser::pageLoaded, this, [this]() {
        m_pageReady = true;
        if (!m_pendingPrompt.isEmpty()) {
            const QString prompt   = m_pendingPrompt;
            const QString provider = m_pendingProvider;
            m_pendingPrompt.clear();
            m_pendingProvider.clear();
            doSend(prompt, provider);
        }
    });
}

void AiBridge::sendPrompt(const QString &prompt, const QString &provider)
{
    if (m_currentProvider == provider && m_pageReady) {
        doSend(prompt, provider);
    } else {
        m_pendingPrompt   = prompt;
        m_pendingProvider = provider;
        m_pageReady       = false;
        m_currentProvider = provider;
        m_browser->loadUrl(providerUrl(provider));
    }
}

void AiBridge::newChat(const QString &provider)
{
    qDebug() << "[AiBridge] newChat for provider:" << provider;
    m_pageReady       = false;
    m_currentProvider = provider;
    m_pendingPrompt.clear();
    m_pendingProvider.clear();
    m_browser->loadUrl(providerUrl(provider));
}

void AiBridge::doSend(const QString &prompt, const QString &provider)
{
    qDebug() << "[AiBridge] doSend provider=" << provider
             << "promptLen=" << prompt.length();

    const QString respSel  = responseSelector(provider);
    const QString inputSel = inputSelector(provider);

    // Step 1: capture baseline NOW (before prompt arrives) so we can detect
    //         the new response even if GPT replies very quickly.
    m_browser->captureBaseline(respSel, [this, prompt, inputSel, respSel]() {
        // Step 2: wait 1.5s for React to finish hydrating, then inject
        QTimer::singleShot(1500, this, [this, prompt, inputSel, respSel]() {
            m_browser->injectPrompt(prompt, inputSel);

            // Step 3: start polling 1s after injection
            QTimer::singleShot(1000, this, [this, respSel]() {
                m_browser->startPolling(respSel);
            });
        });
    });
}

QString AiBridge::providerUrl(const QString &provider)
{
    if (provider == "Claude")  return QStringLiteral("https://claude.ai/new");
    if (provider == "Gemini")  return QStringLiteral("https://gemini.google.com/");
    return QStringLiteral("https://chatgpt.com/");
}

QString AiBridge::inputSelector(const QString &provider)
{
    if (provider == "Claude")
        return QStringLiteral("fieldset div[contenteditable='true']");
    if (provider == "Gemini")
        return QStringLiteral("rich-textarea div[contenteditable='true']");
    return QStringLiteral("#prompt-textarea");
}

QString AiBridge::responseSelector(const QString &provider)
{
    if (provider == "Claude")
        return QStringLiteral(".font-claude-message");
    if (provider == "Gemini")
        return QStringLiteral("model-response");
    // ChatGPT — try markdown content first, fall back to role attribute
    return QStringLiteral("div[data-message-author-role='assistant'] .markdown,"
                          "div[data-message-author-role='assistant'],"
                          ".agent-turn .prose");
}
