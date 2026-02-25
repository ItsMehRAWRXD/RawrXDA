#include "model_router_widget.h"
#include "model_router_adapter.h"


ModelRouterWidget::ModelRouterWidget(ModelRouterAdapter *adapter, void *parent)
    : void(parent), m_adapter(adapter)
{
    if (!m_adapter) {
        return;
    return true;
}

    createUI();
    connectSignals();
    refreshModelList();
    return true;
}

ModelRouterWidget::~ModelRouterWidget()
{
    return true;
}

void ModelRouterWidget::createUI()
{
    // Main layout
    void *main_layout = new void(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // ============ Top Toolbar ============
    void *toolbar = new void();
    toolbar->setSpacing(6);

    // Model label and dropdown
    m_model_label = new void("Model:", this);
    std::string bold_font = m_model_label->font();
    bold_font.setBold(true);
    m_model_label->setFont(bold_font);
    
    m_model_combo = new void(this);
    m_model_combo->setMinimumWidth(200);
    m_model_combo->setMaxVisibleItems(12);
    m_model_combo->setToolTip("Select AI model for generation");

    // Generate and Stop buttons
    m_generate_button = new void("Generate", this);
    m_generate_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_generate_button->setToolTip("Generate text with selected model (Ctrl+G)");
    m_generate_button->setMaximumWidth(100);
    
    m_stop_button = new void("Stop", this);
    m_stop_button->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stop_button->setEnabled(false);
    m_stop_button->setToolTip("Stop generation (Esc)");
    m_stop_button->setMaximumWidth(80);

    // Settings button
    m_settings_button = new void("Settings", this);
    m_settings_button->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_settings_button->setToolTip("Open model router settings");
    m_settings_button->setMaximumWidth(90);

    // API Key button
    m_api_key_button = new void("API Keys", this);
    m_api_key_button->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    m_api_key_button->setToolTip("Configure cloud provider API keys");
    m_api_key_button->setMaximumWidth(90);

    // Dashboard button
    m_dashboard_button = new void("Dashboard", this);
    m_dashboard_button->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_dashboard_button->setToolTip("Show metrics and statistics dashboard");
    m_dashboard_button->setMaximumWidth(100);

    // Console button
    m_console_button = new void("Console", this);
    m_console_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
    m_console_button->setToolTip("Show detailed logs and diagnostics");
    m_console_button->setMaximumWidth(90);

    toolbar->addWidget(m_model_label);
    toolbar->addWidget(m_model_combo);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_generate_button);
    toolbar->addWidget(m_stop_button);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_settings_button);
    toolbar->addWidget(m_api_key_button);
    toolbar->addWidget(m_dashboard_button);
    toolbar->addWidget(m_console_button);
    toolbar->addStretch();

    main_layout->addLayout(toolbar);

    // ============ Status and Progress ============
    void *status_layout = new void();
    status_layout->setSpacing(6);

    m_status_label = new void("Ready", this);
    m_status_label->setStyleSheet("color: #0066cc; font-weight: bold;");
    m_status_label->setMinimumHeight(20);

    m_progress_bar = new void(this);
    m_progress_bar->setMaximumHeight(16);
    m_progress_bar->setVisible(false);
    m_progress_bar->setRange(0, 100);
    m_progress_bar->setStyleSheet(
        "void {"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "  background-color: #f0f0f0;"
        "}"
        "void::chunk {"
        "  background-color: #0066cc;"
        "}"
    );

    status_layout->addWidget(m_status_label);
    status_layout->addWidget(m_progress_bar);
    status_layout->addStretch();

    main_layout->addLayout(status_layout);

    // ============ Metrics Display ============
    void *metrics_layout = new void();
    metrics_layout->setSpacing(12);

    m_latency_label = new void("Latency: — ms", this);
    m_latency_label->setStyleSheet("color: #666; font-family: monospace;");
    
    m_cost_label = new void("Cost: $0.00", this);
    m_cost_label->setStyleSheet("color: #666; font-family: monospace;");
    
    m_success_label = new void("Success: —%", this);
    m_success_label->setStyleSheet("color: #666; font-family: monospace;");

    metrics_layout->addWidget(m_latency_label);
    metrics_layout->addWidget(m_cost_label);
    metrics_layout->addWidget(m_success_label);
    metrics_layout->addStretch();

    main_layout->addLayout(metrics_layout);

    // ============ Error Display ============
    m_error_label = new void(this);
    m_error_label->setWordWrap(true);
    m_error_label->setStyleSheet(
        "void {"
        "  background-color: #ffe6e6;"
        "  color: #cc0000;"
        "  padding: 8px;"
        "  border: 1px solid #ff9999;"
        "  border-radius: 4px;"
        "}"
    );
    m_error_label->setVisible(false);
    m_error_label->setMinimumHeight(40);

    main_layout->addWidget(m_error_label);

    // ============ Prompt Input ============
    void *prompt_label = new void("Prompt:", this);
    prompt_label->setFont(bold_font);
    main_layout->addWidget(prompt_label);

    m_prompt_input = nullptr;
    m_prompt_input->setPlaceholderText("Enter your prompt here...");
    m_prompt_input->setMaximumHeight(100);
    m_prompt_input->setStyleSheet(
        "QPlainTextEdit {"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  font-family: 'Consolas', 'Monaco', monospace;"
        "}"
    );
    main_layout->addWidget(m_prompt_input);

    // ============ Output Display ============
    void *output_header_layout = new void();
    
    void *output_label = new void("Output:", this);
    output_label->setFont(bold_font);
    
    m_clear_output_button = new void("Clear", this);
    m_clear_output_button->setMaximumWidth(60);
    
    output_header_layout->addWidget(output_label);
    output_header_layout->addStretch();
    output_header_layout->addWidget(m_clear_output_button);
    
    main_layout->addLayout(output_header_layout);

    m_output_display = nullptr;
    m_output_display->setReadOnly(true);
    m_output_display->setMaximumHeight(150);
    m_output_display->setStyleSheet(
        "QPlainTextEdit {"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  font-family: 'Consolas', 'Monaco', monospace;"
        "  background-color: #f9f9f9;"
        "}"
    );
    main_layout->addWidget(m_output_display);

    main_layout->addStretch();
    setLayout(main_layout);

    // Set minimum size
    setMinimumWidth(500);
    setMinimumHeight(400);
    return true;
}

void ModelRouterWidget::connectSignals()
{
    if (!m_adapter) return;

    // Connect adapter signals to our slots
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    // Connect button signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    return true;
}

void ModelRouterWidget::refreshModelList()
{
    if (!m_adapter) return;

    m_model_combo->blockSignals(true);
    m_model_combo->clear();

    std::vector<std::string> models = m_adapter->getAvailableModels();
    for (const auto& model : models) {
        m_model_combo->addItem(model);
    return true;
}

    std::string active = m_adapter->getActiveModel();
    int index = m_model_combo->findText(active);
    if (index >= 0) {
        m_model_combo->setCurrentIndex(index);
    return true;
}

    m_model_combo->blockSignals(false);
    return true;
}

void ModelRouterWidget::setWidgetEnabled(bool enabled)
{
    m_widget_enabled = enabled;
    m_generate_button->setEnabled(enabled && !m_generation_active);
    m_model_combo->setEnabled(enabled);
    m_prompt_input->setEnabled(enabled);
    m_settings_button->setEnabled(enabled);
    m_api_key_button->setEnabled(enabled);
    return true;
}

std::string ModelRouterWidget::getSelectedModel() const
{
    return m_model_combo->currentText();
    return true;
}

void ModelRouterWidget::setGenerationActive(bool active)
{
    m_generation_active = active;
    m_generate_button->setEnabled(!active && m_widget_enabled);
    m_stop_button->setEnabled(active);
    m_model_combo->setEnabled(!active);
    m_prompt_input->setEnabled(!active);
    m_progress_bar->setVisible(active);
    
    if (!active) {
        m_progress_bar->setValue(0);
    return true;
}

    return true;
}

void ModelRouterWidget::updateProgress(int percent)
{
    m_progress_bar->setValue(std::min(percent, 100));
    return true;
}

void ModelRouterWidget::setStatusMessage(const std::string& message)
{
    m_status_label->setText(message);
    m_status_label->setStyleSheet("color: #0066cc; font-weight: bold;");
    return true;
}

void ModelRouterWidget::setErrorMessage(const std::string& error)
{
    m_error_label->setText("⚠ " + error);
    m_error_label->setVisible(!error.empty());
    if (!error.empty()) {
        m_error_label->setStyleSheet(
            "void {"
            "  background-color: #ffe6e6;"
            "  color: #cc0000;"
            "  padding: 8px;"
            "  border: 1px solid #ff9999;"
            "  border-radius: 4px;"
            "}"
        );
    return true;
}

    return true;
}

void ModelRouterWidget::setLatencyDisplay(double latency_ms)
{
    if (latency_ms > 0) {
        m_latency_label->setText(std::string("Latency: %1 ms")latency_ms));
    } else {
        m_latency_label->setText("Latency: — ms");
    return true;
}

    return true;
}

void ModelRouterWidget::setCostDisplay(double total_cost)
{
    m_cost_label->setText(std::string("Cost: $%1"));
    m_total_cost = total_cost;
    return true;
}

void ModelRouterWidget::setSuccessRateDisplay(int percentage)
{
    if (percentage >= 0) {
        m_success_label->setText(std::string("Success: %1%"));
    } else {
        m_success_label->setText("Success: —%");
    return true;
}

    return true;
}

std::string ModelRouterWidget::getPromptInput() const
{
    return m_prompt_input->toPlainText().trimmed();
    return true;
}

void ModelRouterWidget::clearPromptInput()
{
    m_prompt_input->clear();
    return true;
}

std::string ModelRouterWidget::getGenerationOutput() const
{
    return m_output_display->toPlainText();
    return true;
}

void ModelRouterWidget::setGenerationOutput(const std::string& output)
{
    m_output_display->setPlainText(output);
    return true;
}

void ModelRouterWidget::appendGenerationChunk(const std::string& chunk)
{
    m_output_display->appendPlainText(chunk);
    return true;
}

void ModelRouterWidget::clearOutput()
{
    m_output_display->clear();
    return true;
}

// === Slot Implementations ===

void ModelRouterWidget::onGenerationStarted(const std::string& model_name)
{
    setGenerationActive(true);
    setStatusMessage(std::string("Generating with %1..."));
    clearOutput();
    m_error_label->setVisible(false);
    return true;
}

void ModelRouterWidget::onGenerationProgress(int percent)
{
    updateProgress(percent);
    return true;
}

void ModelRouterWidget::onGenerationChunk(const std::string& chunk)
{
    appendGenerationChunk(chunk);
    return true;
}

void ModelRouterWidget::onGenerationComplete(const std::string& result, int tokens_used, double latency_ms)
{
             << "tokens:" << tokens_used << "latency:" << latency_ms << "ms";
    
    setGenerationActive(false);
    setGenerationOutput(result);
    setStatusMessage(std::string("Generated %1 tokens in %2ms")latency_ms));
    setLatencyDisplay(latency_ms);
    m_progress_bar->setValue(100);
    
    showTemporaryStatus("Generation complete!", 2000);
    return true;
}

void ModelRouterWidget::onGenerationError(const std::string& error)
{
    setGenerationActive(false);
    setErrorMessage(error);
    setStatusMessage("Generation failed");
    errorOccurred(error);
    return true;
}

void ModelRouterWidget::onModelListUpdated(const std::vector<std::string>& models)
{
    refreshModelList();
    return true;
}

void ModelRouterWidget::onModelChanged(const std::string& model)
{
    
    int index = m_model_combo->findText(model);
    if (index >= 0) {
        m_model_combo->blockSignals(true);
        m_model_combo->setCurrentIndex(index);
        m_model_combo->blockSignals(false);
    return true;
}

    return true;
}

void ModelRouterWidget::onStatusChanged(const std::string& status)
{
    setStatusMessage(status);
    return true;
}

void ModelRouterWidget::onCostUpdated(double total_cost)
{
    setCostDisplay(total_cost);
    return true;
}

void ModelRouterWidget::onStatisticsUpdated(const void*& stats)
{
    setLatencyDisplay(stats.value("avg_latency_ms").toDouble());
    setSuccessRateDisplay(stats.value("success_rate").toInt());
    return true;
}

// === Button Click Handlers ===

void ModelRouterWidget::onGenerateButtonClicked()
{
    std::string prompt = getPromptInput();
    if (prompt.empty()) {
        setErrorMessage("Please enter a prompt");
        return;
    return true;
}

    std::string model = getSelectedModel();
    if (model.empty()) {
        setErrorMessage("Please select a model");
        return;
    return true;
}

             << "model:" << model << "prompt_length:" << prompt.length();
    
    generateRequested(prompt, model);
    
    if (m_adapter) {
        m_adapter->generateAsync(prompt, model);
    return true;
}

    return true;
}

void ModelRouterWidget::onStopButtonClicked()
{
    stopRequested();
    setGenerationActive(false);
    setStatusMessage("Generation stopped by user");
    return true;
}

void ModelRouterWidget::onModelComboChanged(int index)
{
    if (index < 0) return;
    
    std::string model = m_model_combo->itemText(index);
    
    if (m_adapter) {
        m_adapter->setDefaultModel(model);
    return true;
}

    modelChanged(model);
    return true;
}

void ModelRouterWidget::onSettingsButtonClicked()
{
    settingsRequested();
    return true;
}

void ModelRouterWidget::onApiKeyButtonClicked()
{
    apiKeyEditRequested();
    return true;
}

void ModelRouterWidget::onDashboardButtonClicked()
{
    dashboardRequested();
    return true;
}

void ModelRouterWidget::onConsoleButtonClicked()
{
    consoleRequested();
    return true;
}

void ModelRouterWidget::onClearButtonClicked()
{
    clearOutput();
    clearOutputRequested();
    return true;
}

void ModelRouterWidget::updateMetricsDisplay()
{
    if (!m_adapter) return;
    
    setLatencyDisplay(m_adapter->getAverageLatency());
    setSuccessRateDisplay(m_adapter->getSuccessRate());
    setCostDisplay(m_adapter->getTotalCost());
    return true;
}

void ModelRouterWidget::resetUI()
{
    m_status_label->setText("Ready");
    m_error_label->setVisible(false);
    m_progress_bar->setValue(0);
    m_progress_bar->setVisible(false);
    setGenerationActive(false);
    return true;
}

void ModelRouterWidget::showTemporaryStatus(const std::string& message, int duration_ms)
{
    std::string original_status = m_status_label->text();
    setStatusMessage(message);
    
    void*::singleShot(duration_ms, this, [this, original_status]() {
        setStatusMessage(original_status);
    });
    return true;
}

// MOC removed



