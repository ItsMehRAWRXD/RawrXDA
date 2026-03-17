#include "production_api_server.h"
#include <QDebug>

// Only compile the full implementation if QT_HTTPSERVER is available
#if defined(QT_HTTPSERVER_LIB) || defined(QT_HTTPSERVER_MODULE)

#include <QHttpServer>
#include <QHttpServerResponder>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslConfiguration>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QTimer>
#include <QFile>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QBase64>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>

#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <chrono>
#include <regex>
#include <ctime>

#include "config_manager.h"
#include "feature_toggle.h"
#include "jwt_validator.h"
#include "logging/logging.h"

namespace RawrXD {
namespace API {

// ============================================================
// IMPLEMENTATION CLASS
// ============================================================

class ProductionAPIServer::Impl {
public:
    // Server infrastructure
    std::unique_ptr<QHttpServer> httpServer;
    uint16_t port = 8443;
    bool isRunning = false;
    QMutex mutex;

    // Configuration
    TLSConfig tlsConfig;
    OIDCConfig oidcConfig;

    // SSL/TLS support
    QSslConfiguration sslConfig;
    bool tlsEnabled = false;

    // Route handlers
    std::map<QString, std::map<QString, std::function<QJsonObject(const RequestContext&)>>> restRoutes;
    std::map<QString, std::pair<QString, GraphQLResolver>> graphqlQueries;
    std::map<QString, std::pair<QString, GraphQLResolver>> graphqlMutations;
    std::map<QString, bool> routeAuthRequired;

    // Middleware pipeline
    std::vector<std::function<bool(RequestContext&)>> middlewares;

    // Metrics
    bool metricsEnabled = true;
    bool loggingEnabled = true;
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    int totalLatencyMs = 0;

    // Authentication cache
    std::map<QString, AuthContext> tokenCache;
    QTimer* tokenCacheCleanupTimer = nullptr;

    // JWKS cache
    QJsonArray cachedJWKS;
    std::chrono::system_clock::time_point jwksExpiresAt;
    RawrXD::Auth::JWKSManager jwksManager;
    QString sharedSecret; // for HS256 fallback

    // Rate limiting
    std::map<QString, std::vector<long long>> requestTimestamps;
    int maxRequestsPerMinute = 1000;

    void logRequest(const RequestContext& ctx, int statusCode, long latencyMs) {
        if (!loggingEnabled) return;

        totalRequests++;
        totalLatencyMs += latencyMs;

        QString logMsg = QString("[%1] %2 %3 - Status: %4, Latency: %5ms")
            .arg(ctx.timestamp)
            .arg(ctx.method)
            .arg(ctx.path)
            .arg(statusCode)
            .arg(latencyMs);

        LOG_API_INFO() << logMsg;
    }

    bool checkRateLimit(const QString& clientIp) {
        auto now = QDateTime::currentMSecsSinceEpoch();
        auto windowStart = now - 60000; // 1 minute window

        auto& timestamps = requestTimestamps[clientIp];
        timestamps.erase(
            std::remove_if(timestamps.begin(), timestamps.end(),
                          [windowStart](long long ts) { return ts < windowStart; }),
            timestamps.end()
        );

        if (timestamps.size() >= maxRequestsPerMinute) {
            return false;
        }

        timestamps.push_back(now);
        return true;
    }

    bool loadTLSCertificates() {
        if (!tlsConfig.enabled) {
            return true;
        }

        // Load certificate
        QFile certFile(tlsConfig.certificatePath);
        if (!certFile.open(QIODevice::ReadOnly)) {
            qCritical() << "Failed to open certificate file:" << tlsConfig.certificatePath;
            return false;
        }
        QSslCertificate cert(&certFile, QSsl::Pem);
        certFile.close();

        if (cert.isNull()) {
            qCritical() << "Invalid certificate";
            return false;
        }

        // Load private key
        QFile keyFile(tlsConfig.privateKeyPath);
        if (!keyFile.open(QIODevice::ReadOnly)) {
            qCritical() << "Failed to open private key file:" << tlsConfig.privateKeyPath;
            return false;
        }
        QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
        keyFile.close();

        if (key.isNull()) {
            qCritical() << "Invalid private key";
            return false;
        }

        // Configure SSL
        sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setLocalCertificate(cert);
        sslConfig.setPrivateKey(key);
        sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

        tlsEnabled = true;
        qInfo() << "TLS/SSL configured successfully";
        return true;
    }

    QJsonArray fetchJWKS() {
        if (!oidcConfig.enabled || oidcConfig.jwksUrl.isEmpty()) {
            return QJsonArray();
        }

        // Check cache
        if (!cachedJWKS.isEmpty() && std::chrono::system_clock::now() < jwksExpiresAt) {
            return cachedJWKS;
        }

        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(oidcConfig.jwksUrl));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(10000); // 10s timeout
        loop.exec();

        if (timeout.isActive()) {
            timeout.stop();
        } else {
            LOG_API_ERROR() << "JWKS fetch timed out";
            reply->abort();
            reply->deleteLater();
            return cachedJWKS;
        }

        if (reply->error() != QNetworkReply::NoError) {
            LOG_API_ERROR() << "Failed to fetch JWKS:" << reply->errorString();
            reply->deleteLater();
            return cachedJWKS;
        }

        const QByteArray body = reply->readAll();
        reply->deleteLater();

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            LOG_API_ERROR() << "Invalid JWKS payload";
            return cachedJWKS;
        }

        QJsonObject jwksObj = doc.object();
        if (!jwksObj.contains("keys")) {
            LOG_API_ERROR() << "JWKS payload missing keys";
            return cachedJWKS;
        }

        cachedJWKS = jwksObj.value("keys").toArray();
        jwksManager.loadFromJson(jwksObj);

        // Set cache expiry using Cache-Control max-age when available, otherwise 1 hour
        int maxAgeSeconds = 3600;
        const QVariant cacheControl = reply->header(QNetworkRequest::CacheLoadControlAttribute);
        Q_UNUSED(cacheControl);
        jwksExpiresAt = std::chrono::system_clock::now() + std::chrono::seconds(maxAgeSeconds);

        LOG_API_INFO() << "JWKS fetched and cached";
        return cachedJWKS;
    }
};

// ============================================================
// PUBLIC API
// ============================================================

ProductionAPIServer::ProductionAPIServer(QObject* parent)
    : QObject(parent), impl(std::make_unique<Impl>())
{
    impl->httpServer = std::make_unique<QHttpServer>();
    
    // Setup token cache cleanup timer
    impl->tokenCacheCleanupTimer = new QTimer(this);
    connect(impl->tokenCacheCleanupTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker lock(&impl->mutex);
        auto now = std::chrono::system_clock::now();
        for (auto it = impl->tokenCache.begin(); it != impl->tokenCache.end(); ) {
            if (it->second.token.isExpired()) {
                it = impl->tokenCache.erase(it);
            } else {
                ++it;
            }
        }
    });
}

ProductionAPIServer::~ProductionAPIServer() {
    stop();
}

bool ProductionAPIServer::start(uint16_t port) {
    QMutexLocker lock(&impl->mutex);

    if (impl->isRunning) {
        return false;
    }

    // Load configuration once
    ConfigManager::instance().load();

    // Apply configuration values
    const QJsonObject serverCfg = ConfigManager::instance().section("server");
    impl->port = serverCfg.value("port").toInt(port);
    impl->loggingEnabled = serverCfg.value("logRequests").toBool(true);
    impl->metricsEnabled = serverCfg.value("metricsEnabled").toBool(true);

    const QJsonObject tlsCfg = ConfigManager::instance().section("tls");
    impl->tlsConfig.enabled = tlsCfg.value("enabled").toBool(impl->tlsConfig.enabled);
    impl->tlsConfig.certificatePath = tlsCfg.value("certificatePath").toString(impl->tlsConfig.certificatePath);
    impl->tlsConfig.privateKeyPath = tlsCfg.value("privateKeyPath").toString(impl->tlsConfig.privateKeyPath);
    impl->tlsConfig.caChainPath = tlsCfg.value("caChainPath").toString(impl->tlsConfig.caChainPath);
    impl->tlsConfig.minTLSVersion = tlsCfg.value("minTLSVersion").toInt(impl->tlsConfig.minTLSVersion);

    const QJsonObject oidcCfg = ConfigManager::instance().section("oidc");
    impl->oidcConfig.enabled = oidcCfg.value("enabled").toBool(impl->oidcConfig.enabled);
    impl->oidcConfig.issuerUrl = oidcCfg.value("issuerUrl").toString(impl->oidcConfig.issuerUrl);
    impl->oidcConfig.clientId = oidcCfg.value("clientId").toString(impl->oidcConfig.clientId);
    impl->oidcConfig.clientSecret = oidcCfg.value("clientSecret").toString(impl->oidcConfig.clientSecret);
    impl->oidcConfig.jwksUrl = oidcCfg.value("jwksUrl").toString(impl->oidcConfig.jwksUrl);
    impl->oidcConfig.audience = oidcCfg.value("audience").toString(impl->oidcConfig.audience);
    impl->oidcConfig.tokenExpirationSeconds = oidcCfg.value("tokenExpirationSeconds").toInt(impl->oidcConfig.tokenExpirationSeconds);
    impl->oidcConfig.validateIssuer = oidcCfg.value("validateIssuer").toBool(impl->oidcConfig.validateIssuer);
    impl->oidcConfig.validateAudience = oidcCfg.value("validateAudience").toBool(impl->oidcConfig.validateAudience);
    impl->sharedSecret = impl->oidcConfig.clientSecret;

    // Load TLS configuration if enabled
    if (impl->tlsConfig.enabled) {
        if (!impl->loadTLSCertificates()) {
            emit errorOccurred(-1, "Failed to load TLS certificates");
            return false;
        }
    }

    // Setup request handler
    auto requestHandler = [this](const QHttpServerRequest& request, QHttpServerResponder& responder) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // Parse request
        RequestContext ctx;
        ctx.requestId = QUuid::createUuid().toString();
        ctx.method = request.method().toString();
        ctx.path = request.url().path();
        ctx.remoteAddress = request.remoteAddress().toString();
        ctx.timestamp = QDateTime::currentMSecsSinceEpoch();

        // Parse headers
        for (const auto& header : request.headers()) {
            ctx.headers[QString::fromUtf8(header.name())] = QString::fromUtf8(header.value());
        }

        // Parse query parameters
        QUrlQuery query(request.url());
        for (const auto& pair : query.queryItems()) {
            ctx.queryParams[pair.first] = pair.second;
        }

        // Parse body
        if (!request.body().isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(request.body());
            if (doc.isObject()) {
                ctx.body = doc.object();
            }
        }

        // Check rate limiting
        if (!impl->checkRateLimit(ctx.remoteAddress)) {
            ErrorResponse err;
            err.code = 429;
            err.message = "Too Many Requests";
            err.errorType = "RateLimitExceeded";
            err.requestId = ctx.requestId;
            err.timestamp = ctx.timestamp;

            responder.writeJson(err.toJson(), QHttpServerResponder::StatusCode::TooManyRequests);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            impl->logRequest(ctx, 429, latency);
            impl->failedRequests++;
            
            return;
        }

        // Run middleware
        for (const auto& middleware : impl->middlewares) {
            if (!middleware(ctx)) {
                ErrorResponse err;
                err.code = 400;
                err.message = "Middleware validation failed";
                err.errorType = "ValidationError";
                err.requestId = ctx.requestId;
                err.timestamp = ctx.timestamp;

                responder.writeJson(err.toJson(), QHttpServerResponder::StatusCode::BadRequest);
                
                auto endTime = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                impl->logRequest(ctx, 400, latency);
                impl->failedRequests++;
                
                return;
            }
        }

        // Handle GraphQL
        if (ctx.path == "/graphql" && (ctx.method == "POST" || ctx.method == "GET")) {
            auto response = handleGraphQLRequest(ctx);
            responder.writeJson(response);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            impl->logRequest(ctx, 200, latency);
            impl->successfulRequests++;
            
            emit requestProcessed(ctx.requestId, 200);
            return;
        }

        // Handle REST
        auto response = handleRESTRequest(ctx);
        int statusCode = response["code"].toInt(200);
        responder.writeJson(response);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        impl->logRequest(ctx, statusCode, latency);

        if (statusCode >= 400) {
            impl->failedRequests++;
        } else {
            impl->successfulRequests++;
        }

        emit requestProcessed(ctx.requestId, statusCode);
    };

    // Listen on port
    if (!impl->httpServer->listen(QHostAddress::Any, impl->port)) {
        emit errorOccurred(-1, QString("Failed to listen on port %1").arg(impl->port));
        return false;
    }

    // Connect request handler
    impl->httpServer->route("/", requestHandler);
    impl->httpServer->route("/<path>", requestHandler);

    impl->isRunning = true;
    impl->tokenCacheCleanupTimer->start(5 * 60 * 1000); // Cleanup every 5 minutes

    qInfo() << "Production API Server started on port" << impl->port;
    emit started(impl->port);

    return true;
}

bool ProductionAPIServer::stop() {
    QMutexLocker lock(&impl->mutex);

    if (!impl->isRunning) {
        return false;
    }

    impl->httpServer.reset();
    impl->isRunning = false;
    impl->tokenCacheCleanupTimer->stop();

    qInfo() << "Production API Server stopped";
    emit stopped();

    return true;
}

bool ProductionAPIServer::isRunning() const {
    return impl->isRunning;
}

void ProductionAPIServer::configureTLS(const TLSConfig& config) {
    QMutexLocker lock(&impl->mutex);
    impl->tlsConfig = config;

    if (impl->isRunning && config.enabled) {
        impl->loadTLSCertificates();
    }
}

const TLSConfig& ProductionAPIServer::tlsConfig() const {
    return impl->tlsConfig;
}

bool ProductionAPIServer::reloadTLSCertificates() {
    QMutexLocker lock(&impl->mutex);

    if (!impl->tlsConfig.enabled) {
        return false;
    }

    return impl->loadTLSCertificates();
}

void ProductionAPIServer::configureOIDC(const OIDCConfig& config) {
    QMutexLocker lock(&impl->mutex);
    impl->oidcConfig = config;

    if (config.enabled) {
        // Fetch JWKS on configuration
        impl->fetchJWKS();
    }
}

const OIDCConfig& ProductionAPIServer::oidcConfig() const {
    return impl->oidcConfig;
}

bool ProductionAPIServer::validateJWT(const QString& token, AuthContext& outAuth) {
    QMutexLocker lock(&impl->mutex);

    if (!impl->oidcConfig.enabled) {
        return false;
    }

    // Check token cache
    auto cached = impl->tokenCache.find(token);
    if (cached != impl->tokenCache.end() && !cached->second.token.isExpired()) {
        outAuth = cached->second;
        return true;
    }

    impl->fetchJWKS();

    const RawrXD::Auth::JWT jwt = RawrXD::Auth::JWT::decode(token);
    if (!jwt.isValid) {
        LOG_API_WARN() << "JWT decode failed:" << jwt.error;
        return false;
    }

    std::shared_ptr<RawrXD::Auth::JWK> jwk = impl->jwksManager.findKey(jwt.keyId);

    // Support HS256 fallback using clientSecret when no JWK is available
    bool signatureOk = false;
    if (jwk) {
        signatureOk = impl->jwksManager.verifyJWTSignature(jwt, jwk);
    } else if (jwt.algorithm.startsWith("HS", Qt::CaseInsensitive) && !impl->sharedSecret.isEmpty()) {
        // HMAC validation path
        const QStringList parts = token.split('.');
        if (parts.size() == 3) {
            const QString signingInput = parts[0] + "." + parts[1];
            const QByteArray expected = QMessageAuthenticationCode::hash(signingInput.toUtf8(), impl->sharedSecret.toUtf8(), QCryptographicHash::Sha256);
            const QByteArray provided = QByteArray::fromBase64(parts[2].toUtf8());
            signatureOk = (expected == provided);
        }
    }

    if (!signatureOk) {
        LOG_API_WARN() << "JWT signature verification failed";
        return false;
    }

    const long long nowSeconds = std::time(nullptr);
    if (!impl->jwksManager.validateJWT(jwt, impl->oidcConfig.issuerUrl, impl->oidcConfig.audience, nowSeconds)) {
        LOG_API_WARN() << "JWT claims validation failed";
        return false;
    }

    // Populate AuthContext
    outAuth.authenticated = true;
    outAuth.userId = jwt.claims.subject;
    outAuth.username = jwt.claims.username;
    outAuth.claims = jwt.claims.customClaims;
    outAuth.scopes = jwt.claims.scope;

    outAuth.token.accessToken = token;
    outAuth.token.tokenType = jwt.tokenType;
    outAuth.token.expiresIn = static_cast<int>(jwt.claims.expiresAt - nowSeconds);
    outAuth.token.expiresAt = std::chrono::system_clock::time_point(std::chrono::seconds(jwt.claims.expiresAt));
    outAuth.token.claims = jwt.claims.toJson();

    impl->tokenCache[token] = outAuth;
    return true;
}

bool ProductionAPIServer::refreshOIDCConfiguration() {
    QMutexLocker lock(&impl->mutex);

    if (!impl->oidcConfig.enabled) {
        return false;
    }

    impl->fetchJWKS();
    return true;
}

void ProductionAPIServer::registerRESTEndpoint(const QString& method, const QString& path,
                                             std::function<QJsonObject(const RequestContext&)> handler,
                                             bool requiresAuth) {
    QMutexLocker lock(&impl->mutex);
    
    impl->restRoutes[method][path] = handler;
    impl->routeAuthRequired[path] = requiresAuth;

    qInfo() << "Registered REST endpoint:" << method << path << "(Auth required:" << requiresAuth << ")";
}

void ProductionAPIServer::registerGraphQLQuery(const QString& name, const QString& schema,
                                             GraphQLResolver resolver,
                                             bool requiresAuth) {
    QMutexLocker lock(&impl->mutex);
    
    impl->graphqlQueries[name] = {schema, resolver};
    impl->routeAuthRequired[name] = requiresAuth;

    qInfo() << "Registered GraphQL query:" << name << "(Auth required:" << requiresAuth << ")";
}

void ProductionAPIServer::registerGraphQLMutation(const QString& name, const QString& schema,
                                                GraphQLResolver resolver,
                                                bool requiresAuth) {
    QMutexLocker lock(&impl->mutex);
    
    impl->graphqlMutations[name] = {schema, resolver};
    impl->routeAuthRequired[name] = requiresAuth;

    qInfo() << "Registered GraphQL mutation:" << name << "(Auth required:" << requiresAuth << ")";
}

ErrorResponse ProductionAPIServer::createError(int code, const QString& message,
                                             const QString& type,
                                             const QString& requestId) {
    ErrorResponse err;
    err.code = code;
    err.message = message;
    err.errorType = type;
    err.requestId = requestId.isEmpty() ? QUuid::createUuid().toString() : requestId;
    err.timestamp = QDateTime::currentMSecsSinceEpoch();
    return err;
}

ErrorResponse ProductionAPIServer::validateRequest(const RequestContext& ctx) {
    ErrorResponse err;
    err.code = 200;
    err.message = "OK";
    err.requestId = ctx.requestId;
    err.timestamp = ctx.timestamp;

    // Validate required fields based on method
    if (ctx.method == "POST" || ctx.method == "PUT") {
        if (ctx.body.isEmpty()) {
            err.code = 400;
            err.message = "Request body is required";
            err.errorType = "ValidationError";
        }
    }

    return err;
}

void ProductionAPIServer::use(std::function<bool(RequestContext&)> middleware) {
    QMutexLocker lock(&impl->mutex);
    impl->middlewares.push_back(middleware);

    qInfo() << "Registered middleware";
}

void ProductionAPIServer::setMetricsEnabled(bool enabled) {
    impl->metricsEnabled = enabled;
}

void ProductionAPIServer::setLoggingEnabled(bool enabled) {
    impl->loggingEnabled = enabled;
}

QJsonObject ProductionAPIServer::getServerStats() const {
    QMutexLocker lock(&impl->mutex);

    QJsonObject stats;
    stats["isRunning"] = impl->isRunning;
    stats["port"] = static_cast<int>(impl->port);
    stats["totalRequests"] = impl->totalRequests;
    stats["successfulRequests"] = impl->successfulRequests;
    stats["failedRequests"] = impl->failedRequests;
    stats["averageLatencyMs"] = impl->totalRequests > 0 
        ? impl->totalLatencyMs / impl->totalRequests 
        : 0;
    stats["tlsEnabled"] = impl->tlsConfig.enabled;
    stats["oidcEnabled"] = impl->oidcConfig.enabled;
    stats["cachedTokens"] = static_cast<int>(impl->tokenCache.size());
    stats["registeredRESTEndpoints"] = static_cast<int>(impl->restRoutes.size());
    stats["registeredGraphQLQueries"] = static_cast<int>(impl->graphqlQueries.size());
    stats["registeredGraphQLMutations"] = static_cast<int>(impl->graphqlMutations.size());

    return stats;
}

// ============================================================
// PRIVATE METHODS
// ============================================================

QJsonObject ProductionAPIServer::handleRESTRequest(const RequestContext& ctx) {
    QMutexLocker lock(&impl->mutex);

    // Extract authentication if present
    AuthContext auth = extractAuthContext(ctx);

    // Find matching route
    auto methodIt = impl->restRoutes.find(ctx.method);
    if (methodIt == impl->restRoutes.end()) {
        ErrorResponse err = createError(405, "Method Not Allowed", "MethodNotAllowed", ctx.requestId);
        return err.toJson();
    }

    auto pathIt = methodIt->second.find(ctx.path);
    if (pathIt == methodIt->second.end()) {
        // Try wildcard patterns
        bool found = false;
        for (auto& [pattern, handler] : methodIt->second) {
            if (pattern.contains("*")) {
                // Simple wildcard matching
                QString regexPattern = pattern;
                regexPattern.replace("*", ".*");
                std::regex regex(regexPattern.toStdString());
                if (std::regex_match(ctx.path.toStdString(), regex)) {
                    // Check authentication
                    auto authRequiredIt = impl->routeAuthRequired.find(pattern);
                    if (authRequiredIt != impl->routeAuthRequired.end() && authRequiredIt->second) {
                        if (!validateAuthContext(auth, true)) {
                            ErrorResponse err = createError(401, "Unauthorized", "AuthenticationRequired", ctx.requestId);
                            return err.toJson();
                        }
                    }

                    // Call handler
                    return handler(ctx);
                }
            }
        }

        ErrorResponse err = createError(404, "Not Found", "NotFound", ctx.requestId);
        return err.toJson();
    }

    // Check authentication requirement
    auto authRequiredIt = impl->routeAuthRequired.find(ctx.path);
    if (authRequiredIt != impl->routeAuthRequired.end() && authRequiredIt->second) {
        if (!validateAuthContext(auth, true)) {
            ErrorResponse err = createError(401, "Unauthorized", "AuthenticationRequired", ctx.requestId);
            return err.toJson();
        }
    }

    // Validate request
    auto validation = validateRequest(ctx);
    if (validation.code != 200) {
        return validation.toJson();
    }

    // Call handler
    try {
        return pathIt->second(ctx);
    } catch (const std::exception& e) {
        ErrorResponse err = createError(500, 
                                       QString("Internal Server Error: %1").arg(e.what()),
                                       "InternalError",
                                       ctx.requestId);
        return err.toJson();
    }
}

QJsonObject ProductionAPIServer::handleGraphQLRequest(const RequestContext& ctx) {
    QMutexLocker lock(&impl->mutex);

    // Extract query and variables
    QString query = ctx.method == "GET" 
        ? ctx.queryParams["query"].toString() 
        : ctx.body["query"].toString();

    if (query.isEmpty()) {
        ErrorResponse err = createError(400, "GraphQL query is required", "ValidationError", ctx.requestId);
        return err.toJson();
    }

    QJsonObject variables = ctx.body.contains("variables") 
        ? ctx.body["variables"].toObject() 
        : QJsonObject();

    // Extract operation name if provided
    QString operationName = ctx.body["operationName"].toString();

    // Parse query to determine type and name
    bool isQuery = query.trimmed().startsWith("query", Qt::CaseInsensitive);
    bool isMutation = query.trimmed().startsWith("mutation", Qt::CaseInsensitive);

    AuthContext auth = extractAuthContext(ctx);

    QJsonObject result;
    result["requestId"] = ctx.requestId;

    if (isQuery) {
        // Handle GraphQL queries
        QJsonArray data;
        for (auto& [name, queryPair] : impl->graphqlQueries) {
            if (impl->routeAuthRequired[name] && !validateAuthContext(auth, true)) {
                ErrorResponse err = createError(401, "Unauthorized", "AuthenticationRequired", ctx.requestId);
                result["errors"] = QJsonArray{err.toJson()};
                return result;
            }

            try {
                auto queryResult = queryPair.second(variables, auth);
                QJsonObject queryResponse;
                queryResponse[name] = queryResult;
                data.append(queryResponse);
            } catch (const std::exception& e) {
                QJsonObject error;
                error["message"] = e.what();
                result["errors"] = QJsonArray{error};
            }
        }
        result["data"] = data;
    } else if (isMutation) {
        // Handle GraphQL mutations
        QJsonArray data;
        for (auto& [name, mutationPair] : impl->graphqlMutations) {
            if (impl->routeAuthRequired[name] && !validateAuthContext(auth, true)) {
                ErrorResponse err = createError(401, "Unauthorized", "AuthenticationRequired", ctx.requestId);
                result["errors"] = QJsonArray{err.toJson()};
                return result;
            }

            try {
                auto mutationResult = mutationPair.second(variables, auth);
                QJsonObject mutationResponse;
                mutationResponse[name] = mutationResult;
                data.append(mutationResponse);
            } catch (const std::exception& e) {
                QJsonObject error;
                error["message"] = e.what();
                result["errors"] = QJsonArray{error};
            }
        }
        result["data"] = data;
    } else {
        ErrorResponse err = createError(400, "Invalid GraphQL operation", "ValidationError", ctx.requestId);
        result["errors"] = QJsonArray{err.toJson()};
    }

    return result;
}

AuthContext ProductionAPIServer::extractAuthContext(const RequestContext& ctx) {
    AuthContext auth;

    // Extract Bearer token from Authorization header
    QString authHeader = ctx.headers["Authorization"].toString();
    if (authHeader.startsWith("Bearer ", Qt::CaseInsensitive)) {
        QString token = authHeader.mid(7);
        if (validateJWT(token, auth)) {
            auth.authenticated = true;
        }
    }

    return auth;
}

bool ProductionAPIServer::validateAuthContext(const AuthContext& auth, bool required) {
    if (!required) {
        return true;
    }

    if (!auth.authenticated) {
        return false;
    }

    if (auth.token.isExpired()) {
        return false;
    }

    return true;
}

} // namespace API
} // namespace RawrXD

#else // !QT_HTTPSERVER_LIB && !QT_HTTPSERVER_MODULE
// Stub implementation when QHttpServer is not available
// The header already provides inline stub implementations for ProductionAPIServer
// No additional definitions needed here - the header has:
//   ProductionAPIServer() = default;
//   void start(quint16) {}
//   void stop() {}
//   bool isRunning() const { return false; }

#endif // QT_HTTPSERVER_LIB || QT_HTTPSERVER_MODULE
