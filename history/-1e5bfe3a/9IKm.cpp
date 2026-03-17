#include "MainWindow.h"
#include "unified_hotpatch_manager.hpp"
#include <QMessageBox>
#include <QDebug>

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
    });

    connect(m_hotpatchManager, &UnifiedHotpatchManager::errorOccurred,
            this, [this](const UnifiedResult& error) {
        qWarning() << "[MainWindow] Hotpatch error:" << error.operationName << error.errorDetail;
        if (m_hexMagConsole) {
            m_hexMagConsole->appendLog(QString("ERROR: %1 - %2").arg(error.operationName, error.errorDetail));
        }
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
