#include "webbrowser.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QDebug>

AliceBrowser::AliceBrowser(QObject *parent)
    : QObject(parent)
{
    // Persistent profile — cookies and session data survive between Alice restarts
    const QString storagePath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/webengine";
    QDir().mkpath(storagePath);

    m_profile = new QWebEngineProfile("AliceWebAI", this);
    m_profile->setPersistentStoragePath(storagePath);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, true);

    m_view = new QWebEngineView;
    // Qt::Tool prevents this helper window from appearing in the Windows taskbar.
    // It still has a real window handle so WebEngine renders and runs JS normally.
    m_view->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    m_view->resize(1280, 900);
    m_view->move(-2000, -2000);
    m_view->show();

    auto *page = new QWebEnginePage(m_profile, nullptr); // setPage() adopts ownership
    m_view->setPage(page);

    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        qDebug() << "[AliceBrowser] loadFinished ok=" << ok;
        emit pageLoaded();
    });

    connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        qDebug() << "[AliceBrowser] urlChanged:" << url.toString();
        // If polling is active and URL changed to a conversation URL,
        // reset baseline and previousText so we don't miss the response
        if (m_pollTimer->isActive()) {
            m_baseline     = "";
            m_previousText = "";
            m_stableCount  = 0;
            qDebug() << "[AliceBrowser] URL changed during poll, baseline reset";
        }
    });

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &AliceBrowser::pollTick);
}

AliceBrowser::~AliceBrowser()
{
    stopPolling();
    if (m_ownsView) delete m_view;
    // If !m_ownsView the view is embedded in the GPT drawer and owned by that widget
}

void AliceBrowser::setView(QWebEngineView *view)
{
    if (m_view) {
        disconnect(m_view, nullptr, this, nullptr);
        if (m_ownsView) {
            delete m_view;  // destroy the internal off-screen helper — no longer needed
        }
    }
    m_view     = view;
    m_ownsView = false;  // caller's widget tree owns this view now
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        qDebug() << "[AliceBrowser] loadFinished ok=" << ok;
        emit pageLoaded();
    });
    connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        qDebug() << "[AliceBrowser] urlChanged:" << url.toString();
        if (m_pollTimer->isActive()) {
            m_baseline     = "";
            m_previousText = "";
            m_stableCount  = 0;
            qDebug() << "[AliceBrowser] URL changed during poll, baseline reset";
        }
    });
}

void AliceBrowser::loadUrl(const QString &url)
{
    qDebug() << "[AliceBrowser] loadUrl" << url;
    m_view->load(QUrl(url));
}

void AliceBrowser::runJavaScript(const QString &script,
                                  std::function<void(const QVariant &)> callback)
{
    if (callback) {
        m_view->page()->runJavaScript(script,
            [cb = std::move(callback)](const QVariant &result) { cb(result); });
    } else {
        m_view->page()->runJavaScript(script);
    }
}

QString AliceBrowser::buildQueryJs(const QString &) const
{
    // This JS runs inside the WebEngine on every poll tick.
    // It converts GPT's DOM structure into clean Markdown so Qt's
    // setMarkdown() can render bullets, lines, bold, code blocks, etc.
    return QStringLiteral(R"JS(
(function() {
    // Still generating?
    var stopBtn = document.querySelector(
        'button[data-testid="stop-button"], button[aria-label="Stop streaming"]');
    if (stopBtn) return '__THINKING__';

    var els = document.querySelectorAll('[data-message-author-role="assistant"]');
    if (els.length === 0) return '';

    var el = els[els.length - 1];

    // Find the markdown/prose container — prefer it over the outer wrapper
    var content = el.querySelector(
        '.markdown, [class*="markdown"], .prose, [class*="prose"]');
    if (!content) content = el;

    // ── Recursive HTML → Markdown converter ──────────────────────────────
    function toMd(node, listCtx) {
        if (!node) return '';
        if (node.nodeType === 3) return node.textContent; // raw text node

        if (node.nodeType !== 1) return '';
        var tag = node.tagName.toLowerCase();

        // Skip non-content chrome
        if (tag === 'button' || tag === 'svg'   || tag === 'script' ||
            tag === 'style'  || tag === 'select' || tag === 'input') return '';

        // Tables need direct DOM access — short-circuit before building inner
        if (tag === 'table') {
            var trs = node.querySelectorAll('tr');
            if (!trs.length) return '';
            var tblMd = '\n';
            for (var ri = 0; ri < trs.length; ri++) {
                var cells = trs[ri].querySelectorAll('th, td');
                if (!cells.length) continue;
                var line = '|';
                for (var ci = 0; ci < cells.length; ci++)
                    line += ' ' + cells[ci].textContent.replace(/[\|\r\n]+/g, ' ').trim() + ' |';
                tblMd += line + '\n';
                if (ri === 0) {
                    // Separator after first row (becomes the header)
                    var sep = '|';
                    for (var sc = 0; sc < cells.length; sc++) sep += ' --- |';
                    tblMd += sep + '\n';
                }
            }
            return tblMd + '\n';
        }

        // Build inner content (not needed for tables, handled above)
        var inner = '';
        var olIdx = 1;
        var childCtx = (tag === 'ol') ? 'ol' : (tag === 'ul' ? 'ul' : listCtx);
        for (var i = 0; i < node.childNodes.length; i++) {
            var child = node.childNodes[i];
            if ((tag === 'ol' || tag === 'ul') && child.tagName === 'LI') {
                inner += toMdLi(child, tag, olIdx);
                if (tag === 'ol') olIdx++;
            } else {
                inner += toMd(child, childCtx);
            }
        }

        switch (tag) {
            case 'p':          return inner.trim() + '\n\n';
            case 'br':         return '\n';
            case 'hr':         return '\n---\n\n';
            case 'strong': case 'b': return '**' + inner + '**';
            case 'em':    case 'i': return '_' + inner + '_';
            case 'del':        return '~~' + inner + '~~';
            case 'code':       return '`' + inner.replace(/`/g, "'") + '`';
            case 'pre': {
                var raw = (node.querySelector('code') || node).textContent;
                return '\n```\n' + raw + '\n```\n\n';
            }
            case 'h1': return '\n# '    + inner.trim() + '\n\n';
            case 'h2': return '\n## '   + inner.trim() + '\n\n';
            case 'h3': return '\n### '  + inner.trim() + '\n\n';
            case 'h4': return '\n#### ' + inner.trim() + '\n\n';
            case 'h5': case 'h6': return '\n##### ' + inner.trim() + '\n\n';
            case 'ul': case 'ol': return inner + '\n';
            case 'blockquote': {
                var lines = inner.trim().split('\n');
                return '\n' + lines.map(function(l){ return '> ' + l; }).join('\n') + '\n\n';
            }
            case 'a': {
                var href = node.getAttribute('href');
                return href ? '[' + inner + '](' + href + ')' : inner;
            }
            // th/td/tr are consumed by the table handler above; skip if encountered standalone
            case 'tr': case 'th': case 'td': return inner;
            default:   return inner;
        }
    }

    function toMdLi(li, listType, idx) {
        var inner = '';
        for (var i = 0; i < li.childNodes.length; i++)
            inner += toMd(li.childNodes[i], listType);
        var prefix = (listType === 'ol') ? (idx + '. ') : '- ';
        return prefix + inner.trim() + '\n';
    }

    var md = toMd(content, '').trim();

    // Fallback: plain textContent if DOM conversion produced nothing
    if (md.length < 5) md = (el.textContent || '').trim();

    return md;
})();
)JS");
}

void AliceBrowser::injectPrompt(const QString &prompt, const QString &inputSelector)
{
    QString escaped = prompt;
    escaped.replace('\\', "\\\\");
    escaped.replace('`',  "\\`");
    escaped.replace("${", "\\${");

    QString sel = inputSelector;
    sel.replace('\'', "\\'");

    const QString js = QString(R"JS(
(function() {
    var box = document.querySelector('%1');
    if (!box) return 'NO_INPUT_BOX';
    box.focus();
    box.click();
    if (box.tagName === 'TEXTAREA') {
        var setter = Object.getOwnPropertyDescriptor(
            window.HTMLTextAreaElement.prototype, 'value').set;
        setter.call(box, `%2`);
    } else {
        box.innerHTML = '';
        box.innerText  = `%2`;
    }
    box.dispatchEvent(new InputEvent('input', { bubbles: true, composed: true }));
    box.dispatchEvent(new Event('change', { bubbles: true }));
    setTimeout(function() {
        // Try send button first (ChatGPT, Gemini use a button now)
        var sendBtn = document.querySelector(
            'button[data-testid="send-button"], ' +
            'button[aria-label="Send prompt"], ' +
            'button[aria-label="Send message"], ' +
            'button.send-button'
        );
        if (sendBtn && !sendBtn.disabled) {
            sendBtn.click();
        } else {
            box.dispatchEvent(new KeyboardEvent('keydown', {
                key: 'Enter', code: 'Enter', keyCode: 13,
                bubbles: true, composed: true
            }));
        }
    }, 800);
    return 'OK';
})();
)JS").arg(sel, escaped);

    runJavaScript(js, [](const QVariant &r) {
        qDebug() << "[AliceBrowser] injectPrompt result:" << r.toString();
    });
}

// Phase 1 — capture what's on page RIGHT NOW (before prompt is sent)
void AliceBrowser::captureBaseline(const QString &responseSelector,
                                    std::function<void()> done)
{
    stopPolling();
    m_responseSelector = responseSelector;
    m_stableCount      = 0;
    m_pollCount        = 0;
    m_responseCallback = nullptr;

    m_view->page()->runJavaScript(buildQueryJs(responseSelector),
        [this, done](const QVariant &v) {
            m_baseline     = v.toString();
            m_previousText = m_baseline;
            qDebug() << "[AliceBrowser] baseline captured, len:" << m_baseline.length();
            if (done) done();
        });
}

// Phase 2 — start watching for a response different from baseline
void AliceBrowser::startPolling(const QString &responseSelector)
{
    m_responseSelector = responseSelector;
    m_stableCount = 0;
    m_pollCount   = 0;
    qDebug() << "[AliceBrowser] startPolling";
    m_pollTimer->start();
}

// Legacy combined helper
void AliceBrowser::extractResponse(const QString &responseSelector,
                                    std::function<void(const QString &)> callback)
{
    m_responseCallback = std::move(callback);
    captureBaseline(responseSelector, [this, responseSelector]() {
        QTimer::singleShot(1000, this, [this, responseSelector]() {
            startPolling(responseSelector);
        });
    });
}

void AliceBrowser::stopPolling()
{
    m_pollTimer->stop();
}

void AliceBrowser::pollTick()
{
    ++m_pollCount;

    m_view->page()->runJavaScript(buildQueryJs(m_responseSelector),
        [this](const QVariant &v) {
        const QString current = v.toString();

        if (m_pollCount % 10 == 0)
            qDebug() << "[AliceBrowser] poll #" << m_pollCount
                     << "len:" << current.length()
                     << "stable:" << m_stableCount;

        if (m_pollCount == 50) {
            m_view->page()->runJavaScript(
                QStringLiteral("document.body.innerText.slice(0, 500)"),
                [](const QVariant &v) {
                    qDebug() << "[AliceBrowser] DOM snapshot:" << v.toString();
                });
            m_view->page()->runJavaScript(
                QStringLiteral("document.querySelectorAll('[data-message-author-role]').length + ' role elements, ' + document.querySelectorAll('article').length + ' articles, ' + document.querySelectorAll('.prose').length + ' prose'"),
                [](const QVariant &v) {
                    qDebug() << "[AliceBrowser] selector counts:" << v.toString();
                });
        }

        // Timeout
        if (m_pollCount >= MAX_POLLS) {
            qDebug() << "[AliceBrowser] poll timeout";
            stopPolling();
            const QString result = (current.length() >= 20 && current != m_baseline)
                ? current
                : "No response received. Make sure you are logged in (Settings → Login).";
            emit responseReady(result);
            if (m_responseCallback) { m_responseCallback(result); m_responseCallback = nullptr; }
            return;
        }

        // ChatGPT is still generating — keep waiting
        if (current == QStringLiteral("__THINKING__")) {
            m_stableCount = 0;
            return;
        }

        // Nothing useful yet
        if (current.isEmpty() || current == m_baseline)
            return;

        // Track stability — any non-empty, non-baseline response counts,
        // regardless of length (short replies like "Sure!" are valid responses).
        if (current != m_previousText) {
            m_previousText = current;
            m_stableCount  = 0;
        } else {
            ++m_stableCount;
        }

        if (m_stableCount >= STABLE_NEEDED) {
            qDebug() << "[AliceBrowser] response stable (" << current.length() << " chars)";
            stopPolling();
            emit responseReady(current);
            if (m_responseCallback) { m_responseCallback(current); m_responseCallback = nullptr; }
        }
    });
}
