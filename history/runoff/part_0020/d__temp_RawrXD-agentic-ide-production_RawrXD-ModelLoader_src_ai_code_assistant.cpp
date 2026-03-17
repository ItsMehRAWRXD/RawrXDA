#include "ai_code_assistant.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <QDebug>
#include <QDateTime>
#include <chrono>
#include <regex>

AICodeAssistant::AICodeAssistant(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_ollamaHost("localhost"),
      m_ollamaPort(11434),
      m_model("ministral-3"),
      m_temperature(0.7f),
      m_maxTokens(256),
      m_searchTimer(new QTimer(this)),
      m_commandTimer(new QTimer(this)) {
    
    // Setup timers
    connect(m_searchTimer, &QTimer::timeout, this, &AICodeAssistant::onSearchTimeout);
    connect(m_commandTimer, &QTimer::timeout, this, &AICodeAssistant::onCommandTimeout);
    
    // Network reply handling
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AICodeAssistant::onNetworkReplyFinished);
    
    logStructured("INFO", "AICodeAssistant initialized", {
        {"model", m_model},
        {"host", m_ollamaHost},
        {"port", m_ollamaPort}
    });
}

AICodeAssistant::~AICodeAssistant() {
    logStructured("INFO", "AICodeAssistant destroyed", {});
}

// ===== Configuration Methods =====

void AICodeAssistant::setOllamaServer(const QString &host, int port) {
    m_ollamaHost = host;
    m_ollamaPort = port;
    logStructured("DEBUG", "Ollama server configured", {
        {"host", host},
        {"port", port}
    });
}

void AICodeAssistant::setWorkspaceRoot(const QString &root) {
    m_workspaceRoot = root;
    logStructured("DEBUG", "Workspace root set", {{"path", root}});
}

void AICodeAssistant::setTemperature(float temperature) {
    m_temperature = qBound(0.0f, temperature, 2.0f);
    logStructured("DEBUG", "Temperature set", {{"temperature", QString::number(m_temperature)}});
}

void AICodeAssistant::setMaxTokens(int maxTokens) {
    m_maxTokens = qBound(32, maxTokens, 512);
}

void AICodeAssistant::setModel(const QString &modelName) {
    m_model = modelName;
    logStructured("DEBUG", "Model changed", {{"model", modelName}});
}

// ===== AI SUGGESTION METHODS =====

void AICodeAssistant::getCodeCompletion(const QString &code, const QString &context) {
    QString prompt = QString("Complete this code:\n%1\n\nContext: %2\n\nComplete the code:").arg(code, context);
    sendAIRequest(prompt, "completionReady");
}

void AICodeAssistant::getRefactoringSuggestions(const QString &code) {
    QString prompt = QString("Refactor this code to improve readability and performance:\n%1\n\nProvide refactored version:").arg(code);
    sendAIRequest(prompt, "refactoringReady");
}

void AICodeAssistant::getCodeExplanation(const QString &code) {
    QString prompt = QString("Explain what this code does:\n%1\n\nExplanation:").arg(code);
    sendAIRequest(prompt, "explanationReady");
}

void AICodeAssistant::getBugFixSuggestions(const QString &code, const QString &errorMsg) {
    QString prompt = QString("Fix this code that produces the error:\n%1\n\nError: %2\n\nFixed code:").arg(code, errorMsg);
    sendAIRequest(prompt, "bugFixReady");
}

void AICodeAssistant::getOptimizationSuggestions(const QString &code) {
    QString prompt = QString("Optimize this code for performance:\n%1\n\nOptimized version:").arg(code);
    sendAIRequest(prompt, "optimizationReady");
}

// ===== IDE TOOL METHODS (Agentic Actions) =====

void AICodeAssistant::searchFiles(const QString &pattern, const QString &directory) {
    emit agentActionStarted("searchFiles");
    logStructured("INFO", "File search started", {
        {"pattern", pattern},
        {"directory", directory.isEmpty() ? m_workspaceRoot : directory}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    if (!QDir(searchDir).exists()) {
        emit errorOccurred(QString("Directory does not exist: %1").arg(searchDir));
        return;
    }
    
    QStringList results;
    int processed = 0;
    
    try {
        recursiveSearch(searchDir, pattern, results, processed);
        emit fileSearchProgress(processed, processed);
        emit searchResultsReady(results);
        
        qint64 elapsed = getElapsedMilliseconds();
        emit latencyMeasured(elapsed);
        logStructured("INFO", "File search completed", {
            {"filesProcessed", QString::number(processed)},
            {"resultsFound", QString::number(results.count())},
            {"durationMs", QString::number(elapsed)}
        });
    } catch (const std::exception &e) {
        emit errorOccurred(QString("Search failed: %1").arg(e.what()));
    }
    
    emit agentActionCompleted("searchFiles");
}

void AICodeAssistant::grepFiles(const QString &pattern, const QString &directory, bool caseSensitive) {
    emit agentActionStarted("grepFiles");
    logStructured("INFO", "Grep search started", {
        {"pattern", pattern},
        {"directory", directory.isEmpty() ? m_workspaceRoot : directory},
        {"caseSensitive", caseSensitive ? "true" : "false"}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    QJsonArray results;
    
    try {
        recursiveGrep(searchDir, pattern, results, caseSensitive);
        emit grepResultsReady(results);
        
        qint64 elapsed = getElapsedMilliseconds();
        emit latencyMeasured(elapsed);
        logStructured("INFO", "Grep search completed", {
            {"matchesFound", QString::number(results.count())},
            {"durationMs", QString::number(elapsed)}
        });
    } catch (const std::exception &e) {
        emit errorOccurred(QString("Grep failed: %1").arg(e.what()));
    }
    
    emit agentActionCompleted("grepFiles");
}

void AICodeAssistant::findInFile(const QString &filePath, const QString &pattern) {
    emit agentActionStarted("findInFile");
    logStructured("DEBUG", "Find in file started", {
        {"file", filePath},
        {"pattern", pattern}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(filePath));
        emit agentActionCompleted("findInFile");
        return;
    }
    
    QJsonArray results;
    QStringList lines = QString(file.readAll()).split('\n');
    
    for (int i = 0; i < lines.count(); ++i) {
        if (lines[i].contains(pattern)) {
            results.append(QJsonObject{
                {"lineNumber", i + 1},
                {"content", lines[i]}
            });
        }
    }
    
    file.close();
    
    emit grepResultsReady(results);
    qint64 elapsed = getElapsedMilliseconds();
    emit latencyMeasured(elapsed);
    logStructured("DEBUG", "Find in file completed", {
        {"matchesFound", QString::number(results.count())},
        {"durationMs", QString::number(elapsed)}
    });
    
    emit agentActionCompleted("findInFile");
}

void AICodeAssistant::executePowerShellCommand(const QString &command) {
    emit agentActionStarted("executePowerShellCommand");
    emit commandProgress("Starting command execution...");
    
    logStructured("INFO", "PowerShell command execution started", {
        {"command", command}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString output;
    int exitCode = executeCommandSync(command, output);
    
    emit commandOutputReceived(output);
    emit commandCompleted(exitCode);
    
    qint64 elapsed = getElapsedMilliseconds();
    emit latencyMeasured(elapsed);
    logStructured("INFO", "PowerShell command completed", {
        {"command", command},
        {"exitCode", QString::number(exitCode)},
        {"outputLength", QString::number(output.length())},
        {"durationMs", QString::number(elapsed)}
    });
    
    emit agentActionCompleted("executePowerShellCommand");
}

void AICodeAssistant::runBuildCommand(const QString &command) {
    emit agentActionStarted("runBuildCommand");
    emit commandProgress("Starting build...");
    
    logStructured("INFO", "Build command started", {
        {"command", command}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString output;
    int exitCode = executeCommandSync(command, output);
    
    emit commandOutputReceived(output);
    emit commandCompleted(exitCode);
    
    qint64 elapsed = getElapsedMilliseconds();
    logStructured("INFO", "Build completed", {
        {"command", command},
        {"exitCode", QString::number(exitCode)},
        {"durationMs", QString::number(elapsed)}
    });
    
    emit agentActionCompleted("runBuildCommand");
}

void AICodeAssistant::runTestCommand(const QString &command) {
    emit agentActionStarted("runTestCommand");
    emit commandProgress("Running tests...");
    
    logStructured("INFO", "Test command started", {
        {"command", command}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString output;
    int exitCode = executeCommandSync(command, output);
    
    emit commandOutputReceived(output);
    emit commandCompleted(exitCode);
    
    qint64 elapsed = getElapsedMilliseconds();
    logStructured("INFO", "Tests completed", {
        {"command", command},
        {"exitCode", QString::number(exitCode)},
        {"durationMs", QString::number(elapsed)}
    });
    
    emit agentActionCompleted("runTestCommand");
}

void AICodeAssistant::analyzeAndRecommend(const QString &context) {
    emit agentActionStarted("analyzeAndRecommend");
    logStructured("INFO", "Code analysis started", {
        {"contextLength", QString::number(context.length())}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString prompt = QString("Analyze this code and provide recommendations:\n%1\n\nProvide analysis and recommendations:").arg(context);
    
    // Send AI request for analysis
    QJsonObject requestPayload = buildRequestPayload(prompt);
    QNetworkRequest request(QUrl(QString("http://%1:%2/api/generate").arg(m_ollamaHost).arg(m_ollamaPort)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_networkManager->post(request, QJsonDocument(requestPayload).toJson());
    
    emit agentActionCompleted("analyzeAndRecommend");
}

void AICodeAssistant::autoFixIssue(const QString &issueDescription, const QString &codeContext) {
    emit agentActionStarted("autoFixIssue");
    logStructured("INFO", "Auto-fix started", {
        {"issue", issueDescription}
    });
    
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QString prompt = QString("Fix this issue:\n%1\n\nCode context:\n%2\n\nProvide fixed code:").arg(issueDescription, codeContext);
    
    QJsonObject requestPayload = buildRequestPayload(prompt);
    QNetworkRequest request(QUrl(QString("http://%1:%2/api/generate").arg(m_ollamaHost).arg(m_ollamaPort)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_networkManager->post(request, QJsonDocument(requestPayload).toJson());
    
    emit agentActionCompleted("autoFixIssue");
}

// ===== LOGGING & METRICS =====

void AICodeAssistant::logStructured(const QString &level, const QString &message, const QJsonObject &metadata) {
    QJsonObject logEntry{
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"level", level},
        {"message", message}
    };
    
    if (!metadata.isEmpty()) {
        logEntry.insert("metadata", metadata);
    }
    
    // Output to console/file (in production, write to file)
    qDebug() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

// ===== PRIVATE HELPER METHODS =====

void AICodeAssistant::sendAIRequest(const QString &prompt, const QString &responseSignal) {
    m_operationStart = std::chrono::high_resolution_clock::now();
    
    QJsonObject requestPayload = buildRequestPayload(prompt);
    QNetworkRequest request(QUrl(QString("http://%1:%2/api/generate").arg(m_ollamaHost).arg(m_ollamaPort)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_networkManager->post(request, QJsonDocument(requestPayload).toJson());
}

QJsonObject AICodeAssistant::buildRequestPayload(const QString &prompt) {
    return QJsonObject{
        {"model", m_model},
        {"prompt", prompt},
        {"temperature", m_temperature},
        {"num_predict", m_maxTokens},
        {"stream", false}
    };
}

void AICodeAssistant::parseAIResponse(const QString &response, const QString &signalName) {
    // Parse JSON response and emit appropriate signal
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (!doc.isObject()) {
        emit errorOccurred("Invalid response format");
        return;
    }
    
    QString responseText = doc.object()["response"].toString();
    qint64 elapsed = getElapsedMilliseconds();
    
    emit latencyMeasured(elapsed);
    logStructured("DEBUG", "AI response received", {
        {"signal", signalName},
        {"responseLength", QString::number(responseText.length())},
        {"latencyMs", QString::number(elapsed)}
    });
}

void AICodeAssistant::recursiveSearch(const QString &dir, const QString &pattern, QStringList &results, int &processed) {
    QDirIterator it(dir, QDirIterator::Subdirectories);
    
    std::regex regex(pattern.toStdString(), std::regex::icase);
    
    while (it.hasNext()) {
        it.next();
        processed++;
        
        if (it.fileInfo().isFile() && std::regex_search(it.fileName().toStdString(), regex)) {
            results.append(it.filePath());
        }
    }
}

void AICodeAssistant::recursiveGrep(const QString &dir, const QString &pattern, QJsonArray &results, bool caseSensitive) {
    QDirIterator it(dir, QDirIterator::Subdirectories);
    
    std::regex::flag_type flags = caseSensitive ? std::regex::ECMAScript : std::regex::icase;
    std::regex regex(pattern.toStdString(), flags);
    
    while (it.hasNext()) {
        it.next();
        
        if (!it.fileInfo().isFile()) continue;
        
        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        
        QStringList lines = QString(file.readAll()).split('\n');
        for (int i = 0; i < lines.count(); ++i) {
            if (std::regex_search(lines[i].toStdString(), regex)) {
                results.append(QJsonObject{
                    {"file", it.filePath()},
                    {"lineNumber", i + 1},
                    {"content", lines[i]}
                });
            }
        }
        
        file.close();
    }
}

int AICodeAssistant::executeCommandSync(const QString &command, QString &output) {
    QProcess process;
    
    // Use PowerShell on Windows
#ifdef Q_OS_WIN
    process.start("powershell.exe", QStringList() << "-NoProfile" << "-Command" << command);
#else
    process.start("/bin/sh", QStringList() << "-c" << command);
#endif
    
    // Wait up to 30 seconds
    if (!process.waitForFinished(30000)) {
        process.kill();
        output = "Command timeout (30 seconds)";
        return -1;
    }
    
    output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    if (!errorOutput.isEmpty()) {
        output += "\nSTDERR:\n" + errorOutput;
    }
    
    return process.exitCode();
}

void AICodeAssistant::onNetworkReplyFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("Network error: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    parseAIResponse(QString(responseData), "completionReady");
    
    reply->deleteLater();
}

void AICodeAssistant::onSearchTimeout() {
    emit errorOccurred("File search timeout");
    emit agentActionCompleted("searchFiles");
}

void AICodeAssistant::onCommandTimeout() {
    emit errorOccurred("Command execution timeout");
    emit agentActionCompleted("executePowerShellCommand");
}

qint64 AICodeAssistant::getElapsedMilliseconds() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_operationStart);
    return duration.count();
}
