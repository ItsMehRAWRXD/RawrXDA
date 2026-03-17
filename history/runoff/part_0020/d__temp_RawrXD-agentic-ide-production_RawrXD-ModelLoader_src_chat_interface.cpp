// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include "plan_orchestrator.h"
#include "zero_day_agentic_engine.hpp"
#include "ui/agentic_browser.h"
#include "monitoring/enterprise_metrics_collector.hpp"
#include "qtapp/EnterpriseTelemetry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimer>
#include <QListWidget>
#include <QUrl>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent>
#include <QProcess>
#include <QRegularExpression>
#include <QProcess>

// Lightweight constructor - no widget creation
ChatInterface::ChatInterface(QWidget* parent) 
    : QWidget(parent)
    , maxMode_(false)
    , message_history_(nullptr)
    , message_input_(nullptr)
    , modelSelector_(nullptr)
    , modelSelector2_(nullptr)
    , maxModeToggle_(nullptr)
    , statusLabel_(nullptr)
{
    // Deferred to initialize() - safe to call before QApplication
}

// Two-phase init: Create Qt widgets after QApplication is running
void ChatInterface::initialize() {
    if (message_history_) return;  // Already initialized
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Header with title
    QLabel* title = new QLabel("Agent Chat", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    
    // === NEW: Workflow Breadcrumb Dropdown ===
    QHBoxLayout* breadcrumbLayout = new QHBoxLayout();
    QLabel* workflowLabel = new QLabel("Workflow:", this);
    workflowBreadcrumb_ = new QComboBox(this);
    workflowBreadcrumb_->setMinimumWidth(180);
    workflowBreadcrumb_->addItem("Agent", static_cast<int>(AgentWorkflowState::Agent));
    workflowBreadcrumb_->addItem("Ask", static_cast<int>(AgentWorkflowState::Ask));
    workflowBreadcrumb_->addItem("Plan", static_cast<int>(AgentWorkflowState::Plan));
    workflowBreadcrumb_->addItem("Edit", static_cast<int>(AgentWorkflowState::Edit));
    workflowBreadcrumb_->addItem("Configure Custom Agents", static_cast<int>(AgentWorkflowState::Configure));
    
    connect(workflowBreadcrumb_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onWorkflowStateChanged);
    
    breadcrumbLayout->addWidget(workflowLabel);
    breadcrumbLayout->addWidget(workflowBreadcrumb_);
    breadcrumbLayout->addStretch();
    layout->addLayout(breadcrumbLayout);
    
    // Model selector row
    QHBoxLayout* modelLayout = new QHBoxLayout();
    
    QLabel* modelLabel = new QLabel("Model 1:", this);
    modelLayout->addWidget(modelLabel);
    
    modelSelector_ = new QComboBox(this);
    modelSelector_->setMinimumWidth(200);
    modelSelector_->addItem("No Model Selected");
    connect(modelSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onModelChanged);
    modelLayout->addWidget(modelSelector_);
    
    // === NEW: Auto-selecting Model Dropdown ===
    QLabel* autoModelLabel = new QLabel("Auto Model:", this);
    modelLayout->addWidget(autoModelLabel);
    
    modelAutoSelector_ = new QComboBox(this);
    modelAutoSelector_->setMinimumWidth(200);
    modelAutoSelector_->addItem("Auto - Context");
    modelAutoSelector_->addItem("Auto - Agentic");
    modelAutoSelector_->addItem("Auto - Large");
    modelAutoSelector_->addItem("Auto - Code");
    modelAutoSelector_->setToolTip("Smart model selection based on task requirements");
    
    connect(modelAutoSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onAutoModelSelected);
    
    autoModelButton_ = new QPushButton("🤖 Auto-Select", this);
    autoModelButton_->setMaximumWidth(120);
    autoModelButton_->setToolTip("Automatically select best model for current workflow");
    connect(autoModelButton_, &QPushButton::clicked, this, &ChatInterface::onAutoModelSelected);
    
    modelLayout->addWidget(modelAutoSelector_);
    modelLayout->addWidget(autoModelButton_);
    
    // Second model selector for dual GGUF loading
    QLabel* model2Label = new QLabel("Model 2:", this);
    modelLayout->addWidget(model2Label);
    
    modelSelector2_ = new QComboBox(this);
    modelSelector2_->setMinimumWidth(200);
    modelSelector2_->addItem("No Model Selected");
    connect(modelSelector2_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onModel2Changed);
    modelLayout->addWidget(modelSelector2_);
    
    modelLayout->addStretch();
    
    // Max Mode toggle
    maxModeToggle_ = new QCheckBox("Max Mode", this);
    maxModeToggle_->setToolTip("Enable maximum context and response length");
    connect(maxModeToggle_, &QCheckBox::toggled, this, &ChatInterface::onMaxModeToggled);
    modelLayout->addWidget(maxModeToggle_);
    
    // Refresh models button
    QPushButton* refreshBtn = new QPushButton("🔄", this);
    refreshBtn->setMaximumWidth(30);
    refreshBtn->setToolTip("Refresh model list");
    connect(refreshBtn, &QPushButton::clicked, this, &ChatInterface::refreshModels);
    modelLayout->addWidget(refreshBtn);
    
    layout->addLayout(modelLayout);
    
    // Initialize model preferences for smart selection
    m_modelPreferences[AgentWorkflowState::Agent] = "llama3.2:3b";      // Default agent
    m_modelPreferences[AgentWorkflowState::Ask] = "llama3.2:3b";        // Clarification - smaller
    m_modelPreferences[AgentWorkflowState::Plan] = "llama3.2:8b";       // Planning - medium
    m_modelPreferences[AgentWorkflowState::Edit] = "dolphin3:8b";       // Code editing - specialized
    m_modelPreferences[AgentWorkflowState::Configure] = "llama3.2:8b";  // Configuration - medium
    
    // ==== PHASE 2: STREAMING TOKEN PROGRESS BAR ====
    m_tokenProgress = new QProgressBar(this);
    m_tokenProgress->setRange(0, 0);       // Busy indicator (indeterminate)
    m_tokenProgress->setTextVisible(false);
    m_tokenProgress->setFixedHeight(4);    // Slim bar
    m_tokenProgress->setStyleSheet(
        "QProgressBar { background: transparent; border: none; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #4ec9b0, stop:0.5 #569cd6, stop:1 #4ec9b0); }"
    );
    m_tokenProgress->hide();
    layout->insertWidget(2, m_tokenProgress);  // Insert after breadcrumb
    
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &ChatInterface::hideProgress);
    // ===============================================
    
    // Message history
    message_history_ = new QTextEdit(this);
    message_history_->setReadOnly(true);
    message_history_->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; }");
    layout->addWidget(message_history_);
    
    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    message_input_ = new QLineEdit(this);
    message_input_->setPlaceholderText("Type your message here...");
    message_input_->setStyleSheet(
        "QLineEdit { background-color: #252526; color: #d4d4d4; border: 1px solid #3c3c3c; padding: 5px; }");
    connect(message_input_, &QLineEdit::returnPressed, this, &ChatInterface::sendMessage);
    
    QPushButton* sendButton = new QPushButton("Send", this);
    sendButton->setStyleSheet(
        "QPushButton { background-color: #0e639c; color: white; padding: 5px 15px; border: none; }"
        "QPushButton:hover { background-color: #1177bb; }");
    connect(sendButton, &QPushButton::clicked, this, &ChatInterface::sendMessage);
    
    inputLayout->addWidget(message_input_);
    inputLayout->addWidget(sendButton);
    layout->addLayout(inputLayout);
    
    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    layout->addWidget(statusLabel_);
    
    // Connect messageReceived signal to add response message and reset busy state
    connect(this, &ChatInterface::messageReceived,
            this, [this](const QString& reply){ 
                addMessage("Bot", reply); 
                m_busy = false;
                statusLabel_->setText("Ready");
            });
    
    // Load available Ollama models on startup
    loadAvailableModels();
    loadAvailableModelsForSecond();
    
    // Wire up any existing autonomous components if available
    wireAgentSignals();
    
    // Initial state: Input enabled, prompt user to select model
    if (statusLabel_) {
        statusLabel_->setText("Select workflow and model, or use Auto-Select");
    }
}

void ChatInterface::loadAvailableModels() {
    // === DYNAMIC OLLAMA MODEL DETECTION ===
    // Queries 'ollama list' command directly for real-time model availability
    qDebug() << "[ChatInterface] Querying Ollama for available models...";
    
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", QStringList() << "list");
    
    if (!ollamaProcess.waitForStarted(3000)) {
        qWarning() << "[ChatInterface] Failed to start ollama command";
        statusLabel_->setText("Error: Cannot connect to Ollama. Is it installed?");
        return;
    }
    
    if (!ollamaProcess.waitForFinished(5000)) {
        qWarning() << "[ChatInterface] Ollama command timed out";
        statusLabel_->setText("Error: Ollama command timed out");
        ollamaProcess.kill();
        return;
    }
    
    QString output = QString::fromUtf8(ollamaProcess.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    int modelsFound = 0;
    for (int i = 1; i < lines.size(); ++i) {  // Skip header line
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        // Parse: "NAME                   ID              SIZE      MODIFIED"
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        QString modelName = parts[0];  // e.g., "llama3.2:3b" or "dolphin3:latest"
        
        // Store model name as both display and data
        modelSelector_->addItem(modelName, modelName);
        modelsFound++;
        
        qDebug() << "  [ChatInterface] Found model:" << modelName;
    }
    
    if (modelsFound == 0) {
        statusLabel_->setText("No Ollama models found. Run 'ollama pull <model>'");
        qDebug() << "[ChatInterface] No models detected from ollama list";
    } else {
        statusLabel_->setText(QString("Found %1 Ollama model(s)").arg(modelsFound));
        qDebug() << "[ChatInterface] Total Ollama models found:" << modelsFound;
    }
}

void ChatInterface::refreshModels() {
    QString currentModel = modelSelector_->currentData().toString();
    QString currentModel2 = modelSelector2_->currentData().toString();
    
    modelSelector_->clear();
    modelSelector_->addItem("No Model Selected");
    loadAvailableModels();
    
    modelSelector2_->clear();
    modelSelector2_->addItem("No Model Selected");
    loadAvailableModelsForSecond();
    
    // Try to restore previous selections
    int idx = modelSelector_->findData(currentModel);
    if (idx >= 0) {
        modelSelector_->setCurrentIndex(idx);
    }
    
    int idx2 = modelSelector2_->findData(currentModel2);
    if (idx2 >= 0) {
        modelSelector2_->setCurrentIndex(idx2);
    }
    
    statusLabel_->setText("Model list refreshed");
}

void ChatInterface::onModelChanged(int index) {
    if (index > 0) {
        QString modelName = modelSelector_->currentData().toString();
        QString displayName = modelSelector_->currentText();
        statusLabel_->setText("Selected: " + displayName + " - Resolving GGUF file...");
        
        // Resolve Ollama model name to actual GGUF file path
        QString ggufPath = resolveGgufPath(modelName);
        
        if (!ggufPath.isEmpty()) {
            qDebug() << "[ChatInterface::onModelChanged] Resolved" << modelName << "to" << ggufPath;
            statusLabel_->setText("Loading: " + displayName);
            emit modelSelected(ggufPath);  // Emit actual file path, not model name
        } else {
            qWarning() << "[ChatInterface::onModelChanged] Failed to resolve GGUF for" << modelName;
            statusLabel_->setText("❌ No GGUF file found for " + modelName + " in D:/OllamaModels");
            // Don't emit - user needs to fix path
        }
    } else {
        statusLabel_->setText("No model selected");
    }
}

void ChatInterface::onMaxModeToggled(bool enabled) {
    maxMode_ = enabled;
    if (enabled) {
        statusLabel_->setText("Max Mode enabled - Extended context and responses");
    } else {
        statusLabel_->setText("Standard mode");
    }
    emit maxModeChanged(enabled);
}

void ChatInterface::loadAvailableModelsForSecond() {
    // === DYNAMIC OLLAMA MODEL DETECTION (Model 2) ===
    qDebug() << "[ChatInterface] Querying Ollama for Model 2 options...";
    
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", QStringList() << "list");
    
    if (!ollamaProcess.waitForStarted(3000)) {
        qWarning() << "[ChatInterface] Failed to start ollama command for Model 2";
        return;
    }
    
    if (!ollamaProcess.waitForFinished(5000)) {
        qWarning() << "[ChatInterface] Ollama command timed out for Model 2";
        ollamaProcess.kill();
        return;
    }
    
    QString output = QString::fromUtf8(ollamaProcess.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (int i = 1; i < lines.size(); ++i) {  // Skip header line
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        QString modelName = parts[0];
        modelSelector2_->addItem(modelName, modelName);
    }
}

void ChatInterface::onModel2Changed(int index) {
    if (index > 0) {
        QString modelName = modelSelector2_->currentData().toString();
        QString displayName = modelSelector2_->currentText();
        statusLabel_->setText("Model 2 Selected: " + displayName + " - Resolving GGUF file...");
        
        // Resolve Ollama model name to actual GGUF file path
        QString ggufPath = resolveGgufPath(modelName);
        
        if (!ggufPath.isEmpty()) {
            qDebug() << "[ChatInterface::onModel2Changed] Resolved" << modelName << "to" << ggufPath;
            statusLabel_->setText("Loading Model 2: " + displayName);
            emit model2Selected(ggufPath);  // Emit actual file path, not model name
        } else {
            qWarning() << "[ChatInterface::onModel2Changed] Failed to resolve GGUF for" << modelName;
            statusLabel_->setText("❌ No GGUF file found for " + modelName + " in D:/OllamaModels");
            // Don't emit - user needs to fix path
        }
    } else {
        statusLabel_->setText("No secondary model selected");
    }
}

QString ChatInterface::selectedModel() const {
    return modelSelector_->currentData().toString();
}

bool ChatInterface::isMaxMode() const {
    return maxMode_;
}

void ChatInterface::onWorkflowStateChanged(int state) {
    m_workflowState = static_cast<AgentWorkflowState>(state);
    
    // Log workflow change to telemetry
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    QString stateStr = QString::number(static_cast<int>(m_workflowState));
    telemetry.recordEvent("agent.workflowChange", stateStr, workflowBreadcrumb_->currentText());
    
    // Update status
    statusLabel_->setText("Workflow changed to: " + workflowBreadcrumb_->currentText());
    
    // Emit signal for observers
    emit workflowStateChanged(m_workflowState);
}

QString ChatInterface::selectBestModelForTask(AgentWorkflowState task) {
    // Smart model selection based on workflow state and available models
    QString preferredModel = m_modelPreferences.value(task, "llama3.2:3b");
    
    // Try to find the preferred model in our list
    for (int i = 0; i < modelSelector_->count(); ++i) {
        QString modelData = modelSelector_->itemData(i).toString();
        if (modelData.contains(preferredModel, Qt::CaseInsensitive)) {
            return modelData;
        }
    }
    
    // Fallback: use task-specific heuristics
    switch (task) {
        case AgentWorkflowState::Ask:
            // Clarification - prefer smaller, faster models
            for (int i = 0; i < modelSelector_->count(); ++i) {
                QString model = modelSelector_->itemData(i).toString();
                if (model.contains("3b", Qt::CaseInsensitive)) return model;
            }
            break;
            
        case AgentWorkflowState::Plan:
        case AgentWorkflowState::Configure:
            // Planning - prefer medium-sized models
            for (int i = 0; i < modelSelector_->count(); ++i) {
                QString model = modelSelector_->itemData(i).toString();
                if (model.contains("8b", Qt::CaseInsensitive) || model.contains("7b", Qt::CaseInsensitive)) {
                    return model;
                }
            }
            break;
            
        case AgentWorkflowState::Edit:
            // Code editing - prefer specialized (Dolphin, etc) or larger models
            for (int i = 0; i < modelSelector_->count(); ++i) {
                QString model = modelSelector_->itemData(i).toString();
                if (model.contains("dolphin", Qt::CaseInsensitive)) return model;
                if (model.contains("13b", Qt::CaseInsensitive)) return model;
                if (model.contains("8b", Qt::CaseInsensitive)) return model;
            }
            break;
            
        case AgentWorkflowState::Agent:
        default:
            // Default - use any available model
            break;
    }
    
    // Final fallback: first available model that isn't "No Model Selected"
    for (int i = 1; i < modelSelector_->count(); ++i) {
        return modelSelector_->itemData(i).toString();
    }
    
    return "";  // No model available
}

void ChatInterface::onAutoModelSelected() {
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("agent.autoSelectModel"));
    
    QString autoMode = modelAutoSelector_->currentText();
    QString selectedModel;
    
    // Select model based on auto-mode selection
    if (autoMode.contains("Context")) {
        // Context analysis - use smaller model for efficiency
        selectedModel = selectBestModelForTask(AgentWorkflowState::Ask);
    } else if (autoMode.contains("Agentic")) {
        // Agentic reasoning - use larger model
        selectedModel = selectBestModelForTask(AgentWorkflowState::Plan);
    } else if (autoMode.contains("Large")) {
        // Large model - for complex tasks
        for (int i = modelSelector_->count() - 1; i >= 1; --i) {
            QString model = modelSelector_->itemData(i).toString();
            if (model.contains("13b", Qt::CaseInsensitive) || model.contains("70b", Qt::CaseInsensitive)) {
                selectedModel = model;
                break;
            }
        }
    } else if (autoMode.contains("Code")) {
        // Code-specialized models
        selectedModel = selectBestModelForTask(AgentWorkflowState::Edit);
    } else {
        // Default: use current workflow preference
        selectedModel = selectBestModelForTask(m_workflowState);
    }
    
    // Apply the selected model
    if (!selectedModel.isEmpty()) {
        int idx = modelSelector_->findData(selectedModel);
        if (idx >= 0) {
            modelSelector_->setCurrentIndex(idx);
            statusLabel_->setText("Auto-selected: " + selectedModel + " for " + autoMode);
            
            QString details = QString("mode=%1 model=%2 workflow=%3").arg(autoMode, selectedModel, workflowBreadcrumb_->currentText());
            telemetry.recordEvent("agent.autoModelSelected", selectedModel, details);
        }
    } else {
        statusLabel_->setText("No suitable model found for " + autoMode);
    }
}


void ChatInterface::addMessage(const QString& sender, const QString& message) {
    static const qint64 kMaxDisplayBytes = 2ll * 1024 * 1024 * 1024;
    QString color = (sender == "User") ? "#569cd6" : "#4ec9b0";
    auto sanitize = [](QString s) {
        s.replace("<unk>", "", Qt::CaseInsensitive);
        s.replace("[UNK]", "", Qt::CaseInsensitive);
        s.replace("<|unk|>", "", Qt::CaseInsensitive);
        return s;
    };
    QString safe = sanitize(message);
    const qint64 sz = safe.toUtf8().size();
    if (sz >= kMaxDisplayBytes) {
        message_history_->append("<span style='color:" + color + ";font-weight:bold;'>" + sender + ":</span> "
                                 + QString("[Content suppressed: %1 bytes exceeds 2GB display limit]").arg(sz));
        return;
    }
    message_history_->append("<span style='color:" + color + ";font-weight:bold;'>" + sender + ":</span> " + safe);
}

void ChatInterface::displayResponse(const QString& response) {
    addMessage("Agent", response);
    statusLabel_->setText("Ready");
    m_busy = false;  // Unblock input after response received
}

void ChatInterface::focusInput() {
    message_input_->setFocus();
}

void ChatInterface::sendMessage() {
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer(QStringLiteral("chat.sendMessage"));

    if (m_busy) {
        telemetry.recordEvent(QStringLiteral("chat"), QStringLiteral("sendMessage.skipped"), QStringLiteral("busy"));
        if (m_metrics) {
            m_metrics->recordCounter("chat.send_message_skipped_busy");
        }
        return;  // Ignore while generating
    }
    
    QString message = message_input_->text().trimmed();
    if (!message.isEmpty()) {
        telemetry.recordEvent(QStringLiteral("chat"), QStringLiteral("sendMessage.begin"), QStringLiteral("len=%1").arg(message.size()));
        m_busy = true;
        m_lastPrompt = message;
        m_commandHistory << message;
        
        // Enhance prompt with editor context if available
        QString enhancedMessage = message;
        enhanceMessageWithContext(enhancedMessage);
        
        addMessage("User", message);
        statusLabel_->setText("Processing...");
        
        // Check if this is an agent command
        if (isAgentCommand(message)) {
            executeAgentCommand(message);
            m_busy = false;
            telemetry.recordTiming(QStringLiteral("chat"), QStringLiteral("sendMessage.command"), timer.elapsedMs(), QStringLiteral("cmd=%1").arg(message.split(' ').first()));
            if (m_metrics) {
                m_metrics->recordHistogram("chat.command_latency_ms", timer.elapsedMs());
            }
        } else {
            // Emit signal to process message (will be handled asynchronously)
            emit messageSent(enhancedMessage);
            telemetry.recordTiming(QStringLiteral("chat"), QStringLiteral("sendMessage.dispatched"), timer.elapsedMs(), QStringLiteral("len=%1").arg(enhancedMessage.size()));
            if (m_metrics) {
                m_metrics->recordCounter("chat.message_sent");
                m_metrics->recordHistogram("chat.message_length", enhancedMessage.size());
            }
        }
        
        message_input_->clear();
    } else {
        telemetry.recordEvent(QStringLiteral("chat"), QStringLiteral("sendMessage.ignored"), QStringLiteral("empty"));
    }
}

void ChatInterface::sendMessageProgrammatically(const QString& message) {
    if (message_input_) {
        message_input_->setText(message);
        sendMessage();
    }
}

bool ChatInterface::isAgentCommand(const QString& message) const {
    // Commands start with @ or / or use known keywords
    return message.startsWith("@") || 
           message.startsWith("/") ||
           message.startsWith("grep ") ||
           message.startsWith("read ") ||
           message.startsWith("search ") ||
           message.startsWith("ref ");
}

void ChatInterface::executeAgentCommand(const QString& command, const QString& args) {
    Q_UNUSED(args);  // Currently using command parsing instead
    
    if (!m_agenticEngine) {
        addMessage("System", "Agentic Engine not initialized");
        statusLabel_->setText("Agent error: Engine not ready");
        return;
    }
    
    QString response;
    
    // Parse and execute agent commands
    if (command.startsWith("@grep ")) {
        QString pattern = command.mid(6).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@read ")) {
        QString filepath = command.mid(6).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@search ")) {
        QString query = command.mid(8).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@ref ")) {
        QString symbol = command.mid(5).trimmed();
        response = m_agenticEngine->referenceSymbol(symbol);
        addMessage("Agent", response);
    }
    else if (command.startsWith("grep ")) {
        // Support without @ prefix
        QString pattern = command.mid(5).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("read ")) {
        QString filepath = command.mid(5).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    }
    else if (command.startsWith("search ")) {
        QString query = command.mid(7).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("ref ")) {
        QString symbol = command.mid(4).trimmed();
        response = m_agenticEngine->referenceSymbol(symbol);
        addMessage("Agent", response);
    }
    else if (command.startsWith("/refactor ")) {
        // Multi-file AI refactoring command
        if (!m_planOrchestrator) {
            addMessage("System", "PlanOrchestrator not initialized");
            statusLabel_->setText("Refactor error: Orchestrator not ready");
            return;
        }
        
        QString prompt = command.mid(10).trimmed();  // Remove "/refactor "
        if (prompt.isEmpty()) {
            addMessage("System", "Usage: /refactor <description>\nExample: /refactor change UserManager to use UUID instead of int ID");
            return;
        }
        
        addMessage("System", "Planning multi-file refactor: " + prompt);
        statusLabel_->setText("Planning refactor...");
        
        // Execute multi-file refactor (synchronous for now)
        // TODO: Use current workspace root from project manager
        QString workspaceRoot = QDir::currentPath();
        RawrXD::ExecutionResult result = m_planOrchestrator->planAndExecute(prompt, workspaceRoot, false);
        
        if (result.success) {
            QString summary = QString("✓ Refactor complete: %1 files modified\n").arg(result.successCount);
            for (const QString& file : result.successfulFiles) {
                summary += "  • " + file + "\n";
            }
            addMessage("Agent", summary);
            statusLabel_->setText(QString("Refactored %1 files").arg(result.successCount));
        } else {
            QString errorMsg = QString("✗ Refactor failed: %1\n").arg(result.errorMessage);
            if (!result.failedFiles.isEmpty()) {
                errorMsg += "Failed files:\n";
                for (const QString& file : result.failedFiles) {
                    errorMsg += "  • " + file + "\n";
                }
            }
            addMessage("System", errorMsg);
            statusLabel_->setText("Refactor failed");
        }
    }
    else if (command.startsWith("/plan ")) {
        // Create implementation plan
        if (!m_planOrchestrator) {
            addMessage("System", "PlanOrchestrator not initialized");
            statusLabel_->setText("Plan error: Orchestrator not ready");
            return;
        }
        
        QString task = command.mid(6).trimmed();  // Remove "/plan "
        if (task.isEmpty()) {
            addMessage("System", "Usage: /plan <task description>\nExample: /plan implement unit tests for UserManager.cpp");
            return;
        }
        
        addMessage("System", "📋 Creating implementation plan: " + task);
        statusLabel_->setText("Creating plan...");
        
        // Create plan using PlanOrchestrator
        QString workspaceRoot = QDir::currentPath();
        RawrXD::PlanningResult plan = m_planOrchestrator->generatePlan(task, workspaceRoot);
        
        if (plan.tasks.isEmpty()) {
            addMessage("System", "⚠ Could not generate plan for this task");
            statusLabel_->setText("Plan generation failed");
        } else {
            QString planSummary = QString("✓ Plan created: %1 tasks\n\n").arg(plan.tasks.size());
            for (int i = 0; i < plan.tasks.size(); ++i) {
                planSummary += QString("%1. %2\n").arg(i + 1).arg(plan.tasks[i].description);
            }
            planSummary += "\nUse /execute to run the plan";
            addMessage("Agent", planSummary);
            statusLabel_->setText(QString("Plan ready: %1 tasks").arg(plan.tasks.size()));
        }
    }
    else if (command.startsWith("/mission ")) {
        // Zero-Day Agentic Engine autonomous mission
        if (!m_zeroDayAgent) {
            addMessage("System", "Zero-Day Agent not initialized");
            statusLabel_->setText("Mission error: Agent not ready");
            return;
        }
        
        QString goal = command.mid(9).trimmed();  // Remove "/mission "
        if (goal.isEmpty()) {
            addMessage("System", "Usage: /mission <goal>\nExample: /mission List all files in the current directory");
            return;
        }
        
        startAutonomousMission(goal);
    }
    else if (command == "/abort") {
        // Abort current mission
        abortAutonomousMission();
    }
    else if (command.startsWith("/help") || command == "/?") {
        QString helpText = "<h3>Available Commands</h3>"
                          "<b>File Operations:</b><br>"
                          "  @grep &lt;pattern&gt; - Search for text in files<br>"
                          "  @read &lt;file&gt; - Read file contents<br>"
                          "  @search &lt;query&gt; - Find files<br>"
                          "  @ref &lt;symbol&gt; - Find symbol references<br><br>"
                          "<b>AI Operations:</b><br>"
                          "  /mission &lt;goal&gt; - Autonomous Zero-Day agent mission<br>"
                          "  /refactor &lt;description&gt; - Multi-file refactoring<br>"
                          "  /plan &lt;task&gt; - Create implementation plan<br>"
                          "  /help - Show this help<br><br>"
                          "<b>Tips:</b><br>"
                          "  • Select code in editor before chatting for context<br>"
                          "  • Use AI → Inference Settings to tune model behavior<br>";
        addMessage("System", helpText);
    }
    else {
        response = "Unknown command. Type /help for available commands.";
        addMessage("System", response);
    }
    
    statusLabel_->setText("Agent command executed");
}

// ==== PHASE 2: STREAMING TOKEN PROGRESS IMPLEMENTATION ====
void ChatInterface::onTokenGenerated(int delta)
{
    Q_UNUSED(delta);
    if (m_tokenProgress->isHidden()) {
        m_tokenProgress->show();
        m_hideTimer->stop();
        qDebug() << "[ChatInterface] Token progress bar shown - generation started";
    }
    // Restart hide timer - will hide 1s after last token
    m_hideTimer->start(1000);
}

void ChatInterface::hideProgress()
{
    m_tokenProgress->hide();
    qDebug() << "[ChatInterface] Token progress bar hidden - generation complete";
}
// ========================================================

void ChatInterface::setCanSendMessage(bool enabled) {
    qDebug() << "[ChatInterface] ═══ setCanSendMessage(" << enabled << ") called ═══";
    if (message_input_) {
        // Always enable input so user can try selecting another model
        message_input_->setEnabled(true);
        qDebug() << "[ChatInterface] Message input ENABLED";
    }
    if (statusLabel_) {
        QString statusText;
        if (enabled) {
            statusText = "✓ Ready - Model loaded";
        } else {
            statusText = "⚠ No GGUF file found - Select a model with matching .gguf in D:/OllamaModels";
        }
        statusLabel_->setText(statusText);
        qDebug() << "[ChatInterface] Status set to:" << statusText;
    }
}

QString ChatInterface::resolveGgufPath(const QString& modelName)
{
    qDebug() << "[ChatInterface::resolveGgufPath] Resolving model:" << modelName;
    
    // First, try Ollama's 'show' command to get model information
    // This is more reliable than trying to find files manually
    QProcess ollamaShow;
    ollamaShow.start("ollama", QStringList() << "show" << modelName);
    
    if (ollamaShow.waitForStarted(2000)) {
        if (ollamaShow.waitForFinished(3000)) {
            QString output = QString::fromUtf8(ollamaShow.readAllStandardOutput());
            qDebug() << "[ChatInterface::resolveGgufPath] Ollama show output:\n" << output;
            
            // Ollama models are stored in blobs, but they're accessible through Ollama API
            // For now, we'll fall through to file search below, but at least we know the model exists
            if (!output.isEmpty()) {
                qDebug() << "[ChatInterface::resolveGgufPath] Model exists in Ollama - will use Ollama runner";
            }
        }
    }
    
    // Search for GGUF file in multiple locations
    QStringList searchPaths = {
        // User's custom models directory
        "D:/OllamaModels",
        // Windows Ollama models
        "C:/Users/" + qEnvironmentVariable("USERNAME") + "/.ollama/models",
        QDir::homePath() + "/.ollama/models",
        // Alternative locations
        "C:/Ollama/models",
        QDir::homePath() + "/.cache/ollama",
    };
    
    // Extract base model name (e.g., "llama3.2" from "llama3.2:3b" or "unlocked-350M" from "unlocked-350M:latest")
    QString baseName = modelName.split(':').first();
    QString searchPattern = "*" + baseName + "*.gguf";
    
    qDebug() << "[ChatInterface::resolveGgufPath] Looking for pattern:" << searchPattern;
    
    // First, try direct GGUF search in standard paths
    for (const QString& searchPath : searchPaths) {
        QDir dir(searchPath);
        if (!dir.exists()) {
            qDebug() << "  [ChatInterface::resolveGgufPath] Skipped (not found):" << searchPath;
            continue;
        }
        
        // Search for matching GGUF files
        QStringList filters;
        filters << searchPattern;
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        
        if (!files.isEmpty()) {
            // Return first matching GGUF file
            QString path = files.first().absoluteFilePath();
            qDebug() << "  [ChatInterface::resolveGgufPath] ✓ Found:" << path;
            return path;
        }
        
        qDebug() << "  [ChatInterface::resolveGgufPath] No matches in:" << searchPath;
    }
    
    // Second attempt: Search recursively in Ollama blobs (for Ollama-managed models)
    // Ollama stores models in ~/.ollama/blobs/blobs/ as sha256-* files without .gguf extension
    QStringList blobsPaths = {
        QDir::homePath() + "/.ollama/blobs/blobs",  // Windows/Linux actual location
        QDir::homePath() + "/.ollama/models/blobs",  // Alternative location
    };
    
    for (const QString& ollamaBlobs : blobsPaths) {
        QDir blobDir(ollamaBlobs);
        if (!blobDir.exists()) {
            qDebug() << "  [ChatInterface::resolveGgufPath] Skipped (not found):" << ollamaBlobs;
            continue;
        }
        
        qDebug() << "  [ChatInterface::resolveGgufPath] Searching Ollama blobs:" << ollamaBlobs;
        
        // Ollama blobs are named sha256-<hash> without extension
        // Look for large files (>100MB) which are likely model weights
        QFileInfoList blobFiles = blobDir.entryInfoList(QStringList() << "sha256-*", 
                                                        QDir::Files | QDir::Hidden, 
                                                        QDir::Name);
        
        // Sort by size descending and pick the largest file (likely the model)
        QList<QPair<qint64, QString>> sizedFiles;
        for (const QFileInfo& file : blobFiles) {
            qint64 sizeMB = file.size() / (1024 * 1024);
            if (sizeMB > 100) {  // Only consider files > 100MB
                sizedFiles.append(qMakePair(file.size(), file.absoluteFilePath()));
                qDebug() << "    Found blob:" << file.fileName() << "Size:" << sizeMB << "MB";
            }
        }
        
        if (!sizedFiles.isEmpty()) {
            // Sort by size and return largest (most recent/primary model)
            std::sort(sizedFiles.begin(), sizedFiles.end(), 
                     [](const QPair<qint64, QString>& a, const QPair<qint64, QString>& b) {
                         return a.first > b.first;  // Descending
                     });
            
            QString path = sizedFiles.first().second;
            qint64 sizeMB = sizedFiles.first().first / (1024 * 1024);
            
            // Verify file is readable
            QFile testFile(path);
            if (!testFile.exists()) {
                qWarning() << "  [ChatInterface::resolveGgufPath] File doesn't exist:" << path;
                continue;  // Try next blobs directory
            }
            
            if (!testFile.open(QIODevice::ReadOnly)) {
                qWarning() << "  [ChatInterface::resolveGgufPath] Cannot open file:" << path;
                continue;  // Try next blobs directory
            }
            
            // Verify it's a valid GGUF file by checking magic bytes
            QByteArray magic = testFile.read(4);
            testFile.close();
            
            if (magic.size() == 4 && magic[0] == 'G' && magic[1] == 'G' && 
                magic[2] == 'U' && magic[3] == 'F') {
                qDebug() << "  [ChatInterface::resolveGgufPath] ✓ Valid GGUF found in Ollama blobs:" << path << "(" << sizeMB << "MB)";
                return path;
            } else {
                qWarning() << "  [ChatInterface::resolveGgufPath] File is not a valid GGUF (bad magic bytes):" << path;
                continue;  // Try next file
            }
        }
    }
    
    // If model exists in Ollama but we can't find GGUF file, suggest using Ollama directly
    qWarning() << "[ChatInterface::resolveGgufPath] ✗ No local GGUF file found for" << modelName;
    qWarning() << "  Model may be stored in Ollama's blob directory (.ollama/models/blobs)";
    qWarning() << "  Consider: 1) Moving model to D:/OllamaModels, or";
    qWarning() << "           2) Exporting from Ollama and saving as .gguf file";
    return QString();
}

    // Remove duplicate implementation
    // void ChatInterface::onWorkflowStateChanged(int state)
    // {
    //     Q_UNUSED(state);
    //     // Placeholder implementation - workflow state change handling
    // }
    
    // void ChatInterface::onAutoModelSelected()
    // {
    //     // Placeholder implementation - auto model selection
    // }

// ============================================================
// AUTONOMOUS & AGENTIC FEATURES WIRING
// ============================================================

void ChatInterface::wireAgentSignals() {
    if (!m_zeroDayAgent) return;
    
    // Connect ZeroDayAgenticEngine signals to chat display
    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentStream,
            this, &ChatInterface::onAgentStreamToken);
    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentComplete,
            this, &ChatInterface::onAgentComplete);
    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentError,
            this, &ChatInterface::onAgentError);
    
    qDebug() << "[ChatInterface::wireAgentSignals] Zero-Day Agent signals wired";
}

void ChatInterface::onAgentStreamToken(const QString& token) {
    if (m_missionActive) {
        addMessage("🤖 Agent", token);
        if (m_metrics) {
            m_metrics->recordCounter("chat.agent_stream_token");
        }
    }
}

void ChatInterface::onAgentComplete(const QString& summary) {
    m_missionActive = false;
    addMessage("✓ Mission Complete", summary);
    statusLabel_->setText("Autonomous mission completed");
    if (m_metrics) {
        m_metrics->recordCounter("chat.agent_mission_complete");
    }
}

void ChatInterface::onAgentError(const QString& error) {
    m_missionActive = false;
    addMessage("⚠ Agent Error", error);
    statusLabel_->setText("Agent error: " + error);
    if (m_metrics) {
        m_metrics->recordCounter("chat.agent_error");
    }
}

void ChatInterface::onBrowserNavigated(const QUrl& url, bool success, int httpStatus) {
    QString msg = success ? 
        QString("✓ Browser: %1 (%2)").arg(url.host()).arg(httpStatus) :
        QString("✗ Browser: Failed to load %1").arg(url.toString());
    addMessage("🌐 Browser", msg);
    if (m_metrics) {
        m_metrics->recordMetric("chat.browser_nav_status", httpStatus);
    }
}

void ChatInterface::startAutonomousMission(const QString& goal) {
    if (m_missionActive) {
        addMessage("System", "Mission already in progress. Use /abort to stop.");
        return;
    }
    
    if (!m_zeroDayAgent) {
        addMessage("System", "Zero-Day Agent not initialized");
        return;
    }
    
    m_missionActive = true;
    m_currentMissionGoal = goal;
    statusLabel_->setText("🚀 Mission started: " + goal);
    addMessage("System", "🚀 Starting autonomous mission...");
    
    m_zeroDayAgent->startMission(goal);
    if (m_metrics) {
        m_metrics->recordCounter("chat.agent_mission_started");
    }
}

void ChatInterface::abortAutonomousMission() {
    if (!m_missionActive) {
        addMessage("System", "No mission currently running");
        return;
    }
    
    if (m_zeroDayAgent) {
        m_zeroDayAgent->abortMission();
    }
    
    m_missionActive = false;
    addMessage("System", "❌ Mission aborted");
    statusLabel_->setText("Mission aborted");
    if (m_metrics) {
        m_metrics->recordCounter("chat.agent_mission_aborted");
    }
}

QStringList ChatInterface::getAvailableCommands() const {
    return QStringList {
        "@grep <pattern>",
        "@read <file>",
        "@search <query>",
        "@ref <symbol>",
        "/mission <goal>",
        "/refactor <description>",
        "/plan <task>",
        "/abort",
        "/help"
    };
}

QStringList ChatInterface::getSuggestedCommands() const {
    QStringList suggestions;
    
    switch (m_workflowState) {
        case AgentWorkflowState::Ask:
            suggestions << "@grep" << "@search" << "@read" << "/help";
            break;
        case AgentWorkflowState::Plan:
            suggestions << "/plan" << "/mission" << "@ref" << "/help";
            break;
        case AgentWorkflowState::Edit:
            suggestions << "/refactor" << "@grep" << "@read" << "/help";
            break;
        case AgentWorkflowState::Agent:
            suggestions << "/mission" << "@grep" << "@search" << "/plan";
            break;
        case AgentWorkflowState::Configure:
            suggestions << "/mission" << "/plan" << "/refactor" << "/help";
            break;
    }
    
    return suggestions;
}

void ChatInterface::updateCommandSuggestions(const QString& filter) {
    if (!m_commandSuggestions) return;
    
    m_commandSuggestions->clear();
    auto suggested = getSuggestedCommands();
    
    for (const QString& cmd : suggested) {
        if (filter.isEmpty() || cmd.contains(filter, Qt::CaseInsensitive)) {
            m_commandSuggestions->addItem(cmd);
        }
    }
}

void ChatInterface::enhanceMessageWithContext(QString& message) {
    // This would augment the message with editor context
    // For now, it's a placeholder for future editor integration
    Q_UNUSED(message);
}