#ifndef MODEL_ROUTER_WIDGET_H
#define MODEL_ROUTER_WIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QHBoxLayout>
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
class ModelRouterWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModelRouterWidget(ModelRouterAdapter *adapter, QWidget *parent = nullptr);
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
    QString getSelectedModel() const;

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
    void setStatusMessage(const QString& message);

    /**
     * Display error message (red background)
     */
    void setErrorMessage(const QString& error);

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
    QString getPromptInput() const;

    /**
     * Clear prompt input
     */
    void clearPromptInput();

    /**
     * Get generated output
     */
    QString getGenerationOutput() const;

    /**
     * Set generated output display
     */
    void setGenerationOutput(const QString& output);

    /**
     * Append chunk to output (for streaming)
     */
    void appendGenerationChunk(const QString& chunk);

    /**
     * Clear output display
     */
    void clearOutput();

signals:
    // User action signals (emitted when user interacts with widget)
    void generateRequested(const QString& prompt, const QString& model);
    void stopRequested();
    void modelChanged(const QString& new_model);
    void settingsRequested();
    void dashboardRequested();
    void consoleRequested();
    void apiKeyEditRequested();
    void clearOutputRequested();

    // Status update signals (reported to IDE)
    void statusUpdated(const QString& status);
    void errorOccurred(const QString& error);

private slots:
    // Model router adapter signals
    void onGenerationStarted(const QString& model_name);
    void onGenerationProgress(int percent);
    void onGenerationChunk(const QString& chunk);
    void onGenerationComplete(const QString& result, int tokens_used, double latency_ms);
    void onGenerationError(const QString& error);
    void onModelListUpdated(const QStringList& models);
    void onModelChanged(const QString& model);
    void onStatusChanged(const QString& status);
    void onCostUpdated(double total_cost);
    void onStatisticsUpdated(const QJsonObject& stats);

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
    void showTemporaryStatus(const QString& message, int duration_ms = 3000);

    // Members
    ModelRouterAdapter *m_adapter;
    
    // Top toolbar widgets
    QLabel *m_model_label;
    QComboBox *m_model_combo;
    QPushButton *m_generate_button;
    QPushButton *m_stop_button;
    QPushButton *m_settings_button;
    QPushButton *m_api_key_button;
    QPushButton *m_dashboard_button;
    QPushButton *m_console_button;
    QPushButton *m_clear_output_button;
    
    // Status widgets
    QLabel *m_status_label;
    QProgressBar *m_progress_bar;
    
    // Metrics display
    QLabel *m_latency_label;      // "Latency: 150 ms"
    QLabel *m_cost_label;         // "Cost: $0.05"
    QLabel *m_success_label;      // "Success: 95%"
    
    // Error display
    QLabel *m_error_label;
    
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
