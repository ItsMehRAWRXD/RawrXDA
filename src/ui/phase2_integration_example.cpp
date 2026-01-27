// Phase 2 Polish Features - Integration Example
// Shows how to use all new widgets in MainWindow_v5.cpp

#include "ui/diff_preview_widget.h"
#include "ui/streaming_token_progress.h"
#include "ui/gpu_backend_selector.h"
#include "ui/model_download_dialog.h"
#include "ui/telemetry_optin_dialog.h"

namespace RawrXD {

// ============================================================================
// INTEGRATION EXAMPLE 1: Diff Preview Widget
// ============================================================================
// Add to MainWindow class members:
//   DiffPreviewWidget* m_diffPreview{nullptr};
//
// In MainWindow::initialize():

void integrateoDiffPreview() {
    // Create diff preview dock
    QDockWidget* diffDock = new QDockWidget("Code Changes", this);
    m_diffPreview = new DiffPreviewWidget(diffDock);
    diffDock->setWidget(m_diffPreview);
    addDockWidget(Qt::BottomDockWidgetArea, diffDock);
    diffDock->hide();  // Hidden until there's a diff to show
    
    // Connect to agentic engine for AI-generated code changes
    connect(m_agenticEngine, &AgenticEngine::codeChangeProposed,
            this, [this](const QString& file, const QString& original, const QString& proposed) {
        DiffChange change;
        change.filePath = file;
        change.originalContent = original;
        change.proposedContent = proposed;
        change.changeDescription = "AI-suggested refactoring";
        m_diffPreview->showDiff(change);
        diffDock->show();
    });
    
    // Handle user acceptance
    m_diffPreview->setAcceptCallback([this](const DiffChange& change) {
        // Apply the change to the file
        QFile file(change.filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(change.proposedContent.toUtf8());
            file.close();
            statusBar()->showMessage("✓ Changes applied to " + change.filePath, 3000);
        }
    });
    
    m_diffPreview->setRejectCallback([this](const DiffChange& change) {
        statusBar()->showMessage("✗ Changes rejected for " + change.filePath, 3000);
    });
}

// ============================================================================
// INTEGRATION EXAMPLE 2: Streaming Token Progress Bar
// ============================================================================
// Add to MainWindow class members:
//   StreamingTokenProgressBar* m_tokenProgress{nullptr};
//
// In MainWindow::initialize():

void integrateTokenProgress() {
    // Add to status bar
    m_tokenProgress = new StreamingTokenProgressBar(this);
    statusBar()->addPermanentWidget(m_tokenProgress);
    
    // Connect to inference engine
    connect(m_inferenceEngine, &InferenceEngine::generationStarted,
            this, [this](int estimatedTokens) {
        m_tokenProgress->startGeneration(estimatedTokens);
    });
    
    connect(m_inferenceEngine, &InferenceEngine::tokenGenerated,
            m_tokenProgress, &StreamingTokenProgressBar::onTokenGenerated);
    
    connect(m_inferenceEngine, &InferenceEngine::generationComplete,
            this, [this]() {
        m_tokenProgress->completeGeneration();
    });
    
    // Optional: Log performance metrics
    connect(m_tokenProgress, &StreamingTokenProgressBar::generationCompleted,
            this, [](int totalTokens, double tokensPerSecond) {
        qDebug() << "Generation metrics: " << totalTokens << "tokens @" 
                 << tokensPerSecond << "tok/s";
    });
}

// ============================================================================
// INTEGRATION EXAMPLE 3: GPU Backend Selector
// ============================================================================
// Add to MainWindow class members:
//   GPUBackendSelector* m_backendSelector{nullptr};
//
// In MainWindow::initialize():

void integrateBackendSelector() {
    // Add to toolbar
    QToolBar* aiToolbar = addToolBar("AI Settings");
    aiToolbar->setObjectName("AIToolbar");
    
    m_backendSelector = new GPUBackendSelector(this);
    aiToolbar->addWidget(m_backendSelector);
    
    // Connect to inference engine to change backend
    connect(m_backendSelector, &GPUBackendSelector::backendChanged,
            this, [this](ComputeBackend backend) {
        qDebug() << "Switching to backend:" << (int)backend;
        
        // Configure inference engine
        if (m_inferenceEngine) {
            QString backendStr;
            switch (backend) {
                case ComputeBackend::CUDA: backendStr = "cuda"; break;
                case ComputeBackend::Vulkan: backendStr = "vulkan"; break;
                case ComputeBackend::CPU: backendStr = "cpu"; break;
                default: backendStr = "auto"; break;
            }
            
            // Reload model with new backend
            m_inferenceEngine->setBackend(backendStr);
            statusBar()->showMessage("✓ Switched to " + backendStr + " backend", 3000);
        }
    });
    
    // Optional: Add refresh button
    QPushButton* refreshBtn = new QPushButton("🔄 Refresh", this);
    refreshBtn->setToolTip("Refresh available backends");
    connect(refreshBtn, &QPushButton::clicked, 
            m_backendSelector, &GPUBackendSelector::refreshBackends);
    aiToolbar->addWidget(refreshBtn);
}

// ============================================================================
// INTEGRATION EXAMPLE 4: Auto Model Download
// ============================================================================
// In MainWindow::initialize(), after checking for models:

void integrateAutoModelDownload() {
    // Check if user has any models
    AutoModelDownloader downloader;
    
    if (!downloader.hasLocalModels()) {
        qDebug() << "No local models detected - showing download dialog";
        
        // Show download dialog on first launch
        QTimer::singleShot(500, this, [this]() {
            ModelDownloadDialog* downloadDialog = new ModelDownloadDialog(this);
            
            if (downloadDialog->exec() == QDialog::Accepted) {
                // Model downloaded successfully
                statusBar()->showMessage("✓ Model downloaded! Refreshing model list...", 5000);
                
                // Refresh chat interface model list
                if (m_chatInterface) {
                    m_chatInterface->refreshModels();
                }
            } else {
                // User skipped download
                statusBar()->showMessage(
                    "No models installed. Use File → Download Model to get started", 
                    10000);
            }
            
            downloadDialog->deleteLater();
        });
    }
    
    // Add menu action for manual downloads
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* downloadAction = fileMenu->addAction("📥 Download Model...");
    connect(downloadAction, &QAction::triggered, this, [this]() {
        ModelDownloadDialog* dialog = new ModelDownloadDialog(this);
        dialog->exec();
        dialog->deleteLater();
    });
}

// ============================================================================
// INTEGRATION EXAMPLE 5: Telemetry Opt-In
// ============================================================================
// In MainWindow::initialize(), after UI is ready:

void integrateTelemetryOptIn() {
    // Check if user has already made a telemetry decision
    if (!hasTelemetryPreference()) {
        qDebug() << "No telemetry preference found - showing opt-in dialog";
        
        // Show telemetry dialog after 2 seconds (give UI time to load)
        QTimer::singleShot(2000, this, [this]() {
            TelemetryOptInDialog* telemetryDialog = new TelemetryOptInDialog(this);
            
            connect(telemetryDialog, &TelemetryOptInDialog::telemetryDecisionMade,
                    this, [this](bool enabled) {
                if (enabled) {
                    qDebug() << "Telemetry ENABLED by user";
                    m_telemetry->enableTelemetry(true);
                    m_telemetry->initializeHardware();
                    
                    // Record first launch event
                    QJsonObject metadata;
                    metadata["version"] = "5.0";
                    metadata["first_launch"] = true;
                    m_telemetry->recordEvent("app_started", metadata);
                    
                    statusBar()->showMessage("✓ Thank you for helping improve RawrXD IDE!", 5000);
                } else {
                    qDebug() << "Telemetry DISABLED by user";
                    m_telemetry->enableTelemetry(false);
                    statusBar()->showMessage("Telemetry disabled - you can enable it later in Settings", 5000);
                }
            });
            
            telemetryDialog->exec();
            telemetryDialog->deleteLater();
        });
    } else {
        // Load saved preference
        bool enabled = getTelemetryPreference();
        m_telemetry->enableTelemetry(enabled);
        
        if (enabled) {
            m_telemetry->initializeHardware();
            qDebug() << "Telemetry enabled (from saved preference)";
        } else {
            qDebug() << "Telemetry disabled (from saved preference)";
        }
    }
    
    // Add settings menu to change preference later
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    QAction* telemetrySettings = settingsMenu->addAction("📊 Telemetry Settings...");
    connect(telemetrySettings, &QAction::triggered, this, [this]() {
        TelemetryOptInDialog* dialog = new TelemetryOptInDialog(this);
        dialog->exec();
        dialog->deleteLater();
    });
}

// ============================================================================
// COMPLETE INTEGRATION - Call all integration functions
// ============================================================================

void MainWindow::initializePhase2Features() {
    qDebug() << "[MainWindow] Initializing Phase 2 polish features...";
    
    integrateDiffPreview();
    integrateTokenProgress();
    integrateBackendSelector();
    integrateAutoModelDownload();
    integrateTelemetryOptIn();
    
    qDebug() << "[MainWindow] ✓ Phase 2 features initialized";
}

} // namespace RawrXD
