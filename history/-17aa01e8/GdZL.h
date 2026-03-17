#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <QList>
#include <QJsonObject>

// Forward declarations
class AgenticEngine;

// Forward declarations for structs if not in header
struct ModelConfig;
struct GenerationOptions;
struct GenerationResult {
    QString text;
    bool success;
};

/**
 * @brief ModelInterface - Provides unified interface for model operations
 * 
 * This class serves as a bridge between the Qt GUI and the underlying
 * MASM/C++ model loading and execution system.
 */
class ModelInterface {
public:
    ModelInterface(QObject* parent = nullptr) {}
    ~ModelInterface();

    bool loadConfig(const QString& path) { return true; }
    void registerModel(const QString& name, const ModelConfig& config) {}
    QString selectBestModel(const QString& task, const QString& lang, bool local) { return ""; }
    QString selectCostOptimalModel(const QString& prompt, double budget) { return ""; }
    QString selectFastestModel(const QString& task = "") { return ""; }
    
    GenerationResult generate(const QString& prompt, const QString& model, const GenerationOptions& opts) { 
        return { "", false }; 
    }
    
    QList<QString> getAvailableModels() const { return {}; }
    QJsonObject getUsageStatistics() const { return {}; }
    double getAverageLatency(const QString& model) const { return 0.0; }
    int getSuccessRate(const QString& model) const { return 100; }
    double getTotalCost() const { return 0.0; }

    // Model loading and management
    bool loadModel(const QString& modelPath);
    bool unloadModel();
    bool isModelLoaded() const;
    
    // Model execution
    QJsonObject executeModel(const QString& input);
    QString generateAutonomous(const QString& prompt);
    
    // Code generation and analysis
    QJsonArray planTasks(const QString& goal);
    QJsonObject analyzeCode(const QString& code);
    QJsonArray generateTests(const QString& code);
    
    // Configuration
    void setConfiguration(const QJsonObject& config);
    QJsonObject getConfiguration() const;
    
    // Model information
    QString getModelName() const;
    QString getModelVersion() const;
    
private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // MODEL_INTERFACE_H
