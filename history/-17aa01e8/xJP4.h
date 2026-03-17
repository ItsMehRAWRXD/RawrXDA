#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <memory>

struct ModelConfig;
struct GenerationOptions;
struct GenerationResult;

class ModelInterface : public QObject {
    Q_OBJECT

public:
    explicit ModelInterface(QObject *parent = nullptr);
    ~ModelInterface();

    // Model registration and management
    void registerModel(const QString &modelName, const struct ModelConfig &config);
    QList<QString> getAvailableModels() const;
    
    // Model selection strategies
    QString selectBestModel(const QString &taskType, const QString &inputHint, bool prioritizeSpeed = false);
    QString selectFastestModel(const QString &taskType);
    QString selectCostOptimalModel(const QString &taskType, double maxCostPerRequest);
    
    // Core inference
    struct GenerationResult generate(const QString &modelName, const QString &prompt, 
                                      const struct GenerationOptions &options);
    
    // Configuration
    bool loadConfig(const QString &configPath);
    
    // Monitoring
    QJsonObject getUsageStatistics() const;
    double getAverageLatency(const QString &modelName) const;
    int getSuccessRate(const QString &modelName) const;
    double getTotalCost() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

// Supporting structures
struct ModelConfig {
    QString modelPath;
    QString modelType;
};

struct GenerationOptions {
    int maxTokens = 2048;
    float temperature = 0.7f;
    float topP = 0.95f;
};

struct GenerationResult {
    bool success = false;
    QString output;
    int tokensUsed = 0;
};
