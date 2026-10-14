#include "mainwindow.h"
#include "commands.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QScreen>
#include <QGuiApplication>
#include <QPointer>
#include <QSpinBox>
#include <memory>

void MainWindow::onAddAppProfile()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Add App Profile");
    dialog->setModal(true);
    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QLineEdit *nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("Profile name (e.g. 'Gaming')");
    layout->addWidget(nameEdit);

    QLineEdit *exePathEdit = new QLineEdit();
    exePathEdit->setPlaceholderText("Executable path");
    layout->addWidget(exePathEdit);

    QPushButton *browseBtn = new QPushButton("Browse...");
    layout->addWidget(browseBtn);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton("Save");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    QObject::connect(browseBtn, &QPushButton::clicked, [=]() {
        QString file = QFileDialog::getOpenFileName(dialog, "Select executable", "", "Executable files (*.exe)");
        if (!file.isEmpty()) exePathEdit->setText(file);
    });

    QObject::connect(saveBtn, &QPushButton::clicked, [=]() {
        QString name = nameEdit->text().trimmed();
        QString path = exePathEdit->text().trimmed();
        if (!name.isEmpty() && !path.isEmpty()) {
            addAppProfileEntry(name, path);
            dialog->accept();
        }
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

    dialog->exec();
}

void MainWindow::addAppProfileEntry(const QString &name, const QString &exePath)
{
    // Add to the UI
    QPushButton *btn = new QPushButton(name);
    btn->setObjectName("profileBtn");
    btn->setFixedHeight(32);
    btn->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; padding-left: 10px; }"
                      "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }");

    // Store the path in the button's data
    btn->setProperty("exePath", exePath);

    QObject::connect(btn, &QPushButton::clicked, this, [this, btn]() {
        QString exe = btn->property("exePath").toString();
        if (!exe.isEmpty()) {
            // Start watching for this app
            addAppWatcher(btn->text(), exe, btn);
        }
    });

    appListEntriesLayout->addWidget(btn);

    // Save to file
    saveProfiles();
}

void MainWindow::saveProfiles()
{
    QJsonArray profiles;
    for (int i = 0; i < appListEntriesLayout->count(); ++i) {
        QPushButton *btn = qobject_cast<QPushButton*>(appListEntriesLayout->itemAt(i)->widget());
        if (btn && btn->objectName() == "profileBtn") {
            QJsonObject profile;
            profile["name"] = btn->text();
            profile["exePath"] = btn->property("exePath").toString();
            profiles.append(profile);
        }
    }

    QJsonDocument doc(profiles);
    QFile file(profilesFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::loadProfiles()
{
    // Clear existing entries
    for (int i = appListEntriesLayout->count() - 1; i >= 0; --i) {
        QLayoutItem *item = appListEntriesLayout->takeAt(i);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
    }

    QFile file(profilesFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return;

        for (const auto &value : doc.array()) {
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                addAppProfileEntry(obj["name"].toString(), obj["exePath"].toString());
            }
        }
    }
}

void MainWindow::addAppWatcher(const QString &appName, const QString &exeName, QPushButton *profileBtn)
{
    // Register with the system
    registerWatcher(exeName, [this, profileBtn](bool running) {
        if (running) {
            profileBtn->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; padding-left: 10px; }"
                                      "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }"
                                      "QPushButton { color: #00ff00; }");
        } else {
            profileBtn->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; padding-left: 10px; }"
                                      "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }");
        }
    });
}

void MainWindow::setupAppProfilesPage()
{
    // This is a stub - actual implementation in mainwindow_profiles.cpp
}

// ── Sequences ───────────────────────────────────────────────────────────────

void MainWindow::addActionRow(QWidget *container, const QJsonObject &data)
{
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QPushButton *actionBtn = new QPushButton("Action");
    actionBtn->setFixedWidth(100);
    layout->addWidget(actionBtn);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItem("Run Program");
    typeCombo->addItem("Send Message");
    typeCombo->addItem("Wait");
    typeCombo->setCurrentText(data["type"].toString());
    layout->addWidget(typeCombo);

    QLineEdit *argEdit = new QLineEdit();
    argEdit->setText(data["argument"].toString());
    layout->addWidget(argEdit);

    QPushButton *removeBtn = new QPushButton("Remove");
    removeBtn->setFixedWidth(80);
    layout->addWidget(removeBtn);

    // Set up connections
    QObject::connect(removeBtn, &QPushButton::clicked, [container]() {
        container->deleteLater();
    });
}

void MainWindow::addSequenceEntry(const QString &name, const QString &triggerType,
                                  const QString &triggerExe, const QJsonArray &actions)
{
    // Add to UI
    QPushButton *btn = new QPushButton(name);
    btn->setObjectName("seqBtn");
    btn->setFixedHeight(32);
    btn->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; padding-left: 10px; }"
                      "QPushButton:hover { background: rgba(128, 128, 128, 0.2); }");

    QObject::connect(btn, &QPushButton::clicked, this, [this, name, triggerType, triggerExe, actions]() {
        // Execute sequence
        executeSequenceActions(actions, -1);
    });

    seqListEntriesLayout->addWidget(btn);

    // Save to file
    saveSequences();
}

void MainWindow::executeSequenceActions(const QJsonArray &actions, int idx)
{
    for (int i = 0; i < actions.size(); ++i) {
        QJsonObject action = actions[i].toObject();
        QString type = action["type"].toString();
        QString argument = action["argument"].toString();

        if (type == "Run Program") {
            QProcess *process = new QProcess(this);
            process->start(argument);
        } else if (type == "Send Message") {
            // Send message to AI
            sendToAI(argument, false);
        } else if (type == "Wait") {
            // Wait for specified time
            int waitTime = argument.toInt();
            QTimer::singleShot(waitTime * 1000, this, [this, actions, idx]() {
                executeSequenceActions(actions, idx + 1);
            });
            return;
        }
    }
}

void MainWindow::saveSequences()
{
    QJsonArray sequences;
    for (int i = 0; i < seqListEntriesLayout->count(); ++i) {
        QPushButton *btn = qobject_cast<QPushButton*>(seqListEntriesLayout->itemAt(i)->widget());
        if (btn && btn->objectName() == "seqBtn") {
            QJsonObject sequence;
            sequence["name"] = btn->text();
            // In a real implementation, this would save the actual sequence data
            sequences.append(sequence);
        }
    }

    QJsonDocument doc(sequences);
    QFile file(sequencesFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::loadSequences()
{
    // Clear existing entries
    for (int i = seqListEntriesLayout->count() - 1; i >= 0; --i) {
        QLayoutItem *item = seqListEntriesLayout->takeAt(i);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
    }

    QFile file(sequencesFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return;

        for (const auto &value : doc.array()) {
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                // In a real implementation, we would load the sequence data properly
                addSequenceEntry(obj["name"].toString(), "Run Program", "", QJsonArray());
            }
        }
    }
}

void MainWindow::setupSequencesPage()
{
    // This is a stub - actual implementation in mainwindow_sequences.cpp
}

void MainWindow::startSequenceWatcher(QPushButton *btn)
{
    // Implementation for sequence watcher
}