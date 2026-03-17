// Agentic Auto-Correction System Implementation
// Add this to the end of MainWindow.cpp before the final closing brace

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonObject>
#include <QStandardPaths>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

// ============================================================
// Agentic Auto-Correction System Implementation
// ============================================================

void MainWindow::initializeAgenticSystem()
{
    qInfo() << "Initializing Agentic Auto-Correction System...";
    
    // Create memory module first (persistent storage)
    m_memoryModule = new AgenticMemoryModule(this);
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/agentic_memory.db";
    if (!m_memoryModule->initialize(dbPath)) {
        qWarning() << "Failed to initialize memory module at:" << dbPath;
    } else {
        qInfo() << "Memory module initialized:" << dbPath;
    }
    
    // Create failure detector
    m_failureDetector = new AgenticFailureDetector(this);
    m_failureDetector->setRefusalThreshold(0.6);
    m_failureDetector->setQualityThreshold(0.4);
    m_failureDetector->setEnableToolValidation(true);
    
    connect(m_failureDetector, &AgenticFailureDetector::failureDetected,
            this, &MainWindow::onFailureDetected);
    
    // Create puppeteer (advanced correction)
    m_puppeteer = new AgenticPuppeteer(this);
    m_puppeteer->setAggressiveness(7);  // 1-10, moderate-high
    m_puppeteer->setConfidenceThreshold(70);
    m_puppeteer->enableLearning(true);
    m_puppeteer->enableAutoRetry(true, 3);
    
    connect(m_puppeteer, &AgenticPuppeteer::correctionApplied,
            [this](const QString& original, const QString& corrected) {
        onCorrectionApplied("Puppeteer", original, corrected);
    });
    
    // Create self-corrector (coordinates detection + correction)
    m_selfCorrector = new AgenticSelfCorrector(this);
    m_selfCorrector->setFailureDetector(m_failureDetector);
    m_selfCorrector->enableRefusalCorrection(true);
    m_selfCorrector->enableCapabilityInjection(true);
    m_selfCorrector->enableQualityEnhancement(true);
    m_selfCorrector->enableAdaptiveLearning(true);
    
    // Set available tools (these will be injected when model forgets)
    QStringList availableTools = {
        "read_file(path)",
        "write_file(path, content)",
        "search_code(pattern)",
        "execute_command(cmd)",
        "analyze_errors()",
        "suggest_fixes()",
        "refactor_code()",
        "generate_tests()"
    };
    m_selfCorrector->setAvailableTools(availableTools);
    
    connect(m_selfCorrector, &AgenticSelfCorrector::correctionApplied,
            this, &MainWindow::onCorrectionApplied);
    
    // Create hotpatch proxy (intercepts responses)
    m_hotpatchProxy = new OllamaHotpatchProxy(this);
    
    // AUTOMATED PORT CONFIGURATION: Detect Ollama and configure proxy
    // Check if Ollama is running on default port 11434
    QNetworkAccessManager* portCheckMgr = new QNetworkAccessManager(this);
    QNetworkRequest portCheckReq(QUrl("http://127.0.0.1:11434/"));
    QNetworkReply* portCheckReply = portCheckMgr->get(portCheckReq);
    
    connect(portCheckReply, &QNetworkReply::finished, this, [this, portCheckReply, portCheckMgr]() {
        portCheckReply->deleteLater();
        portCheckMgr->deleteLater();
        
        if (portCheckReply->error() == QNetworkReply::NoError) {
            // Ollama detected on 11434
            m_ollamaPort = 11434;
            m_hotpatchProxy->setUpstreamUrl(QString("http://localhost:%1").arg(m_ollamaPort));
            qInfo() << "Hotpatch proxy upstream configured:" << m_hotpatchProxy->upstreamUrl();
            qInfo() << "Clients should connect to: http://localhost:11436";
            
            if (m_llmLogView) {
                m_llmLogView->appendPlainText(QString("[AGENTIC] Hotpatch proxy configured: 11436 → %1").arg(m_ollamaPort));
            }
        } else {
            // Fallback to default
            m_hotpatchProxy->setUpstreamUrl("http://localhost:11434");
            qWarning() << "Ollama not detected on 11434, using default upstream";
        }
    });
    
    m_hotpatchProxy->setDebugLogging(true);
    
    // Connect proxy to self-corrector
    m_selfCorrector->setHotpatchProxy(m_hotpatchProxy);
    
    // Add custom processor that applies all corrections
    m_hotpatchProxy->addCustomPostProcessor([this](const QString& response) -> QString {
        if (!m_autoCorrectionEnabled) return response;
        
        // Extract prompt from context (simplified - real impl would track this)
        QString prompt = ""; // TODO: Track actual prompt
        
        // Analyze response for failures
        auto analysis = m_failureDetector->analyzeResponse(response, prompt, QStringList());
        
        // If failures detected, apply corrections
        if (!analysis.failures.isEmpty()) {
            QString corrected = m_selfCorrector->applyCorrections(response, prompt, analysis);
            
            // Store in memory for learning
            if (m_memoryModule && corrected != response) {
                QJsonObject metadata;
                metadata["original_length"] = response.length();
                metadata["corrected_length"] = corrected.length();
                metadata["failure_count"] = analysis.failures.size();
                
                m_memoryModule->recordCorrectionPattern(
                    "execution_failure",
                    response.left(100),  // pattern
                    corrected.left(100), // correction
                    metadata
                );
            }
            
            return corrected;
        }
        
        // Store successful responses for context extension
        if (m_contextExtensionEnabled && m_memoryModule) {
            QJsonObject metadata;
            metadata["prompt"] = prompt;
            metadata["response_quality"] = "good";
            
            m_memoryModule->storeMemory(
                AgenticMemoryModule::MemoryType::Conversation,
                response,
                AgenticMemoryModule::MemoryPriority::Normal,
                metadata
            );
        }
        
        return response;
    });
    
    // Start proxy on port 11436 (client connects here instead of 11434)
    if (m_hotpatchProxy->start(11436)) {
        qInfo() << "Hotpatch proxy started on port 11436";
        statusBar()->showMessage(tr("Auto-correction enabled (proxy: 11436)"), 5000);
    } else {
        qWarning() << "Failed to start hotpatch proxy";
    }
    
    // Create control panel dock
    createAgenticControlPanel();
    
    qInfo() << "Agentic system initialized successfully";
    
    // Start Ollama log monitoring
    QTimer::singleShot(1000, this, &MainWindow::startOllamaMonitoring);
    
    // Log to LLM console
    if (m_llmLogView) {
        m_llmLogView->appendPlainText("════════════════════════════════════════");
        m_llmLogView->appendPlainText("  AGENTIC AUTO-CORRECTION ACTIVE");
        m_llmLogView->appendPlainText("════════════════════════════════════════");
        m_llmLogView->appendPlainText("  Proxy Port:       11436");
        m_llmLogView->appendPlainText("  Upstream:         11434");
        m_llmLogView->appendPlainText("  Memory DB:        " + dbPath);
        m_llmLogView->appendPlainText("  Auto-Correction:  ENABLED");
        m_llmLogView->appendPlainText("  Context Extend:   ENABLED");
        m_llmLogView->appendPlainText("════════════════════════════════════════");
    }
}

void MainWindow::createAgenticControlPanel()
{
    m_agenticControlDock = new QDockWidget(tr("Agentic Control"), this);
    m_agenticControlDock->setObjectName("DockAgenticControl");
    
    QWidget* controlWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(controlWidget);
    
    // Title
    QLabel* title = new QLabel("<b>Auto-Correction System</b>");
    title->setStyleSheet("color: #4ec9b0; font-size: 14px;");
    layout->addWidget(title);
    
    // Auto-correction toggle
    QCheckBox* autoCorrectionCheck = new QCheckBox("Enable Auto-Correction");
    autoCorrectionCheck->setChecked(m_autoCorrectionEnabled);
    connect(autoCorrectionCheck, &QCheckBox::toggled, this, &MainWindow::toggleAutoCorrection);
    layout->addWidget(autoCorrectionCheck);
    
    // Context extension toggle
    QCheckBox* contextExtensionCheck = new QCheckBox("Enable Context Extension");
    contextExtensionCheck->setChecked(m_contextExtensionEnabled);
    connect(contextExtensionCheck, &QCheckBox::toggled, this, &MainWindow::toggleContextExtension);
    layout->addWidget(contextExtensionCheck);
    
    layout->addSpacing(10);
    
    // Statistics
    QLabel* statsLabel = new QLabel("<b>Statistics</b>");
    statsLabel->setStyleSheet("color: #4ec9b0;");
    layout->addWidget(statsLabel);
    
    QLabel* stats = new QLabel("Corrections: 0\nFailures Detected: 0\nMemories Stored: 0");
    stats->setObjectName("agenticStats");
    layout->addWidget(stats);
    
    layout->addSpacing(10);
    
    // Action buttons
    QPushButton* configBtn = new QPushButton("Configure Rules");
    connect(configBtn, &QPushButton::clicked, this, &MainWindow::configureHotpatchRules);
    layout->addWidget(configBtn);
    
    QPushButton* memoryBtn = new QPushButton("View Memory");
    connect(memoryBtn, &QPushButton::clicked, this, &MainWindow::viewMemoryDashboard);
    layout->addWidget(memoryBtn);
    
    QPushButton* exportBtn = new QPushButton("Export Learnings");
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportLearnings);
    layout->addWidget(exportBtn);
    
    QPushButton* importBtn = new QPushButton("Import Learnings");
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::importLearnings);
    layout->addWidget(importBtn);
    
    layout->addStretch();
    
    m_agenticControlDock->setWidget(controlWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_agenticControlDock);
}

void MainWindow::toggleAutoCorrection(bool enabled)
{
    m_autoCorrectionEnabled = enabled;
    qInfo() << "Auto-correction:" << (enabled ? "ENABLED" : "DISABLED");
    
    if (m_llmLogView) {
        m_llmLogView->appendPlainText(
            QString("Auto-correction %1").arg(enabled ? "ENABLED" : "DISABLED")
        );
    }
    
    statusBar()->showMessage(
        tr("Auto-correction %1").arg(enabled ? "enabled" : "disabled"), 3000
    );
}

void MainWindow::toggleContextExtension(bool enabled)
{
    m_contextExtensionEnabled = enabled;
    qInfo() << "Context extension:" << (enabled ? "ENABLED" : "DISABLED");
    
    if (m_llmLogView) {
        m_llmLogView->appendPlainText(
            QString("Context extension %1").arg(enabled ? "ENABLED" : "DISABLED")
        );
    }
    
    statusBar()->showMessage(
        tr("Context extension %1").arg(enabled ? "enabled" : "disabled"), 3000
    );
}

void MainWindow::onFailureDetected(int failureType, const QString& description, double confidence)
{
    QString typeStr;
    switch (static_cast<AgenticFailureDetector::FailureType>(failureType)) {
        case AgenticFailureDetector::FailureType::Refusal: typeStr = "REFUSAL"; break;
        case AgenticFailureDetector::FailureType::Hallucination: typeStr = "HALLUCINATION"; break;
        case AgenticFailureDetector::FailureType::QualityDegradation: typeStr = "QUALITY"; break;
        case AgenticFailureDetector::FailureType::TaskAbandonment: typeStr = "ABANDONMENT"; break;
        case AgenticFailureDetector::FailureType::CapabilityConfusion: typeStr = "CAP_CONFUSION"; break;
        case AgenticFailureDetector::FailureType::ToolMisuse: typeStr = "TOOL_MISUSE"; break;
        case AgenticFailureDetector::FailureType::ContextLoss: typeStr = "CONTEXT_LOSS"; break;
        case AgenticFailureDetector::FailureType::RepetitiveLoop: typeStr = "LOOP"; break;
        default: typeStr = "UNKNOWN"; break;
    }
    
    qWarning() << "Failure detected:" << typeStr << "-" << description << "(" << confidence << ")";
    
    if (m_llmLogView) {
        m_llmLogView->appendPlainText(
            QString("⚠️  FAILURE: %1 - %2 (confidence: %3%)")
                .arg(typeStr)
                .arg(description.left(80))
                .arg(static_cast<int>(confidence * 100))
        );
    }
}

void MainWindow::onCorrectionApplied(const QString& type, const QString& before, const QString& after)
{
    qInfo() << "Correction applied (" << type << "):" << before.left(50) << "->" << after.left(50);
    
    if (m_llmLogView) {
        m_llmLogView->appendPlainText(
            QString("✓ CORRECTED (%1):").arg(type)
        );
        m_llmLogView->appendPlainText(QString("  Before: %1...").arg(before.left(60)));
        m_llmLogView->appendPlainText(QString("  After:  %1...").arg(after.left(60)));
    }
}

void MainWindow::onMemoryStored(const QString& memoryId)
{
    qDebug() << "Memory stored:" << memoryId;
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
    QMessageBox::information(this, tr("Configure Rules"),
        tr("Hotpatch rule configuration coming soon!\n\n"
           "Current rules:\n"
           "• Math calculation enforcement\n"
           "• Sequential logic tracking\n"
           "• Context variable retention\n"
           "• Refusal bypass\n"
           "• Quality enhancement"));
}

void MainWindow::viewMemoryDashboard()
{
    if (!m_memoryModule) {
        QMessageBox::warning(this, tr("Memory Dashboard"), 
            tr("Memory module not initialized"));
        return;
    }
    
    auto stats = m_memoryModule->getStatistics();
    
    QString info = QString(
        "<b>Memory Dashboard</b><br><br>"
        "<b>Total Memories:</b> %1<br>"
        "<b>Database Size:</b> %2 KB<br>"
    ).arg(stats.totalMemories)
     .arg(stats.databaseSizeKB);
    
    QMessageBox::information(this, tr("Memory Dashboard"), info);
}

void MainWindow::exportLearnings()
{
    QString filename = QFileDialog::getSaveFileName(this, 
        tr("Export Learnings"), "", tr("JSON Files (*.json)"));
    
    if (filename.isEmpty()) return;
    
    if (!m_memoryModule) {
        QMessageBox::warning(this, tr("Export Failed"), 
            tr("Memory module not initialized"));
        return;
    }
    
    bool success = m_memoryModule->exportToJson(filename);
    
    if (success) {
        QMessageBox::information(this, tr("Export Complete"),
            tr("Learnings exported to: %1").arg(filename));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to export learnings"));
    }
}

void MainWindow::importLearnings()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Import Learnings"), "", tr("JSON Files (*.json)"));
    
    if (filename.isEmpty()) return;
    
    if (!m_memoryModule) {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Memory module not initialized"));
        return;
    }
    
    bool success = m_memoryModule->importFromJson(filename);
    
    if (success) {
        QMessageBox::information(this, tr("Import Complete"),
            tr("Learnings imported from: %1").arg(filename));
    } else {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Failed to import learnings"));
    }
}
