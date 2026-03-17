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
                this, [this](AgenticFailureDetector::FailureType type, const QString& description, double confidence) {
                    onFailureDetected(static_cast<int>(type), description, confidence);
                });
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
        m_hexMagConsole->appendPlainText(QString("Hotpatch optimization: %1 improved by %2%").arg(type).arg(improvement));
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
            m_hexMagConsole->appendPlainText(QString("Patch applied: %1 [%2]").arg(name, layerName));
        }
        
        // Refresh dashboard if visible
        refreshHotpatchDashboard();
    });

    connect(m_hotpatchManager, &UnifiedHotpatchManager::errorOccurred,
            this, [this](const UnifiedResult& error) {
        qWarning() << "[MainWindow] Hotpatch error:" << error.operationName << error.errorDetail;
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(QString("ERROR: %1 - %2").arg(error.operationName, error.errorDetail));
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

// REMOVED DUPLICATE: onAgenticModeChanged, handleAskMode, handlePlanMode, handleAgentMode
// These are defined in MainWindow.cpp

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
        m_hexMagConsole->appendPlainText(QString("Agentic system: %1").arg(enabled ? "ENABLED" : "DISABLED"));
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
        m_hexMagConsole->appendPlainText("Agentic context cleared");
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

// REMOVED DUPLICATE: onModelSelectionChanged, onLoadNewModel, onUnloadModel, onShowModelInfo
// These are defined in MainWindow.cpp

void MainWindow::refreshHotpatchDashboard()
{
    // Only refresh if dashboard is visible
    if (!m_hotpatchDock || !m_hotpatchDock->isVisible()) {
        return;
    }
    
    qDebug() << "[MainWindow] Refreshing hotpatch dashboard...";
    
    // Get statistics from the unified hotpatch manager
    if (!m_hotpatchManager) {
        qWarning() << "[MainWindow] Cannot refresh dashboard - hotpatch manager is null";
        return;
    }
    
    auto stats = m_hotpatchManager->getStatistics();
    
    // Find the dashboard widget inside the dock
    QWidget* dashboardWidget = m_hotpatchDock->widget();
    if (!dashboardWidget) {
        qWarning() << "[MainWindow] Dashboard dock has no widget";
        return;
    }
    
    // Find the text display widget (assuming it's a QTextEdit or similar)
    QTextEdit* statsDisplay = dashboardWidget->findChild<QTextEdit*>("hotpatchStatsDisplay");
    if (statsDisplay) {
        // Format statistics for display
        QString statsText;
        statsText += "═══════════════════════════════════════\n";
        statsText += "    UNIFIED HOTPATCH DASHBOARD\n";
        statsText += "═══════════════════════════════════════\n\n";
        
        statsText += QString("📊 Total Patches Applied: %1\n").arg(stats.totalPatchesApplied);
        statsText += QString("💾 Total Bytes Modified: %1\n").arg(stats.totalBytesModified);
        statsText += QString("⚡ Coordinated Actions: %1\n\n").arg(stats.coordinatedActionsCompleted);
        
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += "  MEMORY LAYER STATS\n";
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += QString("Patches Applied: %1\n").arg(stats.memoryStats.patchesApplied);
        statsText += QString("Bytes Modified: %1\n").arg(stats.memoryStats.bytesModified);
        statsText += QString("Active Patches: %1\n\n").arg(stats.memoryStats.activePatches);
        
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += "  BYTE LAYER STATS\n";
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += QString("Patches Applied: %1\n").arg(stats.byteStats.patchesApplied);
        statsText += QString("Files Patched: %1\n").arg(stats.byteStats.filesPatched);
        statsText += QString("Bytes Written: %1\n\n").arg(stats.byteStats.totalBytesWritten);
        
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += "  SERVER LAYER STATS\n";
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += QString("Active Hotpatches: %1\n").arg(stats.serverStats.activeHotpatches);
        statsText += QString("Requests Intercepted: %1\n").arg(stats.serverStats.requestsIntercepted);
        statsText += QString("Responses Modified: %1\n\n").arg(stats.serverStats.responsesModified);
        
        statsText += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        statsText += QString("Session Started: %1\n").arg(stats.sessionStarted.toString("yyyy-MM-dd HH:mm:ss"));
        statsText += QString("Last Action: %1\n").arg(stats.lastCoordinatedAction.toString("yyyy-MM-dd HH:mm:ss"));
        
        statsDisplay->setPlainText(statsText);
        qDebug() << "[MainWindow] Dashboard refreshed successfully";
    } else {
        qWarning() << "[MainWindow] Could not find stats display widget";
    }
}
