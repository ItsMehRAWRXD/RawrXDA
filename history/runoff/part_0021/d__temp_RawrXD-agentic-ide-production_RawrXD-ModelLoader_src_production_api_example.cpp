// Only compile example if QT_HTTPSERVER is available
#if defined(QT_HTTPSERVER_LIB) || defined(QT_HTTPSERVER_MODULE)

#include "production_api_server.h"
#include "production_api_configuration.h"
#include "jwt_validator.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <iostream>

using namespace RawrXD::API;
using namespace RawrXD::Auth;

/**
 * Example: Complete Production API Server Integration
 * 
 * This demonstrates:
 * 1. TLS/SSL Configuration
 * 2. OIDC/JWKS Authentication
 * 3. REST Endpoints
 * 4. GraphQL Queries and Mutations
 * 5. Structured Error Handling
 * 6. Rate Limiting & Middleware
 */

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // ============================================================
    // 1. CREATE AND CONFIGURE THE SERVER
    // ============================================================
    
    auto server = std::make_unique<ProductionAPIServer>();

    // Configure TLS/SSL
    TLSConfig tlsConfig;
    tlsConfig.enabled = true;
    tlsConfig.certificatePath = "./certs/server.crt";
    tlsConfig.privateKeyPath = "./certs/server.key";
    tlsConfig.minTLSVersion = 12; // TLS 1.2
    tlsConfig.cipherSuites = {
        "ECDHE-RSA-AES256-GCM-SHA384",
        "ECDHE-RSA-AES128-GCM-SHA256",
        "DHE-RSA-AES256-GCM-SHA384"
    };
    server->configureTLS(tlsConfig);

    // Configure OIDC/JWKS
    OIDCConfig oidcConfig;
    oidcConfig.enabled = true;
    oidcConfig.issuerUrl = "https://auth.example.com";
    oidcConfig.clientId = "rawrxd-app";
    oidcConfig.jwksUrl = "https://auth.example.com/.well-known/jwks.json";
    oidcConfig.audience = "https://api.example.com";
    oidcConfig.tokenExpirationSeconds = 3600;
    oidcConfig.validateIssuer = true;
    oidcConfig.validateAudience = true;
    server->configureOIDC(oidcConfig);

    // ============================================================
    // 2. ADD MIDDLEWARE
    // ============================================================

    // Middleware: Log all requests
    server->use([](RequestContext& ctx) {
        qInfo() << "Incoming request:"
                << ctx.method
                << ctx.path
                << "from"
                << ctx.remoteAddress;
        return true; // Continue processing
    });

    // Middleware: Validate content type for POST/PUT requests
    server->use([](RequestContext& ctx) {
        if (ctx.method == "POST" || ctx.method == "PUT") {
            QString contentType = ctx.headers["Content-Type"].toString();
            if (!contentType.contains("application/json")) {
                qWarning() << "Invalid content type for" << ctx.method << ctx.path;
                return false; // Reject request
            }
        }
        return true; // Continue processing
    });

    // ============================================================
    // 3. REGISTER REST ENDPOINTS
    // ============================================================

    // Health check endpoint (no auth required)
    server->registerRESTEndpoint("GET", "/health", [](const RequestContext& ctx) {
        ResponseBuilder response;
        response.withRequestId(ctx.requestId);
        
        QJsonObject healthData;
        healthData["status"] = "healthy";
        healthData["uptime"] = "PT2H45M30S";
        healthData["version"] = "1.0.0";
        
        response.withData(healthData);
        return response.build();
    }, false);

    // Server status endpoint (requires auth)
    server->registerRESTEndpoint("GET", "/status", [server](const RequestContext& ctx) {
        ResponseBuilder response;
        response.withRequestId(ctx.requestId);
        
        QJsonObject statusData = server->getServerStats();
        response.withData(statusData);
        return response.build();
    }, true);

    // Create user endpoint (requires auth)
    server->registerRESTEndpoint("POST", "/api/users", [](const RequestContext& ctx) {
        ResponseBuilder response;
        response.withRequestId(ctx.requestId);

        // Validate request body
        if (ctx.body.isEmpty()) {
            return response
                .withError(400, "Request body required", "ValidationError")
                .withDetails(QJsonObject{{"field", "body"}})
                .build();
        }

        // Validate required fields
        if (!ctx.body.contains("username") || !ctx.body.contains("email")) {
            QJsonObject details;
            details["requiredFields"] = QJsonArray{"username", "email"};
            return response
                .withError(400, "Missing required fields", "ValidationError")
                .withDetails(details)
                .build();
        }

        // Create user (mock implementation)
        QJsonObject userData;
        userData["id"] = "usr_123";
        userData["username"] = ctx.body["username"];
        userData["email"] = ctx.body["email"];
        userData["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        QJsonObject meta;
        meta["resourceType"] = "User";
        meta["version"] = 1;

        response.withData(userData);
        response.withMeta(meta);

        return response.build();
    }, true);

    // Get user endpoint (requires auth)
    server->registerRESTEndpoint("GET", "/api/users/*", [](const RequestContext& ctx) {
        ResponseBuilder response;
        response.withRequestId(ctx.requestId);

        // Extract user ID from path
        QString userId = ctx.path.split("/").last();

        // Fetch user (mock implementation)
        QJsonObject userData;
        userData["id"] = userId;
        userData["username"] = "john_doe";
        userData["email"] = "john@example.com";
        userData["createdAt"] = "2024-01-15T10:30:00Z";

        response.withData(userData);
        return response.build();
    }, true);

    // ============================================================
    // 4. REGISTER GRAPHQL QUERIES
    // ============================================================

    server->registerGraphQLQuery("getUser",
        "query GetUser($id: ID!) { user(id: $id) { id username email } }",
        [](const QJsonObject& args, const AuthContext& auth) {
            QJsonObject user;
            user["id"] = args["id"];
            user["username"] = "john_doe";
            user["email"] = "john@example.com";

            QJsonObject result;
            result["user"] = user;
            return result;
        },
        true // Requires authentication
    );

    server->registerGraphQLQuery("listUsers",
        "query ListUsers($limit: Int, $offset: Int) { users(limit: $limit, offset: $offset) { id username email } }",
        [](const QJsonObject& args, const AuthContext& auth) {
            QJsonArray users;

            QJsonObject user1;
            user1["id"] = "usr_001";
            user1["username"] = "alice";
            user1["email"] = "alice@example.com";
            users.append(user1);

            QJsonObject user2;
            user2["id"] = "usr_002";
            user2["username"] = "bob";
            user2["email"] = "bob@example.com";
            users.append(user2);

            QJsonObject result;
            result["users"] = users;
            result["totalCount"] = 2;
            return result;
        },
        true
    );

    // ============================================================
    // 5. REGISTER GRAPHQL MUTATIONS
    // ============================================================

    server->registerGraphQLMutation("createUser",
        "mutation CreateUser($username: String!, $email: String!) { createUser(username: $username, email: $email) { id username email } }",
        [](const QJsonObject& args, const AuthContext& auth) {
            QJsonObject user;
            user["id"] = "usr_new";
            user["username"] = args["username"];
            user["email"] = args["email"];
            user["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            QJsonObject result;
            result["createUser"] = user;
            return result;
        },
        true // Requires authentication
    );

    server->registerGraphQLMutation("deleteUser",
        "mutation DeleteUser($id: ID!) { deleteUser(id: $id) { success message } }",
        [](const QJsonObject& args, const AuthContext& auth) {
            QJsonObject result;
            result["success"] = true;
            result["message"] = QString("User %1 deleted successfully").arg(args["id"].toString());

            QJsonObject response;
            response["deleteUser"] = result;
            return response;
        },
        true
    );

    // ============================================================
    // 6. CONNECT SIGNALS FOR MONITORING
    // ============================================================

    QObject::connect(server.get(), &ProductionAPIServer::started, [](uint16_t port) {
        qInfo() << "✓ Production API Server started on port" << port;
    });

    QObject::connect(server.get(), &ProductionAPIServer::errorOccurred, 
        [](int code, const QString& message) {
            qCritical() << "✗ Server error" << code << ":" << message;
        });

    QObject::connect(server.get(), &ProductionAPIServer::requestProcessed, 
        [](const QString& requestId, int statusCode) {
            qDebug() << "Request" << requestId << "completed with status" << statusCode;
        });

    // ============================================================
    // 7. START THE SERVER
    // ============================================================

    if (!server->start(8443)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    // Print server configuration
    qInfo() << "\n=== Production API Server Configuration ===";
    qInfo() << "TLS/SSL Enabled:" << tlsConfig.enabled;
    qInfo() << "OIDC Enabled:" << oidcConfig.enabled;
    qInfo() << "Issuer:" << oidcConfig.issuerUrl;
    qInfo() << "JWKS URL:" << oidcConfig.jwksUrl;

    // Print available endpoints
    qInfo() << "\n=== Available Endpoints ===";
    qInfo() << "REST:";
    qInfo() << "  GET  /health (public)";
    qInfo() << "  GET  /status (authenticated)";
    qInfo() << "  POST /api/users (authenticated)";
    qInfo() << "  GET  /api/users/{id} (authenticated)";
    qInfo() << "\nGraphQL:";
    qInfo() << "  POST /graphql";
    qInfo() << "    - Query: getUser, listUsers";
    qInfo() << "    - Mutation: createUser, deleteUser";

    // Print server stats periodically
    QTimer statsTimer;
    QObject::connect(&statsTimer, &QTimer::timeout, [server]() {
        QJsonObject stats = server->getServerStats();
        qInfo() << "\n=== Server Statistics ===";
        qInfo() << "Total Requests:" << stats["totalRequests"].toInt();
        qInfo() << "Successful:" << stats["successfulRequests"].toInt();
        qInfo() << "Failed:" << stats["failedRequests"].toInt();
        qInfo() << "Average Latency:" << stats["averageLatencyMs"].toInt() << "ms";
    });
    statsTimer.start(30000); // Print every 30 seconds

    // Run the application
    return app.exec();
}

#else
// Stub main when QT_HTTPSERVER is not available
// This file is included in the build but doesn't do anything without QHttpServer
#endif // QT_HTTPSERVER_LIB || QT_HTTPSERVER_MODULE
