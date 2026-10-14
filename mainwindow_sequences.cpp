#include "mainwindow.h"
#include "commands.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <QSizePolicy>
#include <QProcess>
#include <QApplication>
#include <QFont>

void MainWindow::addActionRow(QWidget *container, const QJsonObject &data)
{
    QString type = data["type"].toString("Launch App");

    QWidget *row = new QWidget();
    row->setObjectName("actionRow");
    row->setStyleSheet("QWidget#actionRow { border-top: 1px solid #1e2130; background: transparent; }");
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(10, 8, 10, 8);
    rowLayout->setSpacing(8);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->setObjectName("actionTypeCombo");
    typeCombo->addItems({"Launch App", "Wait", "Dir App Launch"});
    typeCombo->setFixedWidth(120);
    typeCombo->setFont(QFont(QApplication::font().family(), 10));
    typeCombo->setStyleSheet(
        "QComboBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
        "color: #ffffff; font-size: 12px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox QAbstractItemView { background: #13161f; border: 1px solid #1e2130;"
        "color: #ffffff; selection-background-color: #181c28; }");

    QStackedWidget *fieldsStack = new QStackedWidget();
    fieldsStack->setObjectName("actionFieldsStack");

    // --- Page 0: Launch App ---
    QWidget *launchPage = new QWidget();
    launchPage->setStyleSheet("background: transparent;");
    QHBoxLayout *launchLayout = new QHBoxLayout(launchPage);
    launchLayout->setContentsMargins(0, 0, 0, 0);
    launchLayout->setSpacing(6);
    QLineEdit *launchPathEdit = new QLineEdit(data["path"].toString());
    launchPathEdit->setObjectName("launchPathEdit");
    launchPathEdit->setPlaceholderText("Path to .exe");
    launchPathEdit->setStyleSheet("QLineEdit { background: #0c0e15; border: 1px solid #1e2130;"
                                  "border-radius: 4px; color: #ffffff; font-size: 12px; padding: 4px 8px; }");
    QPushButton *browseBtn = new QPushButton("Browse");
    browseBtn->setFlat(true);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 11px; padding: 4px 8px;"
                             "border: 1px solid #1e2130; border-radius: 4px; }"
                             "QPushButton:hover { color: #ffffff; background: #181c28; }");
    connect(browseBtn, &QPushButton::clicked, this, [launchPathEdit]() {
        QString path = QFileDialog::getOpenFileName(nullptr, "Select Application", "",
                                                    "All Files (*);;Executables (*.exe)");
        if (!path.isEmpty()) launchPathEdit->setText(path);
    });
    launchLayout->addWidget(launchPathEdit, 1);
    launchLayout->addWidget(browseBtn);
    fieldsStack->addWidget(launchPage);  // index 0

    // --- Page 1: Wait ---
    QWidget *waitPage = new QWidget();
    waitPage->setStyleSheet("background: transparent;");
    QHBoxLayout *waitLayout = new QHBoxLayout(waitPage);
    waitLayout->setContentsMargins(0, 0, 0, 0);
    waitLayout->setSpacing(8);
    QLabel *waitLabel = new QLabel("Seconds:");
    waitLabel->setStyleSheet("color: #9098b0; font-size: 12px;");
    QSpinBox *waitBox = new QSpinBox();
    waitBox->setObjectName("waitSecondsBox");
    waitBox->setMinimum(1);
    waitBox->setMaximum(3600);
    waitBox->setValue(qMax(1, data["seconds"].toInt()));
    waitBox->setStyleSheet("QSpinBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
                           "color: #ffffff; font-size: 12px; padding: 4px 8px; }"
                           "QSpinBox::up-button, QSpinBox::down-button { width: 16px; background: #13161f; border: none; }");
    waitLayout->addWidget(waitLabel);
    waitLayout->addWidget(waitBox);
    waitLayout->addStretch();
    fieldsStack->addWidget(waitPage);   // index 1

    // --- Page 2: Dir App Launch ---
    // For apps like OBS that must be launched from a specific working directory.
    QWidget *dirPage = new QWidget();
    dirPage->setStyleSheet("background: transparent;");
    QVBoxLayout *dirPageLayout = new QVBoxLayout(dirPage);
    dirPageLayout->setContentsMargins(0, 2, 0, 2);
    dirPageLayout->setSpacing(4);

    const QString fieldStyle = "QLineEdit { background: #0c0e15; border: 1px solid #1e2130;"
                               "border-radius: 4px; color: #ffffff; font-size: 12px; padding: 3px 8px; }";
    const QString smallBtnStyle = "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 8px;"
                                  "border: 1px solid #1e2130; border-radius: 4px; }"
                                  "QPushButton:hover { color: #ffffff; background: #181c28; }";

    // Exe row
    QWidget *dirExeRow = new QWidget(); dirExeRow->setStyleSheet("background: transparent;");
    QHBoxLayout *dirExeLayout = new QHBoxLayout(dirExeRow);
    dirExeLayout->setContentsMargins(0,0,0,0); dirExeLayout->setSpacing(6);
    QLabel *dirExeLbl = new QLabel("Exe:"); dirExeLbl->setStyleSheet("color: #9098b0; font-size: 12px;"); dirExeLbl->setFixedWidth(26);
    QLineEdit *dirExeEdit = new QLineEdit(data["exe"].toString());
    dirExeEdit->setObjectName("dirExeEdit");
    dirExeEdit->setPlaceholderText("e.g. obs64.exe");
    dirExeEdit->setStyleSheet(fieldStyle);
    QPushButton *browseExeBtn = new QPushButton("Browse"); browseExeBtn->setFlat(true); browseExeBtn->setCursor(Qt::PointingHandCursor); browseExeBtn->setStyleSheet(smallBtnStyle);
    connect(browseExeBtn, &QPushButton::clicked, this, [dirExeEdit]() {
        QString p = QFileDialog::getOpenFileName(nullptr, "Select Executable", "", "All Files (*);;Executables (*.exe)");
        if (!p.isEmpty()) dirExeEdit->setText(QDir::toNativeSeparators(p));
    });
    dirExeLayout->addWidget(dirExeLbl);
    dirExeLayout->addWidget(dirExeEdit, 1);
    dirExeLayout->addWidget(browseExeBtn);

    // Dir row
    QWidget *dirDirRow = new QWidget(); dirDirRow->setStyleSheet("background: transparent;");
    QHBoxLayout *dirDirLayout = new QHBoxLayout(dirDirRow);
    dirDirLayout->setContentsMargins(0,0,0,0); dirDirLayout->setSpacing(6);
    QLabel *dirDirLbl = new QLabel("Dir:"); dirDirLbl->setStyleSheet("color: #9098b0; font-size: 12px;"); dirDirLbl->setFixedWidth(26);
    QLineEdit *dirDirEdit = new QLineEdit(data["dir"].toString());
    dirDirEdit->setObjectName("dirDirEdit");
    dirDirEdit->setPlaceholderText("Working directory (folder containing the exe)");
    dirDirEdit->setStyleSheet(fieldStyle);
    QPushButton *browseDirBtn = new QPushButton("Browse"); browseDirBtn->setFlat(true); browseDirBtn->setCursor(Qt::PointingHandCursor); browseDirBtn->setStyleSheet(smallBtnStyle);
    connect(browseDirBtn, &QPushButton::clicked, this, [dirDirEdit]() {
        QString d = QFileDialog::getExistingDirectory(nullptr, "Select Working Directory");
        if (!d.isEmpty()) dirDirEdit->setText(QDir::toNativeSeparators(d));
    });
    dirDirLayout->addWidget(dirDirLbl);
    dirDirLayout->addWidget(dirDirEdit, 1);
    dirDirLayout->addWidget(browseDirBtn);

    dirPageLayout->addWidget(dirExeRow);
    dirPageLayout->addWidget(dirDirRow);
    fieldsStack->addWidget(dirPage);  // index 2

    // Make the stack only as tall as the currently-visible page.
    // Non-current pages get QSizePolicy::Ignored so they don't contribute
    // to the stack's sizeHint (otherwise it would always be as tall as the
    // tallest page, i.e. "Dir App Launch" with its two-row layout).
    auto syncStackSize = [fieldsStack]() {
        int cur = fieldsStack->currentIndex();
        for (int i = 0; i < fieldsStack->count(); ++i) {
            QSizePolicy sp = fieldsStack->widget(i)->sizePolicy();
            sp.setVerticalPolicy(i == cur ? QSizePolicy::Preferred : QSizePolicy::Ignored);
            fieldsStack->widget(i)->setSizePolicy(sp);
        }
    };

    // Set initial type
    int typeIdx = (type == "Wait") ? 1 : (type == "Dir App Launch") ? 2 : 0;
    typeCombo->setCurrentIndex(typeIdx);
    fieldsStack->setCurrentIndex(typeIdx);
    syncStackSize();   // apply on first load
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [fieldsStack, syncStackSize](int idx) {
        fieldsStack->setCurrentIndex(idx);
        syncStackSize();
    });

    // Reorder / delete buttons
    const QString btnStyle = "QPushButton { color: #9098b0; font-size: 11px; padding: 3px 6px;"
                             "border: 1px solid #1e2130; border-radius: 4px; }"
                             "QPushButton:hover { color: #ffffff; background: #181c28; }";
    QPushButton *upBtn   = new QPushButton("\u2191"); upBtn->setFlat(true);   upBtn->setFixedWidth(26); upBtn->setCursor(Qt::PointingHandCursor); upBtn->setStyleSheet(btnStyle);
    QPushButton *downBtn = new QPushButton("\u2193"); downBtn->setFlat(true); downBtn->setFixedWidth(26); downBtn->setCursor(Qt::PointingHandCursor); downBtn->setStyleSheet(btnStyle);
    QPushButton *delBtn  = new QPushButton("\u00d7"); delBtn->setFlat(true);  delBtn->setFixedWidth(26); delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 14px; padding: 1px 6px;"
                          "border: 1px solid #1e2130; border-radius: 4px; }"
                          "QPushButton:hover { color: #ff6b6b; background: #1a1520; }");

    connect(upBtn, &QPushButton::clicked, this, [row]() {
        auto *layout = qobject_cast<QVBoxLayout*>(row->parentWidget()->layout());
        if (!layout) return;
        int idx = layout->indexOf(row);
        if (idx > 0) { layout->removeWidget(row); layout->insertWidget(idx - 1, row); }
    });
    connect(downBtn, &QPushButton::clicked, this, [row]() {
        auto *layout = qobject_cast<QVBoxLayout*>(row->parentWidget()->layout());
        if (!layout) return;
        int idx = layout->indexOf(row);
        if (idx >= 0 && idx < layout->count() - 1) { layout->removeWidget(row); layout->insertWidget(idx + 1, row); }
    });
    connect(delBtn, &QPushButton::clicked, this, [row]() { row->deleteLater(); });

    rowLayout->addWidget(typeCombo);
    rowLayout->addWidget(fieldsStack, 1);
    rowLayout->addWidget(upBtn);
    rowLayout->addWidget(downBtn);
    rowLayout->addWidget(delBtn);

    if (auto *cl = qobject_cast<QVBoxLayout*>(container->layout()))
        cl->addWidget(row);
}

void MainWindow::addSequenceEntry(const QString &name, const QString &triggerType,
                                  const QString &triggerExe, const QJsonArray &actions)
{
    int seqIndex = seqDetail->count();

    // ── Left panel button ──
    QPushButton *btn = new QPushButton(name);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet("QPushButton { color: #c8d0e8; font-size: 13px; background: transparent;"
                       "padding: 10px 14px; text-align: left; }"
                       "QPushButton:hover { background: #181c28; }");
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QPushButton::customContextMenuRequested, this, [this, btn](const QPoint &pos) {
        QMenu *m = new QMenu(btn);
        m->setStyleSheet("QMenu { background: #13161f; border: 1px solid #1e2130; border-radius: 6px; padding: 6px 0; }"
                         "QMenu::item { color: #9098b0; font-size: 12px; padding: 5px 16px; }"
                         "QMenu::item:selected { background: #181c28; color: #ffffff; }");
        connect(m->addAction("Remove"), &QAction::triggered, this, [this, btn]() {
            int idx = seqListEntriesLayout->indexOf(btn);
            if (idx < 0) return;
            seqListEntriesLayout->removeWidget(btn);
            btn->deleteLater();
            if (idx + 1 < seqDetail->count()) {
                QWidget *w = seqDetail->widget(idx + 1);
                seqDetail->removeWidget(w);
                w->deleteLater();
            }
            seqDetail->setCurrentIndex(0);
            saveSequences();
        });
        m->popup(btn->mapToGlobal(pos));
    });
    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        int idx = seqListEntriesLayout->indexOf(btn);
        if (idx >= 0) seqDetail->setCurrentIndex(idx + 1);
    });
    seqListEntriesLayout->addWidget(btn);

    // ── Right panel detail ──
    QWidget *detail = new QWidget();
    detail->setStyleSheet("background: #0c0e15;");
    QScrollArea *detailScroll = new QScrollArea();
    detailScroll->setWidgetResizable(true);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setStyleSheet("QScrollArea { border: none; background: #0c0e15; }"
                                "QScrollBar:vertical { width: 0px; }");
    QWidget *detailContent = new QWidget();
    detailContent->setStyleSheet("background: #0c0e15;");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailContent);
    detailLayout->setContentsMargins(24, 24, 24, 24);
    detailLayout->setSpacing(14);

    // Name
    QLineEdit *nameEditW = new QLineEdit(name);
    nameEditW->setObjectName("seqNameEdit");
    nameEditW->setStyleSheet("QLineEdit { background: transparent; border: none; border-bottom: 1px solid #1e2130;"
                             "color: #c8d0e8; font-size: 20px; padding: 4px 0; }"
                             "QLineEdit:focus { border-bottom: 1px solid #7b8cde; }");
    connect(nameEditW, &QLineEdit::editingFinished, this, [this, btn, nameEditW]() {
        QString n = nameEditW->text().trimmed();
        if (!n.isEmpty()) { btn->setText(n); saveSequences(); }
    });
    detailLayout->addWidget(nameEditW);

    // ── TRIGGER ──
    QLabel *trigLbl = new QLabel("TRIGGER");
    trigLbl->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    detailLayout->addWidget(trigLbl);

    QWidget *trigCard = new QWidget();
    trigCard->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    trigCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *trigCardLayout = new QVBoxLayout(trigCard);
    trigCardLayout->setContentsMargins(14, 12, 14, 12);
    trigCardLayout->setSpacing(8);
    trigCardLayout->setSizeConstraint(QLayout::SetMinimumSize);

    QComboBox *trigTypeCombo = new QComboBox();
    trigTypeCombo->setObjectName("triggerTypeCombo");
    trigTypeCombo->addItems({"App Launch", "Manual"});
    trigTypeCombo->setFont(QFont(QApplication::font().family(), 10));
    trigTypeCombo->setStyleSheet(
        "QComboBox { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
        "color: #ffffff; font-size: 12px; padding: 5px 10px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #13161f; border: 1px solid #1e2130;"
        "color: #ffffff; selection-background-color: #181c28; }");

    QStackedWidget *trigFieldsStack = new QStackedWidget();
    trigFieldsStack->setObjectName("triggerFieldsStack");
    trigFieldsStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // App Launch page
    QWidget *appLaunchPage = new QWidget();
    appLaunchPage->setStyleSheet("background: transparent;");
    appLaunchPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QHBoxLayout *appLaunchLayout = new QHBoxLayout(appLaunchPage);
    appLaunchLayout->setContentsMargins(0, 0, 0, 0);
    appLaunchLayout->setSpacing(10);
    QLabel *watchLbl = new QLabel("Watch for exe:");
    watchLbl->setStyleSheet("color: #9098b0; font-size: 12px;");
    QLineEdit *trigExeEdit = new QLineEdit(triggerExe);
    trigExeEdit->setObjectName("triggerExeEdit");
    trigExeEdit->setPlaceholderText("e.g. obs64.exe");
    trigExeEdit->setStyleSheet("QLineEdit { background: #0c0e15; border: 1px solid #1e2130; border-radius: 4px;"
                               "color: #ffffff; font-size: 12px; padding: 5px 10px; }");
    appLaunchLayout->addWidget(watchLbl);
    appLaunchLayout->addWidget(trigExeEdit, 1);
    trigFieldsStack->addWidget(appLaunchPage);

    // Manual page
    QWidget *manualPage = new QWidget();
    manualPage->setStyleSheet("background: transparent;");
    manualPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *manualLayout = new QVBoxLayout(manualPage);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *manualHint = new QLabel("Type the sequence name in chat to run it.");
    manualHint->setStyleSheet("color: #4a5068; font-size: 12px;");
    manualLayout->addWidget(manualHint);
    trigFieldsStack->addWidget(manualPage);

    int trigIdx = (triggerType == "Manual") ? 1 : 0;
    trigTypeCombo->setCurrentIndex(trigIdx);
    trigFieldsStack->setCurrentIndex(trigIdx);
    connect(trigTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            trigFieldsStack, &QStackedWidget::setCurrentIndex);

    trigCardLayout->addWidget(trigTypeCombo);
    trigCardLayout->addWidget(trigFieldsStack);
    detailLayout->addWidget(trigCard);

    // ── ACTIONS ──
    QLabel *actLbl = new QLabel("ACTIONS");
    actLbl->setStyleSheet("color: #9098b0; font-size: 11px; letter-spacing: 2px;");
    detailLayout->addWidget(actLbl);

    QWidget *actionsCard = new QWidget();
    actionsCard->setStyleSheet("background: #13161f; border-radius: 8px; border: 1px solid #1e2130;");
    actionsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *actionsCardLayout = new QVBoxLayout(actionsCard);
    actionsCardLayout->setContentsMargins(0, 0, 0, 0);
    actionsCardLayout->setSpacing(0);
    actionsCardLayout->setSizeConstraint(QLayout::SetMinimumSize);

    QWidget *actionsContainer = new QWidget();
    actionsContainer->setObjectName("actionsContainer");
    actionsContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *actionsContainerLayout = new QVBoxLayout(actionsContainer);
    actionsContainerLayout->setContentsMargins(0, 0, 0, 0);
    actionsContainerLayout->setSpacing(0);
    actionsCardLayout->addWidget(actionsContainer);

    QPushButton *addActionBtn = new QPushButton("+ Add Action");
    addActionBtn->setFlat(true);
    addActionBtn->setCursor(Qt::PointingHandCursor);
    addActionBtn->setStyleSheet("QPushButton { color: #9098b0; font-size: 12px; padding: 10px 14px; text-align: left;"
                                "border-top: 1px solid #1e2130; }"
                                "QPushButton:hover { color: #ffffff; }");
    connect(addActionBtn, &QPushButton::clicked, this, [this, actionsContainer]() {
        addActionRow(actionsContainer, QJsonObject());
    });
    actionsCardLayout->addWidget(addActionBtn);
    detailLayout->addWidget(actionsCard);

    // Save button
    QPushButton *saveBtn = new QPushButton("Save");
    saveBtn->setFlat(true);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet("QPushButton { background: #181c28; color: #ffffff; border: 1px solid #1e2130;"
                           "border-radius: 6px; padding: 8px 24px; font-size: 13px; }"
                           "QPushButton:hover { background: #252a3a; }");
    connect(saveBtn, &QPushButton::clicked, this, [this, btn, nameEditW]() {
        QString n = nameEditW->text().trimmed();
        if (!n.isEmpty()) btn->setText(n);
        saveSequences();
        startSequenceWatcher(btn);
        addMessage("Sequence \"" + btn->text() + "\" saved.", "alice");
    });
    QHBoxLayout *saveBtnRow = new QHBoxLayout();
    saveBtnRow->addStretch();
    saveBtnRow->addWidget(saveBtn);
    detailLayout->addLayout(saveBtnRow);
    detailLayout->addStretch();

    detailScroll->setWidget(detailContent);
    QVBoxLayout *detailWrap = new QVBoxLayout(detail);
    detailWrap->setContentsMargins(0, 0, 0, 0);
    detailWrap->setSpacing(0);
    detailWrap->addWidget(detailScroll);

    seqDetail->addWidget(detail);
    seqDetail->setCurrentIndex(seqIndex);

    // Populate actions from data
    for (const QJsonValue &av : std::as_const(actions))
        addActionRow(actionsContainer, av.toObject());

    // Start watcher if needed
    if (triggerType == "App Launch" && !triggerExe.isEmpty())
        startSequenceWatcher(btn);
}

QJsonArray MainWindow::getSequenceActions(int detailIdx) const
{
    QJsonArray result;
    if (detailIdx <= 0 || detailIdx >= seqDetail->count()) return result;
    QWidget *detail = seqDetail->widget(detailIdx);
    QWidget *container = detail->findChild<QWidget*>("actionsContainer");
    if (!container) return result;

    for (QWidget *row : container->findChildren<QWidget*>("actionRow", Qt::FindDirectChildrenOnly)) {
        QComboBox *typeCombo = row->findChild<QComboBox*>("actionTypeCombo");
        QStackedWidget *fStack = row->findChild<QStackedWidget*>("actionFieldsStack");
        if (!typeCombo || !fStack) continue;

        QJsonObject action;
        QString t = typeCombo->currentText();
        action["type"] = t;
        if (t == "Launch App") {
            auto *e = fStack->widget(0)->findChild<QLineEdit*>("launchPathEdit");
            action["path"] = e ? e->text() : "";
        } else if (t == "Wait") {
            // Page index 1 (was wrongly 3 when Registry Edit / Sound Settings pages existed)
            auto *b = fStack->widget(1)->findChild<QSpinBox*>("waitSecondsBox");
            action["seconds"] = b ? b->value() : 1;
        } else if (t == "Dir App Launch") {
            auto *exe = fStack->widget(2)->findChild<QLineEdit*>("dirExeEdit");
            auto *dir = fStack->widget(2)->findChild<QLineEdit*>("dirDirEdit");
            action["exe"] = exe ? exe->text() : "";
            action["dir"] = dir ? dir->text() : "";
        }
        result.append(action);
    }
    return result;
}

void MainWindow::executeSequenceActions(const QJsonArray &actions, int idx)
{
    if (idx >= actions.size()) return;
    QJsonObject action = actions[idx].toObject();
    QString type = action["type"].toString();

    auto next = [this, actions, idx]() { executeSequenceActions(actions, idx + 1); };

    if (type == "Launch App") {
        QString path = action["path"].toString().trimmed();
        if (path.isEmpty()) {
            addMessage("Sequence error: empty path in Launch App action.", "alice");
        } else {
            // Auto-append .exe if no extension given
            if (QFileInfo(path).suffix().isEmpty())
                path += ".exe";
            Commands::launchApp(path);
        }
        next();
    } else if (type == "Wait") {
        int ms = qMax(1, action["seconds"].toInt()) * 1000;
        QTimer::singleShot(ms, this, next);
    } else if (type == "Dir App Launch") {
        QString exe = action["exe"].toString().trimmed();
        QString dir = action["dir"].toString().trimmed();
        if (exe.isEmpty()) {
            addMessage("Sequence error: no exe set in Dir App Launch action.", "alice");
        } else {
            if (QFileInfo(exe).suffix().isEmpty()) exe += ".exe";
            QString nativeExe = QDir::toNativeSeparators(exe);
            QString nativeDir = QDir::toNativeSeparators(dir);
            // Use cmd /c start "" /D "dir" "exe" — same as the working OBS command
            QStringList args;
            args << "/c" << "start" << "";
            if (!nativeDir.isEmpty()) args << "/D" << nativeDir;
            args << nativeExe;
            QProcess::startDetached("cmd.exe", args);
            logAppEvent("Dir App Launch: " + nativeExe + " (dir: " + nativeDir + ")");
        }
        next();
    } else {
        // Unknown or removed action type — skip gracefully
        next();
    }
}

void MainWindow::saveSequences()
{
    QJsonArray sequences;
    for (int i = 0; i < seqListEntriesLayout->count(); i++) {
        QPushButton *btn = qobject_cast<QPushButton*>(seqListEntriesLayout->itemAt(i)->widget());
        if (!btn || i + 1 >= seqDetail->count()) continue;
        QWidget *detail = seqDetail->widget(i + 1);
        auto *nameEd   = detail->findChild<QLineEdit*>("seqNameEdit");
        auto *tCombo   = detail->findChild<QComboBox*>("triggerTypeCombo");
        auto *exeEd    = detail->findChild<QLineEdit*>("triggerExeEdit");
        QJsonObject seq;
        seq["name"]        = nameEd  ? nameEd->text()          : btn->text();
        seq["triggerType"] = tCombo  ? tCombo->currentText()   : "Manual";
        seq["triggerExe"]  = exeEd   ? exeEd->text()           : "";
        seq["actions"]     = getSequenceActions(i + 1);
        sequences.append(seq);
    }
    QJsonObject root;
    root["sequences"] = sequences;
    QFile f(sequencesFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

void MainWindow::loadSequences()
{
    QFile f(sequencesFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) return;
    const QJsonArray seqArr = doc["sequences"].toArray();
    for (const QJsonValue &v : seqArr) {
        QJsonObject s = v.toObject();
        addSequenceEntry(s["name"].toString("Sequence"),
                         s["triggerType"].toString("Manual"),
                         s["triggerExe"].toString(),
                         s["actions"].toArray());
    }
}

void MainWindow::startSequenceWatcher(QPushButton *btn)
{
    int idx = seqListEntriesLayout->indexOf(btn);
    if (idx < 0 || idx + 1 >= seqDetail->count()) return;
    QWidget *detail = seqDetail->widget(idx + 1);
    auto *tCombo  = detail->findChild<QComboBox*>("triggerTypeCombo");
    auto *exeEdit = detail->findChild<QLineEdit*>("triggerExeEdit");
    if (!tCombo || tCombo->currentIndex() != 0) return;
    if (!exeEdit || exeEdit->text().isEmpty()) return;

    // Invalidate any previous watcher registered for this sequence button.
    // registerWatcher() appends — without this, Save would create a second
    // callback and the sequence would execute (and launch the app) twice.
    if (m_seqWatcherTokens.contains(btn))
        *m_seqWatcherTokens[btn] = false;
    auto active = std::make_shared<bool>(true);
    m_seqWatcherTokens[btn] = active;

    QPointer<QPushButton> btnPtr = btn;
    registerWatcher(exeEdit->text(), [this, btnPtr, active](bool running) {
        if (!*active) return;   // superseded by a later Save — ignore
        if (!btnPtr || !running) return;
        int i = seqListEntriesLayout->indexOf(btnPtr);
        if (i < 0 || i + 1 >= seqDetail->count()) return;
        QWidget *d  = seqDetail->widget(i + 1);
        auto *tc = d ? d->findChild<QComboBox*>("triggerTypeCombo") : nullptr;
        if (!tc || tc->currentIndex() != 0) return;
        executeSequenceActions(getSequenceActions(i + 1), 0);
        logAppEvent("Sequence triggered: " + btnPtr->text());
    });
}
