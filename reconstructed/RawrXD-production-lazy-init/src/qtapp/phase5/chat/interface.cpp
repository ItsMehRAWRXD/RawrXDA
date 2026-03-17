// ============================================================================
// Phase 5: Model-aware Chat Interface Implementation
// ============================================================================

#include "phase5_chat_interface.h"
#include "phase5_model_router.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <QStandardPaths>
#include <QDebug>

Phase5ChatInterface::Phase5ChatInterface(Phase5ModelRouter* router)
    : QObject(nullptr)
    , m_router(router)
    , m_temperature(0.7)
    , m_maxTokens(512)
    , m_contextWindow(2048)
    , m_streamingEnabled(false)
    , m_multiModelMode(false)
    , m_isGenerating(false)
{
    m_generationTimer = new QTimer(this);
    m_generationTimer->setSingleShot(true);
    
        // Connect router signals
        connect(m_router, &Phase5ModelRouter::inferenceCompleted,
            this, [this](const QString&, const QString& result, double tokensPerSecond) {
            emit responseReceived(result, tokensPerSecond);
            });
    connect(m_router, &Phase5ModelRouter::progressUpdated,
            this, &Phase5ChatInterface::progressUpdated);
        connect(m_router, &Phase5ModelRouter::routingDecisionMade,
            this, &Phase5ChatInterface::onRoutingDecision);
}

Phase5ChatInterface::~Phase5ChatInterface() {
    saveSession();
}

// ===== Session Management =====

QString Phase5ChatInterface::createSession(const QString& title, Phase5ChatMode mode) {
    m_currentSession = std::make_unique<ChatSession>();
    m_currentSession->sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentSession->title = title.isEmpty() ? "New Session" : title;
    m_currentSession->mode = mode;
    m_currentSession->multiModel = false;
    m_currentSession->totalTokensUsed = 0;
    m_currentSession->totalCostEstimate = 0.0;
    m_currentSession->createdAt = QDateTime::currentDateTime();
    m_currentSession->lastUpdatedAt = QDateTime::currentDateTime();
    
    // Set default model
    QStringList models = m_router->availableModels();
    if (!models.isEmpty()) {
        m_currentSession->selectedModel = models.first();
    }
    
    // Initialize session settings
    QJsonObject settings;
    settings["temperature"] = m_temperature;
    settings["maxTokens"] = m_maxTokens;
    settings["contextWindow"] = m_contextWindow;
    settings["streaming"] = m_streamingEnabled;
    m_currentSession->settings = settings;
    
    emit sessionCreated(m_currentSession->sessionId);
    return m_currentSession->sessionId;
}

bool Phase5ChatInterface::loadSession(const QString& sessionId) {
    return loadSessionFromFile(sessionId);
}

bool Phase5ChatInterface::saveSession() {
    if (!m_currentSession) {
        return false;
    }
    return saveSessionToFile();
}

void Phase5ChatInterface::onRoutingDecision(const QString& modelId, const QString& reason) {
    Q_UNUSED(reason);
    if (!m_currentSession) {
        return;
    }
    if (m_currentSession->selectedModel != modelId) {
        m_currentSession->selectedModel = modelId;
        m_currentSession->lastUpdatedAt = QDateTime::currentDateTime();
        emit modelChanged(modelId);
    }
}

QStringList Phase5ChatInterface::getSessionList() const {
    QStringList sessions;
    QString sessionsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sessions";
    QDir dir(sessionsDir);
    
    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString& file : files) {
        QString sessionId = file.left(file.length() - 5); // Remove .json
        sessions.append(sessionId);
    }
    
    return sessions;
}

bool Phase5ChatInterface::deleteSession(const QString& sessionId) {
    QString path = getSessionPath(sessionId);
    return QFile::remove(path);
}

ChatSession Phase5ChatInterface::getCurrentSession() const {
    return m_currentSession ? *m_currentSession : ChatSession();
}

// ===== Messaging =====

void Phase5ChatInterface::sendMessage(const QString& message) {
    if (!m_currentSession) {
        createSession("Auto-created Session", Phase5ChatMode::Standard);
    }
    
    // Add user message to history
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = message;
    userMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_currentSession->messages.append(userMsg);
    
    emit messageSent(message);
    
    // Prepare inference request
    InferenceRequest request;
    request.prompt = message;
    request.maxTokens = m_maxTokens;
    request.temperature = m_temperature;
    request.contextWindow = m_contextWindow;
    
    // Set mode-specific parameters
    switch (m_currentSession->mode) {
        case Phase5ChatMode::Max:
            request.mode = InferenceMode::Max;
            request.temperature = 0.9;
            break;
        case Phase5ChatMode::Research:
            request.mode = InferenceMode::Research;
            request.contextWindow = 8192;
            break;
        case Phase5ChatMode::DeepResearch:
            request.mode = InferenceMode::DeepResearch;
            request.contextWindow = 16384;
            break;
        case Phase5ChatMode::Thinking:
            request.mode = InferenceMode::Thinking;
            request.maxTokens = 4096;
            break;
        default:
            request.mode = InferenceMode::Standard;
            break;
    }
    
    request.preferredModel = m_currentSession->selectedModel;
    
    m_isGenerating = true;
    
    // Execute inference
    QString response = m_router->executeInference(request);
    
    // Add assistant response to history
    QJsonObject assistantMsg;
    assistantMsg["role"] = "assistant";
    assistantMsg["content"] = response;
    assistantMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    assistantMsg["model"] = m_currentSession->selectedModel;
    m_currentSession->messages.append(assistantMsg);
    
    // Update tokens and cost
    m_currentSession->totalTokensUsed += m_maxTokens; // Estimate
    m_currentSession->totalCostEstimate += calculateCost(m_maxTokens);
    m_currentSession->lastUpdatedAt = QDateTime::currentDateTime();
    
    emit costUpdated(m_currentSession->totalCostEstimate);
    
    m_isGenerating = false;
    
    // Auto-save
    saveSession();
}

void Phase5ChatInterface::sendMessageAsync(const QString& message) {
    QTimer::singleShot(0, this, [this, message]() {
        sendMessage(message);
    });
}

void Phase5ChatInterface::addMessageToHistory(const QString& role, const QString& content) {
    if (!m_currentSession) {
        createSession("Auto-created Session", Phase5ChatMode::Standard);
    }
    
    QJsonObject msg;
    msg["role"] = role;
    msg["content"] = content;
    msg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_currentSession->messages.append(msg);
}

QJsonArray Phase5ChatInterface::getHistory() const {
    return m_currentSession ? m_currentSession->messages : QJsonArray();
}

void Phase5ChatInterface::clearHistory() {
    if (m_currentSession) {
        m_currentSession->messages = QJsonArray();
        m_currentSession->totalTokensUsed = 0;
        m_currentSession->totalCostEstimate = 0.0;
        emit costUpdated(0.0);
    }
}

QJsonObject Phase5ChatInterface::getMessage(int index) const {
    if (m_currentSession && index >= 0 && index < m_currentSession->messages.size()) {
        return m_currentSession->messages[index].toObject();
    }
    return QJsonObject();
}

// ===== Model Management =====

void Phase5ChatInterface::setPrimaryModel(const QString& modelName) {
    if (m_currentSession) {
        m_currentSession->selectedModel = modelName;
        emit modelChanged(modelName);
    }
}

void Phase5ChatInterface::setSecondaryModel(const QString& modelName) {
    if (m_currentSession) {
        m_currentSession->selectedModel2 = modelName;
    }
}

QStringList Phase5ChatInterface::getAvailableModels() const {
    return m_router->availableModels();
}

void Phase5ChatInterface::setMultiModelMode(bool enabled) {
    m_multiModelMode = enabled;
    if (m_currentSession) {
        m_currentSession->multiModel = enabled;
    }
}

// ===== Chat Modes =====

void Phase5ChatInterface::setChatMode(Phase5ChatMode mode) {
    if (m_currentSession) {
        m_currentSession->mode = mode;
        emit modeChanged(mode);
    }
}

Phase5ChatMode Phase5ChatInterface::getChatMode() const {
    return m_currentSession ? m_currentSession->mode : Phase5ChatMode::Standard;
}

QString Phase5ChatInterface::executeMax(const QString& prompt) {
    return m_router->executeMax(prompt, m_maxTokens);
}

QString Phase5ChatInterface::executeResearch(const QString& prompt) {
    return m_router->executeResearch(prompt, 1024);
}

QString Phase5ChatInterface::executeDeepResearch(const QString& prompt) {
    return m_router->executeDeepResearch(prompt, 2048);
}

QString Phase5ChatInterface::executeThinking(const QString& prompt) {
    return m_router->executeThinking(prompt, 4096);
}

QJsonObject Phase5ChatInterface::compareModels(const QString& prompt) {
    QJsonObject comparison;
    
    if (!m_currentSession || !m_multiModelMode) {
        return comparison;
    }
    
    // Execute on primary model
    InferenceRequest request1;
    request1.prompt = prompt;
    request1.maxTokens = m_maxTokens;
    request1.temperature = m_temperature;
    request1.preferredModel = m_currentSession->selectedModel;
    
    QString response1 = m_router->executeInference(request1);
    
    // Execute on secondary model
    InferenceRequest request2;
    request2.prompt = prompt;
    request2.maxTokens = m_maxTokens;
    request2.temperature = m_temperature;
    request2.preferredModel = m_currentSession->selectedModel2;
    
    QString response2 = m_router->executeInference(request2);
    
    // Build comparison
    QJsonObject model1;
    model1["name"] = m_currentSession->selectedModel;
    model1["response"] = response1;
    model1["metrics"] = QJsonObject(); // Add metrics
    
    QJsonObject model2;
    model2["name"] = m_currentSession->selectedModel2;
    model2["response"] = response2;
    model2["metrics"] = QJsonObject(); // Add metrics
    
    comparison["model1"] = model1;
    comparison["model2"] = model2;
    comparison["prompt"] = prompt;
    
    return comparison;
}

// ===== Configuration =====

void Phase5ChatInterface::setTemperature(double temp) {
    m_temperature = qBound(0.0, temp, 2.0);
    if (m_currentSession) {
        m_currentSession->settings["temperature"] = m_temperature;
    }
}

void Phase5ChatInterface::setMaxTokens(int maxTokens) {
    m_maxTokens = qMax(1, maxTokens);
    if (m_currentSession) {
        m_currentSession->settings["maxTokens"] = m_maxTokens;
    }
}

void Phase5ChatInterface::setContextWindow(int contextSize) {
    m_contextWindow = qMax(512, contextSize);
    if (m_currentSession) {
        m_currentSession->settings["contextWindow"] = m_contextWindow;
    }
}

void Phase5ChatInterface::setSystemPrompt(const QString& prompt) {
    m_systemPrompt = prompt;
    if (m_currentSession) {
        m_currentSession->settings["systemPrompt"] = m_systemPrompt;
    }
}

QJsonObject Phase5ChatInterface::getCurrentSettings() const {
    return m_currentSession ? m_currentSession->settings : QJsonObject();
}

// ===== Analytics =====

QJsonObject Phase5ChatInterface::getSessionStats() const {
    QJsonObject stats;
    
    if (m_currentSession) {
        stats["sessionId"] = m_currentSession->sessionId;
        stats["title"] = m_currentSession->title;
        stats["messageCount"] = m_currentSession->messages.size();
        stats["totalTokens"] = m_currentSession->totalTokensUsed;
        stats["estimatedCost"] = m_currentSession->totalCostEstimate;
        stats["createdAt"] = m_currentSession->createdAt.toString(Qt::ISODate);
        stats["duration"] = m_currentSession->createdAt.secsTo(m_currentSession->lastUpdatedAt);
    }
    
    return stats;
}

int Phase5ChatInterface::getTotalTokensUsed() const {
    return m_currentSession ? m_currentSession->totalTokensUsed : 0;
}

double Phase5ChatInterface::getEstimatedCost() const {
    return m_currentSession ? m_currentSession->totalCostEstimate : 0.0;
}

QJsonObject Phase5ChatInterface::getQualityMetrics() const {
    QJsonObject metrics;
    
    if (m_currentSession) {
        // Calculate average response length, quality scores, etc.
        int totalLength = 0;
        int count = 0;
        
        for (const QJsonValue& val : m_currentSession->messages) {
            QJsonObject msg = val.toObject();
            if (msg["role"].toString() == "assistant") {
                totalLength += msg["content"].toString().length();
                count++;
            }
        }
        
        metrics["avgResponseLength"] = count > 0 ? totalLength / count : 0;
        metrics["responseCount"] = count;
        metrics["coherenceScore"] = 0.85; // Placeholder
        metrics["relevanceScore"] = 0.90; // Placeholder
    }
    
    return metrics;
}

// ===== Streaming & Progress =====

void Phase5ChatInterface::enableStreaming(bool enable) {
    m_streamingEnabled = enable;
    if (m_currentSession) {
        m_currentSession->settings["streaming"] = enable;
    }
}

void Phase5ChatInterface::setProgressCallback(std::function<void(int, int)> callback) {
    m_progressCallback = callback;
}

void Phase5ChatInterface::cancelGeneration() {
    m_isGenerating = false;
    emit generationCancelled();
}

// ===== Export & Sharing =====

QString Phase5ChatInterface::exportToMarkdown() const {
    if (!m_currentSession) {
        return "";
    }
    
    QString markdown;
    markdown += "# " + m_currentSession->title + "\n\n";
    markdown += "**Session ID:** " + m_currentSession->sessionId + "\n";
    markdown += "**Created:** " + m_currentSession->createdAt.toString() + "\n";
    markdown += "**Model:** " + m_currentSession->selectedModel + "\n\n";
    markdown += "---\n\n";
    
    for (const QJsonValue& val : m_currentSession->messages) {
        QJsonObject msg = val.toObject();
        QString role = msg["role"].toString();
        QString content = msg["content"].toString();
        
        if (role == "user") {
            markdown += "## 🙋 User\n\n";
        } else {
            markdown += "## 🤖 Assistant\n\n";
        }
        
        markdown += content + "\n\n";
        markdown += "---\n\n";
    }
    
    return markdown;
}

QString Phase5ChatInterface::exportToJson() const {
    if (!m_currentSession) {
        return "{}";
    }
    
    QJsonObject session;
    session["sessionId"] = m_currentSession->sessionId;
    session["title"] = m_currentSession->title;
    session["mode"] = static_cast<int>(m_currentSession->mode);
    session["selectedModel"] = m_currentSession->selectedModel;
    session["messages"] = m_currentSession->messages;
    session["settings"] = m_currentSession->settings;
    session["totalTokens"] = m_currentSession->totalTokensUsed;
    session["totalCost"] = m_currentSession->totalCostEstimate;
    session["createdAt"] = m_currentSession->createdAt.toString(Qt::ISODate);
    
    QJsonDocument doc(session);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool Phase5ChatInterface::importFromMarkdown(const QString& content) {
    // Parse markdown and recreate session
    // This is a simplified implementation
    createSession("Imported Session", Phase5ChatMode::Standard);
    
    QStringList lines = content.split('\n');
    QString currentRole;
    QString currentContent;
    
    for (const QString& line : lines) {
        if (line.startsWith("## 🙋 User")) {
            if (!currentContent.isEmpty()) {
                addMessageToHistory(currentRole, currentContent.trimmed());
            }
            currentRole = "user";
            currentContent.clear();
        } else if (line.startsWith("## 🤖 Assistant")) {
            if (!currentContent.isEmpty()) {
                addMessageToHistory(currentRole, currentContent.trimmed());
            }
            currentRole = "assistant";
            currentContent.clear();
        } else if (!line.startsWith("#") && !line.startsWith("**") && !line.startsWith("---")) {
            currentContent += line + "\n";
        }
    }
    
    if (!currentContent.isEmpty()) {
        addMessageToHistory(currentRole, currentContent.trimmed());
    }
    
    return true;
}

// ===== Private Methods =====

QString Phase5ChatInterface::getSessionPath(const QString& sessionId) const {
    QString sessionsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sessions";
    QDir().mkpath(sessionsDir);
    return sessionsDir + "/" + sessionId + ".json";
}

bool Phase5ChatInterface::loadSessionFromFile(const QString& sessionId) {
    QString path = getSessionPath(sessionId);
    QFile file(path);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    m_currentSession = std::make_unique<ChatSession>();
    m_currentSession->sessionId = obj["sessionId"].toString();
    m_currentSession->title = obj["title"].toString();
    m_currentSession->mode = static_cast<Phase5ChatMode>(obj["mode"].toInt());
    m_currentSession->selectedModel = obj["selectedModel"].toString();
    m_currentSession->messages = obj["messages"].toArray();
    m_currentSession->settings = obj["settings"].toObject();
    m_currentSession->totalTokensUsed = obj["totalTokens"].toInt();
    m_currentSession->totalCostEstimate = obj["totalCost"].toDouble();
    m_currentSession->createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    
    // Load settings
    m_temperature = m_currentSession->settings["temperature"].toDouble(0.7);
    m_maxTokens = m_currentSession->settings["maxTokens"].toInt(512);
    m_contextWindow = m_currentSession->settings["contextWindow"].toInt(2048);
    
    emit sessionLoaded(sessionId);
    return true;
}

bool Phase5ChatInterface::saveSessionToFile() {
    if (!m_currentSession) {
        return false;
    }
    
    QString path = getSessionPath(m_currentSession->sessionId);
    QFile file(path);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonObject obj;
    obj["sessionId"] = m_currentSession->sessionId;
    obj["title"] = m_currentSession->title;
    obj["mode"] = static_cast<int>(m_currentSession->mode);
    obj["selectedModel"] = m_currentSession->selectedModel;
    obj["selectedModel2"] = m_currentSession->selectedModel2;
    obj["multiModel"] = m_currentSession->multiModel;
    obj["messages"] = m_currentSession->messages;
    obj["settings"] = m_currentSession->settings;
    obj["totalTokens"] = m_currentSession->totalTokensUsed;
    obj["totalCost"] = m_currentSession->totalCostEstimate;
    obj["createdAt"] = m_currentSession->createdAt.toString(Qt::ISODate);
    obj["lastUpdated"] = m_currentSession->lastUpdatedAt.toString(Qt::ISODate);
    
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

double Phase5ChatInterface::calculateCost(int tokens) const {
    // Simplified cost calculation ($0.001 per 1000 tokens)
    return (tokens / 1000.0) * 0.001;
}
