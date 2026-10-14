#include "theme.h"

// Forward declaration — defined further down in this file.
static QColor defaultAccent(Theme::Mode m);

Theme &Theme::instance()
{
    static Theme t;
    return t;
}

Theme::Theme() = default;

void Theme::setMode(Mode m)
{
    if (m_mode == m) return;
    m_mode = m;
    m_accent = defaultAccent(m);   // each theme carries its own accent
    emit themeChanged();
}

void Theme::setAccent(const QColor &c)
{
    if (m_accent == c) return;
    m_accent = c;
    emit themeChanged();
}

void Theme::setAnimated(bool on)
{
    if (m_animated == on) return;
    m_animated = on;
    emit themeChanged();
}

QColor Theme::windowBg() const
{
    switch (m_mode) {
        case Midnight: return QColor("#0a0c12");
        case Dark:     return QColor("#15171f");
        case Light:    return QColor("#f3f4f8");
        case Ocean:    return QColor("#060e18");
        case Forest:   return QColor("#060e09");
        case Sunset:   return QColor("#100810");
        case Slate:    return QColor("#0e1014");
        case Rose:     return QColor("#100810");
        case Neon:     return QColor("#050c0e");
    }
    return QColor("#0a0c12");
}
QColor Theme::surface() const
{
    switch (m_mode) {
        case Midnight: return QColor("#13161f");
        case Dark:     return QColor("#1d2030");
        case Light:    return QColor("#ffffff");
        case Ocean:    return QColor("#0d1a28");
        case Forest:   return QColor("#0d1a0f");
        case Sunset:   return QColor("#1a0d18");
        case Slate:    return QColor("#161820");
        case Rose:     return QColor("#1a0d16");
        case Neon:     return QColor("#071214");
    }
    return QColor("#13161f");
}
QColor Theme::surfaceAlt() const
{
    switch (m_mode) {
        case Midnight: return QColor("#181c28");
        case Dark:     return QColor("#262a3a");
        case Light:    return QColor("#eceef5");
        case Ocean:    return QColor("#122030");
        case Forest:   return QColor("#122014");
        case Sunset:   return QColor("#20121e");
        case Slate:    return QColor("#1e2028");
        case Rose:     return QColor("#20121a");
        case Neon:     return QColor("#091618");
    }
    return QColor("#181c28");
}
QColor Theme::border() const
{
    switch (m_mode) {
        case Midnight: return QColor("#1e2130");
        case Dark:     return QColor("#2a2e40");
        case Light:    return QColor("#d8dbe8");
        case Ocean:    return QColor("#1a2e42");
        case Forest:   return QColor("#1a2e1e");
        case Sunset:   return QColor("#2e1a2a");
        case Slate:    return QColor("#262830");
        case Rose:     return QColor("#2e1a24");
        case Neon:     return QColor("#0e2428");
    }
    return QColor("#1e2130");
}
QColor Theme::text() const
{
    return (m_mode == Light) ? QColor("#1a1d27") : QColor("#ffffff");
}
QColor Theme::textDim() const
{
    return (m_mode == Light) ? QColor("#4a5070") : QColor("#c8d0e8");
}
QColor Theme::textMuted() const
{
    switch (m_mode) {
        case Light:  return QColor("#7e85a3");
        case Ocean:  return QColor("#6090b0");
        case Forest: return QColor("#608060");
        case Sunset: return QColor("#906080");
        case Slate:  return QColor("#909098");
        case Rose:   return QColor("#906070");
        case Neon:   return QColor("#40b0b8");
        default:     return QColor("#9098b0");
    }
}
QColor Theme::sidebarBg() const
{
    switch (m_mode) {
        case Midnight: return QColor("#0d0f17");
        case Dark:     return QColor("#171a25");
        case Light:    return QColor("#e8eaf2");
        case Ocean:    return QColor("#081018");
        case Forest:   return QColor("#08100a");
        case Sunset:   return QColor("#10080e");
        case Slate:    return QColor("#0c0e12");
        case Rose:     return QColor("#10080c");
        case Neon:     return QColor("#04090b");
    }
    return QColor("#0d0f17");
}
QColor Theme::topBarBg() const
{
    switch (m_mode) {
        case Midnight: return QColor("#000000");
        case Dark:     return QColor("#000000");
        case Light:    return QColor("#ffffff");
        case Ocean:    return QColor("#000000");
        case Forest:   return QColor("#000000");
        case Sunset:   return QColor("#000000");
        case Slate:    return QColor("#000000");
        case Rose:     return QColor("#000000");
        case Neon:     return QColor("#000000");
    }
    return QColor("#000000");
}
QColor Theme::inputBg() const
{
    switch (m_mode) {
        case Midnight: return QColor("#0c0e15");
        case Dark:     return QColor("#171a25");
        case Light:    return QColor("#f7f8fc");
        case Ocean:    return QColor("#08121e");
        case Forest:   return QColor("#08120a");
        case Sunset:   return QColor("#120810");
        case Slate:    return QColor("#10121a");
        case Rose:     return QColor("#12080e");
        case Neon:     return QColor("#040a0c");
    }
    return QColor("#0c0e15");
}
// Per-theme accent helper (used when accent hasn't been customised)
static QColor defaultAccent(Theme::Mode m) {
    switch (m) {
        case Theme::Midnight: return QColor("#7b8cde");
        case Theme::Dark:     return QColor("#6b7cce");
        case Theme::Light:    return QColor("#5a6cbf");
        case Theme::Ocean:    return QColor("#4a9eff");
        case Theme::Forest:   return QColor("#50C878");
        case Theme::Sunset:   return QColor("#ff6b9d");
        case Theme::Slate:    return QColor("#9098b0");
        case Theme::Rose:     return QColor("#fb7185");
        case Theme::Neon:     return QColor("#22d3ee");
    }
    return QColor("#7b8cde");
}

QString Theme::globalStyleSheet() const
{
    const QString bg   = windowBg().name();
    const QString surf = surface().name();
    const QString surfa= surfaceAlt().name();
    const QString brd  = border().name();
    const QString txt  = text().name();
    const QString acc  = m_accent.name();

    // %1=bg  %2=surf  %3=brd  %4=surfa  %5=txt  %6=acc
    QString qss = QString(
        "QToolTip { background: %2; color: %5; border: 1px solid %3;"
        "  border-radius: 6px; padding: 6px 10px; font-size: 11px; }"
        "QMenu { background: %2; color: %5; border: 1px solid %3;"
        "  border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 6px 18px; border-radius: 6px; font-size: 12px; }"
        "QMenu::item:selected { background: %4; color: %6; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }"
        "QScrollBar:vertical   { background: transparent; width: 6px; margin: 2px; }"
        "QScrollBar:horizontal { background: transparent; height: 6px; margin: 2px; }"
        "QScrollBar::handle:vertical   { background: %3; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:horizontal { background: %3; border-radius: 3px; min-width:  30px; }"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: %6; }"
        "QScrollBar::add-line:vertical,  QScrollBar::sub-line:vertical  { height: 0; }"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{ width:  0; }"
        "QScrollBar::add-page:vertical,  QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{ background: none; }"
        "QDialog { background: %1; color: %5; }"
        "QDialog QLabel { color: %5; }"
        "QComboBox QAbstractItemView { background: %2; color: %5; border: 1px solid %3;"
        "  selection-background-color: %4; selection-color: %6;"
        "  border-radius: 6px; padding: 4px; font-size: 12px; }"
    )
    .arg(bg, surf, brd, surfa, txt, acc);

    return qss;
}

QString Theme::cardStyle() const
{
    return QString("background: %1; border: 1px solid %2; border-radius: %3px;")
        .arg(surface().name(), border().name())
        .arg(radius());
}

QString Theme::primaryButtonStyle() const
{
    return QString(
        "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
        "padding: 8px 18px; font-size: 12px; font-weight: 500; }"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }")
        .arg(m_accent.name(),
             m_accent.lighter(115).name(),
             m_accent.darker(115).name(),
             surfaceAlt().name(),
             textMuted().name());
}

QString Theme::ghostButtonStyle() const
{
    return QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        "border-radius: 8px; padding: 7px 14px; font-size: 12px; }"
        "QPushButton:hover { background: %3; color: %4; border-color: %4; }")
        .arg(textMuted().name(),
             border().name(),
             surfaceAlt().name(),
             m_accent.name());
}

QString Theme::inputStyle() const
{
    return QString(
        "QLineEdit, QPlainTextEdit, QTextEdit { background: %1; color: %2;"
        "border: 1px solid %3; border-radius: 8px; padding: 8px 12px; selection-background-color: %4; }"
        "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %4; }")
        .arg(inputBg().name(), text().name(), border().name(), m_accent.name());
}
