#include "ai_code_assistant.h"
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

AICodeAssistant::AICodeAssistant(QObject *parent) 
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_ollamaUrl("http://localhost:11434")
    , m_model("codellama:latest")
    , m_temperature(0.7f)
    , m_maxTokens(512)
    , m_workspaceRoot(QDir::currentPath())
{
    // Initialize network manager
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AICodeAssistant::onNetworkReply);
}

AICodeAssistant::~AICodeAssistant()
{
    // Qt parent-child relationship handles cleanup automatically
    // m_networkManager and m_process are owned by this QObject
    // No manual cleanup needed - Qt will delete them when this object is destroyed
}

void AICodeAssistant::setOllamaUrl(const QString &url)
{
    m_ollamaUrl = url;
}

void AICodeAssistant::setModel(const QString &model)
{
    m_model = model;
}

void AICodeAssistant::setTemperature(float temp)
{
    m_temperature = qBound(0.0f, temp, 1.0f);
}

void AICodeAssistant::setMaxTokens(int tokens)
{
    m_maxTokens = qMax(1, tokens);
}

void AICodeAssistant::setWorkspaceRoot(const QString &root)
{
    m_workspaceRoot = root;
}

void AICodeAssistant::getCodeCompletion(const QString &code)
{
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject request;
    request["model"] = m_model;
    request["prompt"] = QString("Complete the following code:\n%1\n\nCompletion:").arg(code);
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    QNetworkRequest networkRequest(QUrl(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    emit latencyMeasured(timer.elapsed());
}

void AICodeAssistant::getRefactoringSuggestions(const QString &code)
{
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject request;
    request["model"] = m_model;
    request["prompt"] = QString("Refactor the following code for better readability and performance:\n%1\n\nRefactored code:").arg(code);
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    QNetworkRequest networkRequest(QUrl(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    emit latencyMeasured(timer.elapsed());
}

void AICodeAssistant::getCodeExplanation(const QString &code)
{
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject request;
    request["model"] = m_model;
    request["prompt"] = QString("Explain the following code:\n%1\n\nExplanation:").arg(code);
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    QNetworkRequest networkRequest(QUrl(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    emit latencyMeasured(timer.elapsed());
}

void AICodeAssistant::searchFiles(const QString &pattern, const QString &directory)
{
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    QDir dir(searchDir);
    
    QStringList results;
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    
    for (const QFileInfo &file : files) {
        if (file.fileName().contains(pattern, Qt::CaseInsensitive)) {
            results.append(file.absoluteFilePath());
        }
    }
    
    emit searchResultsReady(results);
}

void AICodeAssistant::grepFiles(const QString &pattern, const QString &directory, bool caseSensitive)
{
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    
    // Use QProcess to execute grep command
    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished,
                this, &AICodeAssistant::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &AICodeAssistant::onProcessError);
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &AICodeAssistant::onProcessOutput);
    }
    
    QStringList args;
    if (caseSensitive) {
        args << "-n" << "-H" << pattern << searchDir + "/*";
    } else {
        args << "-n" << "-H" << "-i" << pattern << searchDir + "/*";
    }
    
    if (m_process) {
        m_process->start("grep", args);
    } else {
        emit errorOccurred("Process initialization failed");
    }
}

void AICodeAssistant::executePowerShellCommand(const QString &command)
{
    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished,
                this, &AICodeAssistant::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &AICodeAssistant::onProcessError);
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &AICodeAssistant::onProcessOutput);
    }
    
    QStringList args;
    args << "-Command" << command;
    
    if (m_process) {
        m_process->start("powershell", args);
    } else {
        emit errorOccurred("Process initialization failed");
    }
    emit commandProgress("Executing PowerShell command...");
}

void AICodeAssistant::analyzeAndRecommend(const QString &context)
{
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject request;
    request["model"] = m_model;
    request["prompt"] = QString("Analyze the following code context and provide recommendations:\n%1\n\nRecommendations:").arg(context);
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    QNetworkRequest networkRequest(QUrl(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    emit latencyMeasured(timer.elapsed());
}

void AICodeAssistant::autoFixIssue(const QString &issueDescription, const QString &codeContext)
{
    QElapsedTimer timer;
    timer.start();
    
    QJsonObject request;
    request["model"] = m_model;
    request["prompt"] = QString("Issue: %1\nCode Context: %2\n\nFix:").arg(issueDescription).arg(codeContext);
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    QNetworkRequest networkRequest(QUrl(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    emit latencyMeasured(timer.elapsed());
}

void AICodeAssistant::onNetworkReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("response")) {
                QString responseText = obj["response"].toString();
                emit suggestionReceived(responseText, "completion");
                emit suggestionComplete(true, "Success");
            }
        }
    } else {
        emit errorOccurred(reply->errorString());
        emit suggestionComplete(false, reply->errorString());
    }
    
    reply->deleteLater();
}

void AICodeAssistant::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    
    if (!m_process) {
        emit errorOccurred("Process not initialized");
        return;
    }
    
    QString output = m_process->readAllStandardOutput();
    QString error = m_process->readAllStandardError();
    
    if (!output.isEmpty()) {
        emit commandOutputReceived(output);
    }
    if (!error.isEmpty()) {
        emit commandErrorReceived(error);
    }
    
    emit commandCompleted(exitCode);
    emit commandProgress("Command execution completed");
}

void AICodeAssistant::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Process failed to start";
            break;
        case QProcess::Crashed:
            errorMsg = "Process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Process timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error";
            break;
        default:
            errorMsg = "Unknown process error";
            break;
    }
    
    emit errorOccurred(errorMsg);
    emit commandCompleted(-1);
}

void AICodeAssistant::onProcessOutput()
{
    if (!m_process) {
        return;
    }
    
    QString output = m_process->readAllStandardOutput();
    if (!output.isEmpty()) {
        emit commandOutputReceived(output);
    }
}
