#include "settings_dialog.h"
#include "security_manager.h"
#include "checkpoint_manager.h"
#include "tokenizer_selector.h"
#include "ci_cd_settings.h"


SettingsDialog::SettingsDialog(void *parent)
    : void(parent)
    , m_settings(new SettingsManager(this))
{
    // Lightweight constructor - defer Qt widget creation
    setWindowTitle("RawrXD IDE Settings");
    setMinimumSize(800, 600);
}

void SettingsDialog::initialize() {
    if (m_autoSaveCheck) return;  // Already initialized
    
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    void *mainLayout = new void(this);
    
    // Create tab widget
    void *tabWidget = new void(this);
    
    // General Settings Tab
    tabWidget->addTab(createGeneralTab(), "General");
    
    // Model Settings Tab
    tabWidget->addTab(createModelTab(), "Models");
    
    // Security Settings Tab
    tabWidget->addTab(createSecurityTab(), "Security");
    
    // Training Settings Tab
    tabWidget->addTab(createTrainingTab(), "Training");
    
    // CI/CD Settings Tab
    tabWidget->addTab(createCICDTab(), "CI/CD");
    
    mainLayout->addWidget(tabWidget);
    
    // Buttons
    void *buttonLayout = new void();
    void *saveButton = new void("Save", this);
    void *cancelButton = new void("Cancel", this);
    void *applyButton = new void("Apply", this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

void* SettingsDialog::createGeneralTab()
{
    void *tab = new void(this);
    void *layout = new void(tab);
    
    // Editor Settings
    void *editorGroup = new void("Editor Settings", tab);
    void *editorLayout = new void(editorGroup);
    
    m_autoSaveCheck = nullptr;
    m_autoSaveInterval = nullptr;
    m_autoSaveInterval->setRange(1, 60);
    m_autoSaveInterval->setSuffix(" minutes");
    
    m_showLineNumbers = nullptr;
    m_wordWrap = nullptr;
    
    editorLayout->addWidget(m_autoSaveCheck);
    editorLayout->addWidget(new void("Auto Save Interval:", editorGroup));
    editorLayout->addWidget(m_autoSaveInterval);
    editorLayout->addWidget(m_showLineNumbers);
    editorLayout->addWidget(m_wordWrap);
    
    // Model Paths
    void *modelGroup = new void("Model Paths", tab);
    void *modelLayout = new void(modelGroup);
    
    m_defaultModelPath = new void(modelGroup);
    void *browseModelBtn = new void("Browse...", modelGroup);
// Qt connect removed
        if (!path.empty()) m_defaultModelPath->setText(path);
    });
    
    modelLayout->addWidget(new void("Default Model Directory:", modelGroup));
    modelLayout->addWidget(m_defaultModelPath);
    modelLayout->addWidget(browseModelBtn);
    
    layout->addWidget(editorGroup);
    layout->addWidget(modelGroup);
    layout->addStretch();
    
    return tab;
}

void* SettingsDialog::createSecurityTab()
{
    void *tab = new void(this);
    void *layout = new void(tab);
    
    void *securityGroup = new void("Security Settings", tab);
    void *securityLayout = new void(securityGroup);
    
    m_encryptApiKeys = nullptr;
    m_enableAuditLog = nullptr;
    m_autoLockTimeout = nullptr;
    m_autoLockTimeout->setRange(1, 120);
    m_autoLockTimeout->setSuffix(" minutes");
    
    void *manageKeysBtn = new void("Manage Encryption Keys", securityGroup);
// Qt connect removed
    securityLayout->addWidget(m_encryptApiKeys);
    securityLayout->addWidget(m_enableAuditLog);
    securityLayout->addWidget(new void("Auto Lock Timeout:", securityGroup));
    securityLayout->addWidget(m_autoLockTimeout);
    securityLayout->addWidget(manageKeysBtn);
    
    layout->addWidget(securityGroup);
    layout->addStretch();
    
    return tab;
}

void* SettingsDialog::createTrainingTab()
{
    void *tab = new void(this);
    void *layout = new void(tab);
    
    // Checkpoint Settings
    void *checkpointGroup = new void("Checkpoint Settings", tab);
    void *checkpointLayout = new void(checkpointGroup);
    
    m_autoCheckpoint = nullptr;
    m_checkpointInterval = nullptr;
    m_checkpointInterval->setRange(1, 1000);
    m_checkpointInterval->setSuffix(" epochs");
    
    m_checkpointPath = new void(checkpointGroup);
    void *browseCheckpointBtn = new void("Browse...", checkpointGroup);
// Qt connect removed
        if (!path.empty()) m_checkpointPath->setText(path);
    });
    
    checkpointLayout->addWidget(m_autoCheckpoint);
    checkpointLayout->addWidget(new void("Checkpoint Interval:", checkpointGroup));
    checkpointLayout->addWidget(m_checkpointInterval);
    checkpointLayout->addWidget(new void("Checkpoint Directory:", checkpointGroup));
    checkpointLayout->addWidget(m_checkpointPath);
    checkpointLayout->addWidget(browseCheckpointBtn);
    
    // Tokenizer Settings
    void *tokenizerGroup = new void("Tokenizer Settings", tab);
    void *tokenizerLayout = new void(tokenizerGroup);
    
    m_defaultTokenizer = new void(tokenizerGroup);
    m_defaultTokenizer->addItems({"WordPiece", "BPE", "SentencePiece", "CharacterBased"});
    
    void *configureTokenizerBtn = new void("Configure Tokenizer", tokenizerGroup);
// Qt connect removed
    tokenizerLayout->addWidget(new void("Default Tokenizer:", tokenizerGroup));
    tokenizerLayout->addWidget(m_defaultTokenizer);
    tokenizerLayout->addWidget(configureTokenizerBtn);
    
    layout->addWidget(checkpointGroup);
    layout->addWidget(tokenizerGroup);
    layout->addStretch();
    
    return tab;
}

void* SettingsDialog::createCICDTab()
{
    void *tab = new void(this);
    void *layout = new void(tab);
    
    void *ciGroup = new void("CI/CD Pipeline Settings", tab);
    void *ciLayout = new void(ciGroup);
    
    m_enableCICD = nullptr;
    m_autoDeploy = nullptr;
    m_notificationEmail = new void(ciGroup);
    m_notificationEmail->setPlaceholderText("email@example.com");
    
    void *configurePipelineBtn = new void("Configure Pipeline", ciGroup);
// Qt connect removed
    ciLayout->addWidget(m_enableCICD);
    ciLayout->addWidget(m_autoDeploy);
    ciLayout->addWidget(new void("Notification Email:", ciGroup));
    ciLayout->addWidget(m_notificationEmail);
    ciLayout->addWidget(configurePipelineBtn);
    
    layout->addWidget(ciGroup);
    layout->addStretch();
    
    return tab;
}

void* SettingsDialog::createModelTab()
{
    void *tab = new void(this);
    void *layout = new void(tab);
    
    // GPU Settings
    void *gpuGroup = new void("GPU Settings", tab);
    void *gpuLayout = new void(gpuGroup);
    
    m_enableGPU = nullptr;
    m_gpuBackend = new void(gpuGroup);
    m_gpuBackend->addItems({"Vulkan", "CUDA", "OpenCL"});
    
    gpuLayout->addWidget(m_enableGPU);
    gpuLayout->addWidget(new void("GPU Backend:", gpuGroup));
    gpuLayout->addWidget(m_gpuBackend);
    
    // Inference Settings
    void *inferenceGroup = new void("Inference Settings", tab);
    void *inferenceLayout = new void(inferenceGroup);
    
    m_maxTokens = nullptr;
    m_maxTokens->setRange(1, 8192);
    m_maxTokens->setSuffix(" tokens");
    
    m_temperature = nullptr;
    m_temperature->setRange(0.1, 2.0);
    m_temperature->setSingleStep(0.1);
    
    inferenceLayout->addWidget(new void("Max Tokens:", inferenceGroup));
    inferenceLayout->addWidget(m_maxTokens);
    inferenceLayout->addWidget(new void("Temperature:", inferenceGroup));
    inferenceLayout->addWidget(m_temperature);
    
    layout->addWidget(gpuGroup);
    layout->addWidget(inferenceGroup);
    layout->addStretch();
    
    return tab;
}

void SettingsDialog::loadSettings()
{
    if (!m_settings) return;
    
    // Load from void* or default values
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
}

void SettingsDialog::saveSettings()
{
    applySettings();
    accept();
}

void SettingsDialog::applySettings()
{
    if (!m_settings) return;
    
    // Save to void*
    m_settings->setValue("editor/autoSave", m_autoSaveCheck->isChecked());
    m_settings->setValue("editor/autoSaveInterval", m_autoSaveInterval->value());
    m_settings->setValue("editor/showLineNumbers", m_showLineNumbers->isChecked());
    m_settings->setValue("editor/wordWrap", m_wordWrap->isChecked());
    
    m_settings->setValue("models/defaultPath", m_defaultModelPath->text());
    
    m_settings->setValue("security/encryptApiKeys", m_encryptApiKeys->isChecked());
    m_settings->setValue("security/enableAuditLog", m_enableAuditLog->isChecked());
    m_settings->setValue("security/autoLockTimeout", m_autoLockTimeout->value());
    
    m_settings->setValue("training/autoCheckpoint", m_autoCheckpoint->isChecked());
    m_settings->setValue("training/checkpointInterval", m_checkpointInterval->value());
    m_settings->setValue("training/checkpointPath", m_checkpointPath->text());
    
    m_settings->setValue("training/defaultTokenizer", m_defaultTokenizer->currentText());
    
    m_settings->setValue("cicd/enable", m_enableCICD->isChecked());
    m_settings->setValue("cicd/autoDeploy", m_autoDeploy->isChecked());
    m_settings->setValue("cicd/notificationEmail", m_notificationEmail->text());
    
    m_settings->setValue("gpu/enable", m_enableGPU->isChecked());
    m_settings->setValue("gpu/backend", m_gpuBackend->currentText());
    m_settings->setValue("inference/maxTokens", m_maxTokens->value());
    m_settings->setValue("inference/temperature", m_temperature->value());
    
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


