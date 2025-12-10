// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include "plan_orchestrator.h"
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
    
    // Model selector row
    QHBoxLayout* modelLayout = new QHBoxLayout();
    
    QLabel* modelLabel = new QLabel("Model 1:", this);
    modelLayout->addWidget(modelLabel);
    
    modelSelector_ = new QComboBox(this);
    modelSelector_->setMinimumWidth(200);
    modelSelector_->addItem("No Model Selected");
    //try {
    //    loadAvailableModels(); // Re-enabled scanner
    //} catch (const std::exception &e) {
    //    qWarning() << "[ChatInterface] loadAvailableModels threw exception:" << e.what();
    //} catch (...) {
    //    qWarning() << "[ChatInterface] loadAvailableModels threw unknown exception";
    //}
    connect(modelSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onModelChanged);
    modelLayout->addWidget(modelSelector_);
    
    // Second model selector for dual GGUF loading
    QLabel* model2Label = new QLabel("Model 2:", this);
    modelLayout->addWidget(model2Label);
    
    modelSelector2_ = new QComboBox(this);
    modelSelector2_->setMinimumWidth(200);
    modelSelector2_->addItem("No Model Selected");
    //try {
    //    loadAvailableModelsForSecond(); // Re-enabled scanner
    //} catch (const std::exception &e) {
    //    qWarning() << "[ChatInterface] loadAvailableModelsForSecond threw exception:" << e.what();
    //} catch (...) {
    //    qWarning() << "[ChatInterface] loadAvailableModelsForSecond threw unknown exception";
    //}
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
    layout->insertWidget(1, m_tokenProgress);  // Insert right after title
    
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
    
    // Initial state: Input enabled, prompt user to select model
    if (statusLabel_) {
        statusLabel_->setText("Select a model with matching .gguf file in D:/OllamaModels");
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

void ChatInterface::addMessage(const QString& sender, const QString& message) {
    QString color = (sender == "User") ? "#569cd6" : "#4ec9b0";
    message_history_->append("<span style='color:" + color + ";font-weight:bold;'>" + sender + ":</span> " + message);
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
    if (m_busy) return;  // Ignore while generating
    
    QString message = message_input_->text().trimmed();
    if (!message.isEmpty()) {
        m_busy = true;
        m_lastPrompt = message;
        
        // Enhance prompt with editor context if available
        QString enhancedMessage = message;
        
        // Try to get selected code from the editor
        // This requires a reference to the MultiTabEditor, which we may not have directly
        // For now, we'll emit the message as-is and the MainWindow or AgenticEngine
        // can augment it with context if needed
        
        addMessage("User", message);
        statusLabel_->setText("Processing...");
        
        // Check if this is an agent command
        if (isAgentCommand(message)) {
            executeAgentCommand(message);
            m_busy = false;
        } else {
            // Emit signal to process message (will be handled asynchronously)
            // The enhanced message with context will be prepared by the receiving engine
            emit messageSent(enhancedMessage);
        }
        
        message_input_->clear();
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
    else if (command.startsWith("/help") || command == "/?") {
        QString helpText = "<h3>Available Commands</h3>"
                          "<b>File Operations:</b><br>"
                          "  @grep &lt;pattern&gt; - Search for text in files<br>"
                          "  @read &lt;file&gt; - Read file contents<br>"
                          "  @search &lt;query&gt; - Find files<br>"
                          "  @ref &lt;symbol&gt; - Find symbol references<br><br>"
                          "<b>AI Operations:</b><br>"
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

