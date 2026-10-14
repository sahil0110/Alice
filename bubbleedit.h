#pragma once
#include <QTextEdit>
#include <QFrame>
#include <QSizePolicy>
#include <QTextDocument>
#include <QResizeEvent>
#include <QWheelEvent>

class BubbleEdit : public QTextEdit {
    static constexpr int kMaxBubbleWidth = 520;
public:
    explicit BubbleEdit(QWidget *parent = nullptr) : QTextEdit(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setMaximumWidth(kMaxBubbleWidth);
        setReadOnly(true);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
        document()->setDocumentMargin(10);
        document()->setDefaultStyleSheet(
            "body  { color: #e0e4f0; }"
            "h1, h2, h3 { color: #ffffff; margin: 6px 0 3px 0; }"
            "h4, h5, h6  { color: #d0d8f0; margin: 4px 0 2px 0; }"
            "p    { margin: 3px 0; }"
            "ul, ol { margin: 3px 0; padding-left: 20px; }"
            "li   { color: #e0e4f0; margin: 2px 0; }"
            "code { background-color: #1a1d2a; color: #a8c4ff; font-family: monospace; }"
            "pre  { background-color: #1a1d2a; color: #a8c4ff; padding: 6px; }"
            "hr   { border-top: 1px solid #3a4060; }"
            "strong { color: #ffffff; font-weight: bold; }"
            "em   { color: #c0c8e0; font-style: italic; }"
            "blockquote { border-left: 3px solid #3a4060; padding-left: 10px; color: #9098b0; }"
        );
    }
    int contentHeight(int w) const {
        QTextDocument *doc = document()->clone();
        doc->setPageSize(QSizeF(w, -1));
        int h = qRound(doc->size().height());
        delete doc;
        return h;
    }
    QSize sizeHint() const override {
        QTextDocument *doc = document()->clone();
        doc->setPageSize(QSizeF(-1, -1));
        int idealW = qRound(doc->idealWidth()) + qRound(document()->documentMargin()) * 2 + 8;
        delete doc;
        int w = qMin(idealW, kMaxBubbleWidth);
        return QSize(w, contentHeight(w));
    }
    void resizeEvent(QResizeEvent *e) override {
        QTextEdit::resizeEvent(e);
        int h = contentHeight(e->size().width());
        if (h != height()) {
            setFixedHeight(h);
            updateGeometry();
        }
    }
    // Pass wheel events up so the outer chat scroll area handles them,
    // not the bubble itself (prevents scrolling inside the text box).
    void wheelEvent(QWheelEvent *e) override {
        e->ignore();
    }
};
