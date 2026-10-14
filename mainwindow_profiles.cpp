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
    QString path = QFileDialog::getOpenFileName(this, "Select App", "C:/", "All Files (*);;Executables (*.exe)");
    if (path.isEmpty()) return;
    QString name = QFileInfo(path).baseName();
    addAppProfileEntry(name, path);
    saveProfiles();
}

void MainWindow::addAppProfileEntry(const QString &name, const QString &exePath)
{
    QPushButton *appBtn = new QPushButton(name);
    appBtn->setFlat(true);
    appBtn->setCursor(Qt::PointingHandCursor);
    appBtn->setStyleSheet("QPushButton { color: #ffffff; font-size: 13px; padding: 9px 14px;"
                          "text-align: left; border-bottom: 1px solid #13161f; background: transparent; }"
                          "QPushButton:hover { background: #181c28; }");
    appBtn->setProperty("exePath", exePath);
    appBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(appBtn, &QPushButton::customContextMenuRequested, this, [this, appBtn](const QPoint &pos) {
        QMenu menu;
        menu.setStyleSheet("QMenu { background: #13161f; color: #ffffff; border: 1px solid #1e2130; }"
                           "QMenu::item:selected { background: #181c28; }");
        QAction *removeAction = menu.addAction("Remove Profile");
        if (menu.exec(appBtn->mapToGlobal(pos)) == removeAction) {
            int idx = appListEntriesLayout->indexOf(appBtn);
            if (idx >= 0 && idx + 1 < appProfileDetail->count()) {
                QWidget *detail = appProfileDetail->widget(idx + 1);
                appProfileDetail->removeWidget(detail);
                detail->deleteLater();
            }
            appListEntriesLayout->removeWidget(appBtn);
            appBtn->deleteLater();
            saveProfiles();
        }
    });

    // build detail panel
    QWidget *detailWidget = new QWidget();
    detailWidget->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(0);

    // tabs
    QWidget *tabBar = new QWidget();
    tabBar->setStyleSheet("background: #0a0c12; border-bottom: 1px solid #1e2130;");
    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    QStackedWidget *tabContent = new QStackedWidget();
    tabContent->setStyleSheet("background: #0c0e15;");

    auto makeTab = [tabLayout, tabContent](const QString &label, int idx) {
        QPushButton *tab = new QPushButton(label);
        tab->setFlat(true);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setStyleSheet(idx == 0
                               ? "QPushButton { color: #ffffff; font-size: 12px; padding: 10px 16px; border-bottom: 2px solid #9098b0; background: transparent; }"
                               : "QPushButton { color: #4a5068; font-size: 12px; padding: 10px 16px; border-bottom: 2px solid transparent; background: transparent; }"
                                 "QPushButton:hover { color: #9098b0; }");
        QObject::connect(tab, &QPushButton::clicked, [tabContent, tabLayout, tab, idx]() {
            tabContent->setCurrentIndex(idx);
            for (int i = 0; i < tabLayout->count(); i++) {
                QPushButton *t = qobject_cast<QPushButton*>(tabLayout->itemAt(i)->widget());
                if (t) t->setStyleSheet(t == tab
                                         ? "QPushButton { color: #ffffff; font-size: 12px; padding: 10px 16px; border-bottom: 2px solid #9098b0; background: transparent; }"
                                         : "QPushButton { color: #9098b0; font-size: 12px; padding: 10px 16px; border-bottom: 2px solid transparent; background: transparent; }"
                                           "QPushButton:hover { color: #9098b0; }");
            }
        });
        tabLayout->addWidget(tab);
        return tab;
    };

    makeTab("Kill Apps", 0);
    makeTab("Kill Services", 1);
    makeTab("Display", 2);
    makeTab("Launch Files", 3);
    makeTab("Match Detect", 4);
    QPushButton *saveBtn = new QPushButton("Save");
    saveBtn->setFlat(true);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 16px; }"
                           "QPushButton:hover { color: #ffffff; }");
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveProfiles);
    tabLayout->addStretch();
    tabLayout->addWidget(saveBtn);

    // Kill Apps tab
    QWidget *killAppsTab = new QWidget();
    killAppsTab->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *killAppsLayout = new QVBoxLayout(killAppsTab);
    killAppsLayout->setContentsMargins(16, 16, 16, 16);
    QLabel *killAppsTitle = new QLabel("WHEN APP LAUNCHES, KILL THESE");
    killAppsTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-bottom: 8px;");
    killAppsLayout->addWidget(killAppsTitle);
    QWidget *killAppsTable = new QWidget();
    killAppsTable->setObjectName("killAppsTable");
    killAppsTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *killAppsTableLayout = new QVBoxLayout(killAppsTable);
    killAppsTableLayout->setContentsMargins(0, 0, 0, 0);
    killAppsTableLayout->setSpacing(0);
    QPushButton *addKillAppBtn = new QPushButton("+ Add app to kill");
    addKillAppBtn->setObjectName("addKillAppBtn");
    addKillAppBtn->setFlat(true);
    addKillAppBtn->setCursor(Qt::PointingHandCursor);
    addKillAppBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; }"
                                 "QPushButton:hover { color: #9098b0; }");
    killAppsTableLayout->addWidget(addKillAppBtn);
    killAppsLayout->addWidget(killAppsTable);
    killAppsLayout->addStretch();
    tabContent->addWidget(killAppsTab);
    connect(addKillAppBtn, &QPushButton::clicked, this, [this, killAppsTableLayout, addKillAppBtn]() {
        QString path = QFileDialog::getOpenFileName(this, "Select App to Kill", "C:/", "All Files (*);;Executables (*.exe)");
        if (path.isEmpty()) return;
        QString name = QFileInfo(path).baseName();
        QString exe = QFileInfo(path).fileName();

        QWidget *row = new QWidget();
        row->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 14, 10);

        QLabel *nameLabel = new QLabel(name);
        nameLabel->setStyleSheet("color: #ffffff; font-size: 12px; ");
        QLabel *exeLabel = new QLabel(exe);
        exeLabel->setStyleSheet("color: #9098b0; font-size: 12px;");

        QPushButton *treeToggle = new QPushButton("Kill Tree");
        treeToggle->setFlat(true);
        treeToggle->setCursor(Qt::PointingHandCursor);
        treeToggle->setProperty("toggled", false);
        treeToggle->setStyleSheet("QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }"
                                  "QPushButton:hover { color: #9098b0; }");
        connect(treeToggle, &QPushButton::clicked, [treeToggle]() {
            bool on = !treeToggle->property("toggled").toBool();
            treeToggle->setProperty("toggled", on);
            treeToggle->setStyleSheet(on
                                          ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                                          : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }"
                                            "QPushButton:hover { color: #9098b0; }");
        });

        QPushButton *deleteBtn = new QPushButton("✕");
        deleteBtn->setFlat(true);
        deleteBtn->setCursor(Qt::PointingHandCursor);
        deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                 "QPushButton:hover { color: #ff4444; }");
        connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });

        rowLayout->addWidget(nameLabel, 2);
        rowLayout->addWidget(exeLabel, 2);
        rowLayout->addWidget(treeToggle);
        rowLayout->addSpacing(8);
        rowLayout->addWidget(deleteBtn);

        int idx = killAppsTableLayout->indexOf(addKillAppBtn);
        killAppsTableLayout->insertWidget(idx, row);
        saveProfiles();
    });

    // Kill Services tab
    QWidget *killServicesTab = new QWidget();
    killServicesTab->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *killServicesLayout = new QVBoxLayout(killServicesTab);
    killServicesLayout->setContentsMargins(16, 16, 16, 16);
    QLabel *killServicesTitle = new QLabel("SERVICES TO STOP ON LAUNCH");
    killServicesTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-bottom: 8px;");
    killServicesLayout->addWidget(killServicesTitle);
    QWidget *killServicesTable = new QWidget();
    killServicesTable->setObjectName("killServicesTable");
    killServicesTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *killServicesTableLayout = new QVBoxLayout(killServicesTable);
    killServicesTableLayout->setContentsMargins(0, 0, 0, 0);
    killServicesTableLayout->setSpacing(0);
    QPushButton *addServiceBtn = new QPushButton("+ Add service");
    addServiceBtn->setObjectName("addServiceBtn");
    addServiceBtn->setFlat(true);
    addServiceBtn->setCursor(Qt::PointingHandCursor);
    addServiceBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; }"
                                 "QPushButton:hover { color: #9098b0; }");
    QPushButton *bulkAddServiceBtn = new QPushButton("Bulk Add");
    bulkAddServiceBtn->setFlat(true);
    bulkAddServiceBtn->setCursor(Qt::PointingHandCursor);
    bulkAddServiceBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; border-left: 1px solid #1e2130; }"
                                     "QPushButton:hover { color: #ffffff; }");
    QWidget *servicesBtnRow = new QWidget();
    servicesBtnRow->setObjectName("servicesFooter");
    QHBoxLayout *servicesBtnLayout = new QHBoxLayout(servicesBtnRow);
    servicesBtnLayout->setContentsMargins(0, 0, 0, 0);
    servicesBtnLayout->setSpacing(0);
    QPushButton *recommendedServiceBtn = new QPushButton("Recommended");
    recommendedServiceBtn->setFlat(true);
    recommendedServiceBtn->setCursor(Qt::PointingHandCursor);
    recommendedServiceBtn->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; border-left: 1px solid #1e2130; }"
        "QPushButton:hover { color: #ffffff; }");
    servicesBtnLayout->addWidget(addServiceBtn);
    servicesBtnLayout->addWidget(bulkAddServiceBtn);
    servicesBtnLayout->addWidget(recommendedServiceBtn);
    servicesBtnLayout->addStretch();
    killServicesTableLayout->addWidget(servicesBtnRow);
    killServicesLayout->addWidget(killServicesTable);
    killServicesLayout->addStretch();
    tabContent->addWidget(killServicesTab);
    connect(addServiceBtn, &QPushButton::clicked, this, [this, killServicesTableLayout, servicesBtnRow]() {
        QDialog *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Add Service");
        dialog->setStyleSheet("background: #13161f; color: #ffffff;");
        QVBoxLayout *dl = new QVBoxLayout(dialog);
        dl->setSpacing(10);
        QLineEdit *nameEdit = new QLineEdit();
        nameEdit->setPlaceholderText("Service name (e.g. Windows Update)");
        QLineEdit *idEdit = new QLineEdit();
        idEdit->setPlaceholderText("Service ID (e.g. wuauserv)");
        QString inputStyle = "QLineEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; color: #ffffff; }";
        nameEdit->setStyleSheet(inputStyle);
        idEdit->setStyleSheet(inputStyle);
        QPushButton *confirmBtn = new QPushButton("Add");
        confirmBtn->setStyleSheet("QPushButton { background: #181c28; color: #ffffff; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; }"
                                  "QPushButton:hover { background: #252a3a; }");
        connect(confirmBtn, &QPushButton::clicked, this, [this, dialog, nameEdit, idEdit, killServicesTableLayout, servicesBtnRow]() {
            if (nameEdit->text().isEmpty() || idEdit->text().isEmpty()) return;

            QWidget *row = new QWidget();
            row->setStyleSheet("border-top: 1px solid #1e2130;");
            QHBoxLayout *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(14, 10, 14, 10);

            QLabel *nameLabel = new QLabel(nameEdit->text());
            nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
            QLabel *idLabel = new QLabel(idEdit->text());
            idLabel->setStyleSheet("color: #9098b0; font-size: 12px;");

            QPushButton *activeToggle = new QPushButton("OFF");
            activeToggle->setFlat(true);
            activeToggle->setCursor(Qt::PointingHandCursor);
            activeToggle->setProperty("toggled", false);
            activeToggle->setStyleSheet("QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
            connect(activeToggle, &QPushButton::clicked, [activeToggle]() {
                bool on = !activeToggle->property("toggled").toBool();
                activeToggle->setProperty("toggled", on);
                activeToggle->setText(on ? "ON" : "OFF");
                activeToggle->setStyleSheet(on
                    ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                    : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
            });

            QPushButton *deleteBtn = new QPushButton("✕");
            deleteBtn->setFlat(true);
            deleteBtn->setCursor(Qt::PointingHandCursor);
            deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                     "QPushButton:hover { color: #ff4444; }");
            connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });

            rowLayout->addWidget(nameLabel, 2);
            rowLayout->addWidget(idLabel, 2);
            rowLayout->addWidget(activeToggle);
            rowLayout->addSpacing(8);
            rowLayout->addWidget(deleteBtn);

            int idx = killServicesTableLayout->indexOf(servicesBtnRow);
            killServicesTableLayout->insertWidget(idx, row);
            saveProfiles();
            dialog->accept();
        });
        dl->addWidget(nameEdit);
        dl->addWidget(idEdit);
        dl->addWidget(confirmBtn);
        dialog->exec();
    });
    connect(bulkAddServiceBtn, &QPushButton::clicked, this, [this, killServicesTableLayout, servicesBtnRow]() {
        QDialog *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Bulk Add Services");
        dialog->setMinimumWidth(400);
        dialog->setStyleSheet("background: #13161f; color: #ffffff;");
        QVBoxLayout *dl = new QVBoxLayout(dialog);
        dl->setSpacing(10);
        QLabel *hint = new QLabel("One per line:  Name:ServiceID");
        hint->setStyleSheet("color: #9098b0; font-size: 11px;");
        QPlainTextEdit *textArea = new QPlainTextEdit();
        textArea->setPlaceholderText("Windows Update:wuauserv\nWindows Defender:WinDefend\nSysMain:SysMain");
        textArea->setStyleSheet("QPlainTextEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; color: #ffffff; font-size: 12px; }");
        textArea->setMinimumHeight(160);
        QPushButton *confirmBtn = new QPushButton("Add");
        confirmBtn->setStyleSheet("QPushButton { background: #181c28; color: #ffffff; border: 1px solid #1e2130; border-radius: 6px; padding: 8px; }"
                                  "QPushButton:hover { background: #252a3a; }");
        connect(confirmBtn, &QPushButton::clicked, this, [this, dialog, textArea, killServicesTableLayout, servicesBtnRow]() {
            QStringList lines = textArea->toPlainText().split('\n', Qt::SkipEmptyParts);
            int insertIdx = killServicesTableLayout->indexOf(servicesBtnRow);
            int added = 0;
            for (const QString &line : std::as_const(lines)) {
                int sep = line.trimmed().indexOf(':');
                if (sep <= 0 || sep == line.trimmed().length() - 1) continue;
                QString name = line.trimmed().left(sep).trimmed();
                QString serviceId = line.trimmed().mid(sep + 1).trimmed();
                if (name.isEmpty() || serviceId.isEmpty()) continue;

                QWidget *row = new QWidget();
                row->setStyleSheet("border-top: 1px solid #1e2130;");
                QHBoxLayout *rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(14, 10, 14, 10);

                QLabel *nameLabel = new QLabel(name);
                nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
                QLabel *idLabel = new QLabel(serviceId);
                idLabel->setStyleSheet("color: #9098b0; font-size: 12px;");

                QPushButton *activeToggle = new QPushButton("OFF");
                activeToggle->setFlat(true);
                activeToggle->setCursor(Qt::PointingHandCursor);
                activeToggle->setProperty("toggled", false);
                activeToggle->setStyleSheet("QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                connect(activeToggle, &QPushButton::clicked, [activeToggle]() {
                    bool on = !activeToggle->property("toggled").toBool();
                    activeToggle->setProperty("toggled", on);
                    activeToggle->setText(on ? "ON" : "OFF");
                    activeToggle->setStyleSheet(on
                        ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                        : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                });

                QPushButton *deleteBtn = new QPushButton("✕");
                deleteBtn->setFlat(true);
                deleteBtn->setCursor(Qt::PointingHandCursor);
                deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                         "QPushButton:hover { color: #ff4444; }");
                connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });

                rowLayout->addWidget(nameLabel, 2);
                rowLayout->addWidget(idLabel, 2);
                rowLayout->addWidget(activeToggle);
                rowLayout->addSpacing(8);
                rowLayout->addWidget(deleteBtn);

                killServicesTableLayout->insertWidget(insertIdx + added, row);
                added++;
            }
            if (added > 0) saveProfiles();
            dialog->accept();
        });
        dl->addWidget(hint);
        dl->addWidget(textArea);
        dl->addWidget(confirmBtn);
        dialog->exec();
    });

    // Recommended services
    connect(recommendedServiceBtn, &QPushButton::clicked, this,
            [this, killServicesTableLayout, servicesBtnRow]() {
        struct Rec { const char *name; const char *id; };
        static const Rec recommended[] = {
            { "Windows Update",            "wuauserv"        },
            { "Windows Defender",          "WinDefend"       },
            { "Windows Search",            "WSearch"         },
            { "SysMain",                   "SysMain"         },
            { "Xbox Live Auth",            "XblAuthManager"  },
            { "Xbox Live Game Save",       "XblGameSave"     },
            { "Xbox Live Networking",      "XboxNetApiSvc"   },
            { "Delivery Optimization",     "DoSvc"           },
            { "Connected User Experiences","DiagTrack"       },
        };

        // Collect already-present service IDs to avoid duplicates
        QSet<QString> existing;
        for (int i = 0; i < killServicesTableLayout->count(); ++i) {
            QWidget *w = killServicesTableLayout->itemAt(i)->widget();
            if (!w || w == servicesBtnRow) continue;
            const auto labels = w->findChildren<QLabel*>();
            if (labels.size() >= 2) existing.insert(labels[1]->text());
        }

        int insertIdx = killServicesTableLayout->indexOf(servicesBtnRow);
        int added = 0;
        for (const auto &rec : recommended) {
            if (existing.contains(rec.id)) continue;

            QWidget *row = new QWidget();
            row->setStyleSheet("border-top: 1px solid #1e2130;");
            QHBoxLayout *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(14, 10, 14, 10);

            QLabel *nameLabel = new QLabel(rec.name);
            nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
            QLabel *idLabel = new QLabel(rec.id);
            idLabel->setStyleSheet("color: #9098b0; font-size: 12px;");

            QPushButton *activeToggle = new QPushButton("OFF");
            activeToggle->setFlat(true);
            activeToggle->setCursor(Qt::PointingHandCursor);
            activeToggle->setProperty("toggled", false);
            activeToggle->setStyleSheet(
                "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px;"
                "border: 1px solid #1e2130; border-radius: 4px; }");
            connect(activeToggle, &QPushButton::clicked, [activeToggle]() {
                bool on = !activeToggle->property("toggled").toBool();
                activeToggle->setProperty("toggled", on);
                activeToggle->setText(on ? "ON" : "OFF");
                activeToggle->setStyleSheet(on
                    ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                    : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
            });

            QPushButton *deleteBtn = new QPushButton("✕");
            deleteBtn->setFlat(true);
            deleteBtn->setCursor(Qt::PointingHandCursor);
            deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                     "QPushButton:hover { color: #ff4444; }");
            connect(deleteBtn, &QPushButton::clicked, [this, row]() {
                row->deleteLater(); saveProfiles();
            });

            rowLayout->addWidget(nameLabel, 2);
            rowLayout->addWidget(idLabel, 2);
            rowLayout->addWidget(activeToggle);
            rowLayout->addSpacing(8);
            rowLayout->addWidget(deleteBtn);

            killServicesTableLayout->insertWidget(insertIdx + added, row);
            added++;
        }
        if (added > 0) saveProfiles();
    });

    // Display tab
    QWidget *displayTab = new QWidget();
    displayTab->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *displayLayout = new QVBoxLayout(displayTab);
    displayLayout->setContentsMargins(16, 16, 16, 16);
    QLabel *displayTitle = new QLabel("DISPLAY SETTINGS");
    displayTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-bottom: 8px;");
    displayLayout->addWidget(displayTitle);
    QWidget *displayTable = new QWidget();
    displayTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *displayTableLayout = new QVBoxLayout(displayTable);
    displayTableLayout->setContentsMargins(0, 0, 0, 0);
    displayTableLayout->setSpacing(0);

    auto makeDisplayToggle = [](QVBoxLayout *layout, const QString &label) {
        QWidget *row = new QWidget();
        row->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 14, 10);
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("color: #ffffff; font-size: 12px;");
        QPushButton *toggle = new QPushButton("OFF");
        toggle->setFlat(true);
        toggle->setCursor(Qt::PointingHandCursor);
        toggle->setProperty("toggled", false);
        toggle->setStyleSheet("QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        QObject::connect(toggle, &QPushButton::clicked, [toggle]() {
            bool on = !toggle->property("toggled").toBool();
            toggle->setProperty("toggled", on);
            toggle->setText(on ? "ON" : "OFF");
            toggle->setStyleSheet(on
                                      ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                                      : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        });
        rowLayout->addWidget(lbl);
        rowLayout->addStretch();
        rowLayout->addWidget(toggle);
        layout->addWidget(row);
    };

    makeDisplayToggle(displayTableLayout, "Switch to single monitor on launch");
    makeDisplayToggle(displayTableLayout, "Restore extended display on close");

    // ── Resolution change on launch ──
    {
        const QStringList resWidths  = {"1280", "1920", "2560", "3840", "Custom..."};
        const QStringList resHeights = {"720",  "1080", "1440", "2160", "Custom..."};
        const QString comboStyle =
            "QComboBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
            "color: #ffffff; font-size: 12px; padding: 3px 8px; }"
            "QComboBox::drop-down { border: none; width: 16px; }"
            "QComboBox QAbstractItemView { background: #13161f; border: 1px solid #1e2130;"
            "color: #ffffff; selection-background-color: #181c28; }";

        QWidget *resLaunchRow = new QWidget();
        resLaunchRow->setObjectName("resLaunchRow");
        resLaunchRow->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *resLaunchLayout = new QHBoxLayout(resLaunchRow);
        resLaunchLayout->setContentsMargins(14, 10, 14, 10);
        resLaunchLayout->setSpacing(8);

        QLabel *resLaunchLbl = new QLabel("Change resolution on launch");
        resLaunchLbl->setStyleSheet("color: #ffffff; font-size: 12px;");

        QWidget *resCombosWidget = new QWidget();
        resCombosWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *resCombosLayout = new QHBoxLayout(resCombosWidget);
        resCombosLayout->setContentsMargins(0, 0, 0, 0);
        resCombosLayout->setSpacing(4);

        QComboBox *resWidthCombo = new QComboBox();
        resWidthCombo->setObjectName("resWidthCombo");
        resWidthCombo->addItems(resWidths);
        resWidthCombo->setCurrentIndex(1); // 1920 default
        resWidthCombo->setFixedWidth(70);
        resWidthCombo->setStyleSheet(comboStyle);

        QLabel *resX = new QLabel("\xc3\x97");
        resX->setStyleSheet("color: #9098b0; font-size: 12px;");

        QComboBox *resHeightCombo = new QComboBox();
        resHeightCombo->setObjectName("resHeightCombo");
        resHeightCombo->addItems(resHeights);
        resHeightCombo->setCurrentIndex(1); // 1080 default
        resHeightCombo->setFixedWidth(60);
        resHeightCombo->setStyleSheet(comboStyle);

        // Custom resolution inputs (shown only when "Custom..." is selected in respective combo)
        QLineEdit *customWidthEdit = new QLineEdit();
        customWidthEdit->setObjectName("customResWidthEdit");
        customWidthEdit->setPlaceholderText("W");
        customWidthEdit->setFixedWidth(55);
        customWidthEdit->setVisible(false);
        customWidthEdit->setAlignment(Qt::AlignCenter);
        customWidthEdit->setStyleSheet(
            "QLineEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
            " color: #ffffff; font-size: 12px; padding: 2px 4px; }");

        QLineEdit *customHeightEdit = new QLineEdit();
        customHeightEdit->setObjectName("customResHeightEdit");
        customHeightEdit->setPlaceholderText("H");
        customHeightEdit->setFixedWidth(50);
        customHeightEdit->setVisible(false);
        customHeightEdit->setAlignment(Qt::AlignCenter);
        customHeightEdit->setStyleSheet(
            "QLineEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
            " color: #ffffff; font-size: 12px; padding: 2px 4px; }");

        QObject::connect(resWidthCombo, &QComboBox::currentTextChanged,
                         [customWidthEdit](const QString &t) {
            customWidthEdit->setVisible(t == "Custom...");
        });
        QObject::connect(resHeightCombo, &QComboBox::currentTextChanged,
                         [customHeightEdit](const QString &t) {
            customHeightEdit->setVisible(t == "Custom...");
        });

        resCombosLayout->addWidget(resWidthCombo);
        resCombosLayout->addWidget(customWidthEdit);
        resCombosLayout->addWidget(resX);
        resCombosLayout->addWidget(resHeightCombo);
        resCombosLayout->addWidget(customHeightEdit);
        resCombosWidget->setVisible(false); // hidden until toggle is ON

        QPushButton *resLaunchToggle = new QPushButton("OFF");
        resLaunchToggle->setObjectName("resLaunchToggle");
        resLaunchToggle->setFlat(true);
        resLaunchToggle->setCursor(Qt::PointingHandCursor);
        resLaunchToggle->setProperty("toggled", false);
        resLaunchToggle->setStyleSheet(
            "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        QObject::connect(resLaunchToggle, &QPushButton::clicked, [resLaunchToggle, resCombosWidget]() {
            bool on = !resLaunchToggle->property("toggled").toBool();
            resLaunchToggle->setProperty("toggled", on);
            resLaunchToggle->setText(on ? "ON" : "OFF");
            resLaunchToggle->setStyleSheet(on
                ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
            resCombosWidget->setVisible(on);
        });

        resLaunchLayout->addWidget(resLaunchLbl);
        resLaunchLayout->addStretch();
        resLaunchLayout->addWidget(resCombosWidget);
        resLaunchLayout->addWidget(resLaunchToggle);
        displayTableLayout->addWidget(resLaunchRow);
    }

    // ── Resolution restore on close ──
    {
        QWidget *resRestoreRow = new QWidget();
        resRestoreRow->setObjectName("resRestoreRow");
        resRestoreRow->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *resRestoreLayout = new QHBoxLayout(resRestoreRow);
        resRestoreLayout->setContentsMargins(14, 10, 14, 10);
        QLabel *resRestoreLbl = new QLabel("Restore resolution on close");
        resRestoreLbl->setStyleSheet("color: #ffffff; font-size: 12px;");
        QPushButton *resRestoreToggle = new QPushButton("OFF");
        resRestoreToggle->setObjectName("resRestoreToggle");
        resRestoreToggle->setFlat(true);
        resRestoreToggle->setCursor(Qt::PointingHandCursor);
        resRestoreToggle->setProperty("toggled", false);
        resRestoreToggle->setStyleSheet(
            "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        QObject::connect(resRestoreToggle, &QPushButton::clicked, [resRestoreToggle]() {
            bool on = !resRestoreToggle->property("toggled").toBool();
            resRestoreToggle->setProperty("toggled", on);
            resRestoreToggle->setText(on ? "ON" : "OFF");
            resRestoreToggle->setStyleSheet(on
                ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        });
        resRestoreLayout->addWidget(resRestoreLbl);
        resRestoreLayout->addStretch();
        resRestoreLayout->addWidget(resRestoreToggle);
        displayTableLayout->addWidget(resRestoreRow);
    }

    // ── Highest refresh rate on launch ──
    {
        QWidget *highFpsRow = new QWidget();
        highFpsRow->setStyleSheet("border-top: 1px solid #1e2130;");
        QHBoxLayout *highFpsLayout = new QHBoxLayout(highFpsRow);
        highFpsLayout->setContentsMargins(14, 10, 14, 10);
        QLabel *highFpsLbl = new QLabel("Set highest refresh rate on launch");
        highFpsLbl->setStyleSheet("color: #ffffff; font-size: 12px;");
        QPushButton *highFpsToggle = new QPushButton("OFF");
        highFpsToggle->setObjectName("highFpsToggle");
        highFpsToggle->setFlat(true);
        highFpsToggle->setCursor(Qt::PointingHandCursor);
        highFpsToggle->setProperty("toggled", false);
        highFpsToggle->setStyleSheet(
            "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px;"
            " border: 1px solid #1e2130; border-radius: 4px; }");
        QObject::connect(highFpsToggle, &QPushButton::clicked, [highFpsToggle]() {
            bool on = !highFpsToggle->property("toggled").toBool();
            highFpsToggle->setProperty("toggled", on);
            highFpsToggle->setText(on ? "ON" : "OFF");
            highFpsToggle->setStyleSheet(on
                ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
        });
        highFpsLayout->addWidget(highFpsLbl);
        highFpsLayout->addStretch();
        highFpsLayout->addWidget(highFpsToggle);
        displayTableLayout->addWidget(highFpsRow);
    }

    displayLayout->addWidget(displayTable);
    displayLayout->addStretch();
    tabContent->addWidget(displayTab);

    // ── Launch Files tab ──
    QWidget *launchFilesTab = new QWidget();
    launchFilesTab->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *launchFilesLayout = new QVBoxLayout(launchFilesTab);
    launchFilesLayout->setContentsMargins(16, 16, 16, 16);
    QLabel *launchFilesTitle = new QLabel("LAUNCH WHEN APP STARTS");
    launchFilesTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-bottom: 8px;");
    launchFilesLayout->addWidget(launchFilesTitle);
    QWidget *launchFilesTable = new QWidget();
    launchFilesTable->setObjectName("launchFilesTable");
    launchFilesTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *launchFilesTableLayout = new QVBoxLayout(launchFilesTable);
    launchFilesTableLayout->setContentsMargins(0, 0, 0, 0);
    launchFilesTableLayout->setSpacing(0);
    QPushButton *addLaunchFileBtn = new QPushButton("+ Add file");
    addLaunchFileBtn->setObjectName("addLaunchFileBtn");
    addLaunchFileBtn->setFlat(true);
    addLaunchFileBtn->setCursor(Qt::PointingHandCursor);
    addLaunchFileBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; }"
                                    "QPushButton:hover { color: #ffffff; }");
    launchFilesTableLayout->addWidget(addLaunchFileBtn);
    launchFilesLayout->addWidget(launchFilesTable);

    // ── Run when app stops ──
    QLabel *stopFilesTitle = new QLabel("RUN WHEN APP STOPS");
    stopFilesTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-top: 14px; margin-bottom: 8px;");
    launchFilesLayout->addWidget(stopFilesTitle);
    QWidget *stopFilesTable = new QWidget();
    stopFilesTable->setObjectName("stopFilesTable");
    stopFilesTable->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QVBoxLayout *stopFilesTableLayout = new QVBoxLayout(stopFilesTable);
    stopFilesTableLayout->setContentsMargins(0, 0, 0, 0);
    stopFilesTableLayout->setSpacing(0);
    QPushButton *addStopFileBtn = new QPushButton("+ Add file");
    addStopFileBtn->setObjectName("addStopFileBtn");
    addStopFileBtn->setFlat(true);
    addStopFileBtn->setCursor(Qt::PointingHandCursor);
    addStopFileBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left; }"
                                  "QPushButton:hover { color: #ffffff; }");
    stopFilesTableLayout->addWidget(addStopFileBtn);
    launchFilesLayout->addWidget(stopFilesTable);
    launchFilesLayout->addStretch();
    tabContent->addWidget(launchFilesTab);  // index 3

    // ── Match Detect tab (index 4) ──────────────────────────────────────────
    QWidget *matchDetectTab = new QWidget();
    matchDetectTab->setObjectName("matchDetectTab");
    matchDetectTab->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *matchLayout = new QVBoxLayout(matchDetectTab);
    matchLayout->setContentsMargins(16, 16, 16, 16);
    matchLayout->setSpacing(12);

    QLabel *matchTitle = new QLabel("MATCH FOUND DETECTION");
    matchTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    matchLayout->addWidget(matchTitle);

    // Enable row
    QWidget *matchEnableRow = new QWidget();
    matchEnableRow->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    QHBoxLayout *matchEnableLayout = new QHBoxLayout(matchEnableRow);
    matchEnableLayout->setContentsMargins(14, 10, 14, 10);
    QLabel *matchEnableLbl = new QLabel("Detect when this app's window flashes");
    matchEnableLbl->setStyleSheet("color: #ffffff; font-size: 12px;");
    QLabel *matchEnableDesc = new QLabel("(e.g. Overwatch match found while alt-tabbed)");
    matchEnableDesc->setStyleSheet("color: #9098b0; font-size: 11px;");
    QPushButton *matchDetectToggle = new QPushButton("OFF");
    matchDetectToggle->setObjectName("matchDetectToggle");
    matchDetectToggle->setFlat(true);
    matchDetectToggle->setCursor(Qt::PointingHandCursor);
    matchDetectToggle->setProperty("toggled", false);
    matchDetectToggle->setStyleSheet(
        "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 10px;"
        " border: 1px solid #1e2130; border-radius: 4px; }");
    QObject::connect(matchDetectToggle, &QPushButton::clicked, [matchDetectToggle]() {
        bool on = !matchDetectToggle->property("toggled").toBool();
        matchDetectToggle->setProperty("toggled", on);
        matchDetectToggle->setText(on ? "ON" : "OFF");
        matchDetectToggle->setStyleSheet(on
            ? "QPushButton { color: #50C878; font-size: 11px; padding: 3px 10px;"
              " border: 1px solid #50C878; border-radius: 4px; }"
            : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 10px;"
              " border: 1px solid #1e2130; border-radius: 4px; }");
    });
    QVBoxLayout *matchEnableTxt = new QVBoxLayout();
    matchEnableTxt->setSpacing(2);
    matchEnableTxt->addWidget(matchEnableLbl);
    matchEnableTxt->addWidget(matchEnableDesc);
    matchEnableLayout->addLayout(matchEnableTxt);
    matchEnableLayout->addStretch();
    matchEnableLayout->addWidget(matchDetectToggle);
    matchLayout->addWidget(matchEnableRow);

    const QString spinStyle =
        "QSpinBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
        " color: #ffffff; font-size: 12px; padding: 2px 6px; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 14px; }";

    // ── Reorderable sequence steps ────────────────────────────────────────
    QLabel *seqTitle = new QLabel("SEQUENCE");
    seqTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-top: 4px;");
    matchLayout->addWidget(seqTitle);

    QWidget *stepsContainer = new QWidget();
    stepsContainer->setObjectName("stepsContainer");
    // Solid background prevents backing-colour bleed-through when rows are reordered
    stepsContainer->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *stepsLayout = new QVBoxLayout(stepsContainer);
    stepsLayout->setContentsMargins(0, 0, 0, 0);
    stepsLayout->setSpacing(4);

    // Refresh enabled-state of ▲▼ buttons so first row has no ▲ and last has no ▼
    auto refreshStepBtns = [](QVBoxLayout *lay) {
        int n = lay->count();
        for (int i = 0; i < n; ++i) {
            QLayoutItem *it = lay->itemAt(i);
            QWidget *row = it ? it->widget() : nullptr;
            if (!row) continue;
            QList<QPushButton*> btns = row->findChildren<QPushButton*>();
            if (btns.size() >= 2) {
                btns[0]->setEnabled(i > 0);
                btns[1]->setEnabled(i < n - 1);
            }
        }
    };

    // Build one step row: ▲ ▼  label  [N s]
    struct StepDef { QString label; QString action; int defaultSec; };
    auto makeStepRow = [&](const StepDef &def) -> QWidget * {
        QWidget *row = new QWidget();
        row->setObjectName("stepRow");
        row->setProperty("stepAction", def.action);
        row->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");

        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 8, 14, 8);
        rl->setSpacing(6);

        const QString arrowStyle =
            "QPushButton { color: #6b7494; font-size: 9px; background: transparent; border: none; }"
            "QPushButton:hover:enabled { color: #ffffff; }"
            "QPushButton:disabled { color: #252a3a; }";

        QPushButton *upBtn = new QPushButton("▲");
        upBtn->setFixedSize(20, 20); upBtn->setFlat(true);
        upBtn->setCursor(Qt::PointingHandCursor);
        upBtn->setStyleSheet(arrowStyle);

        QPushButton *downBtn = new QPushButton("▼");
        downBtn->setFixedSize(20, 20); downBtn->setFlat(true);
        downBtn->setCursor(Qt::PointingHandCursor);
        downBtn->setStyleSheet(arrowStyle);

        QLabel *lbl = new QLabel(def.label);
        lbl->setStyleSheet("color: #ffffff; font-size: 12px;");

        QSpinBox *sb = new QSpinBox();
        sb->setObjectName("stepDelay");
        sb->setRange(0, 120); sb->setSingleStep(1);
        sb->setValue(def.defaultSec); sb->setSuffix(" s");
        sb->setFixedWidth(90);
        sb->setStyleSheet(spinStyle);

        rl->addWidget(upBtn);
        rl->addWidget(downBtn);
        rl->addSpacing(6);
        rl->addWidget(lbl, 1);
        rl->addWidget(sb);

        QObject::connect(upBtn, &QPushButton::clicked, [stepsLayout, row, refreshStepBtns]() {
            int idx = stepsLayout->indexOf(row);
            if (idx > 0) {
                stepsLayout->removeWidget(row);
                stepsLayout->insertWidget(idx - 1, row);
                refreshStepBtns(stepsLayout);
            }
        });
        QObject::connect(downBtn, &QPushButton::clicked, [stepsLayout, row, refreshStepBtns]() {
            int idx = stepsLayout->indexOf(row);
            if (idx >= 0 && idx < stepsLayout->count() - 1) {
                stepsLayout->removeWidget(row);
                stepsLayout->insertWidget(idx + 1, row);
                refreshStepBtns(stepsLayout);
            }
        });
        return row;
    };

    const QList<StepDef> defaultSteps = {
        {"Pause browser", "pauseBrowser", 0},
        {"Play Spotify",  "playSpotify",  0},
        {"Focus window",  "focusWindow",  0},
    };
    for (const auto &def : defaultSteps)
        stepsLayout->addWidget(makeStepRow(def));
    refreshStepBtns(stepsLayout);

    matchLayout->addWidget(stepsContainer);

    // ── Presets ───────────────────────────────────────────────────────────
    QLabel *presetTitle = new QLabel("PRESETS");
    presetTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-top: 8px;");
    matchLayout->addWidget(presetTitle);

    struct PresetDef { QString name; int browser; int spotify; int window; };
    const QList<PresetDef> presets = {
        {"Instantly",      2,  0, 0},
        {"Hero Selection", 15, 0, 0},
        {"Ban Selection",  20, 0, 0},
        {"Match",          60, 0, 0},
    };

    QWidget *presetsRow = new QWidget();
    presetsRow->setStyleSheet("background: #0c0e15;");
    QHBoxLayout *presetsHLayout = new QHBoxLayout(presetsRow);
    presetsHLayout->setContentsMargins(0, 0, 0, 0);
    presetsHLayout->setSpacing(6);

    for (const auto &p : presets) {
        QPushButton *btn = new QPushButton(p.name);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: #181c28; color: #c8d0e8; border: 1px solid #1e2130;"
            " border-radius: 6px; padding: 5px 10px; font-size: 11px; }"
            "QPushButton:hover { background: #1e2130; color: #ffffff; border-color: #7b8cde; }");

        int bSec = p.browser, sSec = p.spotify, wSec = p.window;
        QObject::connect(btn, &QPushButton::clicked, [stepsLayout, bSec, sSec, wSec]() {
            auto setDelay = [stepsLayout](const QString &action, int sec) {
                for (int i = 0; i < stepsLayout->count(); ++i) {
                    QLayoutItem *it = stepsLayout->itemAt(i);
                    QWidget *row = it ? it->widget() : nullptr;
                    if (row && row->property("stepAction").toString() == action)
                        if (auto *sb = row->findChild<QSpinBox*>("stepDelay"))
                            sb->setValue(sec);
                }
            };
            setDelay("pauseBrowser", bSec);
            setDelay("playSpotify",  sSec);
            setDelay("focusWindow",  wSec);
        });
        presetsHLayout->addWidget(btn);
    }
    presetsHLayout->addStretch();
    matchLayout->addWidget(presetsRow);

    // ── Cooldown ──────────────────────────────────────────────────────────
    QLabel *cooldownTitle = new QLabel("COOLDOWN");
    cooldownTitle->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px; margin-top: 8px;");
    matchLayout->addWidget(cooldownTitle);

    auto makeSimpleRow = [&](const QString &label, const char *objName, int defaultSec) -> QWidget * {
        QWidget *row = new QWidget();
        row->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 10, 14, 10);
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("color: #ffffff; font-size: 12px;");
        QSpinBox *spin = new QSpinBox();
        spin->setObjectName(objName);
        spin->setRange(0, 300); spin->setSingleStep(1);
        spin->setValue(defaultSec); spin->setSuffix(" s");
        spin->setFixedWidth(90);
        spin->setStyleSheet(spinStyle);
        rl->addWidget(lbl); rl->addStretch(); rl->addWidget(spin);
        return row;
    };
    matchLayout->addWidget(makeSimpleRow("After trigger, ignore flashes for", "matchCooldown", 60));
    matchLayout->addStretch();

    tabContent->addWidget(matchDetectTab);  // index 4

    // Helper: build a file row for either table
    auto makeFileRow = [this](QVBoxLayout *tableLayout, QPushButton *addBtn, const QString &filePath) {
        if (filePath.isEmpty()) return;
        QWidget *row = new QWidget();
        row->setStyleSheet("border-top: 1px solid #1e2130;");
        row->setProperty("filePath", filePath);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 14, 10);
        QLabel *nameLabel = new QLabel(QFileInfo(filePath).fileName());
        nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
        QLabel *pathLabel = new QLabel(filePath);
        pathLabel->setStyleSheet("color: #9098b0; font-size: 11px;");
        pathLabel->setMaximumWidth(240);
        QPushButton *deleteBtn = new QPushButton("\u2715");
        deleteBtn->setFlat(true);
        deleteBtn->setCursor(Qt::PointingHandCursor);
        deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                 "QPushButton:hover { color: #ff4444; }");
        connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });
        rowLayout->addWidget(nameLabel, 2);
        rowLayout->addWidget(pathLabel, 3);
        rowLayout->addWidget(deleteBtn);
        tableLayout->insertWidget(tableLayout->indexOf(addBtn), row);
    };

    auto addLaunchFileRow = [this, launchFilesTableLayout, addLaunchFileBtn, makeFileRow](const QString &fp) {
        makeFileRow(launchFilesTableLayout, addLaunchFileBtn, fp);
    };
    auto addStopFileRow = [this, stopFilesTableLayout, addStopFileBtn, makeFileRow](const QString &fp) {
        makeFileRow(stopFilesTableLayout, addStopFileBtn, fp);
    };

    const QString fileFilter = "Launchable Files (*.exe *.lnk *.reg *.url *.ps1);;All Files (*)";
    connect(addLaunchFileBtn, &QPushButton::clicked, this, [this, addLaunchFileRow, fileFilter]() {
        QString path = QFileDialog::getOpenFileName(this, "Select File to Launch on Start", "C:/", fileFilter);
        if (path.isEmpty()) return;
        addLaunchFileRow(path);
        saveProfiles();
    });
    connect(addStopFileBtn, &QPushButton::clicked, this, [this, addStopFileRow, fileFilter]() {
        QString path = QFileDialog::getOpenFileName(this, "Select File to Run on Stop", "C:/", fileFilter);
        if (path.isEmpty()) return;
        addStopFileRow(path);
        saveProfiles();
    });

    detailLayout->addWidget(tabBar);
    detailLayout->addWidget(tabContent);
    appProfileDetail->addWidget(detailWidget);

    connect(appBtn, &QPushButton::clicked, this, [this, appBtn]() {
        int idx = appListEntriesLayout->indexOf(appBtn);
        if (idx >= 0) appProfileDetail->setCurrentIndex(idx + 1);
    });

    appListEntriesLayout->addWidget(appBtn);
    addAppWatcher(name, QFileInfo(exePath).fileName(), appBtn);
}

void MainWindow::addAppWatcher(const QString &appName, const QString &exeName, QPushButton *profileBtn)
{
    if (exeName.isEmpty()) return;

    auto card             = std::make_shared<QPointer<QWidget>>(nullptr);
    auto stoppedServices  = std::make_shared<QStringList>();
    auto savedResolution  = std::make_shared<QSize>();
    auto savedRefreshRate = std::make_shared<int>(0);

    registerWatcher(exeName, [this, appName, exeName, profileBtn, card, stoppedServices, savedResolution, savedRefreshRate](bool running) {
        if (running) {
            stoppedServices->clear();

            int idx = appListEntriesLayout->indexOf(profileBtn);
            if (idx >= 0 && idx + 1 < appProfileDetail->count()) {
                QWidget *detail = appProfileDetail->widget(idx + 1);
                QStackedWidget *tabContent = detail ? detail->findChild<QStackedWidget*>() : nullptr;
                if (tabContent) {
                    // Kill apps (tab 0)
                    QWidget *killAppsTab = tabContent->widget(0);
                    if (killAppsTab) {
                        QWidget *table = killAppsTab->findChild<QWidget*>("killAppsTable");
                        if (table) {
                            for (QWidget *row : table->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                                if (qobject_cast<QPushButton*>(row)) continue;
                                QList<QLabel*> labels = row->findChildren<QLabel*>();
                                if (labels.size() < 2) continue;
                                QString exe = labels[1]->text();
                                bool killTree = false;
                                for (QPushButton *b : row->findChildren<QPushButton*>())
                                    if (b->property("toggled").isValid()) { killTree = b->property("toggled").toBool(); break; }
                                if (killTree) Commands::killProcessTree(exe);
                                else          Commands::killProcess(exe);
                            }
                        }
                    }

                    // Stop services (tab 1)
                    QWidget *servicesTab = tabContent->widget(1);
                    if (servicesTab) {
                        QWidget *table = servicesTab->findChild<QWidget*>("killServicesTable");
                        if (table) {
                            for (QWidget *row : table->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                                if (qobject_cast<QPushButton*>(row)) continue;
                                QList<QLabel*> labels = row->findChildren<QLabel*>();
                                if (labels.size() < 2) continue;
                                QString serviceId = labels[1]->text();
                                bool active = false;
                                for (QPushButton *b : row->findChildren<QPushButton*>())
                                    if (b->property("toggled").isValid()) { active = b->property("toggled").toBool(); break; }
                                if (active) {
                                    Commands::stopService(serviceId);
                                    stoppedServices->append(serviceId);
                                }
                            }
                        }
                    }

                    // Display (tab 2)
                    QWidget *displayTab = tabContent->widget(2);
                    if (displayTab) {
                        QList<QPushButton*> toggles = displayTab->findChildren<QPushButton*>();
                        if (toggles.size() > 0 && toggles[0]->property("toggled").toBool())
                            Commands::switchToSingleMonitor();
                        if (toggles.size() > 2 && toggles[2]->property("toggled").toBool()) {
                            QScreen *screen = QGuiApplication::primaryScreen();
                            if (screen) *savedResolution = screen->size();
                            auto *wCombo    = displayTab->findChild<QComboBox*>("resWidthCombo");
                            auto *hCombo    = displayTab->findChild<QComboBox*>("resHeightCombo");
                            auto *customW   = displayTab->findChild<QLineEdit*>("customResWidthEdit");
                            auto *customH   = displayTab->findChild<QLineEdit*>("customResHeightEdit");
                            int rw = (wCombo && wCombo->currentText() == "Custom..." && customW)
                                     ? customW->text().toInt() : (wCombo ? wCombo->currentText().toInt() : 1920);
                            int rh = (hCombo && hCombo->currentText() == "Custom..." && customH)
                                     ? customH->text().toInt() : (hCombo ? hCombo->currentText().toInt() : 1080);
                            if (rw <= 0) rw = 1920;
                            if (rh <= 0) rh = 1080;
                            Commands::changeResolution(rw, rh);
                        }
                        // Highest refresh rate
                        auto *highFpsBtn = displayTab->findChild<QPushButton*>("highFpsToggle");
                        if (highFpsBtn && highFpsBtn->property("toggled").toBool()) {
                            QScreen *screen = QGuiApplication::primaryScreen();
                            if (screen) *savedRefreshRate = static_cast<int>(screen->refreshRate());
                            Commands::setHighestRefreshRate();
                        }
                    }

                    // Launch files (tab 3)
                    QWidget *launchFilesTabW = tabContent->widget(3);
                    if (launchFilesTabW) {
                        QWidget *table = launchFilesTabW->findChild<QWidget*>("launchFilesTable");
                        if (table) {
                            for (QWidget *row : table->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                                if (qobject_cast<QPushButton*>(row)) continue;
                                QString fp = row->property("filePath").toString();
                                if (!fp.isEmpty()) Commands::launchApp(fp);
                            }
                        }
                    }
                }
            }

            // Create task card
            QWidget *taskWidget = new QWidget();
            *card = taskWidget;
            taskWidget->setStyleSheet("background: #13161f; border: 1px solid #1e2130; border-radius: 8px;");
            QHBoxLayout *taskLayout = new QHBoxLayout(taskWidget);
            taskLayout->setContentsMargins(14, 10, 14, 10);
            QLabel *nameLabel = new QLabel(appName);
            nameLabel->setStyleSheet("color: #ffffff; font-size: 13px;");
            QLabel *runningLabel = new QLabel("\u25cf Running");
            runningLabel->setStyleSheet("color: #50C878; font-size: 12px;");
            QPushButton *stopBtn = new QPushButton("Stop");
            stopBtn->setFlat(true);
            stopBtn->setCursor(Qt::PointingHandCursor);
            stopBtn->setStyleSheet("QPushButton { color: #ff4444; font-size: 12px; padding: 4px 8px;"
                                   "border: 1px solid #ff4444; border-radius: 4px; }"
                                   "QPushButton:hover { background: #2a1010; }");
            // Stop button kills the process; central watcher detects the stop and fires callback(false)
            connect(stopBtn, &QPushButton::clicked, taskWidget, [this, exeName, card]() {
                Commands::killProcess(exeName);
                if (*card) { (*card)->deleteLater(); *card = nullptr; }
                QTimer::singleShot(0, this, &MainWindow::updateStatus);
            });
            taskLayout->addWidget(nameLabel);
            taskLayout->addStretch();
            taskLayout->addWidget(runningLabel);
            taskLayout->addSpacing(8);
            taskLayout->addWidget(stopBtn);
            if (tasksContentLayout->count() > 1) {
                QLayoutItem *first = tasksContentLayout->itemAt(0);
                if (first && first->widget()) {
                    if (qobject_cast<QLabel*>(first->widget())) delete first->widget();
                }
            }
            tasksContentLayout->insertWidget(0, taskWidget);
            updateStatus();

        } else {
            // Restart any services that were stopped
            for (const QString &svc : std::as_const(*stoppedServices))
                Commands::startService(svc);
            stoppedServices->clear();

            // Restore display
            int idx = appListEntriesLayout->indexOf(profileBtn);
            if (idx >= 0 && idx + 1 < appProfileDetail->count()) {
                QWidget *detail = appProfileDetail->widget(idx + 1);
                QStackedWidget *tabContent = detail ? detail->findChild<QStackedWidget*>() : nullptr;
                if (tabContent) {
                    QWidget *displayTab = tabContent->widget(2);
                    if (displayTab) {
                        QList<QPushButton*> toggles = displayTab->findChildren<QPushButton*>();
                        if (toggles.size() > 1 && toggles[1]->property("toggled").toBool())
                            Commands::restoreExtendedDisplay();
                        if (toggles.size() > 3 && toggles[3]->property("toggled").toBool()
                                && savedResolution->isValid())
                            Commands::changeResolution(savedResolution->width(), savedResolution->height());
                        // Restore refresh rate
                        auto *highFpsBtn = displayTab->findChild<QPushButton*>("highFpsToggle");
                        if (highFpsBtn && highFpsBtn->property("toggled").toBool() && *savedRefreshRate > 0)
                            Commands::setRefreshRate(*savedRefreshRate);
                    }

                    // Stop files (tab 3 — run on close)
                    QWidget *launchTabW2 = tabContent->widget(3);
                    if (launchTabW2) {
                        QWidget *stopTable = launchTabW2->findChild<QWidget*>("stopFilesTable");
                        if (stopTable) {
                            for (QWidget *row : stopTable->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                                if (qobject_cast<QPushButton*>(row)) continue;
                                QString fp = row->property("filePath").toString();
                                if (!fp.isEmpty()) Commands::launchApp(fp);
                            }
                        }
                    }
                }
            }

            if (*card) {
                (*card)->deleteLater();
                *card = nullptr;
                QTimer::singleShot(0, this, &MainWindow::updateStatus);
            }
        }
    });
}

void MainWindow::saveProfiles()
{
    QJsonArray profiles;
    for (int i = 0; i < appListEntriesLayout->count(); i++) {
        QPushButton *btn = qobject_cast<QPushButton*>(appListEntriesLayout->itemAt(i)->widget());
        if (!btn) continue;

        QJsonObject profile;
        profile["name"] = btn->text();
        profile["exePath"] = btn->property("exePath").toString();

        // get the detail widget for this profile
        if (i + 1 >= appProfileDetail->count()) { profiles.append(profile); continue; }
        QWidget *detail = appProfileDetail->widget(i + 1);
        QStackedWidget *tabContent = detail->findChild<QStackedWidget*>();
        if (!tabContent) { profiles.append(profile); continue; }

        // kill apps tab (index 0)
        QWidget *killAppsTab = tabContent->widget(0);
        QJsonArray killApps;
        if (killAppsTab) {
            QWidget *killAppsTable = killAppsTab->findChild<QWidget*>("killAppsTable");
            if (killAppsTable) {
                QList<QWidget*> rows = killAppsTable->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                for (QWidget *row : std::as_const(rows)) {
                    if (qobject_cast<QPushButton*>(row)) continue; // skip addKillAppBtn
                    QList<QLabel*> labels = row->findChildren<QLabel*>();
                    if (labels.size() < 2) continue;
                    QJsonObject killApp;
                    killApp["name"] = labels[0]->text();
                    killApp["exe"] = labels[1]->text();
                    bool kt = false;
                    for (QPushButton *b : row->findChildren<QPushButton*>()) {
                        if (b->property("toggled").isValid()) { kt = b->property("toggled").toBool(); break; }
                    }
                    killApp["killTree"] = kt;
                    killApps.append(killApp);
                }
            }
        }
        profile["killApps"] = killApps;

        // services tab (index 1)
        QWidget *killServicesTab = tabContent->widget(1);
        QJsonArray services;
        if (killServicesTab) {
            QWidget *killServicesTable = killServicesTab->findChild<QWidget*>("killServicesTable");
            if (killServicesTable) {
                QList<QWidget*> rows = killServicesTable->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                for (QWidget *row : std::as_const(rows)) {
                    if (qobject_cast<QPushButton*>(row)) continue; // skip addServiceBtn
                    QList<QLabel*> labels = row->findChildren<QLabel*>();
                    if (labels.size() < 2) continue;
                    QJsonObject svc;
                    svc["name"] = labels[0]->text();
                    svc["serviceId"] = labels[1]->text();
                    bool active = false;
                    for (QPushButton *b : row->findChildren<QPushButton*>()) {
                        if (b->property("toggled").isValid()) { active = b->property("toggled").toBool(); break; }
                    }
                    svc["active"] = active;
                    services.append(svc);
                }
            }
        }
        profile["services"] = services;

        // display tab (index 2)
        QWidget *displayTab = tabContent->widget(2);
        QJsonArray displayToggles;
        if (displayTab) {
            QList<QPushButton*> toggles = displayTab->findChildren<QPushButton*>();
            for (QPushButton *t : std::as_const(toggles))
                displayToggles.append(t->property("toggled").toBool());
            // Save resolution combo selections (including custom values)
            auto *wCombo   = displayTab->findChild<QComboBox*>("resWidthCombo");
            auto *hCombo   = displayTab->findChild<QComboBox*>("resHeightCombo");
            auto *customW  = displayTab->findChild<QLineEdit*>("customResWidthEdit");
            auto *customH  = displayTab->findChild<QLineEdit*>("customResHeightEdit");
            profile["displayResW"]       = wCombo  ? wCombo->currentText()  : "1920";
            profile["displayResH"]       = hCombo  ? hCombo->currentText()  : "1080";
            profile["displayCustomResW"] = customW ? customW->text()        : "";
            profile["displayCustomResH"] = customH ? customH->text()        : "";
        }
        profile["display"] = displayToggles;

        // launch files tab (index 3)
        QWidget *launchFilesTabW = tabContent->widget(3);
        QJsonArray launchFiles;
        if (launchFilesTabW) {
            QWidget *launchTable = launchFilesTabW->findChild<QWidget*>("launchFilesTable");
            if (launchTable) {
                for (QWidget *row : launchTable->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (qobject_cast<QPushButton*>(row)) continue;
                    QString fp = row->property("filePath").toString();
                    if (!fp.isEmpty()) launchFiles.append(fp);
                }
            }
        }
        profile["launchFiles"] = launchFiles;

        QJsonArray stopFiles;
        QWidget *stopTableW = launchFilesTabW->findChild<QWidget*>("stopFilesTable");
        if (stopTableW) {
            for (QWidget *row : stopTableW->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                if (qobject_cast<QPushButton*>(row)) continue;
                QString fp = row->property("filePath").toString();
                if (!fp.isEmpty()) stopFiles.append(fp);
            }
        }
        profile["stopFiles"] = stopFiles;

        // match detect tab (index 4)
        QWidget *matchDetectTabW = tabContent->count() > 4 ? tabContent->widget(4) : nullptr;
        if (matchDetectTabW) {
            QPushButton *mdToggle = matchDetectTabW->findChild<QPushButton *>("matchDetectToggle");
            profile["matchDetectEnabled"] = mdToggle ? mdToggle->property("toggled").toBool() : false;

            // Save ordered step sequence
            QJsonArray matchSteps;
            QWidget *sc = matchDetectTabW->findChild<QWidget*>("stepsContainer");
            if (sc) {
                auto *scl = qobject_cast<QVBoxLayout*>(sc->layout());
                if (scl) {
                    for (int j = 0; j < scl->count(); ++j) {
                        QWidget *row = scl->itemAt(j) ? scl->itemAt(j)->widget() : nullptr;
                        if (!row) continue;
                        QJsonObject step;
                        step["action"] = row->property("stepAction").toString();
                        auto *sb = row->findChild<QSpinBox*>("stepDelay");
                        step["delay"] = sb ? sb->value() : 0;
                        matchSteps.append(step);
                    }
                }
            }
            profile["matchSteps"] = matchSteps;

            // Cooldown (separate spinbox)
            auto *coolSb = matchDetectTabW->findChild<QSpinBox*>("matchCooldown");
            profile["matchCooldown"] = coolSb ? coolSb->value() : 60;
        }

        profiles.append(profile);
    }
    QJsonObject root;
    root["profiles"] = profiles;
    QFile f(profilesFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

void MainWindow::loadProfiles()
{
    QFile f(profilesFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) return;
    QJsonArray profiles = doc["profiles"].toArray();
    for (const QJsonValue &v : std::as_const(profiles)) {
        QJsonObject p = v.toObject();
        addAppProfileEntry(p["name"].toString(), p["exePath"].toString());

        int idx = appListEntriesLayout->count() - 1;
        if (idx + 1 >= appProfileDetail->count()) continue;
        QWidget *detail = appProfileDetail->widget(idx + 1);
        QStackedWidget *tabContent = detail->findChild<QStackedWidget*>();
        if (!tabContent) continue;

        // restore kill apps
        QWidget *killAppsTab = tabContent->widget(0);
        if (killAppsTab) {
            QWidget *killAppsTable = killAppsTab->findChild<QWidget*>("killAppsTable");
            QVBoxLayout *killAppsTableLayout = killAppsTable ? qobject_cast<QVBoxLayout*>(killAppsTable->layout()) : nullptr;
            QPushButton *addKillAppBtn = killAppsTable ? killAppsTable->findChild<QPushButton*>("addKillAppBtn") : nullptr;
            QJsonArray killApps = p["killApps"].toArray();
            for (const QJsonValue &ka : std::as_const(killApps)) {
                if (!killAppsTableLayout || !addKillAppBtn) break;
                QJsonObject killApp = ka.toObject();
                QWidget *row = new QWidget();
                row->setStyleSheet("border-top: 1px solid #1e2130;");
                QHBoxLayout *rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(14, 10, 14, 10);
                QLabel *nameLabel = new QLabel(killApp["name"].toString());
                nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
                QLabel *exeLabel = new QLabel(killApp["exe"].toString());
                exeLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
                bool killTree = killApp["killTree"].toBool();
                QPushButton *treeToggle = new QPushButton("Kill Tree");
                treeToggle->setFlat(true);
                treeToggle->setCursor(Qt::PointingHandCursor);
                treeToggle->setProperty("toggled", killTree);
                treeToggle->setStyleSheet(killTree
                                              ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                                              : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                connect(treeToggle, &QPushButton::clicked, [treeToggle]() {
                    bool on = !treeToggle->property("toggled").toBool();
                    treeToggle->setProperty("toggled", on);
                    treeToggle->setStyleSheet(on
                                                  ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                                                  : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                });
                QPushButton *deleteBtn = new QPushButton("\u2715");
                deleteBtn->setFlat(true);
                deleteBtn->setCursor(Qt::PointingHandCursor);
                deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                         "QPushButton:hover { color: #ff4444; }");
                connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });
                rowLayout->addWidget(nameLabel, 2);
                rowLayout->addWidget(exeLabel, 2);
                rowLayout->addWidget(treeToggle);
                rowLayout->addSpacing(8);
                rowLayout->addWidget(deleteBtn);
                int insertIdx = killAppsTableLayout->indexOf(addKillAppBtn);
                killAppsTableLayout->insertWidget(insertIdx, row);
            }
        }

        // restore services
        QWidget *killServicesTab = tabContent->widget(1);
        if (killServicesTab) {
            QWidget *killServicesTable = killServicesTab->findChild<QWidget*>("killServicesTable");
            QVBoxLayout *killServicesTableLayout = killServicesTable ? qobject_cast<QVBoxLayout*>(killServicesTable->layout()) : nullptr;
            QWidget *servicesFooter = killServicesTable ? killServicesTable->findChild<QWidget*>("servicesFooter") : nullptr;
            QJsonArray services = p["services"].toArray();
            for (const QJsonValue &sv : std::as_const(services)) {
                if (!killServicesTableLayout || !servicesFooter) break;
                QJsonObject svc = sv.toObject();
                QWidget *row = new QWidget();
                row->setStyleSheet("border-top: 1px solid #1e2130;");
                QHBoxLayout *rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(14, 10, 14, 10);
                QLabel *nameLabel = new QLabel(svc["name"].toString());
                nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
                QLabel *idLabel = new QLabel(svc["serviceId"].toString());
                idLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
                bool active = svc["active"].toBool();
                QPushButton *activeToggle = new QPushButton(active ? "ON" : "OFF");
                activeToggle->setFlat(true);
                activeToggle->setCursor(Qt::PointingHandCursor);
                activeToggle->setProperty("toggled", active);
                activeToggle->setStyleSheet(active
                    ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                    : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                connect(activeToggle, &QPushButton::clicked, [activeToggle]() {
                    bool on = !activeToggle->property("toggled").toBool();
                    activeToggle->setProperty("toggled", on);
                    activeToggle->setText(on ? "ON" : "OFF");
                    activeToggle->setStyleSheet(on
                        ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                        : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
                });
                QPushButton *deleteBtn = new QPushButton("\u2715");
                deleteBtn->setFlat(true);
                deleteBtn->setCursor(Qt::PointingHandCursor);
                deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                         "QPushButton:hover { color: #ff4444; }");
                connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });
                rowLayout->addWidget(nameLabel, 2);
                rowLayout->addWidget(idLabel, 2);
                rowLayout->addWidget(activeToggle);
                rowLayout->addSpacing(8);
                rowLayout->addWidget(deleteBtn);
                int insertIdx = killServicesTableLayout->indexOf(servicesFooter);
                killServicesTableLayout->insertWidget(insertIdx, row);
            }
        }

        // restore display toggles
        QWidget *displayTab = tabContent->widget(2);
        if (displayTab) {
            QList<QPushButton*> toggles = displayTab->findChildren<QPushButton*>();
            QJsonArray displayToggles = p["display"].toArray();
            for (int t = 0; t < toggles.size() && t < displayToggles.size(); t++) {
                bool on = displayToggles[t].toBool();
                toggles[t]->setProperty("toggled", on);
                toggles[t]->setText(on ? "ON" : "OFF");
                toggles[t]->setStyleSheet(on
                    ? "QPushButton { color: #3a7a4a; font-size: 11px; padding: 3px 8px; border: 1px solid #3a7a4a; border-radius: 4px; }"
                    : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px; border: 1px solid #1e2130; border-radius: 4px; }");
            }
            // Restore resolution combo selections (including custom values)
            auto *wCombo  = displayTab->findChild<QComboBox*>("resWidthCombo");
            auto *hCombo  = displayTab->findChild<QComboBox*>("resHeightCombo");
            auto *customW = displayTab->findChild<QLineEdit*>("customResWidthEdit");
            auto *customH = displayTab->findChild<QLineEdit*>("customResHeightEdit");
            if (wCombo) {
                int wi = wCombo->findText(p["displayResW"].toString("1920"));
                if (wi >= 0) wCombo->setCurrentIndex(wi);
            }
            if (hCombo) {
                int hi = hCombo->findText(p["displayResH"].toString("1080"));
                if (hi >= 0) hCombo->setCurrentIndex(hi);
            }
            if (customW) {
                customW->setText(p["displayCustomResW"].toString());
                customW->setVisible(wCombo && wCombo->currentText() == "Custom...");
            }
            if (customH) {
                customH->setText(p["displayCustomResH"].toString());
                customH->setVisible(hCombo && hCombo->currentText() == "Custom...");
            }
            // Restore launch files (tab 3)
            QWidget *launchFilesTabW = tabContent->widget(3);
            if (launchFilesTabW) {
                QWidget *launchTable = launchFilesTabW->findChild<QWidget*>("launchFilesTable");
                QVBoxLayout *launchTableLayout = launchTable ? qobject_cast<QVBoxLayout*>(launchTable->layout()) : nullptr;
                QPushButton *addBtn = launchTable ? launchTable->findChild<QPushButton*>("addLaunchFileBtn") : nullptr;
                for (const QJsonValue &lf : p["launchFiles"].toArray()) {
                    if (!launchTableLayout || !addBtn) break;
                    QString fp = lf.toString();
                    if (fp.isEmpty()) continue;
                    QWidget *row = new QWidget();
                    row->setStyleSheet("border-top: 1px solid #1e2130;");
                    row->setProperty("filePath", fp);
                    QHBoxLayout *rowLayout = new QHBoxLayout(row);
                    rowLayout->setContentsMargins(14, 10, 14, 10);
                    QLabel *nameLabel = new QLabel(QFileInfo(fp).fileName());
                    nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
                    QLabel *pathLabel = new QLabel(fp);
                    pathLabel->setStyleSheet("color: #9098b0; font-size: 11px;");
                    pathLabel->setMaximumWidth(240);
                    QPushButton *deleteBtn = new QPushButton("\u2715");
                    deleteBtn->setFlat(true);
                    deleteBtn->setCursor(Qt::PointingHandCursor);
                    deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                             "QPushButton:hover { color: #ff4444; }");
                    connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });
                    rowLayout->addWidget(nameLabel, 2);
                    rowLayout->addWidget(pathLabel, 3);
                    rowLayout->addWidget(deleteBtn);
                    launchTableLayout->insertWidget(launchTableLayout->indexOf(addBtn), row);
                }
                // Restore stop files
                QWidget *stopTableW2 = launchFilesTabW->findChild<QWidget*>("stopFilesTable");
                QVBoxLayout *stopTableLayout = stopTableW2 ? qobject_cast<QVBoxLayout*>(stopTableW2->layout()) : nullptr;
                QPushButton *stopAddBtn = stopTableW2 ? stopTableW2->findChild<QPushButton*>("addStopFileBtn") : nullptr;
                for (const QJsonValue &sf : p["stopFiles"].toArray()) {
                    if (!stopTableLayout || !stopAddBtn) break;
                    QString fp = sf.toString();
                    if (fp.isEmpty()) continue;
                    QWidget *row = new QWidget();
                    row->setStyleSheet("border-top: 1px solid #1e2130;");
                    row->setProperty("filePath", fp);
                    QHBoxLayout *rowLayout = new QHBoxLayout(row);
                    rowLayout->setContentsMargins(14, 10, 14, 10);
                    QLabel *nameLabel = new QLabel(QFileInfo(fp).fileName());
                    nameLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
                    QLabel *pathLabel = new QLabel(fp);
                    pathLabel->setStyleSheet("color: #9098b0; font-size: 11px;");
                    pathLabel->setMaximumWidth(240);
                    QPushButton *deleteBtn = new QPushButton("\u2715");
                    deleteBtn->setFlat(true);
                    deleteBtn->setCursor(Qt::PointingHandCursor);
                    deleteBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; }"
                                             "QPushButton:hover { color: #ff4444; }");
                    connect(deleteBtn, &QPushButton::clicked, [this, row]() { row->deleteLater(); saveProfiles(); });
                    rowLayout->addWidget(nameLabel, 2);
                    rowLayout->addWidget(pathLabel, 3);
                    rowLayout->addWidget(deleteBtn);
                    stopTableLayout->insertWidget(stopTableLayout->indexOf(stopAddBtn), row);
                }
            }

            // Show combos if the change-res toggle (index 2) was restored ON
            if (toggles.size() > 2 && toggles[2]->property("toggled").toBool()) {
                auto *resCombosWidget = displayTab->findChild<QWidget*>("resLaunchRow");
                if (resCombosWidget) {
                    // find the combos container inside the row and show it
                    const auto children = resCombosWidget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                    for (QWidget *child : children) {
                        if (child->findChild<QComboBox*>("resWidthCombo"))
                            child->setVisible(true);
                    }
                }
            }
        }

        // restore match detect tab (index 4)
        QWidget *matchDetectTabW = tabContent->count() > 4 ? tabContent->widget(4) : nullptr;
        if (matchDetectTabW && p.contains("matchDetectEnabled")) {
            QPushButton *mdToggle = matchDetectTabW->findChild<QPushButton *>("matchDetectToggle");
            if (mdToggle) {
                bool on = p["matchDetectEnabled"].toBool();
                mdToggle->setProperty("toggled", on);
                mdToggle->setText(on ? "ON" : "OFF");
                mdToggle->setStyleSheet(on
                    ? "QPushButton { color: #50C878; font-size: 11px; padding: 3px 10px;"
                      " border: 1px solid #50C878; border-radius: 4px; }"
                    : "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 10px;"
                      " border: 1px solid #1e2130; border-radius: 4px; }");
            }
            // Restore ordered step sequence
            QWidget *sc = matchDetectTabW->findChild<QWidget*>("stepsContainer");
            if (sc && p.contains("matchSteps")) {
                auto *scl = qobject_cast<QVBoxLayout*>(sc->layout());
                QJsonArray steps = p["matchSteps"].toArray();
                if (scl && !steps.isEmpty()) {
                    // Index existing rows by action
                    QMap<QString, QWidget*> byAction;
                    for (int j = 0; j < scl->count(); ++j) {
                        QWidget *row = scl->itemAt(j) ? scl->itemAt(j)->widget() : nullptr;
                        if (row) byAction[row->property("stepAction").toString()] = row;
                    }
                    // Reorder + set delays
                    for (int j = 0; j < steps.size(); ++j) {
                        QJsonObject step = steps[j].toObject();
                        QWidget *row = byAction.value(step["action"].toString());
                        if (!row) continue;
                        if (auto *sb = row->findChild<QSpinBox*>("stepDelay"))
                            sb->setValue(step["delay"].toInt(0));
                        int cur = scl->indexOf(row);
                        if (cur != j) { scl->removeWidget(row); scl->insertWidget(j, row); }
                    }
                    // Refresh ▲▼ button states
                    int n = scl->count();
                    for (int j = 0; j < n; ++j) {
                        QWidget *row = scl->itemAt(j) ? scl->itemAt(j)->widget() : nullptr;
                        if (!row) continue;
                        auto btns = row->findChildren<QPushButton*>();
                        if (btns.size() >= 2) {
                            btns[0]->setEnabled(j > 0);
                            btns[1]->setEnabled(j < n - 1);
                        }
                    }
                }
            }
            // Cooldown
            if (auto *csb = matchDetectTabW->findChild<QSpinBox*>("matchCooldown"))
                csb->setValue(p["matchCooldown"].toInt(60));
        }
    }
}
