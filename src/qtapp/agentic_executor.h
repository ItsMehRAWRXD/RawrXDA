#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDir>
#include <memory>
#include <vector>

class AgenticEngine;
class InferenceEngine;
class ModelTrainer;
class SettingsManager;

/**
 * @class AgenticExecutor
 * @brief Real agentic execution - not simulated, actually performs tasks
 * 
 * This is the core agent loop that:
 * - Decomposes user requests into actionable steps
 * - Executes file operations (create folders, files)
 * - Runs compilers and reports results
 * - Uses function calling to interact with the IDE
 * - Maintains memory across tasks
 * - Self-corrects on failures
 * - Can fine-tune models with on-device training
 */
class AgenticExecutor : public QObject {
    Q_OBJECT

public:
    explicit AgenticExecutor(QObject* parent = nullptr);
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    // Main agentic execution entry point
    QJsonObject executeUserRequest(const QString& request);

    // Core agentic capabilities
    QJsonArray decomposeTask(const QString& goal);
    bool executeStep(const QJsonObject& step);
    bool verifyStepCompletion(const QJsonObject& step, const QString& result);

    // File system operations (real, not simulated)
    bool createDirectory(const QString& path);
    bool createFile(const QString& path, const QString& content);
    bool writeFile(const QString& path, const QString& content);
    QString readFile(const QString& path);
    bool deleteFile(const QString& path);
    bool deleteDirectory(const QString& path);
    QStringList listDirectory(const QString& path);

    // Compiler integration (real compilation)
    QJsonObject compileProject(const QString& projectPath, const QString& compiler = "g++");
    QJsonObject runExecutable(const QString& executablePath, const QStringList& args = QStringList());

    // Function calling system (tool use)
    QJsonArray getAvailableTools();
    QJsonObject callTool(const QString& toolName, const QJsonObject& params);

    // Model training capabilities
    QJsonObject trainModel(const QString& datasetPath, const QString& modelPath, const QJsonObject& config);
    bool isTrainingModel() const;

    // Memory and context
    void addToMemory(const QString& key, const QVariant& value);
    QVariant getFromMemory(const QString& key);
    void clearMemory();
    QString getFullContext();

    // Self-correction
    bool detectFailure(const QString& output);
    QString generateCorrectionPlan(const QString& failureReason);
    QJsonObject retryWithCorrection(const QJsonObject& failedStep);

signals:
    void stepStarted(const QString& description);
    void stepCompleted(const QString& description, bool success);
    void taskProgress(int current, int total);
    void executionComplete(const QJsonObject& result);
    void errorOccurred(const QString& error);
    void logMessage(const QString& message);
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const QString& modelPath, float finalPerplexity);

private:
    // Agent reasoning using model
    QString planNextAction(const QString& currentState, const QString& goal);
    QJsonObject generateCode(const QString& specification);
    QString analyzeError(const QString& errorOutput);
    QString improveCode(const QString& code, const QString& issue);

    // Memory settings management
    void loadMemorySettings();
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();
    void removeMemoryItem(const QString& key);

    // Internal helpers
    QJsonObject buildToolCallPrompt(const QString& goal, const QJsonArray& tools);
    QString extractCodeFromResponse(const QString& response);
    bool validateGeneratedCode(const QString& code);

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<ModelTrainer> m_modelTrainer;
    SettingsManager* m_settingsManager = nullptr;
    
    QMap<QString, QVariant> m_memory;
    QJsonArray m_executionHistory;
    QString m_currentWorkingDirectory;
    
    bool m_memoryEnabled = false;
    qint64 m_memoryLimitBytes = 134217728; // 128 MB default
    
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;
};
