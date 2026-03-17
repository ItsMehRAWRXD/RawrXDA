#include "model_router_widget.h"
#include "model_router_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QDebug>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QFont>
#include <QStyleFactory>

ModelRouterWidget::ModelRouterWidget(ModelRouterAdapter *adapter, QWidget *parent)
    : QWidget(parent), m_adapter(adapter)
{
    if (!m_adapter) {
        qWarning() << "[ModelRouterWidget] Constructed with null adapter";
        return;
    }

    createUI();
    connectSignals();
    refreshModelList();
    
    qDebug() << "[ModelRouterWidget] Constructed successfully";
}

ModelRouterWidget::~ModelRouterWidget()
{
    qDebug() << "[ModelRouterWidget] Destroyed";
}

void ModelRouterWidget::createUI()
{
    // Main layout
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // ============ Top Toolbar ============
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    // Model label and dropdown
    m_model_label = new QLabel("Model:", this);
    QFont bold_font = m_model_label->font();
    bold_font.setBold(true);
    m_model_label->setFont(bold_font);
    
    m_model_combo = new QComboBox(this);
    m_model_combo->setMinimumWidth(200);
    m_model_combo->setMaxVisibleItems(12);
    m_model_combo->setToolTip("Select AI model for generation");

    // Generate and Stop buttons
    m_generate_button = new QPushButton("Generate", this);
    m_generate_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_generate_button->setToolTip("Generate text with selected model (Ctrl+G)");
    m_generate_button->setMaximumWidth(100);
    
    m_stop_button = new QPushButton("Stop", this);
    m_stop_button->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stop_button->setEnabled(false);
    m_stop_button->setToolTip("Stop generation (Esc)");
    m_stop_button->setMaximumWidth(80);

    // Settings button
    m_settings_button = new QPushButton("Settings", this);
    m_settings_button->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_settings_button->setToolTip("Open model router settings");
    m_settings_button->setMaximumWidth(90);

    // API Key button
    m_api_key_button = new QPushButton("API Keys", this);
    m_api_key_button->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    m_api_key_button->setToolTip("Configure cloud provider API keys");
    m_api_key_button->setMaximumWidth(90);

    // Dashboard button
    m_dashboard_button = new QPushButton("Dashboard", this);
    m_dashboard_button->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_dashboard_button->setToolTip("Show metrics and statistics dashboard");
    m_dashboard_button->setMaximumWidth(100);

    // Console button
    m_console_button = new QPushButton("Console", this);
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
    QHBoxLayout *status_layout = new QHBoxLayout();
    status_layout->setSpacing(6);

    m_status_label = new QLabel("Ready", this);
    m_status_label->setStyleSheet("color: #0066cc; font-weight: bold;");
    m_status_label->setMinimumHeight(20);

    m_progress_bar = new QProgressBar(this);
    m_progress_bar->setMaximumHeight(16);
    m_progress_bar->setVisible(false);
    m_progress_bar->setRange(0, 100);
    m_progress_bar->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "  background-color: #f0f0f0;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #0066cc;"
        "}"
    );

    status_layout->addWidget(m_status_label);
    status_layout->addWidget(m_progress_bar);
    status_layout->addStretch();

    main_layout->addLayout(status_layout);

    // ============ Metrics Display ============
    QHBoxLayout *metrics_layout = new QHBoxLayout();
    metrics_layout->setSpacing(12);

    m_latency_label = new QLabel("Latency: — ms", this);
    m_latency_label->setStyleSheet("color: #666; font-family: monospace;");
    
    m_cost_label = new QLabel("Cost: $0.00", this);
    m_cost_label->setStyleSheet("color: #666; font-family: monospace;");
    
    m_success_label = new QLabel("Success: —%", this);
    m_success_label->setStyleSheet("color: #666; font-family: monospace;");

    metrics_layout->addWidget(m_latency_label);
    metrics_layout->addWidget(m_cost_label);
    metrics_layout->addWidget(m_success_label);
    metrics_layout->addStretch();

    main_layout->addLayout(metrics_layout);

    // ============ Error Display ============
    m_error_label = new QLabel(this);
    m_error_label->setWordWrap(true);
    m_error_label->setStyleSheet(
        "QLabel {"
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
    QLabel *prompt_label = new QLabel("Prompt:", this);
    prompt_label->setFont(bold_font);
    main_layout->addWidget(prompt_label);

    m_prompt_input = new QPlainTextEdit(this);
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
    QHBoxLayout *output_header_layout = new QHBoxLayout();
    
    QLabel *output_label = new QLabel("Output:", this);
    output_label->setFont(bold_font);
    
    m_clear_output_button = new QPushButton("Clear", this);
    m_clear_output_button->setMaximumWidth(60);
    
    output_header_layout->addWidget(output_label);
    output_header_layout->addStretch();
    output_header_layout->addWidget(m_clear_output_button);
    
    main_layout->addLayout(output_header_layout);

    m_output_display = new QPlainTextEdit(this);
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
}

void ModelRouterWidget::connectSignals()
{
    if (!m_adapter) return;

    // Connect adapter signals to our slots
    connect(m_adapter, &ModelRouterAdapter::generationStarted,
            this, &ModelRouterWidget::onGenerationStarted);
    connect(m_adapter, &ModelRouterAdapter::generationProgress,
            this, &ModelRouterWidget::onGenerationProgress);
    connect(m_adapter, &ModelRouterAdapter::generationChunk,
            this, &ModelRouterWidget::onGenerationChunk);
    connect(m_adapter, &ModelRouterAdapter::generationComplete,
            this, &ModelRouterWidget::onGenerationComplete);
    connect(m_adapter, &ModelRouterAdapter::generationError,
            this, &ModelRouterWidget::onGenerationError);
    connect(m_adapter, &ModelRouterAdapter::modelListUpdated,
            this, &ModelRouterWidget::onModelListUpdated);
    connect(m_adapter, &ModelRouterAdapter::modelChanged,
            this, &ModelRouterWidget::onModelChanged);
    connect(m_adapter, &ModelRouterAdapter::statusChanged,
            this, &ModelRouterWidget::onStatusChanged);
    connect(m_adapter, &ModelRouterAdapter::costUpdated,
            this, &ModelRouterWidget::onCostUpdated);
    connect(m_adapter, &ModelRouterAdapter::statisticsUpdated,
            this, &ModelRouterWidget::onStatisticsUpdated);

    // Connect button signals
    connect(m_generate_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onGenerateButtonClicked);
    connect(m_stop_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onStopButtonClicked);
    connect(m_model_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelRouterWidget::onModelComboChanged);
    connect(m_settings_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onSettingsButtonClicked);
    connect(m_api_key_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onApiKeyButtonClicked);
    connect(m_dashboard_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onDashboardButtonClicked);
    connect(m_console_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onConsoleButtonClicked);
    connect(m_clear_output_button, &QPushButton::clicked,
            this, &ModelRouterWidget::onClearButtonClicked);
}

void ModelRouterWidget::refreshModelList()
{
    if (!m_adapter) return;

    m_model_combo->blockSignals(true);
    m_model_combo->clear();

    QStringList models = m_adapter->getAvailableModels();
    for (const auto& model : models) {
        m_model_combo->addItem(model);
    }

    QString active = m_adapter->getActiveModel();
    int index = m_model_combo->findText(active);
    if (index >= 0) {
        m_model_combo->setCurrentIndex(index);
    }

    m_model_combo->blockSignals(false);

    qDebug() << "[ModelRouterWidget::refreshModelList] Updated with" << models.size() << "models";
}

void ModelRouterWidget::setWidgetEnabled(bool enabled)
{
    m_widget_enabled = enabled;
    m_generate_button->setEnabled(enabled && !m_generation_active);
    m_model_combo->setEnabled(enabled);
    m_prompt_input->setEnabled(enabled);
    m_settings_button->setEnabled(enabled);
    m_api_key_button->setEnabled(enabled);
}

QString ModelRouterWidget::getSelectedModel() const
{
    return m_model_combo->currentText();
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
    }
}

void ModelRouterWidget::updateProgress(int percent)
{
    m_progress_bar->setValue(std::min(percent, 100));
}

void ModelRouterWidget::setStatusMessage(const QString& message)
{
    m_status_label->setText(message);
    m_status_label->setStyleSheet("color: #0066cc; font-weight: bold;");
}

void ModelRouterWidget::setErrorMessage(const QString& error)
{
    m_error_label->setText("⚠ " + error);
    m_error_label->setVisible(!error.isEmpty());
    if (!error.isEmpty()) {
        m_error_label->setStyleSheet(
            "QLabel {"
            "  background-color: #ffe6e6;"
            "  color: #cc0000;"
            "  padding: 8px;"
            "  border: 1px solid #ff9999;"
            "  border-radius: 4px;"
            "}"
        );
    }
}

void ModelRouterWidget::setLatencyDisplay(double latency_ms)
{
    if (latency_ms > 0) {
        m_latency_label->setText(QString("Latency: %1 ms").arg((int)latency_ms));
    } else {
        m_latency_label->setText("Latency: — ms");
    }
}

void ModelRouterWidget::setCostDisplay(double total_cost)
{
    m_cost_label->setText(QString("Cost: $%1").arg(total_cost, 0, 'f', 4));
    m_total_cost = total_cost;
}

void ModelRouterWidget::setSuccessRateDisplay(int percentage)
{
    if (percentage >= 0) {
        m_success_label->setText(QString("Success: %1%").arg(percentage));
    } else {
        m_success_label->setText("Success: —%");
    }
}

QString ModelRouterWidget::getPromptInput() const
{
    return m_prompt_input->toPlainText().trimmed();
}

void ModelRouterWidget::clearPromptInput()
{
    m_prompt_input->clear();
}

QString ModelRouterWidget::getGenerationOutput() const
{
    return m_output_display->toPlainText();
}

void ModelRouterWidget::setGenerationOutput(const QString& output)
{
    m_output_display->setPlainText(output);
}

void ModelRouterWidget::appendGenerationChunk(const QString& chunk)
{
    m_output_display->appendPlainText(chunk);
}

void ModelRouterWidget::clearOutput()
{
    m_output_display->clear();
}

// === Slot Implementations ===

void ModelRouterWidget::onGenerationStarted(const QString& model_name)
{
    qDebug() << "[ModelRouterWidget::onGenerationStarted]" << model_name;
    setGenerationActive(true);
    setStatusMessage(QString("Generating with %1...").arg(model_name));
    clearOutput();
    m_error_label->setVisible(false);
}

void ModelRouterWidget::onGenerationProgress(int percent)
{
    updateProgress(percent);
}

void ModelRouterWidget::onGenerationChunk(const QString& chunk)
{
    appendGenerationChunk(chunk);
}

void ModelRouterWidget::onGenerationComplete(const QString& result, int tokens_used, double latency_ms)
{
    qDebug() << "[ModelRouterWidget::onGenerationComplete]"
             << "tokens:" << tokens_used << "latency:" << latency_ms << "ms";
    
    setGenerationActive(false);
    setGenerationOutput(result);
    setStatusMessage(QString("Generated %1 tokens in %2ms").arg(tokens_used).arg((int)latency_ms));
    setLatencyDisplay(latency_ms);
    m_progress_bar->setValue(100);
    
    showTemporaryStatus("Generation complete!", 2000);
}

void ModelRouterWidget::onGenerationError(const QString& error)
{
    qWarning() << "[ModelRouterWidget::onGenerationError]" << error;
    setGenerationActive(false);
    setErrorMessage(error);
    setStatusMessage("Generation failed");
    emit errorOccurred(error);
}

void ModelRouterWidget::onModelListUpdated(const QStringList& models)
{
    qDebug() << "[ModelRouterWidget::onModelListUpdated]" << models.size() << "models";
    refreshModelList();
}

void ModelRouterWidget::onModelChanged(const QString& model)
{
    qDebug() << "[ModelRouterWidget::onModelChanged]" << model;
    
    int index = m_model_combo->findText(model);
    if (index >= 0) {
        m_model_combo->blockSignals(true);
        m_model_combo->setCurrentIndex(index);
        m_model_combo->blockSignals(false);
    }
}

void ModelRouterWidget::onStatusChanged(const QString& status)
{
    setStatusMessage(status);
}

void ModelRouterWidget::onCostUpdated(double total_cost)
{
    setCostDisplay(total_cost);
}

void ModelRouterWidget::onStatisticsUpdated(const QJsonObject& stats)
{
    setLatencyDisplay(stats.value("avg_latency_ms").toDouble());
    setSuccessRateDisplay(stats.value("success_rate").toInt());
}

// === Button Click Handlers ===

void ModelRouterWidget::onGenerateButtonClicked()
{
    QString prompt = getPromptInput();
    if (prompt.isEmpty()) {
        setErrorMessage("Please enter a prompt");
        return;
    }

    QString model = getSelectedModel();
    if (model.isEmpty()) {
        setErrorMessage("Please select a model");
        return;
    }

    qDebug() << "[ModelRouterWidget::onGenerateButtonClicked]"
             << "model:" << model << "prompt_length:" << prompt.length();
    
    emit generateRequested(prompt, model);
    
    if (m_adapter) {
        m_adapter->generateAsync(prompt, model);
    }
}

void ModelRouterWidget::onStopButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onStopButtonClicked]";
    emit stopRequested();
    setGenerationActive(false);
    setStatusMessage("Generation stopped by user");
}

void ModelRouterWidget::onModelComboChanged(int index)
{
    if (index < 0) return;
    
    QString model = m_model_combo->itemText(index);
    qDebug() << "[ModelRouterWidget::onModelComboChanged]" << model;
    
    if (m_adapter) {
        m_adapter->setDefaultModel(model);
    }
    
    emit modelChanged(model);
}

void ModelRouterWidget::onSettingsButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onSettingsButtonClicked]";
    emit settingsRequested();
}

void ModelRouterWidget::onApiKeyButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onApiKeyButtonClicked]";
    emit apiKeyEditRequested();
}

void ModelRouterWidget::onDashboardButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onDashboardButtonClicked]";
    emit dashboardRequested();
}

void ModelRouterWidget::onConsoleButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onConsoleButtonClicked]";
    emit consoleRequested();
}

void ModelRouterWidget::onClearButtonClicked()
{
    qDebug() << "[ModelRouterWidget::onClearButtonClicked]";
    clearOutput();
    emit clearOutputRequested();
}

void ModelRouterWidget::updateMetricsDisplay()
{
    if (!m_adapter) return;
    
    setLatencyDisplay(m_adapter->getAverageLatency());
    setSuccessRateDisplay(m_adapter->getSuccessRate());
    setCostDisplay(m_adapter->getTotalCost());
}

void ModelRouterWidget::resetUI()
{
    m_status_label->setText("Ready");
    m_error_label->setVisible(false);
    m_progress_bar->setValue(0);
    m_progress_bar->setVisible(false);
    setGenerationActive(false);
}

void ModelRouterWidget::showTemporaryStatus(const QString& message, int duration_ms)
{
    QString original_status = m_status_label->text();
    setStatusMessage(message);
    
    QTimer::singleShot(duration_ms, this, [this, original_status]() {
        setStatusMessage(original_status);
    });
}

#include "model_router_widget.moc"
