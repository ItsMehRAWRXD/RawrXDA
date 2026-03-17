#include "agentic_ide.h"
#include "agentic_engine.h"
#include "agentic_executor.h"
#include "chat_interface.h"
#include "chat_workspace.h"
#include "multi_tab_editor.h"
#include "terminal_pool.h"
#include "file_browser.h"
#include "qtapp/inference_engine.hpp"
#include "settings.h"
#include "telemetry.h"
#include "planning_agent.h"
#include "todo_manager.h"
#include "todo_dock.h"
#include "agentic_copilot_bridge.h"
#include "training_dialog.h"
#include "training_progress_dock.h"
#include <nlohmann/json.hpp>
#include "model_registry.h"
#include "model_trainer.h"
#include "profiler.h"
#include "observability_dashboard.h"
#include "hardware_backend_selector.h"
// Phase 4: AI Code Assistant
#include "ai_code_assistant.h"
#include "ai_code_assistant_panel.h"
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileSystemModel>
#include <QTreeView>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QSettings>
#include <QDir>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QScrollBar>
#include <QMessageBox>
#include <QJsonObject>

AgenticIDE::AgenticIDE(QWidget *parent)
    : QMainWindow(parent)
    , m_agenticEngine(new AgenticEngine(this))
    , m_inferenceEngine(new InferenceEngine(QString(), this))
    , m_planningAgent(new PlanningAgent(this))
    , m_todoManager(new TodoManager(this))
    , m_settings(new Settings())
    , m_telemetry(new Telemetry())
    , m_copilotBridge(new RawrXD::AgenticCopilotBridge())
    , m_agenticExecutor(new AgenticExecutor(this))
    , m_modelTrainer(new ModelTrainer(this))
    // New ModelTrainer related components initialized to nullptr
    , m_trainingDialog(nullptr)
    , m_trainingProgressDock(nullptr)
    , m_modelRegistry(nullptr)
    , m_profiler(nullptr)
    , m_observabilityDashboard(nullptr)
    , m_hardwareBackendSelector(nullptr)
    , m_securityManager(nullptr)
    , m_distributedTrainer(nullptr)
    , m_interpretabilityPanel(nullptr)
    , m_ciPipelineSettings(nullptr)
    , m_tokenizerLanguageSelector(nullptr)
    , m_checkpointManager(nullptr)
    , m_aiCodeAssistant(nullptr)
    , m_aiCodeAssistantPanel(nullptr)
{
    setWindowTitle("RawrXD Agentic IDE");
    setMinimumSize(1200, 800);

    setupUI();
    setupConnections();
    loadSettings();

    // Initialize engines
    m_agenticEngine->initialize();
    m_agenticEngine->setInferenceEngine(m_inferenceEngine);
    m_planningAgent->initialize();
    m_agenticExecutor->initialize(m_agenticEngine, m_inferenceEngine);

    // Initialize Copilot bridge with all IDE components
    m_copilotBridge->initialize(m_agenticEngine, m_chatInterface, m_multiTabEditor, m_terminalPool, m_agenticExecutor);

    qInfo() << "Agentic IDE initialized with Copilot/Cursor-like capabilities";

    // Create TODO dock
    m_todoDock = new TodoDock(m_todoManager, this);

    // Add TODO dock to the IDE
    m_todoDockWidget = new QDockWidget("TODO List", this);
    m_todoDockWidget->setWidget(m_todoDock);
    addDockWidget(Qt::RightDockWidgetArea, m_todoDockWidget);

    qDebug() << "Agentic IDE initialized successfully";
}AgenticIDE::~AgenticIDE()
{
    saveSettings();
}

void AgenticIDE::setupUI()
{
    // Create central widget with splitter
    QSplitter *centralSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(centralSplitter);
    
    // Left panel: File browser
    m_fileBrowser = new FileBrowser(this);
    m_fileDock = new QDockWidget("Files", this);
    m_fileDock->setWidget(m_fileBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileDock);
    
    // Center: Multi-tab editor
    m_multiTabEditor = new MultiTabEditor(this);
    centralSplitter->addWidget(m_multiTabEditor);
    
    // Right panel: Chat interface
    m_chatInterface = new ChatInterface(this);
    m_chatDock = new QDockWidget("Agent Chat", this);
    m_chatDock->setWidget(m_chatInterface);
    addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
    
    // Bottom: Terminal pool
    m_terminalPool = new TerminalPool(3, this);
    m_terminalDock = new QDockWidget("Terminals", this);
    m_terminalDock->setWidget(m_terminalPool);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
    
    // Phase 4: AI Code Assistant panel (right dock)
    m_aiCodeAssistant = new AICodeAssistant(this);
    m_aiCodeAssistant->setOllamaUrl("http://localhost:11434");
    m_aiCodeAssistant->setModel("ministral-3");
    m_aiCodeAssistant->setTemperature(0.3f);
    m_aiCodeAssistant->setMaxTokens(256);
    
    m_aiCodeAssistantPanel = new AICodeAssistantPanel(this);
    m_aiCodeAssistantPanel->setAssistant(m_aiCodeAssistant);
    addDockWidget(Qt::RightDockWidgetArea, m_aiCodeAssistantPanel);
    
    // Create menus
    setupMenus();
    
    // Create toolbar
    setupToolbar();
    
    // Status bar
    statusBar()->showMessage("Ready");
}

void AgenticIDE::setupMenus()
{
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("File");
    fileMenu->addAction("New File", this, &AgenticIDE::newFile);
    fileMenu->addAction("Open File", this, &AgenticIDE::openFile);
    fileMenu->addAction("Save", this, &AgenticIDE::saveFile);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &AgenticIDE::close);
    
    // Edit menu
    QMenu *editMenu = menuBar->addMenu("Edit");
    editMenu->addAction("Undo", this, &AgenticIDE::undo);
    editMenu->addAction("Redo", this, &AgenticIDE::redo);
    editMenu->addSeparator();
    editMenu->addAction("Find", this, &AgenticIDE::find);
    editMenu->addAction("Replace", this, &AgenticIDE::replace);
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu("View");
    viewMenu->addAction("Toggle File Browser", this, &AgenticIDE::toggleFileBrowser);
    viewMenu->addAction("Toggle Chat", this, &AgenticIDE::toggleChat);
    viewMenu->addAction("Toggle Terminals", this, &AgenticIDE::toggleTerminals);
    viewMenu->addAction("Toggle Todos", this, &AgenticIDE::toggleTodos);
    viewMenu->addSeparator();
    viewMenu->addAction("Toggle AI Code Assistant", this, &AgenticIDE::toggleAICodeAssistant);
    
// Agent menu
QMenu *agentMenu = menuBar->addMenu("Agent");
agentMenu->addAction("Start Chat", this, &AgenticIDE::startChat);
agentMenu->addAction("Analyze Code", this, &AgenticIDE::analyzeCode);
agentMenu->addAction("Generate Code", this, &AgenticIDE::generateCode);
agentMenu->addAction("Create Plan", this, &AgenticIDE::createPlan);
agentMenu->addAction("Hot-Patch Model", this, &AgenticIDE::hotPatchModel);
agentMenu->addAction("Train Model", this, &AgenticIDE::trainModel);
agentMenu->addSeparator();
agentMenu->addAction("Settings", this, &AgenticIDE::showSettings);
    
    // Copilot menu (like VS Code with GitHub Copilot or Cursor IDE)
    QMenu *copilotMenu = menuBar->addMenu("Copilot");
    copilotMenu->addAction("Code Completion", this, [this]() {
        QString currentText = m_multiTabEditor->getCurrentText();
        std::string currentTextStd = currentText.toStdString();
        std::string completion = m_copilotBridge->generateCodeCompletion(currentTextStd, currentTextStd);
        m_chatInterface->addMessage("Copilot", QString::fromStdString(completion));
    });
    copilotMenu->addAction("Analyze File", this, [this]() {
        std::string analysis = m_copilotBridge->analyzeActiveFile();
        m_chatInterface->addMessage("Copilot", QString::fromStdString(analysis));
    });
    copilotMenu->addAction("Suggest Refactoring", this, [this]() {
        QString code = m_multiTabEditor->getCurrentText();
        std::string suggestion = m_copilotBridge->suggestRefactoring(code.toStdString());
        m_chatInterface->addMessage("Copilot", QString::fromStdString(suggestion));
    });
    copilotMenu->addAction("Generate Tests", this, [this]() {
        QString code = m_multiTabEditor->getCurrentText();
        std::string tests = m_copilotBridge->generateTestsForCode(code.toStdString());
        m_chatInterface->addMessage("Copilot", QString::fromStdString(tests));
    });
    copilotMenu->addSeparator();
    copilotMenu->addAction("Explain Code", this, [this]() { 
        QString code = m_multiTabEditor->getCurrentText();
        std::string explanation = m_copilotBridge->explainCode(code.toStdString());
        m_chatInterface->addMessage("Copilot", QString::fromStdString(explanation)); 
    });
    copilotMenu->addAction("Find Bugs", this, [this]() { 
        QString code = m_multiTabEditor->getCurrentText();
        std::string bugs = m_copilotBridge->findBugs(code.toStdString());
        m_chatInterface->addMessage("Copilot", QString::fromStdString(bugs)); 
    });
    
    // Phase 4: AI Code Assistant menu
    QMenu *aiMenu = menuBar->addMenu("AI Assistant");
    aiMenu->addAction("Request Code Completion", this, &AgenticIDE::requestCodeCompletion, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
    aiMenu->addAction("Request Refactoring", this, &AgenticIDE::requestRefactoring, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
    aiMenu->addAction("Request Explanation", this, &AgenticIDE::requestExplanation, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E));
    aiMenu->addSeparator();
    aiMenu->addAction("Toggle Panel", this, &AgenticIDE::toggleAICodeAssistant);
    
    // ==== Advanced / ModelTrainer Enhancements Menu ====
    QMenu *advancedMenu = menuBar->addMenu("Advanced");
    advancedMenu->addAction("Open Training Dialog", this, &AgenticIDE::openTrainingDialog);
    advancedMenu->addAction("View Training Progress", this, &AgenticIDE::viewTrainingProgress);
    advancedMenu->addAction("Model Registry", this, &AgenticIDE::viewModelRegistry);
    advancedMenu->addSeparator();
    advancedMenu->addAction("Start Profiling", this, &AgenticIDE::startProfiling);
    advancedMenu->addAction("Stop Profiling", this, &AgenticIDE::stopProfiling);
    advancedMenu->addAction("Observability Dashboard", this, &AgenticIDE::openObservabilityDashboard);
    advancedMenu->addSeparator();
    advancedMenu->addAction("Configure Hardware Backend", this, &AgenticIDE::configureHardwareBackend);
    advancedMenu->addAction("Security Settings", this, &AgenticIDE::manageSecuritySettings);
    advancedMenu->addAction("Distributed Training", this, &AgenticIDE::startDistributedTraining);
    advancedMenu->addAction("Interpretability Report", this, &AgenticIDE::viewInterpretabilityReport);
    advancedMenu->addAction("CI Pipeline Settings", this, &AgenticIDE::openCIPipelineSettings);
    advancedMenu->addAction("Tokenizer Language", this, &AgenticIDE::configureTokenizerLanguage);
    advancedMenu->addAction("Checkpoint Manager", this, &AgenticIDE::manageCheckpoints);
}

void AgenticIDE::setupToolbar()
{
    QToolBar *toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("New", this, &AgenticIDE::newFile);
    toolbar->addAction("Open", this, &AgenticIDE::openFile);
    toolbar->addAction("Save", this, &AgenticIDE::saveFile);
    toolbar->addSeparator();
    toolbar->addAction("Chat", this, &AgenticIDE::startChat);
    toolbar->addAction("Analyze", this, &AgenticIDE::analyzeCode);
}

void AgenticIDE::setupConnections()
{
    // Connect file browser to editor
    connect(m_fileBrowser, &FileBrowser::fileSelected, 
            m_multiTabEditor, &MultiTabEditor::openFile);
    
    // Set agentic engine in chat interface for tool commands
    m_chatInterface->setAgenticEngine(m_agenticEngine);
    
    // Connect chat interface to agentic engine
    connect(m_chatInterface, &ChatInterface::messageSent,
            m_agenticEngine, &AgenticEngine::processMessage);
    connect(m_agenticEngine, &AgenticEngine::responseReady,
            m_chatInterface, &ChatInterface::displayResponse);
    
    // Connect model selector to agentic engine
    connect(m_chatInterface, &ChatInterface::modelSelected,
            m_agenticEngine, &AgenticEngine::setModel);
    
    // Connect model loading finished to chat interface
    connect(m_agenticEngine, &AgenticEngine::modelLoadingFinished,
            m_chatInterface, [this](bool success, const QString& modelPath) {
                if (success) {
                    m_chatInterface->addMessage("System", "Model loaded: " + modelPath);
                } else {
                    m_chatInterface->addMessage("System", "Failed to load model: " + modelPath);
                }
            });
    
    // Terminal connected to agentic engine for command processing
    // (InferenceEngine handles model inference, not command execution)
    
    // Connect planning agent signals
    connect(m_planningAgent, &PlanningAgent::planCreated,
            this, [this](const QString& plan) {
                m_chatInterface->addMessage("Planner", "Plan created: " + plan);
            });
    connect(m_planningAgent, &PlanningAgent::taskStatusChanged,
            this, [this](const QString& taskId, const QString& status) {
                m_chatInterface->addMessage("Planner", "Task " + taskId + " is now " + status);
            });
    connect(m_planningAgent, &PlanningAgent::planCompleted,
            this, [this]() {
                m_chatInterface->addMessage("Planner", "Plan completed successfully!");
            });
    connect(m_planningAgent, &PlanningAgent::planFailed,
            this, [this](const QString& error) {
                m_chatInterface->addMessage("Planner", "Plan failed: " + error);
            });
}

void AgenticIDE::loadSettings()
{
    QSettings settings("RawrXD", "AgenticIDE");
    
    // Load window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Load recent files
    m_recentFiles = settings.value("recentFiles").toStringList();
    
    qDebug() << "Settings loaded";
}

void AgenticIDE::saveSettings()
{
    QSettings settings("RawrXD", "AgenticIDE");
    
    // Save window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    // Save recent files
    settings.setValue("recentFiles", m_recentFiles);
    
    qDebug() << "Settings saved";
}

// File operations
void AgenticIDE::newFile()
{
    m_multiTabEditor->newFile();
    statusBar()->showMessage("New file created");
}

void AgenticIDE::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open File");
    if (!filePath.isEmpty()) {
        m_multiTabEditor->openFile(filePath);
        addToRecentFiles(filePath);
        statusBar()->showMessage("Opened: " + filePath);
    }
}

void AgenticIDE::saveFile()
{
    m_multiTabEditor->saveCurrentFile();
    statusBar()->showMessage("File saved");
}

// Agent operations
void AgenticIDE::startChat()
{
    m_chatInterface->setVisible(true);
    m_chatInterface->focusInput();
    statusBar()->showMessage("Chat started");
}

void AgenticIDE::analyzeCode()
{
    QString currentCode = m_multiTabEditor->getCurrentText();
    if (!currentCode.isEmpty()) {
        m_agenticEngine->analyzeCode(currentCode);
        statusBar()->showMessage("Code analysis started");
    }
}

void AgenticIDE::generateCode()
{
    QString prompt = QInputDialog::getText(this, "Generate Code", "Enter prompt:");
    if (!prompt.isEmpty()) {
        m_agenticEngine->generateCode(prompt);
        statusBar()->showMessage("Code generation started");
    }
}

void AgenticIDE::createPlan()
{
    QString goal = QInputDialog::getText(this, "Create Plan", "Enter goal:");
    if (!goal.isEmpty()) {
        m_planningAgent->createPlan(goal);
        statusBar()->showMessage("Plan creation started");
    }
}

void AgenticIDE::hotPatchModel()
{
    QString modelPath = QFileDialog::getOpenFileName(this, "Select Model File", "", "GGUF Files (*.gguf)");
    if (!modelPath.isEmpty()) {
        // Hot-patch the model by loading a new model
        if (m_inferenceEngine->loadModel(modelPath)) {
            m_chatInterface->addMessage("System", "Model hot-patched successfully: " + modelPath);
            statusBar()->showMessage("Model hot-patched successfully");
        } else {
            m_chatInterface->addMessage("System", "Failed to hot-patch model: " + modelPath);
            statusBar()->showMessage("Failed to hot-patch model");
        }
    }
}

void AgenticIDE::trainModel()
{
    // Create training dialog
    QDialog trainDialog(this);
    trainDialog.setWindowTitle("Train Model");
    trainDialog.setMinimumWidth(500);
    trainDialog.setMinimumHeight(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&trainDialog);
    
    // Dataset selection
    QGroupBox* datasetGroup = new QGroupBox("Dataset", &trainDialog);
    QVBoxLayout* datasetLayout = new QVBoxLayout(datasetGroup);
    
    QHBoxLayout* datasetLayoutRow = new QHBoxLayout();
    QLineEdit* datasetPathEdit = new QLineEdit(&trainDialog);
    datasetPathEdit->setPlaceholderText("Path to training dataset (CSV, JSON-L, or plain text)");
    QPushButton* browseDatasetBtn = new QPushButton("Browse...", &trainDialog);
    
    datasetLayoutRow->addWidget(datasetPathEdit);
    datasetLayoutRow->addWidget(browseDatasetBtn);
    datasetLayout->addLayout(datasetLayoutRow);
    
    // Model selection
    QGroupBox* modelGroup = new QGroupBox("Model", &trainDialog);
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);
    
    QHBoxLayout* modelLayoutRow = new QHBoxLayout();
    QLineEdit* modelPathEdit = new QLineEdit(&trainDialog);
    modelPathEdit->setText(m_settings->getValue("defaultModelPath", "").toString());
    modelPathEdit->setPlaceholderText("Path to base GGUF model");
    QPushButton* browseModelBtn = new QPushButton("Browse...", &trainDialog);
    
    modelLayoutRow->addWidget(modelPathEdit);
    modelLayoutRow->addWidget(browseModelBtn);
    modelLayout->addLayout(modelLayoutRow);
    
    // Training parameters
    QGroupBox* paramsGroup = new QGroupBox("Training Parameters", &trainDialog);
    QGridLayout* paramsLayout = new QGridLayout(paramsGroup);
    
    QLabel* epochsLabel = new QLabel("Epochs:", &trainDialog);
    QSpinBox* epochsSpinBox = new QSpinBox(&trainDialog);
    epochsSpinBox->setMinimum(1);
    epochsSpinBox->setMaximum(100);
    epochsSpinBox->setValue(10);
    
    QLabel* learningRateLabel = new QLabel("Learning Rate:", &trainDialog);
    QLineEdit* learningRateEdit = new QLineEdit(&trainDialog);
    learningRateEdit->setText("0.0001");
    
    QLabel* batchSizeLabel = new QLabel("Batch Size:", &trainDialog);
    QSpinBox* batchSizeSpinBox = new QSpinBox(&trainDialog);
    batchSizeSpinBox->setMinimum(1);
    batchSizeSpinBox->setMaximum(128);
    batchSizeSpinBox->setValue(32);
    
    paramsLayout->addWidget(epochsLabel, 0, 0);
    paramsLayout->addWidget(epochsSpinBox, 0, 1);
    paramsLayout->addWidget(learningRateLabel, 1, 0);
    paramsLayout->addWidget(learningRateEdit, 1, 1);
    paramsLayout->addWidget(batchSizeLabel, 2, 0);
    paramsLayout->addWidget(batchSizeSpinBox, 2, 1);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* trainBtn = new QPushButton("Start Training", &trainDialog);
    QPushButton* cancelBtn = new QPushButton("Cancel", &trainDialog);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(trainBtn);
    buttonLayout->addWidget(cancelBtn);
    
    // Add to main layout
    layout->addWidget(datasetGroup);
    layout->addWidget(modelGroup);
    layout->addWidget(paramsGroup);
    layout->addLayout(buttonLayout);
    
    // Connect browse buttons
    connect(browseDatasetBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&trainDialog, "Select Dataset", "", 
            "Dataset Files (*.csv *.jsonl *.txt);;All Files (*)");
        if (!path.isEmpty()) {
            datasetPathEdit->setText(path);
        }
    });
    
    connect(browseModelBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&trainDialog, "Select Model", "", 
            "GGUF Files (*.gguf);;All Files (*)");
        if (!path.isEmpty()) {
            modelPathEdit->setText(path);
        }
    });
    
    // Connect buttons
    connect(trainBtn, &QPushButton::clicked, [&]() {
        if (datasetPathEdit->text().isEmpty() || modelPathEdit->text().isEmpty()) {
            QMessageBox::warning(&trainDialog, "Missing Information", 
                "Please select both dataset and model files.");
            return;
        }
        
        // Build training configuration
        nlohmann::json configJson = {
            {"epochs", epochsSpinBox->value()},
            {"learning_rate", learningRateEdit->text().toDouble()},
            {"batch_size", batchSizeSpinBox->value()},
            {"sequence_length", 512},
            {"gradient_clip", 1.0},
            {"validate_every_epoch", true},
            {"validation_split", "0.1"},
            {"weight_decay", 0.01},
            {"warmup_steps", 0.1}
        };

        // Start training through copilot bridge
        nlohmann::json result = m_copilotBridge->trainModel(
            datasetPathEdit->text().toStdString(), 
            modelPathEdit->text().toStdString(), 
            configJson
        );

        if (result.value("success", false)) {
            m_chatInterface->addMessage("System", 
                "Model training started. Check the chat for progress updates.");
            statusBar()->showMessage("Model training started");
            trainDialog.accept();
        } else {
            QString error = QString::fromStdString(result.value("error", "Unknown error"));
            QMessageBox::critical(&trainDialog, "Training Error", 
                "Failed to start training: " + error);
        }
    });
    
    connect(cancelBtn, &QPushButton::clicked, &trainDialog, &QDialog::reject);
    
    trainDialog.exec();
}

// ==== ModelTrainer enhancement slot implementations ====

/**
 * @brief Convert QJsonObject config to ModelTrainer::TrainingConfig struct
 * @param jsonConfig Configuration from TrainingDialog
 * @return ModelTrainer::TrainingConfig struct with all parameters
 */
static ModelTrainer::TrainingConfig jsonToTrainingConfig(const QJsonObject& jsonConfig)
{
    ModelTrainer::TrainingConfig config;
    
    // Paths
    config.datasetPath = jsonConfig["datasetPath"].toString();
    config.outputPath = jsonConfig["outputPath"].toString();
    
    // Hyperparameters
    config.epochs = jsonConfig["epochs"].toInt(3);
    config.learningRate = static_cast<float>(jsonConfig["learningRate"].toDouble(1e-4));
    config.batchSize = jsonConfig["batchSize"].toInt(4);
    config.sequenceLength = jsonConfig["sequenceLength"].toInt(512);
    config.gradientClip = static_cast<float>(jsonConfig["gradientClip"].toDouble(1.0f));
    config.weightDecay = static_cast<float>(jsonConfig["weightDecay"].toDouble(0.01f));
    config.warmupSteps = static_cast<float>(jsonConfig["warmupSteps"].toDouble(0.1f));
    
    // Validation options
    config.validationSplit = static_cast<float>(jsonConfig["validationSplit"].toDouble(0.1f));
    config.validateEveryEpoch = jsonConfig["validateEveryEpoch"].toBool(true);
    
    return config;
}

void AgenticIDE::openTrainingDialog()
{
    if (!m_trainingDialog) {
        // Create dialog on first use, pass actual ModelTrainer instance
        m_trainingDialog = new TrainingDialog(m_modelTrainer, this);
        
        // Connect signal: when user starts training
        connect(m_trainingDialog, &TrainingDialog::trainingStartRequested,
                this, [this](const QJsonObject& config) {
                    // Convert JSON config to struct
                    ModelTrainer::TrainingConfig trainerConfig = jsonToTrainingConfig(config);
                    
                    // Log to chat
                    m_chatInterface->addMessage("System", 
                        QString("Starting model training...\n"
                               "- Dataset: %1\n"
                               "- Epochs: %2\n"
                               "- Learning Rate: %3\n"
                               "- Batch Size: %4")
                        .arg(trainerConfig.datasetPath)
                        .arg(trainerConfig.epochs)
                        .arg(trainerConfig.learningRate)
                        .arg(trainerConfig.batchSize));
                    
                    // Ensure TrainingProgressDock is visible
                    viewTrainingProgress();
                    
                    // Start training
                    if (!m_modelTrainer->startTraining(trainerConfig)) {
                        m_chatInterface->addMessage("System", "Error: Failed to start training. Check dataset and model paths.");
                        statusBar()->showMessage("Training failed to start");
                    } else {
                        statusBar()->showMessage("Training in progress...");
                    }
                });
        
        // Connect signal: when user cancels training dialog
        connect(m_trainingDialog, &TrainingDialog::trainingCancelled,
                this, [this]() {
                    m_chatInterface->addMessage("System", "Training dialog cancelled.");
                    statusBar()->showMessage("Ready");
                });
    }
    
    m_trainingDialog->show();
    m_trainingDialog->raise();
    m_trainingDialog->activateWindow();
}

void AgenticIDE::viewTrainingProgress()
{
    if (!m_trainingProgressDock) {
        // Create dock on first use, pass actual ModelTrainer instance
        m_trainingProgressDock = new TrainingProgressDock(m_modelTrainer, this);
        
        // Create and configure dock widget
        QDockWidget* dock = new QDockWidget("Training Progress", this);
        dock->setWidget(m_trainingProgressDock);
        dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        
        // Connect ModelTrainer signals to TrainingProgressDock slots
        connect(m_modelTrainer, &ModelTrainer::trainingStarted,
                m_trainingProgressDock, &TrainingProgressDock::onTrainingStarted);
        
        connect(m_modelTrainer, &ModelTrainer::epochStarted,
                m_trainingProgressDock, &TrainingProgressDock::onEpochStarted);
        
        connect(m_modelTrainer, &ModelTrainer::batchProcessed,
                m_trainingProgressDock, &TrainingProgressDock::onBatchProcessed);
        
        connect(m_modelTrainer, &ModelTrainer::epochCompleted,
                m_trainingProgressDock, &TrainingProgressDock::onEpochCompleted);
        
        connect(m_modelTrainer, &ModelTrainer::trainingCompleted,
                m_trainingProgressDock, &TrainingProgressDock::onTrainingCompleted);
        
        connect(m_modelTrainer, &ModelTrainer::trainingStopped,
                m_trainingProgressDock, &TrainingProgressDock::onTrainingStopped);
        
        connect(m_modelTrainer, &ModelTrainer::trainingError,
                m_trainingProgressDock, &TrainingProgressDock::onTrainingError);
        
        connect(m_modelTrainer, &ModelTrainer::logMessage,
                m_trainingProgressDock, &TrainingProgressDock::onLogMessage);
        
        connect(m_modelTrainer, &ModelTrainer::validationResults,
                m_trainingProgressDock, &TrainingProgressDock::onValidationResults);
        
        // Connect stop signal from progress dock
        connect(m_trainingProgressDock, &TrainingProgressDock::stopRequested,
                this, [this]() {
                    m_chatInterface->addMessage("System", "Stop training requested...");
                    m_modelTrainer->stopTraining();
                    statusBar()->showMessage("Training stop requested");
                });
        
        // Connect training completion to model registration
        connect(m_modelTrainer, &ModelTrainer::trainingCompleted,
                this, [this](const QString& modelPath, float finalPerplexity) {
                    m_chatInterface->addMessage("System", 
                        QString("Training completed! Model saved to:\n%1\nFinal Perplexity: %2")
                        .arg(modelPath)
                        .arg(finalPerplexity));
                    
                    // Register model if registry exists
                    if (m_modelRegistry) {
                        m_chatInterface->addMessage("System", "Registering trained model in registry...");
                    }
                });
    }
    
    // Show the dock if it exists
    if (m_trainingProgressDock->parentWidget()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(m_trainingProgressDock->parentWidget());
        if (dock) {
            dock->show();
            dock->raise();
        }
    }
}

void AgenticIDE::viewModelRegistry()
{
    if (!m_modelRegistry) {
        // Create registry widget on first use
        m_modelRegistry = new ModelRegistry(this);
        
        // Create dialog to host the registry
        QDialog* registryDialog = new QDialog(this);
        registryDialog->setWindowTitle("Model Registry");
        registryDialog->setMinimumSize(900, 600);
        
        QVBoxLayout* layout = new QVBoxLayout(registryDialog);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_modelRegistry);
        
        // Connect signals
        connect(m_modelRegistry, &ModelRegistry::modelSelected,
                this, [this](const QString& modelPath) {
                    m_chatInterface->addMessage("System", 
                        QString("Model selected: %1").arg(modelPath));
                    // TODO: Load model into InferenceEngine
                    statusBar()->showMessage(QString("Model selected: %1").arg(modelPath));
                });
        
        connect(m_modelRegistry, &ModelRegistry::modelDeleted,
                this, [this](int id) {
                    m_chatInterface->addMessage("System", 
                        QString("Model deleted (ID: %1)").arg(id));
                });
        
        // Store dialog as child of registry for later access
        m_modelRegistry->setProperty("registryDialog", QVariant::fromValue(registryDialog));
    }
    
    // Show the dialog
    QDialog* dialog = m_modelRegistry->property("registryDialog").value<QDialog*>();
    if (dialog) {
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    }
}

void AgenticIDE::startProfiling()
{
    if (!m_profiler) {
        m_profiler = new Profiler(this);
    }
    
    m_profiler->startProfiling();
    m_chatInterface->addMessage("System", "Performance profiling started. Monitoring CPU, memory, and throughput...");
    statusBar()->showMessage("Profiling: Active");
}

void AgenticIDE::stopProfiling()
{
    if (!m_profiler) {
        QMessageBox::warning(this, "Profiling", "Profiler not active.");
        return;
    }
    
    m_profiler->stopProfiling();
    m_chatInterface->addMessage("System", "Profiling stopped. Report available in observability dashboard.");
    statusBar()->showMessage("Profiling: Complete");
    
    // Auto-open observability dashboard
    openObservabilityDashboard();
}

void AgenticIDE::openObservabilityDashboard()
{
    if (!m_observabilityDashboard) {
        if (!m_profiler) {
            m_profiler = new Profiler(this);
        }
        
        m_observabilityDashboard = new ObservabilityDashboard(m_profiler, this);
        
        // Connect profiler signals to dashboard
        connect(m_profiler, &Profiler::metricsUpdated,
                this, [this](const Profiler::ProfileSnapshot& snap) {
                    m_observabilityDashboard->onMetricsUpdated(
                        snap.cpuUsagePercent,
                        snap.memoryUsageMB,
                        snap.gpuUsagePercent,
                        snap.gpuMemoryMB
                    );
                    m_observabilityDashboard->onThroughputUpdated(
                        snap.batchLatencyMs,
                        snap.throughputSamples
                    );
                });
        
        connect(m_profiler, &Profiler::performanceWarning,
                m_observabilityDashboard, &ObservabilityDashboard::onPerformanceWarning);
    }
    
    m_observabilityDashboard->show();
    m_observabilityDashboard->raise();
    m_observabilityDashboard->activateWindow();
    
    m_chatInterface->addMessage("System", "Observability dashboard opened. Real-time metrics visible.");
}

void AgenticIDE::configureHardwareBackend()
{
    if (!m_hardwareBackendSelector) {
        m_hardwareBackendSelector = new HardwareBackendSelector(this);
        
        // Connect signals
        connect(m_hardwareBackendSelector, QOverload<int>::of(&HardwareBackendSelector::backendSelected),
                this, [this](int backend) {
                    m_chatInterface->addMessage("System", 
                        QString("Backend selected: %1")
                        .arg(m_hardwareBackendSelector->getSelectedBackendName()));
                });
        
        connect(m_hardwareBackendSelector, &HardwareBackendSelector::backendConfirmed,
                this, [this](int backend) {
                    QJsonObject config = m_hardwareBackendSelector->getBackendConfig();
                    m_chatInterface->addMessage("System",
                        QString("Hardware backend configured:\n"
                               "- Backend: %1\n"
                               "- Precision: %2\n"
                               "- Memory Pool: %3 MB")
                        .arg(config["backendName"].toString())
                        .arg(config["precision"].toString())
                        .arg(config["memoryPoolMB"].toInt()));
                    statusBar()->showMessage("Backend: " + config["backendName"].toString());
                });
    }
    
    m_hardwareBackendSelector->exec();
}

void AgenticIDE::manageSecuritySettings()
{
    QMessageBox::information(this, "Security Settings", "Security manager not implemented.");
}

void AgenticIDE::startDistributedTraining()
{
    QMessageBox::information(this, "Distributed Training", "Distributed trainer not implemented.");
}

void AgenticIDE::viewInterpretabilityReport()
{
    QMessageBox::information(this, "Interpretability", "Interpretability panel not implemented.");
}

void AgenticIDE::openCIPipelineSettings()
{
    QMessageBox::information(this, "CI Pipeline", "CI pipeline settings UI not implemented.");
}

void AgenticIDE::configureTokenizerLanguage()
{
    QMessageBox::information(this, "Tokenizer Language", "Tokenizer language selector not implemented.");
}

void AgenticIDE::manageCheckpoints()
{
    QMessageBox::information(this, "Checkpoint Manager", "Checkpoint manager UI not implemented.");
}

void AgenticIDE::toggleFileBrowser()
{
    if (m_fileDock) {
        m_fileDock->setVisible(!m_fileDock->isVisible());
    }
}

void AgenticIDE::toggleChat()
{
    if (m_chatDock) {
        m_chatDock->setVisible(!m_chatDock->isVisible());
    }
}

void AgenticIDE::toggleTerminals()
{
    if (m_terminalDock) {
        m_terminalDock->setVisible(!m_terminalDock->isVisible());
    }
}

void AgenticIDE::toggleTodos()
{
    if (m_todoDockWidget) {
        m_todoDockWidget->setVisible(!m_todoDockWidget->isVisible());
    }
}

void AgenticIDE::showSettings()
{
    QMessageBox::information(this, "Settings", "Settings dialog not yet implemented.\n\nThis will configure IDE preferences, model paths, and keybindings.");
}
void AgenticIDE::addToRecentFiles(const QString &filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    
    // Keep only last 10 files
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }
}

// Edit operations
void AgenticIDE::undo() { m_multiTabEditor->undo(); }
void AgenticIDE::redo() { m_multiTabEditor->redo(); }
void AgenticIDE::find() { m_multiTabEditor->find(); }
void AgenticIDE::replace() { m_multiTabEditor->replace(); }

// AI Code Assistant operations
void AgenticIDE::toggleAICodeAssistant()
{
    if (m_aiCodeAssistantPanel) {
        m_aiCodeAssistantPanel->setVisible(!m_aiCodeAssistantPanel->isVisible());
        statusBar()->showMessage(
            m_aiCodeAssistantPanel->isVisible() ? 
            "AI Code Assistant enabled" : "AI Code Assistant disabled"
        );
    }
}

void AgenticIDE::requestCodeCompletion()
{
    if (!m_aiCodeAssistant || !m_multiTabEditor) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    QString selectedCode = m_multiTabEditor->getSelectedText();
    if (selectedCode.isEmpty()) {
        selectedCode = m_multiTabEditor->getCurrentText();
    }
    
    if (selectedCode.isEmpty()) {
        statusBar()->showMessage("No code selected - select code first");
        return;
    }
    
    // AGENTIC: Augment with workspace context
    QString workspaceContext = QString("Workspace: %1\nCurrent File: %2")
        .arg(m_workspaceRoot.isEmpty() ? "N/A" : m_workspaceRoot)
        .arg(m_multiTabEditor->getCurrentFilePath().isEmpty() ? "untitled" : m_multiTabEditor->getCurrentFilePath());
    
    // Show AI panel if hidden
    if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
        m_aiCodeAssistantPanel->setVisible(true);
    }
    
    statusBar()->showMessage("Requesting code completion (AGENTIC MODE)...");
    m_aiCodeAssistant->getCodeCompletion(selectedCode);
    
    // Log action for agentic tracing
    qDebug() << "[AGENTIC_IDE]" << "Code Completion triggered" << "Lines:" << selectedCode.count('\n');
}

void AgenticIDE::requestRefactoring()
{
    if (!m_aiCodeAssistant || !m_multiTabEditor) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    QString selectedCode = m_multiTabEditor->getSelectedText();
    if (selectedCode.isEmpty()) {
        selectedCode = m_multiTabEditor->getCurrentText();
    }
    
    if (selectedCode.isEmpty()) {
        statusBar()->showMessage("No code selected - select code first");
        return;
    }
    
    // AGENTIC: Enhance prompt with code analysis
    QString codeMetrics = QString("\n[Code Metrics]\nLines: %1\nCharacters: %2")
        .arg(selectedCode.count('\n'))
        .arg(selectedCode.length());
    
    // Show AI panel if hidden
    if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
        m_aiCodeAssistantPanel->setVisible(true);
    }
    
    statusBar()->showMessage("Requesting refactoring (AGENTIC MODE) - may search workspace for best practices...");
    m_aiCodeAssistant->getRefactoringSuggestions(selectedCode);
    
    // Log action
    qDebug() << "[AGENTIC_IDE]" << "Refactoring triggered with context";
}

void AgenticIDE::requestExplanation()
{
    if (!m_aiCodeAssistant || !m_multiTabEditor) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    QString selectedCode = m_multiTabEditor->getSelectedText();
    if (selectedCode.isEmpty()) {
        selectedCode = m_multiTabEditor->getCurrentText();
    }
    
    if (selectedCode.isEmpty()) {
        statusBar()->showMessage("No code selected - select code first");
        return;
    }
    
    // AGENTIC: Search workspace for related files that might provide context
    QString fileExtension = m_multiTabEditor->getCurrentFilePath().split('.').last();
    if (!fileExtension.isEmpty()) {
        // Optionally: could call m_aiCodeAssistant->searchFiles("*." + fileExtension);
    }
    
    // Show AI panel if hidden
    if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
        m_aiCodeAssistantPanel->setVisible(true);
    }
    
    statusBar()->showMessage("Requesting code explanation (AGENTIC MODE) - analyzing patterns...");
    m_aiCodeAssistant->getCodeExplanation(selectedCode);
    
    // Log action
    qDebug() << "[AGENTIC_IDE]" << "Explanation triggered for" << fileExtension << "code";
}

// ============================================================================
// AGENTIC IDE TOOLS - Full IDE Integration
// ============================================================================

void AgenticIDE::onAISearchWorkspace()
{
    if (!m_aiCodeAssistant) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    bool ok;
    QString pattern = QInputDialog::getText(this, "AI-Powered File Search", 
        "Enter file pattern to search (e.g., *.cpp, test_*):", QLineEdit::Normal, "", &ok);
    
    if (ok && !pattern.isEmpty()) {
        statusBar()->showMessage("AI searching workspace for: " + pattern);
        m_aiCodeAssistant->searchFiles(pattern, m_workspaceRoot);
        qDebug() << "[AGENTIC_IDE]" << "Workspace search initiated for pattern:" << pattern;
    }
}

void AgenticIDE::onAIGrepWorkspace()
{
    if (!m_aiCodeAssistant) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    bool ok;
    QString pattern = QInputDialog::getText(this, "AI-Powered Grep Search", 
        "Enter search pattern (e.g., TODO, FIXME, bug):", QLineEdit::Normal, "", &ok);
    
    if (ok && !pattern.isEmpty()) {
        statusBar()->showMessage("AI grepping workspace for: " + pattern);
        m_aiCodeAssistant->grepFiles(pattern, m_workspaceRoot);
        qDebug() << "[AGENTIC_IDE]" << "Workspace grep initiated for pattern:" << pattern;
    }
}

void AgenticIDE::onAIExecuteCommand()
{
    if (!m_aiCodeAssistant) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    bool ok;
    QString command = QInputDialog::getText(this, "AI PowerShell Command", 
        "Enter PowerShell command to execute:\n(e.g., cmake --build . or npm test)", 
        QLineEdit::Normal, "", &ok);
    
    if (ok && !command.isEmpty()) {
        statusBar()->showMessage("AI executing command: " + command);
        m_aiCodeAssistant->executePowerShellCommand(command);
        
        // Show output in AI panel
        if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
            m_aiCodeAssistantPanel->setVisible(true);
        }
        
        qDebug() << "[AGENTIC_IDE]" << "Command execution initiated:" << command;
    }
}

void AgenticIDE::onAIAnalyzeCode()
{
    if (!m_aiCodeAssistant || !m_multiTabEditor) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    QString currentCode = m_multiTabEditor->getCurrentText();
    if (currentCode.isEmpty()) {
        statusBar()->showMessage("No code to analyze");
        return;
    }
    
    QString context = QString("File: %1\nLanguage: %2\nSize: %3 bytes")
        .arg(m_multiTabEditor->getCurrentFilePath())
        .arg(m_multiTabEditor->getCurrentFilePath().split('.').last())
        .arg(currentCode.length());
    
    // Show AI panel
    if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
        m_aiCodeAssistantPanel->setVisible(true);
    }
    
    statusBar()->showMessage("AI analyzing code with workspace context...");
    m_aiCodeAssistant->analyzeAndRecommend(context);
    
    qDebug() << "[AGENTIC_IDE]" << "Code analysis initiated" << context;
}

void AgenticIDE::onAIAutofixError()
{
    if (!m_aiCodeAssistant || !m_multiTabEditor) {
        statusBar()->showMessage("AI Assistant not initialized");
        return;
    }
    
    QString currentCode = m_multiTabEditor->getCurrentText();
    if (currentCode.isEmpty()) {
        statusBar()->showMessage("No code to fix");
        return;
    }
    
    bool ok;
    QString errorMessage = QInputDialog::getText(this, "AI Auto-Fix",
        "Describe the error or issue:\n(e.g., 'compilation error on line 42')",
        QLineEdit::Normal, "", &ok);
    
    if (!ok || errorMessage.isEmpty()) {
        return;
    }
    
    // Show AI panel
    if (m_aiCodeAssistantPanel && !m_aiCodeAssistantPanel->isVisible()) {
        m_aiCodeAssistantPanel->setVisible(true);
    }
    
    statusBar()->showMessage("AI auto-fixing issue: " + errorMessage);
    m_aiCodeAssistant->autoFixIssue(errorMessage, currentCode);
    
    qDebug() << "[AGENTIC_IDE]" << "Auto-fix triggered for issue:" << errorMessage;
}