#include "widgets.h"
#include "theme.h"
#include <QPainter>
#include <QPen>
#include <QConicalGradient>

Spinner::Spinner(QWidget *parent, int size) : QWidget(parent)
{
    setFixedSize(size, size);
    m_color = Theme::instance().accent();
    m_timer.setInterval(28);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 12) % 360;
        update();
    });
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]() {
        m_color = Theme::instance().accent();
        update();
    });
}

void Spinner::showEvent(QShowEvent *)  { m_timer.start(); }
void Spinner::hideEvent(QHideEvent *)  { m_timer.stop();  }

void Spinner::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect().adjusted(m_line, m_line, -m_line, -m_line);

    // Faint background ring
    QColor faded = m_color; faded.setAlpha(50);
    p.setPen(QPen(faded, m_line, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 0, 360 * 16);

    // Rotating arc (270°)
    QConicalGradient grad(r.center(), -m_angle);
    grad.setColorAt(0.0,  m_color);
    QColor tail = m_color; tail.setAlpha(0);
    grad.setColorAt(0.75, tail);
    grad.setColorAt(1.0,  tail);
    QPen pen(QBrush(grad), m_line, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawArc(r, m_angle * 16, 270 * 16);
}

// ── PulseDots ─────────────────────────────────────────────────────────────
PulseDots::PulseDots(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(14);
    setMinimumWidth(36);
    m_timer.setInterval(160);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_phase = (m_phase + 1) % 12;
        update();
    });
}
void PulseDots::showEvent(QShowEvent *) { m_timer.start(); }
void PulseDots::hideEvent(QHideEvent *) { m_timer.stop();  }

void PulseDots::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor base = Theme::instance().accent();
    p.setPen(Qt::NoPen);
    int cy = height() / 2;
    int spacing = 12;
    int x0 = (width() - 2 * spacing) / 2;
    for (int i = 0; i < 3; ++i) {
        int dist = (m_phase / 4 + i) % 3;
        int alpha = (dist == 0) ? 255 : (dist == 1 ? 160 : 80);
        QColor c = base; c.setAlpha(alpha);
        p.setBrush(c);
        int radius = (dist == 0) ? 4 : 3;
        p.drawEllipse(QPoint(x0 + i * spacing, cy), radius, radius);
    }
}

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QParallelAnimationGroup>

void Toast::show(QWidget *parent, const QString &message, Variant variant, int durationMs)
{
    if (!parent) return;
    Toast *t = new Toast(parent, message, variant);
    t->adjustSize();
    int x = (parent->width() - t->width()) / 2;
    int yEnd   = parent->height() - t->height() - 32;
    int yStart = yEnd + 30;
    t->move(x, yStart);
    t->setVisible(true);
    t->raise();
    t->appearAt(QPoint(x, yEnd));
    QTimer::singleShot(durationMs, t, &Toast::dismiss);
}

Toast::Toast(QWidget *parent, const QString &message, Variant v)
    : QWidget(parent), m_text(message), m_variant(v)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    m_label = new QLabel(message, this);
    QString textColor = "#ffffff";
    m_label->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 500;").arg(textColor));

    QHBoxLayout *l = new QHBoxLayout(this);
    l->setContentsMargins(18, 10, 18, 10);
    l->addWidget(m_label);

    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(0.0);
    setGraphicsEffect(m_opacity);
}

void Toast::appearAt(const QPoint &endPos)
{
    auto *grp = new QParallelAnimationGroup(this);

    auto *slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(280);
    slide->setStartValue(pos());
    slide->setEndValue(endPos);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    grp->addAnimation(slide);

    auto *fade = new QPropertyAnimation(m_opacity, "opacity", this);
    fade->setDuration(280);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    grp->addAnimation(fade);

    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::dismiss()
{
    auto *grp = new QParallelAnimationGroup(this);

    auto *slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(220);
    slide->setStartValue(pos());
    slide->setEndValue(pos() + QPoint(0, 20));
    slide->setEasingCurve(QEasingCurve::InCubic);
    grp->addAnimation(slide);

    auto *fade = new QPropertyAnimation(m_opacity, "opacity", this);
    fade->setDuration(220);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    grp->addAnimation(fade);

    connect(grp, &QParallelAnimationGroup::finished, this, &QObject::deleteLater);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    Theme &T = Theme::instance();
    QColor bg, accent;
    switch (m_variant) {
        case Success: accent = T.success(); break;
        case Warning: accent = QColor("#f0b440"); break;
        case Error:   accent = T.danger();  break;
        default:      accent = T.accent();
    }
    bg = T.surface();

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 22, 22);
    p.fillPath(path, bg);

    QPen pen(accent, 1.5);
    p.setPen(pen);
    p.drawPath(path);

    // Left accent bar
    QPainterPath bar;
    bar.addRoundedRect(QRectF(6, height()/2 - 8, 3, 16), 1.5, 1.5);
    p.fillPath(bar, accent);
}

#include "theme.h"

AnimatedStack::AnimatedStack(QWidget *parent) : QStackedWidget(parent) {}

void AnimatedStack::setCurrentIndex(int idx)
{
    if (idx == currentIndex()) return;
    QStackedWidget::setCurrentIndex(idx);
    if (Theme::instance().animated())
        runFade(currentWidget());
}

void AnimatedStack::runFade(QWidget *target)
{
    if (!target) return;
    auto *eff = new QGraphicsOpacityEffect(target);
    eff->setOpacity(0.0);
    target->setGraphicsEffect(eff);

    auto *anim = new QPropertyAnimation(eff, "opacity", target);
    anim->setDuration(220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, target, [target]() {
        target->setGraphicsEffect(nullptr);   // remove so child widgets render normally
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

#include "bubbleedit.h"
#include <QTextEdit>
#include <QFrame>
#include <QSizePolicy>
#include <QTextDocument>
#include <QResizeEvent>
#include <QWheelEvent>

BubbleEdit::BubbleEdit(QWidget *parent) : QTextEdit(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumWidth(kMaxBubbleWidth);
    setReadOnly(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    document()->setDocumentMargin(10);
    document()->setDefaultStyleSheet(
        "body  { color: #e0e4f0; }"
        "h1, h2, h3 { color: #ffffff; margin: 6px 0 3px 0; }"
        "h4, h5, h6  { color: #d0d8f0; margin: 4px 0 2px 0; }"
        "p    { margin: 3px 0; }"
        "ul, ol { margin: 3px 0; padding-left: 20px; }"
        "li   { color: #e0e4f0; margin: 2px 0; }"
        "code { background-color: #1a1d2a; color: #a8c4ff; font-family: monospace; }"
        "pre  { background-color: #1a1d2a; color: #a8c4ff; padding: 6px; }"
        "hr   { border-top: 1px solid #3a4060; }"
        "strong { color: #ffffff; font-weight: bold; }"
        "em   { color: #c0c8e0; font-style: italic; }"
        "blockquote { border-left: 3px solid #3a4060; padding-left: 10px; color: #9098b0; }"
    );
}
int BubbleEdit::contentHeight(int w) const {
    QTextDocument *doc = document()->clone();
    doc->setPageSize(QSizeF(w, -1));
    int h = qRound(doc->size().height());
    delete doc;
    return h;
}
QSize BubbleEdit::sizeHint() const {
    QTextDocument *doc = document()->clone();
    doc->setPageSize(QSizeF(-1, -1));
    int idealW = qRound(doc->idealWidth()) + qRound(document()->documentMargin()) * 2 + 8;
    delete doc;
    int w = qMin(idealW, kMaxBubbleWidth);
    return QSize(w, contentHeight(w));
}
void BubbleEdit::resizeEvent(QResizeEvent *e) {
    QTextEdit::resizeEvent(e);
    int h = contentHeight(e->size().width());
    if (h != height()) {
        setFixedHeight(h);
        updateGeometry();
    }
}
// Pass wheel events up so the outer chat scroll area handles them,
// not the bubble itself (prevents scrolling inside the text box).
void BubbleEdit::wheelEvent(QWheelEvent *e) {
    e->ignore();
}