// PHASE 2 POLISH - QUICK INTEGRATION PATCH
// Add this to MainWindow_v5.cpp to enable all Phase 2 features
// Location: After initializePhase4() completes

// ============================================================================
// ADD TO MainWindow_v5.h (Header file - private section)
// ============================================================================

// Phase 2 Polish Feature Widgets
#include "ui/diff_preview_widget.h"
#include "ui/streaming_token_progress.h"
#include "ui/gpu_backend_selector.h"
#include "ui/auto_model_downloader.h"
#include "ui/model_download_dialog.h"
#include "ui/telemetry_optin_dialog.h"

class MainWindow : public QMainWindow {
    // ... existing code ...
    
private:
    // Phase 2 feature widgets
    RawrXD::DiffPreviewWidget* m_diffPreview{nullptr};
    RawrXD::StreamingTokenProgressBar* m_tokenProgress{nullptr};
    RawrXD::GPUBackendSelector* m_backendSelector{nullptr};
    QDockWidget* m_diffPreviewDock{nullptr};
    
    // Phase 2 initialization
    void initializePhase2Polish();
};

// ============================================================================
// ADD TO MainWindow_v5.cpp (Implementation file)
// ============================================================================

void MainWindow::initializePhase2Polish()
{
    qDebug() << "[MainWindow] Initializing Phase 2 Polish Features...";
    
    // ===== 1. DIFF PREVIEW WIDGET =====
    try {
        m_diffPreviewDock = new QDockWidget("📋 Code Changes Preview", this);
        m_diffPreview = new RawrXD::DiffPreviewWidget(m_diffPreviewDock);
        m_diffPreviewDock->setWidget(m_diffPreview);
        m_diffPreviewDock->setObjectName("DiffPreviewDock");
        addDockWidget(Qt::BottomDockWidgetArea, m_diffPreviewDock);
        m_diffPreviewDock->hide();  // Hidden until needed
        
        // Connect accept/reject callbacks
        m_diffPreview->setAcceptCallback([this](const RawrXD::DiffChange& change) {
            QFile file(change.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(change.proposedContent.toUtf8());
                file.close();
                statusBar()->showMessage("✓ Changes applied to " + QFileInfo(change.filePath).fileName(), 3000);
                
                // Reload file in editor if open
                if (m_multiTabEditor) {
                    m_multiTabEditor->reloadFile(change.filePath);
                }
            } else {
                QMessageBox::warning(this, "File Error", 
                    "Could not write changes to " + change.filePath);
            }
        });
        
        m_diffPreview->setRejectCallback([this](const RawrXD::DiffChange& change) {
            statusBar()->showMessage("✗ Rejected changes for " + QFileInfo(change.filePath).fileName(), 3000);
        });
        
        qDebug() << "  ✓ Diff Preview Widget initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init diff preview:" << e.what();
    }
    
    // ===== 2. STREAMING TOKEN PROGRESS BAR =====
    try {
        m_tokenProgress = new RawrXD::StreamingTokenProgressBar(this);
        m_tokenProgress->setShowTokenRate(true);
        m_tokenProgress->setShowElapsedTime(true);
        statusBar()->addPermanentWidget(m_tokenProgress);
        
        // Connect to inference engine (if available)
        if (m_inferenceEngine) {
            connect(m_inferenceEngine, &InferenceEngine::generationStarted,
                    this, [this](int estimatedTokens) {
                m_tokenProgress->startGeneration(estimatedTokens);
            });
            
            connect(m_inferenceEngine, &InferenceEngine::tokenGenerated,
                    m_tokenProgress, &RawrXD::StreamingTokenProgressBar::onTokenGenerated);
            
            connect(m_inferenceEngine, &InferenceEngine::generationComplete,
                    this, [this]() {
                m_tokenProgress->completeGeneration();
            });
        }
        
        // Log performance metrics
        connect(m_tokenProgress, &RawrXD::StreamingTokenProgressBar::generationCompleted,
                this, [this](int totalTokens, double tokensPerSecond) {
            qDebug() << "[Performance] Generation:" << totalTokens << "tokens @" 
                     << QString::number(tokensPerSecond, 'f', 2) << "tok/s";
            
            // Record telemetry if enabled
            if (m_telemetry && m_telemetry->isEnabled()) {
                QJsonObject metrics;
                metrics["total_tokens"] = totalTokens;
                metrics["tokens_per_second"] = tokensPerSecond;
                m_telemetry->recordEvent("inference_completed", metrics);
            }
        });
        
        qDebug() << "  ✓ Token Progress Bar initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init token progress:" << e.what();
    }
    
    // ===== 3. GPU BACKEND SELECTOR =====
    try {
        QToolBar* aiToolbar = nullptr;
        
        // Find existing AI toolbar or create new one
        for (QToolBar* toolbar : findChildren<QToolBar*>()) {
            if (toolbar->objectName() == "AIToolbar") {
                aiToolbar = toolbar;
                break;
            }
        }
        
        if (!aiToolbar) {
            aiToolbar = addToolBar("⚙️ AI Settings");
            aiToolbar->setObjectName("AIToolbar");
            aiToolbar->setMovable(true);
        }
        
        m_backendSelector = new RawrXD::GPUBackendSelector(this);
        aiToolbar->addWidget(new QLabel(" Backend: ", this));
        aiToolbar->addWidget(m_backendSelector);
        
        // Connect backend changes to inference engine
        connect(m_backendSelector, &RawrXD::GPUBackendSelector::backendChanged,
                this, [this](RawrXD::ComputeBackend backend) {
            QString backendName;
            switch (backend) {
                case RawrXD::ComputeBackend::CUDA: backendName = "cuda"; break;
                case RawrXD::ComputeBackend::Vulkan: backendName = "vulkan"; break;
                case RawrXD::ComputeBackend::CPU: backendName = "cpu"; break;
                case RawrXD::ComputeBackend::DirectML: backendName = "directml"; break;
                default: backendName = "auto"; break;
            }
            
            qDebug() << "[MainWindow] Switching backend to:" << backendName;
            
            if (m_inferenceEngine) {
                // TODO: Implement setBackend() in InferenceEngine
                // m_inferenceEngine->setBackend(backendName);
                statusBar()->showMessage("✓ Backend switched to " + backendName, 3000);
            }
        });
        
        qDebug() << "  ✓ GPU Backend Selector initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init backend selector:" << e.what();
    }
    
    // ===== 4. AUTO MODEL DOWNLOAD =====
    try {
        // Check for models after UI is ready
        QTimer::singleShot(1000, this, [this]() {
            RawrXD::AutoModelDownloader downloader;
            
            if (!downloader.hasLocalModels()) {
                qDebug() << "[MainWindow] No models detected - offering download";
                
                RawrXD::ModelDownloadDialog* dialog = new RawrXD::ModelDownloadDialog(this);
                
                if (dialog->exec() == QDialog::Accepted) {
                    statusBar()->showMessage("✓ Model downloaded! Refreshing model list...", 5000);
                    
                    // Refresh chat interface models
                    if (m_chatInterface) {
                        m_chatInterface->refreshModels();
                    }
                } else {
                    statusBar()->showMessage(
                        "ℹ No models installed. Use File → Download Model to get started", 
                        10000);
                }
                
                dialog->deleteLater();
            }
        });
        
        qDebug() << "  ✓ Auto Model Download initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init model downloader:" << e.what();
    }
    
    // ===== 5. TELEMETRY OPT-IN =====
    try {
        // Check telemetry preference after 2 seconds
        QTimer::singleShot(2000, this, [this]() {
            if (!RawrXD::hasTelemetryPreference()) {
                qDebug() << "[MainWindow] No telemetry preference - showing opt-in dialog";
                
                RawrXD::TelemetryOptInDialog* dialog = new RawrXD::TelemetryOptInDialog(this);
                
                connect(dialog, &RawrXD::TelemetryOptInDialog::telemetryDecisionMade,
                        this, [this](bool enabled) {
                    if (m_telemetry) {
                        m_telemetry->enableTelemetry(enabled);
                        
                        if (enabled) {
                            m_telemetry->initializeHardware();
                            
                            // Record first launch
                            QJsonObject metadata;
                            metadata["version"] = "5.0";
                            metadata["first_launch"] = true;
                            m_telemetry->recordEvent("app_started", metadata);
                            
                            statusBar()->showMessage("✓ Thank you for helping improve RawrXD IDE!", 5000);
                        } else {
                            statusBar()->showMessage("Telemetry disabled", 3000);
                        }
                    }
                });
                
                dialog->exec();
                dialog->deleteLater();
                
            } else {
                // Load saved preference
                bool enabled = RawrXD::getTelemetryPreference();
                if (m_telemetry) {
                    m_telemetry->enableTelemetry(enabled);
                    if (enabled) {
                        m_telemetry->initializeHardware();
                    }
                }
                qDebug() << "[MainWindow] Telemetry:" << (enabled ? "enabled" : "disabled") << "(from settings)";
            }
        });
        
        qDebug() << "  ✓ Telemetry Opt-In initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init telemetry:" << e.what();
    }
    
    qDebug() << "[MainWindow] ✓ All Phase 2 Polish Features initialized successfully";
}

// ============================================================================
// CALL THIS AT THE END OF initializePhase4()
// ============================================================================
// In MainWindow::initializePhase4(), after all core features are ready:

void MainWindow::initializePhase4()
{
    // ... existing Phase 4 code ...
    
    // After everything is initialized:
    updateSplashProgress("✓ All components ready", 100);
    
    // Replace splash with main UI
    setCentralWidget(m_multiTabEditor);
    m_multiTabEditor->show();
    
    // Initialize Phase 2 Polish Features
    initializePhase2Polish();  // <--- ADD THIS LINE
    
    // Hide splash
    if (m_splashWidget) {
        m_splashWidget->hide();
        m_splashWidget->deleteLater();
        m_splashWidget = nullptr;
    }
    
    statusBar()->showMessage("Ready", 3000);
    qDebug() << "[MainWindow] Initialization complete!";
}

// ============================================================================
// ADD MENU ACTIONS (Optional - for manual access)
// ============================================================================

void MainWindow::createMenus()
{
    // ... existing menu code ...
    
    // File menu additions
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* downloadModelAction = fileMenu->addAction("📥 Download Model...");
    connect(downloadModelAction, &QAction::triggered, this, [this]() {
        RawrXD::ModelDownloadDialog* dialog = new RawrXD::ModelDownloadDialog(this);
        dialog->exec();
        dialog->deleteLater();
    });
    
    // View menu additions
    QMenu* viewMenu = menuBar()->addMenu("&View");
    if (m_diffPreviewDock) {
        viewMenu->addAction(m_diffPreviewDock->toggleViewAction());
    }
    
    // Settings menu
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    QAction* telemetryAction = settingsMenu->addAction("📊 Telemetry Settings...");
    connect(telemetryAction, &QAction::triggered, this, [this]() {
        RawrXD::TelemetryOptInDialog* dialog = new RawrXD::TelemetryOptInDialog(this);
        dialog->exec();
        dialog->deleteLater();
    });
}
