#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QString>

// Forward declarations
class QTcpServer;
class QWebSocketServer;
class QWebSocket;
class AgentOrchestrator;

/**
 * @class AgentAPIService
 * @brief Local API Gateway for the Autonomous Agent Framework
 * 
 * Provides REST and WebSocket endpoints to control the AgentOrchestrator
 * from external clients (web UI, CLI tools, external services).
 * 
 * Key Features:
 * - REST API for orchestration control (start, stop, persist)
 * - WebSocket streaming for real-time task status and LLM output
 * - Decoupled architecture (separates business logic from transport layer)
 * 
 * Phase 3 Architecture Component
 */
class AgentAPIService : public QObject {
    Q_OBJECT

public:
    explicit AgentAPIService(QSharedPointer<AgentOrchestrator> orchestrator, 
                            int port = 8080, 
                            QObject* parent = nullptr);
    ~AgentAPIService();

    // Server lifecycle control
    bool startServer();
    void stopServer();
    
    bool isRunning() const { return restServer_ != nullptr && wsServer_ != nullptr; }
    int getPort() const { return apiPort_; }

signals:
    // Logging and diagnostic signals
    void logMessage(const QString& message);
    void serverStarted(int restPort, int wsPort);
    void serverStopped();
    void clientConnected(const QString& clientInfo);
    void clientDisconnected(const QString& clientInfo);

private slots:
    // WebSocket connection management
    void handleWebSocketConnection();
    void handleWebSocketDisconnection();
    void handleWebSocketMessage(const QString& message);
    
    // Orchestrator signal forwarding (for WebSocket broadcast)
    void forwardTaskStatus(const QString& taskId, const QString& status, const QString& agentType);
    void forwardTaskChunk(const QString& taskId, const QString& chunk, const QString& agentType);
    void forwardOrchestrationFinished(bool success);

private:
    // REST request routing (conceptual - would integrate with cpp-httplib/Pistache)
    void handleRestRequest(const QByteArray& requestPayload, const QString& endpoint);
    
    // REST endpoint handlers
    void handlePostOrchestrate(const QByteArray& payload);
    void handlePostPersist(const QByteArray& payload, const QString& action);
    void handlePostControl(const QByteArray& payload, const QString& action);
    
    // WebSocket message broadcasting
    void broadcastToClients(const QString& message);

    // Core components
    QSharedPointer<AgentOrchestrator> orchestrator_;
    
    // Network servers
    QTcpServer* restServer_{nullptr};           // Placeholder for REST (would use cpp-httplib)
    QWebSocketServer* wsServer_{nullptr};       // Real-time streaming server
    QList<QWebSocket*> wsClients_;              // Connected WebSocket clients
    
    // Configuration
    int apiPort_{8080};                         // REST API port
    int wsPort_{8081};                          // WebSocket port (apiPort_ + 1)
};
