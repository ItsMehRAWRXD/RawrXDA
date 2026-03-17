#ifndef AI_CODE_ASSISTANT_H
#define AI_CODE_ASSISTANT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QElapsedTimer>
#include <QTimer>
#include <QMutex>
#include <QThreadPool>
#include <QHash>
#include <chrono>
#include <functional>

/**
 * @brief AICodeAssistant - AGENTIC AI assistant with full IDE integration
 * 
 * Full IDE integration capabilities:
 * - Code completion/refactoring/explanation via Ollama
 * - File searching and grepping across workspace
 * - PowerShell command execution and automation
 * - Structured logging and performance metrics
 * - Real-time performance monitoring
 */
class AICodeAssistant : public QObject
{
    Q_OBJECT

public:
    // Data structures
    struct CodeSuggestion {
        QString text;           // The suggestion text
        QString type;           // Type: "completion", "refactoring", "explanation", "bugfix", "optimization"
        QString context;        // Original code context
        float confidence;       // Confidence score 0.0-1.0
        qint64 latency_ms;      // Response latency in milliseconds
        QDateTime timestamp;    // When the suggestion was generated
    };

    explicit AICodeAssistant(QObject *parent = nullptr);
    ~AICodeAssistant();

    // Configuration
    void setOllamaUrl(const QString &url);
    void setOllamaServer(const QString &host, int port);
    void setModel(const QString &model);
    void setTemperature(float temp);
    void setMaxTokens(int tokens);
    void setWorkspaceRoot(const QString &root);

    // AI Code Suggestions
    void getCodeCompletion(const QString &code);
    void getCodeCompletion(const QString &code, const QString &context);
    void getRefactoringSuggestions(const QString &code);
    void getCodeExplanation(const QString &code);
    void getBugFixSuggestions(const QString &code);
    void getBugFixSuggestions(const QString &code, const QString &errorMsg);
    void getOptimizationSuggestions(const QString &code);

    // IDE Integration - File Operations
    void searchFiles(const QString &pattern, const QString &directory = "");
    void grepFiles(const QString &pattern, const QString &directory = "", bool caseSensitive = false);
    void findInFile(const QString &filePath, const QString &pattern);

    // IDE Integration - Command Execution (PowerShell)
    void executePowerShellCommand(const QString &command);
    void runBuildCommand(const QString &command);
    void runTestCommand(const QString &command);

    // Agentic reasoning
    void analyzeAndRecommend(const QString &context);
    void autoFixIssue(const QString &issueDescription, const QString &codeContext);

    // Cache and performance tuning
    void preloadModel(const QString &modelName);
    void setCacheSize(size_t size);
    void clearCache();
    size_t cacheSize() const;
    size_t cacheHits() const;
    size_t cacheMisses() const;
    void setAIRequestTimeout(int milliseconds);
    void setSearchTimeout(int milliseconds);
    void setCommandTimeout(int milliseconds);

signals:
    // AI response signals
    void suggestionReceived(const QString &suggestion, const QString &type);
    void suggestionStreamChunk(const QString &chunk);
    void suggestionComplete(bool success, const QString &message);
    // Fine-grained suggestion signals used by UI panel
    void completionReady(const QString &suggestion);
    void refactoringReady(const QString &suggestion);
    void explanationReady(const QString &explanation);
    void bugFixReady(const QString &suggestion);
    void optimizationReady(const QString &suggestion);
    void analysisReady(const QJsonObject &analysis);
    void autoFixReady(const QString &fixedCode);
    
    // File search signals
    void searchResultsReady(const QStringList &results);
    void grepResultsReady(const QJsonArray &results);
    void fileSearchProgress(int processed, int total);
    
    // Command execution signals
    void commandOutputReceived(const QString &output);
    void commandErrorReceived(const QString &error);
    void commandCompleted(int exitCode);
    void commandProgress(const QString &status);
    
    // Agentic signals
    void analysisComplete(const QString &recommendation);
    void agentActionExecuted(const QString &action, const QString &result);
    void agentActionStarted(const QString &action);
    void agentActionCompleted(const QString &action);
    
    // Metrics and logging
    void latencyMeasured(qint64 milliseconds);
    void errorOccurred(const QString &error);
    void operationMetrics(const QJsonObject &metrics);

private slots:
    void onNetworkReplyFinished(QNetworkReply *reply);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();
    void onSearchTimeout();
    void onCommandTimeout();

private:
    // Network helpers
    void performOllamaRequest(const QString &systemPrompt, const QString &userPrompt, 
                             const QString &suggestType);
    void setupNetworkRequest(QNetworkRequest &request);
    void sendAIRequest(const QString &prompt, const QString &responseSignal);
    QJsonObject buildRequestPayload(const QString &prompt);
    
    // File system helpers
    void recursiveSearch(const QString &dir, const QString &pattern, QStringList &results, int &processed);
    void recursiveGrep(const QString &dir, const QString &pattern, QJsonArray &results, bool caseSensitive);
    
    // Command execution helpers
    QString executePowerShellSync(const QString &command, bool &success);
    int executeCommandSync(const QString &command, QString &output);
    void executePowerShellAsync(const QString &command);
    
    // Agentic reasoning helpers
    QString parseAIResponse(const QString &response);
    void parseAIResponse(const QString &response, const QString &signalName);
    QString formatAgentPrompt(const QString &context);
    
    // Performance measurement
    void startTiming();
    void endTiming(const QString &operation);
    qint64 getElapsedMilliseconds();
    
    // Logging
    void logStructured(const QString &level, const QString &message, 
                      const QJsonObject &metadata = QJsonObject());
    void emitOperationMetrics(const QString &operation, qint64 durationMs, const QJsonObject &extra = QJsonObject());

    // Members
    QNetworkAccessManager *m_networkManager = nullptr;
    QProcess *m_process = nullptr;
    QString m_ollamaUrl;
    QString m_ollamaHost;
    int m_ollamaPort = 0;
    QString m_model;
    float m_temperature = 0.0f;
    int m_maxTokens = 0;
    QString m_workspaceRoot;
    QElapsedTimer m_timer;
    QByteArray m_responseBuffer;
    QString m_currentSuggestionType;
    // Timers and timeouts
    QTimer *m_searchTimer = nullptr;
    QTimer *m_commandTimer = nullptr;
    int m_aiRequestTimeout = 0;
    int m_searchTimeout = 0;
    int m_commandTimeout = 0;
    // Caching
    QHash<QString, QString> m_aiCache;
    size_t m_cacheSize = 0;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
    // Performance
    std::chrono::high_resolution_clock::time_point m_operationStart;
};

#endif // AI_CODE_ASSISTANT_H
