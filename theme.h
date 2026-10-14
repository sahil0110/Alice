#pragma once
#include <QObject>
#include <QColor>
#include <QString>

// Centralised palette + global stylesheet generator.
// Emits `themeChanged()` whenever any visual setting changes;
// MainWindow listens and re-applies styles on the surfaces it controls.
class Theme : public QObject
{
    Q_OBJECT
public:
    enum Mode { Midnight, Dark, Light, Ocean, Forest, Sunset, Slate, Rose, Neon };

    static Theme &instance();

    Mode    mode()     const { return m_mode; }
    QColor  accent()   const { return m_accent; }
    bool    animated() const { return m_animated; }

    // Palette accessors — automatically chosen from the current mode
    QColor windowBg()  const;
    QColor surface()   const;
    QColor surfaceAlt()const;   // a touch lighter/darker than surface
    QColor border()    const;
    QColor text()      const;
    QColor textDim()   const;
    QColor textMuted() const;
    QColor sidebarBg() const;
    QColor topBarBg()  const;
    QColor inputBg()   const;
    QColor success()   const { return QColor("#50C878"); }
    QColor danger()    const { return QColor("#ff5577"); }

    int padding() const { return 6; }  // fixed compact padding
    int radius()  const { return 10; }

    void setMode(Mode m);
    void setAccent(const QColor &c);
    void setAnimated(bool on);

    // Returns a comprehensive QSS string for QApplication::setStyleSheet
    QString globalStyleSheet() const;

    // Convenience builders for re-styled surfaces
    QString cardStyle() const;
    QString primaryButtonStyle() const;
    QString ghostButtonStyle() const;
    QString inputStyle() const;

signals:
    void themeChanged();

private:
    Theme();
    Mode    m_mode     = Midnight;
    QColor  m_accent   = QColor("#7b8cde");
    bool    m_animated = true;
};
