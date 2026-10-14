#include "mainwindow.h"
#include "bubbleedit.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QStackedWidget>
#include <QScrollBar>
#include <QRegularExpression>

// Low-level writer — receives a pre-formatted line from aliceMessageHandler.
// Do NOT call this directly; use the logAppEvent() macro instead.
void MainWindow::writeAppLog(const QString &formatted)
{
    QFile f(appLogFilePath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << formatted << "\n";
        f.close();
    }
}

void MainWindow::saveMessage(const QString &text, const QString &sender)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm");

    QFile f(sessionFilePath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QJsonObject obj;
        obj["time"] = time;
        obj["sender"] = sender;
        obj["text"] = text;
        QTextStream out(&f);
        out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
        f.close();
    }

    QFile lf(logFilePath);
    if (lf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&lf);
        out << "[" << time << "] " << sender << ": " << text << "\n";
        lf.close();
    }

    QFile hf(historyFilePath);
    if (hf.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&hf);
        out << "[" << time << "] " << sender << ": " << text << "\n";
        hf.close();
    }
}

void MainWindow::refreshAppLogs()
{
    QFile f(appLogFilePath);
    if (!f.exists()) {
        appLogsTextLabel->setTextFormat(Qt::PlainText);
        appLogsTextLabel->setText("No application log yet.");
        return;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    // Matches the structured format written by aliceMessageHandler:
    // [HH:mm:ss.zzz] [D/I/W/C/F] file.cpp:line (Class::method) message
    static const QRegularExpression logRe(
        R"(\[(\d{2}:\d{2}:\d{2}\.\d{3})\] \[([DIWCF])\] (\S+:\d+) \(([^)]*)\) (.*))");

    QTextStream in(&f);
    QString html = "<div style='font-family:\"Consolas\",monospace; font-size:12px; line-height:1.7;'>";

    while (!in.atEnd()) {
        QString line = in.readLine();

        // \u2500\u2500 Session / Loader dividers \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        if (line.contains("\u2500\u2500 Session Start \u2500\u2500")) {
            html += "<div style='color:#3a4060; margin:14px 0 4px 0; border-top:1px solid #1e2130; padding-top:8px;'>"
                    + line.toHtmlEscaped() + "</div>";
            continue;
        }
        if (line.contains("\u2500\u2500 Loader Start \u2500\u2500") || line.contains("\u2500\u2500 Loader End")) {
            html += "<div style='color:#2a5060; margin:6px 0 2px 0; padding-left:4px;'>"
                    + line.toHtmlEscaped() + "</div>";
            continue;
        }
        if (line.trimmed().isEmpty()) continue;

        // \u2500\u2500 Structured log line \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        QRegularExpressionMatch m = logRe.match(line);
        if (m.hasMatch()) {
            QString ts    = m.captured(1).toHtmlEscaped();  // HH:mm:ss.zzz
            QString level = m.captured(2);                  // D/I/W/C/F
            QString src   = m.captured(3).toHtmlEscaped();  // file.cpp:line
            QString func  = m.captured(4).toHtmlEscaped();  // Class::method
            QString msg   = m.captured(5).toHtmlEscaped();  // the message

            // Colours by severity
            QString tsColor, lvColor, srcColor, msgColor;
            bool bold = false;

            if (level == "D") {
                tsColor = "#404860"; lvColor = "#505870"; srcColor = "#404860"; msgColor = "#606880";
            } else if (level == "I") {
                tsColor = "#505870"; lvColor = "#7b8cde"; srcColor = "#505870"; msgColor = "#c8d0e8";
            } else if (level == "W") {
                tsColor = "#806830"; lvColor = "#f0b429"; srcColor = "#806830"; msgColor = "#f0c44a";
                bold = true;
            } else { // C or F
                tsColor = "#804040"; lvColor = "#ef4444"; srcColor = "#804040"; msgColor = "#ef6060";
                bold = true;
            }

            QString bO = bold ? "<b>" : "";
            QString bC = bold ? "</b>" : "";

            html += QString(
                "<div>"
                "<span style='color:%1'>[%2]</span>&nbsp;"
                "<span style='color:%3'>%4[%5]%6</span>&nbsp;"
                "<span style='color:%7'>%8&nbsp;<span style='opacity:0.6'>(%9)</span></span>&nbsp;"
                "<span style='color:%10'>%11</span>"
                "</div>")
                .arg(tsColor, ts,
                     lvColor, bO, level.toHtmlEscaped(), bC,
                     srcColor, src, func,
                     msgColor, msg);
            continue;
        }

        // \u2500\u2500 Old-format or unrecognised line (backward compat) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        html += "<div style='color:#505870'>" + line.toHtmlEscaped() + "</div>";
    }

    html += "</div>";
    f.close();
    appLogsTextLabel->setTextFormat(Qt::RichText);
    appLogsTextLabel->setText(html);
    // Scroll to bottom immediately (works when the scroll area is already laid out)
    // and also on rangeChanged (fires on first open when layout hasn't run yet).
    {
        QScrollBar *sb = logsScroll->verticalScrollBar();
        sb->setValue(sb->maximum());
        QObject::connect(sb, &QScrollBar::rangeChanged, sb,
                         [sb](int, int max) { sb->setValue(max); },
                         Qt::SingleShotConnection);
    }
}

void MainWindow::refreshHistory()
{
    QFile f(historyFilePath);
    if (!f.exists()) {
        historyTextLabel->setTextFormat(Qt::PlainText);
        historyTextLabel->setText("No history yet.");
        return;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    QString html = "<div style='font-family:monospace; font-size:12px; line-height:1.6;'>";
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("\u2500\u2500 Session Start \u2500\u2500")) {
            html += "<div style='color:#3a4060; margin:12px 0 4px 0;'>"
                    + line.toHtmlEscaped() + "</div>";
        } else if (line.trimmed().isEmpty()) {
            // skip blank lines
        } else {
            html += "<div>" + line.toHtmlEscaped() + "</div>";
        }
    }
    html += "</div>";
    f.close();
    historyTextLabel->setTextFormat(Qt::RichText);
    historyTextLabel->setText(html);
    {
        QScrollBar *sb = historyScroll->verticalScrollBar();
        sb->setValue(sb->maximum());
        QObject::connect(sb, &QScrollBar::rangeChanged, sb,
                         [sb](int, int max) { sb->setValue(max); },
                         Qt::SingleShotConnection);
    }
}

void MainWindow::loadHistorySession(const QString &path, const QString &name)
{
    historyViewTitle->setText(name);

    QLayoutItem *item;
    while (historyViewLayout->count() > 1) {
        item = historyViewLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith("---")) {
            QWidget *sepWrapper = new QWidget();
            QVBoxLayout *sepLayout = new QVBoxLayout(sepWrapper);
            sepLayout->setContentsMargins(12, 10, 12, 10);
            sepLayout->setSpacing(0);
            QFrame *sep = new QFrame();
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color: #3a3f55; background-color: #3a3f55; max-height: 1px;");
            sepLayout->addWidget(sep);
            historyViewLayout->insertWidget(historyViewLayout->count()-1, sepWrapper);
            continue;
        }
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isNull()) continue;
        QJsonObject obj = doc.object();
        QString sender = obj["sender"].toString();
        QString text = obj["text"].toString();
        QString time = obj["time"].toString();

        QWidget *msgWidget = new QWidget();
        QVBoxLayout *msgLayout = new QVBoxLayout(msgWidget);
        msgLayout->setContentsMargins(0,0,0,0);
        msgLayout->setSpacing(3);

        if (sender == "user") {
            QLabel *label = new QLabel("User  " + time);
            label->setStyleSheet("color: #9098b0; font-size: 12px; letter-spacing: 1px;");
            label->setAlignment(Qt::AlignRight);
            msgLayout->addWidget(label);
        } else {
            QLabel *label = new QLabel(sender == "alice" ? "Alice  " + time : sender + "  " + time);
            label->setStyleSheet("color: #9098b0; font-size: 12px; letter-spacing: 1px;");
            msgLayout->addWidget(label);
        }

        BubbleEdit *bubble = new BubbleEdit();
        bubble->setPlainText(text);

        if (sender == "user") {
            bubble->setStyleSheet("QTextEdit { background: #161924; color: #ffffff; border: 1px solid #252a3a;"
                                  "border-radius: 8px; font-size: 13px; padding: 0px 4px; }");
            msgWidget->setContentsMargins(100, 0, 0, 0);
        } else if (sender == "system") {
            bubble->setStyleSheet("QTextEdit { background: #0f1a14; color: #9098b0; border: 1px solid #1a2e20;"
                                  "border-radius: 8px; font-size: 12px; padding: 0px 4px; }");
        } else {
            bubble->setStyleSheet("QTextEdit { background: #13161f; color: #9098b0; border: 1px solid #1e2130;"
                                  "border-radius: 8px; font-size: 12px; padding: 0px 4px; }");
            msgWidget->setContentsMargins(0, 0, 100, 0);
        }

        msgLayout->addWidget(bubble);
        historyViewLayout->insertWidget(historyViewLayout->count()-1, msgWidget);
    }
    f.close();
    stack->setCurrentIndex(2);
    {
        QScrollBar *sb = historyEntryScroll->verticalScrollBar();
        sb->setValue(sb->maximum());
        QObject::connect(sb, &QScrollBar::rangeChanged, sb,
                         [sb](int, int max) { sb->setValue(max); },
                         Qt::SingleShotConnection);
    }
}
