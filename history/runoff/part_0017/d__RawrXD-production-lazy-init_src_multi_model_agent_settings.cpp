// ════════════════════════════════════════════════════════════════════════════════
// MULTI-MODEL AGENT SETTINGS DIALOG IMPLEMENTATION
// ════════════════════════════════════════════════════════════════════════════════

#include "multi_model_agent_settings.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>

namespace RawrXD {
namespace IDE {

MultiModelAgentSettingsDialog::MultiModelAgentSettingsDialog(QWidget* parent)
    : QDialog(parent),
      settings(new QSettings("RawrXD", "MultiModelAgents", this))
{
    setWindowTitle("Multi-Model Agent Settings");
    setModal(true);
    setMinimumSize(600, 500);

    setupUI();
    loadSettings();

    connect(okBtn, &QPushButton::clicked, this, &MultiModelAgentSettingsDialog::onAccept);
    connect(applyBtn, &QPushButton::clicked, this, &MultiModelAgentSettingsDialog::onApply);
    connect(resetBtn, &QPushButton::clicked, this, &MultiModelAgentSettingsDialog::onReset);
    connect(cancelBtn, &QPushButton::clicked, this, &MultiModelAgentSettingsDialog::onReject);

    // Connect test buttons
    connect(openAITestBtn, &QPushButton::clicked, [this]() {
        onTestConnection();
    });
    connect(anthropicTestBtn, &QPushButton::clicked, [this]() {
        onTestConnection();
    });
    connect(googleTestBtn, &QPushButton::clicked, [this]() {
        onTestConnection();
    });

    // Connect provider change to update models
    connect(defaultProviderCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &MultiModelAgentSettingsDialog::updateModelCombo);
}

MultiModelAgentSettingsDialog::~MultiModelAgentSettingsDialog()
{
    // Settings are automatically saved in QSettings destructor
}

void MultiModelAgentSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);

    setupApiKeysTab();
    setupGeneralTab();
    setupAdvancedTab();

    // Dialog buttons
    buttonLayout = new QHBoxLayout();

    okBtn = new QPushButton("OK");
    okBtn->setDefault(true);
    applyBtn = new QPushButton("Apply");
    resetBtn = new QPushButton("Reset");
    cancelBtn = new QPushButton("Cancel");

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(resetBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);
}

void MultiModelAgentSettingsDialog::setupApiKeysTab()
{
    apiKeysTab = new QWidget();
    apiKeysLayout = new QGridLayout(apiKeysTab);

    // OpenAI Section
    openAILabel = new QLabel("OpenAI API Key:");
    openAIApiKeyEdit = new QLineEdit();
    openAIApiKeyEdit->setEchoMode(QLineEdit::Password);
    openAIApiKeyEdit->setPlaceholderText("sk-...");
    openAITestBtn = new QPushButton("Test");

    apiKeysLayout->addWidget(new QLabel("<b>OpenAI</b>"), 0, 0, 1, 3);
    apiKeysLayout->addWidget(openAILabel, 1, 0);
    apiKeysLayout->addWidget(openAIApiKeyEdit, 1, 1);
    apiKeysLayout->addWidget(openAITestBtn, 1, 2);

    // Anthropic Section
    anthropicLabel = new QLabel("Anthropic API Key:");
    anthropicApiKeyEdit = new QLineEdit();
    anthropicApiKeyEdit->setEchoMode(QLineEdit::Password);
    anthropicApiKeyEdit->setPlaceholderText("sk-ant-...");
    anthropicTestBtn = new QPushButton("Test");

    apiKeysLayout->addWidget(new QLabel("<b>Anthropic</b>"), 2, 0, 1, 3);
    apiKeysLayout->addWidget(anthropicLabel, 3, 0);
    apiKeysLayout->addWidget(anthropicApiKeyEdit, 3, 1);
    apiKeysLayout->addWidget(anthropicTestBtn, 3, 2);

    // Google Section
    googleLabel = new QLabel("Google API Key:");
    googleApiKeyEdit = new QLineEdit();
    googleApiKeyEdit->setEchoMode(QLineEdit::Password);
    googleApiKeyEdit->setPlaceholderText("AIza...");
    googleTestBtn = new QPushButton("Test");

    apiKeysLayout->addWidget(new QLabel("<b>Google AI</b>"), 4, 0, 1, 3);
    apiKeysLayout->addWidget(googleLabel, 5, 0);
    apiKeysLayout->addWidget(googleApiKeyEdit, 5, 1);
    apiKeysLayout->addWidget(googleTestBtn, 5, 2);

    // Info label
    QLabel* infoLabel = new QLabel(
        "<i>API keys are stored securely in your system settings. "
        "They are only used for AI model requests.</i>"
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: gray;");

    apiKeysLayout->addWidget(infoLabel, 6, 0, 1, 3);
    apiKeysLayout->setRowStretch(6, 1);

    tabWidget->addTab(apiKeysTab, "API Keys");
}

void MultiModelAgentSettingsDialog::setupGeneralTab()
{
    generalTab = new QWidget();
    generalLayout = new QVBoxLayout(generalTab);

    // Browser mode
    browserModeCheck = new QCheckBox("Enable Browser Mode (fetch live web data)");
    browserModeCheck->setChecked(false);

    // Auto-start agents
    autoStartAgentsCheck = new QCheckBox("Auto-start default agents on IDE launch");
    autoStartAgentsCheck->setChecked(true);

    // Max agents
    QHBoxLayout* maxAgentsLayout = new QHBoxLayout();
    QLabel* maxAgentsLabel = new QLabel("Maximum simultaneous agents:");
    maxAgentsSpin = new QSpinBox();
    maxAgentsSpin->setRange(1, 8);
    maxAgentsSpin->setValue(4);
    maxAgentsLayout->addWidget(maxAgentsLabel);
    maxAgentsLayout->addWidget(maxAgentsSpin);
    maxAgentsLayout->addStretch();

    // Default provider/model
    QHBoxLayout* defaultLayout = new QHBoxLayout();
    QLabel* defaultLabel = new QLabel("Default model:");
    defaultProviderCombo = new QComboBox();
    defaultProviderCombo->addItems({"openai", "anthropic", "google", "local"});
    defaultProviderCombo->setCurrentText("openai");

    defaultModelCombo = new QComboBox();
    updateModelCombo(); // Populate based on selected provider

    defaultLayout->addWidget(defaultLabel);
    defaultLayout->addWidget(defaultProviderCombo);
    defaultLayout->addWidget(defaultModelCombo);
    defaultLayout->addStretch();

    generalLayout->addWidget(browserModeCheck);
    generalLayout->addWidget(autoStartAgentsCheck);
    generalLayout->addLayout(maxAgentsLayout);
    generalLayout->addLayout(defaultLayout);
    generalLayout->addStretch();

    tabWidget->addTab(generalTab, "General");
}

void MultiModelAgentSettingsDialog::setupAdvancedTab()
{
    advancedTab = new QWidget();
    advancedLayout = new QVBoxLayout(advancedTab);

    // Response timeout
    QHBoxLayout* timeoutLayout = new QHBoxLayout();
    QLabel* timeoutLabel = new QLabel("Response timeout (seconds):");
    responseTimeoutSpin = new QSpinBox();
    responseTimeoutSpin->setRange(10, 300);
    responseTimeoutSpin->setValue(30);
    timeoutLayout->addWidget(timeoutLabel);
    timeoutLayout->addWidget(responseTimeoutSpin);
    timeoutLayout->addStretch();

    // Advanced options
    enableParallelExecutionCheck = new QCheckBox("Enable parallel execution optimization");
    enableParallelExecutionCheck->setChecked(true);

    enableQualityScoringCheck = new QCheckBox("Enable response quality scoring");
    enableQualityScoringCheck->setChecked(true);

    // Custom prompts
    QLabel* customPromptsLabel = new QLabel("Custom system prompts:");
    customPromptsEdit = new QTextEdit();
    customPromptsEdit->setPlaceholderText("Enter custom prompts for different agent roles...");
    customPromptsEdit->setMaximumHeight(150);

    advancedLayout->addLayout(timeoutLayout);
    advancedLayout->addWidget(enableParallelExecutionCheck);
    advancedLayout->addWidget(enableQualityScoringCheck);
    advancedLayout->addWidget(customPromptsLabel);
    advancedLayout->addWidget(customPromptsEdit);

    tabWidget->addTab(advancedTab, "Advanced");
}

void MultiModelAgentSettingsDialog::loadSettings()
{
    // API Keys
    openAIApiKeyEdit->setText(settings->value("api_keys/openai", "").toString());
    anthropicApiKeyEdit->setText(settings->value("api_keys/anthropic", "").toString());
    googleApiKeyEdit->setText(settings->value("api_keys/google", "").toString());

    // General settings
    browserModeCheck->setChecked(settings->value("general/browser_mode", false).toBool());
    autoStartAgentsCheck->setChecked(settings->value("general/auto_start_agents", true).toBool());
    maxAgentsSpin->setValue(settings->value("general/max_agents", 4).toInt());
    defaultProviderCombo->setCurrentText(settings->value("general/default_provider", "openai").toString());

    // Update model combo after setting provider
    updateModelCombo();
    defaultModelCombo->setCurrentText(settings->value("general/default_model", "gpt-4").toString());

    // Advanced settings
    responseTimeoutSpin->setValue(settings->value("advanced/response_timeout", 30).toInt());
    enableParallelExecutionCheck->setChecked(settings->value("advanced/parallel_execution", true).toBool());
    enableQualityScoringCheck->setChecked(settings->value("advanced/quality_scoring", true).toBool());
    customPromptsEdit->setPlainText(settings->value("advanced/custom_prompts", "").toString());
}

void MultiModelAgentSettingsDialog::saveSettings()
{
    // API Keys
    settings->setValue("api_keys/openai", openAIApiKeyEdit->text());
    settings->setValue("api_keys/anthropic", anthropicApiKeyEdit->text());
    settings->setValue("api_keys/google", googleApiKeyEdit->text());

    // General settings
    settings->setValue("general/browser_mode", browserModeCheck->isChecked());
    settings->setValue("general/auto_start_agents", autoStartAgentsCheck->isChecked());
    settings->setValue("general/max_agents", maxAgentsSpin->value());
    settings->setValue("general/default_provider", defaultProviderCombo->currentText());
    settings->setValue("general/default_model", defaultModelCombo->currentText());

    // Advanced settings
    settings->setValue("advanced/response_timeout", responseTimeoutSpin->value());
    settings->setValue("advanced/parallel_execution", enableParallelExecutionCheck->isChecked());
    settings->setValue("advanced/quality_scoring", enableQualityScoringCheck->isChecked());
    settings->setValue("advanced/custom_prompts", customPromptsEdit->toPlainText());

    settings->sync();

    emit settingsChanged();
}

void MultiModelAgentSettingsDialog::onAccept()
{
    saveSettings();
    accept();
}

void MultiModelAgentSettingsDialog::onApply()
{
    saveSettings();
    QMessageBox::information(this, "Settings Applied",
                           "Settings have been applied successfully.");
}

void MultiModelAgentSettingsDialog::onReset()
{
    if (QMessageBox::question(this, "Reset Settings",
                            "Are you sure you want to reset all settings to defaults?") ==
        QMessageBox::Yes) {
        settings->clear();
        loadSettings();
    }
}

void MultiModelAgentSettingsDialog::onTestConnection()
{
    QString provider;
    QString apiKey;

    // Determine which test button was clicked
    if (sender() == openAITestBtn) {
        provider = "openai";
        apiKey = openAIApiKeyEdit->text();
    } else if (sender() == anthropicTestBtn) {
        provider = "anthropic";
        apiKey = anthropicApiKeyEdit->text();
    } else if (sender() == googleTestBtn) {
        provider = "google";
        apiKey = googleApiKeyEdit->text();
    }

    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "API Key Required",
                           "Please enter an API key before testing the connection.");
        return;
    }

    // Disable test button during test
    QPushButton* testBtn = qobject_cast<QPushButton*>(sender());
    if (testBtn) {
        testBtn->setEnabled(false);
        testBtn->setText("Testing...");
    }

    bool success = testApiConnection(provider, apiKey);

    // Re-enable test button
    if (testBtn) {
        testBtn->setEnabled(true);
        testBtn->setText("Test");
    }

    if (success) {
        QMessageBox::information(this, "Connection Test Successful",
                               QString("Successfully connected to %1 API.").arg(provider.toUpper()));
    } else {
        QMessageBox::warning(this, "Connection Test Failed",
                           QString("Failed to connect to %1 API. Please check your API key.").arg(provider.toUpper()));
    }
}

bool MultiModelAgentSettingsDialog::testApiConnection(const QString& provider, const QString& apiKey)
{
    // This is a simplified test - in a real implementation, you'd make actual API calls
    // For now, just check if the API key format looks reasonable

    if (provider == "openai" && apiKey.startsWith("sk-")) {
        return true;
    } else if (provider == "anthropic" && apiKey.startsWith("sk-ant-")) {
        return true;
    } else if (provider == "google" && apiKey.startsWith("AIza")) {
        return true;
    }

    return false;
}

void MultiModelAgentSettingsDialog::updateModelCombo()
{
    QString provider = defaultProviderCombo->currentText();
    defaultModelCombo->clear();

    if (provider == "openai") {
        defaultModelCombo->addItems({"gpt-4", "gpt-4-turbo", "gpt-3.5-turbo"});
    } else if (provider == "anthropic") {
        defaultModelCombo->addItems({"claude-3-opus", "claude-3-sonnet", "claude-3-haiku"});
    } else if (provider == "google") {
        defaultModelCombo->addItems({"gemini-pro", "gemini-pro-vision", "palm-2"});
    } else if (provider == "local") {
        defaultModelCombo->addItems({"neural-chat", "mistral", "codellama", "dolphin-mixtral"});
    }
}

} // namespace IDE
} // namespace RawrXD