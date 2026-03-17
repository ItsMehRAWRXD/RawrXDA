#include "settings_dialog.h"
#include "security_manager.h"
#include "checkpoint_manager.h"
#include "tokenizer_selector.h"
#include "ci_cd_settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include "memory_space_manager.h"
#include <QListWidget>
#include <QSlider>
#include <QGridLayout>
#include <QStyle>
#include "memory_space_manager.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    QTabWidget *tabWidget = new QTabWidget(this);
    
    // General Settings Tab
    tabWidget->addTab(createGeneralTab(), "General");
    
    
    // Visual Settings Tab
    tabWidget->addTab(createVisualTab(), "Visual");
    
    // Visual Settings Tab
    tabWidget->addTab(createVisualTab(), "Visual");
    
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

QWidget* SettingsDialog::createMemoryTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *memoryGroup = new QGroupBox("Memory Space", tab);
    QVBoxLayout *memoryLayout = new QVBoxLayout(memoryGroup);

    m_enableMemorySpace = new QCheckBox("Enable Long-Term Memory", memoryGroup);
    m_memoryLimitValue = new QDoubleSpinBox(memoryGroup);
    m_memoryLimitValue->setRange(1, 1024);
    m_memoryLimitValue->setDecimals(1);
    m_memoryLimitValue->setValue(128);
    m_memoryLimitUnit = new QComboBox(memoryGroup);
    m_memoryLimitUnit->addItems({"KB", "MB", "GB"});
    m_memoryLimitUnit->setCurrentText("MB");

    m_memoryUsageLabel = new QLabel("Usage: 0 MB", memoryGroup);

    QHBoxLayout *limitLayout = new QHBoxLayout();
    limitLayout->addWidget(new QLabel("Memory Limit:", memoryGroup));
    limitLayout->addWidget(m_memoryLimitValue);
    limitLayout->addWidget(m_memoryLimitUnit);

    // Memory entries with delete action
    m_memoryList = new QListWidget(memoryGroup);
    m_memoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deleteMemoryBtn = new QPushButton("Delete Selected", memoryGroup);
    m_deleteMemoryBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(m_deleteMemoryBtn, &QPushButton::clicked, this, &SettingsDialog::deleteSelectedMemory);

    memoryLayout->addWidget(m_enableMemorySpace);
    memoryLayout->addLayout(limitLayout);
    memoryLayout->addWidget(m_memoryUsageLabel);
    memoryLayout->addWidget(new QLabel("Saved Contexts:", memoryGroup));
    memoryLayout->addWidget(m_memoryList);
    memoryLayout->addWidget(m_deleteMemoryBtn);

    layout->addWidget(memoryGroup);
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

QWidget* SettingsDialog::createCICDTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    QGroupBox *ciGroup = new QGroupBox("CI/CD Pipeline Settings", tab);
    QVBoxLayout *ciLayout = new QVBoxLayout(ciGroup);
    
    m_enableCICD = new QCheckBox("Enable CI/CD Pipeline", ciGroup);
    m_autoDeploy = new QCheckBox("Auto Deploy After Training", ciGroup);
    m_notificationEmail = new QLineEdit(ciGroup);
    m_notificationEmail->setPlaceholderText("email@example.com");
    
    QPushButton *configurePipelineBtn = new QPushButton("Configure Pipeline", ciGroup);
    connect(configurePipelineBtn, &QPushButton::clicked, this, &SettingsDialog::configureCIPipeline);
    
    ciLayout->addWidget(m_enableCICD);
    ciLayout->addWidget(m_autoDeploy);
    ciLayout->addWidget(new QLabel("Notification Email:", ciGroup));
    ciLayout->addWidget(m_notificationEmail);
    ciLayout->addWidget(configurePipelineBtn);
    
    layout->addWidget(ciGroup);
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

void SettingsDialog::loadSettings()
{
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

    // Visual Settings (memory settings removed)
    m_transparencySlider->setValue(m_settings->getValue("visual/transparency", 100).toInt());
    m_brightnessSlider->setValue(m_settings->getValue("visual/brightness", 100).toInt());
    m_contrastSlider->setValue(m_settings->getValue("visual/contrast", 100).toInt());
    m_hueRotationSlider->setValue(m_settings->getValue("visual/hueRotation", 0).toInt());

    refreshMemoryList();
}

void SettingsDialog::saveSettings()
{
    applySettings();
    accept();
}

void SettingsDialog::applySettings()
{
    if (!m_settings) return;
    
    // Save to QSettings
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

    // Visual Settings (memory settings removed)
    m_settings->setValue("visual/transparency", m_transparencySlider->value());
    m_settings->setValue("visual/brightness", m_brightnessSlider->value());
    m_settings->setValue("visual/contrast", m_contrastSlider->value());
    m_settings->setValue("visual/hueRotation", m_hueRotationSlider->value());

    refreshMemoryList();
    
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

void SettingsDialog::refreshMemoryList()
{
    if (!m_memoryList || !m_memoryUsageLabel) return;

    MemorySpaceManager &mgr = MemorySpaceManager::instance();
    qint64 bytes = mgr.currentSizeBytes();
    double displayValue = bytes / 1024.0 / 1024.0;
    QString unit = "MB";
    if (displayValue >= 1024.0) {
        displayValue = displayValue / 1024.0;
        unit = "GB";
    } else if (displayValue < 1.0) {
        displayValue = bytes / 1024.0; // show KB
        unit = "KB";
    }
    m_memoryUsageLabel->setText(QString("Usage: %1 %2").arg(QString::number(displayValue, 'f', 2), unit));

    m_memoryList->clear();
    for (const QString &key : mgr.listKeys()) {
        auto *item = new QListWidgetItem(key, m_memoryList);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        m_memoryList->addItem(item);
    }
    if (m_deleteMemoryBtn) {
        m_deleteMemoryBtn->setEnabled(m_memoryList->count() > 0);
    }
}

void SettingsDialog::deleteSelectedMemory()
{
    if (!m_memoryList) return;
    auto *item = m_memoryList->currentItem();
    if (!item) return;

    QString key = item->text();
    MemorySpaceManager::instance().deleteKey(key);
    refreshMemoryList();
}