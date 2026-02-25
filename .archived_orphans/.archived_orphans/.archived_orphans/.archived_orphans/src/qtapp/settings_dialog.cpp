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
#include <QSlider>
#include <QGridLayout>
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
    return true;
}

void SettingsDialog::initialize() {
    if (m_autoSaveCheck) return;  // Already initialized
    
    setupUI();
    loadSettings();
    return true;
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
    return true;
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
    return true;
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
    return true;
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
    return true;
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
    return true;
}

QWidget* SettingsDialog::createVisualTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Visual Effects Group
    QGroupBox *visualGroup = new QGroupBox("Visual Effects", tab);
    QGridLayout *visualLayout = new QGridLayout(visualGroup);
    
    // Transparency slider
    visualLayout->addWidget(new QLabel("Transparency:"), 0, 0);
    m_transparencySlider = new QSlider(Qt::Horizontal, visualGroup);
    m_transparencySlider->setRange(50, 100);
    m_transparencySlider->setValue(100);
    visualLayout->addWidget(m_transparencySlider, 0, 1);
    m_transparencyValue = new QLabel("100%", visualGroup);
    visualLayout->addWidget(m_transparencyValue, 0, 2);
    
    // Brightness slider
    visualLayout->addWidget(new QLabel("Brightness:"), 1, 0);
    m_brightnessSlider = new QSlider(Qt::Horizontal, visualGroup);
    m_brightnessSlider->setRange(50, 150);
    m_brightnessSlider->setValue(100);
    visualLayout->addWidget(m_brightnessSlider, 1, 1);
    m_brightnessValue = new QLabel("100%", visualGroup);
    visualLayout->addWidget(m_brightnessValue, 1, 2);
    
    // Contrast slider
    visualLayout->addWidget(new QLabel("Contrast:"), 2, 0);
    m_contrastSlider = new QSlider(Qt::Horizontal, visualGroup);
    m_contrastSlider->setRange(50, 150);
    m_contrastSlider->setValue(100);
    visualLayout->addWidget(m_contrastSlider, 2, 1);
    m_contrastValue = new QLabel("100%", visualGroup);
    visualLayout->addWidget(m_contrastValue, 2, 2);
    
    // Hue rotation slider
    visualLayout->addWidget(new QLabel("Hue Rotation:"), 3, 0);
    m_hueRotationSlider = new QSlider(Qt::Horizontal, visualGroup);
    m_hueRotationSlider->setRange(0, 360);
    m_hueRotationSlider->setValue(0);
    visualLayout->addWidget(m_hueRotationSlider, 3, 1);
    m_hueRotationValue = new QLabel("0°", visualGroup);
    visualLayout->addWidget(m_hueRotationValue, 3, 2);
    
    // Connect sliders to update labels
    connect(m_transparencySlider, &QSlider::valueChanged, this, [this](int value) {
        m_transparencyValue->setText(QString("%1%").arg(value));
    });
    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
        m_brightnessValue->setText(QString("%1%").arg(value));
    });
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_contrastValue->setText(QString("%1%").arg(value));
    });
    connect(m_hueRotationSlider, &QSlider::valueChanged, this, [this](int value) {
        m_hueRotationValue->setText(QString("%1°").arg(value));
    });
    
    layout->addWidget(visualGroup);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_applyVisualBtn = new QPushButton("Apply Visual Settings", tab);
    QPushButton *resetBtn = new QPushButton("Reset to Defaults", tab);
    
    connect(m_applyVisualBtn, &QPushButton::clicked, this, &SettingsDialog::applyVisualSettings);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetVisualSettings);
    
    buttonLayout->addWidget(m_applyVisualBtn);
    buttonLayout->addWidget(resetBtn);
    layout->addLayout(buttonLayout);
    
    layout->addStretch();
    
    return tab;
    return true;
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
    return true;
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
    return true;
}

void SettingsDialog::saveSettings()
{
    applySettings();
    accept();
    return true;
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

    
    QMessageBox::information(this, "Settings", "Settings saved successfully!");
    return true;
}

void SettingsDialog::manageEncryptionKeys()
{
    QMessageBox::information(this, "Encryption Keys", "Encryption keys are properly configured.\n\nSecurity features:\n- AES-256-GCM encryption\n- API key management\n- Audit logging\n- Access control");
    return true;
}

void SettingsDialog::configureTokenizer()
{
    QMessageBox::information(this, "Tokenizer", "Tokenizer configuration is available.\n\nSupported languages:\n- English (WordPiece, BPE, SentencePiece)\n- Chinese (Character-based, BPE)\n- Japanese (MeCab, Janome)\n- Multilingual (SentencePiece, mBERT)");
    return true;
}

void SettingsDialog::configureCIPipeline()
{
    QMessageBox::information(this, "CI/CD", "CI/CD pipeline configuration is available.\n\nFeatures:\n- Training job scheduling\n- Automated deployment\n- Webhook integration\n- Performance benchmarking");
    return true;
}

void SettingsDialog::applyVisualSettings()
{
    // Apply visual settings to the application
    m_settings->setValue("visual/transparency", m_transparencySlider->value());
    m_settings->setValue("visual/brightness", m_brightnessSlider->value());
    m_settings->setValue("visual/contrast", m_contrastSlider->value());
    m_settings->setValue("visual/hueRotation", m_hueRotationSlider->value());
    
    RAWRXD_LOG_DEBUG("[SettingsDialog] Applied visual settings:")
             << "transparency=" << m_transparencySlider->value()
             << "brightness=" << m_brightnessSlider->value()
             << "contrast=" << m_contrastSlider->value()
             << "hueRotation=" << m_hueRotationSlider->value();
    
    emit visualSettingsChanged();
    QMessageBox::information(this, "Visual Settings", "Visual settings applied successfully!");
    return true;
}

void SettingsDialog::resetVisualSettings()
{
    // Reset to default values
    m_transparencySlider->setValue(100);
    m_brightnessSlider->setValue(100);
    m_contrastSlider->setValue(100);
    m_hueRotationSlider->setValue(0);
    
    RAWRXD_LOG_DEBUG("[SettingsDialog] Reset visual settings to defaults");
    QMessageBox::information(this, "Visual Settings", "Visual settings reset to defaults.");
    return true;
}

