#include "toast.h"
#include "theme.h"
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
