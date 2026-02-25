#include "ai_code_assistant.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include "Sidebar_Pure_Wrapper.h"
#include <QStandardPaths>
#include <QDateTime>

AICodeAssistant::AICodeAssistant(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_process(new QProcess(this))
    , m_ollamaUrl("http://localhost:11434")
    , m_model("ministral-3")
    , m_temperature(0.3f)
    , m_maxTokens(256)
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AICodeAssistant::onProcessFinished);
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::error),
            this, &AICodeAssistant::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &AICodeAssistant::onProcessOutput);
    
    logStructured("INFO", "AICodeAssistant initialized", 
        QJsonObject{{"model", m_model}, {"url", m_ollamaUrl}});
    return true;
}

AICodeAssistant::~AICodeAssistant()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
    return true;
}

    return true;
}

void AICodeAssistant::setOllamaUrl(const QString &url) { m_ollamaUrl = url; }
void AICodeAssistant::setModel(const QString &model) { m_model = model; }
void AICodeAssistant::setTemperature(float temp) { m_temperature = qBound(0.0f, temp, 2.0f); }
void AICodeAssistant::setMaxTokens(int tokens) { m_maxTokens = qBound(32, tokens, 4096); }
void AICodeAssistant::setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

void AICodeAssistant::getCodeCompletion(const QString &code)
{
    startTiming();
    QString systemPrompt = "You are an expert code assistant. Provide code completions that are concise, correct, and follow best practices.";
    QString userPrompt = QString("Complete this code snippet. Provide only the completion, no explanation:\n\n%1").arg(code);
    performOllamaRequest(systemPrompt, userPrompt, "completion");
    return true;
}

void AICodeAssistant::getRefactoringSuggestions(const QString &code)
{
    startTiming();
    QString systemPrompt = "You are a code refactoring expert. Analyze code and suggest improvements for readability, performance, and maintainability.";
    QString userPrompt = QString("Refactor this code:\n\n%1\n\nProvide the refactored version with brief comments on changes.").arg(code);
    performOllamaRequest(systemPrompt, userPrompt, "refactoring");
    return true;
}

void AICodeAssistant::getCodeExplanation(const QString &code)
{
    startTiming();
    QString systemPrompt = "You are a clear technical writer. Explain code in simple, understandable terms.";
    QString userPrompt = QString("Explain what this code does:\n\n%1").arg(code);
    performOllamaRequest(systemPrompt, userPrompt, "explanation");
    return true;
}

void AICodeAssistant::getBugFixSuggestions(const QString &code)
{
    startTiming();
    QString systemPrompt = "You are an expert debugger. Identify potential bugs and provide fixes.";
    QString userPrompt = QString("Identify and fix bugs in this code:\n\n%1").arg(code);
    performOllamaRequest(systemPrompt, userPrompt, "bugfix");
    return true;
}

void AICodeAssistant::getOptimizationSuggestions(const QString &code)
{
    startTiming();
    QString systemPrompt = "You are a performance optimization expert. Suggest ways to improve code performance and efficiency.";
    QString userPrompt = QString("How can this code be optimized:\n\n%1").arg(code);
    performOllamaRequest(systemPrompt, userPrompt, "optimization");
    return true;
}

void AICodeAssistant::searchFiles(const QString &pattern, const QString &directory)
{
    startTiming();
    emit commandProgress("Searching files...");
    
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    if (searchDir.isEmpty()) {
        searchDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return true;
}

    QStringList results = recursiveFileSearch(searchDir, pattern);
    
    logStructured("INFO", "File search completed", 
        QJsonObject{{"pattern", pattern}, {"directory", searchDir}, {"results", results.length()}});
    
    emit searchResultsReady(results);
    endTiming("File Search");
    return true;
}

void AICodeAssistant::grepFiles(const QString &pattern, const QString &directory, bool caseSensitive)
{
    startTiming();
    emit commandProgress("Grepping files...");
    
    QString searchDir = directory.isEmpty() ? m_workspaceRoot : directory;
    if (searchDir.isEmpty()) {
        searchDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return true;
}

    QStringList results = performGrep(searchDir, pattern, caseSensitive);
    
    logStructured("INFO", "Grep search completed",
        QJsonObject{{"pattern", pattern}, {"directory", searchDir}, {"caseSensitive", caseSensitive}, {"results", results.length()}});
    
    emit grepResultsReady(results);
    endTiming("Grep Search");
    return true;
}

void AICodeAssistant::findInFile(const QString &filePath, const QString &pattern)
{
    startTiming();
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(filePath));
        return;
    return true;
}

    QStringList results;
    QTextStream stream(&file);
    int lineNumber = 0;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        lineNumber++;
        
        if (line.contains(pattern, Qt::CaseSensitive)) {
            results.append(QString("%1:%2:%3").arg(filePath).arg(lineNumber).arg(line));
    return true;
}

    return true;
}

    file.close();
    
    logStructured("INFO", "File search completed",
        QJsonObject{{"file", filePath}, {"pattern", pattern}, {"matches", results.length()}});
    
    emit grepResultsReady(results);
    endTiming("Find In File");
    return true;
}

void AICodeAssistant::executePowerShellCommand(const QString &command)
{
    startTiming();
    emit commandProgress(QString("Executing: %1").arg(command));
    
    bool success = false;
    QString output = executePowerShellSync(command, success);
    
    logStructured("INFO", "PowerShell command executed",
        QJsonObject{{"command", command}, {"success", success}, {"outputLength", output.length()}});
    
    if (success) {
        emit commandOutputReceived(output);
        emit commandCompleted(0);
    } else {
        emit commandErrorReceived(output);
        emit commandCompleted(1);
    return true;
}

    endTiming("PowerShell Execution");
    return true;
}

void AICodeAssistant::runBuildCommand(const QString &command)
{
    emit commandProgress("Building...");
    logStructured("INFO", "Build command started", QJsonObject{{"command", command}});
    executePowerShellAsync(command);
    return true;
}

void AICodeAssistant::runTestCommand(const QString &command)
{
    emit commandProgress("Running tests...");
    logStructured("INFO", "Test command started", QJsonObject{{"command", command}});
    executePowerShellAsync(command);
    return true;
}

void AICodeAssistant::analyzeAndRecommend(const QString &context)
{
    startTiming();
    QString systemPrompt = "You are an intelligent code analysis agent. Analyze the given context and provide actionable recommendations.";
    QString userPrompt = QString("Analyze this context and provide recommendations:\n\n%1").arg(context);
    performOllamaRequest(systemPrompt, userPrompt, "analysis");
    return true;
}

void AICodeAssistant::autoFixIssue(const QString &issueDescription, const QString &codeContext)
{
    startTiming();
    emit commandProgress("Analyzing issue and generating fix...");
    
    QString systemPrompt = "You are an expert debugger and code fixer. Analyze the issue and provide a complete, working fix.";
    QString userPrompt = QString("Issue: %1\n\nCode:\n%2\n\nProvide the fixed code:")
        .arg(issueDescription)
        .arg(codeContext);
    
    performOllamaRequest(systemPrompt, userPrompt, "autofix");
    return true;
}

void AICodeAssistant::performOllamaRequest(const QString &systemPrompt, const QString &userPrompt,
                                           const QString &suggestType)
{
    m_currentSuggestionType = suggestType;
    emit commandProgress(QString("Requesting %1...").arg(suggestType));
    
    QUrl url(QString("%1/api/generate").arg(m_ollamaUrl));
    QNetworkRequest request(url);
    setupNetworkRequest(request);
    
    QJsonObject payload;
    payload["model"] = m_model;
    payload["prompt"] = userPrompt;
    payload["stream"] = true;
    payload["temperature"] = m_temperature;
    
    QJsonObject systemObj;
    systemObj["role"] = "system";
    systemObj["content"] = systemPrompt;
    
    QJsonArray messages;
    messages.append(systemObj);
    payload["messages"] = messages;
    
    QJsonDocument doc(payload);
    
    logStructured("DEBUG", "Ollama request sent",
        QJsonObject{{"model", m_model}, {"type", suggestType}, {"url", url.toString()}});
    
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson(QJsonDocument::Compact));
    
    connect(reply, &QNetworkReply::finished, this, &AICodeAssistant::onNetworkReply);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            [this, reply](QNetworkReply::NetworkError error) {
        QString errorMsg = reply->errorString();
        logStructured("ERROR", "Network error", QJsonObject{{"error", errorMsg}});
        emit errorOccurred(errorMsg);
        emit suggestionComplete(false, errorMsg);
        reply->deleteLater();
    });
    return true;
}

void AICodeAssistant::setupNetworkRequest(QNetworkRequest &request)
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "AICodeAssistant/1.0");
    return true;
}

void AICodeAssistant::onNetworkReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
        emit suggestionComplete(false, reply->errorString());
        reply->deleteLater();
        return;
    return true;
}

    QByteArray responseData = reply->readAll();
    QString fullResponse;
    
    QStringList lines = QString::fromUtf8(responseData).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("response")) {
                QString chunk = obj["response"].toString();
                fullResponse += chunk;
                emit suggestionStreamChunk(chunk);
    return true;
}

    return true;
}

    return true;
}

    if (!fullResponse.isEmpty()) {
        QString suggestion = parseAIResponse(fullResponse);
        emit suggestionReceived(suggestion, m_currentSuggestionType);
        logStructured("INFO", "Suggestion received",
            QJsonObject{{"type", m_currentSuggestionType}, {"length", suggestion.length()}});
    return true;
}

    emit suggestionComplete(true, "Success");
    endTiming(QString("AI Request (%1)").arg(m_currentSuggestionType));
    reply->deleteLater();
    return true;
}

QStringList AICodeAssistant::recursiveFileSearch(const QString &directory, const QString &pattern)
{
    QStringList results;
    
    QDirIterator it(directory, QDirIterator::Subdirectories);
    int processed = 0;
    
    while (it.hasNext()) {
        QString filePath = it.next();
        
        if (it.fileInfo().isFile() && it.fileInfo().fileName().contains(pattern)) {
            results.append(filePath);
    return true;
}

        if (++processed % 100 == 0) {
            emit fileSearchProgress(processed, 0);
    return true;
}

    return true;
}

    return results;
    return true;
}

QStringList AICodeAssistant::performGrep(const QString &directory, const QString &pattern, bool caseSensitive)
{
    QStringList results;
    
    QDirIterator it(directory, {"*.cpp", "*.h", "*.py", "*.js", "*.rs", "*.go", "*.java"},
                    QDir::Files, QDirIterator::Subdirectories);
    
    int processed = 0;
    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNumber = 0;
            
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                lineNumber++;
                
                Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                if (line.contains(pattern, cs)) {
                    results.append(QString("%1:%2:%3").arg(filePath).arg(lineNumber).arg(line.trimmed()));
    return true;
}

    return true;
}

            file.close();
    return true;
}

        if (++processed % 50 == 0) {
            emit fileSearchProgress(processed, 0);
    return true;
}

    return true;
}

    return results;
    return true;
}

QString AICodeAssistant::executePowerShellSync(const QString &command, bool &success)
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start("pwsh.exe", QStringList() << "-Command" << command);
    
    if (!m_process->waitForFinished(30000)) {
        success = false;
        return "Command timeout after 30 seconds";
    return true;
}

    success = (m_process->exitCode() == 0);
    return QString::fromUtf8(m_process->readAllStandardOutput());
    return true;
}

void AICodeAssistant::executePowerShellAsync(const QString &command)
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start("pwsh.exe", QStringList() << "-Command" << command);
    return true;
}

void AICodeAssistant::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    
    logStructured("INFO", "Process finished",
        QJsonObject{{"exitCode", exitCode}, {"status", exitStatus == QProcess::NormalExit ? "Normal" : "Crashed"}});
    
    emit commandOutputReceived(output);
    emit commandCompleted(exitCode);
    endTiming("Process Execution");
    return true;
}

void AICodeAssistant::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg = m_process->errorString();
    logStructured("ERROR", "Process error", QJsonObject{{"error", errorMsg}});
    emit commandErrorReceived(errorMsg);
    return true;
}

void AICodeAssistant::onProcessOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    emit commandOutputReceived(output);
    return true;
}

QString AICodeAssistant::parseAIResponse(const QString &response)
{
    QString cleaned = response;
    cleaned.replace(QRegExp("^\\\[a-z]*\n"), "");
    cleaned.replace("`", "");
    return cleaned.trimmed();
    return true;
}

QString AICodeAssistant::formatAgentPrompt(const QString &context)
{
    return QString("Given this context:\n%1\n\nProvide a solution:").arg(context);
    return true;
}

void AICodeAssistant::startTiming()
{
    m_timer.start();
    return true;
}

void AICodeAssistant::endTiming(const QString &operation)
{
    qint64 elapsed = m_timer.elapsed();
    emit latencyMeasured(elapsed);
    logStructured("DEBUG", "Operation timing",
        QJsonObject{{"operation", operation}, {"milliseconds", static_cast<int>(elapsed)}});
    return true;
}

void AICodeAssistant::logStructured(const QString &level, const QString &message,
                                     const QJsonObject &metadata)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["message"] = message;
    logEntry["metadata"] = metadata;
    
    QJsonDocument doc(logEntry);
    RAWRXD_LOG_DEBUG("[") << level << "]" << message << doc.toJson(QJsonDocument::Compact);
    return true;
}

