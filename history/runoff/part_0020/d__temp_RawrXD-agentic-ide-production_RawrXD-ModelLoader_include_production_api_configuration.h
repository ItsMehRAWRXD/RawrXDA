#pragma once
#if defined(QT_HTTPSERVER_LIB) || defined(QT_HTTPSERVER_MODULE)


#include "production_api_server.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <memory>

// ============================================================
// PRODUCTION API SERVER CONFIGURATION & INITIALIZATION
// ============================================================

namespace RawrXD {
namespace API {

/**
 * Configuration file handler for production API server
 */
class ProductionAPIConfiguration {
public:
    /**
     * Load configuration from JSON file
     */
    static std::unique_ptr<ProductionAPIServer> loadFromFile(const QString& configPath) {
        auto server = std::make_unique<ProductionAPIServer>();

        // Load config file
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open config file:" << configPath;
            return server;
        }

        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        configFile.close();

        if (!doc.isObject()) {
            qWarning() << "Invalid config file format";
            return server;
        }

        return loadFromJson(server.get(), doc.object());
    }

    /**
     * Load configuration from JSON object
     */
    static std::unique_ptr<ProductionAPIServer> loadFromJson(const QJsonObject& config) {
        auto server = std::make_unique<ProductionAPIServer>();
        loadFromJson(server.get(), config);
        return server;
    }

    /**
     * Create a default production server configuration
     */
    static QJsonObject createDefaultConfig() {
        QJsonObject config;

        // Server configuration
        QJsonObject serverConfig;
        serverConfig["port"] = 8443;
        serverConfig["logRequests"] = true;
        serverConfig["metricsEnabled"] = true;
        config["server"] = serverConfig;

        // TLS configuration
        QJsonObject tlsConfig;
        tlsConfig["enabled"] = true;
        tlsConfig["certificatePath"] = "/etc/rawrxd/certs/server.crt";
        tlsConfig["privateKeyPath"] = "/etc/rawrxd/certs/server.key";
        tlsConfig["caChainPath"] = "/etc/rawrxd/certs/ca-chain.crt";
        tlsConfig["minTLSVersion"] = 12;
        QJsonArray ciphers;
        ciphers.append("ECDHE-RSA-AES256-GCM-SHA384");
        ciphers.append("ECDHE-RSA-AES128-GCM-SHA256");
        ciphers.append("DHE-RSA-AES256-GCM-SHA384");
        tlsConfig["cipherSuites"] = ciphers;
        config["tls"] = tlsConfig;

        // OIDC configuration
        QJsonObject oidcConfig;
        oidcConfig["enabled"] = true;
        oidcConfig["issuerUrl"] = "https://auth.example.com";
        oidcConfig["clientId"] = "rawrxd-app";
        oidcConfig["clientSecret"] = "${OIDC_CLIENT_SECRET}";
        oidcConfig["jwksUrl"] = "https://auth.example.com/.well-known/jwks.json";
        oidcConfig["audience"] = "https://api.example.com";
        oidcConfig["tokenExpirationSeconds"] = 3600;
        oidcConfig["validateIssuer"] = true;
        oidcConfig["validateAudience"] = true;
        config["oidc"] = oidcConfig;

        // REST endpoints
        QJsonArray restEndpoints;
        QJsonObject healthEndpoint;
        healthEndpoint["method"] = "GET";
        healthEndpoint["path"] = "/health";
        healthEndpoint["requiresAuth"] = false;
        restEndpoints.append(healthEndpoint);

        QJsonObject statusEndpoint;
        statusEndpoint["method"] = "GET";
        statusEndpoint["path"] = "/status";
        statusEndpoint["requiresAuth"] = false;
        restEndpoints.append(statusEndpoint);

        config["restEndpoints"] = restEndpoints;

        // GraphQL
        QJsonObject graphqlConfig;
        graphqlConfig["enabled"] = true;
        graphqlConfig["endpoint"] = "/graphql";
        graphqlConfig["requiresAuth"] = true;
        config["graphql"] = graphqlConfig;

        // Rate limiting
        QJsonObject rateLimitConfig;
        rateLimitConfig["enabled"] = true;
        rateLimitConfig["requestsPerMinute"] = 1000;
        config["rateLimit"] = rateLimitConfig;

        // Security headers
        QJsonObject securityHeaders;
        securityHeaders["x-content-type-options"] = "nosniff";
        securityHeaders["x-frame-options"] = "DENY";
        securityHeaders["x-xss-protection"] = "1; mode=block";
        securityHeaders["strict-transport-security"] = "max-age=31536000; includeSubDomains";
        config["securityHeaders"] = securityHeaders;

        return config;
    }

private:
    static void loadFromJson(ProductionAPIServer* server, const QJsonObject& config) {
#ifdef QT_HTTPSERVER_LIB
        // Configure TLS
        if (config.contains("tls")) {
            QJsonObject tlsObj = config["tls"].toObject();
            TLSConfig tls;
            tls.enabled = tlsObj["enabled"].toBool(false);
            tls.certificatePath = tlsObj["certificatePath"].toString();
            tls.privateKeyPath = tlsObj["privateKeyPath"].toString();
            tls.caChainPath = tlsObj["caChainPath"].toString();
            tls.minTLSVersion = tlsObj["minTLSVersion"].toInt(12);

            QJsonArray cipherArray = tlsObj["cipherSuites"].toArray();
            for (const auto& cipher : cipherArray) {
                tls.cipherSuites.push_back(cipher.toString());
            }

            server->configureTLS(tls);
        }

        // Configure OIDC
        if (config.contains("oidc")) {
            QJsonObject oidcObj = config["oidc"].toObject();
            OIDCConfig oidc;
            oidc.enabled = oidcObj["enabled"].toBool(false);
            oidc.issuerUrl = oidcObj["issuerUrl"].toString();
            oidc.clientId = oidcObj["clientId"].toString();
            oidc.clientSecret = oidcObj["clientSecret"].toString();
            oidc.jwksUrl = oidcObj["jwksUrl"].toString();
            oidc.audience = oidcObj["audience"].toString();
            oidc.tokenExpirationSeconds = oidcObj["tokenExpirationSeconds"].toInt(3600);
            oidc.validateIssuer = oidcObj["validateIssuer"].toBool(true);
            oidc.validateAudience = oidcObj["validateAudience"].toBool(true);

            server->configureOIDC(oidc);
        }

        // Configure server settings
        if (config.contains("server")) {
            QJsonObject serverObj = config["server"].toObject();
            server->setLoggingEnabled(serverObj["logRequests"].toBool(true));
            server->setMetricsEnabled(serverObj["metricsEnabled"].toBool(true));
        }
#endif // QT_HTTPSERVER_LIB
    }
};

} // namespace API
} // namespace RawrXD
#endif // QT_HTTPSERVER_LIB

