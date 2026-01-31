// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include "plan_orchestrator.h"
#include "zero_day_agentic_engine.hpp"
#include "qtapp/EnterpriseTelemetry.h"


// Lightweight constructor - no widget creation
ChatInterface::ChatInterface(void* parent) 
    : void(parent)
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
    //} catch (...) {
    //}
// Qt connect removed
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
    //} catch (...) {
    //}
// Qt connect removed
    modelLayout->addWidget(modelSelector2_);
    
    modelLayout->addStretch();
    
    // Max Mode toggle
    maxModeToggle_ = new QCheckBox("Max Mode", this);
    maxModeToggle_->setToolTip("Enable maximum context and response length");
// Qt connect removed
    modelLayout->addWidget(maxModeToggle_);
    
    // Refresh models button
    QPushButton* refreshBtn = new QPushButton("🔄", this);
    refreshBtn->setMaximumWidth(30);
    refreshBtn->setToolTip("Refresh model list");
// Qt connect removed
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
    
    m_hideTimer = new void*(this);
    m_hideTimer->setSingleShot(true);
// Qt connect removed
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
// Qt connect removed
    QPushButton* sendButton = new QPushButton("Send", this);
    sendButton->setStyleSheet(
        "QPushButton { background-color: #0e639c; color: white; padding: 5px 15px; border: none; }"
        "QPushButton:hover { background-color: #1177bb; }");
// Qt connect removed
    inputLayout->addWidget(message_input_);
    inputLayout->addWidget(sendButton);
    layout->addLayout(inputLayout);
    
    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    layout->addWidget(statusLabel_);
    
    // Connect messageReceived signal to add response message and reset busy state
// Qt connect removed
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
    
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", std::vector<std::string>() << "list");
    
    if (!ollamaProcess.waitForStarted(3000)) {
        statusLabel_->setText("Error: Cannot connect to Ollama. Is it installed?");
        return;
    }
    
    if (!ollamaProcess.waitForFinished(5000)) {
        statusLabel_->setText("Error: Ollama command timed out");
        ollamaProcess.kill();
        return;
    }
    
    std::string output = std::string::fromUtf8(ollamaProcess.readAllStandardOutput());
    std::vector<std::string> lines = output.split('\n', //SkipEmptyParts);
    
    int modelsFound = 0;
    for (int i = 1; i < lines.size(); ++i) {  // Skip header line
        std::string line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        // Parse: "NAME                   ID              SIZE      MODIFIED"
        std::vector<std::string> parts = line.split(std::regex("\\s+"), //SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        std::string modelName = parts[0];  // e.g., "llama3.2:3b" or "dolphin3:latest"
        
        // Store model name as both display and data
        modelSelector_->addItem(modelName, modelName);
        modelsFound++;
        
    }
    
    if (modelsFound == 0) {
        statusLabel_->setText("No Ollama models found. Run 'ollama pull <model>'");
    } else {
        statusLabel_->setText(std::string("Found %1 Ollama model(s)"));
    }
}

void ChatInterface::refreshModels() {
    std::string currentModel = modelSelector_->currentData().toString();
    std::string currentModel2 = modelSelector2_->currentData().toString();
    
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
        std::string modelName = modelSelector_->currentData().toString();
        std::string displayName = modelSelector_->currentText();
        statusLabel_->setText("Selected: " + displayName + " - Resolving GGUF file...");
        
        // Resolve Ollama model name to actual GGUF file path
        std::string ggufPath = resolveGgufPath(modelName);
        
        if (!ggufPath.isEmpty()) {
            statusLabel_->setText("Loading: " + displayName);
            modelSelected(ggufPath);  // actual file path, not model name
        } else {
            statusLabel_->setText("❌ No GGUF file found for " + modelName + " in D:/OllamaModels");
            // Don't - user needs to fix path
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
    maxModeChanged(enabled);
}

void ChatInterface::loadAvailableModelsForSecond() {
    // === DYNAMIC OLLAMA MODEL DETECTION (Model 2) ===
    
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", std::vector<std::string>() << "list");
    
    if (!ollamaProcess.waitForStarted(3000)) {
        return;
    }
    
    if (!ollamaProcess.waitForFinished(5000)) {
        ollamaProcess.kill();
        return;
    }
    
    std::string output = std::string::fromUtf8(ollamaProcess.readAllStandardOutput());
    std::vector<std::string> lines = output.split('\n', //SkipEmptyParts);
    
    for (int i = 1; i < lines.size(); ++i) {  // Skip header line
        std::string line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        std::vector<std::string> parts = line.split(std::regex("\\s+"), //SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        std::string modelName = parts[0];
        modelSelector2_->addItem(modelName, modelName);
    }
}

void ChatInterface::onModel2Changed(int index) {
    if (index > 0) {
        std::string modelName = modelSelector2_->currentData().toString();
        std::string displayName = modelSelector2_->currentText();
        statusLabel_->setText("Model 2 Selected: " + displayName + " - Resolving GGUF file...");
        
        // Resolve Ollama model name to actual GGUF file path
        std::string ggufPath = resolveGgufPath(modelName);
        
        if (!ggufPath.isEmpty()) {
            statusLabel_->setText("Loading Model 2: " + displayName);
            model2Selected(ggufPath);  // actual file path, not model name
        } else {
            statusLabel_->setText("❌ No GGUF file found for " + modelName + " in D:/OllamaModels");
            // Don't - user needs to fix path
        }
    } else {
        statusLabel_->setText("No secondary model selected");
    }
}

std::string ChatInterface::selectedModel() const {
    return modelSelector_->currentData().toString();
}

bool ChatInterface::isMaxMode() const {
    return maxMode_;
}

void ChatInterface::addMessage(const std::string& sender, const std::string& message) {
    std::string color = (sender == "User") ? "#569cd6" : "#4ec9b0";
    message_history_->append("<span style='color:" + color + ";font-weight:bold;'>" + sender + ":</span> " + message);
}

void ChatInterface::displayResponse(const std::string& response) {
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
        return;  // Ignore while generating
    }
    
    std::string message = message_input_->text().trimmed();
    if (!message.isEmpty()) {
        telemetry.recordEvent(QStringLiteral("chat"), QStringLiteral("sendMessage.begin"), QStringLiteral("len=%1")));
        m_busy = true;
        m_lastPrompt = message;
        
        // Enhance prompt with editor context if available
        std::string enhancedMessage = message;
        
        // Try to get selected code from the editor
        // This requires a reference to the MultiTabEditor, which we may not have directly
        // For now, we'll the message as-is and the MainWindow or AgenticEngine
        // can augment it with context if needed
        
        addMessage("User", message);
        statusLabel_->setText("Processing...");
        
        // Check if this is an agent command
        if (isAgentCommand(message)) {
            executeAgentCommand(message);
            m_busy = false;
            telemetry.recordTiming(QStringLiteral("chat"), QStringLiteral("sendMessage.command"), timer.elapsedMs(), QStringLiteral("cmd=%1").first()));
        } else {
            // signal to process message (will be handled asynchronously)
            // The enhanced message with context will be prepared by the receiving engine
            messageSent(enhancedMessage);
            telemetry.recordTiming(QStringLiteral("chat"), QStringLiteral("sendMessage.dispatched"), timer.elapsedMs(), QStringLiteral("len=%1")));
        }
        
        message_input_->clear();
    } else {
        telemetry.recordEvent(QStringLiteral("chat"), QStringLiteral("sendMessage.ignored"), QStringLiteral("empty"));
    }
}

void ChatInterface::sendMessageProgrammatically(const std::string& message) {
    if (message_input_) {
        message_input_->setText(message);
        sendMessage();
    }
}

bool ChatInterface::isAgentCommand(const std::string& message) const {
    // Commands start with @ or / or use known keywords
    return message.startsWith("@") || 
           message.startsWith("/") ||
           message.startsWith("grep ") ||
           message.startsWith("read ") ||
           message.startsWith("search ") ||
           message.startsWith("ref ");
}

void ChatInterface::executeAgentCommand(const std::string& command, const std::string& args) {
    (args);  // Currently using command parsing instead
    
    if (!m_agenticEngine) {
        addMessage("System", "Agentic Engine not initialized");
        statusLabel_->setText("Agent error: Engine not ready");
        return;
    }
    
    std::string response;
    
    // Parse and execute agent commands
    if (command.startsWith("@grep ")) {
        std::string pattern = command.mid(6).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@read ")) {
        std::string filepath = command.mid(6).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@search ")) {
        std::string query = command.mid(8).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@ref ")) {
        std::string symbol = command.mid(5).trimmed();
        response = m_agenticEngine->referenceSymbol(symbol);
        addMessage("Agent", response);
    }
    else if (command.startsWith("grep ")) {
        // Support without @ prefix
        std::string pattern = command.mid(5).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("read ")) {
        std::string filepath = command.mid(5).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    }
    else if (command.startsWith("search ")) {
        std::string query = command.mid(7).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("ref ")) {
        std::string symbol = command.mid(4).trimmed();
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
        
        std::string prompt = command.mid(10).trimmed();  // Remove "/refactor "
        if (prompt.isEmpty()) {
            addMessage("System", "Usage: /refactor <description>\nExample: /refactor change UserManager to use UUID instead of int ID");
            return;
        }
        
        addMessage("System", "Planning multi-file refactor: " + prompt);
        statusLabel_->setText("Planning refactor...");
        
        // Execute multi-file refactor (synchronous for now)
        // TODO: Use current workspace root from project manager
        std::string workspaceRoot = std::filesystem::path::currentPath();
        RawrXD::ExecutionResult result = m_planOrchestrator->planAndExecute(prompt, workspaceRoot, false);
        
        if (result.success) {
            std::string summary = std::string("✓ Refactor complete: %1 files modified\n");
            for (const std::string& file : result.successfulFiles) {
                summary += "  • " + file + "\n";
            }
            addMessage("Agent", summary);
            statusLabel_->setText(std::string("Refactored %1 files"));
        } else {
            std::string errorMsg = std::string("✗ Refactor failed: %1\n");
            if (!result.failedFiles.isEmpty()) {
                errorMsg += "Failed files:\n";
                for (const std::string& file : result.failedFiles) {
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
        
        std::string task = command.mid(6).trimmed();  // Remove "/plan "
        if (task.isEmpty()) {
            addMessage("System", "Usage: /plan <task description>\nExample: /plan implement unit tests for UserManager.cpp");
            return;
        }
        
        addMessage("System", "📋 Creating implementation plan: " + task);
        statusLabel_->setText("Creating plan...");
        
        // Create plan using PlanOrchestrator
        std::string workspaceRoot = std::filesystem::path::currentPath();
        RawrXD::PlanningResult plan = m_planOrchestrator->generatePlan(task, workspaceRoot);
        
        if (plan.tasks.isEmpty()) {
            addMessage("System", "⚠ Could not generate plan for this task");
            statusLabel_->setText("Plan generation failed");
        } else {
            std::string planSummary = std::string("✓ Plan created: %1 tasks\n\n"));
            for (int i = 0; i < plan.tasks.size(); ++i) {
                planSummary += std::string("%1. %2\n");
            }
            planSummary += "\nUse /execute to run the plan";
            addMessage("Agent", planSummary);
            statusLabel_->setText(std::string("Plan ready: %1 tasks")));
        }
    }
    else if (command.startsWith("/mission ")) {
        // Zero-Day Agentic Engine autonomous mission
        if (!m_zeroDayAgent) {
            addMessage("System", "Zero-Day Agent not initialized");
            statusLabel_->setText("Mission error: Agent not ready");
            return;
        }
        
        std::string goal = command.mid(9).trimmed();  // Remove "/mission "
        if (goal.isEmpty()) {
            addMessage("System", "Usage: /mission <goal>\nExample: /mission List all files in the current directory");
            return;
        }
        
        addMessage("User", command);
        addMessage("System", "🚀 Starting autonomous mission: " + goal);
        statusLabel_->setText("Mission in progress...");
        
        // Launch Zero-Day agent (async execution via QtConcurrent)
        m_zeroDayAgent->startMission(goal);
        // Results will stream back via agentStream/agentComplete/agentError signals
    }
    else if (command.startsWith("/help") || command == "/?") {
        std::string helpText = "<h3>Available Commands</h3>"
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
    (delta);
    if (m_tokenProgress->isHidden()) {
        m_tokenProgress->show();
        m_hideTimer->stop();
    }
    // Restart hide timer - will hide 1s after last token
    m_hideTimer->start(1000);
}

void ChatInterface::hideProgress()
{
    m_tokenProgress->hide();
}
// ========================================================

void ChatInterface::setCanSendMessage(bool enabled) {
    if (message_input_) {
        // Always enable input so user can try selecting another model
        message_input_->setEnabled(true);
    }
    if (statusLabel_) {
        std::string statusText;
        if (enabled) {
            statusText = "✓ Ready - Model loaded";
        } else {
            statusText = "⚠ No GGUF file found - Select a model with matching .gguf in D:/OllamaModels";
        }
        statusLabel_->setText(statusText);
    }
}

std::string ChatInterface::resolveGgufPath(const std::string& modelName)
{
    
    // First, try Ollama's 'show' command to get model information
    // This is more reliable than trying to find files manually
    QProcess ollamaShow;
    ollamaShow.start("ollama", std::vector<std::string>() << "show" << modelName);
    
    if (ollamaShow.waitForStarted(2000)) {
        if (ollamaShow.waitForFinished(3000)) {
            std::string output = std::string::fromUtf8(ollamaShow.readAllStandardOutput());
            
            // Ollama models are stored in blobs, but they're accessible through Ollama API
            // For now, we'll fall through to file search below, but at least we know the model exists
            if (!output.isEmpty()) {
            }
        }
    }
    
    // Search for GGUF file in multiple locations
    std::vector<std::string> searchPaths = {
        // User's custom models directory
        "D:/OllamaModels",
        // Windows Ollama models
        "C:/Users/" + qEnvironmentVariable("USERNAME") + "/.ollama/models",
        std::filesystem::path::homePath() + "/.ollama/models",
        // Alternative locations
        "C:/Ollama/models",
        std::filesystem::path::homePath() + "/.cache/ollama",
    };
    
    // Extract base model name (e.g., "llama3.2" from "llama3.2:3b" or "unlocked-350M" from "unlocked-350M:latest")
    std::string baseName = modelName.split(':').first();
    std::string searchPattern = "*" + baseName + "*.gguf";
    
    
    // First, try direct GGUF search in standard paths
    for (const std::string& searchPath : searchPaths) {
        std::filesystem::path dir(searchPath);
        if (!dir.exists()) {
            continue;
        }
        
        // Search for matching GGUF files
        std::vector<std::string> filters;
        filters << searchPattern;
        QFileInfoList files = dir.entryInfoList(filters, std::filesystem::path::Files, std::filesystem::path::Name);
        
        if (!files.isEmpty()) {
            // Return first matching GGUF file
            std::string path = files.first().absoluteFilePath();
            return path;
        }
        
    }
    
    // Second attempt: Search recursively in Ollama blobs (for Ollama-managed models)
    // Ollama stores models in ~/.ollama/blobs/blobs/ as sha256-* files without .gguf extension
    std::vector<std::string> blobsPaths = {
        std::filesystem::path::homePath() + "/.ollama/blobs/blobs",  // Windows/Linux actual location
        std::filesystem::path::homePath() + "/.ollama/models/blobs",  // Alternative location
    };
    
    for (const std::string& ollamaBlobs : blobsPaths) {
        std::filesystem::path blobDir(ollamaBlobs);
        if (!blobDir.exists()) {
            continue;
        }
        
        
        // Ollama blobs are named sha256-<hash> without extension
        // Look for large files (>100MB) which are likely model weights
        QFileInfoList blobFiles = blobDir.entryInfoList(std::vector<std::string>() << "sha256-*", 
                                                        std::filesystem::path::Files | std::filesystem::path::Hidden, 
                                                        std::filesystem::path::Name);
        
        // Sort by size descending and pick the largest file (likely the model)
        std::vector<std::pair<qint64, std::string>> sizedFiles;
        for (const std::filesystem::path& file : blobFiles) {
            qint64 sizeMB = file.size() / (1024 * 1024);
            if (sizeMB > 100) {  // Only consider files > 100MB
                sizedFiles.append(qMakePair(file.size(), file.absoluteFilePath()));
            }
        }
        
        if (!sizedFiles.isEmpty()) {
            // Sort by size and return largest (most recent/primary model)
            std::sort(sizedFiles.begin(), sizedFiles.end(), 
                     [](const std::pair<qint64, std::string>& a, const std::pair<qint64, std::string>& b) {
                         return a.first > b.first;  // Descending
                     });
            
            std::string path = sizedFiles.first().second;
            qint64 sizeMB = sizedFiles.first().first / (1024 * 1024);
            
            // Verify file is readable
            std::fstream testFile(path);
            if (!testFile.exists()) {
                continue;  // Try next blobs directory
            }
            
            if (!testFile.open(QIODevice::ReadOnly)) {
                continue;  // Try next blobs directory
            }
            
            // Verify it's a valid GGUF file by checking magic bytes
            std::vector<uint8_t> magic = testFile.read(4);
            testFile.close();
            
            if (magic.size() == 4 && magic[0] == 'G' && magic[1] == 'G' && 
                magic[2] == 'U' && magic[3] == 'F') {
                return path;
            } else {
                continue;  // Try next file
            }
        }
    }
    
    // If model exists in Ollama but we can't find GGUF file, suggest using Ollama directly
    return std::string();
}

