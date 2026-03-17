#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

// Forward declarations
class AgenticEngine;

/**
 * @brief ModelInterface - Provides unified interface for model operations
 * 
 * This class serves as a bridge between the Qt GUI and the underlying
 * MASM/C++ model loading and execution system.
 */
class ModelInterface {
public:
    ModelInterface();
    ~ModelInterface();

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
