#include "llm_router.hpp"
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <vector>

LLMRouter::LLMRouter(QObject* parent)
    : QObject(parent)
{
    qDebug() << "LLMRouter initialized";
}

LLMRouter::~LLMRouter()
{
    for (auto metrics : m_metrics) {
        delete metrics;
    }
    m_metrics.clear();
}

void LLMRouter::registerModel(const ModelInfo& model)
{
    m_models[model.id] = model;
    if (m_metrics.contains(model.id)) {
        delete m_metrics[model.id];
    }
    m_metrics[model.id] = new PerformanceMetrics();
    
    qDebug() << "Registered model:" << model.id 
             << "Provider:" << model.provider;
    emit modelRegistered(model.id);
}

void LLMRouter::unregisterModel(const QString& modelId)
{
    m_models.remove(modelId);
    if (m_metrics.contains(modelId)) {
        delete m_metrics[modelId];
        m_metrics.remove(modelId);
    }
    
    qDebug() << "Unregistered model:" << modelId;
    emit modelUnregistered(modelId);
}

ModelInfo LLMRouter::getModel(const QString& modelId) const
{
    return m_models.value(modelId);
}

QStringList LLMRouter::getAvailableModels() const
{
    QStringList result;
    for (auto it = m_models.begin(); it != m_models.end(); ++it) {
        if (it.value().available) {
            result.append(it.key());
        }
    }
    return result;
}

RoutingDecision LLMRouter::route(
    const QString& taskDescription,
    const QString& preferredCapability,
    int maxCostTokens)
{
    RoutingDecision decision;
    decision.decisionTimeMs = QDateTime::currentMSecsSinceEpoch();
    
    // Get available models
    auto availableModels = getAvailableModels();
    if (availableModels.isEmpty()) {
        decision.selectedModelId = "";
        decision.routingReason = "No models available";
        decision.confidenceScore = 0;
        qWarning() << "No available models for routing";
        return decision;
    }
    
    // Score each model
    QMap<QString, int> scores;
    for (const auto& modelId : availableModels) {
        const auto& model = m_models[modelId];
        
        int relevanceScore = calculateTaskRelevanceScore(model, preferredCapability);
        int costScore = calculateCostEfficiencyScore(model, maxCostTokens);
        int latencyScore = calculateLatencyScore(model);
        int reliabilityScore = calculateReliabilityScore(modelId);
        
        // Weighted scoring: 40% capability, 20% cost, 20% latency, 20% reliability
        int totalScore = (relevanceScore * 40 + costScore * 20 + 
                         latencyScore * 20 + reliabilityScore * 20) / 100;
        
        scores[modelId] = totalScore;
        
        qDebug() << "Model" << modelId 
                 << "Relevance:" << relevanceScore
                 << "Cost:" << costScore
                 << "Latency:" << latencyScore
                 << "Reliability:" << reliabilityScore
                 << "Total:" << totalScore;
    }
    
    // Select best model
    QString bestModelId;
    int bestScore = -1;
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        if (it.value() > bestScore) {
            bestScore = it.value();
            bestModelId = it.key();
        }
    }
    
    decision.selectedModelId = bestModelId;
    decision.confidenceScore = bestScore;
    decision.selectedInfo = m_models[decision.selectedModelId];
    decision.routingStrategy = m_routingStrategy;
    decision.routingReason = QString("Selected %1 for %2 (score: %3, strategy: %4)")
        .arg(decision.selectedModelId, preferredCapability, 
             QString::number(decision.confidenceScore), m_routingStrategy);
    
    // Get top 2 alternatives - iterate through scores and find top 2
    int topScore1 = bestScore;
    QString topModel1 = bestModelId;
    int topScore2 = -1;
    QString topModel2;
    
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        if (it.value() < topScore1 && it.value() > topScore2 && it.key() != bestModelId) {
            topScore2 = it.value();
            topModel2 = it.key();
        }
    }
    
    if (!topModel2.isEmpty()) {
        decision.alternativeModels.append(topModel2);
    }
    
    // Find third best for second alternative
    int topScore3 = -1;
    QString topModel3;
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        if (it.value() < topScore2 && it.value() > topScore3 && it.key() != bestModelId && it.key() != topModel2) {
            topScore3 = it.value();
            topModel3 = it.key();
        }
    }
    
    if (!topModel3.isEmpty()) {
        decision.alternativeModels.append(topModel3);
    }
    
    qDebug() << "Routing Decision:" << decision.selectedModelId 
             << "Confidence:" << decision.confidenceScore;
    
    emit routingDecisionMade(decision);
    return decision;
}

EnsembleResult LLMRouter::routeEnsemble(
    const QString& taskDescription,
    int numModels,
    const QString& consensusMethod)
{
    EnsembleResult result;
    auto availableModels = getAvailableModels();
    
    if (availableModels.isEmpty()) {
        result.consensus = "No models available";
        return result;
    }
    
    // Score all models and select top N
    QMap<QString, int> scores;
    for (const auto& modelId : availableModels) {
        const auto& model = m_models[modelId];
        // For ensemble, prefer reasoning capability
        int score = model.capabilities.reasoning * 40 + 
                   model.capabilities.coding * 30 +
                   model.capabilities.planning * 30;
        scores[modelId] = score;
    }
    
    // Select top N models - manually find top N
    for (int n = 0; n < numModels; ++n) {
        int topScore = -1;
        QString topModel;
        
        // Find highest score model not yet selected
        for (auto it = scores.begin(); it != scores.end(); ++it) {
            if (!result.selectedModels.contains(it.key()) && it.value() > topScore) {
                topScore = it.value();
                topModel = it.key();
            }
        }
        
        if (!topModel.isEmpty()) {
            result.selectedModels.append(topModel);
        } else {
            break;  // No more models available
        }
    }
    
    result.consensus = QString("Ensemble of %1 models: %2 using %3 strategy")
        .arg(QString::number(result.selectedModels.size()), 
             result.selectedModels.join(", "), 
             consensusMethod);
    
    result.agreementLevel = 0.85f;  // Placeholder: would be calculated from actual responses
    result.finalConfidence = 0.90f;
    
    qDebug() << "Ensemble Result:" << result.selectedModels.join(", ");
    
    return result;
}

void LLMRouter::recordPerformance(
    const QString& modelId,
    int taskDurationMs,
    int tokensUsed,
    double qualityScore)
{
    if (!m_metrics.contains(modelId)) {
        qWarning() << "Model not found for performance recording:" << modelId;
        return;
    }
    
    auto metrics = m_metrics[modelId];
    metrics->totalRequests.fetch_add(1);
    metrics->successfulRequests.fetch_add(1);
    metrics->totalLatencyMs.fetch_add(taskDurationMs);
    metrics->totalTokensUsed.fetch_add(tokensUsed);
    metrics->lastUsed = QDateTime::currentDateTime();
    
    // Update average quality using exponential moving average
    double alpha = 0.1;  // Weight for new value (0.1 = 10% new, 90% historical)
    metrics->averageQualityScore = 
        (1.0 - alpha) * metrics->averageQualityScore + 
        alpha * qualityScore;
    
    qDebug() << "Performance recorded for" << modelId 
             << "Duration:" << taskDurationMs << "ms"
             << "Tokens:" << tokensUsed
             << "Quality:" << qualityScore;
}

QJsonObject LLMRouter::getModelStatus(const QString& modelId) const
{
    QJsonObject status;
    
    if (!m_models.contains(modelId)) {
        status["error"] = "Model not found";
        return status;
    }
    
    const auto& model = m_models[modelId];
    auto metrics = m_metrics.value(modelId);
    
    status["id"] = model.id;
    status["provider"] = model.provider;
    status["available"] = model.available;
    status["endpoint"] = model.endpoint;
    status["contextWindow"] = model.contextWindow;
    status["avgTokenCost"] = model.avgTokenCost;
    status["avgLatencyMs"] = model.avgLatencyMs;
    
    // Capabilities
    QJsonObject capabilities;
    capabilities["reasoning"] = model.capabilities.reasoning;
    capabilities["coding"] = model.capabilities.coding;
    capabilities["planning"] = model.capabilities.planning;
    capabilities["creativity"] = model.capabilities.creativity;
    capabilities["speed"] = model.capabilities.speed;
    capabilities["costEfficiency"] = model.capabilities.costEfficiency;
    status["capabilities"] = capabilities;
    
    // Performance metrics
    QJsonObject perf;
    if (metrics) {
        perf["totalRequests"] = metrics->totalRequests.load();
        perf["successfulRequests"] = metrics->successfulRequests.load();
        perf["failedRequests"] = metrics->failedRequests.load();
        perf["totalLatencyMs"] = metrics->totalLatencyMs.load();
        perf["totalTokensUsed"] = metrics->totalTokensUsed.load();
        perf["averageQualityScore"] = metrics->averageQualityScore;
        perf["lastUsed"] = metrics->lastUsed.toString(Qt::ISODate);
        status["performance"] = perf;
        
        // Calculate success rate
        int total = metrics->totalRequests.load();
        if (total > 0) {
            double successRate = (100.0 * metrics->successfulRequests.load()) / total;
            status["successRate"] = successRate;
        }
    }
    
    return status;
}

QJsonArray LLMRouter::getAllModelStatus() const
{
    QJsonArray array;
    for (const auto& modelId : m_models.keys()) {
        array.append(getModelStatus(modelId));
    }
    return array;
}

void LLMRouter::handleModelFailure(const QString& modelId, const QString& error)
{
    if (!m_models.contains(modelId)) {
        qWarning() << "Model not found for failure handling:" << modelId;
        return;
    }
    
    m_models[modelId].available = false;
    if (m_metrics.contains(modelId)) {
        m_metrics[modelId]->failedRequests.fetch_add(1);
    }
    
    qWarning() << "Model failure:" << modelId << "Error:" << error;
    emit modelHealthChanged(modelId, false);
    
    // Trigger failover
    RoutingDecision fallback = getFallbackModel(modelId);
    if (!fallback.selectedModelId.isEmpty()) {
        emit failoverTriggered(modelId, fallback.selectedModelId);
    }
}

RoutingDecision LLMRouter::getFallbackModel(const QString& failedModelId)
{
    auto available = getAvailableModels();
    available.removeAll(failedModelId);
    
    if (available.isEmpty()) {
        RoutingDecision decision;
        decision.selectedModelId = "";
        decision.routingReason = "No fallback models available";
        return decision;
    }
    
    // Route to best available model
    return route("fallback request after model failure", "balanced", 0);
}

int LLMRouter::calculateTaskRelevanceScore(
    const ModelInfo& model,
    const QString& capability)
{
    return model.capabilities.getCapabilityScore(capability);
}

int LLMRouter::calculateCostEfficiencyScore(
    const ModelInfo& model,
    int maxCostTokens)
{
    if (maxCostTokens <= 0) {
        // No budget limit, use raw cost efficiency
        return model.capabilities.costEfficiency;
    }
    
    // If model cost exceeds budget, score is 0
    if (model.avgTokenCost * maxCostTokens / 1000.0 > maxCostTokens) {
        return 0;
    }
    
    // Otherwise score based on cost efficiency within budget
    return model.capabilities.costEfficiency;
}

int LLMRouter::calculateLatencyScore(const ModelInfo& model)
{
    // Lower latency = higher score
    // Normalize to 0-100 scale
    // Baseline: 100ms = score 100, 5000ms = score 0
    
    if (model.avgLatencyMs <= 100) return 100;
    if (model.avgLatencyMs >= 5000) return 0;
    
    int score = 100 - static_cast<int>((model.avgLatencyMs - 100) / 49.0);
    return qBound(0, score, 100);
}

int LLMRouter::calculateReliabilityScore(const QString& modelId)
{
    if (!m_metrics.contains(modelId)) {
        return 50;  // Unknown model gets middle score
    }
    
    auto metrics = m_metrics[modelId];
    if (!metrics) {
        return 50;
    }
    
    int totalRequests = metrics->totalRequests.load();
    
    if (totalRequests == 0) {
        return 50;  // No history, neutral score
    }
    
    int successfulRequests = metrics->successfulRequests.load();
    int successRate = (successfulRequests * 100) / totalRequests;
    return successRate;
}

QString LLMRouter::selectFromCandidates(const QStringList& candidates)
{
    if (candidates.isEmpty()) {
        return "";
    }
    
    if (!m_loadBalancingEnabled || candidates.size() == 1) {
        return candidates.first();
    }
    
    // Load balance: select least recently used
    QString lruCandidate = candidates.first();
    QDateTime lruTime;
    if (m_metrics.contains(lruCandidate) && m_metrics[lruCandidate]) {
        lruTime = m_metrics[lruCandidate]->lastUsed;
    }
    
    for (int i = 1; i < candidates.size(); ++i) {
        QDateTime candidateTime;
        if (m_metrics.contains(candidates[i]) && m_metrics[candidates[i]]) {
            candidateTime = m_metrics[candidates[i]]->lastUsed;
        }
        if (candidateTime < lruTime) {
            lruCandidate = candidates[i];
            lruTime = candidateTime;
        }
    }
    
    return lruCandidate;
}

void LLMRouter::setRoutingStrategy(const QString& strategy)
{
    m_routingStrategy = strategy;
    qDebug() << "Routing strategy changed to:" << strategy;
}

void LLMRouter::setLoadBalancingEnabled(bool enabled)
{
    m_loadBalancingEnabled = enabled;
    qDebug() << "Load balancing" << (enabled ? "enabled" : "disabled");
}

void LLMRouter::setCostOptimizationEnabled(bool enabled)
{
    m_costOptimizationEnabled = enabled;
    qDebug() << "Cost optimization" << (enabled ? "enabled" : "disabled");
}
