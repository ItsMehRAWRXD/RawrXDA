#include "ai_code_assistant_panel.h"
#include "ai_code_assistant.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSlider>
#include <QSpinBox>
#include <QListWidget>
#include <QProgressBar>
#include <QClipboard>
#include <QApplication>
#include <QDebug>
#include <QDateTime>

AICodeAssistantPanel::AICodeAssistantPanel(QWidget *parent)
    : QDockWidget("AI Code Assistant", parent),
      assistant_(nullptr),
      streaming_in_progress_(false)
{
    setupUI();
    setWindowTitle("AI Code Assistant - Powered by Ministral-3 + MASM Compression");
}

AICodeAssistantPanel::~AICodeAssistantPanel() {
}

void AICodeAssistantPanel::setupUI() {
    QWidget *central = new QWidget(this);
    setWidget(central);
    
    QVBoxLayout *main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(5);
    
    // ============================================================
    // Status Bar
    // ============================================================
    QHBoxLayout *status_layout = new QHBoxLayout();
    status_indicator_ = new QLabel("●");
    status_indicator_->setStyleSheet("color: red; font-size: 14px;");
    status_indicator_->setToolTip("Ollama connection status");
    
    model_label_ = new QLabel("Model: Disconnected");
    model_label_->setStyleSheet("font-weight: bold;");
    
    latency_label_ = new QLabel("Latency: -- ms");
    latency_label_->setStyleSheet("color: gray;");
    
    status_layout->addWidget(status_indicator_);
    status_layout->addWidget(model_label_, 1);
    status_layout->addStretch();
    status_layout->addWidget(latency_label_);
    main_layout->addLayout(status_layout);
    
    // ============================================================
    // Configuration Panel
    // ============================================================
    QGroupBox *config_group = new QGroupBox("Configuration", this);
    QGridLayout *config_layout = new QGridLayout(config_group);
    config_layout->setSpacing(3);
    
    // Suggestion Type
    config_layout->addWidget(new QLabel("Suggestion Type:"), 0, 0);
    suggestion_type_selector_ = new QComboBox();
    suggestion_type_selector_->addItem("Code Completion", 0);
    suggestion_type_selector_->addItem("Refactoring", 1);
    suggestion_type_selector_->addItem("Explanation", 2);
    suggestion_type_selector_->addItem("Bug Fix", 3);
    suggestion_type_selector_->addItem("Optimization", 4);
    config_layout->addWidget(suggestion_type_selector_, 0, 1);
    
    // Temperature
    config_layout->addWidget(new QLabel("Temperature:"), 1, 0);
    QHBoxLayout *temp_layout = new QHBoxLayout();
    temperature_slider_ = new QSlider(Qt::Horizontal);
    temperature_slider_->setRange(0, 20);  // 0.0 to 2.0
    temperature_slider_->setValue(3);       // Default 0.3
    temperature_slider_->setTickPosition(QSlider::TicksBelow);
    temperature_slider_->setTickInterval(2);
    temperature_value_label_ = new QLabel("0.30");
    temperature_value_label_->setFixedWidth(40);
    connect(temperature_slider_, QOverload<int>::of(&QSlider::valueChanged),
            this, &AICodeAssistantPanel::onTemperatureChanged);
    temp_layout->addWidget(temperature_slider_);
    temp_layout->addWidget(temperature_value_label_);
    config_layout->addLayout(temp_layout, 1, 1);
    
    // Max Tokens
    config_layout->addWidget(new QLabel("Max Tokens:"), 2, 0);
    QHBoxLayout *tokens_layout = new QHBoxLayout();
    max_tokens_slider_ = new QSlider(Qt::Horizontal);
    max_tokens_slider_->setRange(32, 512);
    max_tokens_slider_->setValue(256);
    max_tokens_slider_->setTickPosition(QSlider::TicksBelow);
    max_tokens_slider_->setTickInterval(64);
    max_tokens_value_label_ = new QLabel("256");
    max_tokens_value_label_->setFixedWidth(50);
    connect(max_tokens_slider_, QOverload<int>::of(&QSlider::valueChanged),
            this, &AICodeAssistantPanel::onMaxTokensChanged);
    tokens_layout->addWidget(max_tokens_slider_);
    tokens_layout->addWidget(max_tokens_value_label_);
    config_layout->addLayout(tokens_layout, 2, 1);
    
    main_layout->addWidget(config_group);
    
    // ============================================================
    // Progress Indicator
    // ============================================================
    progress_bar_ = new QProgressBar();
    progress_bar_->setMaximum(0);  // Marquee style
    progress_bar_->setVisible(false);
    main_layout->addWidget(progress_bar_);
    
    // ============================================================
    // Code Display Areas
    // ============================================================
    
    // Original Code
    main_layout->addWidget(new QLabel("Original Code:"));
    original_code_display_ = new QTextEdit();
    original_code_display_->setReadOnly(true);
    original_code_display_->setFont(QFont("Courier New", 9));
    original_code_display_->setMaximumHeight(100);
    original_code_display_->setPlaceholderText("Select code in editor to view...");
    main_layout->addWidget(original_code_display_);
    
    // Suggestion
    main_layout->addWidget(new QLabel("AI Suggestion:"));
    suggestion_display_ = new QTextEdit();
    suggestion_display_->setReadOnly(true);
    suggestion_display_->setFont(QFont("Courier New", 9));
    suggestion_display_->setMaximumHeight(120);
    suggestion_display_->setPlaceholderText("AI suggestions will appear here...");
    main_layout->addWidget(suggestion_display_);
    
    // Explanation
    main_layout->addWidget(new QLabel("Explanation:"));
    explanation_display_ = new QTextEdit();
    explanation_display_->setReadOnly(true);
    explanation_display_->setFont(QFont("Segoe UI", 9));
    explanation_display_->setMaximumHeight(80);
    explanation_display_->setPlaceholderText("Explanation or reasoning...");
    main_layout->addWidget(explanation_display_);
    
    // ============================================================
    // Action Buttons
    // ============================================================
    QHBoxLayout *button_layout = new QHBoxLayout();
    
    apply_button_ = new QPushButton("Apply Suggestion");
    apply_button_->setEnabled(false);
    connect(apply_button_, &QPushButton::clicked, this, &AICodeAssistantPanel::onApplySuggestion);
    button_layout->addWidget(apply_button_);
    
    copy_button_ = new QPushButton("Copy");
    copy_button_->setEnabled(false);
    connect(copy_button_, &QPushButton::clicked, this, &AICodeAssistantPanel::onCopyToClipboard);
    button_layout->addWidget(copy_button_);
    
    export_button_ = new QPushButton("Export");
    export_button_->setEnabled(false);
    connect(export_button_, &QPushButton::clicked, this, &AICodeAssistantPanel::onExportSuggestion);
    button_layout->addWidget(export_button_);
    
    clear_button_ = new QPushButton("Clear");
    connect(clear_button_, &QPushButton::clicked, this, &AICodeAssistantPanel::onClearPanel);
    button_layout->addWidget(clear_button_);
    
    main_layout->addLayout(button_layout);
    
    // ============================================================
    // History
    // ============================================================
    main_layout->addWidget(new QLabel("Suggestion History:"));
    suggestion_history_ = new QListWidget();
    suggestion_history_->setMaximumHeight(80);
    main_layout->addWidget(suggestion_history_);
    
    main_layout->addStretch();
}

void AICodeAssistantPanel::setAssistant(AICodeAssistant *assistant) {
    if (assistant_) {
        disconnect(assistant_, nullptr, this, nullptr);
    }
    
    assistant_ = assistant;
    
    if (!assistant_) return;
    
    // Connect signals
    connect(assistant_, &AICodeAssistant::suggestionReady,
            this, &AICodeAssistantPanel::onSuggestionReady);
    connect(assistant_, &AICodeAssistant::suggestionStreaming,
            this, &AICodeAssistantPanel::onSuggestionStreaming);
    connect(assistant_, &AICodeAssistant::suggestionStreamComplete,
            this, &AICodeAssistantPanel::onSuggestionStreamComplete);
    connect(assistant_, &AICodeAssistant::error,
            this, &AICodeAssistantPanel::onError);
    connect(assistant_, &AICodeAssistant::connectionStatusChanged,
            this, &AICodeAssistantPanel::onConnectionStatusChanged);
    connect(assistant_, &AICodeAssistant::latencyMeasured,
            this, &AICodeAssistantPanel::onLatencyMeasured);
    
    // Update UI with model info
    model_label_->setText(QString("Model: %1").arg(assistant_->getModelInfo()));
    
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
    QString history_item = QString("[%1ms] %2 - %3")
        .arg(suggestion.latency_ms)
        .arg(suggestion_type_selector_->currentText())
        .arg(QDateTime::currentTime().toString("hh:mm:ss"));
    suggestion_history_->addItem(history_item);
    
    // Enable action buttons
    apply_button_->setEnabled(true);
    copy_button_->setEnabled(true);
    export_button_->setEnabled(true);
    
    progress_bar_->setVisible(false);
}

void AICodeAssistantPanel::onSuggestionStreaming(const QString &partial) {
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

void AICodeAssistantPanel::onError(const QString &error) {
    explanation_display_->setText(QString("<span style='color: red;'><b>Error:</b> %1</span>").arg(error));
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
    if (current_suggestion_.suggested_code.isEmpty()) return;
    
    // Emit signal for the main window to apply the code change to the active editor
    emit applySuggestionRequested(current_suggestion_.suggested_code, 
                                  current_suggestion_.original_code);
    
    // Update UI to show applied state
    apply_button_->setText("Applied ✓");
    apply_button_->setEnabled(false);
    
    // Reset button text after delay
    QTimer::singleShot(2000, this, [this]() {
        apply_button_->setText("Apply Suggestion");
        apply_button_->setEnabled(!current_suggestion_.suggested_code.isEmpty());
    });
    
    qDebug() << "[AICodeAssistantPanel] Suggestion applied to editor";
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
    if (current_suggestion_.suggested_code.isEmpty()) return;
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(current_suggestion_.suggested_code);
    
    qDebug() << "[AICodeAssistantPanel] Copied suggestion to clipboard";
}

void AICodeAssistantPanel::onExportSuggestion() {
    if (current_suggestion_.suggested_code.isEmpty()) return;
    
    // Open file save dialog
    QString filePath = QFileDialog::getSaveFileName(this, 
        "Export AI Suggestion", 
        QDir::homePath() + "/ai_suggestion.txt",
        "Source Files (*.cpp *.hpp *.h *.py *.js *.ts);;Text Files (*.txt);;All Files (*)");
    
    if (filePath.isEmpty()) return;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AICodeAssistantPanel] Failed to open file for export:" << filePath;
        return;
    }
    
    QTextStream stream(&file);
    stream << "// AI Code Suggestion - Generated by RawrXD IDE\n";
    stream << "// Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    stream << "// Confidence: " << QString::number(current_suggestion_.confidence, 'f', 2) << "\n";
    stream << "//\n";
    if (!current_suggestion_.explanation.isEmpty()) {
        stream << "// Explanation: " << current_suggestion_.explanation << "\n";
    }
    stream << "\n";
    stream << current_suggestion_.suggested_code;
    file.close();
    
    qDebug() << "[AICodeAssistantPanel] Suggestion exported to:" << filePath;
}

void AICodeAssistantPanel::onTemperatureChanged(int value) {
    float temperature = value / 10.0f;
    temperature_value_label_->setText(QString::number(temperature, 'f', 2));
    
    if (assistant_) {
        assistant_->setTemperature(temperature);
    }
}

void AICodeAssistantPanel::onMaxTokensChanged(int value) {
    max_tokens_value_label_->setText(QString::number(value));
    
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

QString AICodeAssistantPanel::formatLatency(int ms) {
    if (ms < 1000) {
        return QString("Latency: %1 ms").arg(ms);
    } else {
        return QString("Latency: %.1f s").arg(ms / 1000.0);
    }
}
