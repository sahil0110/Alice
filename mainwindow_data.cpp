#include "mainwindow.h"
#include "widgets.h"
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
#include <climits>
#include <QProcess>
#include <QPlainTextEdit>
#include <memory>

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
        QTextStream out(&f);
        out << QString("[%1] %2: %3\n").arg(time).arg(sender).arg(text);
        f.close();
    }
}

void MainWindow::refreshHistory()
{
    // Clear existing entries
    for (int i = historyViewLayout->count() - 1; i >= 0; --i) {
        QLayoutItem *item = historyViewLayout->takeAt(i);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
    }

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Alice");
    if (!dir.exists()) return;

    QStringList files = dir.entryList(QStringList() << "*.txt", QDir::Files);
    for (const QString &file : files) {
        QPushButton *btn = new QPushButton(file.remove(".txt"));
        btn->setFixedHeight(32);
        btn->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; padding-left: 10px; }"
                          "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }");

        QObject::connect(btn, &QPushButton::clicked, this, [this, file]() {
            loadHistorySession(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Alice/" + file, file.remove(".txt"));
        });

        historyViewLayout->addWidget(btn);
    }
}

void MainWindow::loadHistorySession(const QString &path, const QString &name)
{
    // Implementation for loading a session from history
}

void MainWindow::addTask(const QString &title, int seconds, bool cancellable)
{
    QWidget *taskWidget = new QWidget();
    taskWidget->setObjectName(title);
    taskWidget->setProperty("activeTask", true);
    taskWidget->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
    QHBoxLayout *taskLayout = new QHBoxLayout(taskWidget);
    taskLayout->setContentsMargins(14, 10, 14, 10);

    QLabel *taskTitle = new QLabel(title);
    taskTitle->setStyleSheet("color: #ffffff; font-size: 13px;");

    QLabel *countdown = new QLabel();
    countdown->setStyleSheet("color: #f0c040; font-size: 12px;");
    countdown->setAlignment(Qt::AlignRight);

    taskLayout->addWidget(taskTitle);
    taskLayout->addStretch();
    taskLayout->addWidget(countdown);

    tasksContentLayout->addWidget(taskWidget);

    // Start countdown
    QTimer *timer = new QTimer(this);
    int remaining = seconds;
    timer->setInterval(1000);

    QObject::connect(timer, &QTimer::timeout, [this, timer, countdown, &remaining]() {
        if (remaining > 0) {
            remaining--;
            int hours = remaining / 3600;
            int minutes = (remaining % 3600) / 60;
            int seconds = remaining % 60;
            countdown->setText(QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0')));
        } else {
            timer->stop();
            // Task completed - remove from UI
            QObject *obj = sender();
            if (obj) {
                QWidget *widget = qobject_cast<QWidget*>(obj->parent());
                if (widget) widget->deleteLater();
            }
        }
    });

    timer->start();

    // Add to scheduled tasks layout if it's a scheduled task
    if (!cancellable) {
        scheduledTasksLayout->addWidget(taskWidget);
    }
}

void MainWindow::addScheduledTask(const QString &task, const QDateTime &when)
{
    QWidget *taskWidget = new QWidget();
    taskWidget->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
    QHBoxLayout *taskLayout = new QHBoxLayout(taskWidget);
    taskLayout->setContentsMargins(14, 10, 14, 10);

    QLabel *taskTitle = new QLabel(task);
    taskTitle->setStyleSheet("color: #ffffff; font-size: 13px;");

    QLabel *timeLabel = new QLabel(when.toString("hh:mm:ss"));
    timeLabel->setStyleSheet("color: #f0c040; font-size: 12px;");
    timeLabel->setAlignment(Qt::AlignRight);

    taskLayout->addWidget(taskTitle);
    taskLayout->addStretch();
    taskLayout->addWidget(timeLabel);

    scheduledTasksLayout->addWidget(taskWidget);
}

void MainWindow::refreshAppLogs()
{
    // Implementation for refreshing app logs
}

void MainWindow::setupHistoryPage()
{
    // This is a stub - actual implementation in mainwindow_history.cpp
}

void MainWindow::setupTasksPage()
{
    // This is a stub - actual implementation in mainwindow_tasks.cpp
}