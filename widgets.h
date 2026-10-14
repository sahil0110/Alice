#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QStackedWidget>
#include <QTextEdit>

// Indeterminate rotating-arc spinner. Drop-in QWidget; size with setFixedSize.
// Auto-starts when shown, auto-stops when hidden. Colour follows Theme accent.
class Spinner : public QWidget
{
    Q_OBJECT
public:
    explicit Spinner(QWidget *parent = nullptr, int size = 22);
    void setColor(const QColor &c) { m_color = c; update(); }
    void setLineWidth(int w) { m_line = w; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    QTimer m_timer;
    int    m_angle = 0;
    int    m_line  = 3;
    QColor m_color;
};

// Pulsing dots — three dots that pulse in sequence ("Thinking..." style).
class PulseDots : public QWidget
{
    Q_OBJECT
public:
    explicit PulseDots(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QTimer m_timer;
    int    m_phase = 0;
};

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

// QTextEdit subclass that auto-sizes to its content and wraps text.
// Also handles scroll events properly for chat bubbles.
class BubbleEdit : public QTextEdit {
    static constexpr int kMaxBubbleWidth = 520;
public:
    explicit BubbleEdit(QWidget *parent = nullptr);
    int contentHeight(int w) const;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent *e) override;
    // Pass wheel events up so the outer chat scroll area handles them,
    // not the bubble itself (prevents scrolling inside the text box).
    void wheelEvent(QWheelEvent *e) override;
};