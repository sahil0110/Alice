#include "animatedstack.h"
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
