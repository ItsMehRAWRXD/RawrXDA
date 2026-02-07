/**
 * Integration Guide: Adding GGUF Server to MainWindow
 * 
 * This shows how to integrate the auto-starting GGUF server
 * into your existing Qt application.
 */

// 1. Add to MainWindow.h header includes:
#include "gguf_server.hpp"
#include "inference_engine.hpp"

// 2. Add to MainWindow class private members:
class MainWindow : public QMainWindow {
    Q_OBJECT
    
private:
    // ... existing members ...
    
    // GGUF Server components
    InferenceEngine* m_inferenceEngine;
    GGUFServer* m_ggufServer;
    
    // Helper methods
    void initializeGGUFServer();
    void updateServerStatus();
};

// 3. Add to MainWindow.cpp constructor:
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_inferenceEngine(nullptr)
    , m_ggufServer(nullptr)
{
    // ... existing initialization ...
    
    // Initialize GGUF server
    initializeGGUFServer();
}

// 4. Implement server initialization:
void MainWindow::initializeGGUFServer()
{
    qInfo() << "Initializing GGUF Server...";
    
    // Create inference engine
    m_inferenceEngine = new InferenceEngine(this);
    
    // Optional: Load default model if specified in settings
    // QString modelPath = settings.value("default_model").toString();
    // if (!modelPath.isEmpty() && QFile::exists(modelPath)) {
    //     m_inferenceEngine->loadModel(modelPath);
    // }
    
    // Create server
    m_ggufServer = new GGUFServer(m_inferenceEngine, this);
    
    // Connect signals for UI updates
    connect(m_ggufServer, &GGUFServer::serverStarted, this, [this](quint16 port) {
        qInfo() << "GGUF Server started on port" << port;
        updateServerStatus();
        
        // Optional: Show notification in status bar
        statusBar()->showMessage(
            QString("API Server running on http://localhost:%1").arg(port),
            5000
        );
    });
    
    connect(m_ggufServer, &GGUFServer::requestReceived, this, [](const QString& endpoint, const QString& method) {
        qDebug() << "API Request:" << method << endpoint;
    });
    
    connect(m_ggufServer, &GGUFServer::requestCompleted, this, [](const QString& endpoint, bool success, qint64 duration) {
        qDebug() << "API Completed:" << endpoint << (success ? "✓" : "✗") << duration << "ms";
    });
    
    connect(m_ggufServer, &GGUFServer::error, this, [this](const QString& errorMsg) {
        qWarning() << "Server error:" << errorMsg;
        QMessageBox::warning(this, "GGUF Server Error", errorMsg);
    });
    
    // Start server (auto-detects if already running)
    if (m_ggufServer->start(11434)) {
        qInfo() << "GGUF Server initialized successfully on port" << m_ggufServer->port();
    } else {
        qWarning() << "Failed to start GGUF Server";
    }
}

// 5. Add server status UI (optional):
void MainWindow::updateServerStatus()
{
    if (!m_ggufServer || !m_ggufServer->isRunning()) {
        return;
    }
    
    auto stats = m_ggufServer->getStats();
    
    QString statusText = QString(
        "API Server: http://localhost:%1 | "
        "Requests: %2 | "
        "Success: %3 | "
        "Tokens: %4"
    ).arg(m_ggufServer->port())
     .arg(stats.totalRequests)
     .arg(stats.successfulRequests)
     .arg(stats.totalTokensGenerated);
    
    // Update status bar or label
    statusBar()->showMessage(statusText);
    
    // Or update a dedicated server info widget
    // m_serverStatusLabel->setText(statusText);
}

// 6. Add menu actions (optional):
void MainWindow::createServerMenu()
{
    QMenu* serverMenu = menuBar()->addMenu("Server");
    
    // Server status action
    QAction* statusAction = serverMenu->addAction("Server Status");
    connect(statusAction, &QAction::triggered, this, [this]() {
        if (!m_ggufServer) {
            QMessageBox::information(this, "Server Status", "Server not initialized");
            return;
        }
        
        auto stats = m_ggufServer->getStats();
        
        QString info = QString(
            "GGUF Server Status\n\n"
            "Running: %1\n"
            "Port: %2\n"
            "Uptime: %3 seconds\n"
            "Total Requests: %4\n"
            "Successful: %5\n"
            "Failed: %6\n"
            "Tokens Generated: %7\n"
            "Model Loaded: %8\n"
        ).arg(m_ggufServer->isRunning() ? "Yes" : "No")
         .arg(m_ggufServer->port())
         .arg(stats.uptimeSeconds)
         .arg(stats.totalRequests)
         .arg(stats.successfulRequests)
         .arg(stats.failedRequests)
         .arg(stats.totalTokensGenerated)
         .arg(m_inferenceEngine && m_inferenceEngine->isModelLoaded() ? "Yes" : "No");
        
        QMessageBox::information(this, "Server Status", info);
    });
    
    serverMenu->addSeparator();
    
    // Load model action
    QAction* loadModelAction = serverMenu->addAction("Load Model...");
    connect(loadModelAction, &QAction::triggered, this, [this]() {
        QString modelPath = QFileDialog::getOpenFileName(
            this,
            "Select GGUF Model",
            QString(),
            "GGUF Models (*.gguf);;All Files (*)"
        );
        
        if (!modelPath.isEmpty() && m_inferenceEngine) {
            if (m_inferenceEngine->loadModel(modelPath)) {
                QMessageBox::information(this, "Success", "Model loaded successfully");
                updateServerStatus();
            } else {
                QMessageBox::warning(this, "Error", "Failed to load model");
            }
        }
    });
    
    serverMenu->addSeparator();
    
    // Copy endpoint URL action
    QAction* copyUrlAction = serverMenu->addAction("Copy API URL");
    connect(copyUrlAction, &QAction::triggered, this, [this]() {
        if (m_ggufServer && m_ggufServer->isRunning()) {
            QString url = QString("http://localhost:%1").arg(m_ggufServer->port());
            QApplication::clipboard()->setText(url);
            statusBar()->showMessage("API URL copied to clipboard", 3000);
        }
    });
    
    // Open API docs action
    QAction* docsAction = serverMenu->addAction("API Documentation");
    connect(docsAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("file:///path/to/GGUF_SERVER_README.md"));
    });
}

// 7. Cleanup in destructor:
MainWindow::~MainWindow()
{
    if (m_ggufServer) {
        m_ggufServer->stop();
        delete m_ggufServer;
    }
    
    if (m_inferenceEngine) {
        delete m_inferenceEngine;
    }
}

// 8. Optional: Add periodic status updates with QTimer:
void MainWindow::setupServerMonitoring()
{
    QTimer* updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateServerStatus);
    updateTimer->start(5000); // Update every 5 seconds
}

/*
 * Complete minimal example in main_qt.cpp:
 */

#include <QApplication>
#include "MainWindow.h"
#include "gguf_server.hpp"
#include "inference_engine.hpp"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    // Initialize GGUF server (integrated in MainWindow)
    // Server auto-starts if not already running
    
    return app.exec();
}

/*
 * Alternative: Standalone server (no GUI):
 */

#include <QCoreApplication>
#include "gguf_server.hpp"
#include "inference_engine.hpp"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    // Create inference engine
    InferenceEngine engine;
    
    // Load model if specified
    if (argc > 1) {
        engine.loadModel(argv[1]);
    }
    
    // Create and start server
    GGUFServer server(&engine);
    
    if (server.start(11434)) {
        qInfo() << "Server running on port" << server.port();
        return app.exec();
    }
    
    qCritical() << "Failed to start server";
    return 1;
}
