/**
 * @file MainWindow_AI_Integration.cpp
 * @brief Cursor-style AI backend switcher and quant integration for MainWindow
 * 
 * This file contains the integration code for:
 * - AI backend switcher (Local GGUF / llama.cpp / OpenAI / Claude / Gemini)
 * - Hot-swap quantization (Q4_0 through F32)
 * - Per-layer mixed precision
 * - Collaborative swarm editing
 * - Interpretability Panel - Model analysis and diagnostics
 * 
 * Add these methods to MainWindow class:
 * - Include this code in MainWindow.cpp or compile as separate translation unit
 * - Link with: ai_switcher, unified_backend, layer_quant_widget, QWebSockets
 */

#include "MainWindow.h"
#include "ai_switcher.hpp"
#include "unified_backend.hpp"
#include "layer_quant_widget.hpp"
#include "inference_engine.hpp"
#include "streaming_inference.hpp"
#include "command_palette.hpp"
#include "ai_chat_panel.hpp"


#include "../agent/auto_bootstrap.hpp"
#include "../agent/hot_reload.hpp"


/**
 * @brief Initialize AI backend switcher and unified backend
 * Call this from MainWindow constructor after creating m_inferenceEngine
 */
void MainWindow::setupAIBackendSwitcher()
{
    // Create AI switcher menu
    m_aiSwitcher = new AISwitcher(this);
    menuBar()->addMenu(m_aiSwitcher);
    
    // Create unified backend
    m_unifiedBackend = new UnifiedBackend(this);
    m_unifiedBackend->setLocalEngine(m_inferenceEngine);
    
    // Connect backend switching
// Qt connect removed
        // Connect unified backend to streaming (adapt signatures)
// Qt connect removed
            this, [this](qint64, const std::string& token) { if (m_streamer) m_streamer->pushToken(token); });
// Qt connect removed
            this, [this](qint64) { if (m_streamer) m_streamer->finishStream(); });
// Qt connect removed
            });
}

/**
 * @brief Setup quantization mode menu
 * Call this from MainWindow::setupMenuBar() in AI menu
 */
void MainWindow::setupQuantizationMenu(QMenu* aiMenu)
{
    QMenu* quantMenu = aiMenu->addMenu("Quant Mode");
    QActionGroup* quantGroup = new QActionGroup(quantMenu);
    quantGroup->setExclusive(true);
    
    std::vector<std::string> modes = {"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q6_K", "Q8_K", "F16", "F32"};
    for (const std::string& mode : modes) {
        QAction* action = quantGroup->addAction(mode);
        action->setCheckable(true);
        action->setChecked(mode == "Q4_0");  // Default
        action->setData(mode);
        quantMenu->addAction(action);
    }
// Qt connect removed
        QMetaObject::invokeMethod(m_inferenceEngine, "setQuantMode", 
                                  //QueuedConnection,
                                  (std::string, mode));
    });
    
    // Connect quantChanged signal to update status bar
// Qt connect removed
}

/**
 * @brief Setup per-layer quantization dock widget
 * Call this from MainWindow constructor
 */
void MainWindow::setupLayerQuantWidget()
{
    m_layerQuantDock = new QDockWidget("Layer Quantization", this);
    m_layerQuantWidget = new LayerQuantWidget(m_layerQuantDock);
    m_layerQuantDock->setWidget(m_layerQuantWidget);
    addDockWidget(//RightDockWidgetArea, m_layerQuantDock);
    m_layerQuantDock->hide();  // Hidden by default
    
    // Add to View menu
    // viewMenu->addAction(m_layerQuantDock->toggleViewAction());
    
    // Connect layer quant changes to inference engine
// Qt connect removed
    // Populate helper (GGUF metadata if available; else fallback examples)
    auto populate = [this]() {
        m_layerQuantWidget->clearTensors();
        std::vector<std::string> names = m_inferenceEngine ? m_inferenceEngine->tensorNames() : std::vector<std::string>();
        if (!names.isEmpty()) {
            for (const std::string& n : names) {
                m_layerQuantWidget->addTensor(n, m_currentQuantMode);
            }
        } else {
            m_layerQuantWidget->addTensor("token_embed.weight", "Q4_0");
            m_layerQuantWidget->addTensor("output.weight", "Q8_K");
            m_layerQuantWidget->addTensor("attn..weight", "Q5_1");
            m_layerQuantWidget->addTensor("attn.k_proj.weight", "Q5_1");
            m_layerQuantWidget->addTensor("attn.v_proj.weight", "Q5_0");
            m_layerQuantWidget->addTensor("mlp.up_proj.weight", "Q4_1");
        }
    };

    // Initial populate
    populate();

    // Repopulate when a model finishes loading
// Qt connect removed
            });
}

/**
 * @brief Setup collaborative swarm editing
 * Call this from MainWindow constructor
 */
void MainWindow::setupSwarmEditing()
{
    m_swarmSocket = new QWebSocket(std::string(), QWebSocketProtocol::VersionLatest, this);
// Qt connect removed
// Qt connect removed
    });
// Qt connect removed
    });
    
    // TODO: Connect code editor textChanged signal to broadcastEdit()
    // connect(codeView_, &QTextEdit::textChanged, this, &MainWindow::broadcastEdit);
}

/**
 * @brief Add swarm collaboration menu item
 * Call this from MainWindow::setupMenuBar() in Collaborate menu
 */
void MainWindow::setupCollaborationMenu()
{
    QMenu* collabMenu = menuBar()->addMenu(tr("Collaborate"));
    
    QAction* joinSwarmAction = collabMenu->addAction(tr("Join Swarm Session..."));
// Qt connect removed
    QAction* leaveSwarmAction = collabMenu->addAction(tr("Leave Swarm Session"));
// Qt connect removed
            m_swarmSessionId.clear();
        }
    });
}

// ============================================================================
// SLOT IMPLEMENTATIONS
// ============================================================================

void MainWindow::onAIBackendChanged(const std::string& id, const std::string& apiKey)
{
    m_currentBackend = id;
    m_currentAPIKey = apiKey;
    
    std::string displayName;
    if (id == "local") displayName = "Local GGUF";
    else if (id == "llama") displayName = "llama.cpp HTTP";
    else if (id == "openai") displayName = "OpenAI API";
    else if (id == "claude") displayName = "Claude API";
    else if (id == "gemini") displayName = "Gemini API";
    else displayName = id;
    
    statusBar()->showMessage("AI Backend: " + displayName, 5000);
    
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        std::string("🔄 AI Backend switched to: %1")
    );
}

void MainWindow::onQuantModeChanged(const std::string& mode)
{
    m_currentQuantMode = mode;
    statusBar()->showMessage("Quantization: " + mode, 3000);
    
    // Update status bar permanently
    static QLabel* quantLabel = nullptr;
    if (!quantLabel) {
        quantLabel = new QLabel(this);
        quantLabel->setStyleSheet("QLabel { padding: 2px 8px; background: #007acc; color: white; border-radius: 3px; }");
        statusBar()->addPermanentWidget(quantLabel);
    }
    quantLabel->setText(std::string("⚡ %1"));
}

void MainWindow::joinSwarmSession()
{
    bool ok = false;
    std::string sessionId = QInputDialog::getText(
        this,
        tr("Join Swarm Session"),
        tr("Enter shared document ID:"),
        QLineEdit::Normal,
        std::string(),
        &ok
    );
    
    if (ok && !sessionId.isEmpty()) {
        m_swarmSessionId = sessionId;
        
        // Connect to HexMag swarm WebSocket endpoint
        std::string url(std::string("ws://localhost:8001/collab/%1"));
        m_swarmSocket->open(url);
    }
}

void MainWindow::onSwarmMessage(const std::string& message)
{
    void* doc = void*::fromJson(message.toUtf8());
    void* obj = doc.object();
    
    std::string delta = obj["delta"].toString();
    int cursor = obj["cursor"].toInt();
    
    // For now, just log to HexMag console
    m_hexMagConsole->appendPlainText(
        std::string("📡 Swarm edit at %1: %2 chars"))
    );
}

void MainWindow::broadcastEdit()
{
    if (m_swarmSocket->state() != QAbstractSocket::ConnectedState) return;
    
    // Get current editor content and cursor position
    std::string content;
    int cursor = 0;
    
    if (codeView_) {
        content = codeView_->toPlainText();
        cursor = codeView_->textCursor().position();
    }
    
    void* msg{
        {"delta", content},
        {"cursor", cursor}
    };
    
    m_swarmSocket->sendTextMessage(
        void*(msg).toJson(void*::Compact)
    );
}

/**
 * @brief Override runInference to use unified backend
 * Replace existing runInference() implementation with this
 */
void MainWindow::runInference()
{
    if (!m_inferenceEngine->isModelLoaded() && m_currentBackend == "local") {
        QMessageBox::warning(this, tr("No Model"), 
                           tr("Please load a GGUF model first (AI → Load GGUF Model)."));
        return;
    }
    
    bool ok;
    std::string prompt = QInputDialog::getText(this, tr("AI Inference"), 
                                          tr("Enter your prompt:"),
                                          QLineEdit::Normal, std::string(), &ok);
    if (!ok || prompt.isEmpty()) return;
    
    qint64 reqId = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
    m_currentStreamId = reqId;
    
    // Start streaming in console
    if (m_streamer) {
        m_streamer->startStream(reqId, prompt);
    }

    // Submit request to unified backend
    if (!m_unifiedBackend) {
        m_hexMagConsole->appendPlainText(std::string("[%1] ERROR: Backend not initialized"));
        return;
    }

    UnifiedRequest req;
    req.prompt = prompt;
    req.reqId = reqId;
    req.backend = m_currentBackend;
    req.apiKey = m_currentAPIKey;
    m_unifiedBackend->submit(req);
}
/**
 * INTEGRATION CHECKLIST:
 * 
 * 1. In MainWindow.h, add forward declarations:
 *    class AISwitcher;
 *    class UnifiedBackend;
 *    class LayerQuantWidget;
 *    class QWebSocket;
 *    class AutoBootstrap;
 *    class HotReload;
 * 
 * 2. In MainWindow.h, add private members (already done above)
 * 
 * 3. In MainWindow.cpp constructor, add:
 *    setupAIBackendSwitcher();
 *    setupLayerQuantWidget();
 *    setupSwarmEditing();
 *    setupAgentSystem();
 * 
 * 4. In MainWindow::setupMenuBar(), add:
 *    setupQuantizationMenu(aiMenu);
 *    setupCollaborationMenu();
 * 
 * 5. Replace existing runInference() with new implementation above
 * 
 * 6. In CMakeLists.txt, add:
 *    target_sources(RawrXD-QtShell PRIVATE
 *        src/qtapp/ai_switcher.cpp
 *        src/qtapp/unified_backend.cpp
 *        src/qtapp/layer_quant_widget.cpp
 *        src/agent/auto_bootstrap.cpp
 *        src/agent/planner.cpp
 *        src/agent/self_patch.cpp
 *        src/agent/release_agent.cpp
 *        src/agent/meta_learn.cpp
 *        src/agent/hot_reload.cpp
 *    )
 *    target_link_libraries(RawrXD-QtShell PRIVATE
 *        Qt${QT_VERSION_MAJOR}::WebSockets
 *        Qt${QT_VERSION_MAJOR}::Network
 *    )
 */

// ========== AUTONOMOUS AGENT SYSTEM INTEGRATION ==========

/**
 * @brief Setup autonomous agent system with Ctrl+Shift+A trigger
 * Call this from MainWindow constructor
 * NOTE: This function is now implemented in MainWindow.cpp to avoid duplicate symbols
 */
/*
void MainWindow::setupAgentSystem()
{
    // Create agent bootstrap instance
    m_agentBootstrap = AutoBootstrap::instance();
    
    // Create hot reload instance
    m_hotReload = new HotReload(this);
    
    // Connect agent signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
    // Connect hot reload signals
// Qt connect removed
            });
    
}
*/

/**
 * @brief Setup Ctrl+Shift+A shortcut for agent mode
 * Call this from MainWindow::setupShortcuts()
 */
void MainWindow::setupShortcuts()
{
    // Ctrl+Shift+A: Trigger agent mode
    QShortcut* agentShortcut = new QShortcut(QKeySequence("Ctrl+Shift+A"), this);
// Qt connect removed
}

/**
 * @brief Triggered by Ctrl+Shift+A - grabs wish and starts agent
 */
void MainWindow::triggerAgentMode()
{
    std::string wish;
    
    // Try to get selected text from code editor
    if (codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        wish = cursor.selectedText().trimmed();
    }
    
    // If no selection, prompt user
    if (wish.isEmpty()) {
        bool ok;
        wish = QInputDialog::getText(
            this,
            "RawrXD Agent",
            "What should I build / fix / ship?",
            QLineEdit::Normal,
            "",
            &ok
        );
        
        if (!ok || wish.isEmpty()) {
            return;
        }
    }
    
    // Set environment variable for agent
    qputenv("RAWRXD_WISH", wish.toUtf8());
    
    // Start agent bootstrap
    m_agentBootstrap->start();
}

/**
 * @brief Slot for agent wish received
 */
void MainWindow::onAgentWishReceived(const std::string& wish)
{
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        std::string("[AGENT] Wish received: %1")
    );
    
    statusBar()->showMessage(std::string("Agent processing: %1"));
}

/**
 * @brief Slot for agent plan generated
 */
void MainWindow::onAgentPlanGenerated(const std::string& planSummary)
{
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        std::string("[AGENT] Plan:\n%1")
    );
    
    statusBar()->showMessage("Agent executing plan...");
}

/**
 * @brief Slot for agent execution completed
 */
void MainWindow::onAgentExecutionCompleted(bool success)
{
    std::string msg = success 
        ? "[AGENT] ✅ Execution completed successfully!"
        : "[AGENT] ❌ Execution failed";
    
    m_hexMagConsole->appendPlainText(msg);
    
    statusBar()->showMessage(
        success ? "Agent completed!" : "Agent failed",
        5000
    );
}

// ========== COMMAND PALETTE (VS Code Ctrl+Shift+P) ==========

/**
 * @brief Setup command palette with all IDE commands
 * Call this from MainWindow constructor
 */
void MainWindow::setupCommandPalette()
{
    m_commandPalette = new CommandPalette(this);
    
    // Register all commands
    CommandPalette::Command cmd;
    
    // File commands
    cmd = {
        "file.new", "New File", "File",
        "Create a new empty file",
        QKeySequence("Ctrl+N"),
        [this]() { handleNewEditor(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "file.open", "Open File...", "File",
        "Open an existing file",
        QKeySequence("Ctrl+O"),
        [this]() { 
            std::string fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                std::string(), tr("All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py)"));
            if (!fileName.isEmpty()) {
                std::fstream file(fileName);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    std::string content = in.readAll();
                    file.close();
                    if (codeView_) {
                        codeView_->setPlainText(content);
                        statusBar()->showMessage("Opened: " + fileName, 3000);
                    } else {
                        statusBar()->showMessage("No editor available", 3000);
                    }
                } else {
                    QMessageBox::warning(this, tr("Open Failed"), tr("Could not read file: %1"));
                }
            }
        }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "file.save", "Save File", "File",
        "Save the current file",
        QKeySequence("Ctrl+S"),
        [this]() {
            std::string fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                std::string(), tr("All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py)"));
            if (!fileName.isEmpty()) {
                std::fstream file(fileName);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    if (codeView_) {
                        out << codeView_->toPlainText();
                        file.close();
                        statusBar()->showMessage("Saved: " + fileName, 3000);
                    } else {
                        statusBar()->showMessage("No editor content to save", 3000);
                    }
                } else {
                    QMessageBox::warning(this, tr("Save Failed"), tr("Could not write to file: %1"));
                }
            }
        }
    };
    m_commandPalette->registerCommand(cmd);
    
    // AI commands
    cmd = {
        "ai.chat", "AI: Open Chat", "AI",
        "Open AI assistant chat panel",
        QKeySequence("Ctrl+Shift+I"),
        [this]() { if (m_aiChatDock) m_aiChatDock->show(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "ai.explain", "AI: Explain Code", "AI",
        "Ask AI to explain selected code",
        QKeySequence("Ctrl+Shift+E"),
        [this]() { explainCode(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "ai.fix", "AI: Fix Code", "AI",
        "Ask AI to fix issues in selected code",
        QKeySequence("Ctrl+Shift+F"),
        [this]() { fixCode(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "ai.refactor", "AI: Refactor Code", "AI",
        "Ask AI to refactor selected code",
        QKeySequence("Ctrl+Shift+R"),
        [this]() { refactorCode(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "ai.agent", "AI: Trigger Agent Mode", "AI",
        "Start autonomous coding agent (Ctrl+Shift+A)",
        QKeySequence("Ctrl+Shift+A"),
        [this]() { triggerAgentMode(); }
    };
    m_commandPalette->registerCommand(cmd);
    
    // Model commands
    cmd = {
        "model.load", "Load GGUF Model...", "Model",
        "Load a GGUF model file",
        QKeySequence(),
        [this]() {
            std::string fileName = QFileDialog::getOpenFileName(this, tr("Load GGUF Model"),
                std::string(), tr("GGUF Models (*.gguf);;All Files (*)"));
            if (!fileName.isEmpty() && m_inferenceEngine) {
                bool success = m_inferenceEngine->loadModel(fileName);
                if (success) {
                    statusBar()->showMessage("Model loaded: " + fileName, 5000);
                } else {
                    QMessageBox::warning(this, tr("Load Failed"), tr("Failed to load model: %1"));
                }
            }
        }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.q4", "Set Quantization: Q4_0", "Model",
        "Switch to Q4_0 quantization",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("Q4_0"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.q5", "Set Quantization: Q5_0", "Model",
        "Switch to Q5_0 quantization",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("Q5_0"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.q6", "Set Quantization: Q6_K", "Model",
        "Switch to Q6_K quantization",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("Q6_K"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.q8", "Set Quantization: Q8_K", "Model",
        "Switch to Q8_K quantization",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("Q8_K"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.f16", "Set Quantization: F16", "Model",
        "Switch to F16 quantization",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("F16"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "model.quant.f32", "Set Quantization: F32", "Model",
        "Switch to F32 (no quantization)",
        QKeySequence(),
        [this]() { if (m_inferenceEngine) m_inferenceEngine->setQuantMode("F32"); }
    };
    m_commandPalette->registerCommand(cmd);
    
    // View commands
    cmd = {
        "view.layerQuant", "Toggle Layer Quantization Panel", "View",
        "Show/hide per-layer quantization widget",
        QKeySequence(),
        [this]() { if (m_layerQuantDock) m_layerQuantDock->setVisible(!m_layerQuantDock->isVisible()); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "view.interpretability", "Toggle Model Interpretability Panel", "View",
        "Show/hide model analysis and diagnostics panel",
        QKeySequence("Ctrl+Shift+I"),
        [this]() { toggleInterpretabilityPanel(!m_interpretabilityPanelDock || !m_interpretabilityPanelDock->isVisible()); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "view.terminal", "Toggle Terminal", "View",
        "Show/hide integrated terminal",
        QKeySequence("Ctrl+`"),
        [this]() { if (terminalDock_) terminalDock_->setVisible(!terminalDock_->isVisible()); }
    };
    m_commandPalette->registerCommand(cmd);
    
    // Backend commands
    cmd = {
        "backend.local", "Switch to Local GGUF", "Backend",
        "Use local GGUF model for inference",
        QKeySequence(),
        [this]() { if (m_aiSwitcher) onAIBackendChanged("local", ""); }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "backend.openai", "Switch to OpenAI", "Backend",
        "Use OpenAI API for inference",
        QKeySequence(),
        [this]() {
            bool ok;
            std::string apiKey = QInputDialog::getText(this, tr("OpenAI API Key"),
                tr("Enter your OpenAI API key:"), QLineEdit::Password,
                m_currentBackend == "openai" ? m_currentAPIKey : std::string(), &ok);
            if (ok) {
                onAIBackendChanged("openai", apiKey);
            }
        }
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "backend.claude", "Switch to Claude", "Backend",
        "Use Anthropic Claude API for inference",
        QKeySequence(),
        [this]() {
            bool ok;
            std::string apiKey = QInputDialog::getText(this, tr("Claude API Key"),
                tr("Enter your Anthropic Claude API key:"), QLineEdit::Password,
                m_currentBackend == "claude" ? m_currentAPIKey : std::string(), &ok);
            if (ok) {
                onAIBackendChanged("claude", apiKey);
            }
        }
    };
    m_commandPalette->registerCommand(cmd);
    
}

// Interpretability Panel setup is in MainWindow.cpp to avoid separate compilation issues

