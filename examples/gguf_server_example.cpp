/**
 * Example: Using the GGUF Server with Auto-Start
 * 
 * This example demonstrates:
 * 1. Creating an inference engine with a GGUF model
 * 2. Starting the GGUF server (auto-detects if already running)
 * 3. Making API requests to the server
 */

#include "inference_engine.hpp"
#include "gguf_server.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "=== GGUF Server Auto-Start Example ===";
    
    // Step 1: Create inference engine
    qInfo() << "Creating inference engine...";
    InferenceEngine* engine = new InferenceEngine();
    
    // Optionally load a model
    if (argc > 1) {
        QString modelPath = argv[1];
        qInfo() << "Loading model:" << modelPath;
        if (engine->loadModel(modelPath)) {
            qInfo() << "Model loaded successfully";
        } else {
            qWarning() << "Failed to load model - server will run without model";
        }
    } else {
        qInfo() << "No model specified - server will run without model";
        qInfo() << "Usage:" << argv[0] << "<model.gguf>";
    }
    
    // Step 2: Create and start GGUF server
    qInfo() << "\nStarting GGUF server...";
    GGUFServer* server = new GGUFServer(engine);
    
    // Connect signals for monitoring
    QObject::connect(server, &GGUFServer::serverStarted, [](quint16 port) {
        qInfo() << "✓ Server started successfully on port" << port;
        qInfo() << "\nAPI Endpoints:";
        qInfo() << "  POST http://localhost:" << port << "/api/generate";
        qInfo() << "  POST http://localhost:" << port << "/v1/chat/completions";
        qInfo() << "  GET  http://localhost:" << port << "/api/tags";
        qInfo() << "  GET  http://localhost:" << port << "/health";
        qInfo() << "\nExample curl command:";
        qInfo() << "  curl -X POST http://localhost:" << port << "/api/generate \\";
        qInfo() << "    -H \"Content-Type: application/json\" \\";
        qInfo() << "    -d '{\"prompt\":\"Explain quantum computing\"}'";
    });
    
    QObject::connect(server, &GGUFServer::requestReceived, [](const QString& endpoint, const QString& method) {
        qInfo() << "Request:" << method << endpoint;
    });
    
    QObject::connect(server, &GGUFServer::requestCompleted, [](const QString& endpoint, bool success, qint64 duration) {
        qInfo() << "Completed:" << endpoint << (success ? "✓" : "✗") << duration << "ms";
    });
    
    QObject::connect(server, &GGUFServer::error, [](const QString& errorMsg) {
        qWarning() << "Server error:" << errorMsg;
    });
    
    // Start on default port (11434 for Ollama compatibility)
    // This will auto-detect if a server is already running
    if (server->start(11434)) {
        qInfo() << "\n✓ Server is running on port" << server->port();
        
        // Print statistics every 30 seconds
        QTimer* statsTimer = new QTimer();
        QObject::connect(statsTimer, &QTimer::timeout, [server]() {
            auto stats = server->getStats();
            qInfo() << "\n=== Server Statistics ===";
            qInfo() << "Uptime:" << stats.uptimeSeconds << "seconds";
            qInfo() << "Total requests:" << stats.totalRequests;
            qInfo() << "Successful:" << stats.successfulRequests;
            qInfo() << "Failed:" << stats.failedRequests;
            qInfo() << "Tokens generated:" << stats.totalTokensGenerated;
            qInfo() << "========================\n";
        });
        statsTimer->start(30000); // Every 30 seconds
        
    } else {
        qCritical() << "✗ Failed to start server";
        return 1;
    }
    
    qInfo() << "\nPress Ctrl+C to stop server";
    
    return app.exec();
}

/*
 * Build instructions:
 * 
 * 1. Configure CMake:
 *    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
 * 
 * 2. Build:
 *    cmake --build build --config Release --target RawrXD-QtShell
 * 
 * 3. Run without model:
 *    ./build/bin/Release/RawrXD-QtShell.exe
 * 
 * 4. Run with model:
 *    ./build/bin/Release/RawrXD-QtShell.exe path/to/model.gguf
 * 
 * 
 * Testing the API:
 * 
 * 1. Generate text (PowerShell):
 *    $body = @{ prompt = "Hello, world!" } | ConvertTo-Json
 *    Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
 *        -Method POST -Body $body -ContentType "application/json"
 * 
 * 2. Chat completion (curl):
 *    curl -X POST http://localhost:11434/v1/chat/completions \
 *      -H "Content-Type: application/json" \
 *      -d '{"model":"gpt-4","messages":[{"role":"user","content":"Hi!"}]}'
 * 
 * 3. Health check:
 *    curl http://localhost:11434/health
 * 
 * 4. List models:
 *    curl http://localhost:11434/api/tags
 * 
 * 
 * Auto-start behavior:
 * 
 * Scenario 1: No server running
 *   → Binds to port 11434
 *   → Returns true
 *   → Server ready for requests
 * 
 * Scenario 2: Server already running on 11434
 *   → Detects existing server
 *   → Returns true
 *   → Uses existing instance (no duplicate)
 * 
 * Scenario 3: Port 11434 busy (non-server)
 *   → Tries port 11435, 11436, ...
 *   → Binds to first available port
 *   → Returns true
 * 
 * Scenario 4: All ports 11434-11443 busy
 *   → Returns false
 *   → Error signal emitted
 */
