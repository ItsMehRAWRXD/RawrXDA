#ifndef AI_CODE_ASSISTANT_H
#define AI_CODE_ASSISTANT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QElapsedTimer>
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
    void setModel(const QString &model);
    void setTemperature(float temp);
    void setMaxTokens(int tokens);
    void setWorkspaceRoot(const QString &root);

    // AI Code Suggestions
    void getCodeCompletion(const QString &code);
    void getRefactoringSuggestions(const QString &code);
    void getCodeExplanation(const QString &code);
    void getBugFixSuggestions(const QString &code);
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

signals:
    // AI response signals
    void suggestionReceived(const QString &suggestion, const QString &type);
    void suggestionStreamChunk(const QString &chunk);
    void suggestionComplete(bool success, const QString &message);
    
    // File search signals
    void searchResultsReady(const QStringList &results);
    void grepResultsReady(const QStringList &results);
    void fileSearchProgress(int processed, int total);
    
    // Command execution signals
    void commandOutputReceived(const QString &output);
    void commandErrorReceived(const QString &error);
    void commandCompleted(int exitCode);
    void commandProgress(const QString &status);
    
    // Agentic signals
    void analysisComplete(const QString &recommendation);
    void agentActionExecuted(const QString &action, const QString &result);
    
    // Metrics and logging
    void latencyMeasured(qint64 milliseconds);
    void errorOccurred(const QString &error);

private slots:
    void onNetworkReply();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();

private:
    // Network helpers
    void performOllamaRequest(const QString &systemPrompt, const QString &userPrompt, 
                             const QString &suggestType);
    void setupNetworkRequest(QNetworkRequest &request);
    
    // File system helpers
    QStringList recursiveFileSearch(const QString &directory, const QString &pattern);
    QStringList performGrep(const QString &directory, const QString &pattern, bool caseSensitive);
    
    // Command execution helpers
    QString executePowerShellSync(const QString &command, bool &success);
    void executePowerShellAsync(const QString &command);
    
    // Agentic reasoning helpers
    QString parseAIResponse(const QString &response);
    QString formatAgentPrompt(const QString &context);
    
    // Performance measurement
    void startTiming();
    void endTiming(const QString &operation);
    
    // Logging
    void logStructured(const QString &level, const QString &message, 
                      const QJsonObject &metadata = QJsonObject());

    // Members
    QNetworkAccessManager *m_networkManager;
    QProcess *m_process;
    QString m_ollamaUrl;
    QString m_model;
    float m_temperature;
    int m_maxTokens;
    QString m_workspaceRoot;
    QElapsedTimer m_timer;
    QByteArray m_responseBuffer;
    QString m_currentSuggestionType;
};

#endif // AI_CODE_ASSISTANT_H
