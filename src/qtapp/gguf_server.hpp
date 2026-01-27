#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QString>
#include <QByteArray>
#include <memory>

class InferenceEngine;

/**
 * @brief HTTP server for GGUF model inference with Ollama-compatible API
 * 
 * Features:
 * - Auto-starts if not already running
 * - Port conflict detection and auto-recovery
 * - Ollama-compatible endpoints (/api/generate, /api/tags, etc.)
 * - OpenAI-compatible endpoints (/v1/chat/completions)
 * - Health monitoring and graceful shutdown
 * - Streaming response support
 */
class GGUFServer : public QObject {
    Q_OBJECT

public:
    explicit GGUFServer(InferenceEngine* engine, QObject* parent = nullptr);
    ~GGUFServer();

    /**
     * @brief Start the server (auto-starts if not already running)
     * @param port Port to listen on (default: 11434 for Ollama compatibility)
     * @return true if server started or already running
     */
    bool start(quint16 port = 11434);

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const;

    /**
     * @brief Get current server port
     */
    quint16 port() const;

    /**
     * @brief Check if a server is already running on the specified port
     * @param port Port to check
     * @return true if server detected on port
     */
    static bool isServerRunningOnPort(quint16 port);

    /**
     * @brief Get server statistics
     */
    struct ServerStats {
        quint64 totalRequests = 0;
        quint64 successfulRequests = 0;
        quint64 failedRequests = 0;
        quint64 totalTokensGenerated = 0;
        qint64 uptimeSeconds = 0;
        QString startTime;
    };
    ServerStats getStats() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const QString& endpoint, const QString& method);
    void requestCompleted(const QString& endpoint, bool success, qint64 durationMs);
    void error(const QString& errorMessage);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onHealthCheck();

private:
    // HTTP request parsing
    struct HttpRequest {
        QString method;
        QString path;
        QString httpVersion;
        QHash<QString, QString> headers;
        QByteArray body;
        QHash<QString, QString> queryParams;
    };

    struct HttpResponse {
        int statusCode = 200;
        QString statusText = "OK";
        QHash<QString, QString> headers;
        QByteArray body;
    };

    // Request handlers
    HttpRequest parseHttpRequest(const QByteArray& rawData);
    void handleRequest(QTcpSocket* socket, const HttpRequest& request);
    void sendResponse(QTcpSocket* socket, const HttpResponse& response);

    // API endpoint handlers
    void handleGenerateRequest(const HttpRequest& request, HttpResponse& response);
    void handleChatCompletionsRequest(const HttpRequest& request, HttpResponse& response);
    void handleTagsRequest(HttpResponse& response);
    void handlePullRequest(const HttpRequest& request, HttpResponse& response);
    void handlePushRequest(const HttpRequest& request, HttpResponse& response);
    void handleShowRequest(const HttpRequest& request, HttpResponse& response);
    void handleDeleteRequest(const HttpRequest& request, HttpResponse& response);
    void handleHealthRequest(HttpResponse& response);
    void handleNotFound(HttpResponse& response);
    void handleCorsPreflightRequest(HttpResponse& response);

    // Auto-start functionality
    bool tryBindPort(quint16 port);
    bool waitForServerShutdown(quint16 port, int maxWaitMs = 5000);
    
    // Utilities
    QString getCurrentTimestamp() const;
    QJsonDocument parseJsonBody(const QByteArray& body);
    void logRequest(const QString& method, const QString& path, int statusCode);
    
    // BOTTLENECK #3 FIX: Lightweight JSON field extraction (streaming-style, no DOM)
    QString extractJsonField(const QByteArray& json, const QString& fieldName);
    QJsonArray extractJsonArray(const QByteArray& json, const QString& fieldName);

private:
    InferenceEngine* m_engine;          ///< Inference engine for model operations
    QTcpServer* m_server;               ///< TCP server instance
    QHash<QTcpSocket*, QByteArray> m_pendingRequests; ///< Buffer for incomplete requests
    QMutex m_mutex;                     ///< Thread safety
    
    // Server state
    bool m_isRunning;
    quint16 m_port;
    QDateTime m_startTime;
    
    // Statistics
    ServerStats m_stats;
    
    // Health monitoring
    QTimer* m_healthTimer;
    
    // Configuration
    static constexpr int MAX_REQUEST_SIZE = 100 * 1024 * 1024; // 100MB max request
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 30000;     // 30 seconds
    static constexpr int DEFAULT_TIMEOUT_MS = 120000;          // 2 minutes
};
