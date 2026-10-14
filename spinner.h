#pragma once
#include <QWidget>
#include <QTimer>

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
