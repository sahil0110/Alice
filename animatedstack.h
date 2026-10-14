#pragma once
#include <QStackedWidget>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

// Drop-in replacement for QStackedWidget that fades the new page in.
// Falls back to instant switch when Theme::animated() is false.
class AnimatedStack : public QStackedWidget
{
    Q_OBJECT
public:
    explicit AnimatedStack(QWidget *parent = nullptr);
    void setCurrentIndex(int idx);          // overrides via slot mechanism

private:
    void runFade(QWidget *target);
};
