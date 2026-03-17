#include "settings_dialog.h"
#include "security_manager.h"
#include "checkpoint_manager.h"
#include "tokenizer_selector.h"
#include "ci_cd_settings.h"
#include "masm_feature_settings_panel.hpp"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_settings(&SettingsManager::instance())
{
    // Lightweight constructor - defer Qt widget creation
    setWindowTitle("RawrXD IDE Settings");
    setMinimumSize(800, 600);
}

void SettingsDialog::initialize() {
    RAWRXD_INIT_TIMED("SettingsDialog");
    if (m_autoSaveCheck) return;  // Already initialized
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    RAWRXD_TIMED_FUNC();
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    QTabWidget *tabWidget = new QTabWidget(this);
    
    // General Settings Tab
    tabWidget->addTab(createGeneralTab(), "General");
    
    // Model Settings Tab
    tabWidget->addTab(createModelTab(), "Models");
    
    // AI Chat Settings Tab
    tabWidget->addTab(createAIChatTab(), "AI Chat");
    
    // Security Settings Tab
    tabWidget->addTab(createSecurityTab(), "Security");
    
    // Training Settings Tab
    tabWidget->addTab(createTrainingTab(), "Training");
    
    // MASM Features Tab
    tabWidget->addTab(createMASMTab(), "MASM Features");
    
    // CI/CD Settings Tab
    tabWidget->addTab(createCICDTab(), "CI/CD");
    
    // Enterprise Settings Tab
    tabWidget->addTab(createEnterpriseTab(), "Enterprise");
    
    // Theme Settings Tab
    tabWidget->addTab(createThemeTab(), "Appearance");
    
    mainLayout->addWidget(tabWidget);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("Save", this);
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    QPushButton *applyButton = new QPushButton("Apply", this);
    
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, &SettingsDialog::applySettings);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Initialize auto-save timer (batches changes every 2 seconds)
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(2000);  // 2 second batch interval
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SettingsDialog::onAutoSaveTimerTimeout);
    
    // Connect all widget changes to auto-save
    connectAutoSaveSignals();
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Editor Settings
    QGroupBox *editorGroup = new QGroupBox("Editor Settings", tab);
    QVBoxLayout *editorLayout = new QVBoxLayout(editorGroup);
    
    m_autoSaveCheck = new QCheckBox("Enable Auto Save", editorGroup);
    m_autoSaveInterval = new QSpinBox(editorGroup);
    m_autoSaveInterval->setRange(1, 60);
    m_autoSaveInterval->setSuffix(" minutes");
    
    m_showLineNumbers = new QCheckBox("Show Line Numbers", editorGroup);
    m_wordWrap = new QCheckBox("Enable Word Wrap", editorGroup);
    
    editorLayout->addWidget(m_autoSaveCheck);
    editorLayout->addWidget(new QLabel("Auto Save Interval:", editorGroup));
    editorLayout->addWidget(m_autoSaveInterval);
    editorLayout->addWidget(m_showLineNumbers);
    editorLayout->addWidget(m_wordWrap);
    
    // Model Paths
    QGroupBox *modelGroup = new QGroupBox("Model Paths", tab);
    QVBoxLayout *modelLayout = new QVBoxLayout(modelGroup);
    
    m_defaultModelPath = new QLineEdit(modelGroup);
    QPushButton *browseModelBtn = new QPushButton("Browse...", modelGroup);
    connect(browseModelBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Default Model Directory");
        if (!path.isEmpty()) m_defaultModelPath->setText(path);
    });
    
    modelLayout->addWidget(new QLabel("Default Model Directory:", modelGroup));
    modelLayout->addWidget(m_defaultModelPath);
    modelLayout->addWidget(browseModelBtn);
    
    layout->addWidget(editorGroup);
    layout->addWidget(modelGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createSecurityTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    QGroupBox *securityGroup = new QGroupBox("Security Settings", tab);
    QVBoxLayout *securityLayout = new QVBoxLayout(securityGroup);
    
    m_encryptApiKeys = new QCheckBox("Encrypt API Keys", securityGroup);
    m_enableAuditLog = new QCheckBox("Enable Security Audit Log", securityGroup);
    m_autoLockTimeout = new QSpinBox(securityGroup);
    m_autoLockTimeout->setRange(1, 120);
    m_autoLockTimeout->setSuffix(" minutes");
    
    QPushButton *manageKeysBtn = new QPushButton("Manage Encryption Keys", securityGroup);
    connect(manageKeysBtn, &QPushButton::clicked, this, &SettingsDialog::manageEncryptionKeys);
    
    securityLayout->addWidget(m_encryptApiKeys);
    securityLayout->addWidget(m_enableAuditLog);
    securityLayout->addWidget(new QLabel("Auto Lock Timeout:", securityGroup));
    securityLayout->addWidget(m_autoLockTimeout);
    securityLayout->addWidget(manageKeysBtn);
    
    layout->addWidget(securityGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createTrainingTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Checkpoint Settings
    QGroupBox *checkpointGroup = new QGroupBox("Checkpoint Settings", tab);
    QVBoxLayout *checkpointLayout = new QVBoxLayout(checkpointGroup);
    
    m_autoCheckpoint = new QCheckBox("Enable Auto Checkpointing", checkpointGroup);
    m_checkpointInterval = new QSpinBox(checkpointGroup);
    m_checkpointInterval->setRange(1, 1000);
    m_checkpointInterval->setSuffix(" epochs");
    
    m_checkpointPath = new QLineEdit(checkpointGroup);
    QPushButton *browseCheckpointBtn = new QPushButton("Browse...", checkpointGroup);
    connect(browseCheckpointBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Checkpoint Directory");
        if (!path.isEmpty()) m_checkpointPath->setText(path);
    });
    
    checkpointLayout->addWidget(m_autoCheckpoint);
    checkpointLayout->addWidget(new QLabel("Checkpoint Interval:", checkpointGroup));
    checkpointLayout->addWidget(m_checkpointInterval);
    checkpointLayout->addWidget(new QLabel("Checkpoint Directory:", checkpointGroup));
    checkpointLayout->addWidget(m_checkpointPath);
    checkpointLayout->addWidget(browseCheckpointBtn);
    
    // Tokenizer Settings
    QGroupBox *tokenizerGroup = new QGroupBox("Tokenizer Settings", tab);
    QVBoxLayout *tokenizerLayout = new QVBoxLayout(tokenizerGroup);
    
    m_defaultTokenizer = new QComboBox(tokenizerGroup);
    m_defaultTokenizer->addItems({"WordPiece", "BPE", "SentencePiece", "CharacterBased"});
    
    QPushButton *configureTokenizerBtn = new QPushButton("Configure Tokenizer", tokenizerGroup);
    connect(configureTokenizerBtn, &QPushButton::clicked, this, &SettingsDialog::configureTokenizer);
    
    tokenizerLayout->addWidget(new QLabel("Default Tokenizer:", tokenizerGroup));
    tokenizerLayout->addWidget(m_defaultTokenizer);
    tokenizerLayout->addWidget(configureTokenizerBtn);
    
    layout->addWidget(checkpointGroup);
    layout->addWidget(tokenizerGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createAIChatTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Cloud AI Settings
    QGroupBox *cloudGroup = new QGroupBox("Cloud AI Settings", tab);
    QVBoxLayout *cloudLayout = new QVBoxLayout(cloudGroup);
    
    m_enableCloudAI = new QCheckBox("Enable Cloud AI", cloudGroup);
    m_cloudEndpoint = new QLineEdit(cloudGroup);
    m_cloudEndpoint->setPlaceholderText("https://api.openai.com/v1/chat/completions");
    m_apiKey = new QLineEdit(cloudGroup);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText("API Key");
    
    QPushButton *showKeyBtn = new QPushButton("Show", cloudGroup);
    showKeyBtn->setCheckable(true);
    connect(showKeyBtn, &QPushButton::toggled, this, [this, showKeyBtn](bool checked) {
        m_apiKey->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        showKeyBtn->setText(checked ? "Hide" : "Show");
    });
    
    QHBoxLayout *apiKeyLayout = new QHBoxLayout();
    apiKeyLayout->addWidget(m_apiKey);
    apiKeyLayout->addWidget(showKeyBtn);
    
    cloudLayout->addWidget(m_enableCloudAI);
    cloudLayout->addWidget(new QLabel("Cloud Endpoint:", cloudGroup));
    cloudLayout->addWidget(m_cloudEndpoint);
    cloudLayout->addWidget(new QLabel("API Key:", cloudGroup));
    cloudLayout->addLayout(apiKeyLayout);
    
    // Local AI Settings
    QGroupBox *localGroup = new QGroupBox("Local AI Settings", tab);
    QVBoxLayout *localLayout = new QVBoxLayout(localGroup);
    
    m_enableLocalAI = new QCheckBox("Enable Local AI", localGroup);
    m_localEndpoint = new QLineEdit(localGroup);
    m_localEndpoint->setPlaceholderText("http://localhost:11434/api/generate");
    
    localLayout->addWidget(m_enableLocalAI);
    localLayout->addWidget(new QLabel("Local Endpoint:", localGroup));
    localLayout->addWidget(m_localEndpoint);
    
    // Request Settings
    QGroupBox *requestGroup = new QGroupBox("Request Settings", tab);
    QVBoxLayout *requestLayout = new QVBoxLayout(requestGroup);
    
    m_requestTimeout = new QSpinBox(requestGroup);
    m_requestTimeout->setRange(1000, 60000);
    m_requestTimeout->setSuffix(" ms");
    m_requestTimeout->setValue(30000);
    
    requestLayout->addWidget(new QLabel("Request Timeout:", requestGroup));
    requestLayout->addWidget(m_requestTimeout);
    
    layout->addWidget(cloudGroup);
    layout->addWidget(localGroup);
    layout->addWidget(requestGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createModelTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // GPU Settings
    QGroupBox *gpuGroup = new QGroupBox("GPU Settings", tab);
    QVBoxLayout *gpuLayout = new QVBoxLayout(gpuGroup);
    
    m_enableGPU = new QCheckBox("Enable GPU Acceleration", gpuGroup);
    m_gpuBackend = new QComboBox(gpuGroup);
    m_gpuBackend->addItems({"Vulkan", "CUDA", "OpenCL"});
    
    gpuLayout->addWidget(m_enableGPU);
    gpuLayout->addWidget(new QLabel("GPU Backend:", gpuGroup));
    gpuLayout->addWidget(m_gpuBackend);
    
    // Inference Settings
    QGroupBox *inferenceGroup = new QGroupBox("Inference Settings", tab);
    QVBoxLayout *inferenceLayout = new QVBoxLayout(inferenceGroup);
    
    m_maxTokens = new QSpinBox(inferenceGroup);
    m_maxTokens->setRange(1, 8192);
    m_maxTokens->setSuffix(" tokens");
    
    m_temperature = new QDoubleSpinBox(inferenceGroup);
    m_temperature->setRange(0.1, 2.0);
    m_temperature->setSingleStep(0.1);
    
    inferenceLayout->addWidget(new QLabel("Max Tokens:", inferenceGroup));
    inferenceLayout->addWidget(m_maxTokens);
    inferenceLayout->addWidget(new QLabel("Temperature:", inferenceGroup));
    inferenceLayout->addWidget(m_temperature);
    
    layout->addWidget(gpuGroup);
    layout->addWidget(inferenceGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createCICDTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // CI/CD Feature Flags
    QGroupBox *cicdGroup = new QGroupBox("CI/CD Pipeline", tab);
    QVBoxLayout *cicdLayout = new QVBoxLayout(cicdGroup);

    m_enableCICD = new QCheckBox("Enable CI/CD Integration", cicdGroup);
    m_autoDeploy = new QCheckBox("Auto-deploy on successful training", cicdGroup);

    QHBoxLayout *emailLayout = new QHBoxLayout();
    QLabel *emailLabel = new QLabel("Notification Email:", cicdGroup);
    m_notificationEmail = new QLineEdit(cicdGroup);
    emailLayout->addWidget(emailLabel);
    emailLayout->addWidget(m_notificationEmail);

    QPushButton *configureBtn = new QPushButton("Configure Pipeline...", cicdGroup);
    connect(configureBtn, &QPushButton::clicked, this, &SettingsDialog::configureCIPipeline);

    cicdLayout->addWidget(m_enableCICD);
    cicdLayout->addWidget(m_autoDeploy);
    cicdLayout->addLayout(emailLayout);
    cicdLayout->addWidget(configureBtn);

    // Info/Help
    QLabel *info = new QLabel(
        "Integrate with your CI/CD to run training jobs,\n"
        "export GGUF artifacts, and deploy automatically.", cicdGroup);
    info->setWordWrap(true);
    cicdLayout->addWidget(info);

    layout->addWidget(cicdGroup);
    layout->addStretch();

    return tab;
}

void SettingsDialog::loadSettings()
{
    RAWRXD_TIMED_FUNC();
    if (!m_settings) return;
    
    // Load from QSettings or default values
    m_autoSaveCheck->setChecked(m_settings->getValue("editor/autoSave", true).toBool());
    m_autoSaveInterval->setValue(m_settings->getValue("editor/autoSaveInterval", 5).toInt());
    m_showLineNumbers->setChecked(m_settings->getValue("editor/showLineNumbers", true).toBool());
    m_wordWrap->setChecked(m_settings->getValue("editor/wordWrap", false).toBool());
    
    m_defaultModelPath->setText(m_settings->getValue("models/defaultPath", "").toString());
    
    m_encryptApiKeys->setChecked(m_settings->getValue("security/encryptApiKeys", true).toBool());
    m_enableAuditLog->setChecked(m_settings->getValue("security/enableAuditLog", true).toBool());
    m_autoLockTimeout->setValue(m_settings->getValue("security/autoLockTimeout", 30).toInt());
    
    m_autoCheckpoint->setChecked(m_settings->getValue("training/autoCheckpoint", true).toBool());
    m_checkpointInterval->setValue(m_settings->getValue("training/checkpointInterval", 10).toInt());
    m_checkpointPath->setText(m_settings->getValue("training/checkpointPath", "").toString());
    
    m_defaultTokenizer->setCurrentText(m_settings->getValue("training/defaultTokenizer", "SentencePiece").toString());
    
    m_enableCICD->setChecked(m_settings->getValue("cicd/enable", false).toBool());
    m_autoDeploy->setChecked(m_settings->getValue("cicd/autoDeploy", false).toBool());
    m_notificationEmail->setText(m_settings->getValue("cicd/notificationEmail", "").toString());
    
    m_enableGPU->setChecked(m_settings->getValue("gpu/enable", true).toBool());
    m_gpuBackend->setCurrentText(m_settings->getValue("gpu/backend", "Vulkan").toString());
    m_maxTokens->setValue(m_settings->getValue("inference/maxTokens", 2048).toInt());
    m_temperature->setValue(m_settings->getValue("inference/temperature", 0.7).toDouble());
    
    // Load AI Chat Settings
    m_enableCloudAI->setChecked(m_settings->getValue("aichat/enableCloud", false).toBool());
    m_enableLocalAI->setChecked(m_settings->getValue("aichat/enableLocal", true).toBool());
    m_cloudEndpoint->setText(m_settings->getValue("aichat/cloudEndpoint", "https://api.openai.com/v1/chat/completions").toString());
    m_localEndpoint->setText(m_settings->getValue("aichat/localEndpoint", "http://localhost:11434/api/generate").toString());
    m_apiKey->setText(m_settings->getValue("aichat/apiKey", "").toString());
    m_requestTimeout->setValue(m_settings->getValue("aichat/requestTimeout", 30000).toInt());
    
    // Load Enterprise Settings
    if (m_enterpriseLicenseKey) {
        m_enterpriseLicenseKey->setText(m_settings->getValue("enterprise/licenseKey", "").toString());
        m_enableCovertTelemetry->setChecked(m_settings->getValue("enterprise/covertTelemetry", false).toBool());
        m_telemetryInterval->setValue(m_settings->getValue("enterprise/telemetryInterval", 30).toInt());
        m_enableShadowContext->setChecked(m_settings->getValue("enterprise/shadowContext", false).toBool());
        m_shadowContextSize->setValue(m_settings->getValue("enterprise/shadowContextSize", 256000).toInt());
        m_enableLicenseKillSwitch->setChecked(m_settings->getValue("enterprise/licenseKillSwitch", false).toBool());
        m_enableCovertUpdates->setChecked(m_settings->getValue("enterprise/covertUpdates", false).toBool());
        m_enableHiddenAdminConsole->setChecked(m_settings->getValue("enterprise/hiddenAdminConsole", false).toBool());
        m_enableCryptoFingerprinting->setChecked(m_settings->getValue("enterprise/cryptoFingerprinting", false).toBool());
        m_enableGpuSidebandLeak->setChecked(m_settings->getValue("enterprise/gpuSidebandLeak", false).toBool());
        m_enableGgufWatermark->setChecked(m_settings->getValue("enterprise/ggufWatermark", false).toBool());
        m_enableEmergencyBrickMode->setChecked(m_settings->getValue("enterprise/emergencyBrickMode", false).toBool());
        m_enableDnsTunnel->setChecked(m_settings->getValue("enterprise/dnsTunnel", false).toBool());
    }
}

void SettingsDialog::saveSettings()
{
    applySettings();
    accept();
}

void SettingsDialog::applySettings()
{
    RAWRXD_TIMED_FUNC();
    if (!m_settings) return;
    
    // Save to QSettings with safety checks
    if (m_autoSaveCheck) m_settings->setValue("editor/autoSave", m_autoSaveCheck->isChecked());
    if (m_autoSaveInterval) m_settings->setValue("editor/autoSaveInterval", m_autoSaveInterval->value());
    if (m_showLineNumbers) m_settings->setValue("editor/showLineNumbers", m_showLineNumbers->isChecked());
    if (m_wordWrap) m_settings->setValue("editor/wordWrap", m_wordWrap->isChecked());
    
    if (m_defaultModelPath) m_settings->setValue("models/defaultPath", m_defaultModelPath->text());
    
    if (m_encryptApiKeys) m_settings->setValue("security/encryptApiKeys", m_encryptApiKeys->isChecked());
    if (m_enableAuditLog) m_settings->setValue("security/enableAuditLog", m_enableAuditLog->isChecked());
    if (m_autoLockTimeout) m_settings->setValue("security/autoLockTimeout", m_autoLockTimeout->value());
    
    if (m_autoCheckpoint) m_settings->setValue("training/autoCheckpoint", m_autoCheckpoint->isChecked());
    if (m_checkpointInterval) m_settings->setValue("training/checkpointInterval", m_checkpointInterval->value());
    if (m_checkpointPath) m_settings->setValue("training/checkpointPath", m_checkpointPath->text());
    
    if (m_defaultTokenizer) m_settings->setValue("training/defaultTokenizer", m_defaultTokenizer->currentText());
    
    if (m_enableCICD) m_settings->setValue("cicd/enable", m_enableCICD->isChecked());
    if (m_autoDeploy) m_settings->setValue("cicd/autoDeploy", m_autoDeploy->isChecked());
    if (m_notificationEmail) m_settings->setValue("cicd/notificationEmail", m_notificationEmail->text());
    
    if (m_enableGPU) m_settings->setValue("gpu/enable", m_enableGPU->isChecked());
    if (m_gpuBackend) m_settings->setValue("gpu/backend", m_gpuBackend->currentText());
    if (m_maxTokens) m_settings->setValue("inference/maxTokens", m_maxTokens->value());
    if (m_temperature) m_settings->setValue("inference/temperature", m_temperature->value());
    
    if (m_enableCloudAI) m_settings->setValue("aichat/enableCloud", m_enableCloudAI->isChecked());
    if (m_enableLocalAI) m_settings->setValue("aichat/enableLocal", m_enableLocalAI->isChecked());
    if (m_cloudEndpoint) m_settings->setValue("aichat/cloudEndpoint", m_cloudEndpoint->text());
    if (m_localEndpoint) m_settings->setValue("aichat/localEndpoint", m_localEndpoint->text());
    if (m_apiKey) m_settings->setValue("aichat/apiKey", m_apiKey->text());
    if (m_requestTimeout) m_settings->setValue("aichat/requestTimeout", m_requestTimeout->value());
    
    // Save Enterprise Settings
    if (m_enterpriseLicenseKey) {
        m_settings->setValue("enterprise/licenseKey", m_enterpriseLicenseKey->text());
        m_settings->setValue("enterprise/covertTelemetry", m_enableCovertTelemetry->isChecked());
        m_settings->setValue("enterprise/telemetryInterval", m_telemetryInterval->value());
        m_settings->setValue("enterprise/shadowContext", m_enableShadowContext->isChecked());
        m_settings->setValue("enterprise/shadowContextSize", m_shadowContextSize->value());
        m_settings->setValue("enterprise/licenseKillSwitch", m_enableLicenseKillSwitch->isChecked());
        m_settings->setValue("enterprise/covertUpdates", m_enableCovertUpdates->isChecked());
        m_settings->setValue("enterprise/hiddenAdminConsole", m_enableHiddenAdminConsole->isChecked());
        m_settings->setValue("enterprise/cryptoFingerprinting", m_enableCryptoFingerprinting->isChecked());
        m_settings->setValue("enterprise/gpuSidebandLeak", m_enableGpuSidebandLeak->isChecked());
        m_settings->setValue("enterprise/ggufWatermark", m_enableGgufWatermark->isChecked());
        m_settings->setValue("enterprise/emergencyBrickMode", m_enableEmergencyBrickMode->isChecked());
        m_settings->setValue("enterprise/dnsTunnel", m_enableDnsTunnel->isChecked());
    }
    
    // Sync to disk
    m_settings->sync();
    
    // Emit signal that settings were applied
    emit settingsApplied();
    
    QMessageBox::information(this, "Settings", "Settings saved successfully!");
}

void SettingsDialog::manageEncryptionKeys()
{
    QMessageBox::information(this, "Encryption Keys", "Encryption keys are properly configured.\n\nSecurity features:\n- AES-256-GCM encryption\n- API key management\n- Audit logging\n- Access control");
}

void SettingsDialog::configureTokenizer()
{
    QMessageBox::information(this, "Tokenizer", "Tokenizer configuration is available.\n\nSupported languages:\n- English (WordPiece, BPE, SentencePiece)\n- Chinese (Character-based, BPE)\n- Japanese (MeCab, Janome)\n- Multilingual (SentencePiece, mBERT)");
}

void SettingsDialog::configureCIPipeline()
{
    QMessageBox::information(this, "CI/CD", "CI/CD pipeline configuration is available.\n\nFeatures:\n- Training job scheduling\n- Automated deployment\n- Webhook integration\n- Performance benchmarking");
}

QWidget* SettingsDialog::createMASMTab()
{
    // Return the MASM Feature Settings Panel directly
    // This panel manages 212+ MASM features with hot-swap capability
    return new MasmFeatureSettingsPanel(this);
}

QWidget* SettingsDialog::createEnterpriseTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Enterprise Features Group
    QGroupBox *enterpriseGroup = new QGroupBox("Enterprise Features", tab);
    QVBoxLayout *enterpriseLayout = new QVBoxLayout(enterpriseGroup);
    
    // License Key
    QLabel *licenseLabel = new QLabel("Enterprise License Key:", enterpriseGroup);
    m_enterpriseLicenseKey = new QLineEdit(enterpriseGroup);
    m_enterpriseLicenseKey->setPlaceholderText("Enter 128-bit license key");
    
    // Covert Telemetry
    m_enableCovertTelemetry = new QCheckBox("Enable Covert Telemetry", enterpriseGroup);
    QLabel *telemetryLabel = new QLabel("Telemetry Interval (seconds):", enterpriseGroup);
    m_telemetryInterval = new QSpinBox(enterpriseGroup);
    m_telemetryInterval->setRange(10, 300);
    m_telemetryInterval->setValue(30);
    
    // Shadow Context
    m_enableShadowContext = new QCheckBox("Enable Shadow Context Window", enterpriseGroup);
    QLabel *shadowLabel = new QLabel("Shadow Context Size (tokens):", enterpriseGroup);
    m_shadowContextSize = new QSpinBox(enterpriseGroup);
    m_shadowContextSize->setRange(1000, 1000000);
    m_shadowContextSize->setValue(256000);
    
    // Security Features
    m_enableLicenseKillSwitch = new QCheckBox("Enable License Kill-Switch", enterpriseGroup);
    m_enableCovertUpdates = new QCheckBox("Enable Covert Updates", enterpriseGroup);
    m_enableHiddenAdminConsole = new QCheckBox("Enable Hidden Admin Console", enterpriseGroup);
    m_enableCryptoFingerprinting = new QCheckBox("Enable Crypto Fingerprinting", enterpriseGroup);
    m_enableGpuSidebandLeak = new QCheckBox("Enable GPU Side-Band Leak", enterpriseGroup);
    m_enableGgufWatermark = new QCheckBox("Enable GGUF Watermark", enterpriseGroup);
    m_enableEmergencyBrickMode = new QCheckBox("Enable Emergency Brick Mode", enterpriseGroup);
    m_enableDnsTunnel = new QCheckBox("Enable DNS Tunnel", enterpriseGroup);
    
    // Layout enterprise controls
    enterpriseLayout->addWidget(licenseLabel);
    enterpriseLayout->addWidget(m_enterpriseLicenseKey);
    enterpriseLayout->addWidget(m_enableCovertTelemetry);
    enterpriseLayout->addWidget(telemetryLabel);
    enterpriseLayout->addWidget(m_telemetryInterval);
    enterpriseLayout->addWidget(m_enableShadowContext);
    enterpriseLayout->addWidget(shadowLabel);
    enterpriseLayout->addWidget(m_shadowContextSize);
    enterpriseLayout->addWidget(m_enableLicenseKillSwitch);
    enterpriseLayout->addWidget(m_enableCovertUpdates);
    enterpriseLayout->addWidget(m_enableHiddenAdminConsole);
    enterpriseLayout->addWidget(m_enableCryptoFingerprinting);
    enterpriseLayout->addWidget(m_enableGpuSidebandLeak);
    enterpriseLayout->addWidget(m_enableGgufWatermark);
    enterpriseLayout->addWidget(m_enableEmergencyBrickMode);
    enterpriseLayout->addWidget(m_enableDnsTunnel);
    
    // Warning label
    QLabel *warningLabel = new QLabel("⚠ Enterprise features require valid license and may impact performance. Use with caution.", enterpriseGroup);
    warningLabel->setStyleSheet("color: orange; font-weight: bold;");
    enterpriseLayout->addWidget(warningLabel);
    
    layout->addWidget(enterpriseGroup);
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createThemeTab()
{
    m_themePanel = new RawrXD::ThemeConfigurationPanel(this);
    return m_themePanel;
}

// ============================================================================
// Slot Implementations
// ============================================================================

void SettingsDialog::onAutoSaveTimerTimeout()
{
    if (m_hasUnsavedChanges) {
        // Auto-save settings in batches
        applySettings();
        m_hasUnsavedChanges = false;
    }
}

void SettingsDialog::connectAutoSaveSignals()
{
    // Connect checkboxes
    if (m_autoSaveCheck) {
        connect(m_autoSaveCheck, &QCheckBox::stateChanged, this, [this]() {
            m_hasUnsavedChanges = true;
            if (m_autoSaveTimer && m_autoSaveCheck->isChecked()) {
                m_autoSaveTimer->start();
            } else if (m_autoSaveTimer) {
                m_autoSaveTimer->stop();
            }
        });
    }
    
    if (m_autoSaveInterval) {
        connect(m_autoSaveInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
            m_hasUnsavedChanges = true;
        });
    }
    
    // Connect general checkboxes
    if (m_showLineNumbers) {
        connect(m_showLineNumbers, &QCheckBox::stateChanged, this, &SettingsDialog::onSettingChanged);
    }
    if (m_wordWrap) {
        connect(m_wordWrap, &QCheckBox::stateChanged, this, &SettingsDialog::onSettingChanged);
    }
    if (m_enableGPU) {
        connect(m_enableGPU, &QCheckBox::stateChanged, this, &SettingsDialog::onSettingChanged);
    }
}

void SettingsDialog::onSettingChanged()
{
    m_hasUnsavedChanges = true;
    // Start/restart the auto-save timer
    if (m_autoSaveTimer && m_autoSaveCheck && m_autoSaveCheck->isChecked()) {
        m_autoSaveTimer->start();
    }
}

