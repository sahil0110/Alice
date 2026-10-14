#include "mainwindow.h"
#include "aibridge.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextEdit>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QProcess>
#include <QTimer>
#include "commands.h"

QString MainWindow::getActiveApiKey()
{
    for (int i = 0; i < apiEntriesLayout->count(); i++) {
        QWidget *w = apiEntriesLayout->itemAt(i)->widget();
        if (w && w->property("apiActive").toBool())
            return w->property("apiKey").toString();
    }
    return QString();
}

QString MainWindow::getActiveModel()
{
    for (int i = 0; i < apiEntriesLayout->count(); i++) {
        QWidget *w = apiEntriesLayout->itemAt(i)->widget();
        if (w && w->property("apiActive").toBool())
            return w->property("apiModel").toString();
    }
    return QString();
}

void MainWindow::sendToAI(const QString &prompt, bool execute)
{
    // Integrated Web AI — embedded QtWebEngine browser, no external browser needed
    if (integratedWebAiToggleBtn->property("toggled").toBool()) {
        const QString provider = integratedWebAiProviderCombo->currentText();
        addMessage("Thinking...", "alice");
        setStatus("working");
        logAppEvent("Integrated Web AI: sending to " + provider +
                    " (" + QString::number(prompt.length()) + " chars)");
        aiBridge->sendPrompt(prompt, provider);
        return;
    }

    QString key = getActiveApiKey();
    QString model = getActiveModel();
    if (key.isEmpty()) {
        addMessage("No active API key. Go to Settings and set one active.", "alice");
        return;
    }

    logAppEvent("AI prompt: " + prompt);
    addMessage("Thinking...", "alice");
    setStatus("working");

    QString url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-goog-api-key", key.toUtf8());

    QJsonObject part;
    part["text"] = execute
                       ? QString(
                             "You are a Windows command-line expert. "
                             "The user wants to run a task on their Windows PC. "
                             "Reply with ONLY the single Windows command or cmd.exe line that accomplishes the task. "
                             "Rules: no explanation, no markdown, no code fences, no backticks, no comments. "
                             "Output nothing except the raw command so it can be pasted directly into cmd.exe and run. "
                             "Task: ") + prompt
                       : prompt;

    QJsonArray parts;
    parts.append(part);
    QJsonObject content;
    content["parts"] = parts;
    QJsonArray contents;
    contents.append(content);
    QJsonObject body;
    body["contents"] = contents;

    QByteArray bodyData = QJsonDocument(body).toJson();
    QNetworkReply *reply = networkManager->post(request, bodyData);
    connect(reply, &QNetworkReply::finished, this, [this, reply, execute]() {
        reply->deleteLater();

        // Remove "Thinking..." bubble and reset status unconditionally
        bool thinkingRemoved = false;
        for (int i = outputLayout->count() - 2; i >= 0 && !thinkingRemoved; i--) {
            QLayoutItem *item = outputLayout->itemAt(i);
            if (!item || !item->widget()) continue;
            QWidget *w = item->widget();
            const auto editors = w->findChildren<QTextEdit*>();
            for (QTextEdit *ed : editors) {
                if (ed->toPlainText() == "Thinking...") {
                    outputLayout->takeAt(i);
                    delete w;
                    thinkingRemoved = true;
                    break;
                }
            }
        }
        setStatus("ready");

        if (reply->error() != QNetworkReply::NoError) {
            logAppEvent("AI error: " + reply->errorString());
            addMessage("API error: " + reply->errorString(), "alice");
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString response = doc["candidates"][0]["content"]["parts"][0]["text"].toString().trimmed();
        logAppEvent("AI response: " + response);

        if (execute) {
            logAppEvent("Executing command: " + response);
            addMessage("Running: " + response, "alice");
            QProcess *proc = new QProcess(this);
            connect(proc, &QProcess::finished, proc, &QProcess::deleteLater);
            proc->start("cmd.exe", QStringList() << "/c" << response);
            return;
        }

        // Multi-command response: split on "|||" and dispatch each part sequentially
        if (response.contains("|||")) {
            QStringList parts = response.split("|||", Qt::SkipEmptyParts);
            for (int i = 0; i < parts.size(); ++i) {
                QString part = parts[i].trimmed();
                QTimer::singleShot(i * 1000, this, [this, part, i]() {
                    QString cmdResult = Commands::parse(part);
                    if (!cmdResult.isEmpty()) {
                        logAppEvent("AI multi-command [" + QString::number(i) + "]: " + part);
                        addMessage(cmdResult, "alice");
                    } else {
                        logAppEvent("AI multi-message [" + QString::number(i) + "]: " + part);
                        addMessage(part, "alice");
                    }
                });
            }
        } else {
            addMessage(response, "alice");
        }
    });
}

void MainWindow::sendToAIWithCallback(const QString &systemPrompt,
                                      const QString &userPrompt,
                                      std::function<void(const QString &)> callback)
{
    QString key = getActiveApiKey();
    QString model = getActiveModel();
    if (key.isEmpty()) {
        callback(QString());
        return;
    }

    QString url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-goog-api-key", key.toUtf8());

    // Build request: system instruction + user message
    QJsonObject sysPart;
    sysPart["text"] = systemPrompt;
    QJsonArray sysParts;
    sysParts.append(sysPart);
    QJsonObject sysContent;
    sysContent["parts"] = sysParts;

    QJsonObject userPart;
    userPart["text"] = userPrompt;
    QJsonArray userParts;
    userParts.append(userPart);
    QJsonObject userContent;
    userContent["role"] = "user";
    userContent["parts"] = userParts;

    QJsonObject systemInstruction;
    systemInstruction["parts"] = sysParts;

    QJsonArray contents;
    contents.append(userContent);

    QJsonObject body;
    body["contents"] = contents;
    body["system_instruction"] = systemInstruction;

    QByteArray bodyData = QJsonDocument(body).toJson();
    QNetworkReply *reply = networkManager->post(req, bodyData);

    // Move callback into heap so we can pass it through the signal
    auto cb = std::make_shared<std::function<void(const QString &)>>(std::move(callback));

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            (*cb)(QString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString response = doc["candidates"][0]["content"]["parts"][0]["text"].toString().trimmed();
        (*cb)(response);
    });
}
