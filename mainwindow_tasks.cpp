#include "mainwindow.h"
#include <climits>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QProcess>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QRegularExpression>
#include <QStackedWidget>
#include <memory>

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

    auto remaining = std::make_shared<int>(seconds);
    auto updateCountdown = [countdown, remaining]() {
        int m = *remaining / 60;
        int s = *remaining % 60;
        countdown->setText(QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0')));
        (*remaining)--;
    };
    updateCountdown();

    QTimer *timer = new QTimer(taskWidget);
    QObject::connect(timer, &QTimer::timeout, taskWidget, [this, updateCountdown, remaining, taskWidget, timer]() {
        if (*remaining <= 0) {
            timer->stop();
            taskWidget->deleteLater();
            QTimer::singleShot(0, this, &MainWindow::updateStatus);
            return;
        }
        updateCountdown();
    });
    timer->start(1000);

    taskLayout->addWidget(taskTitle);
    taskLayout->addStretch();
    taskLayout->addWidget(countdown);

    if (cancellable) {
        QPushButton *cancelBtn = new QPushButton("Cancel");
        cancelBtn->setFlat(true);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet("QPushButton { color: #ff4444; font-size: 12px; padding: 4px 8px;"
                                 "border: 1px solid #ff4444; border-radius: 4px; }"
                                 "QPushButton:hover { background: #2a1010; }");
        connect(cancelBtn, &QPushButton::clicked, taskWidget, [this, taskWidget, timer]() {
            timer->stop();
            QProcess::startDetached("shutdown", QStringList() << "/a");
            taskWidget->deleteLater();
            QTimer::singleShot(0, this, &MainWindow::updateStatus);
        });
        taskLayout->addSpacing(8);
        taskLayout->addWidget(cancelBtn);
    }

    // remove "No active tasks" label if present
    if (tasksContentLayout->count() > 1) {
        QLayoutItem *first = tasksContentLayout->itemAt(0);
        if (first && first->widget()) {
            QLabel *lbl = qobject_cast<QLabel*>(first->widget());
            if (lbl) { delete lbl; }
        }
    }

    tasksContentLayout->insertWidget(0, taskWidget);
    updateStatus();
}

void MainWindow::addScheduledTask(const QString &task, const QDateTime &when)
{
    // Remove the "No scheduled tasks." placeholder if present
    for (int i = 0; i < scheduledTasksLayout->count(); ++i) {
        QLayoutItem *li = scheduledTasksLayout->itemAt(i);
        if (!li || !li->widget()) continue;
        QLabel *lbl = qobject_cast<QLabel*>(li->widget());
        if (lbl && lbl->objectName() == "noScheduledLabel") {
            delete scheduledTasksLayout->takeAt(i)->widget();
            break;
        }
    }

    // Create card
    QWidget *card = new QWidget();
    card->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(14, 10, 14, 10);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);
    QLabel *taskLbl = new QLabel(task);
    taskLbl->setStyleSheet("color: #ffffff; font-size: 13px;");
    QLabel *timeLbl = new QLabel(when.toString("ddd dd MMM, HH:mm"));
    timeLbl->setStyleSheet("color: #9098b0; font-size: 11px;");
    infoLayout->addWidget(taskLbl);
    infoLayout->addWidget(timeLbl);

    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setFlat(true);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet("QPushButton { color: #ff4444; font-size: 12px; padding: 4px 8px;"
                             "border: 1px solid #ff4444; border-radius: 4px; }"
                             "QPushButton:hover { background: #2a1010; }");

    cardLayout->addLayout(infoLayout, 1);
    cardLayout->addWidget(cancelBtn);

    scheduledTasksLayout->addWidget(card);

    // Fire at the specified datetime
    qint64 msUntil = QDateTime::currentDateTime().msecsTo(when);
    if (msUntil <= 0) msUntil = 1;

    QTimer *timer = new QTimer(card);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, task, card, timer]() {
        timer->stop();
        addMessage("\u23f0 Reminder: " + task, "alice");
        logAppEvent("Scheduled task fired: " + task);
        card->deleteLater();

        // Re-add placeholder if no scheduled tasks remain
        bool hasCards = false;
        for (int i = 0; i < scheduledTasksLayout->count(); ++i) {
            QLayoutItem *li = scheduledTasksLayout->itemAt(i);
            if (li && li->widget() && li->widget() != card)
                hasCards = true;
        }
        if (!hasCards) {
            QLabel *noSched = new QLabel("No scheduled tasks.");
            noSched->setObjectName("noScheduledLabel");
            noSched->setStyleSheet("color: #9098b0; font-size: 12px;");
            scheduledTasksLayout->addWidget(noSched);
        }
    });
    timer->start(static_cast<int>(qMin(msUntil, static_cast<qint64>(INT_MAX))));

    connect(cancelBtn, &QPushButton::clicked, this, [this, card, timer]() {
        timer->stop();
        card->deleteLater();

        // Re-add placeholder if layout is now empty
        QTimer::singleShot(0, this, [this]() {
            bool hasCards = false;
            for (int i = 0; i < scheduledTasksLayout->count(); ++i) {
                QLayoutItem *li = scheduledTasksLayout->itemAt(i);
                if (li && li->widget()) hasCards = true;
            }
            if (!hasCards) {
                QLabel *noSched = new QLabel("No scheduled tasks.");
                noSched->setObjectName("noScheduledLabel");
                noSched->setStyleSheet("color: #9098b0; font-size: 12px;");
                scheduledTasksLayout->addWidget(noSched);
            }
        });
    });
}

void MainWindow::startWingetListing()
{
    // Discard any existing card
    if (wingetTaskCard) {
        wingetTaskCard->deleteLater();
        wingetTaskCard = nullptr;
        wingetOutput   = nullptr;
    }
    wingetPending = false;
    wingetUpgradeCount   = 0;

    // Build task card
    QWidget *card = new QWidget();
    card->setObjectName("WingetUpgrade");
    card->setProperty("activeTask", true);
    card->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(8);

    QLabel *cardTitle = new QLabel("Checking for updates...");
    cardTitle->setStyleSheet("color: #ffffff; font-size: 13px;");
    cardLayout->addWidget(cardTitle);

    QPlainTextEdit *outputEdit = new QPlainTextEdit();
    outputEdit->setReadOnly(true);
    outputEdit->setMaximumHeight(220);
    outputEdit->setStyleSheet(
        "QPlainTextEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
        "color: #9098b0; font-size: 11px; font-family: monospace; padding: 6px; }");
    cardLayout->addWidget(outputEdit);

    wingetTaskCard = card;
    wingetOutput   = outputEdit;

    // Remove "No active tasks" label if present
    if (tasksContentLayout->count() > 1) {
        QLayoutItem *first = tasksContentLayout->itemAt(0);
        if (first && first->widget() && qobject_cast<QLabel*>(first->widget()))
            delete first->widget();
    }
    tasksContentLayout->insertWidget(0, card);
    updateStatus();
    stack->setCurrentIndex(4); // switch to Tasks view

    // Run winget upgrade (listing only)
    QProcess *proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        if (!wingetOutput) return;
        wingetOutput->appendPlainText(
            QString::fromLocal8Bit(proc->readAllStandardOutput()));
        wingetOutput->verticalScrollBar()->setValue(wingetOutput->verticalScrollBar()->maximum());
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, cardTitle](int, QProcess::ExitStatus) {
        proc->deleteLater();

        // Parse upgrade count from winget's own summary line: "X upgrades available."
        if (wingetOutput) {
            const QString fullText = wingetOutput->toPlainText();
            for (const QString &ln : fullText.split('\n')) {
                const QString t = ln.trimmed();
                // Matches e.g. "6 upgrades available." or "1 upgrade available."
                static const QRegularExpression re(R"(^(\d+) upgrades? available\.$)",
                                                   QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch m = re.match(t);
                if (m.hasMatch()) {
                    wingetUpgradeCount = m.captured(1).toInt();
                    break;
                }
            }
        }

        if (wingetTaskCard && cardTitle)
            cardTitle->setText("Winget Update");

        if (wingetUpgradeCount > 0) {
            addMessage("Should I start the update? Type yes or no.", "alice");
            logAppEvent(QString("Winget found %1 upgrade(s) available").arg(wingetUpgradeCount));
            wingetPending = true;
        } else {
            addMessage("No upgrades available.", "alice");
            logAppEvent("Winget upgrade: no upgrades available");
        }
    });

    proc->start("winget", QStringList() << "upgrade");
}
