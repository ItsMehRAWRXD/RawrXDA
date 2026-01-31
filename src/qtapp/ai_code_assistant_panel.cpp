#include "ai_code_assistant_panel.h"
#include "ai_code_assistant.h"


AICodeAssistantPanel::AICodeAssistantPanel(void *parent)
    : QDockWidget("AI Code Assistant", parent),
      assistant_(nullptr),
      status_indicator_(nullptr),
      model_label_(nullptr),
      suggestion_type_selector_(nullptr),
      original_code_display_(nullptr),
      suggestion_display_(nullptr),
      explanation_display_(nullptr),
      temperature_slider_(nullptr),
      temperature_value_label_(nullptr),
      max_tokens_slider_(nullptr),
      max_tokens_value_label_(nullptr),
      progress_bar_(nullptr),
      latency_label_(nullptr),
      apply_button_(nullptr),
      clear_button_(nullptr),
      copy_button_(nullptr),
      export_button_(nullptr),
      suggestion_history_(nullptr),
      streaming_in_progress_(false)
{
    // Lightweight constructor - defer Qt widget creation
}

void AICodeAssistantPanel::initialize() {
    if (status_indicator_) return;  // Already initialized
    
    setupUI();
}

AICodeAssistantPanel::~AICodeAssistantPanel()
{
}

void AICodeAssistantPanel::setAssistant(AICodeAssistant *assistant)
{
    assistant_ = assistant;
    
    if (assistant_) {
        // Connect signals from assistant to panel using lambdas for signal signature compatibility
// Qt connect removed
            apply_button_->setEnabled(true);
            progress_bar_->setVisible(false);
            streaming_in_progress_ = false;
            status_indicator_->setText(std::string("Status: Suggestion received ✅"));
        });
// Qt connect removed
// Qt connect removed
        });
// Qt connect removed
// Qt connect removed
        });
        
        status_indicator_->setText("Status: Connected ✅");
        updateStatusIndicator(true);
    }
}

void AICodeAssistantPanel::setupUI()
{
    void *mainWidget = new void(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    
    // Status and controls group
    QGroupBox *controlsGroup = new QGroupBox("Controls");
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsGroup);
    
    // Suggestion type selector
    QHBoxLayout *typeLayout = new QHBoxLayout();
    QLabel *typeLabel = new QLabel("Suggestion Type:");
    suggestion_type_selector_ = new QComboBox();
    suggestion_type_selector_->addItem("Code Completion", "completion");
    suggestion_type_selector_->addItem("Refactoring", "refactoring");
    suggestion_type_selector_->addItem("Explanation", "explanation");
    suggestion_type_selector_->addItem("Bug Fix", "bugfix");
    suggestion_type_selector_->addItem("Optimization", "optimization");
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(suggestion_type_selector_);
    controlsLayout->addLayout(typeLayout);
    
    // Temperature slider
    QHBoxLayout *tempLayout = new QHBoxLayout();
    QLabel *tempLabel = new QLabel("Temperature:");
    temperature_slider_ = new QSlider(//Horizontal);
    temperature_slider_->setRange(0, 100);
    temperature_slider_->setValue(70);
    temperature_value_label_ = new QLabel("0.7");
// Qt connect removed
        onTemperatureChanged(value);
    });
    tempLayout->addWidget(tempLabel);
    tempLayout->addWidget(temperature_slider_);
    tempLayout->addWidget(temperature_value_label_);
    controlsLayout->addLayout(tempLayout);
    
    // Status and latency
    status_indicator_ = new QLabel("Status: Disconnected ❌");
    latency_label_ = new QLabel("Latency: -- ms");
    model_label_ = new QLabel("Model: --");
    controlsLayout->addWidget(status_indicator_);
    controlsLayout->addWidget(latency_label_);
    controlsLayout->addWidget(model_label_);
    
    mainLayout->addWidget(controlsGroup);
    
    // Suggestion display area
    QGroupBox *suggestionGroup = new QGroupBox("AI Suggestion");
    QVBoxLayout *suggestionLayout = new QVBoxLayout(suggestionGroup);
    
    suggestion_display_ = new QTextEdit();
    suggestion_display_->setReadOnly(true);
    suggestion_display_->setPlaceholderText("AI suggestions will appear here...");
    suggestionLayout->addWidget(suggestion_display_);
    
    mainLayout->addWidget(suggestionGroup);
    
    // Progress bar
    progress_bar_ = new QProgressBar();
    progress_bar_->setRange(0, 0);
    progress_bar_->setVisible(false);
    mainLayout->addWidget(progress_bar_);
    
    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    apply_button_ = new QPushButton("Apply Suggestion");
    apply_button_->setEnabled(false);
// Qt connect removed
    clear_button_ = new QPushButton("Clear");
// Qt connect removed
    copy_button_ = new QPushButton("Copy to Clipboard");
// Qt connect removed
    export_button_ = new QPushButton("Export");
// Qt connect removed
    buttonLayout->addWidget(apply_button_);
    buttonLayout->addWidget(clear_button_);
    buttonLayout->addWidget(copy_button_);
    buttonLayout->addWidget(export_button_);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    
    setWidget(mainWidget);
}

// Slots - AI Response

void AICodeAssistantPanel::onSuggestionReady(const int dummy)
{
    // This slot matches the header signature but is not actually used.
    // Signal connections are handled in setAssistant() using lambdas.
    (dummy);
}

void AICodeAssistantPanel::onSuggestionStreaming(const std::string &partial)
{
    suggestion_display_->insertPlainText(partial);
    progress_bar_->setVisible(true);
    streaming_in_progress_ = true;
}

void AICodeAssistantPanel::onSuggestionStreamComplete()
{
    progress_bar_->setVisible(false);
    apply_button_->setEnabled(true);
    status_indicator_->setText("Status: Streaming complete ✅");
    streaming_in_progress_ = false;
}

void AICodeAssistantPanel::onError(const std::string &error)
{
    suggestion_display_->setPlainText(std::string("❌ Error: %1"));
    status_indicator_->setText("Status: Error ❌");
    progress_bar_->setVisible(false);
    apply_button_->setEnabled(false);
    streaming_in_progress_ = false;
    
}

void AICodeAssistantPanel::onConnectionStatusChanged(bool connected)
{
    updateStatusIndicator(connected);
}

void AICodeAssistantPanel::onLatencyMeasured(int latency_ms)
{
    latency_label_->setText(formatLatency(latency_ms));
    
    // Color-code latency
    if (latency_ms < 500) {
        latency_label_->setStyleSheet("color: green;");
    } else if (latency_ms < 2000) {
        latency_label_->setStyleSheet("color: orange;");
    } else {
        latency_label_->setStyleSheet("color: red;");
    }
}

// Slots - User Actions

void AICodeAssistantPanel::onApplySuggestion()
{
    std::string suggestion = suggestion_display_->toPlainText();
    if (!suggestion.isEmpty()) {
        // In production, this would apply the suggestion to the editor
        // For now, just log it
    }
}

void AICodeAssistantPanel::onClearPanel()
{
    suggestion_display_->clear();
    apply_button_->setEnabled(false);
    latency_label_->setText("Latency: -- ms");
}

void AICodeAssistantPanel::onCopyToClipboard()
{
    std::string suggestion = suggestion_display_->toPlainText();
    if (!suggestion.isEmpty()) {
        QClipboard *clipboard = nullptr;
        clipboard->setText(suggestion);
        status_indicator_->setText("Status: Copied to clipboard ✅");
    }
}

void AICodeAssistantPanel::onExportSuggestion()
{
    std::string filePath = QFileDialog::getSaveFileName(this, "Export Suggestion", "", "Text Files (*.txt);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        std::fstream file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << suggestion_display_->toPlainText();
            file.close();
            
            status_indicator_->setText("Status: Exported successfully ✅");
        } else {
            onError("Failed to export suggestion");
        }
    }
}

void AICodeAssistantPanel::onTemperatureChanged(int value)
{
    if (assistant_) {
        assistant_->setTemperature(value / 100.0f);
    }
}

void AICodeAssistantPanel::onMaxTokensChanged(int value)
{
    if (assistant_) {
        assistant_->setMaxTokens(value);
    }
}

// Private helper methods

void AICodeAssistantPanel::updateStatusIndicator(bool connected)
{
    if (connected) {
        status_indicator_->setText("Status: Connected ✅");
        status_indicator_->setStyleSheet("color: green;");
    } else {
        status_indicator_->setText("Status: Disconnected ❌");
        status_indicator_->setStyleSheet("color: red;");
    }
}

std::string AICodeAssistantPanel::formatLatency(int ms)
{
    if (ms < 1000) {
        return std::string("Latency: %1 ms");
    } else {
        return std::string("Latency: %1.%2 s") / 100);
    }
}

