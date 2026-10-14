#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPoint>
#include <QFrame>

class ChatHud : public QWidget
{
    Q_OBJECT
public:
    explicit ChatHud(QWidget *parent = nullptr);
    void   setAiMode(bool on);
    void   setHudPos(const QPoint &pos);
    QPoint hudPos() const;
    void   applyTheme(const QString &bg, const QString &border,
                      const QString &text, const QString &accent);

signals:
    void commandEntered(const QString &text, bool aiMode);
    void aiModeChanged(bool on);
    void showAliceRequested();
    void hudMoved(const QPoint &pos);

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void changeEvent(QEvent *e) override;

private slots:
    void toggleCollapse();

private:
    void startDrag(QMouseEvent *e);
    bool handleDragMove(QMouseEvent *e);
    bool handleDragRelease(QMouseEvent *e);

    QLineEdit   *m_input      = nullptr;
    QPushButton *m_aiBtn      = nullptr;
    QPushButton *m_aliceBtn   = nullptr;
    QPushButton *m_toggleBtn  = nullptr;
    QLabel      *m_dragHandle = nullptr;
    QList<QWidget*> m_dragWidgets;

    bool    m_aiMode    = false;
    bool    m_collapsed = false;
    bool    m_dragging  = false;
    QPoint  m_pressPos;
    QPoint  m_dragOffset;

    QString m_bg     = "#111318";
    QString m_border = "#1e2130";
    QString m_text   = "#d0d4e8";
    QString m_accent = "#4a9eff";

    static constexpr int kHeight         = 40;
    static constexpr int kExpandedWidth  = 440;
    static constexpr int kCollapsedWidth = 44;
    static constexpr int kBtnWidth       = 52;
};