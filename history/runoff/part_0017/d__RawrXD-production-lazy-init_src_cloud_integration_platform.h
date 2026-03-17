// Enterprise-Grade Cloud Integration Platform
// Multi-cloud support with unified API for AWS, Azure, GCP
#pragma once

#include "cloud_config.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>

// Import enterprise types from agentic_executor
#include "agentic_executor.h"

struct ContainerImage {
    QString imageName;
    QString tag;
    QString registry;
    QString digest;
    int sizeBytes;
    QString createdAt;
    bool isMultiArch;
};

struct KubernetesManifest {
    QString apiVersion;
    QString kind; // Deployment, Service, ConfigMap, etc.
    QString name;
    QString k8sNamespace; // Changed from 'namespace' (C++ keyword)
    QJsonObject spec;
    QMap<QString, QString> labels;
    QMap<QString, QString> annotations;
};

struct CloudMonitoringConfig {
    QString metricsNamespace;
    QVector<QString> enabledMetrics;
    QVector<QString> customMetrics;
    int metricsRetentionDays = 30;
    bool enableDistributedTracing = true;
    bool enableLogging = true;
    bool enableAnomalyDetection = true;
};

struct AutoScalingPolicy {
    QString metricName;
    double targetValue;
    int minInstances = 1;
    int maxInstances = 10;
    int scaleUpThreshold = 80;
    int scaleDownThreshold = 20;
    int cooldownSeconds = 300;
};

struct LoadBalancerConfig {
    QString name;
    QString protocol; // HTTP, HTTPS, TCP
    int port;
    int targetPort;
    QString healthCheckPath = "/health";
    int healthCheckIntervalSeconds = 30;
    bool stickySession = false;
    QString sessionAffinity; // ClientIP, None
};

struct DisasterRecoveryConfig {
    bool enabled = true;
    QString backupFrequency; // daily, weekly, hourly
    int retentionDays = 30;
    QString primaryRegion;
    QString secondaryRegion;
    bool crossRegionReplication = true;
    int rpoMinutes = 15; // Recovery Point Objective
    int rtoMinutes = 60;  // Recovery Time Objective
};

// ========== AWS PROVIDER ==========

class AWSProvider : public QObject {
    Q_OBJECT

public:
    explicit AWSProvider(const CloudConfig& config, QObject* parent = nullptr);
    ~AWSProvider();

    // Authentication
    bool authenticate(const QString& accessKey, const QString& secretKey);
    bool authenticateWithIAMRole();
    bool authenticateWithSTS(const QString& roleArn);

    // Container Management (ECR)
    bool createRepository(const QString& repositoryName);
    bool pushImage(const ContainerImage& image);
    bool pullImage(const ContainerImage& image);
    QVector<ContainerImage> listImages(const QString& repositoryName);
    bool deleteImage(const QString& imageDigest);

    // Deployment (ECS & EKS)
    bool createECSCluster(const QString& clusterName);
    bool createECSService(const DeploymentConfig& config);
    bool updateECSService(const QString& serviceName, const DeploymentConfig& config);
    bool deleteECSService(const QString& serviceName);
    QString getECSServiceStatus(const QString& serviceName);

    bool createEKSCluster(const QString& clusterName, int nodeCount);
    bool deployKubernetesManifest(const KubernetesManifest& manifest);
    bool updateKubernetesDeployment(const QString& deploymentName, const QString& image);
    bool scaleKubernetesDeployment(const QString& deploymentName, int replicas);

    // Auto Scaling
    bool configureAutoScaling(const AutoScalingPolicy& policy);
    bool createLaunchTemplate(const DeploymentConfig& config);

    // Load Balancing
    bool createLoadBalancer(const LoadBalancerConfig& config);
    bool registerTarget(const QString& targetName, const QString& lbName);
    QString getLoadBalancerDNS(const QString& lbName);

    // Networking
    bool configureVPC(const QString& vpcName, const QString& cidrBlock);
    bool createSecurityGroup(const QString& groupName, const QString& vpcName);
    bool addSecurityGroupRule(const QString& groupName, const QString& protocol, int port);

    // Storage
    bool createS3Bucket(const QString& bucketName);
    bool uploadToS3(const QString& bucketName, const QString& filePath);
    bool downloadFromS3(const QString& bucketName, const QString& key, const QString& localPath);
    bool enableS3Versioning(const QString& bucketName);
    bool configureCrossRegionReplication(const QString& bucketName, const QString& targetRegion);

    // Disaster Recovery
    bool enableBackup(const DisasterRecoveryConfig& config);
    bool createBackup(const QString& resourceId);
    bool restoreFromBackup(const QString& backupId);
    bool enableCrossRegionFailover(const DisasterRecoveryConfig& config);

    // Monitoring & Logging
    bool enableCloudWatch();
    bool createMetricAlarm(const QString& alarmName, const QString& metricName, double threshold);
    QVector<QString> getMetrics(const QString& resourceId);
    QString getLogs(const QString& logGroupName, int lines = 100);

    // Cost Optimization
    QJsonObject estimateCosts(const DeploymentConfig& config);
    QVector<QString> getUnusedResources();
    bool optimizeResourceAllocation();

signals:
    void authenticationSucceeded();
    void authenticationFailed(QString error);
    void deploymentStarted(QString applicationName);
    void deploymentCompleted(QString applicationName);
    void deploymentFailed(QString applicationName, QString error);
    void metricsUpdated(QString resource, QJsonObject metrics);
    void alarmTriggered(QString alarmName);

private:
    CloudConfig m_config;
    QString m_accessToken;
    bool m_authenticated = false;

    QString buildAwsRequestSignature(const QString& method, const QString& path, const QJsonObject& params);
};

// ========== AZURE PROVIDER ==========

class AzureProvider : public QObject {
    Q_OBJECT

public:
    explicit AzureProvider(const CloudConfig& config, QObject* parent = nullptr);
    ~AzureProvider();

    // Authentication
    bool authenticate(const QString& clientId, const QString& clientSecret, const QString& tenantId);
    bool authenticateManagedIdentity();

    // Container Management (ACR)
    bool createRegistry(const QString& registryName);
    bool pushImage(const ContainerImage& image);
    bool pullImage(const ContainerImage& image);
    QVector<ContainerImage> listImages(const QString& registryName);

    // Deployment (AKS & App Service)
    bool createAKSCluster(const QString& clusterName, int nodeCount);
    bool deployContainerInstance(const DeploymentConfig& config);
    bool deployAppService(const DeploymentConfig& config);
    bool updateDeployment(const QString& deploymentName, const DeploymentConfig& config);

    // Kubernetes Management
    bool deployKubernetesManifest(const KubernetesManifest& manifest);
    bool scaleDeployment(const QString& deploymentName, int replicas);

    // Load Balancing
    bool createApplicationGateway(const LoadBalancerConfig& config);
    bool configureLoadBalancer(const LoadBalancerConfig& config);

    // Networking
    bool createVirtualNetwork(const QString& vnetName, const QString& addressSpace);
    bool createNetworkSecurityGroup(const QString& groupName);
    bool addNetworkSecurityRule(const QString& groupName, const QString& protocol, int port);

    // Storage
    bool createStorageAccount(const QString& accountName);
    bool uploadBlob(const QString& accountName, const QString& containerName, const QString& filePath);
    bool downloadBlob(const QString& accountName, const QString& containerName, const QString& blobName, const QString& localPath);
    bool enableGeographicReplication(const QString& accountName);

    // Disaster Recovery
    bool enableBackup(const DisasterRecoveryConfig& config);
    bool configureGeoRedundantStorage(const QString& accountName);
    bool createRecoveryVault(const QString& vaultName);

    // Monitoring
    bool enableApplicationInsights(const QString& appName);
    bool createMetricAlert(const QString& alertName, const QString& metricName, double threshold);
    QVector<QString> queryLogs(const QString& kuqlQuery);

signals:
    void authenticationSucceeded();
    void deploymentCompleted(QString applicationName);
    void monitoringDataCollected(QJsonObject data);

private:
    CloudConfig m_config;
    QString m_accessToken;
};

// ========== GCP PROVIDER ==========

class GCPProvider : public QObject {
    Q_OBJECT

public:
    explicit GCPProvider(const CloudConfig& config, QObject* parent = nullptr);
    ~GCPProvider();

    // Authentication
    bool authenticate(const QString& serviceAccountKeyPath);
    bool authenticateWithOAuth(const QString& clientId, const QString& clientSecret);

    // Container Management (Artifact Registry)
    bool createRepository(const QString& repositoryName);
    bool pushImage(const ContainerImage& image);
    bool pullImage(const ContainerImage& image);
    QVector<ContainerImage> listImages(const QString& repositoryName);

    // Deployment (GKE, Cloud Run, App Engine)
    bool createGKECluster(const QString& clusterName, int nodeCount);
    bool deployCloudRun(const DeploymentConfig& config);
    bool deployAppEngine(const DeploymentConfig& config);
    bool updateCloudRunService(const QString& serviceName, const DeploymentConfig& config);

    // Kubernetes Management
    bool deployKubernetesManifest(const KubernetesManifest& manifest);
    bool configureClusterAutoscaling(const AutoScalingPolicy& policy);

    // Load Balancing
    bool createLoadBalancer(const LoadBalancerConfig& config);
    bool createCloudCDN(const QString& lbName);

    // Storage
    bool createBucket(const QString& bucketName);
    bool uploadToGCS(const QString& bucketName, const QString& filePath);
    bool downloadFromGCS(const QString& bucketName, const QString& objectName, const QString& localPath);
    bool enableUniformBucketLevelAccess(const QString& bucketName);

    // Disaster Recovery
    bool enableBackup(const DisasterRecoveryConfig& config);
    bool configureCloudSQL(const QString& instanceName);
    bool enableAutomaticFailover(const QString& instanceName);

    // Monitoring
    bool enableCloudMonitoring();
    bool createAlertPolicy(const QString& policyName, const QString& metricName, double threshold);
    QVector<QString> getTimeSeriesMetrics(const QString& metricType);

    // Serverless
    bool deployCloudFunction(const QString& functionName, const QString& sourceCode);
    bool deployCloudScheduler(const QString& jobName, const QString& schedule, const QString& target);

signals:
    void deploymentSucceeded(QString service);
    void deploymentFailed(QString service, QString error);

private:
    CloudConfig m_config;
    QString m_accessToken;
};

// ========== CLOUD ORCHESTRATOR ==========

class CloudOrchestrator : public QObject {
    Q_OBJECT

public:
    explicit CloudOrchestrator(QObject* parent = nullptr);
    ~CloudOrchestrator();

    // Initialization
    void initialize(const CloudConfig& config);
    void addProvider(CloudProvider provider, const CloudConfig& config);

    // Multi-Cloud Operations
    bool deployMultiCloud(const DeploymentConfig& config, const QVector<CloudProvider>& providers);
    bool updateMultiCloudDeployment(const QString& appName, const DeploymentConfig& config);
    int getDeploymentCountAcrossClouds(const QString& appName);

    // Deployment Strategies
    bool executeBlueGreenDeployment(const DeploymentConfig& config);
    bool executeCanaryDeployment(const DeploymentConfig& config, int canaryPercentage);
    bool executeRollingDeployment(const DeploymentConfig& config, int batchSize);

    // Failover & High Availability
    bool configureActiveActiveFailover(const QString& region1, const QString& region2);
    bool configureActivePrimaryFailover(const QString& primaryRegion, const QString& secondaryRegion);
    bool triggerManualFailover(const QString& applicationName);

    // Cost Optimization
    QJsonObject analyzeMultiCloudCosts();
    QString recommendCostOptimizations();
    bool enableSpotInstances(double savingsThreshold);

    // Monitoring Across Clouds
    QJsonObject getAggregatedMetrics();
    QString generateUnifiedDashboard();
    bool configureUnifiedLogging();

    // CI/CD Integration
    bool integrateCICD(const QString& cicdProvider, const QString& accessToken);
    bool setupAutomaticDeployment(const QString& repositoryUrl, const QString& branch);

signals:
    void deploymentInitiated(QString applicationName);
    void deploymentSucceeded(QString applicationName, CloudProvider provider);
    void deploymentFailed(QString applicationName, CloudProvider provider, QString error);
    void failoverTriggered(QString applicationName);
    void metricsCollected(QJsonObject aggregatedMetrics);

private:
    QMap<CloudProvider, std::unique_ptr<QObject>> m_providers;
    CloudConfig m_primaryConfig;
    
    std::unique_ptr<AWSProvider> m_awsProvider;
    std::unique_ptr<AzureProvider> m_azureProvider;
    std::unique_ptr<GCPProvider> m_gcpProvider;

    CloudProvider m_activeProvider;
};

// ========== CI/CD PIPELINE MANAGER ==========

class CICDPipelineManager : public QObject {
    Q_OBJECT

public:
    explicit CICDPipelineManager(const CloudConfig& config, QObject* parent = nullptr);
    ~CICDPipelineManager();

    // Pipeline Configuration
    bool createPipeline(const QString& pipelineName, const QString& repositoryUrl);
    bool addBuildStage(const QString& pipelineName, const QString& stage, const QString& command);
    bool addTestStage(const QString& pipelineName, const QString& testCommand);
    bool addDeploymentStage(const QString& pipelineName, const DeploymentConfig& config);

    // Pipeline Execution
    bool triggerPipeline(const QString& pipelineName);
    bool triggerPipelineWithVersion(const QString& pipelineName, const QString& version);
    QString getPipelineStatus(const QString& pipelineName);
    QVector<QString> getPipelineHistory(const QString& pipelineName);

    // Approval & Gating
    bool requireApprovalStage(const QString& pipelineName, const QString& stage);
    bool approveDeployment(const QString& pipelineExecutionId);
    bool rejectDeployment(const QString& pipelineExecutionId, const QString& reason);

    // Artifact Management
    bool publishArtifact(const QString& artifactName, const QString& filePath);
    bool promoteArtifact(const QString& artifactName, const QString& fromEnv, const QString& toEnv);

signals:
    void pipelineCreated(QString pipelineName);
    void pipelineStarted(QString pipelineName);
    void pipelineSucceeded(QString pipelineName);
    void pipelineFailed(QString pipelineName, QString error);
    void approvalRequested(QString pipelineExecutionId);

private:
    CloudConfig m_config;
    QMap<QString, QVector<QString>> m_pipelineStages;
};

// ========== CLOUD UTILITIES ==========

class CloudUtils {
public:
    static QString getProviderString(CloudProvider provider);
    static CloudProvider getProviderFromString(const QString& providerString);
    static QString getRegionFromLatLong(double lat, double lon);
    static QVector<QString> getAvailableRegions(CloudProvider provider);
    static QString selectOptimalRegion(CloudProvider provider, const QVector<QString>& requirements);
};
