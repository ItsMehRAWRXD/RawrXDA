#include "LoadBalancingStrategies.h"
#include "ModelRouterExtension.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>
#include <algorithm>
#include <cmath>

// ============ LoadBalancingStats Serialization ============

QJsonObject LoadBalancingStats::toJson() const {
    QJsonObject obj;
    obj["endpointId"] = endpointId;
    obj["requestCount"] = requestCount;
    obj["successCount"] = successCount;
    obj["failureCount"] = failureCount;
    obj["averageResponseTime"] = averageResponseTime;
    obj["currentLoad"] = currentLoad;
    obj["activeConnections"] = activeConnections;
    obj["lastUpdated"] = lastUpdated.toString(Qt::ISODate);
    
    return obj;
}

// ============ MLFeatures ============

QVector<double> MLFeatures::toVector() const {
    QVector<double> vec;
    vec << requestSize << historicalLatency << currentLoad << timeOfDay 
        << dayOfWeek << queueDepth << errorRate;
    return vec;
}

// ============ SimpleNeuralNetwork Implementation ============

SimpleNeuralNetwork::SimpleNeuralNetwork(int inputSize, int hiddenSize, int outputSize)
    : m_inputSize(inputSize), m_hiddenSize(hiddenSize), m_outputSize(outputSize)
{
    // Initialize weights with random values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-0.5, 0.5);
    
    // Input to hidden
    m_weightsInputHidden.resize(inputSize);
    for (int i = 0; i < inputSize; ++i) {
        m_weightsInputHidden[i].resize(hiddenSize);
        for (int j = 0; j < hiddenSize; ++j) {
            m_weightsInputHidden[i][j] = dis(gen);
        }
    }
    
    // Hidden to output
    m_weightsHiddenOutput.resize(hiddenSize);
    for (int i = 0; i < hiddenSize; ++i) {
        m_weightsHiddenOutput[i].resize(outputSize);
        for (int j = 0; j < outputSize; ++j) {
            m_weightsHiddenOutput[i][j] = dis(gen);
        }
    }
    
    // Biases
    m_biasHidden.resize(hiddenSize);
    m_biasOutput.resize(outputSize);
    for (int i = 0; i < hiddenSize; ++i) {
        m_biasHidden[i] = dis(gen);
    }
    for (int i = 0; i < outputSize; ++i) {
        m_biasOutput[i] = dis(gen);
    }
}

QVector<double> SimpleNeuralNetwork::forward(const QVector<double>& input) {
    if (input.size() != m_inputSize) {
        qWarning() << "Input size mismatch:" << input.size() << "expected" << m_inputSize;
        return QVector<double>(m_outputSize, 0.0);
    }
    
    // Hidden layer
    QVector<double> hidden(m_hiddenSize, 0.0);
    for (int j = 0; j < m_hiddenSize; ++j) {
        double sum = m_biasHidden[j];
        for (int i = 0; i < m_inputSize; ++i) {
            sum += input[i] * m_weightsInputHidden[i][j];
        }
        hidden[j] = sigmoid(sum);
    }
    
    // Output layer
    QVector<double> output(m_outputSize, 0.0);
    for (int j = 0; j < m_outputSize; ++j) {
        double sum = m_biasOutput[j];
        for (int i = 0; i < m_hiddenSize; ++i) {
            sum += hidden[i] * m_weightsHiddenOutput[i][j];
        }
        output[j] = sigmoid(sum);
    }
    
    return output;
}

void SimpleNeuralNetwork::train(const QVector<double>& input, double target, double learningRate) {
    // Forward pass
    QVector<double> hidden(m_hiddenSize);
    for (int j = 0; j < m_hiddenSize; ++j) {
        double sum = m_biasHidden[j];
        for (int i = 0; i < m_inputSize; ++i) {
            sum += input[i] * m_weightsInputHidden[i][j];
        }
        hidden[j] = sigmoid(sum);
    }
    
    double output = 0.0;
    for (int i = 0; i < m_hiddenSize; ++i) {
        output += hidden[i] * m_weightsHiddenOutput[i][0];
    }
    output = sigmoid(output + m_biasOutput[0]);
    
    // Backward pass
    double outputError = target - output;
    double outputDelta = outputError * sigmoidDerivative(output);
    
    // Update hidden-output weights
    for (int i = 0; i < m_hiddenSize; ++i) {
        m_weightsHiddenOutput[i][0] += learningRate * outputDelta * hidden[i];
    }
    m_biasOutput[0] += learningRate * outputDelta;
    
    // Hidden layer error
    QVector<double> hiddenErrors(m_hiddenSize);
    for (int i = 0; i < m_hiddenSize; ++i) {
        hiddenErrors[i] = outputDelta * m_weightsHiddenOutput[i][0] * sigmoidDerivative(hidden[i]);
    }
    
    // Update input-hidden weights
    for (int i = 0; i < m_inputSize; ++i) {
        for (int j = 0; j < m_hiddenSize; ++j) {
            m_weightsInputHidden[i][j] += learningRate * hiddenErrors[j] * input[i];
        }
    }
    for (int j = 0; j < m_hiddenSize; ++j) {
        m_biasHidden[j] += learningRate * hiddenErrors[j];
    }
}

QJsonObject SimpleNeuralNetwork::saveWeights() const {
    QJsonObject obj;
    obj["inputSize"] = m_inputSize;
    obj["hiddenSize"] = m_hiddenSize;
    obj["outputSize"] = m_outputSize;
    
    // Save weights (simplified - would need proper serialization for production)
    QJsonArray inputHidden;
    for (const auto& row : m_weightsInputHidden) {
        QJsonArray rowArray;
        for (double val : row) {
            rowArray.append(val);
        }
        inputHidden.append(rowArray);
    }
    obj["weightsInputHidden"] = inputHidden;
    
    return obj;
}

void SimpleNeuralNetwork::loadWeights(const QJsonObject& weights) {
    // Simplified implementation
    m_inputSize = weights["inputSize"].toInt();
    m_hiddenSize = weights["hiddenSize"].toInt();
    m_outputSize = weights["outputSize"].toInt();
    
    // Would load actual weights in production
}

double SimpleNeuralNetwork::sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

double SimpleNeuralNetwork::sigmoidDerivative(double x) {
    return x * (1.0 - x);
}

// ============ LoadBalancingStrategies Implementation ============

LoadBalancingStrategies::LoadBalancingStrategies(QObject* parent)
    : QObject(parent)
    , m_algorithm(LoadBalancingAlgorithm::RoundRobin)
    , m_roundRobinIndex(0)
    , m_hashRingInitialized(false)
    , m_mlModelInitialized(false)
    , m_randomEngine(std::random_device{}())
{
}

LoadBalancingStrategies::~LoadBalancingStrategies() {
}

// ============ Algorithm Selection ============

void LoadBalancingStrategies::setAlgorithm(LoadBalancingAlgorithm algorithm) {
    QMutexLocker locker(&m_mutex);
    m_algorithm = algorithm;
    qDebug() << "Load balancing algorithm set to:" << static_cast<int>(algorithm);
}

void LoadBalancingStrategies::setCustomBalancingFunction(
    std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> func) {
    QMutexLocker locker(&m_mutex);
    m_customBalancingFunction = func;
    qDebug() << "Custom balancing function set";
}

// ============ Server Management ============

void LoadBalancingStrategies::addServer(const QString& endpointId, double weight) {
    QMutexLocker locker(&m_mutex);
    
    ServerWeight sw;
    sw.endpointId = endpointId;
    sw.weight = weight;
    sw.currentWeight = 0;
    sw.effectiveWeight = static_cast<int>(weight * 100);
    
    m_serverWeights[endpointId] = sw;
    
    LoadBalancingStats stats;
    stats.endpointId = endpointId;
    m_statistics[endpointId] = stats;
    
    qDebug() << "Server added:" << endpointId << "with weight:" << weight;
}

void LoadBalancingStrategies::removeServer(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    m_serverWeights.remove(endpointId);
    m_statistics.remove(endpointId);
    
    // Remove from hash ring if present
    if (m_hashRingInitialized) {
        removeFromHashRing(endpointId);
    }
    
    qDebug() << "Server removed:" << endpointId;
}

void LoadBalancingStrategies::updateServerWeight(const QString& endpointId, double weight) {
    QMutexLocker locker(&m_mutex);
    
    if (m_serverWeights.contains(endpointId)) {
        m_serverWeights[endpointId].weight = weight;
        m_serverWeights[endpointId].effectiveWeight = static_cast<int>(weight * 100);
        qDebug() << "Server weight updated:" << endpointId << "to" << weight;
    }
}

QList<QString> LoadBalancingStrategies::getAllServers() const {
    QMutexLocker locker(&m_mutex);
    return m_serverWeights.keys();
}

// ============ Load Balancing ============

QString LoadBalancingStrategies::selectEndpoint(const RoutingRequest& request, 
                                                const QList<ModelEndpoint>& availableEndpoints) {
    if (availableEndpoints.isEmpty()) {
        return QString();
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString selected;
    
    switch (m_algorithm) {
        case LoadBalancingAlgorithm::RoundRobin:
            selected = roundRobinSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::WeightedRoundRobin:
            selected = weightedRoundRobinSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::Random:
            selected = randomSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::WeightedRandom:
            selected = weightedRandomSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::LeastConnections:
            selected = leastConnectionsSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::LeastResponseTime:
            selected = leastResponseTimeSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::LeastLoad:
            selected = leastLoadSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::PowerOfTwoChoices:
            selected = powerOfTwoChoicesSelect(availableEndpoints);
            break;
        case LoadBalancingAlgorithm::ConsistentHashing:
            selected = consistentHashSelect(request.requestId);
            break;
        case LoadBalancingAlgorithm::IpHash:
            // Would need client IP from request
            selected = ipHashSelect("127.0.0.1", availableEndpoints);
            break;
        case LoadBalancingAlgorithm::AdaptiveMLBased:
            selected = mlBasedSelect(request, availableEndpoints);
            break;
        case LoadBalancingAlgorithm::Custom:
            if (m_customBalancingFunction) {
                selected = m_customBalancingFunction(request, availableEndpoints);
            } else {
                selected = roundRobinSelect(availableEndpoints);
            }
            break;
    }
    
    if (!selected.isEmpty()) {
        emit endpointSelected(selected, m_algorithm);
    }
    
    return selected;
}

QString LoadBalancingStrategies::selectHealthyEndpoint(const RoutingRequest& request, 
                                                       const QList<ModelEndpoint>& endpoints) {
    // Filter to healthy endpoints only
    QList<ModelEndpoint> healthy;
    for (const ModelEndpoint& ep : endpoints) {
        if (ep.healthy && ep.enabled) {
            healthy.append(ep);
        }
    }
    
    return selectEndpoint(request, healthy);
}

// ============ Round Robin Variants ============

QString LoadBalancingStrategies::roundRobinSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    m_roundRobinIndex = (m_roundRobinIndex + 1) % endpoints.size();
    return endpoints[m_roundRobinIndex].id;
}

QString LoadBalancingStrategies::weightedRoundRobinSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    return selectWeightedRoundRobinInternal();
}

// ============ Random Variants ============

QString LoadBalancingStrategies::randomSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    std::uniform_int_distribution<> dist(0, endpoints.size() - 1);
    int index = dist(m_randomEngine);
    return endpoints[index].id;
}

QString LoadBalancingStrategies::weightedRandomSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    double totalWeight = 0.0;
    for (const ModelEndpoint& ep : endpoints) {
        totalWeight += m_serverWeights.value(ep.id).weight;
    }
    
    std::uniform_real_distribution<> dist(0.0, totalWeight);
    double random = dist(m_randomEngine);
    double cumulative = 0.0;
    
    for (const ModelEndpoint& ep : endpoints) {
        cumulative += m_serverWeights.value(ep.id).weight;
        if (random <= cumulative) {
            return ep.id;
        }
    }
    
    return endpoints.last().id;
}

// ============ Connection-Based ============

QString LoadBalancingStrategies::leastConnectionsSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    QString bestId;
    int minConnections = INT_MAX;
    
    for (const ModelEndpoint& ep : endpoints) {
        const LoadBalancingStats& stats = m_statistics.value(ep.id);
        if (stats.activeConnections < minConnections) {
            minConnections = stats.activeConnections;
            bestId = ep.id;
        }
    }
    
    return bestId.isEmpty() ? endpoints.first().id : bestId;
}

QString LoadBalancingStrategies::leastResponseTimeSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    QString bestId;
    double minResponseTime = std::numeric_limits<double>::max();
    
    for (const ModelEndpoint& ep : endpoints) {
        const LoadBalancingStats& stats = m_statistics.value(ep.id);
        if (stats.averageResponseTime < minResponseTime && stats.requestCount > 0) {
            minResponseTime = stats.averageResponseTime;
            bestId = ep.id;
        }
    }
    
    return bestId.isEmpty() ? endpoints.first().id : bestId;
}

QString LoadBalancingStrategies::leastLoadSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    QString bestId;
    double minLoad = std::numeric_limits<double>::max();
    
    for (const ModelEndpoint& ep : endpoints) {
        const LoadBalancingStats& stats = m_statistics.value(ep.id);
        if (stats.currentLoad < minLoad) {
            minLoad = stats.currentLoad;
            bestId = ep.id;
        }
    }
    
    return bestId.isEmpty() ? endpoints.first().id : bestId;
}

// ============ Power of Two Choices ============

QString LoadBalancingStrategies::powerOfTwoChoicesSelect(const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    if (endpoints.size() == 1) return endpoints.first().id;
    
    // Randomly select two endpoints
    std::uniform_int_distribution<> dist(0, endpoints.size() - 1);
    int idx1 = dist(m_randomEngine);
    int idx2 = dist(m_randomEngine);
    
    // Ensure they're different
    while (idx1 == idx2 && endpoints.size() > 1) {
        idx2 = dist(m_randomEngine);
    }
    
    const QString& ep1 = endpoints[idx1].id;
    const QString& ep2 = endpoints[idx2].id;
    
    // Pick the better one (least connections)
    return isBetterEndpoint(ep1, ep2) ? ep1 : ep2;
}

// ============ Consistent Hashing ============

void LoadBalancingStrategies::initializeHashRing(const QList<QString>& endpointIds, int virtualNodes) {
    QMutexLocker locker(&m_mutex);
    
    m_hashRing.clear();
    m_virtualNodeHashes.clear();
    
    for (const QString& endpointId : endpointIds) {
        addToHashRing(endpointId, virtualNodes);
    }
    
    // Sort the ring
    std::sort(m_hashRing.begin(), m_hashRing.end());
    
    m_hashRingInitialized = true;
    
    qDebug() << "Hash ring initialized with" << endpointIds.size() << "servers and" 
             << m_hashRing.size() << "total nodes";
}

QString LoadBalancingStrategies::consistentHashSelect(const QString& key) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_hashRingInitialized || m_hashRing.isEmpty()) {
        qWarning() << "Hash ring not initialized";
        return QString();
    }
    
    uint32_t hash = hashString(key);
    
    // Binary search for the first node >= hash
    auto it = std::lower_bound(m_hashRing.begin(), m_hashRing.end(), HashRingNode{"", hash},
                               [](const HashRingNode& a, const HashRingNode& b) {
                                   return a.hash < b.hash;
                               });
    
    if (it == m_hashRing.end()) {
        // Wrap around to the beginning
        return m_hashRing.first().endpointId;
    }
    
    return it->endpointId;
}

void LoadBalancingStrategies::addToHashRing(const QString& endpointId, int virtualNodes) {
    QList<uint32_t> hashes;
    
    for (int i = 0; i < virtualNodes; ++i) {
        QString virtualNodeName = QString("%1-vnode-%2").arg(endpointId).arg(i);
        uint32_t hash = hashString(virtualNodeName);
        
        HashRingNode node;
        node.endpointId = endpointId;
        node.hash = hash;
        
        m_hashRing.append(node);
        hashes.append(hash);
    }
    
    m_virtualNodeHashes[endpointId] = hashes;
}

void LoadBalancingStrategies::removeFromHashRing(const QString& endpointId) {
    if (!m_virtualNodeHashes.contains(endpointId)) return;
    
    QList<uint32_t> hashes = m_virtualNodeHashes[endpointId];
    
    // Remove all virtual nodes for this endpoint
    m_hashRing.erase(std::remove_if(m_hashRing.begin(), m_hashRing.end(),
                                   [&endpointId](const HashRingNode& node) {
                                       return node.endpointId == endpointId;
                                   }), m_hashRing.end());
    
    m_virtualNodeHashes.remove(endpointId);
}

// ============ IP Hash ============

QString LoadBalancingStrategies::ipHashSelect(const QString& clientIp, const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    
    uint32_t hash = hashString(clientIp);
    int index = hash % endpoints.size();
    
    return endpoints[index].id;
}

// ============ ML-Based Adaptive Routing ============

void LoadBalancingStrategies::initializeMLModel(int inputFeatures, int hiddenNeurons) {
    QMutexLocker locker(&m_mutex);
    
    m_mlModel = std::make_unique<SimpleNeuralNetwork>(inputFeatures, hiddenNeurons, 1);
    m_mlModelInitialized = true;
    
    qDebug() << "ML model initialized with" << inputFeatures << "inputs and" 
             << hiddenNeurons << "hidden neurons";
}

QString LoadBalancingStrategies::mlBasedSelect(const RoutingRequest& request, 
                                              const QList<ModelEndpoint>& endpoints) {
    if (endpoints.isEmpty()) return QString();
    if (!m_mlModelInitialized) {
        qWarning() << "ML model not initialized, falling back to round-robin";
        return roundRobinSelect(endpoints);
    }
    
    QString bestId;
    double bestPrediction = std::numeric_limits<double>::max();
    
    for (const ModelEndpoint& ep : endpoints) {
        MLFeatures features = extractFeatures(request, ep);
        double prediction = predictResponseTime(features);
        
        if (prediction < bestPrediction) {
            bestPrediction = prediction;
            bestId = ep.id;
        }
    }
    
    return bestId.isEmpty() ? endpoints.first().id : bestId;
}

void LoadBalancingStrategies::trainMLModel(const QString& endpointId, const RoutingRequest& request,
                                          const ModelEndpoint& endpoint, double responseTime, bool success) {
    if (!m_mlModelInitialized) return;
    
    QMutexLocker locker(&m_mutex);
    
    MLFeatures features = extractFeatures(request, endpoint);
    QVector<double> input = features.toVector();
    
    // Normalize response time (target)
    double normalizedResponseTime = normalizeValue(responseTime, 0, 5000);  // Assume max 5000ms
    
    m_mlModel->train(input, normalizedResponseTime, 0.01);
}

MLFeatures LoadBalancingStrategies::extractFeatures(const RoutingRequest& request, 
                                                    const ModelEndpoint& endpoint) const {
    MLFeatures features;
    
    // Request size estimation (based on token count)
    features.requestSize = request.maxTokens / 1000.0;  // Normalize to KB
    
    // Historical latency
    const LoadBalancingStats& stats = m_statistics.value(endpoint.id);
    features.historicalLatency = normalizeValue(stats.averageResponseTime, 0, 5000);
    
    // Current load
    features.currentLoad = stats.currentLoad;
    
    // Time of day (0-1)
    QTime currentTime = QTime::currentTime();
    features.timeOfDay = (currentTime.hour() * 3600 + currentTime.minute() * 60 + currentTime.second()) / 86400.0;
    
    // Day of week (0-1)
    features.dayOfWeek = QDate::currentDate().dayOfWeek() / 7.0;
    
    // Queue depth
    features.queueDepth = stats.activeConnections;
    
    // Error rate
    features.errorRate = stats.requestCount > 0 ? 
        static_cast<double>(stats.failureCount) / stats.requestCount : 0.0;
    
    return features;
}

// ============ Statistics ============

void LoadBalancingStrategies::recordRequest(const QString& endpointId, double responseTime, bool success) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_statistics.contains(endpointId)) {
        LoadBalancingStats stats;
        stats.endpointId = endpointId;
        m_statistics[endpointId] = stats;
    }
    
    LoadBalancingStats& stats = m_statistics[endpointId];
    stats.requestCount++;
    
    if (success) {
        stats.successCount++;
    } else {
        stats.failureCount++;
    }
    
    // Update average response time
    double oldAvg = stats.averageResponseTime;
    stats.averageResponseTime = ((oldAvg * (stats.requestCount - 1)) + responseTime) / stats.requestCount;
    stats.lastUpdated = QDateTime::currentDateTime();
}

void LoadBalancingStrategies::updateEndpointLoad(const QString& endpointId, double cpuLoad, double memoryLoad) {
    QMutexLocker locker(&m_mutex);
    
    if (m_statistics.contains(endpointId)) {
        // Combined load metric
        m_statistics[endpointId].currentLoad = (cpuLoad + memoryLoad) / 2.0;
        m_statistics[endpointId].lastUpdated = QDateTime::currentDateTime();
        
        emit loadChanged(endpointId, m_statistics[endpointId].currentLoad);
    }
}

LoadBalancingStats LoadBalancingStrategies::getEndpointStats(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    return m_statistics.value(endpointId);
}

QMap<QString, LoadBalancingStats> LoadBalancingStrategies::getAllStats() const {
    QMutexLocker locker(&m_mutex);
    return m_statistics;
}

void LoadBalancingStrategies::resetStatistics() {
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_statistics.begin(); it != m_statistics.end(); ++it) {
        it.value().requestCount = 0;
        it.value().successCount = 0;
        it.value().failureCount = 0;
        it.value().averageResponseTime = 0;
        it.value().activeConnections = 0;
    }
    
    qDebug() << "Statistics reset";
}

// ============ Health Scoring ============

double LoadBalancingStrategies::calculateHealthScore(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_statistics.contains(endpointId)) return 0.0;
    
    const LoadBalancingStats& stats = m_statistics[endpointId];
    
    if (stats.requestCount == 0) return 1.0;
    
    double successRate = static_cast<double>(stats.successCount) / stats.requestCount;
    return successRate;
}

double LoadBalancingStrategies::calculateLoadScore(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_statistics.contains(endpointId)) return 1.0;
    
    const LoadBalancingStats& stats = m_statistics[endpointId];
    
    // Lower load = higher score
    return 1.0 - stats.currentLoad;
}

double LoadBalancingStrategies::calculateCombinedScore(const QString& endpointId) const {
    double healthScore = calculateHealthScore(endpointId);
    double loadScore = calculateLoadScore(endpointId);
    
    // Weighted combination
    return (healthScore * 0.6) + (loadScore * 0.4);
}

// ============ Configuration ============

QJsonObject LoadBalancingStrategies::saveConfiguration() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject config;
    config["algorithm"] = static_cast<int>(m_algorithm);
    
    QJsonObject weightsObj;
    for (auto it = m_serverWeights.begin(); it != m_serverWeights.end(); ++it) {
        weightsObj[it.key()] = it.value().weight;
    }
    config["serverWeights"] = weightsObj;
    
    if (m_mlModelInitialized && m_mlModel) {
        config["mlModel"] = m_mlModel->saveWeights();
    }
    
    return config;
}

bool LoadBalancingStrategies::loadConfiguration(const QJsonObject& config) {
    QMutexLocker locker(&m_mutex);
    
    m_algorithm = static_cast<LoadBalancingAlgorithm>(config["algorithm"].toInt());
    
    QJsonObject weightsObj = config["serverWeights"].toObject();
    for (auto it = weightsObj.begin(); it != weightsObj.end(); ++it) {
        QString endpointId = it.key();
        double weight = it.value().toDouble();
        
        // Unlock to call addServer
        m_mutex.unlock();
        addServer(endpointId, weight);
        m_mutex.lock();
    }
    
    if (config.contains("mlModel")) {
        if (!m_mlModelInitialized) {
            m_mutex.unlock();
            initializeMLModel();
            m_mutex.lock();
        }
        m_mlModel->loadWeights(config["mlModel"].toObject());
    }
    
    qDebug() << "Configuration loaded";
    
    return true;
}

// ============ Private Helper Functions ============

uint32_t LoadBalancingStrategies::hashString(const QString& str) const {
    QByteArray data = str.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    
    // Use first 4 bytes as uint32
    uint32_t result = 0;
    for (int i = 0; i < 4 && i < hash.size(); ++i) {
        result = (result << 8) | static_cast<unsigned char>(hash[i]);
    }
    
    return result;
}

uint32_t LoadBalancingStrategies::hashCombine(uint32_t hash1, uint32_t hash2) const {
    return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
}

void LoadBalancingStrategies::updateWeights() {
    for (auto it = m_serverWeights.begin(); it != m_serverWeights.end(); ++it) {
        it.value().currentWeight += it.value().effectiveWeight;
    }
}

QString LoadBalancingStrategies::selectWeightedRoundRobinInternal() {
    if (m_serverWeights.isEmpty()) return QString();
    
    updateWeights();
    
    // Find server with highest current weight
    QString bestId;
    int maxWeight = INT_MIN;
    
    for (auto it = m_serverWeights.begin(); it != m_serverWeights.end(); ++it) {
        if (it.value().currentWeight > maxWeight) {
            maxWeight = it.value().currentWeight;
            bestId = it.key();
        }
    }
    
    if (!bestId.isEmpty()) {
        // Calculate total effective weight
        int totalWeight = 0;
        for (const ServerWeight& sw : m_serverWeights) {
            totalWeight += sw.effectiveWeight;
        }
        
        // Decrease selected server's current weight
        m_serverWeights[bestId].currentWeight -= totalWeight;
    }
    
    return bestId;
}

double LoadBalancingStrategies::normalizeValue(double value, double min, double max) const {
    if (max <= min) return 0.0;
    return std::max(0.0, std::min(1.0, (value - min) / (max - min)));
}

double LoadBalancingStrategies::predictResponseTime(const MLFeatures& features) {
    if (!m_mlModelInitialized) return 0.0;
    
    QVector<double> input = features.toVector();
    QVector<double> output = m_mlModel->forward(input);
    
    return output.isEmpty() ? 0.0 : output.first() * 5000.0;  // Denormalize
}

bool LoadBalancingStrategies::isBetterEndpoint(const QString& ep1, const QString& ep2) const {
    const LoadBalancingStats& stats1 = m_statistics.value(ep1);
    const LoadBalancingStats& stats2 = m_statistics.value(ep2);
    
    // Compare based on active connections
    if (stats1.activeConnections != stats2.activeConnections) {
        return stats1.activeConnections < stats2.activeConnections;
    }
    
    // If equal, compare based on response time
    return stats1.averageResponseTime < stats2.averageResponseTime;
}
