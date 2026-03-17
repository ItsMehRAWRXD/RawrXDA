#ifndef AI_CODE_ASSISTANT_PANEL_H
#define AI_CODE_ASSISTANT_PANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QJsonArray>

class AICodeAssistant;

/**
 * AICodeAssistantPanel - Qt UI for AICodeAssistant
 * 
 * Provides a user-friendly interface for:
 * - AI code suggestions (completion, refactoring, explanation, etc.)
 * - File search and grep operations
 * - Command execution and monitoring
 * - Real-time progress and latency metrics
 */
class AICodeAssistantPanel : public QWidget {
    Q_OBJECT

public:
    explicit AICodeAssistantPanel(QWidget *parent = nullptr);
    ~AICodeAssistantPanel();

    void setAssistant(AICodeAssistant *assistant);

private slots:
    // AI Suggestion Handlers
    void onCompletionReady(const QString &suggestion);
    void onRefactoringReady(const QString &suggestion);
    void onExplanationReady(const QString &explanation);
    void onBugFixReady(const QString &suggestion);
    void onOptimizationReady(const QString &suggestion);

    // File Operation Handlers
    void onFileSearchProgress(int processed, int total);
    void onSearchResultsReady(const QStringList &results);
    void onGrepResultsReady(const QJsonArray &results);

    // Command Execution Handlers
    void onCommandProgress(const QString &status);
    void onCommandOutputReceived(const QString &output);
    void onCommandCompleted(int exitCode);

    // Metrics Handlers
    void onLatencyMeasured(qint64 milliseconds);
    void onOperationMetrics(const QJsonObject &metrics);

    // Error Handler
    void onErrorOccurred(const QString &error);

    // UI Actions
    void onRequestCompletion();
    void onRequestRefactoring();
    void onRequestExplanation();
    void onRequestBugFix();
    void onRequestOptimization();
    void onSearchFiles();
    void onGrepFiles();
    void onExecuteCommand();

    void displaySuggestion(const QString &title, const QString &suggestion);
    void displayResults(const QStringList &results);
    void displayGrepResults(const QJsonArray &results);
    void displayError(const QString &error);

private:
    void setupUI();

    // Components
    QTextEdit *m_suggestionsDisplay;
    QListWidget *m_resultsWidget;
    QProgressBar *m_progressBar;
    QLineEdit *m_searchInput;
    QLineEdit *m_commandInput;
    QLabel *m_latencyLabel;
    QLabel *m_statusLabel;

    // Buttons
    QPushButton *m_completeBtn;
    QPushButton *m_refactorBtn;
    QPushButton *m_explainBtn;
    QPushButton *m_bugFixBtn;
    QPushButton *m_optimizeBtn;
    QPushButton *m_searchBtn;
    QPushButton *m_grepBtn;
    QPushButton *m_executeBtn;

    // Reference to assistant
    AICodeAssistant *m_assistant;
};

#endif // AI_CODE_ASSISTANT_PANEL_H
