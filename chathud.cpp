#include "chathud.h"
#include <QApplication>
#include <QScreen>
#include <QShowEvent>
#include <QEvent>
#include <QFrame>
#include <QStyle>
#include <QAbstractButton>

// QPushButton::hitButton uses SC_PushButtonBevel which is smaller than the widget
// rect (platform style reserves space for focus rings / shadows).  Override it so
// the ENTIRE button cell is clickable, not just the inner text area.
class HudButton : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
};

ChatHud::ChatHud(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kExpandedWidth, kHeight);

    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto makeSep = [this]() {
        auto *f = new QFrame();
        f->setFrameShape(QFrame::VLine);
        f->setFixedSize(1, kHeight);
        f->setStyleSheet("background: #1e2130; border: none;");
        f->setCursor(Qt::SizeAllCursor);
        f->installEventFilter(this);
        m_dragWidgets.append(f);
        return f;
    };

    // Drag handle
    m_dragHandle = new QLabel("\u2807");
    m_dragHandle->setFixedSize(32, kHeight);
    m_dragHandle->setAlignment(Qt::AlignCenter);
    m_dragHandle->setCursor(Qt::SizeAllCursor);
    m_dragHandle->installEventFilter(this);
    m_dragWidgets.append(m_dragHandle);

    // Input box — full height
    m_input = new QLineEdit();
    m_input->setPlaceholderText("command...");
    m_input->setFrame(false);
    m_input->setFixedHeight(kHeight);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_input->installEventFilter(this);
    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        const QString text = m_input->text().trimmed();
        if (text.isEmpty()) return;
        m_input->clear();
        emit commandEntered(text, m_aiMode);
    });

    // Button factory — HudButton overrides hitButton so the full cell is clickable
    auto makeBtn = [this](const QString &label) {
        auto *b = new HudButton(label);
        b->setFixedSize(kBtnWidth, kHeight);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->installEventFilter(this);
        return b;
    };

    m_aiBtn    = makeBtn("AI");
    m_aliceBtn = makeBtn("\u229f");   // ⊟ glyph
    m_toggleBtn= makeBtn("\u00d7");  // × glyph

    QFont glyph = m_aliceBtn->font();
    glyph.setPixelSize(16);
    m_aliceBtn->setFont(glyph);
    m_toggleBtn->setFont(glyph);

    QFont aiFont = m_aiBtn->font();
    aiFont.setPixelSize(13);
    aiFont.setBold(true);
    m_aiBtn->setFont(aiFont);

    m_aiBtn->setCheckable(true);
    m_aiBtn->setToolTip("Toggle AI mode");
    m_aliceBtn->setToolTip("Open Alice");
    m_toggleBtn->setToolTip("Collapse / expand");

    connect(m_aiBtn, &QPushButton::toggled, this, [this](bool on) {
        m_aiMode = on;
        applyTheme(m_bg, m_border, m_text, m_accent);
        emit aiModeChanged(on);
    });
    connect(m_aliceBtn,  &QPushButton::clicked, this, &ChatHud::showAliceRequested);
    connect(m_toggleBtn, &QPushButton::clicked, this, &ChatHud::toggleCollapse);

    // ⠿ | input | | AI | | ⊡ | | ×
    lay->addWidget(m_dragHandle);
    lay->addWidget(m_input);
    lay->addWidget(makeSep());
    lay->addWidget(m_aiBtn);
    lay->addWidget(makeSep());
    lay->addWidget(m_aliceBtn);
    lay->addWidget(makeSep());
    lay->addWidget(m_toggleBtn);

    applyTheme(m_bg, m_border, m_text, m_accent);
}

bool ChatHud::eventFilter(QObject *obj, QEvent *e)
{
    QWidget *w = qobject_cast<QWidget *>(obj);
    if (!w) return QWidget::eventFilter(obj, e);

    QMouseEvent *me = static_cast<QMouseEvent *>(e);

    if (m_dragWidgets.contains(w)) {
        switch (e->type()) {
        case QEvent::MouseButtonPress:   startDrag(me); return true;
        case QEvent::MouseMove:          return handleDragMove(me);
        case QEvent::MouseButtonRelease: return handleDragRelease(me);
        default: break;
        }
    } else if (w == m_aiBtn || w == m_aliceBtn || w == m_toggleBtn || w == m_input) {
        const int threshold = (w == m_input) ? 8 : 5;
        switch (e->type()) {
        case QEvent::MouseButtonPress:
            if (me->button() == Qt::LeftButton) {
                m_pressPos   = me->globalPosition().toPoint();
                m_dragOffset = m_pressPos - frameGeometry().topLeft();
                m_dragging   = false;
            }
            return false;
        case QEvent::MouseMove:
            if ((me->buttons() & Qt::LeftButton) && !m_dragging) {
                if ((me->globalPosition().toPoint() - m_pressPos).manhattanLength() > threshold) {
                    m_dragging = true;
                    if (auto *btn = qobject_cast<QAbstractButton *>(w))
                        btn->setDown(false);
                }
            }
            if (m_dragging) {
                setHudPos(me->globalPosition().toPoint() - m_dragOffset);
                return true;
            }
            return false;
        case QEvent::MouseButtonRelease:
            if (me->button() == Qt::LeftButton && m_dragging) {
                m_dragging = false;
                emit hudMoved(pos());
                return true;
            }
            return false;
        default: break;
        }
    }

    return QWidget::eventFilter(obj, e);
}

void ChatHud::startDrag(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging   = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

bool ChatHud::handleDragMove(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        setHudPos(e->globalPosition().toPoint() - m_dragOffset);
        return true;
    }
    return false;
}

bool ChatHud::handleDragRelease(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        emit hudMoved(pos());
        return true;
    }
    return false;
}

void ChatHud::mousePressEvent(QMouseEvent *e)   { startDrag(e); }
void ChatHud::mouseMoveEvent(QMouseEvent *e)    { handleDragMove(e); }
void ChatHud::mouseReleaseEvent(QMouseEvent *e) { handleDragRelease(e); }

void ChatHud::setAiMode(bool on)
{
    if (m_aiMode == on) return;
    m_aiMode = on;
    QSignalBlocker blocker(m_aiBtn);
    m_aiBtn->setChecked(on);
    applyTheme(m_bg, m_border, m_text, m_accent);
}

void ChatHud::toggleCollapse()
{
    m_collapsed = !m_collapsed;
    const bool show = !m_collapsed;

    m_dragHandle->setVisible(show);
    m_input->setVisible(show);
    m_aiBtn->setVisible(show);
    m_aliceBtn->setVisible(show);
    for (QFrame *f : findChildren<QFrame *>())
        f->setVisible(show);

    m_toggleBtn->setText(m_collapsed ? "\u25b6" : "\u00d7");

    const int oldW = width();
    const int newW = m_collapsed ? kCollapsedWidth : kExpandedWidth;
    move(x() + (oldW - newW), y());
    setFixedWidth(newW);
}

void ChatHud::applyTheme(const QString &bg, const QString &border,
                         const QString &text, const QString &accent)
{
    m_bg = bg; m_border = border; m_text = text; m_accent = accent;

    setStyleSheet(QString(R"(
ChatHud {
    background: %1;
    border: 1px solid %2;
    border-radius: 10px;
}
QLineEdit {
    background: transparent;
    color: %3;
    font-size: 13px;
    padding: 0 10px;
    border: none;
    selection-background-color: %4;
}
QLineEdit::placeholder {
    color: #3a3f55;
}
QPushButton {
    color: #555d80;
    background: transparent;
    border: none;
    border-radius: 0px;
}
QPushButton:hover {
    color: %3;
    background: rgba(255,255,255,0.05);
}
QPushButton:pressed {
    background: rgba(255,255,255,0.09);
}
QPushButton#aiOn {
    color: %4;
}
    )").arg(bg, border, text, accent));

    m_aiBtn->setObjectName(m_aiMode ? "aiOn" : "");
    m_aiBtn->style()->unpolish(m_aiBtn);
    m_aiBtn->style()->polish(m_aiBtn);
}

void ChatHud::setHudPos(const QPoint &p)
{
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(qBound(screen.left(), p.x(), screen.right()  - width()),
         qBound(screen.top(),  p.y(), screen.bottom() - height()));
}

QPoint ChatHud::hudPos() const { return pos(); }

void ChatHud::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    lower();
}

void ChatHud::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::ActivationChange && !isActiveWindow())
        lower();
}