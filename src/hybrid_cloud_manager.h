#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QQueue>
#include <memory>

struct CloudModel {
    QString modelId;
    QString name;
    QString provider; // "aws", "azure", "gcp", "huggingface"
    QString type; // "completion", "chat", "embedding"
    qint64 size;
    double costPerToken;
    double latency;
    bool isAvailable;
    QString endpoint;
    QString apiKey;
    QJsonObject capabilities;
    QJsonObject restrictions;
};

struct CloudCredentials {
    QString provider;
    QString accessKey;
    QString secretKey;
    QString region;
    QString endpoint;
    bool isValid;
    QDateTime expiryTime;
};

struct HybridExecution {
    QString executionId;
    QString localModelId;
    QString cloudModelId;
    QString executionType; // "completion", "chat", "analysis"
    QJsonObject request;
    QJsonObject response;
    qint64 localLatency;
    qint64 cloudLatency;
    double localCost;
    double cloudCost;
    bool wasSuccessful;
    QDateTime timestamp;
    qint64 totalTimeMs;
};

class HybridCloudManager : public QObject {
    Q_OBJECT
    
private:
    QNetworkAccessManager* networkManager;
    QTimer* healthCheckTimer;
    QTimer* costOptimizationTimer;
    QTimer* syncTimer;
    
    QVector<CloudModel> availableCloudModels;
    QVector<CloudModel> connectedCloudModels;
    QHash<QString, CloudCredentials> cloudCredentials;
    QQueue<HybridExecution> executionHistory;
    
    // Configuration
    bool enableHybridOperation = true;
    bool enableCloudFailover = true;
    bool enableCostOptimization = true;
    bool enableAutoScaling = true;
    bool enableEnterpriseFeatures = true;
    
    QString defaultCloudProvider = "huggingface";
    double maxCloudCostPerRequest = 0.01; // $0.01 per request
    qint64 maxCloudLatency = 2000; // 2 seconds
    int maxConcurrentCloudRequests = 10;
    double cloudUsageThreshold = 0.8; // 80% usage before switching
    
    // Enterprise features
    QString enterpriseAccountId;
    QString teamId;
    QString department;
    QString complianceLevel; // "standard", "enterprise", "government"

public:
    HybridCloudManager(QObject* parent = nullptr);
    ~HybridCloudManager();
    
    // Hybrid operation management
    HybridExecution executeHybrid(const QJsonObject& request, const QString& executionType);
    bool switchToCloud(const QString& reason);
    bool switchToLocal(const QString& reason);
    bool isHybridEnabled() const { return enableHybridOperation; }
    
    // Cloud model management
    bool connectToCloudProvider(const QString& provider, const CloudCredentials& credentials);
    bool disconnectFromCloudProvider(const QString& provider);
    bool refreshCloudModels();
    QVector<CloudModel> getAvailableCloudModels();
    QVector<CloudModel> getConnectedCloudModels();
    
    // Intelligent model selection
    CloudModel selectOptimalCloudModel(const QString& taskType, const QJsonObject& requirements);
    CloudModel selectCostEffectiveModel(const QString& taskType, double budget);
    CloudModel selectFastestModel(const QString& taskType, qint64 maxLatency);
    CloudModel selectEnterpriseModel(const QString& taskType, const QString& compliance);
    
    // Cost and performance optimization
    double calculateHybridCost(const HybridExecution& execution);
    qint64 estimateHybridLatency(const QJsonObject& request);
    bool optimizeForCost(const QString& taskType, double budget);
    bool optimizeForPerformance(const QString& taskType, qint64 maxLatency);
    bool optimizeForCompliance(const QString& taskType, const QString& compliance);
    
    // Failover and recovery
    bool enableCloudFailoverMode(bool enable);
    bool handleCloudFailure(const QString& provider, const QString& error);
    bool recoverFromCloudFailure(const QString& provider);
    bool validateCloudHealth(const QString& provider);
    
    // Enterprise integration
    bool setupEnterpriseAccount(const QString& accountId, const QString& teamId);
    bool configureTeamCollaboration(const QString& teamId);
    bool enableComplianceMode(const QString& complianceLevel);
    bool integrateWithEnterpriseSSO(const QString& ssoProvider);
    bool setupAuditLogging(const QString& auditEndpoint);
    
    // Synchronization and backup
    bool syncSettingsAcrossDevices(const QString& userId);
    bool backupModelsToCloud(const QString& backupLocation);
    bool restoreModelsFromCloud(const QString& backupLocation);
    bool synchronizeTeamModels(const QString& teamId);
    
    // Real-time monitoring
    QJsonObject getHybridMetrics() const;
    QJsonObject getCloudHealthStatus() const;
    QJsonObject getCostAnalytics() const;
    QJsonObject getPerformanceAnalytics() const;
    
    // Machine learning integration
    bool learnFromExecutionHistory(const QVector<HybridExecution>& history);
    bool predictOptimalConfiguration(const QString& usagePattern);
    bool adaptToUserBehavior(const QJsonObject& userBehavior);
    bool optimizeForTeamUsage(const QString& teamId);

signals:
    void cloudConnectionEstablished(const QString& provider);
    void cloudConnectionLost(const QString& provider, const QString& reason);
    void hybridExecutionCompleted(const HybridExecution& execution);
    void cloudCostUpdated(double totalCost);
    void cloudHealthStatusChanged(bool isHealthy);
    void modelRecommendationReady(const CloudModel& model);
    void enterpriseFeatureEnabled(const QString& feature);
    void hybridMetricsUpdated(const QJsonObject& metrics);
    void modelsUpdated();

private:
    void setupNetworkManager();
    void setupTimers();
    void loadDefaultCloudProviders();
    void setupEnterpriseIntegration();
    
    // Core cloud functions
    bool authenticateWithCloudProvider(const QString& provider, const CloudCredentials& credentials);
    bool fetchCloudModels(const QString& provider);
    bool validateCloudModel(const CloudModel& model);
    bool executeCloudRequest(const QJsonObject& request, const CloudModel& model);
    
    // Intelligent decision functions
    bool shouldUseCloud(const QJsonObject& request);
    CloudModel selectBestCloudModel(const QString& taskType, const QJsonObject& requirements);
    double calculateExecutionCost(const QJsonObject& request, const CloudModel& model);
    qint64 estimateExecutionLatency(const QJsonObject& request, const CloudModel& model);
    double calculateModelSuitability(const CloudModel& model, const QJsonObject& requirements);
    
    // Enterprise utility functions
    bool checkEnterpriseCompliance(const QJsonObject& request);
    bool validateEnterpriseSecurity(const QJsonObject& request);
    bool auditEnterpriseUsage(const QJsonObject& request);
    bool reportEnterpriseMetrics(const QJsonObject& metrics);
    
    // Helper functions
    void performHealthCheck();
    void optimizeCosts();
    void syncSettings();
    void onNetworkReply(QNetworkReply* reply);
    QString generateExecutionId() const;
    qint64 getCurrentSystemLoad() const;
    CloudModel getCheapestAvailableModel(const QString& taskType) const;
    double estimateExecutionCost(const QJsonObject& request, const CloudModel& model) const;
    qint64 estimateHybridLatency(const QJsonObject& request) const;
    QJsonObject buildCloudRequest(const QJsonObject& request, const CloudModel& model);
};
