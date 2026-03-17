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
