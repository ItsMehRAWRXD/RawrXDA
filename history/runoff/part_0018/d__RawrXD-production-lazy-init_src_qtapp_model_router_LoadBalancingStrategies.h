#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QJsonObject>
#include <functional>
#include <memory>
#include <random>

// Forward declaration
struct ModelEndpoint;
struct RoutingRequest;

/**
 * @brief Load balancing algorithm types
 */
enum class LoadBalancingAlgorithm {
    RoundRobin,              // Sequential distribution
    WeightedRoundRobin,      // Weight-based sequential
    Random,                  // Purely random
    WeightedRandom,          // Weight-based random
    LeastConnections,        // Fewest active connections
    LeastResponseTime,       // Fastest average response
    PowerOfTwoChoices,       // Random two, pick better
    ConsistentHashing,       // Hash-based routing
    IpHash,                  // Client IP based
    LeastLoad,               // Lowest CPU/memory load
    AdaptiveMLBased,         // Machine learning prediction
    Custom                   // User-defined
};

/**
 * @brief Server weight configuration
 */
struct ServerWeight {
    QString endpointId;
    double weight;           // Relative weight (0.0-1.0)
    int currentWeight;       // For weighted round-robin
    int effectiveWeight;     // Adjusted by health
    
    ServerWeight() : weight(1.0), currentWeight(0), effectiveWeight(100) {}
};

/**
 * @brief Load balancing statistics
 */
struct LoadBalancingStats {
    QString endpointId;
    qint64 requestCount;
    qint64 successCount;
    qint64 failureCount;
    double averageResponseTime;
    double currentLoad;       // CPU/memory utilization
    int activeConnections;
    QDateTime lastUpdated;
    
    LoadBalancingStats() : requestCount(0), successCount(0), failureCount(0),
                          averageResponseTime(0), currentLoad(0),
                          activeConnections(0) {
        lastUpdated = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief Consistent hashing ring node
 */
struct HashRingNode {
    QString endpointId;
    uint32_t hash;
    
    bool operator<(const HashRingNode& other) const {
        return hash < other.hash;
    }
};

/**
 * @brief Machine learning features for adaptive routing
 */
struct MLFeatures {
    double requestSize;       // Estimated size in KB
    double historicalLatency; // Past latency for similar requests
    double currentLoad;       // Current endpoint load
    double timeOfDay;         // Normalized time (0-1)
    double dayOfWeek;         // Normalized day (0-1)
    int queueDepth;          // Waiting requests
    double errorRate;         // Recent error rate
    
    MLFeatures() : requestSize(0), historicalLatency(0), currentLoad(0),
                  timeOfDay(0), dayOfWeek(0), queueDepth(0), errorRate(0) {}
    
    QVector<double> toVector() const;
};

/**
 * @brief Simple neural network for adaptive routing
 */
class SimpleNeuralNetwork {
public:
    SimpleNeuralNetwork(int inputSize, int hiddenSize, int outputSize);
    
    // Forward pass
    QVector<double> forward(const QVector<double>& input);
    
    // Train on feedback
    void train(const QVector<double>& input, double target, double learningRate = 0.01);
    
    // Save/load weights
    QJsonObject saveWeights() const;
    void loadWeights(const QJsonObject& weights);
    
private:
    double sigmoid(double x);
    double sigmoidDerivative(double x);
    
    int m_inputSize;
    int m_hiddenSize;
    int m_outputSize;
    
    QVector<QVector<double>> m_weightsInputHidden;
    QVector<QVector<double>> m_weightsHiddenOutput;
    QVector<double> m_biasHidden;
    QVector<double> m_biasOutput;
};

/**
 * @brief Advanced Load Balancing Strategies
 * 
 * Implements sophisticated load balancing algorithms including
 * consistent hashing, power-of-two-choices, and ML-based routing.
 */
class LoadBalancingStrategies : public QObject {
    Q_OBJECT
    
public:
    explicit LoadBalancingStrategies(QObject* parent = nullptr);
    ~LoadBalancingStrategies();
    
    // ============ Algorithm Selection ============
    
    /**
     * @brief Set load balancing algorithm
     */
    void setAlgorithm(LoadBalancingAlgorithm algorithm);
    
    /**
     * @brief Get current algorithm
     */
    LoadBalancingAlgorithm getAlgorithm() const { return m_algorithm; }
    
    /**
     * @brief Set custom load balancing function
     */
    void setCustomBalancingFunction(std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> func);
    
    // ============ Server Management ============
    
    /**
     * @brief Add server to load balancer
     */
    void addServer(const QString& endpointId, double weight = 1.0);
    
    /**
     * @brief Remove server from load balancer
     */
    void removeServer(const QString& endpointId);
    
    /**
     * @brief Update server weight
     */
    void updateServerWeight(const QString& endpointId, double weight);
    
    /**
     * @brief Get all servers
     */
    QList<QString> getAllServers() const;
    
    // ============ Load Balancing ============
    
    /**
     * @brief Select endpoint for request
     */
    QString selectEndpoint(const RoutingRequest& request, const QList<ModelEndpoint>& availableEndpoints);
    
    /**
     * @brief Select with health consideration
     */
    QString selectHealthyEndpoint(const RoutingRequest& request, const QList<ModelEndpoint>& endpoints);
    
    // ============ Round Robin Variants ============
    
    /**
     * @brief Simple round-robin selection
     */
    QString roundRobinSelect(const QList<ModelEndpoint>& endpoints);
    
    /**
     * @brief Weighted round-robin selection
     */
    QString weightedRoundRobinSelect(const QList<ModelEndpoint>& endpoints);
    
    // ============ Random Variants ============
    
    /**
     * @brief Random selection
     */
    QString randomSelect(const QList<ModelEndpoint>& endpoints);
    
    /**
     * @brief Weighted random selection
     */
    QString weightedRandomSelect(const QList<ModelEndpoint>& endpoints);
    
    // ============ Connection-Based ============
    
    /**
     * @brief Select endpoint with fewest connections
     */
    QString leastConnectionsSelect(const QList<ModelEndpoint>& endpoints);
    
    /**
     * @brief Select endpoint with best response time
     */
    QString leastResponseTimeSelect(const QList<ModelEndpoint>& endpoints);
    
    /**
     * @brief Select endpoint with lowest load
     */
    QString leastLoadSelect(const QList<ModelEndpoint>& endpoints);
    
    // ============ Power of Two Choices ============
    
    /**
     * @brief Power-of-two-choices algorithm
     * Randomly select two, pick the better one
     */
    QString powerOfTwoChoicesSelect(const QList<ModelEndpoint>& endpoints);
    
    // ============ Consistent Hashing ============
    
    /**
     * @brief Initialize consistent hashing ring
     */
    void initializeHashRing(const QList<QString>& endpointIds, int virtualNodes = 150);
    
    /**
     * @brief Select endpoint using consistent hashing
     */
    QString consistentHashSelect(const QString& key);
    
    /**
     * @brief Add server to hash ring
     */
    void addToHashRing(const QString& endpointId, int virtualNodes = 150);
    
    /**
     * @brief Remove server from hash ring
     */
    void removeFromHashRing(const QString& endpointId);
    
    // ============ IP Hash ============
    
    /**
     * @brief Select based on client IP hash
     */
    QString ipHashSelect(const QString& clientIp, const QList<ModelEndpoint>& endpoints);
    
    // ============ ML-Based Adaptive Routing ============
    
    /**
     * @brief Initialize ML model
     */
    void initializeMLModel(int inputFeatures = 7, int hiddenNeurons = 10);
    
    /**
     * @brief Select endpoint using ML prediction
     */
    QString mlBasedSelect(const RoutingRequest& request, const QList<ModelEndpoint>& endpoints);
    
    /**
     * @brief Train ML model with feedback
     */
    void trainMLModel(const QString& endpointId, const RoutingRequest& request,
                     const ModelEndpoint& endpoint, double responseTime, bool success);
    
    /**
     * @brief Extract ML features from request and endpoint
     */
    MLFeatures extractFeatures(const RoutingRequest& request, const ModelEndpoint& endpoint) const;
    
    // ============ Statistics ============
    
    /**
     * @brief Record request for endpoint
     */
    void recordRequest(const QString& endpointId, double responseTime, bool success);
    
    /**
     * @brief Update endpoint load
     */
    void updateEndpointLoad(const QString& endpointId, double cpuLoad, double memoryLoad);
    
    /**
     * @brief Get statistics for endpoint
     */
    LoadBalancingStats getEndpointStats(const QString& endpointId) const;
    
    /**
     * @brief Get all statistics
     */
    QMap<QString, LoadBalancingStats> getAllStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();
    
    // ============ Health Scoring ============
    
    /**
     * @brief Calculate health score for endpoint
     */
    double calculateHealthScore(const QString& endpointId) const;
    
    /**
     * @brief Calculate load score for endpoint
     */
    double calculateLoadScore(const QString& endpointId) const;
    
    /**
     * @brief Calculate combined score for endpoint
     */
    double calculateCombinedScore(const QString& endpointId) const;
    
    // ============ Configuration ============
    
    /**
     * @brief Save configuration
     */
    QJsonObject saveConfiguration() const;
    
    /**
     * @brief Load configuration
     */
    bool loadConfiguration(const QJsonObject& config);
    
signals:
    /**
     * @brief Emitted when endpoint is selected
     */
    void endpointSelected(const QString& endpointId, LoadBalancingAlgorithm algorithm);
    
    /**
     * @brief Emitted when load changes significantly
     */
    void loadChanged(const QString& endpointId, double newLoad);
    
private:
    // Hashing functions
    uint32_t hashString(const QString& str) const;
    uint32_t hashCombine(uint32_t hash1, uint32_t hash2) const;
    
    // Weighted round-robin helpers
    void updateWeights();
    QString selectWeightedRoundRobinInternal();
    
    // ML helpers
    double normalizeValue(double value, double min, double max) const;
    double predictResponseTime(const MLFeatures& features);
    
    // Comparison functions
    bool isBetterEndpoint(const QString& ep1, const QString& ep2) const;
    
    // Data members
    LoadBalancingAlgorithm m_algorithm;
    std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> m_customBalancingFunction;
    
    // Server tracking
    QMap<QString, ServerWeight> m_serverWeights;
    QMap<QString, LoadBalancingStats> m_statistics;
    
    // Round-robin state
    int m_roundRobinIndex;
    
    // Consistent hashing
    QVector<HashRingNode> m_hashRing;
    QMap<QString, QList<uint32_t>> m_virtualNodeHashes;  // endpointId -> hashes
    bool m_hashRingInitialized;
    
    // ML model
    std::unique_ptr<SimpleNeuralNetwork> m_mlModel;
    bool m_mlModelInitialized;
    
    // Random number generator
    std::mt19937 m_randomEngine;
    
    mutable QMutex m_mutex;
};
