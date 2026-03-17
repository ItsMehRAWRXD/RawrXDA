// ════════════════════════════════════════════════════════════════════════════════
// MULTI-MODEL AGENT COORDINATOR - Cursor 2.0 Style Multi-Agent System
// Supports up to 8 simultaneous AI agents with different models
// ════════════════════════════════════════════════════════════════════════════════

#include "multi_model_agent_coordinator.h"
#include "agentic_engine.h"
#include "external_model_client.h"
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>
#include <QUuid>
#include <algorithm>
#include <chrono>

namespace RawrXD {
namespace IDE {

const int MAX_SIMULTANEOUS_AGENTS = 8;
const int DEFAULT_RESPONSE_TIMEOUT_MS = 30000; // 30 seconds

MultiModelAgentCoordinator::MultiModelAgentCoordinator(QObject* parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_activeAgents(0),
      m_browserModeEnabled(false)
{
    qDebug() << "[MultiModelAgentCoordinator] Initialized - Ready for multi-model coordination";

    // Initialize supported model providers
    initializeModelProviders();

    // Setup network manager for browser mode
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MultiModelAgentCoordinator::onNetworkReplyFinished);
}

MultiModelAgentCoordinator::~MultiModelAgentCoordinator()
{
    // Stop all active agents
    stopAllAgents();

    qDebug() << "[MultiModelAgentCoordinator] Destroyed";
}

void MultiModelAgentCoordinator::initializeModelProviders()
{
    // OpenAI Models
    m_supportedProviders["openai"] = {
        {"gpt-4", "GPT-4 (Latest)"},
        {"gpt-4-turbo", "GPT-4 Turbo"},
        {"gpt-3.5-turbo", "GPT-3.5 Turbo"}
    };

    // Anthropic Models
    m_supportedProviders["anthropic"] = {
        {"claude-3-opus", "Claude 3 Opus"},
        {"claude-3-sonnet", "Claude 3 Sonnet"},
        {"claude-3-haiku", "Claude 3 Haiku"}
    };

    // Google Models
    m_supportedProviders["google"] = {
        {"gemini-pro", "Gemini Pro"},
        {"gemini-pro-vision", "Gemini Pro Vision"},
        {"palm-2", "PaLM 2"}
    };

    // Meta Models
    m_supportedProviders["meta"] = {
        {"llama-2-70b", "Llama 2 70B"},
        {"llama-2-13b", "Llama 2 13B"},
        {"llama-2-7b", "Llama 2 7B"}
    };

    // Local Models (Ollama, etc.)
    m_supportedProviders["local"] = {
        {"neural-chat", "Neural Chat"},
        {"mistral", "Mistral"},
        {"codellama", "Code Llama"},
        {"dolphin-mixtral", "Dolphin Mixtral"}
    };
}

// ════════════════════════════════════════════════════════════════════════════════
// AGENT MANAGEMENT
// ════════════════════════════════════════════════════════════════════════════════

QString MultiModelAgentCoordinator::createAgent(const QString& modelProvider,
                                                const QString& modelName,
                                                const QString& role)
{
    if (m_activeAgents >= MAX_SIMULTANEOUS_AGENTS) {
        qWarning() << "[MultiModelAgentCoordinator] Maximum agents reached:" << MAX_SIMULTANEOUS_AGENTS;
        return QString();
    }

    QString agentId = QUuid::createUuid().toString();

    auto agent = std::make_unique<MultiModelAgent>();
    agent->agentId = agentId;
    agent->modelProvider = modelProvider;
    agent->modelName = modelName;
    agent->role = role;
    agent->isActive = false;
    agent->isAvailable = true;
    agent->lastActive = QDateTime::currentDateTime();
    agent->responseTimeMs = 0;
    agent->qualityScore = 0.0f;

    // Create inference engine for this agent
    agent->inferenceEngine = new InferenceEngine(this);
    agent->externalClient = new ExternalModelClient(this);

    // Setup signal connections
    setupAgentConnections(agent.get());

    m_agents[agentId] = std::move(agent);
    m_activeAgents++;

    emit agentCreated(agentId, modelProvider, modelName);

    qInfo() << "[MultiModelAgentCoordinator] Created agent:" << agentId
            << "with model:" << modelProvider << "/" << modelName;

    return agentId;
}

void MultiModelAgentCoordinator::removeAgent(const QString& agentId)
{
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        if (it->second->isActive) {
            stopAgent(agentId);
        }

        m_agents.erase(it);
        m_activeAgents--;

        emit agentRemoved(agentId);

        qInfo() << "[MultiModelAgentCoordinator] Removed agent:" << agentId;
    }
}

bool MultiModelAgentCoordinator::startAgent(const QString& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent || agent->isActive) {
        return false;
    }

    agent->isActive = true;
    agent->isAvailable = false;
    agent->lastActive = QDateTime::currentDateTime();

    emit agentStarted(agentId);

    qInfo() << "[MultiModelAgentCoordinator] Started agent:" << agentId;

    return true;
}

bool MultiModelAgentCoordinator::stopAgent(const QString& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent || !agent->isActive) {
        return false;
    }

    agent->isActive = false;
    agent->isAvailable = true;

    // Cancel any pending requests
    if (agent->currentRequestId != -1) {
        agent->inferenceEngine->cancelRequest(agent->currentRequestId);
        agent->currentRequestId = -1;
    }

    emit agentStopped(agentId);

    qInfo() << "[MultiModelAgentCoordinator] Stopped agent:" << agentId;

    return true;
}

void MultiModelAgentCoordinator::stopAllAgents()
{
    for (auto& pair : m_agents) {
        if (pair.second->isActive) {
            stopAgent(pair.first);
        }
    }
}

MultiModelAgent* MultiModelAgentCoordinator::getAgent(const QString& agentId)
{
    auto it = m_agents.find(agentId);
    return it != m_agents.end() ? it->second.get() : nullptr;
}

QStringList MultiModelAgentCoordinator::getActiveAgentIds() const
{
    QStringList activeIds;
    for (const auto& pair : m_agents) {
        if (pair.second->isActive) {
            activeIds.append(pair.first);
        }
    }
    return activeIds;
}

QStringList MultiModelAgentCoordinator::getAllAgentIds() const
{
    QStringList allIds;
    for (const auto& pair : m_agents) {
        allIds.append(pair.first);
    }
    return allIds;
}

// ════════════════════════════════════════════════════════════════════════════════
// MULTI-AGENT EXECUTION
// ════════════════════════════════════════════════════════════════════════════════

QString MultiModelAgentCoordinator::executeParallelQuery(const QString& query,
                                                         const QStringList& agentIds,
                                                         bool enableBrowserMode)
{
    QString sessionId = QUuid::createUuid().toString();

    // Create parallel execution session
    auto session = std::make_unique<ParallelExecutionSession>();
    session->sessionId = sessionId;
    session->query = query;
    session->agentIds = agentIds;
    session->startTime = QDateTime::currentDateTime();
    session->enableBrowserMode = enableBrowserMode;
    session->completedAgents = 0;
    session->totalAgents = agentIds.size();

    // Initialize responses map
    for (const QString& agentId : agentIds) {
        session->responses[agentId] = QString();
        session->responseTimes[agentId] = 0;
        session->responseQualities[agentId] = 0.0f;
    }

    m_activeSessions[sessionId] = std::move(session);

    // Start parallel execution
    for (const QString& agentId : agentIds) {
        executeQueryOnAgent(sessionId, agentId, query, enableBrowserMode);
    }

    emit parallelExecutionStarted(sessionId, agentIds.size());

    qInfo() << "[MultiModelAgentCoordinator] Started parallel execution session:"
            << sessionId << "with" << agentIds.size() << "agents";

    return sessionId;
}

void MultiModelAgentCoordinator::executeQueryOnAgent(const QString& sessionId,
                                                    const QString& agentId,
                                                    const QString& query,
                                                    bool enableBrowserMode)
{
    auto agent = getAgent(agentId);
    auto session = getSession(sessionId);

    if (!agent || !session) {
        qWarning() << "[MultiModelAgentCoordinator] Invalid agent or session for execution";
        return;
    }

    if (!agent->isActive) {
        startAgent(agentId);
    }

    agent->lastActive = QDateTime::currentDateTime();
    agent->currentSessionId = sessionId;

    // Prepare enhanced query with browser mode if enabled
    QString enhancedQuery = query;
    if (enableBrowserMode && m_browserModeEnabled) {
        enhancedQuery = enhanceQueryWithBrowserData(query);
    }

    // Execute based on model provider
    if (agent->modelProvider == "openai") {
        executeOpenAIQuery(agent, enhancedQuery);
    } else if (agent->modelProvider == "anthropic") {
        executeAnthropicQuery(agent, enhancedQuery);
    } else if (agent->modelProvider == "google") {
        executeGoogleQuery(agent, enhancedQuery);
    } else if (agent->modelProvider == "local") {
        executeLocalQuery(agent, enhancedQuery);
    } else {
        qWarning() << "[MultiModelAgentCoordinator] Unsupported provider:" << agent->modelProvider;
    }
}

void MultiModelAgentCoordinator::executeOpenAIQuery(MultiModelAgent* agent, const QString& query)
{
    if (!agent->externalClient) return;

    // Configure OpenAI client
    QJsonObject config;
    config["model"] = agent->modelName;
    config["temperature"] = 0.7;
    config["max_tokens"] = 2048;

    agent->currentRequestId = agent->externalClient->sendRequest("openai", query, config);
}

void MultiModelAgentCoordinator::executeAnthropicQuery(MultiModelAgent* agent, const QString& query)
{
    if (!agent->externalClient) return;

    // Configure Anthropic client
    QJsonObject config;
    config["model"] = agent->modelName;
    config["temperature"] = 0.7;
    config["max_tokens"] = 2048;

    agent->currentRequestId = agent->externalClient->sendRequest("anthropic", query, config);
}

void MultiModelAgentCoordinator::executeGoogleQuery(MultiModelAgent* agent, const QString& query)
{
    if (!agent->externalClient) return;

    // Configure Google client
    QJsonObject config;
    config["model"] = agent->modelName;
    config["temperature"] = 0.7;
    config["max_tokens"] = 2048;

    agent->currentRequestId = agent->externalClient->sendRequest("google", query, config);
}

void MultiModelAgentCoordinator::executeLocalQuery(MultiModelAgent* agent, const QString& query)
{
    if (!agent->inferenceEngine) return;

    // Configure local inference
    GenerationConfig config;
    config.temperature = 0.7f;
    config.topP = 0.9f;
    config.maxTokens = 2048;

    agent->currentRequestId = agent->inferenceEngine->generateResponse(query, config);
}

// ════════════════════════════════════════════════════════════════════════════════
// BROWSER MODE INTEGRATION
// ════════════════════════════════════════════════════════════════════════════════

void MultiModelAgentCoordinator::setBrowserModeEnabled(bool enabled)
{
    m_browserModeEnabled = enabled;

    if (enabled) {
        qInfo() << "[MultiModelAgentCoordinator] Browser mode enabled";
    } else {
        qInfo() << "[MultiModelAgentCoordinator] Browser mode disabled";
    }
}

QString MultiModelAgentCoordinator::enhanceQueryWithBrowserData(const QString& query)
{
    // For now, add a note that browser data could be fetched
    // In a full implementation, this would fetch relevant web data
    QString enhanced = query + "\n\n[Browser Mode: Consider fetching up-to-date information from web sources if needed]";

    // TODO: Implement actual web data fetching based on query context
    // This could include:
    // - API documentation lookup
    // - Library version checking
    // - Current best practices research

    return enhanced;
}

void MultiModelAgentCoordinator::fetchWebData(const QString& url, const QString& context)
{
    if (!m_browserModeEnabled) return;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                     "RawrXD-IDE/1.0 (Multi-Model Agent Browser Mode)");

    QNetworkReply* reply = m_networkManager->get(request);

    // Store context for when reply arrives
    m_pendingRequests[reply] = context;

    qDebug() << "[MultiModelAgentCoordinator] Fetching web data from:" << url;
}

void MultiModelAgentCoordinator::onNetworkReplyFinished(QNetworkReply* reply)
{
    if (!m_pendingRequests.contains(reply)) {
        reply->deleteLater();
        return;
    }

    QString context = m_pendingRequests.take(reply);

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString content = QString::fromUtf8(data);

        emit webDataFetched(context, content);

        qDebug() << "[MultiModelAgentCoordinator] Successfully fetched web data for context:" << context;
    } else {
        qWarning() << "[MultiModelAgentCoordinator] Failed to fetch web data:" << reply->errorString();
    }

    reply->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════════
// RESPONSE HANDLING
// ════════════════════════════════════════════════════════════════════════════════

void MultiModelAgentCoordinator::onAgentResponseReceived(const QString& agentId,
                                                        const QString& response,
                                                        qint64 responseTimeMs,
                                                        float qualityScore)
{
    auto agent = getAgent(agentId);
    if (!agent) return;

    agent->responseTimeMs = responseTimeMs;
    agent->qualityScore = qualityScore;
    agent->lastActive = QDateTime::currentDateTime();

    // Update session if agent is part of one
    if (!agent->currentSessionId.isEmpty()) {
        updateSessionResponse(agent->currentSessionId, agentId, response,
                            responseTimeMs, qualityScore);
    }

    emit agentResponseReceived(agentId, response, responseTimeMs, qualityScore);

    qInfo() << "[MultiModelAgentCoordinator] Agent" << agentId
            << "responded in" << responseTimeMs << "ms with quality score:" << qualityScore;
}

void MultiModelAgentCoordinator::updateSessionResponse(const QString& sessionId,
                                                      const QString& agentId,
                                                      const QString& response,
                                                      qint64 responseTimeMs,
                                                      float qualityScore)
{
    auto session = getSession(sessionId);
    if (!session) return;

    session->responses[agentId] = response;
    session->responseTimes[agentId] = responseTimeMs;
    session->responseQualities[agentId] = qualityScore;
    session->completedAgents++;

    // Check if all agents have responded
    if (session->completedAgents >= session->totalAgents) {
        finalizeParallelExecution(sessionId);
    }
}

void MultiModelAgentCoordinator::finalizeParallelExecution(const QString& sessionId)
{
    auto session = getSession(sessionId);
    if (!session) return;

    qint64 totalTime = session->startTime.msecsTo(QDateTime::currentDateTime());

    // Calculate aggregate statistics
    double avgQuality = 0.0;
    qint64 fastestTime = LLONG_MAX;
    qint64 slowestTime = 0;

    for (const auto& time : session->responseTimes) {
        avgQuality += session->responseQualities[time.first];
        fastestTime = std::min(fastestTime, time.second);
        slowestTime = std::max(slowestTime, time.second);
    }

    if (!session->responseTimes.empty()) {
        avgQuality /= session->responseTimes.size();
    }

    emit parallelExecutionCompleted(sessionId, session->responses,
                                  totalTime, avgQuality, fastestTime, slowestTime);

    qInfo() << "[MultiModelAgentCoordinator] Completed parallel execution session:"
            << sessionId << "in" << totalTime << "ms, average quality:" << avgQuality;

    // Clean up session
    m_activeSessions.erase(sessionId);
}

// ════════════════════════════════════════════════════════════════════════════════
// MODEL MANAGEMENT
// ════════════════════════════════════════════════════════════════════════════════

QStringList MultiModelAgentCoordinator::getSupportedProviders() const
{
    QStringList providers;
    for (const auto& pair : m_supportedProviders) {
        providers.append(pair.first);
    }
    return providers;
}

QStringList MultiModelAgentCoordinator::getModelsForProvider(const QString& provider) const
{
    QStringList models;
    auto it = m_supportedProviders.find(provider);
    if (it != m_supportedProviders.end()) {
        for (const auto& modelPair : it->second) {
            models.append(modelPair.first);
        }
    }
    return models;
}

bool MultiModelAgentCoordinator::isModelSupported(const QString& provider, const QString& model) const
{
    auto providerIt = m_supportedProviders.find(provider);
    if (providerIt != m_supportedProviders.end()) {
        return providerIt->second.find(model) != providerIt->second.end();
    }
    return false;
}

void MultiModelAgentCoordinator::switchAgentModel(const QString& agentId,
                                                 const QString& newProvider,
                                                 const QString& newModel)
{
    auto agent = getAgent(agentId);
    if (!agent) return;

    if (!isModelSupported(newProvider, newModel)) {
        qWarning() << "[MultiModelAgentCoordinator] Unsupported model:" << newProvider << "/" << newModel;
        return;
    }

    bool wasActive = agent->isActive;
    if (wasActive) {
        stopAgent(agentId);
    }

    agent->modelProvider = newProvider;
    agent->modelName = newModel;

    if (wasActive) {
        startAgent(agentId);
    }

    emit agentModelSwitched(agentId, newProvider, newModel);

    qInfo() << "[MultiModelAgentCoordinator] Switched agent" << agentId
            << "to model:" << newProvider << "/" << newModel;
}

// ════════════════════════════════════════════════════════════════════════════════
// UTILITY METHODS
// ════════════════════════════════════════════════════════════════════════════════

void MultiModelAgentCoordinator::setupAgentConnections(MultiModelAgent* agent)
{
    if (!agent) return;

    // Connect inference engine signals
    connect(agent->inferenceEngine, &InferenceEngine::responseReady,
            this, [this, agent](const QString& response) {
        onAgentResponseReceived(agent->agentId, response,
                              agent->inferenceEngine->getLastResponseTime(),
                              calculateResponseQuality(response));
    });

    // Connect external client signals
    connect(agent->externalClient, &ExternalModelClient::responseFinished,
            this, [this, agent](const QString& response) {
        onAgentResponseReceived(agent->agentId, response,
                              agent->externalClient->getLastResponseTime(),
                              calculateResponseQuality(response));
    });
}

float MultiModelAgentCoordinator::calculateResponseQuality(const QString& response)
{
    // Simple quality heuristic based on response characteristics
    // In a real implementation, this could use more sophisticated metrics

    if (response.isEmpty()) return 0.0f;

    float score = 0.5f; // Base score

    // Length appropriateness (not too short, not too long)
    int length = response.length();
    if (length > 50 && length < 5000) score += 0.2f;

    // Contains code if it looks like a coding response
    if (response.contains("```") || response.contains("function") || response.contains("class")) {
        score += 0.1f;
    }

    // Well-structured response
    if (response.contains("\n\n") || response.contains("- ")) {
        score += 0.1f;
    }

    return std::min(score, 1.0f);
}

ParallelExecutionSession* MultiModelAgentCoordinator::getSession(const QString& sessionId)
{
    auto it = m_activeSessions.find(sessionId);
    return it != m_activeSessions.end() ? it->second.get() : nullptr;
}

QJsonObject MultiModelAgentCoordinator::getAgentStatus(const QString& agentId) const
{
    QJsonObject status;

    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        const auto& agent = it->second;
        status["agentId"] = agent->agentId;
        status["modelProvider"] = agent->modelProvider;
        status["modelName"] = agent->modelName;
        status["role"] = agent->role;
        status["isActive"] = agent->isActive;
        status["isAvailable"] = agent->isAvailable;
        status["lastActive"] = agent->lastActive.toString();
        status["responseTimeMs"] = static_cast<int>(agent->responseTimeMs);
        status["qualityScore"] = agent->qualityScore;
    }

    return status;
}

QJsonObject MultiModelAgentCoordinator::getSessionStatus(const QString& sessionId) const
{
    QJsonObject status;

    auto it = m_activeSessions.find(sessionId);
    if (it != m_activeSessions.end()) {
        const auto& session = it->second;
        status["sessionId"] = session->sessionId;
        status["query"] = session->query;
        status["totalAgents"] = session->totalAgents;
        status["completedAgents"] = session->completedAgents;
        status["startTime"] = session->startTime.toString();
        status["enableBrowserMode"] = session->enableBrowserMode;

        QJsonObject responses;
        for (const auto& resp : session->responses) {
            responses[resp.first] = resp.second;
        }
        status["responses"] = responses;
    }

    return status;
}

} // namespace IDE
} // namespace RawrXD