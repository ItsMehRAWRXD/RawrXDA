#include "ai_digestion_panel.hpp"
#include "ai_digestion_engine.hpp"
#include "ai_workers.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>

AIDigestionPanel::AIDigestionPanel(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_digestionEngine(nullptr)
    , m_trainingPipeline(nullptr)
    , m_workerManager(std::make_unique<AIWorkerManager>())
    , m_digestionWorker(nullptr)
    , m_trainingWorker(nullptr)
    , m_isDigesting(false)
    , m_isTraining(false)
    , m_digestionProgress(nullptr)
    , m_trainingProgressValue(0.0)
    , m_progressUpdateTimer(nullptr)
    , m_statusUpdateTimer(nullptr)
{
    RAWRXD_INIT_TIMED("AIDigestionPanel");
    setupUI();
    
    // Initialize engines
    m_digestionEngine = std::make_unique<AIDigestionEngine>(this);
    m_trainingPipeline = std::make_unique<AITrainingPipeline>(this);
    
    connectSignals();
    connectWorkerSignals();
    
    // Setup timers
    m_progressUpdateTimer = new QTimer(this);
    m_progressUpdateTimer->setInterval(250);
    connect(m_progressUpdateTimer, &QTimer::timeout, this, &AIDigestionPanel::updateProgressDisplays);
    
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(1000);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &AIDigestionPanel::updateParameterWidgets);
    
    // Load default settings
    resetToDefaults();
    setupModelPresets();
}

AIDigestionPanel::~AIDigestionPanel() {
    if (m_digestionEngine && m_digestionEngine->isDigesting()) {
        m_digestionEngine->stopDigestion();
    }
    
    if (m_trainingPipeline && m_trainingPipeline->isTraining()) {
        m_trainingPipeline->stopTraining();
    }
}

void AIDigestionPanel::setupUI() {
    RAWRXD_TIMED_FUNC();
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);
    
    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setMovable(false);
    
    // Setup tabs
    setupFileInputTab();
    setupParametersTab();
    setupTrainingTab();
    setupModelsTab();
    setupLogTab();
    
    mainLayout->addWidget(m_tabWidget);
    
    setMinimumSize(900, 700);
    setWindowTitle("AI Digestion & Training System");
}

void AIDigestionPanel::setupFileInputTab() {
    m_fileInputTab = new QWidget();
    m_fileLayout = new QVBoxLayout(m_fileInputTab);
    m_fileLayout->setContentsMargins(12, 12, 12, 12);
    m_fileLayout->setSpacing(8);
    
    // Header
    QLabel* headerLabel = new QLabel("Input Files & Directories");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    m_fileLayout->addWidget(headerLabel);
    
    // File drop widget
    m_fileDropWidget = new FileDropWidget();
    m_fileDropWidget->setMinimumHeight(120);
    m_fileDropWidget->setMaximumHeight(120);
    m_fileLayout->addWidget(m_fileDropWidget);
    
    // File list
    m_fileListWidget = new QListWidget();
    m_fileListWidget->setAlternatingRowColors(true);
    m_fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileListWidget->setDragDropMode(QAbstractItemView::DropOnly);
    m_fileLayout->addWidget(m_fileListWidget);
    
    // File buttons
    m_fileButtonLayout = new QHBoxLayout();
    m_addFilesButton = new QPushButton("Add Files...");
    m_addDirectoryButton = new QPushButton("Add Directory...");
    m_clearFilesButton = new QPushButton("Clear All");
    
    m_addFilesButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    m_addDirectoryButton->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_clearFilesButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    
    m_fileButtonLayout->addWidget(m_addFilesButton);
    m_fileButtonLayout->addWidget(m_addDirectoryButton);
    m_fileButtonLayout->addStretch();
    m_fileButtonLayout->addWidget(m_clearFilesButton);
    
    m_fileLayout->addLayout(m_fileButtonLayout);
    
    // File statistics
    m_fileStatsLabel = new QLabel("No files selected");
    m_fileStatsLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    m_fileLayout->addWidget(m_fileStatsLabel);
    
    m_tabWidget->addTab(m_fileInputTab, "Input Files");
}

void AIDigestionPanel::setupParametersTab() {
    m_parametersTab = new QWidget();
    m_parametersLayout = new QVBoxLayout(m_parametersTab);
    m_parametersLayout->setContentsMargins(0, 0, 0, 0);
    
    // Scroll area for parameters
    m_parametersScrollArea = new QScrollArea();
    m_parametersScrollArea->setWidgetResizable(true);
    m_parametersScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_parametersScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_parametersWidget = new QWidget();
    m_parametersWidget->setMinimumWidth(850);
    
    QVBoxLayout* scrollLayout = new QVBoxLayout(m_parametersWidget);
    scrollLayout->setContentsMargins(12, 12, 12, 12);
    scrollLayout->setSpacing(12);
    
    setupParameterGroups();
    setupPresetButtons();
    
    scrollLayout->addStretch();
    
    m_parametersScrollArea->setWidget(m_parametersWidget);
    m_parametersLayout->addWidget(m_parametersScrollArea);
    
    m_tabWidget->addTab(m_parametersTab, "Parameters");
}

void AIDigestionPanel::setupParameterGroups() {
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(m_parametersWidget->layout());
    
    // Model Configuration Group
    m_modelConfigGroup = new QGroupBox("Model Configuration");
    m_modelConfigLayout = new QGridLayout(m_modelConfigGroup);
    
    // Model name
    m_modelConfigLayout->addWidget(new QLabel("Model Name:"), 0, 0);
    m_modelNameEdit = new QLineEdit("CustomAI");
    m_modelConfigLayout->addWidget(m_modelNameEdit, 0, 1, 1, 2);
    
    // Model size
    m_modelConfigLayout->addWidget(new QLabel("Model Size:"), 1, 0);
    m_modelSizeCombo = new QComboBox();
    m_modelSizeCombo->addItems({"1GB (Compact)", "3GB (Small)", "7GB (Medium)", "13GB (Large)", 
                                "30GB (XL)", "65GB (XXL)", "Custom..."});
    m_modelSizeCombo->setCurrentText("7GB (Medium)");
    m_modelConfigLayout->addWidget(m_modelSizeCombo, 1, 1);
    
    // Quantization
    m_modelConfigLayout->addWidget(new QLabel("Quantization:"), 1, 2);
    m_quantizationCombo = new QComboBox();
    m_quantizationCombo->addItems({"Q4_0 (Fast)", "Q4_1 (Balanced)", "Q5_0 (Quality)", 
                                   "Q5_1 (High Quality)", "Q8_0 (Very High)", "F16 (Float16)", "F32 (Float32)"});
    m_quantizationCombo->setCurrentText("Q4_0 (Fast)");
    m_modelConfigLayout->addWidget(m_quantizationCombo, 1, 3);
    
    // Output directory
    m_modelConfigLayout->addWidget(new QLabel("Output Directory:"), 2, 0);
    m_outputDirectoryEdit = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AI Models");
    m_browseDirButton = new QPushButton("Browse...");
    QHBoxLayout* dirLayout = new QHBoxLayout();
    dirLayout->addWidget(m_outputDirectoryEdit);
    dirLayout->addWidget(m_browseDirButton);
    m_modelConfigLayout->addLayout(dirLayout, 2, 1, 1, 3);
    
    // Max tokens
    m_modelConfigLayout->addWidget(new QLabel("Max Tokens:"), 3, 0);
    m_maxTokensSpin = new QSpinBox();
    m_maxTokensSpin->setRange(512, 8192);
    m_maxTokensSpin->setValue(2048);
    m_maxTokensSpin->setSuffix(" tokens");
    m_modelConfigLayout->addWidget(m_maxTokensSpin, 3, 1);
    
    // Chunk size
    m_modelConfigLayout->addWidget(new QLabel("Chunk Size:"), 3, 2);
    m_chunkSizeSpin = new QSpinBox();
    m_chunkSizeSpin->setRange(128, 2048);
    m_chunkSizeSpin->setValue(512);
    m_chunkSizeSpin->setSuffix(" tokens");
    m_modelConfigLayout->addWidget(m_chunkSizeSpin, 3, 3);
    
    // Overlap size
    m_modelConfigLayout->addWidget(new QLabel("Overlap Size:"), 4, 0);
    m_overlapSizeSpin = new QSpinBox();
    m_overlapSizeSpin->setRange(0, 512);
    m_overlapSizeSpin->setValue(64);
    m_overlapSizeSpin->setSuffix(" tokens");
    m_modelConfigLayout->addWidget(m_overlapSizeSpin, 4, 1);
    
    scrollLayout->addWidget(m_modelConfigGroup);
    
    // Extraction Configuration Group
    m_extractionConfigGroup = new QGroupBox("Content Extraction");
    m_extractionConfigLayout = new QGridLayout(m_extractionConfigGroup);
    
    // Extraction mode
    m_extractionConfigLayout->addWidget(new QLabel("Extraction Mode:"), 0, 0);
    m_extractionModeCombo = new QComboBox();
    m_extractionModeCombo->addItems({"Syntactic", "Semantic", "Functional", "Comprehensive"});
    m_extractionModeCombo->setCurrentText("Comprehensive");
    m_extractionConfigLayout->addWidget(m_extractionModeCombo, 0, 1, 1, 2);
    
    // Extraction options
    m_extractFunctionsCheck = new QCheckBox("Extract Functions");
    m_extractFunctionsCheck->setChecked(true);
    m_extractionConfigLayout->addWidget(m_extractFunctionsCheck, 1, 0);
    
    m_extractClassesCheck = new QCheckBox("Extract Classes");
    m_extractClassesCheck->setChecked(true);
    m_extractionConfigLayout->addWidget(m_extractClassesCheck, 1, 1);
    
    m_extractVariablesCheck = new QCheckBox("Extract Variables");
    m_extractVariablesCheck->setChecked(true);
    m_extractionConfigLayout->addWidget(m_extractVariablesCheck, 1, 2);
    
    m_extractCommentsCheck = new QCheckBox("Extract Comments");
    m_extractCommentsCheck->setChecked(true);
    m_extractionConfigLayout->addWidget(m_extractCommentsCheck, 2, 0);
    
    m_preserveStructureCheck = new QCheckBox("Preserve Structure");
    m_preserveStructureCheck->setChecked(true);
    m_extractionConfigLayout->addWidget(m_preserveStructureCheck, 2, 1);
    
    // Minimum content length
    m_extractionConfigLayout->addWidget(new QLabel("Min Content Length:"), 3, 0);
    m_minContentLengthSpin = new QSpinBox();
    m_minContentLengthSpin->setRange(10, 1000);
    m_minContentLengthSpin->setValue(50);
    m_minContentLengthSpin->setSuffix(" chars");
    m_extractionConfigLayout->addWidget(m_minContentLengthSpin, 3, 1);
    
    scrollLayout->addWidget(m_extractionConfigGroup);
    
    // Training Hyperparameters Group
    m_hyperparametersGroup = new QGroupBox("Training Hyperparameters");
    m_hyperparametersLayout = new QGridLayout(m_hyperparametersGroup);
    
    // Learning rate
    m_hyperparametersLayout->addWidget(new QLabel("Learning Rate:"), 0, 0);
    m_learningRateSpin = new QDoubleSpinBox();
    m_learningRateSpin->setDecimals(6);
    m_learningRateSpin->setRange(1e-6, 1e-1);
    m_learningRateSpin->setValue(5e-5);
    m_learningRateSpin->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    m_hyperparametersLayout->addWidget(m_learningRateSpin, 0, 1);
    
    // Epochs
    m_hyperparametersLayout->addWidget(new QLabel("Epochs:"), 0, 2);
    m_epochsSpin = new QSpinBox();
    m_epochsSpin->setRange(1, 100);
    m_epochsSpin->setValue(10);
    m_hyperparametersLayout->addWidget(m_epochsSpin, 0, 3);
    
    // Batch size
    m_hyperparametersLayout->addWidget(new QLabel("Batch Size:"), 1, 0);
    m_batchSizeSpin = new QSpinBox();
    m_batchSizeSpin->setRange(1, 64);
    m_batchSizeSpin->setValue(4);
    m_hyperparametersLayout->addWidget(m_batchSizeSpin, 1, 1);
    
    // Weight decay
    m_hyperparametersLayout->addWidget(new QLabel("Weight Decay:"), 1, 2);
    m_weightDecaySpin = new QDoubleSpinBox();
    m_weightDecaySpin->setDecimals(4);
    m_weightDecaySpin->setRange(0.0, 1.0);
    m_weightDecaySpin->setValue(0.0);
    m_hyperparametersLayout->addWidget(m_weightDecaySpin, 1, 3);
    
    // Warmup ratio
    m_hyperparametersLayout->addWidget(new QLabel("Warmup Ratio:"), 2, 0);
    m_warmupRatioSpin = new QDoubleSpinBox();
    m_warmupRatioSpin->setDecimals(3);
    m_warmupRatioSpin->setRange(0.0, 0.5);
    m_warmupRatioSpin->setValue(0.03);
    m_hyperparametersLayout->addWidget(m_warmupRatioSpin, 2, 1);
    
    // Scheduler
    m_hyperparametersLayout->addWidget(new QLabel("Scheduler:"), 2, 2);
    m_schedulerCombo = new QComboBox();
    m_schedulerCombo->addItems({"linear", "cosine", "cosine_with_restarts", "polynomial", "constant"});
    m_schedulerCombo->setCurrentText("cosine");
    m_hyperparametersLayout->addWidget(m_schedulerCombo, 2, 3);
    
    // Advanced options
    m_gradientCheckpointingCheck = new QCheckBox("Gradient Checkpointing");
    m_gradientCheckpointingCheck->setChecked(true);
    m_hyperparametersLayout->addWidget(m_gradientCheckpointingCheck, 3, 0, 1, 2);
    
    m_useFp16Check = new QCheckBox("Use FP16 Precision");
    m_useFp16Check->setChecked(false);
    m_hyperparametersLayout->addWidget(m_useFp16Check, 3, 2, 1, 2);
    
    scrollLayout->addWidget(m_hyperparametersGroup);
}

void AIDigestionPanel::setupPresetButtons() {
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(m_parametersWidget->layout());
    
    // Model Presets Group
    m_presetsGroup = new QGroupBox("Model Presets");
    m_presetsLayout = new QVBoxLayout(m_presetsGroup);
    
    QLabel* presetsInfo = new QLabel("Quick configurations for specialized AI models:");
    presetsInfo->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    m_presetsLayout->addWidget(presetsInfo);
    
    // Create preset widget
    m_presetWidget = new ModelPresetWidget();
    m_presetsLayout->addWidget(m_presetWidget);
    
    // Preset buttons layout
    QGridLayout* presetButtonsLayout = new QGridLayout();
    
    m_codeExpertButton = new QPushButton("Code Expert\n(C++, Python, JS)");
    m_codeExpertButton->setMinimumHeight(60);
    m_codeExpertButton->setStyleSheet("QPushButton { text-align: left; padding: 8px; }");
    presetButtonsLayout->addWidget(m_codeExpertButton, 0, 0);
    
    m_asmExpertButton = new QPushButton("Assembly Expert\n(ASM, Reverse Engineering)");
    m_asmExpertButton->setMinimumHeight(60);
    m_asmExpertButton->setStyleSheet("QPushButton { text-align: left; padding: 8px; }");
    presetButtonsLayout->addWidget(m_asmExpertButton, 0, 1);
    
    m_securityExpertButton = new QPushButton("Security Expert\n(Malware, Vulnerabilities)");
    m_securityExpertButton->setMinimumHeight(60);
    m_securityExpertButton->setStyleSheet("QPushButton { text-align: left; padding: 8px; }");
    presetButtonsLayout->addWidget(m_securityExpertButton, 1, 0);
    
    m_generalPurposeButton = new QPushButton("General Purpose\n(Balanced Configuration)");
    m_generalPurposeButton->setMinimumHeight(60);
    m_generalPurposeButton->setStyleSheet("QPushButton { text-align: left; padding: 8px; }");
    presetButtonsLayout->addWidget(m_generalPurposeButton, 1, 1);
    
    m_customPresetButton = new QPushButton("Save Custom Preset...");
    m_customPresetButton->setMinimumHeight(40);
    presetButtonsLayout->addWidget(m_customPresetButton, 2, 0, 1, 2);
    
    m_presetsLayout->addLayout(presetButtonsLayout);
    
    scrollLayout->addWidget(m_presetsGroup);
}

void AIDigestionPanel::setupTrainingTab() {
    m_trainingTab = new QWidget();
    m_trainingLayout = new QVBoxLayout(m_trainingTab);
    m_trainingLayout->setContentsMargins(12, 12, 12, 12);
    m_trainingLayout->setSpacing(8);
    
    setupDigestionControls();
    setupTrainingControls();
    
    // Progress display
    m_progressGroup = new QGroupBox("Progress & Status");
    m_progressLayout = new QVBoxLayout(m_progressGroup);
    
    // Digestion progress
    QLabel* digestionLabel = new QLabel("Digestion Progress:");
    m_progressLayout->addWidget(digestionLabel);
    
    m_digestionProgress = new QProgressBar();
    m_digestionProgress->setRange(0, 100);
    m_digestionProgress->setValue(0);
    m_progressLayout->addWidget(m_digestionProgress);
    
    m_digestionStatusLabel = new QLabel("Ready to digest content");
    m_digestionStatusLabel->setStyleSheet("QLabel { color: gray; }");
    m_progressLayout->addWidget(m_digestionStatusLabel);
    
    m_filesProcessedLabel = new QLabel("Files processed: 0 / 0");
    m_filesProcessedLabel->setStyleSheet("QLabel { color: gray; }");
    m_progressLayout->addWidget(m_filesProcessedLabel);
    
    // Training progress
    QLabel* trainingLabel = new QLabel("Training Progress:");
    m_progressLayout->addWidget(trainingLabel);
    
    m_trainingProgress = new QProgressBar();
    m_trainingProgress->setRange(0, 100);
    m_trainingProgress->setValue(0);
    m_progressLayout->addWidget(m_trainingProgress);
    
    m_trainingStatusLabel = new QLabel("Ready to train model");
    m_trainingStatusLabel->setStyleSheet("QLabel { color: gray; }");
    m_progressLayout->addWidget(m_trainingStatusLabel);
    
    m_trainingMetricsLabel = new QLabel("Training metrics will appear here");
    m_trainingMetricsLabel->setStyleSheet("QLabel { color: gray; }");
    m_progressLayout->addWidget(m_trainingMetricsLabel);
    
    m_trainingLayout->addWidget(m_progressGroup);
    
    // Training metrics widget
    m_metricsWidget = new TrainingMetricsWidget();
    m_trainingLayout->addWidget(m_metricsWidget);
    
    m_trainingLayout->addStretch();
    
    m_tabWidget->addTab(m_trainingTab, "Training");
}

void AIDigestionPanel::setupDigestionControls() {
    m_digestionControlsGroup = new QGroupBox("Content Digestion");
    m_digestionControlsLayout = new QHBoxLayout(m_digestionControlsGroup);
    
    m_startDigestionButton = new QPushButton("Start Digestion");
    m_startDigestionButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_startDigestionButton->setMinimumHeight(40);
    
    m_pauseDigestionButton = new QPushButton("Pause");
    m_pauseDigestionButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseDigestionButton->setEnabled(false);
    m_pauseDigestionButton->setMinimumHeight(40);
    
    m_resumeDigestionButton = new QPushButton("Resume");
    m_resumeDigestionButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_resumeDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setMinimumHeight(40);
    
    m_stopDigestionButton = new QPushButton("Stop");
    m_stopDigestionButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopDigestionButton->setEnabled(false);
    m_stopDigestionButton->setMinimumHeight(40);
    
    m_digestionControlsLayout->addWidget(m_startDigestionButton);
    m_digestionControlsLayout->addWidget(m_pauseDigestionButton);
    m_digestionControlsLayout->addWidget(m_resumeDigestionButton);
    m_digestionControlsLayout->addWidget(m_stopDigestionButton);
    m_digestionControlsLayout->addStretch();
    
    m_trainingLayout->addWidget(m_digestionControlsGroup);
}

void AIDigestionPanel::setupTrainingControls() {
    m_trainingControlsGroup = new QGroupBox("Model Training");
    m_trainingControlsLayout = new QHBoxLayout(m_trainingControlsGroup);
    
    m_startTrainingButton = new QPushButton("Start Training");
    m_startTrainingButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_startTrainingButton->setEnabled(false); // Enabled after successful digestion
    m_startTrainingButton->setMinimumHeight(40);
    
    m_pauseTrainingButton = new QPushButton("Pause");
    m_pauseTrainingButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseTrainingButton->setEnabled(false);
    m_pauseTrainingButton->setMinimumHeight(40);
    
    m_resumeTrainingButton = new QPushButton("Resume");
    m_resumeTrainingButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_resumeTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setMinimumHeight(40);
    
    m_stopTrainingButton = new QPushButton("Stop");
    m_stopTrainingButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopTrainingButton->setEnabled(false);
    m_stopTrainingButton->setMinimumHeight(40);
    
    m_trainingControlsLayout->addWidget(m_startTrainingButton);
    m_trainingControlsLayout->addWidget(m_pauseTrainingButton);
    m_trainingControlsLayout->addWidget(m_resumeTrainingButton);
    m_trainingControlsLayout->addWidget(m_stopTrainingButton);
    m_trainingControlsLayout->addStretch();
    
    m_trainingLayout->addWidget(m_trainingControlsGroup);
}

void AIDigestionPanel::setupModelsTab() {
    m_modelsTab = new QWidget();
    m_modelsLayout = new QVBoxLayout(m_modelsTab);
    m_modelsLayout->setContentsMargins(12, 12, 12, 12);
    m_modelsLayout->setSpacing(8);
    
    // Header
    QLabel* headerLabel = new QLabel("Trained Models");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    m_modelsLayout->addWidget(headerLabel);
    
    // Model manager widget
    m_modelManagerWidget = new ModelManagerWidget();
    m_modelsLayout->addWidget(m_modelManagerWidget);
    
    // Models table
    m_modelsTable = new QTableWidget(0, 5);
    m_modelsTable->setHorizontalHeaderLabels({"Name", "Size", "Type", "Created", "Path"});
    m_modelsTable->horizontalHeader()->setStretchLastSection(true);
    m_modelsTable->setAlternatingRowColors(true);
    m_modelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelsLayout->addWidget(m_modelsTable);
    
    // Model buttons
    m_modelButtonsLayout = new QHBoxLayout();
    
    m_loadModelButton = new QPushButton("Load Model");
    m_loadModelButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    
    m_testModelButton = new QPushButton("Test Model");
    m_testModelButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    
    m_quantizeModelButton = new QPushButton("Quantize");
    m_quantizeModelButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    
    m_exportModelButton = new QPushButton("Export");
    m_exportModelButton->setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
    
    m_deleteModelButton = new QPushButton("Delete");
    m_deleteModelButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    
    m_refreshModelsButton = new QPushButton("Refresh");
    m_refreshModelsButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    
    m_modelButtonsLayout->addWidget(m_loadModelButton);
    m_modelButtonsLayout->addWidget(m_testModelButton);
    m_modelButtonsLayout->addWidget(m_quantizeModelButton);
    m_modelButtonsLayout->addWidget(m_exportModelButton);
    m_modelButtonsLayout->addStretch();
    m_modelButtonsLayout->addWidget(m_deleteModelButton);
    m_modelButtonsLayout->addWidget(m_refreshModelsButton);
    
    m_modelsLayout->addLayout(m_modelButtonsLayout);
    
    m_tabWidget->addTab(m_modelsTab, "Models");
}

void AIDigestionPanel::setupLogTab() {
    m_logTab = new QWidget();
    m_logLayout = new QVBoxLayout(m_logTab);
    m_logLayout->setContentsMargins(12, 12, 12, 12);
    m_logLayout->setSpacing(8);
    
    // Header
    QLabel* headerLabel = new QLabel("Training & Digestion Log");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    m_logLayout->addWidget(headerLabel);
    
    // Log text edit
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Courier", 9));
    m_logTextEdit->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    m_logLayout->addWidget(m_logTextEdit);
    
    // Log buttons
    m_logButtonsLayout = new QHBoxLayout();
    
    m_clearLogButton = new QPushButton("Clear Log");
    m_clearLogButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    
    m_saveLogButton = new QPushButton("Save Log");
    m_saveLogButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    
    m_autoScrollLogCheck = new QCheckBox("Auto-scroll");
    m_autoScrollLogCheck->setChecked(true);
    
    m_logButtonsLayout->addWidget(m_clearLogButton);
    m_logButtonsLayout->addWidget(m_saveLogButton);
    m_logButtonsLayout->addStretch();
    m_logButtonsLayout->addWidget(m_autoScrollLogCheck);
    
    m_logLayout->addLayout(m_logButtonsLayout);
    
    m_tabWidget->addTab(m_logTab, "Log");
}

void AIDigestionPanel::connectSignals() {
    // File input signals
    connect(m_fileDropWidget, &FileDropWidget::filesDropped, this, &AIDigestionPanel::onFilesDropped);
    connect(m_addFilesButton, &QPushButton::clicked, this, &AIDigestionPanel::onAddFilesClicked);
    connect(m_addDirectoryButton, &QPushButton::clicked, this, &AIDigestionPanel::onAddDirectoryClicked);
    connect(m_clearFilesButton, &QPushButton::clicked, this, &AIDigestionPanel::onClearFilesClicked);
    connect(m_fileListWidget, &QListWidget::itemChanged, this, &AIDigestionPanel::onFileItemChanged);
    
    // Directory browser
    connect(m_browseDirButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", 
                                                       m_outputDirectoryEdit->text());
        if (!dir.isEmpty()) {
            m_outputDirectoryEdit->setText(dir);
        }
    });
    
    // Parameter signals
    connect(m_modelSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AIDigestionPanel::onModelSizeChanged);
    connect(m_quantizationCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &AIDigestionPanel::onQuantizationChanged);
    connect(m_extractionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIDigestionPanel::onExtractionModeChanged);
    connect(m_learningRateSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AIDigestionPanel::onLearningRateChanged);
    connect(m_epochsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AIDigestionPanel::onEpochsChanged);
    connect(m_batchSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AIDigestionPanel::onBatchSizeChanged);
    connect(m_maxTokensSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AIDigestionPanel::onMaxTokensChanged);
    
    // Preset buttons
    connect(m_codeExpertButton, &QPushButton::clicked, this, [this]() { 
        applyModelPreset("Code Expert"); 
    });
    connect(m_asmExpertButton, &QPushButton::clicked, this, [this]() { 
        applyModelPreset("Assembly Expert"); 
    });
    connect(m_securityExpertButton, &QPushButton::clicked, this, [this]() { 
        applyModelPreset("Security Expert"); 
    });
    connect(m_generalPurposeButton, &QPushButton::clicked, this, [this]() { 
        applyModelPreset("General Purpose"); 
    });
    
    // Control signals
    connect(m_startDigestionButton, &QPushButton::clicked, this, &AIDigestionPanel::onStartDigestionClicked);
    connect(m_stopDigestionButton, &QPushButton::clicked, this, &AIDigestionPanel::onStopDigestionClicked);
    connect(m_pauseDigestionButton, &QPushButton::clicked, this, &AIDigestionPanel::onPauseDigestionClicked);
    connect(m_resumeDigestionButton, &QPushButton::clicked, this, &AIDigestionPanel::onResumeDigestionClicked);
    
    connect(m_startTrainingButton, &QPushButton::clicked, this, &AIDigestionPanel::onStartTrainingClicked);
    connect(m_stopTrainingButton, &QPushButton::clicked, this, &AIDigestionPanel::onStopTrainingClicked);
    connect(m_pauseTrainingButton, &QPushButton::clicked, this, &AIDigestionPanel::onPauseTrainingClicked);
    connect(m_resumeTrainingButton, &QPushButton::clicked, this, &AIDigestionPanel::onResumeTrainingClicked);
    
    // Model management signals
    connect(m_loadModelButton, &QPushButton::clicked, this, &AIDigestionPanel::onLoadModelClicked);
    connect(m_testModelButton, &QPushButton::clicked, this, &AIDigestionPanel::onTestModelClicked);
    connect(m_exportModelButton, &QPushButton::clicked, this, &AIDigestionPanel::onExportModelClicked);
    connect(m_deleteModelButton, &QPushButton::clicked, this, &AIDigestionPanel::onDeleteModelClicked);
    connect(m_refreshModelsButton, &QPushButton::clicked, this, &AIDigestionPanel::updateModelList);
    
    // Log signals
    connect(m_clearLogButton, &QPushButton::clicked, this, [this]() {
        m_logTextEdit->clear();
    });
    connect(m_saveLogButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "Save Log", 
                                                       "digestion_log.txt", "Text Files (*.txt)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << m_logTextEdit->toPlainText();
            }
        }
    });
}

// File management slots
void AIDigestionPanel::onAddFilesClicked() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this, 
        "Select Files to Process",
        QString(),
        "Source Files (*.cpp *.hpp *.h *.c *.py *.js *.ts *.asm *.s);;All Files (*)"
    );
    
    if (!fileNames.isEmpty()) {
        for (const QString& fileName : fileNames) {
            addFileToList(fileName);
        }
        updateFileStats();
        logMessage(QString("Added %1 files").arg(fileNames.size()));
    }
}

void AIDigestionPanel::onAddDirectoryClicked() {
    QString dirName = QFileDialog::getExistingDirectory(
        this,
        "Select Directory to Process",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dirName.isEmpty()) {
        int addedCount = addDirectoryToList(dirName);
        updateFileStats();
        logMessage(QString("Added %1 files from directory: %2").arg(addedCount).arg(dirName));
    }
}

void AIDigestionPanel::onClearFilesClicked() {
    int count = m_fileListWidget->count();
    m_fileListWidget->clear();
    m_inputFiles.clear();
    updateFileStats();
    logMessage(QString("Cleared %1 files from list").arg(count));
}

void AIDigestionPanel::onFileItemChanged(QListWidgetItem* item) {
    if (!item) return;
    
    QString filePath = item->data(Qt::UserRole).toString();
    bool isChecked = (item->checkState() == Qt::Checked);
    
    if (isChecked && !m_inputFiles.contains(filePath)) {
        m_inputFiles.append(filePath);
    } else if (!isChecked && m_inputFiles.contains(filePath)) {
        m_inputFiles.removeOne(filePath);
    }
    
    updateFileStats();
}

void AIDigestionPanel::onFilesDropped(const QStringList& files) {
    int addedCount = 0;
    for (const QString& file : files) {
        QFileInfo info(file);
        if (info.isFile()) {
            addFileToList(file);
            addedCount++;
        } else if (info.isDir()) {
            addedCount += addDirectoryToList(file);
        }
    }
    
    updateFileStats();
    logMessage(QString("Dropped and added %1 files").arg(addedCount));
}

// Digestion control slots
void AIDigestionPanel::onStartDigestionClicked() {
    if (m_inputFiles.isEmpty()) {
        showErrorMessage("No Files Selected", "Please select files to process before starting digestion.");
        return;
    }
    
    if (!m_digestionEngine) {
        showErrorMessage("Engine Error", "AI Digestion Engine not available.");
        return;
    }
    
    // Update configuration from UI
    updateDigestionConfig();
    
    // Create and configure digestion worker
    m_digestionWorker = m_workerManager->createDigestionWorker(m_digestionEngine.get());
    
    // Connect worker signals
    connect(m_digestionWorker, &AIDigestionWorker::progressChanged,
            this, &AIDigestionPanel::onDigestionProgressChanged);
    connect(m_digestionWorker, &AIDigestionWorker::fileStarted,
            this, &AIDigestionPanel::onDigestionFileStarted);
    connect(m_digestionWorker, &AIDigestionWorker::fileCompleted,
            this, &AIDigestionPanel::onDigestionFileCompleted);
    connect(m_digestionWorker, &AIDigestionWorker::digestionCompleted,
            this, &AIDigestionPanel::onDigestionFinished);
    connect(m_digestionWorker, &AIDigestionWorker::errorOccurred,
            this, &AIDigestionPanel::onDigestionError);
    connect(m_digestionWorker, &AIDigestionWorker::stateChanged,
            this, &AIDigestionPanel::onDigestionStateChanged);
    
    // Update UI state
    setDigestionControlsEnabled(false);
    m_startDigestionButton->setEnabled(false);
    m_pauseDigestionButton->setEnabled(true);
    m_stopDigestionButton->setEnabled(true);
    
    // Start digestion in background worker
    m_workerManager->startDigestionWorker(m_digestionWorker, m_inputFiles);
    
    logMessage("Started content digestion process in background");
    emit digestionStarted();
}

void AIDigestionPanel::onStopDigestionClicked() {
    if (m_digestionWorker) {
        m_digestionWorker->stopDigestion();
        logMessage("Requested digestion stop");
    }
}

void AIDigestionPanel::onPauseDigestionClicked() {
    if (m_digestionWorker && m_digestionWorker->isRunning()) {
        m_digestionWorker->pauseDigestion();
        m_pauseDigestionButton->setEnabled(false);
        m_resumeDigestionButton->setEnabled(true);
        logMessage("Paused digestion process");
    }
}

void AIDigestionPanel::onResumeDigestionClicked() {
    if (m_digestionWorker && m_digestionWorker->isPaused()) {
        m_digestionWorker->resumeDigestion();
        m_pauseDigestionButton->setEnabled(true);
        m_resumeDigestionButton->setEnabled(false);
        logMessage("Resumed digestion process");
    }
}

// Training control slots
void AIDigestionPanel::onStartTrainingClicked() {
    if (!m_trainingPipeline) {
        showErrorMessage("Training Error", "AI Training Pipeline not available.");
        return;
    }
    
    if (m_currentDataset.samples.isEmpty()) {
        showErrorMessage("No Data", "Please complete content digestion before starting training.");
        return;
    }
    
    // Update training configuration from UI
    updateTrainingConfig();
    
    // Get configuration from UI
    AITrainingWorker::TrainingConfig config;
    config.epochs = m_epochsSpin->value();
    config.batchSize = m_batchSizeSpin->value();
    config.learningRate = m_learningRateSpin->value();
    config.useEarlyStopping = true;  // Default to true
    config.patience = 5;              // Default patience
    config.saveCheckpoints = true;
    config.checkpointFrequency = 1;
    
    QString modelName = m_modelNameEdit->text();
    QString outputPath = m_outputDirectoryEdit->text();
    
    // Create and configure training worker
    m_trainingWorker = m_workerManager->createTrainingWorker(m_trainingPipeline.get());
    
    // Connect all worker signals for comprehensive monitoring
    connect(m_trainingWorker, &AITrainingWorker::progressChanged,
            this, &AIDigestionPanel::onTrainingProgressChanged);
    connect(m_trainingWorker, &AITrainingWorker::epochStarted,
            this, &AIDigestionPanel::onTrainingEpochStarted);
    connect(m_trainingWorker, &AITrainingWorker::epochCompleted,
            this, &AIDigestionPanel::onTrainingEpochCompleted);
    connect(m_trainingWorker, &AITrainingWorker::batchCompleted,
            this, &AIDigestionPanel::onTrainingBatchCompleted);
    connect(m_trainingWorker, &AITrainingWorker::validationCompleted,
            this, &AIDigestionPanel::onTrainingValidationCompleted);
    connect(m_trainingWorker, &AITrainingWorker::checkpointSaved,
            this, &AIDigestionPanel::onTrainingCheckpointSaved);
    connect(m_trainingWorker, &AITrainingWorker::trainingCompleted,
            this, &AIDigestionPanel::onTrainingCompleted);
    connect(m_trainingWorker, &AITrainingWorker::errorOccurred,
            this, &AIDigestionPanel::onTrainingError);
    connect(m_trainingWorker, &AITrainingWorker::stateChanged,
            this, &AIDigestionPanel::onTrainingStateChanged);
    
    // Update UI state
    setTrainingControlsEnabled(false);
    m_startTrainingButton->setEnabled(false);
    m_pauseTrainingButton->setEnabled(true);
    m_stopTrainingButton->setEnabled(true);
    
    // Start training in background worker
    m_workerManager->startTrainingWorker(m_trainingWorker, m_currentDataset, modelName, outputPath, config);
    
    logMessage("Started model training process in background");
    emit trainingStarted();
}

void AIDigestionPanel::onStopTrainingClicked() {
    if (m_trainingWorker) {
        m_trainingWorker->stopTraining();
        logMessage("Requested training stop");
    }
}

void AIDigestionPanel::onPauseTrainingClicked() {
    if (m_trainingWorker && m_trainingWorker->isRunning()) {
        m_trainingWorker->pauseTraining();
        m_pauseTrainingButton->setEnabled(false);
        m_resumeTrainingButton->setEnabled(true);
        logMessage("Paused training process");
    }
}

void AIDigestionPanel::onResumeTrainingClicked() {
    if (m_trainingWorker && m_trainingWorker->isPaused()) {
        m_trainingWorker->resumeTraining();
        m_pauseTrainingButton->setEnabled(true);
        m_resumeTrainingButton->setEnabled(false);
        logMessage("Resumed training process");
    }
}

// Parameter update slots
void AIDigestionPanel::onParametersChanged() {
    updateDigestionConfig();
    updateTrainingConfig();
}

void AIDigestionPanel::onPresetSelected(int index) {
    if (index < 0 || index >= m_presetCombo->count()) {
        return;
    }
    
    QString presetName = m_presetCombo->itemText(index);
    loadPreset(presetName);
    logMessage(QString("Applied preset: %1").arg(presetName));
}

void AIDigestionPanel::onSavePresetClicked() {
    bool ok;
    QString name = QInputDialog::getText(
        this, 
        "Save Preset", 
        "Preset name:", 
        QLineEdit::Normal,
        "My Preset", 
        &ok
    );
    
    if (ok && !name.isEmpty()) {
        savePreset(name);
        m_presetCombo->addItem(name);
        logMessage(QString("Saved preset: %1").arg(name));
    }
}

void AIDigestionPanel::onDeletePresetClicked() {
    int currentIndex = m_presetCombo->currentIndex();
    if (currentIndex >= 0) {
        QString presetName = m_presetCombo->itemText(currentIndex);
        deletePreset(presetName);
        m_presetCombo->removeItem(currentIndex);
        logMessage(QString("Deleted preset: %1").arg(presetName));
    }
}

void AIDigestionPanel::onSelectOutputDirectory() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Output Directory",
        m_outputDirectoryEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        m_outputDirectoryEdit->setText(dir);
        logMessage(QString("Set output directory: %1").arg(dir));
    }
}

// Helper methods
void AIDigestionPanel::addFileToList(const QString& filePath) {
    // Check if file already exists
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        QListWidgetItem* item = m_fileListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == filePath) {
            return; // Already exists
        }
    }
    
    QFileInfo info(filePath);
    QListWidgetItem* item = new QListWidgetItem();
    item->setText(info.fileName());
    item->setData(Qt::UserRole, filePath);
    item->setToolTip(filePath);
    item->setCheckState(Qt::Checked);
    
    // Set icon based on file type
    QString suffix = info.suffix().toLower();
    if (suffix == "cpp" || suffix == "hpp" || suffix == "h" || suffix == "c") {
        item->setIcon(QIcon(":/icons/cpp.png"));
    } else if (suffix == "py") {
        item->setIcon(QIcon(":/icons/python.png"));
    } else if (suffix == "js" || suffix == "ts") {
        item->setIcon(QIcon(":/icons/javascript.png"));
    } else if (suffix == "asm" || suffix == "s") {
        item->setIcon(QIcon(":/icons/assembly.png"));
    }
    
    m_fileListWidget->addItem(item);
    m_inputFiles.append(filePath);
}

int AIDigestionPanel::addDirectoryToList(const QString& dirPath) {
    QStringList nameFilters;
    nameFilters << "*.cpp" << "*.hpp" << "*.h" << "*.c" 
                << "*.py" << "*.js" << "*.ts" << "*.asm" << "*.s";
    
    QDir dir(dirPath);
    QStringList files = dir.entryList(nameFilters, QDir::Files, QDir::Name);
    
    int addedCount = 0;
    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        addFileToList(fullPath);
        addedCount++;
    }
    
    // Recursively add subdirectories if option is enabled
    if (m_recursiveCheckBox && m_recursiveCheckBox->isChecked()) {
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& subDir : subDirs) {
            QString fullSubPath = dir.absoluteFilePath(subDir);
            addedCount += addDirectoryToList(fullSubPath);
        }
    }
    
    return addedCount;
}

void AIDigestionPanel::updateFileStats() {
    int totalFiles = m_fileListWidget->count();
    int selectedFiles = 0;
    
    for (int i = 0; i < totalFiles; ++i) {
        QListWidgetItem* item = m_fileListWidget->item(i);
        if (item && item->checkState() == Qt::Checked) {
            selectedFiles++;
        }
    }
    
    m_fileStatsLabel->setText(QString("Files: %1 selected / %2 total").arg(selectedFiles).arg(totalFiles));
    
    // Enable/disable digestion based on selection
    m_startDigestionButton->setEnabled(selectedFiles > 0);
}

void AIDigestionPanel::setDigestionControlsEnabled(bool enabled) {
    m_digestionControlsWidget->setEnabled(enabled);
    m_parametersTab->setEnabled(enabled);
}

void AIDigestionPanel::setTrainingControlsEnabled(bool enabled) {
    m_trainingControlsWidget->setEnabled(enabled);
}

void AIDigestionPanel::updateDigestionConfig() {
    if (!m_digestionEngine) return;
    
    DigestionConfig config;
    
    // Model configuration
    config.modelName = m_modelNameEdit->text();
    config.outputDirectory = m_outputDirectoryEdit->text();
    
    // Token and chunking settings
    config.maxTokens = m_maxTokensSpin->value();
    config.chunkSize = m_chunkSizeSpin->value();
    config.overlapSize = m_overlapSizeSpin->value();
    config.minContentLength = m_minContentLengthSpin->value();
    
    // Extraction mode
    QString modeText = m_extractionModeCombo->currentText();
    if (modeText == "Syntactic") {
        config.mode = ExtractionMode::Syntactic;
    } else if (modeText == "Semantic") {
        config.mode = ExtractionMode::Semantic;
    } else if (modeText == "Functional") {
        config.mode = ExtractionMode::Functional;
    } else {
        config.mode = ExtractionMode::Comprehensive;
    }
    
    // Extraction options
    config.extractComments = m_extractCommentsCheck->isChecked();
    config.extractFunctions = m_extractFunctionsCheck->isChecked();
    config.extractClasses = m_extractClassesCheck->isChecked();
    config.extractVariables = m_extractVariablesCheck->isChecked();
    config.preserveStructure = m_preserveStructureCheck->isChecked();
    
    // Quantization
    QString quantText = m_quantizationCombo->currentText();
    if (quantText.startsWith("Q4_0")) config.quantization = "Q4_0";
    else if (quantText.startsWith("Q4_1")) config.quantization = "Q4_1";
    else if (quantText.startsWith("Q5_0")) config.quantization = "Q5_0";
    else if (quantText.startsWith("Q5_1")) config.quantization = "Q5_1";
    else if (quantText.startsWith("Q8_0")) config.quantization = "Q8_0";
    else if (quantText.startsWith("F16")) config.quantization = "F16";
    else if (quantText.startsWith("F32")) config.quantization = "F32";
    
    // Training parameters
    config.learningRate = m_learningRateSpin->value();
    config.epochs = m_epochsSpin->value();
    
    // Structured logging: Configuration update
    qInfo() << "[CONFIG] Digestion config updated:"
            << "model=" << config.modelName
            << "mode=" << static_cast<int>(config.mode)
            << "max_tokens=" << config.maxTokens
            << "chunk_size=" << config.chunkSize
            << "quantization=" << config.quantization;
    
    m_digestionEngine->setConfig(config);
}

void AIDigestionPanel::updateTrainingConfig() {
    if (!m_trainingPipeline) return;
    
    // Set training hyperparameters
    TrainingHyperparameters params;
    params.learningRate = m_learningRateSpin->value();
    params.batchSize = m_batchSizeSpin->value();
    params.weightDecay = m_weightDecaySpin->value();
    params.warmupRatio = m_warmupRatioSpin->value();
    params.scheduler = m_schedulerCombo->currentText();
    params.useGradientCheckpointing = m_gradientCheckpointingCheck->isChecked();
    params.useFp16 = m_useFp16Check->isChecked();
    
    // Structured logging: Configuration update
    qInfo() << "[CONFIG] Training config updated:"
            << "learning_rate=" << params.learningRate
            << "batch_size=" << params.batchSize
            << "scheduler=" << params.scheduler
            << "gradient_checkpointing=" << params.useGradientCheckpointing
            << "fp16=" << params.useFp16;
    
    m_trainingPipeline->setHyperparameters(params);
}

void AIDigestionPanel::loadPreset(const QString& presetName) {
    // Load preset from configuration
    // This would load saved parameter combinations
    logMessage(QString("Loading preset: %1").arg(presetName));
}

void AIDigestionPanel::savePreset(const QString& presetName) {
    // Save current parameters as preset
    // This would save current UI state as a named preset
    logMessage(QString("Saving preset: %1").arg(presetName));
}

void AIDigestionPanel::deletePreset(const QString& presetName) {
    // Delete preset from configuration
    logMessage(QString("Deleting preset: %1").arg(presetName));
}

void AIDigestionPanel::logMessage(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp).arg(message);
    m_logTextEdit->append(logEntry);
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

void AIDigestionPanel::showErrorMessage(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
    logMessage(QString("ERROR: %1 - %2").arg(title).arg(message));
}

void AIDigestionPanel::connectWorkerSignals() {
    // Connect worker manager signals
    connect(m_workerManager.get(), &AIWorkerManager::workerStarted, this, 
            [this](QObject* worker) {
                logMessage("Background worker started");
            });
    
    connect(m_workerManager.get(), &AIWorkerManager::workerFinished, this,
            [this](QObject* worker) {
                logMessage("Background worker finished");
            });
    
    connect(m_workerManager.get(), &AIWorkerManager::allWorkersFinished, this,
            [this]() {
                logMessage("All background workers finished");
            });
}

void AIDigestionPanel::onDigestionProgressChanged(const AIDigestionWorker::Progress& progress) {
    // Update progress bar
    if (m_digestionProgress) {
        m_digestionProgress->setValue(static_cast<int>(progress.percentage));
    }
    
    // Update status label
    if (m_digestionStatusLabel) {
        QString statusText = QString("%1 (%2/%3 files)")
            .arg(progress.status)
            .arg(progress.currentFile)
            .arg(progress.totalFiles);
        m_digestionStatusLabel->setText(statusText);
    }
    
    // Update files processed label
    if (m_filesProcessedLabel) {
        QString timeInfo;
        if (progress.estimatedTime > 0) {
            int remainingMinutes = progress.estimatedTime / (60 * 1000);
            timeInfo = QString(" - ETA: %1 min").arg(remainingMinutes);
        }
        
        QString processed = QString("Processing: %1%2")
            .arg(progress.currentFileName)
            .arg(timeInfo);
        m_filesProcessedLabel->setText(processed);
    }
    
    logMessage(QString("Digestion progress: %1% - %2")
               .arg(QString::number(progress.percentage, 'f', 1))
               .arg(progress.status));
}

void AIDigestionPanel::onDigestionFileStarted(const QString& fileName) {
    logMessage(QString("Started processing file: %1").arg(fileName));
}

void AIDigestionPanel::onDigestionFileCompleted(const QString& fileName, bool success) {
    QString status = success ? "completed" : "failed";
    logMessage(QString("File %1: %2").arg(status).arg(fileName));
}

void AIDigestionPanel::onDigestionFinished(bool success, const QString& message) {
    logMessage(message);

    // Update UI state
    setDigestionControlsEnabled(true);
    m_startDigestionButton->setEnabled(true);
    m_pauseDigestionButton->setEnabled(false);
    m_stopDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setEnabled(false);
    
    if (success) {
        showSuccessMessage("Digestion Complete", "Content digestion completed successfully.");
        emit digestionCompleted(QString());
    } else {
        showErrorMessage("Digestion Failed", message);
    }
    
    // Clean up worker
    m_digestionWorker = nullptr;
}

void AIDigestionPanel::onDigestionError(const QString& error) {
    logMessage(QString("Digestion error: %1").arg(error));
    showErrorMessage("Digestion Error", error);
}

void AIDigestionPanel::onDigestionStateChanged(AIDigestionWorker::State state) {
    QString stateText;
    switch (state) {
        case AIDigestionWorker::State::Idle:
            stateText = "Idle";
            break;
        case AIDigestionWorker::State::Running:
            stateText = "Running";
            break;
        case AIDigestionWorker::State::Paused:
            stateText = "Paused";
            break;
        case AIDigestionWorker::State::Stopping:
            stateText = "Stopping";
            break;
        case AIDigestionWorker::State::Finished:
            stateText = "Finished";
            break;
        case AIDigestionWorker::State::Error:
            stateText = "Error";
            break;
    }
    
    logMessage(QString("Digestion state changed to: %1").arg(stateText));
}

void AIDigestionPanel::onTrainingProgressChanged(const AITrainingWorker::Progress& progress) {
    // Update progress bar
    if (m_trainingProgress) {
        m_trainingProgress->setValue(static_cast<int>(progress.percentage));
    }
    
    // Update status labels
    if (m_trainingStatusLabel) {
        QString statusText = QString("%1 - Epoch %2/%3")
            .arg(progress.status)
            .arg(progress.currentEpoch)
            .arg(progress.totalEpochs);
        m_trainingStatusLabel->setText(statusText);
    }
    
    if (m_trainingMetricsLabel) {
        QString metricsText = QString("Loss: %1 | Accuracy: %2 | LR: %3")
            .arg(QString::number(progress.loss, 'f', 4))
            .arg(QString::number(progress.accuracy, 'f', 3))
            .arg(QString::number(progress.learningRate, 'e', 2));
        m_trainingMetricsLabel->setText(metricsText);
    }
    
    logMessage(QString("Training progress: %1% - Epoch %2/%3 - Loss: %4")
               .arg(QString::number(progress.percentage, 'f', 1))
               .arg(progress.currentEpoch)
               .arg(progress.totalEpochs)
               .arg(QString::number(progress.loss, 'f', 4)));
}

void AIDigestionPanel::onTrainingEpochStarted(int epoch) {
    logMessage(QString("Training epoch %1 started").arg(epoch));
}

void AIDigestionPanel::onTrainingEpochCompleted(int epoch, double loss, double accuracy) {
    logMessage(QString("Epoch %1 completed - Loss: %2, Accuracy: %3")
               .arg(epoch)
               .arg(QString::number(loss, 'f', 4))
               .arg(QString::number(accuracy, 'f', 3)));
}

void AIDigestionPanel::onTrainingBatchCompleted(int batch, double batchLoss) {
    // Only log every 10th batch to avoid spam
    if (batch % 10 == 0) {
        logMessage(QString("Batch %1 completed - Loss: %2")
                   .arg(batch)
                   .arg(QString::number(batchLoss, 'f', 4)));
    }
}

void AIDigestionPanel::onTrainingValidationCompleted(double valLoss, double valAccuracy) {
    logMessage(QString("Validation completed - Loss: %1, Accuracy: %2")
               .arg(QString::number(valLoss, 'f', 4))
               .arg(QString::number(valAccuracy, 'f', 3)));
}

void AIDigestionPanel::onTrainingCheckpointSaved(const QString& path) {
    logMessage(QString("Checkpoint saved: %1").arg(path));
}

void AIDigestionPanel::onTrainingCompleted(bool success, const QString& modelPath) {
    QString message = success ? 
        QString("Model training completed successfully. Model saved to: %1").arg(modelPath) :
        "Model training failed";
    
    logMessage(message);
    
    // Update UI state
    setTrainingControlsEnabled(true);
    m_startTrainingButton->setEnabled(true);
    m_pauseTrainingButton->setEnabled(false);
    m_stopTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setEnabled(false);
    
    if (success) {
        showSuccessMessage("Training Complete", message);
        emit trainingCompleted(modelPath);
    } else {
        showErrorMessage("Training Failed", message);
    }
    
    // Clean up worker
    m_trainingWorker = nullptr;
}

void AIDigestionPanel::onTrainingError(const QString& error) {
    logMessage(QString("Training error: %1").arg(error));
    showErrorMessage("Training Error", error);
}

void AIDigestionPanel::onTrainingStateChanged(AITrainingWorker::State state) {
    QString stateText;
    switch (state) {
        case AITrainingWorker::State::Idle:
            stateText = "Idle";
            break;
        case AITrainingWorker::State::Preparing:
            stateText = "Preparing";
            break;
        case AITrainingWorker::State::Training:
            stateText = "Training";
            break;
        case AITrainingWorker::State::Validating:
            stateText = "Validating";
            break;
        case AITrainingWorker::State::Paused:
            stateText = "Paused";
            break;
        case AITrainingWorker::State::Stopping:
            stateText = "Stopping";
            break;
        case AITrainingWorker::State::Finished:
            stateText = "Finished";
            break;
        case AITrainingWorker::State::Error:
            stateText = "Error";
            break;
    }
    
    logMessage(QString("Training state changed to: %1").arg(stateText));
}

void AIDigestionPanel::showSuccessMessage(const QString& title, const QString& message) {
    QMessageBox::information(this, title, message);
    logMessage(QString("SUCCESS: %1 - %2").arg(title).arg(message));
}
void AIDigestionPanel::onModelSizeChanged(int size) { updateDigestionConfig(); }
void AIDigestionPanel::onQuantizationChanged(const QString& q) { updateDigestionConfig(); }
void AIDigestionPanel::onExtractionModeChanged(int mode) { updateDigestionConfig(); }
void AIDigestionPanel::onLearningRateChanged(double rate) { updateTrainingConfig(); }
void AIDigestionPanel::onEpochsChanged(int epochs) { updateTrainingConfig(); }
void AIDigestionPanel::onBatchSizeChanged(int size) { updateTrainingConfig(); }
void AIDigestionPanel::onMaxTokensChanged(int tokens) { updateDigestionConfig(); }
void AIDigestionPanel::onTrainingErrorWorker(const QString& error) { onTrainingError(error); }

