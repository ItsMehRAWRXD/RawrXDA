#pragma once
// This module requires Qt HttpServer
#if defined(QT_HTTPSERVER_LIB) || defined(QT_HTTPSERVER_MODULE)
#include <QtGlobal>


#include <QObject>
#include <QHttpServer>
#include <QSslSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <chrono>

// ============================================================
// PRODUCTION API SERVER - TLS/SSL + REST + GraphQL + OIDC/JWKS
// ============================================================

namespace RawrXD {
namespace API {

/**
 * Structured error response for consistent error handling across all endpoints
 */
struct ErrorResponse {
    int code;
    QString message;
    QString errorType;
    QJsonObject details;
    QString requestId;
    long long timestamp;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["code"] = code;
        obj["message"] = message;
        obj["errorType"] = errorType;
        obj["details"] = details;
        obj["requestId"] = requestId;
        obj["timestamp"] = timestamp;
        return obj;
    }
};

/**
 * Structured success response for consistent response format
 */
struct SuccessResponse {
    QJsonObject data;
    QJsonObject meta;
    QString requestId;
    long long timestamp;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["success"] = true;
        obj["data"] = data;
        obj["meta"] = meta;
        obj["requestId"] = requestId;
        obj["timestamp"] = timestamp;
        return obj;
    }
};

/**
 * TLS/SSL Configuration
 */
struct TLSConfig {
    bool enabled = false;
    QString certificatePath;
    QString privateKeyPath;
    QString caChainPath;
    int minTLSVersion = 12; // 1.2
    std::vector<QString> cipherSuites;

    bool isValid() const {
        if (!enabled) return true;
        return !certificatePath.isEmpty() && !privateKeyPath.isEmpty();
    }
};

/**
 * OIDC/JWKS Configuration
 */
struct OIDCConfig {
    bool enabled = false;
    QString issuerUrl;
    QString clientId;
    QString clientSecret;
    QString jwksUrl;
    QString audience;
    int tokenExpirationSeconds = 3600;
    bool validateIssuer = true;
    bool validateAudience = true;

    bool isValid() const {
        if (!enabled) return true;
        return !issuerUrl.isEmpty() && !clientId.isEmpty() && !jwksUrl.isEmpty();
    }
};

/**
 * JWT Token information for authenticated requests
 */
struct JWTToken {
    QString accessToken;
    QString tokenType;
    int expiresIn;
    QString scope;
    QString userId;
    QJsonObject claims;
    std::chrono::system_clock::time_point expiresAt;

    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiresAt;
    }
};

/**
 * Authentication context for requests
 */
struct AuthContext {
    bool authenticated = false;
    QString userId;
    QString username;
    JWTToken token;
    QJsonArray scopes;
    QJsonObject claims;
};

/**
 * Request context with metadata
 */
struct RequestContext {
    QString requestId;
    QString method;
    QString path;
    QJsonObject headers;
    QJsonObject queryParams;
    QJsonObject body;
    QString remoteAddress;
    long long timestamp;
    AuthContext auth;
};

/**
 * Response builder for fluent API
 */
class ResponseBuilder {
public:
    ResponseBuilder() : requestId(QUuid::createUuid().toString()) {}

    ResponseBuilder& withData(const QJsonObject& d) {
        data = d;
        return *this;
    }

    ResponseBuilder& withMeta(const QJsonObject& m) {
        meta = m;
        return *this;
    }

    ResponseBuilder& withRequestId(const QString& id) {
        requestId = id;
        return *this;
    }

    ResponseBuilder& withError(int code, const QString& message, const QString& type = "Error") {
        isError = true;
        errorCode = code;
        errorMessage = message;
        errorType = type;
        return *this;
    }

    ResponseBuilder& withDetails(const QJsonObject& d) {
        errorDetails = d;
        return *this;
    }

    QJsonObject build() {
        if (isError) {
            ErrorResponse err;
            err.code = errorCode;
            err.message = errorMessage;
            err.errorType = errorType;
            err.details = errorDetails;
            err.requestId = requestId;
            err.timestamp = QDateTime::currentMSecsSinceEpoch();
            return err.toJson();
        } else {
            SuccessResponse resp;
            resp.data = data;
            resp.meta = meta;
            resp.requestId = requestId;
            resp.timestamp = QDateTime::currentMSecsSinceEpoch();
            return resp.toJson();
        }
    }

private:
    QString requestId;
    QJsonObject data;
    QJsonObject meta;
    bool isError = false;
    int errorCode = 0;
    QString errorMessage;
    QString errorType;
    QJsonObject errorDetails;
};

/**
 * GraphQL Query Resolver
 */
using GraphQLResolver = std::function<QJsonObject(const QJsonObject& args, const AuthContext& auth)>;

/**
 * Production-grade API server with TLS/SSL, REST, GraphQL, structured errors, and OIDC/JWKS
 */
class ProductionAPIServer : public QObject {
    Q_OBJECT

public:
    explicit ProductionAPIServer(QObject* parent = nullptr);
    ~ProductionAPIServer();

    // ============================================================
    // SERVER LIFECYCLE
    // ============================================================
    bool start(uint16_t port);
    bool stop();
    bool isRunning() const;

    // ============================================================
    // TLS/SSL CONFIGURATION
    // ============================================================
    void configureTLS(const TLSConfig& config);
    const TLSConfig& tlsConfig() const;
    bool reloadTLSCertificates();

    // ============================================================
    // OIDC/JWKS CONFIGURATION
    // ============================================================
    void configureOIDC(const OIDCConfig& config);
    const OIDCConfig& oidcConfig() const;
    bool validateJWT(const QString& token, AuthContext& outAuth);
    bool refreshOIDCConfiguration();

    // ============================================================
    // REST API ENDPOINTS
    // ============================================================
    void registerRESTEndpoint(const QString& method, const QString& path,
                            std::function<QJsonObject(const RequestContext&)> handler,
                            bool requiresAuth = false);

    // ============================================================
    // GRAPHQL ENDPOINTS
    // ============================================================
    void registerGraphQLQuery(const QString& name, const QString& schema,
                            GraphQLResolver resolver,
                            bool requiresAuth = false);
    void registerGraphQLMutation(const QString& name, const QString& schema,
                               GraphQLResolver resolver,
                               bool requiresAuth = true);

    // ============================================================
    // ERROR HANDLING
    // ============================================================
    ErrorResponse createError(int code, const QString& message,
                             const QString& type = "Error",
                             const QString& requestId = "");
    ErrorResponse validateRequest(const RequestContext& ctx);

    // ============================================================
    // MIDDLEWARE
    // ============================================================
    void use(std::function<bool(RequestContext&)> middleware);

    // ============================================================
    // METRICS & LOGGING
    // ============================================================
    void setMetricsEnabled(bool enabled);
    void setLoggingEnabled(bool enabled);
    QJsonObject getServerStats() const;

signals:
    void started(uint16_t port);
    void stopped();
    void errorOccurred(int code, const QString& message);
    void requestProcessed(const QString& requestId, int statusCode);

private slots:
    void onIncomingRequest();
    void onTLSError();

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    QJsonObject handleRESTRequest(const RequestContext& ctx);
    QJsonObject handleGraphQLRequest(const RequestContext& ctx);
    AuthContext extractAuthContext(const RequestContext& ctx);
    bool validateAuthContext(const AuthContext& auth, bool required);
};

} // namespace API
} // namespace RawrXD

#else  // Qt HttpServer not available - provide stub

#include <QtGlobal>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <vector>
#include <chrono>

namespace RawrXD {
namespace API {

// Minimal stub types when Qt HttpServer is not available
struct TLSConfig {
    bool enabled = false;
    QString certificatePath;
    QString privateKeyPath;
    QString caChainPath;
    int minTLSVersion = 12;
    std::vector<QString> cipherSuites;
    bool isValid() const { return !enabled || (!certificatePath.isEmpty() && !privateKeyPath.isEmpty()); }
};

struct OIDCConfig {
    bool enabled = false;
    QString issuerUrl;
    QString clientId;
    QString clientSecret;
    QString jwksUrl;
    std::chrono::seconds tokenExpiry{3600};
    bool isValid() const { return !enabled || (!issuerUrl.isEmpty() && !clientId.isEmpty()); }
};

class ProductionAPIServer {
public:
    ProductionAPIServer() = default;
    void start(quint16) {}
    void stop() {}
    bool isRunning() const { return false; }
};

} // namespace API
} // namespace RawrXD

#endif  // QT_HTTPSERVER_LIB
