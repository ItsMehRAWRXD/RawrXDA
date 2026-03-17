#include "autonomous_settings_dialog.h"
#include "../autonomous_feature_engine.h"
#include "../autonomous_model_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QDebug>

AutonomousSettingsDialog::AutonomousSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    
    setWindowTitle("Autonomous Features Settings");
    setMinimumWidth(500);
    setMinimumHeight(600);
    
    setupUI();
    setupConnections();
}

AutonomousSettingsDialog::~AutonomousSettingsDialog() = default;

void AutonomousSettingsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    QTabWidget* tabWidget = new QTabWidget();
    
    // ===== Analysis Tab =====
    QWidget* analysisTab = new QWidget();
    QVBoxLayout* analysisLayout = new QVBoxLayout(analysisTab);
    
    // Analysis Interval
    QGroupBox* intervalGroup = new QGroupBox("Analysis Interval");
    QVBoxLayout* intervalLayout = new QVBoxLayout(intervalGroup);
    
    QHBoxLayout* intervalH = new QHBoxLayout();
    intervalH->addWidget(new QLabel("Interval (ms):"));
    m_analysisIntervalSpinBox = new QSpinBox();
    m_analysisIntervalSpinBox->setRange(1000, 300000);
    m_analysisIntervalSpinBox->setValue(30000);
    m_analysisIntervalSpinBox->setSingleStep(1000);
    intervalH->addWidget(m_analysisIntervalSpinBox);
    intervalH->addStretch();
    intervalLayout->addLayout(intervalH);
    
    QLabel* intervalLabel = new QLabel("How often to analyze code (milliseconds)");
    intervalLabel->setStyleSheet("color: #666;");
    intervalLayout->addWidget(intervalLabel);
    
    analysisLayout->addWidget(intervalGroup);
    
    // Confidence Threshold
    QGroupBox* confidenceGroup = new QGroupBox("Confidence Threshold");
    QVBoxLayout* confidenceLayout = new QVBoxLayout(confidenceGroup);
    
    QHBoxLayout* confidenceH = new QHBoxLayout();
    confidenceH->addWidget(new QLabel("Threshold:"));
    m_confidenceThresholdSpinBox = new QDoubleSpinBox();
    m_confidenceThresholdSpinBox->setRange(0.0, 1.0);
    m_confidenceThresholdSpinBox->setValue(0.70);
    m_confidenceThresholdSpinBox->setSingleStep(0.05);
    m_confidenceThresholdSpinBox->setDecimals(2);
    confidenceH->addWidget(m_confidenceThresholdSpinBox);
    confidenceH->addStretch();
    confidenceLayout->addLayout(confidenceH);
    
    QLabel* confidenceLabel = new QLabel("Minimum confidence (0.0-1.0) to show suggestions");
    confidenceLabel->setStyleSheet("color: #666;");
    confidenceLayout->addWidget(confidenceLabel);
    
    analysisLayout->addWidget(confidenceGroup);
    
    // Feature Toggles
    QGroupBox* toggleGroup = new QGroupBox("Feature Toggles");
    QVBoxLayout* toggleLayout = new QVBoxLayout(toggleGroup);
    
    m_enableRealtimeCheckbox = new QCheckBox("Enable Real-Time Analysis");
    m_enableRealtimeCheckbox->setChecked(true);
    toggleLayout->addWidget(m_enableRealtimeCheckbox);
    
    m_enableAutoSuggestionsCheckbox = new QCheckBox("Enable Automatic Suggestions");
    m_enableAutoSuggestionsCheckbox->setChecked(true);
    toggleLayout->addWidget(m_enableAutoSuggestionsCheckbox);
    
    QHBoxLayout* maxAnalysisH = new QHBoxLayout();
    maxAnalysisH->addWidget(new QLabel("Max Concurrent Analyses:"));
    m_maxConcurrentAnalysesSpinBox = new QSpinBox();
    m_maxConcurrentAnalysesSpinBox->setRange(1, 16);
    m_maxConcurrentAnalysesSpinBox->setValue(4);
    maxAnalysisH->addWidget(m_maxConcurrentAnalysesSpinBox);
    maxAnalysisH->addStretch();
    toggleLayout->addLayout(maxAnalysisH);
    
    analysisLayout->addWidget(toggleGroup);
    analysisLayout->addStretch();
    
    tabWidget->addTab(analysisTab, "Analysis");
    
    // ===== Features Tab =====
    QWidget* featuresTab = new QWidget();
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresTab);
    
    QGroupBox* featureGroup = new QGroupBox("Feature Toggles");
    QVBoxLayout* featureGroupLayout = new QVBoxLayout(featureGroup);
    
    m_enableTestGenCheckbox = new QCheckBox("Test Generation");
    m_enableTestGenCheckbox->setChecked(true);
    featureGroupLayout->addWidget(m_enableTestGenCheckbox);
    
    m_enableSecurityCheckbox = new QCheckBox("Security Analysis");
    m_enableSecurityCheckbox->setChecked(true);
    featureGroupLayout->addWidget(m_enableSecurityCheckbox);
    
    m_enableOptimizationCheckbox = new QCheckBox("Performance Optimization");
    m_enableOptimizationCheckbox->setChecked(true);
    featureGroupLayout->addWidget(m_enableOptimizationCheckbox);
    
    m_enableDocumentationCheckbox = new QCheckBox("Documentation Generation");
    m_enableDocumentationCheckbox->setChecked(true);
    featureGroupLayout->addWidget(m_enableDocumentationCheckbox);
    
    featuresLayout->addWidget(featureGroup);
    featuresLayout->addStretch();
    
    tabWidget->addTab(featuresTab, "Features");
    
    // ===== Memory Tab =====
    QWidget* memoryTab = new QWidget();
    QVBoxLayout* memoryLayout = new QVBoxLayout(memoryTab);
    
    QGroupBox* memoryGroup = new QGroupBox("Memory Settings");
    QVBoxLayout* memoryGroupLayout = new QVBoxLayout(memoryGroup);
    
    QHBoxLayout* maxMemH = new QHBoxLayout();
    maxMemH->addWidget(new QLabel("Max Memory (GB):"));
    m_maxMemorySpinBox = new QSpinBox();
    m_maxMemorySpinBox->setRange(1, 256);
    m_maxMemorySpinBox->setValue(16);
    maxMemH->addWidget(m_maxMemorySpinBox);
    maxMemH->addStretch();
    memoryGroupLayout->addLayout(maxMemH);
    
    QHBoxLayout* prefetchH = new QHBoxLayout();
    prefetchH->addWidget(new QLabel("Prefetch Size (MB):"));
    m_prefetchSizeSpinBox = new QSpinBox();
    m_prefetchSizeSpinBox->setRange(10, 1000);
    m_prefetchSizeSpinBox->setValue(256);
    prefetchH->addWidget(m_prefetchSizeSpinBox);
    prefetchH->addStretch();
    memoryGroupLayout->addLayout(prefetchH);
    
    QHBoxLayout* strategyH = new QHBoxLayout();
    strategyH->addWidget(new QLabel("Prefetch Strategy:"));
    m_prefetchStrategyCombo = new QComboBox();
    m_prefetchStrategyCombo->addItems({"LRU", "Sequential", "Adaptive", "Aggressive"});
    m_prefetchStrategyCombo->setCurrentIndex(0);
    strategyH->addWidget(m_prefetchStrategyCombo);
    strategyH->addStretch();
    memoryGroupLayout->addLayout(strategyH);
    
    m_enableStreamingCheckbox = new QCheckBox("Enable Streaming (for models >2GB)");
    m_enableStreamingCheckbox->setChecked(true);
    memoryGroupLayout->addWidget(m_enableStreamingCheckbox);
    
    memoryLayout->addWidget(memoryGroup);
    memoryLayout->addStretch();
    
    tabWidget->addTab(memoryTab, "Memory");
    
    // ===== Models Tab =====
    QWidget* modelsTab = new QWidget();
    QVBoxLayout* modelsLayout = new QVBoxLayout(modelsTab);
    
    QGroupBox* modelGroup = new QGroupBox("Model Settings");
    QVBoxLayout* modelGroupLayout = new QVBoxLayout(modelGroup);
    
    QHBoxLayout* maxSizeH = new QHBoxLayout();
    maxSizeH->addWidget(new QLabel("Max Model Size (GB):"));
    m_maxModelSizeSpinBox = new QSpinBox();
    m_maxModelSizeSpinBox->setRange(1, 100);
    m_maxModelSizeSpinBox->setValue(10);
    maxSizeH->addWidget(m_maxModelSizeSpinBox);
    maxSizeH->addStretch();
    modelGroupLayout->addLayout(maxSizeH);
    
    m_autoDownloadCheckbox = new QCheckBox("Auto-Download Recommended Models");
    m_autoDownloadCheckbox->setChecked(false);
    modelGroupLayout->addWidget(m_autoDownloadCheckbox);
    
    m_autoOptimizeCheckbox = new QCheckBox("Auto-Optimize Models for System");
    m_autoOptimizeCheckbox->setChecked(true);
    modelGroupLayout->addWidget(m_autoOptimizeCheckbox);
    
    modelsLayout->addWidget(modelGroup);
    modelsLayout->addStretch();
    
    tabWidget->addTab(modelsTab, "Models");
    
    mainLayout->addWidget(tabWidget);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_applyButton = new QPushButton("Apply");
    m_applyButton->setStyleSheet("background-color: #2196F3; color: white; padding: 8px; border-radius: 4px;");
    buttonLayout->addWidget(m_applyButton);
    
    m_restoreDefaultsButton = new QPushButton("Restore Defaults");
    m_restoreDefaultsButton->setStyleSheet("background-color: #FF9800; color: white; padding: 8px; border-radius: 4px;");
    buttonLayout->addWidget(m_restoreDefaultsButton);
    
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("OK");
    m_okButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px; border-radius: 4px;");
    buttonLayout->addWidget(m_okButton);
    
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setStyleSheet("background-color: #f44336; color: white; padding: 8px; border-radius: 4px;");
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}

void AutonomousSettingsDialog::setupConnections() {
    connect(m_applyButton, &QPushButton::clicked, this, &AutonomousSettingsDialog::onApplyClicked);
    connect(m_okButton, &QPushButton::clicked, this, &AutonomousSettingsDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &AutonomousSettingsDialog::onCancelClicked);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &AutonomousSettingsDialog::onRestoreDefaultsClicked);
    
    connect(m_analysisIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AutonomousSettingsDialog::onAnalysisIntervalChanged);
    connect(m_confidenceThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AutonomousSettingsDialog::onConfidenceThresholdChanged);
}

void AutonomousSettingsDialog::onApplyClicked() {
    qDebug() << "[AutonomousSettingsDialog] Settings applied";
}

void AutonomousSettingsDialog::onOkClicked() {
    onApplyClicked();
    accept();
}

void AutonomousSettingsDialog::onCancelClicked() {
    reject();
}

void AutonomousSettingsDialog::onRestoreDefaultsClicked() {
    // Restore defaults
    m_analysisIntervalSpinBox->setValue(30000);
    m_confidenceThresholdSpinBox->setValue(0.70);
    m_enableRealtimeCheckbox->setChecked(true);
    m_enableAutoSuggestionsCheckbox->setChecked(true);
    m_maxConcurrentAnalysesSpinBox->setValue(4);
    m_enableTestGenCheckbox->setChecked(true);
    m_enableSecurityCheckbox->setChecked(true);
    m_enableOptimizationCheckbox->setChecked(true);
    m_enableDocumentationCheckbox->setChecked(true);
    m_maxMemorySpinBox->setValue(16);
    m_prefetchSizeSpinBox->setValue(256);
    m_prefetchStrategyCombo->setCurrentIndex(0);
    m_enableStreamingCheckbox->setChecked(true);
    m_maxModelSizeSpinBox->setValue(10);
    m_autoDownloadCheckbox->setChecked(false);
    m_autoOptimizeCheckbox->setChecked(true);
    
    qDebug() << "[AutonomousSettingsDialog] Settings restored to defaults";
}

void AutonomousSettingsDialog::onAnalysisIntervalChanged(int value) {
    qDebug() << "[AutonomousSettingsDialog] Analysis interval changed to" << value << "ms";
}

void AutonomousSettingsDialog::onConfidenceThresholdChanged(double value) {
    qDebug() << "[AutonomousSettingsDialog] Confidence threshold changed to" << value;
}

void AutonomousSettingsDialog::loadSettings(AutonomousFeatureEngine* featureEngine, AutonomousModelManager* modelManager) {
    if (!featureEngine) return;
    
    // Load from feature engine
    m_analysisIntervalSpinBox->setValue(30000); // TODO: Get from engine
    m_confidenceThresholdSpinBox->setValue(featureEngine->getConfidenceThreshold());
}

void AutonomousSettingsDialog::applySettings(AutonomousFeatureEngine* featureEngine, AutonomousModelManager* modelManager) {
    if (!featureEngine) return;
    
    // Apply to feature engine
    featureEngine->setAnalysisInterval(m_analysisIntervalSpinBox->value());
    featureEngine->setConfidenceThreshold(m_confidenceThresholdSpinBox->value());
    featureEngine->enableRealTimeAnalysis(m_enableRealtimeCheckbox->isChecked());
    featureEngine->enableAutomaticSuggestions(m_enableAutoSuggestionsCheckbox->isChecked());
    featureEngine->setMaxConcurrentAnalyses(m_maxConcurrentAnalysesSpinBox->value());
    
    qDebug() << "[AutonomousSettingsDialog] Settings applied to engines";
}
