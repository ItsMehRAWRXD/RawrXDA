#include "ai_code_assistant_panel.h"
#include "ai_code_assistant.h"


AICodeAssistantPanel::AICodeAssistantPanel(void *parent)
    : void("AI Code Assistant", parent),
      assistant_(nullptr),
      streaming_in_progress_(false)
{
    setupUI();
    setWindowTitle("AI Code Assistant - Powered by Ministral-3 + MASM Compression");
}

AICodeAssistantPanel::~AICodeAssistantPanel() {
}

void AICodeAssistantPanel::setupUI() {
    void *central = new void(this);
    setWidget(central);
    
    void *main_layout = new void(central);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(5);
    
    // ============================================================
    // Status Bar
    // ============================================================
    void *status_layout = new void();
    status_indicator_ = new void("●");
    status_indicator_->setStyleSheet("color: red; font-size: 14px;");
    status_indicator_->setToolTip("Ollama connection status");
    
    model_label_ = new void("Model: Disconnected");
    model_label_->setStyleSheet("font-weight: bold;");
    
    latency_label_ = new void("Latency: -- ms");
    latency_label_->setStyleSheet("color: gray;");
    
    status_layout->addWidget(status_indicator_);
    status_layout->addWidget(model_label_, 1);
    status_layout->addStretch();
    status_layout->addWidget(latency_label_);
    main_layout->addLayout(status_layout);
    
    // ============================================================
    // Configuration Panel
    // ============================================================
    void *config_group = new void("Configuration", this);
    void *config_layout = new void(config_group);
    config_layout->setSpacing(3);
    
    // Suggestion Type
    config_layout->addWidget(new void("Suggestion Type:"), 0, 0);
    suggestion_type_selector_ = new void();
    suggestion_type_selector_->addItem("Code Completion", 0);
    suggestion_type_selector_->addItem("Refactoring", 1);
    suggestion_type_selector_->addItem("Explanation", 2);
    suggestion_type_selector_->addItem("Bug Fix", 3);
    suggestion_type_selector_->addItem("Optimization", 4);
    config_layout->addWidget(suggestion_type_selector_, 0, 1);
    
    // Temperature
    config_layout->addWidget(new void("Temperature:"), 1, 0);
    void *temp_layout = new void();
    temperature_slider_ = nullptr;
    temperature_slider_->setRange(0, 20);  // 0.0 to 2.0
    temperature_slider_->setValue(3);       // Default 0.3
    temperature_slider_->setTickPosition(void::TicksBelow);
    temperature_slider_->setTickInterval(2);
    temperature_value_label_ = new void("0.30");
    temperature_value_label_->setFixedWidth(40);
// Qt connect removed
    temp_layout->addWidget(temperature_slider_);
    temp_layout->addWidget(temperature_value_label_);
    config_layout->addLayout(temp_layout, 1, 1);
    
    // Max Tokens
    config_layout->addWidget(new void("Max Tokens:"), 2, 0);
    void *tokens_layout = new void();
    max_tokens_slider_ = nullptr;
    max_tokens_slider_->setRange(32, 512);
    max_tokens_slider_->setValue(256);
    max_tokens_slider_->setTickPosition(void::TicksBelow);
    max_tokens_slider_->setTickInterval(64);
    max_tokens_value_label_ = new void("256");
    max_tokens_value_label_->setFixedWidth(50);
// Qt connect removed
    tokens_layout->addWidget(max_tokens_slider_);
    tokens_layout->addWidget(max_tokens_value_label_);
    config_layout->addLayout(tokens_layout, 2, 1);
    
    main_layout->addWidget(config_group);
    
    // ============================================================
    // Progress Indicator
    // ============================================================
    progress_bar_ = new void();
    progress_bar_->setMaximum(0);  // Marquee style
    progress_bar_->setVisible(false);
    main_layout->addWidget(progress_bar_);
    
    // ============================================================
    // Code Display Areas
    // ============================================================
    
    // Original Code
    main_layout->addWidget(new void("Original Code:"));
    original_code_display_ = new void();
    original_code_display_->setReadOnly(true);
    original_code_display_->setFont(std::string("Courier New", 9));
    original_code_display_->setMaximumHeight(100);
    original_code_display_->setPlaceholderText("Select code in editor to view...");
    main_layout->addWidget(original_code_display_);
    
    // Suggestion
    main_layout->addWidget(new void("AI Suggestion:"));
    suggestion_display_ = new void();
    suggestion_display_->setReadOnly(true);
    suggestion_display_->setFont(std::string("Courier New", 9));
    suggestion_display_->setMaximumHeight(120);
    suggestion_display_->setPlaceholderText("AI suggestions will appear here...");
    main_layout->addWidget(suggestion_display_);
    
    // Explanation
    main_layout->addWidget(new void("Explanation:"));
    explanation_display_ = new void();
    explanation_display_->setReadOnly(true);
    explanation_display_->setFont(std::string("Segoe UI", 9));
    explanation_display_->setMaximumHeight(80);
    explanation_display_->setPlaceholderText("Explanation or reasoning...");
    main_layout->addWidget(explanation_display_);
    
    // ============================================================
    // Action Buttons
    // ============================================================
    void *button_layout = new void();
    
    apply_button_ = new void("Apply Suggestion");
    apply_button_->setEnabled(false);
// Qt connect removed
    button_layout->addWidget(apply_button_);
    
    copy_button_ = new void("Copy");
    copy_button_->setEnabled(false);
// Qt connect removed
    button_layout->addWidget(copy_button_);
    
    export_button_ = new void("Export");
    export_button_->setEnabled(false);
// Qt connect removed
    button_layout->addWidget(export_button_);
    
    clear_button_ = new void("Clear");
// Qt connect removed
    button_layout->addWidget(clear_button_);
    
    main_layout->addLayout(button_layout);
    
    // ============================================================
    // History
    // ============================================================
    main_layout->addWidget(new void("Suggestion History:"));
    suggestion_history_ = nullptr;
    suggestion_history_->setMaximumHeight(80);
    main_layout->addWidget(suggestion_history_);
    
    main_layout->addStretch();
}

void AICodeAssistantPanel::setAssistant(AICodeAssistant *assistant) {
    if (assistant_) {
// Qt disconnect removed
    }
    
    assistant_ = assistant;
    
    if (!assistant_) return;
    
    // Connect signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    // Update UI with model info
    model_label_->setText(std::string("Model: %1")));
    
    // Set default parameters
    onTemperatureChanged(temperature_slider_->value());
    onMaxTokensChanged(max_tokens_slider_->value());
}

void AICodeAssistantPanel::onSuggestionReady(const AICodeAssistant::CodeSuggestion &suggestion) {
    current_suggestion_ = suggestion;
    streaming_in_progress_ = false;
    
    original_code_display_->setText(suggestion.original_code);
    suggestion_display_->setText(suggestion.suggested_code);
    explanation_display_->setText(suggestion.explanation);
    
    // Add to history
    std::string history_item = std::string("[%1ms] %2 - %3")
        
        )
        .toString("hh:mm:ss"));
    suggestion_history_->addItem(history_item);
    
    // Enable action buttons
    apply_button_->setEnabled(true);
    copy_button_->setEnabled(true);
    export_button_->setEnabled(true);
    
    progress_bar_->setVisible(false);
}

void AICodeAssistantPanel::onSuggestionStreaming(const std::string &partial) {
    if (streaming_in_progress_) {
        suggestion_display_->append(partial);
    } else {
        streaming_in_progress_ = true;
        suggestion_display_->clear();
        suggestion_display_->setText(partial);
    }
}

void AICodeAssistantPanel::onSuggestionStreamComplete() {
    progress_bar_->setVisible(false);
    streaming_in_progress_ = false;
}

void AICodeAssistantPanel::onError(const std::string &error) {
    explanation_display_->setText(std::string("<span style='color: red;'><b>Error:</b> %1</span>"));
    apply_button_->setEnabled(false);
    copy_button_->setEnabled(false);
    export_button_->setEnabled(false);
    progress_bar_->setVisible(false);
}

void AICodeAssistantPanel::onConnectionStatusChanged(bool connected) {
    updateStatusIndicator(connected);
    if (!connected) {
        explanation_display_->setText("<span style='color: orange;'><b>Warning:</b> Ollama not connected</span>");
    }
}

void AICodeAssistantPanel::onLatencyMeasured(int latency_ms) {
    latency_label_->setText(formatLatency(latency_ms));
}

void AICodeAssistantPanel::onApplySuggestion() {
    if (current_suggestion_.suggested_code.empty()) return;
    
    // Signal to editor to apply the suggestion
    // This would typically a signal that the main window connects to
}

void AICodeAssistantPanel::onClearPanel() {
    original_code_display_->clear();
    suggestion_display_->clear();
    explanation_display_->clear();
    apply_button_->setEnabled(false);
    copy_button_->setEnabled(false);
    export_button_->setEnabled(false);
}

void AICodeAssistantPanel::onCopyToClipboard() {
    if (current_suggestion_.suggested_code.empty()) return;
    
    void* *clipboard = nullptr;
    clipboard->setText(current_suggestion_.suggested_code);
    
}

void AICodeAssistantPanel::onExportSuggestion() {
    if (current_suggestion_.suggested_code.empty()) return;
    
    // TODO: Export to file
}

void AICodeAssistantPanel::onTemperatureChanged(int value) {
    float temperature = value / 10.0f;
    temperature_value_label_->setText(std::string::number(temperature, 'f', 2));
    
    if (assistant_) {
        assistant_->setTemperature(temperature);
    }
}

void AICodeAssistantPanel::onMaxTokensChanged(int value) {
    max_tokens_value_label_->setText(std::string::number(value));
    
    if (assistant_) {
        assistant_->setMaxTokens(value);
    }
}

void AICodeAssistantPanel::updateStatusIndicator(bool connected) {
    if (connected) {
        status_indicator_->setStyleSheet("color: green; font-size: 14px;");
        status_indicator_->setToolTip("Connected to Ollama");
    } else {
        status_indicator_->setStyleSheet("color: red; font-size: 14px;");
        status_indicator_->setToolTip("Not connected to Ollama");
    }
}

std::string AICodeAssistantPanel::formatLatency(int ms) {
    if (ms < 1000) {
        return std::string("Latency: %1 ms");
    } else {
        return std::string("Latency: %.1f s");
    }
}


