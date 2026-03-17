#ifndef AI_CODE_ASSISTANT_PANEL_H
#define AI_CODE_ASSISTANT_PANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include "ai_code_assistant.h"

/**
 * @brief AICodeAssistantPanel - UI panel for agentic AI assistant
 * 
 * Features:
 * - Real-time suggestion display with streaming
 * - Configuration controls (temperature, tokens)
 * - Search/grep interface
 * - Command execution interface
 * - Suggestion history
 * - Connection status indicator
 */
class AICodeAssistantPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AICodeAssistantPanel(AICodeAssistant *assistant, QWidget *parent = nullptr);
    ~AICodeAssistantPanel();

    // UI Components (public for direct access)
    QTextEdit *originalCodeDisplay_;
    QTextEdit *suggestionDisplay_;
    QTextEdit *explanationDisplay_;
    QSlider *temperatureSlider_;
    QSlider *maxTokensSlider_;
    QLabel *connectionStatusLabel_;
    QLabel *latencyLabel_;
    QListWidget *suggestionHistoryList_;
    QComboBox *suggestionTypeCombo_;
    
    // Configuration
    void setConnectionStatus(bool connected);
    void updateLatency(qint64 ms);

signals:
    void applyButtonClicked(const QString &suggestion);

private slots:
    // AI Response slots
    void onSuggestionReceived(const QString &suggestion, const QString &type);
    void onSuggestionStreamChunk(const QString &chunk);
    void onSuggestionComplete(bool success, const QString &message);
    
    // File operations slots
    void onSearchResultsReady(const QStringList &results);
    void onGrepResultsReady(const QStringList &results);
    void onFileSearchProgress(int processed, int total);
    
    // Command execution slots
    void onCommandOutputReceived(const QString &output);
    void onCommandErrorReceived(const QString &error);
    void onCommandCompleted(int exitCode);
    void onCommandProgress(const QString &status);
    
    // UI control slots
    void onApplyButtonClicked();
    void onCopyButtonClicked();
    void onExportButtonClicked();
    void onClearHistoryButtonClicked();
    void onTemperatureChanged(int value);
    void onMaxTokensChanged(int value);
    void onSearchButtonClicked();
    void onGrepButtonClicked();
    void onExecuteCommandButtonClicked();
    void onHistoryItemDoubleClicked();

private:
    void setupUI();
    void connectSignals();
    void addToHistory(const QString &suggestion, const QString &type);
    void showSearchDialog();
    void showGrepDialog();
    void showExecuteCommandDialog();
    QString formatResultsForDisplay(const QStringList &results);

    AICodeAssistant *m_assistant;
    QStringList m_currentResults;
    int m_historyMaxSize;
};

#endif // AI_CODE_ASSISTANT_PANEL_H
