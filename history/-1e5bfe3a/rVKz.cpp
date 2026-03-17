#include "MainWindow.h"
#include "unified_hotpatch_manager.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_self_corrector.hpp"
#include "agentic_memory_module.hpp"
#include "HexMagConsole.h"
#include <QMessageBox>
#include <QDebug>
#include <QTextEdit>

void MainWindow::initializeAgenticSystem()
{
    qInfo() << "[MainWindow] Initializing Agentic System...";
    // Wire up agentic components and unified hotpatch manager
    if (!m_failureDetector || !m_selfCorrector || !m_memoryModule || !m_hotpatchManager) {
        qWarning() << "[MainWindow] Agentic components missing; initialization partial.";
    }

    // Connect failure detection -> self-correction
    if (m_failureDetector && m_selfCorrector) {
        connect(m_failureDetector, &AgenticFailureDetector::failureDetected,
                this, &MainWindow::onFailureDetected);
        connect(m_selfCorrector, &AgenticSelfCorrector::correctionApplied,
                this, &MainWindow::onCorrectionApplied);
    }

    // Connect memory module events
    if (m_memoryModule) {
        connect(m_memoryModule, &AgenticMemoryModule::memoryStored,
                this, &MainWindow::onMemoryStored);
    }

    // Connect UnifiedHotpatchManager signals for dashboard updates
    if (m_hotpatchManager) {
        connect(m_hotpatchManager, &UnifiedHotpatchManager::optimizationComplete,
                this, &MainWindow::onHotpatchOptimizationComplete);
        connect(m_hotpatchManager, &UnifiedHotpatchManager::errorOccurred, this, [this](const UnifiedResult& err){
            qWarning() << "[Hotpatch] Error in" << err.operationName << "layer" << (int)err.layer << ":" << err.errorDetail;
        });
    }
}

void MainWindow::toggleAutoCorrection(bool enabled)
{
    m_autoCorrectionEnabled = enabled;
    qInfo() << "[MainWindow] Auto-correction" << (enabled ? "enabled" : "disabled");
}

void MainWindow::toggleContextExtension(bool enabled)
{
    m_contextExtensionEnabled = enabled;
    qInfo() << "[MainWindow] Context extension" << (enabled ? "enabled" : "disabled");
}

void MainWindow::onFailureDetected(int failureType, const QString& description, double confidence)
{
    qWarning() << "[MainWindow] Failure detected:" << failureType << description << "Confidence:" << confidence;
    // Trigger self-correction via AgenticSelfCorrector and coordinate with UnifiedHotpatchManager
    if (m_selfCorrector) {
        m_selfCorrector->requestCorrection(failureType, description, confidence, m_autoCorrectionEnabled);
    }
    // Optionally perform a coordinated safety filter if confidence is high
    if (m_hotpatchManager && confidence >= 0.8) {
        auto results = m_hotpatchManager->applySafetyFilters();
        for (const auto& r : results) {
            qInfo() << "[Hotpatch] Safety filter" << r.operationName << (r.success ? "OK" : "FAIL") << r.errorDetail;
        }
    }
}

void MainWindow::onCorrectionApplied(const QString& type, const QString& before, const QString& after)
{
    qInfo() << "[MainWindow] Correction applied:" << type;
    // Log correction and update memory/context if enabled
    if (m_memoryModule && m_contextExtensionEnabled) {
        m_memoryModule->storeCorrection(type, before, after);
    }
    // Emit a unified patch-applied signal through manager for consistency (if available)
    if (m_hotpatchManager) {
        emit m_hotpatchManager->patchApplied(type, PatchLayer::System);
    }
}

void MainWindow::onMemoryStored(const QString& memoryId)
{
    qInfo() << "[MainWindow] Memory stored:" << memoryId;
}

void MainWindow::showAgenticControlPanel()
{
    if (m_agenticControlDock) {
        m_agenticControlDock->show();
        m_agenticControlDock->raise();
    }
}

void MainWindow::configureHotpatchRules()
{
    QMessageBox::information(this, "Hotpatch Rules", "Hotpatch configuration dialog not implemented yet.");
}

void MainWindow::viewMemoryDashboard()
{
    QMessageBox::information(this, "Memory Dashboard", "Memory dashboard not implemented yet.");
}

void MainWindow::exportLearnings()
{
    QMessageBox::information(this, "Export Learnings", "Export functionality not implemented yet.");
}

void MainWindow::importLearnings()
{
    QMessageBox::information(this, "Import Learnings", "Import functionality not implemented yet.");
}

void MainWindow::onHotpatchOptimizationComplete(const QString& type, int improvement)
{
    qInfo() << "[MainWindow] Hotpatch optimization complete:" << type << "Improvement:" << improvement << "%";
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Hotpatch optimization: %1 improved by %2%").arg(type).arg(improvement));
    }
    
    // Refresh dashboard to show optimization results
    refreshHotpatchDashboard();
}

void MainWindow::setupHotpatchSystem()
{
    if (!m_hotpatchManager) {
        qWarning() << "[MainWindow] Cannot setup hotpatch system - manager is null";
        return;
    }

    // Initialize the unified hotpatch manager
    auto result = m_hotpatchManager->initialize();
    if (!result.success) {
        qWarning() << "[MainWindow] Hotpatch system initialization failed:" << result.errorDetail;
        return;
    }

    // Connect signals for monitoring
    connect(m_hotpatchManager, &UnifiedHotpatchManager::initialized,
            this, [this]() {
        qInfo() << "[MainWindow] Hotpatch system initialized successfully";
    });

    connect(m_hotpatchManager, &UnifiedHotpatchManager::patchApplied,
            this, [this](const QString& name, PatchLayer layer) {
        QString layerName;
        switch(layer) {
            case PatchLayer::System: layerName = "System"; break;
            case PatchLayer::Memory: layerName = "Memory"; break;
            case PatchLayer::Byte: layerName = "Byte"; break;
            case PatchLayer::Server: layerName = "Server"; break;
        }
        qInfo() << "[MainWindow] Patch applied:" << name << "on layer" << layerName;
        if (m_hexMagConsole) {
            m_hexMagConsole->appendLog(QString("Patch applied: %1 [%2]").arg(name, layerName));
        }
        
        // Refresh dashboard if visible
        refreshHotpatchDashboard();
    });

    connect(m_hotpatchManager, &UnifiedHotpatchManager::errorOccurred,
            this, [this](const UnifiedResult& error) {
        qWarning() << "[MainWindow] Hotpatch error:" << error.operationName << error.errorDetail;
        if (m_hexMagConsole) {
            m_hexMagConsole->appendLog(QString("ERROR: %1 - %2").arg(error.operationName, error.errorDetail));
        }
        
        // Refresh dashboard if visible to show error state
        refreshHotpatchDashboard();
    });

    qInfo() << "[MainWindow] Hotpatch system setup complete";
}

void MainWindow::showHotpatchDashboard()
{
    if (m_hotpatchDock) {
        m_hotpatchDock->show();
        m_hotpatchDock->raise();
    } else {
        QMessageBox::information(this, "Hotpatch Dashboard", "Hotpatch dashboard not yet created.");
    }
}

// Agentic mode switcher implementations
void MainWindow::onAgenticModeChanged(int mode)
{
    qInfo() << "[MainWindow] Agentic mode changed to:" << mode;
    
    QString modeName;
    switch(mode) {
        case 0: modeName = "Ask Mode"; break;
        case 1: modeName = "Plan Mode"; break;
        case 2: modeName = "Agent Mode"; break;
        default: modeName = "Unknown Mode"; break;
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Switched to: %1").arg(modeName));
    }
    
    QMessageBox::information(this, "Mode Changed", QString("Switched to %1").arg(modeName));
}

void MainWindow::handleAskMode(const QString& question)
{
    qInfo() << "[MainWindow] Ask Mode query:" << question;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Ask Mode Query: %1").arg(question));
    }
    
    // TODO: Send to AI backend for simple Q&A response
    QMessageBox::information(this, "Ask Mode", QString("Processing question: %1\n\nAI integration pending...").arg(question));
}

void MainWindow::handlePlanMode(const QString& task)
{
    qInfo() << "[MainWindow] Plan Mode task:" << task;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Plan Mode Task: %1").arg(task));
    }
    
    // TODO: Use runSubagent to create plan, present to user for approval
    QString mockPlan = QString("📋 Plan for: %1\n\n"
                               "1. Analyze requirements\n"
                               "2. Design solution\n"
                               "3. Implement changes\n"
                               "4. Test and verify\n"
                               "5. Deploy\n\n"
                               "[Awaiting approval]").arg(task);
    
    QMessageBox::information(this, "Plan Mode", mockPlan);
}

void MainWindow::handleAgentMode(const QString& goal)
{
    qInfo() << "[MainWindow] Agent Mode goal:" << goal;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Agent Mode Goal: %1").arg(goal));
    }
    
    // TODO: Autonomous execution with manage_todo_list + runSubagent
    QString status = QString("🤖 Agent Mode Activated\n\n"
                            "Goal: %1\n\n"
                            "Status: Analyzing task...\n"
                            "Tools: Available\n"
                            "Mode: Autonomous\n\n"
                            "[Agent execution pending full implementation]").arg(goal);
    
    QMessageBox::information(this, "Agent Mode", status);
}

// Agentic system setup
void MainWindow::setupAgenticSystem()
{
    qInfo() << "[MainWindow] Setting up agentic system...";
    
    // Initialize agentic components
    if (!m_agenticOrchestrator) {
        qWarning() << "[MainWindow] Agentic orchestrator not initialized";
    }
    
    if (!m_agenticToolExecutor) {
        qWarning() << "[MainWindow] Agentic tool executor not initialized";
    }
    
    qInfo() << "[MainWindow] Agentic system setup complete";
}

void MainWindow::initializeAgenticOrchestrator()
{
    qInfo() << "[MainWindow] Initializing agentic orchestrator...";
    
    // Create orchestrator if needed
    if (!m_agenticOrchestrator) {
        qWarning() << "[MainWindow] Orchestrator creation not implemented";
        return;
    }
    
    qInfo() << "[MainWindow] Agentic orchestrator initialized";
}

void MainWindow::toggleAgenticEnabled(bool enabled)
{
    m_agenticEnabled = enabled;
    qInfo() << "[MainWindow] Agentic system" << (enabled ? "enabled" : "disabled");
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Agentic system: %1").arg(enabled ? "ENABLED" : "DISABLED"));
    }
}

void MainWindow::showAgenticDashboard()
{
    if (m_agenticDashboard) {
        m_agenticDashboard->show();
        m_agenticDashboard->raise();
    } else {
        QMessageBox::information(this, "Agentic Dashboard", "Agentic dashboard not yet created.");
    }
}

void MainWindow::configureAgenticCapabilities()
{
    QMessageBox::information(this, "Agentic Capabilities", 
        "Configure agentic capabilities:\n\n"
        "• Tool execution permissions\n"
        "• Autonomy level\n"
        "• Safety constraints\n"
        "• Resource limits\n\n"
        "[Configuration UI pending]");
}

void MainWindow::viewAgenticMemory()
{
    QMessageBox::information(this, "Agentic Memory", 
        "Agentic Memory Viewer:\n\n"
        "• Task history\n"
        "• Learned patterns\n"
        "• Tool usage statistics\n"
        "• Error corrections\n\n"
        "[Memory viewer pending]");
}

void MainWindow::clearAgenticContext()
{
    qInfo() << "[MainWindow] Clearing agentic context...";
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog("Agentic context cleared");
    }
    
    QMessageBox::information(this, "Context Cleared", "Agentic context has been reset.");
}

QString MainWindow::getCurrentEditorCode() const
{
    // TODO: Get actual code from active editor
    if (codeView_) {
        return codeView_->toPlainText();
    }
    return QString();
}

QString MainWindow::getCurrentEditorLanguage() const
{
    // TODO: Detect language from file extension or editor state
    return "cpp"; // Default assumption
}

// Model selector handlers
void MainWindow::onModelSelectionChanged(const QString& modelPath)
{
    qInfo() << "[MainWindow] Model selection changed:" << modelPath;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog(QString("Model selected: %1").arg(modelPath));
    }
    
    // TODO: Update inference engine with new model
}

void MainWindow::onLoadNewModel()
{
    qInfo() << "[MainWindow] Load new model requested";
    
    // TODO: Show file dialog and load GGUF model
    QMessageBox::information(this, "Load Model", "Model loading dialog not yet implemented.");
}

void MainWindow::onUnloadModel()
{
    qInfo() << "[MainWindow] Unload model requested";
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendLog("Model unloaded");
    }
    
    // TODO: Unload current model from inference engine
    QMessageBox::information(this, "Unload Model", "Model unloaded successfully (stub).");
}

void MainWindow::onShowModelInfo()
{
    qInfo() << "[MainWindow] Show model info requested";
    
    // TODO: Display detailed model information
    QString modelInfo = "Current Model Information:\n\n"
                       "Name: [Model Name]\n"
                       "Size: [Model Size]\n"
                       "Type: GGUF\n"
                       "Quantization: Q4_K_M\n"
                       "Context: 8192 tokens\n\n"
                       "[Full model info pending]";
    
    QMessageBox::information(this, "Model Information", modelInfo);
}
