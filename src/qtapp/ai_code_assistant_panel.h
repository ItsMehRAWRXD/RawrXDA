#ifndef AI_CODE_ASSISTANT_PANEL_H
#define AI_CODE_ASSISTANT_PANEL_H


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
class AICodeAssistantPanel : public void {

public:
    explicit AICodeAssistantPanel(void *parent = nullptr);
    ~AICodeAssistantPanel();
    
    void initialize();
    void setAssistant(AICodeAssistant *assistant);

private:
    // Response from AI
    void onSuggestionReady(const int dummy);  // Placeholder slot, actual connections use lambdas
    void onSuggestionStreaming(const std::string &partial);
    void onSuggestionStreamComplete();
    void onError(const std::string &error);
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
    std::string formatLatency(int ms);

    AICodeAssistant *assistant_;
    
    // UI Components
    void *status_indicator_;
    void *model_label_;
    
    void *suggestion_type_selector_;
    void *original_code_display_;
    void *suggestion_display_;
    void *explanation_display_;
    
    void *temperature_slider_;
    void *temperature_value_label_;
    
    void *max_tokens_slider_;
    void *max_tokens_value_label_;
    
    void *progress_bar_;
    void *latency_label_;
    
    void *apply_button_;
    void *clear_button_;
    void *copy_button_;
    void *export_button_;
    
    QListWidget *suggestion_history_;
    
    // State
    std::string current_suggestion_text_;
    std::string current_suggestion_type_;
    bool streaming_in_progress_;
};

#endif // AI_CODE_ASSISTANT_PANEL_H

