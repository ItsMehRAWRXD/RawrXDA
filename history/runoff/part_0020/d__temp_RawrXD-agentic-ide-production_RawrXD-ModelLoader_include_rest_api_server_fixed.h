// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\include\rest_api_server.h
// Production REST API Server Header with comprehensive type definitions

#pragma once

#ifdef QT_HTTPSERVER_LIB
    #include <QHttpServer>
    #include <QHttpServerRequest>
    #include <QHttpServerResponse>
    #include <QRegularExpression>
    #include <QObject>
    #include <QJsonObject>
    #include <QMap>
    #include <QPair>
    #include <QList>
    #include <QDateTime>
    #include <memory>
    #include <functional>

    namespace RawrXD {
    namespace API {

    // HTTP request structure
    struct RESTRequest {
        QHttpServerRequest httpRequest;
        QString method;
        QString path;
        QMap<QString, QString> headers;
        QJsonObject body;
        QList<QPair<QString, QString>> queryParams;
        QString clientIP;
        QDateTime timestamp;
        QString traceId;
        QString spanId;
    };

    // HTTP response structure
    struct RESTResponse {
        int statusCode = 200;
        QMap<QString, QString> headers;
        QJsonObject body;
        QDateTime timestamp;
    };

    // API Request/Response types for versioning
    enum class APIVersion {
        V1,
        V2,
        BETA
    };

    // Route handler function type
    using RouteHandler = std::function<QJsonObject(const RESTRequest&)>;

    // Middleware handler function type
    using MiddlewareHandler = std::function<bool(RESTRequest&, RESTResponse&)>;

    class RESTAPIServer {
    public:
        RESTAPIServer();
        ~RESTAPIServer();
        
        // Server lifecycle
        bool start(uint16_t port = 8080);
        void stop();
        bool isRunning() const;
        
        // Route registration
        void registerRoute(const QString& method, const QString& path, RouteHandler handler);
        
        // Configuration
        void setRateLimit(int requestsPerMinute);
        void enableMetrics(bool enabled);
        
    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };

    } // namespace API
    } // namespace RawrXD

#else  // QT_HTTPSERVER_LIB not available

namespace RawrXD {
namespace API {
    class RESTAPIServer {
    public:
        RESTAPIServer() = default;
        bool start(uint16_t port = 8080) { return false; }
        void stop() {}
        bool isRunning() const { return false; }
    };
} // namespace API
} // namespace RawrXD

#endif // QT_HTTPSERVER_LIB
