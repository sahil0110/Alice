#include "spinner.h"
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
