#pragma once
#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

// Slide-up notification pill anchored to a parent's bottom-centre.
// Usage:  Toast::show(this, "Settings saved", Toast::Success);
class Toast : public QWidget
{
    Q_OBJECT
public:
    enum Variant { Info, Success, Warning, Error };

    static void show(QWidget *parent, const QString &message,
                     Variant variant = Info, int durationMs = 2200);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Toast(QWidget *parent, const QString &message, Variant v);
    void appearAt(const QPoint &endPos);
    void dismiss();

    QString m_text;
    Variant m_variant;
    QLabel *m_label;
    QGraphicsOpacityEffect *m_opacity;
};
