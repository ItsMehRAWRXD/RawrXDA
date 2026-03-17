#ifndef MODEL_ROUTER_WIDGET_H
#define MODEL_ROUTER_WIDGET_H


#include <memory>

class ModelRouterAdapter;

/**
 * @class ModelRouterWidget
 * @brief Toolbar/panel widget for model router control and monitoring
 * 
 * Provides user-facing GUI for:
 * - Model selection (dropdown with all available models)
 * - Generation control (Generate/Stop buttons)
 * - Real-time status display (current operation, progress)
 * - Performance indicators (latency, cost, success rate)
 * - Error display and recovery
 * 
 * Connects to ModelRouterAdapter for all operations.
 * Emits user intent signals for IDE integration.
 */
class ModelRouterWidget : public void {

public:
    explicit ModelRouterWidget(ModelRouterAdapter *adapter, void *parent = nullptr);
    ~ModelRouterWidget();

    // === Control Methods ===
    
    /**
     * Populate model list from available models
     */
    void refreshModelList();

    /**
     * Set widget enabled/disabled state
     */
    void setWidgetEnabled(bool enabled);

    /**
     * Get currently selected model name
     */
    std::string getSelectedModel() const;

    /**
     * Set UI to show generation in progress
     */
    void setGenerationActive(bool active);

    /**
     * Update progress bar during generation
     */
    void updateProgress(int percent);

    /**
     * Display operation status message
     */
    void setStatusMessage(const std::string& message);

    /**
     * Display error message (red background)
     */
    void setErrorMessage(const std::string& error);

    /**
     * Update latency display (ms)
     */
    void setLatencyDisplay(double latency_ms);

    /**
     * Update cost display (USD)
     */
    void setCostDisplay(double total_cost);

    /**
     * Update success rate display (0-100)
     */
    void setSuccessRateDisplay(int percentage);

    /**
     * Get prompt from user input (if embedded)
     */
    std::string getPromptInput() const;

    /**
     * Clear prompt input
     */
    void clearPromptInput();

    /**
     * Get generated output
     */
    std::string getGenerationOutput() const;

    /**
     * Set generated output display
     */
    void setGenerationOutput(const std::string& output);

    /**
     * Append chunk to output (for streaming)
     */
    void appendGenerationChunk(const std::string& chunk);

    /**
     * Clear output display
     */
    void clearOutput();


    // User action signals (emitted when user interacts with widget)
    void generateRequested(const std::string& prompt, const std::string& model);
    void stopRequested();
    void modelChanged(const std::string& new_model);
    void settingsRequested();
    void dashboardRequested();
    void consoleRequested();
    void apiKeyEditRequested();
    void clearOutputRequested();

    // Status update signals (reported to IDE)
    void statusUpdated(const std::string& status);
    void errorOccurred(const std::string& error);

private:
    // Model router adapter signals
    void onGenerationStarted(const std::string& model_name);
    void onGenerationProgress(int percent);
    void onGenerationChunk(const std::string& chunk);
    void onGenerationComplete(const std::string& result, int tokens_used, double latency_ms);
    void onGenerationError(const std::string& error);
    void onModelListUpdated(const std::vector<std::string>& models);
    void onModelChanged(const std::string& model);
    void onStatusChanged(const std::string& status);
    void onCostUpdated(double total_cost);
    void onStatisticsUpdated(const void*& stats);

    // Button clicks
    void onGenerateButtonClicked();
    void onStopButtonClicked();
    void onModelComboChanged(int index);
    void onSettingsButtonClicked();
    void onDashboardButtonClicked();
    void onConsoleButtonClicked();
    void onClearButtonClicked();
    void onApiKeyButtonClicked();

private:
    // UI Layout helpers
    void createUI();
    void connectSignals();
    void updateMetricsDisplay();
    void resetUI();
    void showTemporaryStatus(const std::string& message, int duration_ms = 3000);

    // Members
    ModelRouterAdapter *m_adapter;
    
    // Top toolbar widgets
    void *m_model_label;
    void *m_model_combo;
    void *m_generate_button;
    void *m_stop_button;
    void *m_settings_button;
    void *m_api_key_button;
    void *m_dashboard_button;
    void *m_console_button;
    void *m_clear_output_button;
    
    // Status widgets
    void *m_status_label;
    void *m_progress_bar;
    
    // Metrics display
    void *m_latency_label;      // "Latency: 150 ms"
    void *m_cost_label;         // "Cost: $0.05"
    void *m_success_label;      // "Success: 95%"
    
    // Error display
    void *m_error_label;
    
    // Input/Output widgets
    QPlainTextEdit *m_prompt_input;
    QPlainTextEdit *m_output_display;
    
    // State
    bool m_generation_active = false;
    bool m_widget_enabled = true;
    double m_total_cost = 0.0;
    double m_avg_latency = 0.0;
    int m_success_rate = 0;
};

#endif // MODEL_ROUTER_WIDGET_H

