#ifndef AI_CODE_ASSISTANT_PANEL_H
#define AI_CODE_ASSISTANT_PANEL_H

// Qt include removed (Qt-free build)
// <QLabel> removed (Qt-free build)
// <QPushButton> removed (Qt-free build)
// <QTextEdit> removed (Qt-free build)
// <QListWidget> removed (Qt-free build)
// <QProgressBar> removed (Qt-free build)
// <QComboBox> removed (Qt-free build)
// Qt include removed (Qt-free build)
#include <memory>

class AICodeAssistant;

/**
 * @brief UI Panel for AI Code Assistant
 * 
 * Provides an interface for:
 * - Viewing code suggestions in real-time
 * - Selecting suggestion types (completion, refactoring, explanation, etc.)
 * - Adjusting temperature and token limits
 * - Monitoring suggestion latency
 * - Applying suggestions to editor
 */
class AICodeAssistantPanel : public QDockWidget {
    /* Q_OBJECT */

public:
    explicit AICodeAssistantPanel(QWidget *parent = nullptr);
    ~AICodeAssistantPanel();
    
    void initialize();
    void setAssistant(AICodeAssistant *assistant);

private slots:
    // Response from AI
    void onSuggestionReady(const int dummy);  // Legacy signal compat — active connections use lambdas
    void onSuggestionStreaming(const QString &partial);
    void onSuggestionStreamComplete();
    void onError(const QString &error);
    void onConnectionStatusChanged(bool connected);
    void onLatencyMeasured(int latency_ms);

    // User actions
    void onApplySuggestion();
    void onClearPanel();
    void onCopyToClipboard();
    void onExportSuggestion();
    
    // UI interactions
    void onTemperatureChanged(int value);
    void onMaxTokensChanged(int value);

private:
    void setupUI();
    void updateStatusIndicator(bool connected);
    QString formatLatency(int ms);

    AICodeAssistant *assistant_;
    
    // UI Components
    QLabel *status_indicator_;
    QLabel *model_label_;
    
    QComboBox *suggestion_type_selector_;
    QTextEdit *original_code_display_;
    QTextEdit *suggestion_display_;
    QTextEdit *explanation_display_;
    
    QSlider *temperature_slider_;
    QLabel *temperature_value_label_;
    
    QSlider *max_tokens_slider_;
    QLabel *max_tokens_value_label_;
    
    QProgressBar *progress_bar_;
    QLabel *latency_label_;
    
    QPushButton *apply_button_;
    QPushButton *clear_button_;
    QPushButton *copy_button_;
    QPushButton *export_button_;
    
    QListWidget *suggestion_history_;
    
    // State
    QString current_suggestion_text_;
    QString current_suggestion_type_;
    bool streaming_in_progress_;
};

#endif // AI_CODE_ASSISTANT_PANEL_H
