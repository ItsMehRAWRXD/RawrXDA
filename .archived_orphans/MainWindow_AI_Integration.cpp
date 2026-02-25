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
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QLabel>
#include "../agent/auto_bootstrap.hpp"
#include "../agent/hot_reload.hpp"
#include <QWebSocket>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QDockWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QDateTime>
#include <QAbstractSocket>
#include <QUrl>
#include <QShortcut>
#include <QKeySequence>
#include <QProcessEnvironment>

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
    connect(m_aiSwitcher, &AISwitcher::backendChanged,
            this, &MainWindow::onAIBackendChanged);
    
        // Connect unified backend to streaming (adapt signatures)
        connect(m_unifiedBackend, &UnifiedBackend::streamToken,
            this, [this](qint64, const QString& token) { if (m_streamer) m_streamer->pushToken(token); });
        connect(m_unifiedBackend, &UnifiedBackend::streamFinished,
            this, [this](qint64) { if (m_streamer) m_streamer->finishStream(); });
    connect(m_unifiedBackend, &UnifiedBackend::error,
            this, [this](qint64 reqId, const QString& error) {
                m_hexMagConsole->appendPlainText(
                    QString("[%1] ERROR: %2").arg(reqId).arg(error)
                );
            });
    return true;
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
    
    QStringList modes = {"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q6_K", "Q8_K", "F16", "F32"};
    for (const QString& mode : modes) {
        QAction* action = quantGroup->addAction(mode);
        action->setCheckable(true);
        action->setChecked(mode == "Q4_0");  // Default
        action->setData(mode);
        quantMenu->addAction(action);
    return true;
}

    connect(quantGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString mode = action->data().toString();
        QMetaObject::invokeMethod(m_inferenceEngine, "setQuantMode", 
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, mode));
    });
    
    // Connect quantChanged signal to update status bar
    connect(m_inferenceEngine, &InferenceEngine::quantChanged,
            this, &MainWindow::onQuantModeChanged);
    return true;
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
    addDockWidget(Qt::RightDockWidgetArea, m_layerQuantDock);
    m_layerQuantDock->hide();  // Hidden by default
    
    // Add to View menu
    // viewMenu->addAction(m_layerQuantDock->toggleViewAction());
    
    // Connect layer quant changes to inference engine
    connect(m_layerQuantWidget, &LayerQuantWidget::quantChanged,
            m_inferenceEngine, &InferenceEngine::setLayerQuant);
    
    // Populate helper (GGUF metadata if available; else fallback examples)
    auto populate = [this]() {
        m_layerQuantWidget->clearTensors();
        QStringList names = m_inferenceEngine ? m_inferenceEngine->tensorNames() : QStringList();
        if (!names.isEmpty()) {
            for (const QString& n : names) {
                m_layerQuantWidget->addTensor(n, m_currentQuantMode);
    return true;
}

        } else {
            m_layerQuantWidget->addTensor("token_embed.weight", "Q4_0");
            m_layerQuantWidget->addTensor("output.weight", "Q8_K");
            m_layerQuantWidget->addTensor("attn.q_proj.weight", "Q5_1");
            m_layerQuantWidget->addTensor("attn.k_proj.weight", "Q5_1");
            m_layerQuantWidget->addTensor("attn.v_proj.weight", "Q5_0");
            m_layerQuantWidget->addTensor("mlp.up_proj.weight", "Q4_1");
    return true;
}

    };

    // Initial populate
    populate();

    // Repopulate when a model finishes loading
    connect(m_inferenceEngine, &InferenceEngine::modelLoadedChanged,
            this, [populate](bool loaded, const QString&) mutable {
                if (loaded) populate();
            });
    return true;
}

/**
 * @brief Setup collaborative swarm editing
 * Call this from MainWindow constructor
 */
void MainWindow::setupSwarmEditing()
{
    m_swarmSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    
    connect(m_swarmSocket, &QWebSocket::textMessageReceived,
            this, &MainWindow::onSwarmMessage);
    
    connect(m_swarmSocket, &QWebSocket::connected, this, [this]() {
        statusBar()->showMessage("Swarm session connected: " + m_swarmSessionId, 3000);
    });
    
    connect(m_swarmSocket, &QWebSocket::disconnected, this, [this]() {
        statusBar()->showMessage("Swarm session disconnected", 3000);
    });
    
    // Connect code editor textChanged signal to broadcastEdit for swarm collaboration
    if (codeView_) {
        connect(codeView_, &QTextEdit::textChanged, this, &MainWindow::broadcastEdit);
    return true;
}

    return true;
}

/**
 * @brief Add swarm collaboration menu item
 * Call this from MainWindow::setupMenuBar() in Collaborate menu
 */
void MainWindow::setupCollaborationMenu()
{
    QMenu* collabMenu = menuBar()->addMenu(tr("Collaborate"));
    
    QAction* joinSwarmAction = collabMenu->addAction(tr("Join Swarm Session..."));
    connect(joinSwarmAction, &QAction::triggered, this, &MainWindow::joinSwarmSession);
    
    QAction* leaveSwarmAction = collabMenu->addAction(tr("Leave Swarm Session"));
    connect(leaveSwarmAction, &QAction::triggered, this, [this]() {
        if (m_swarmSocket->state() == QAbstractSocket::ConnectedState) {
            m_swarmSocket->close();
            m_swarmSessionId.clear();
    return true;
}

    });
    return true;
}

// ============================================================================
// SLOT IMPLEMENTATIONS
// ============================================================================

void MainWindow::onAIBackendChanged(const QString& id, const QString& apiKey)
{
    m_currentBackend = id;
    m_currentAPIKey = apiKey;
    
    QString displayName;
    if (id == "local") displayName = "Local GGUF";
    else if (id == "llama") displayName = "llama.cpp HTTP";
    else if (id == "openai") displayName = "OpenAI API";
    else if (id == "claude") displayName = "Claude API";
    else if (id == "gemini") displayName = "Gemini API";
    else displayName = id;
    
    statusBar()->showMessage("AI Backend: " + displayName, 5000);
    
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        QString("🔄 AI Backend switched to: %1").arg(displayName)
    );
    return true;
}

void MainWindow::onQuantModeChanged(const QString& mode)
{
    m_currentQuantMode = mode;
    statusBar()->showMessage("Quantization: " + mode, 3000);
    
    // Update status bar permanently
    static QLabel* quantLabel = nullptr;
    if (!quantLabel) {
        quantLabel = new QLabel(this);
        quantLabel->setStyleSheet("QLabel { padding: 2px 8px; background: #007acc; color: white; border-radius: 3px; }");
        statusBar()->addPermanentWidget(quantLabel);
    return true;
}

    quantLabel->setText(QString("⚡ %1").arg(mode));
    return true;
}

void MainWindow::joinSwarmSession()
{
    bool ok = false;
    QString sessionId = QInputDialog::getText(
        this,
        tr("Join Swarm Session"),
        tr("Enter shared document ID:"),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    
    if (ok && !sessionId.isEmpty()) {
        m_swarmSessionId = sessionId;
        
        // Connect to HexMag swarm WebSocket endpoint
        QUrl url(QString("ws://localhost:8001/collab/%1").arg(sessionId));
        m_swarmSocket->open(url);
    return true;
}

    return true;
}

void MainWindow::onSwarmMessage(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();
    
    QString delta = obj["delta"].toString();
    int cursor = obj["cursor"].toInt();
    
    // For now, just log to HexMag console
    m_hexMagConsole->appendPlainText(
        QString("📡 Swarm edit at %1: %2 chars").arg(cursor).arg(delta.length())
    );
    return true;
}

void MainWindow::broadcastEdit()
{
    if (m_swarmSocket->state() != QAbstractSocket::ConnectedState) return;
    
    // Get current editor content and cursor position
    QString content;
    int cursor = 0;
    
    if (codeView_) {
        content = codeView_->toPlainText();
        cursor = codeView_->textCursor().position();
    return true;
}

    QJsonObject msg{
        {"delta", content},
        {"cursor", cursor}
    };
    
    m_swarmSocket->sendTextMessage(
        QJsonDocument(msg).toJson(QJsonDocument::Compact)
    );
    return true;
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
    return true;
}

    bool ok;
    QString prompt = QInputDialog::getText(this, tr("AI Inference"), 
                                          tr("Enter your prompt:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || prompt.isEmpty()) return;
    
    qint64 reqId = QDateTime::currentMSecsSinceEpoch();
    m_currentStreamId = reqId;
    
    // Start streaming in console
    if (m_streamer) {
        m_streamer->startStream(reqId, prompt);
    return true;
}

    // Submit request to unified backend
    if (!m_unifiedBackend) {
        m_hexMagConsole->appendPlainText(QString("[%1] ERROR: Backend not initialized").arg(reqId));
        return;
    return true;
}

    UnifiedRequest req;
    req.prompt = prompt;
    req.reqId = reqId;
    req.backend = m_currentBackend;
    req.apiKey = m_currentAPIKey;
    m_unifiedBackend->submit(req);
    return true;
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
    connect(m_agentBootstrap, &AutoBootstrap::wishReceived,
            this, &MainWindow::onAgentWishReceived);
    connect(m_agentBootstrap, &AutoBootstrap::planGenerated,
            this, &MainWindow::onAgentPlanGenerated);
    connect(m_agentBootstrap, &AutoBootstrap::executionCompleted,
            this, &MainWindow::onAgentExecutionCompleted);
    
    // Connect hot reload signals
    connect(m_hotReload, &HotReload::quantReloaded,
            this, [this](const QString& quant) {
                statusBar()->showMessage(QString("Quantization hot-reloaded: %1").arg(quant), 3000);
            });
    
    RAWRXD_LOG_DEBUG("Agent system initialized");
    return true;
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
    connect(agentShortcut, &QShortcut::activated, this, &MainWindow::triggerAgentMode);
    
    RAWRXD_LOG_DEBUG("Agent shortcut Ctrl+Shift+A registered");
    return true;
}

/**
 * @brief Triggered by Ctrl+Shift+A - grabs wish and starts agent
 */
void MainWindow::triggerAgentMode()
{
    QString wish;
    
    // Try to get selected text from code editor
    if (codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        wish = cursor.selectedText().trimmed();
    return true;
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
    return true;
}

    return true;
}

    // Set environment variable for agent
    qputenv("RAWRXD_WISH", wish.toUtf8());
    
    // Start agent bootstrap
    m_agentBootstrap->start();
    return true;
}

/**
 * @brief Slot for agent wish received
 */
void MainWindow::onAgentWishReceived(const QString& wish)
{
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        QString("[AGENT] Wish received: %1").arg(wish)
    );
    
    statusBar()->showMessage(QString("Agent processing: %1").arg(wish));
    return true;
}

/**
 * @brief Slot for agent plan generated
 */
void MainWindow::onAgentPlanGenerated(const QString& planSummary)
{
    // Log to HexMag console
    m_hexMagConsole->appendPlainText(
        QString("[AGENT] Plan:\n%1").arg(planSummary)
    );
    
    statusBar()->showMessage("Agent executing plan...");
    return true;
}

/**
 * @brief Slot for agent execution completed
 */
void MainWindow::onAgentExecutionCompleted(bool success)
{
    QString msg = success 
        ? "[AGENT] ✅ Execution completed successfully!"
        : "[AGENT] ❌ Execution failed";
    
    m_hexMagConsole->appendPlainText(msg);
    
    statusBar()->showMessage(
        success ? "Agent completed!" : "Agent failed",
        5000
    );
    return true;
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
            QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                QString(), tr("All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py)"));
            if (!fileName.isEmpty()) {
                QFile file(fileName);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    QString content = in.readAll();
                    file.close();
                    if (codeView_) {
                        codeView_->setPlainText(content);
                        statusBar()->showMessage("Opened: " + fileName, 3000);
                    } else {
                        statusBar()->showMessage("No editor available", 3000);
    return true;
}

                } else {
                    QMessageBox::warning(this, tr("Open Failed"), tr("Could not read file: %1").arg(fileName));
    return true;
}

    return true;
}

    return true;
}

    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "file.save", "Save File", "File",
        "Save the current file",
        QKeySequence("Ctrl+S"),
        [this]() {
            QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                QString(), tr("All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py)"));
            if (!fileName.isEmpty()) {
                QFile file(fileName);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    if (codeView_) {
                        out << codeView_->toPlainText();
                        file.close();
                        statusBar()->showMessage("Saved: " + fileName, 3000);
                    } else {
                        statusBar()->showMessage("No editor content to save", 3000);
    return true;
}

                } else {
                    QMessageBox::warning(this, tr("Save Failed"), tr("Could not write to file: %1").arg(fileName));
    return true;
}

    return true;
}

    return true;
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
            QString fileName = QFileDialog::getOpenFileName(this, tr("Load GGUF Model"),
                QString(), tr("GGUF Models (*.gguf);;All Files (*)"));
            if (!fileName.isEmpty() && m_inferenceEngine) {
                bool success = m_inferenceEngine->loadModel(fileName);
                if (success) {
                    statusBar()->showMessage("Model loaded: " + fileName, 5000);
                } else {
                    QMessageBox::warning(this, tr("Load Failed"), tr("Failed to load model: %1").arg(fileName));
    return true;
}

    return true;
}

    return true;
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
            QString apiKey = QInputDialog::getText(this, tr("OpenAI API Key"),
                tr("Enter your OpenAI API key:"), QLineEdit::Password,
                m_currentBackend == "openai" ? m_currentAPIKey : QString(), &ok);
            if (ok) {
                onAIBackendChanged("openai", apiKey);
    return true;
}

    return true;
}

    };
    m_commandPalette->registerCommand(cmd);
    
    cmd = {
        "backend.claude", "Switch to Claude", "Backend",
        "Use Anthropic Claude API for inference",
        QKeySequence(),
        [this]() {
            bool ok;
            QString apiKey = QInputDialog::getText(this, tr("Claude API Key"),
                tr("Enter your Anthropic Claude API key:"), QLineEdit::Password,
                m_currentBackend == "claude" ? m_currentAPIKey : QString(), &ok);
            if (ok) {
                onAIBackendChanged("claude", apiKey);
    return true;
}

    return true;
}

    };
    m_commandPalette->registerCommand(cmd);
    
    RAWRXD_LOG_DEBUG("Command palette initialized with") << "commands";
    return true;
}

// Interpretability Panel setup is in MainWindow.cpp to avoid separate compilation issues

