#include "enterprise_auth_manager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

EnterpriseAuthManager::EnterpriseAuthManager(QObject *parent)
    : QObject(parent)
    , m_authenticated(false)
{
}

EnterpriseAuthManager::~EnterpriseAuthManager()
{
}

bool EnterpriseAuthManager::loadConfig(const QString &configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open enterprise config:" << configPath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    configFile.close();

    if (!doc.isObject()) {
        qWarning() << "Invalid enterprise config JSON";
        return false;
    }

    QJsonObject obj = doc.object();
    m_provider = obj.value("provider").toString();
    m_clientId = obj.value("client_id").toString();
    m_jwksUrl = obj.value("jwks_url").toString();

    qDebug() << "Loaded enterprise config:" << m_provider << m_clientId;

    // Fetch public keys from JWKS endpoint
    if (!m_jwksUrl.isEmpty()) {
        return fetchPublicKeys();
    }

    return true;
}

bool EnterpriseAuthManager::authenticateWithToken(const QString &bearerToken)
{
    // Validate the JWT token
    if (!validateToken(bearerToken)) {
        emit authenticationFailed("Invalid token");
        return false;
    }

    // Extract UPN from token claims
    m_userUPN = extractUPN(bearerToken);
    if (m_userUPN.isEmpty()) {
        emit authenticationFailed("Failed to extract UPN from token");
        return false;
    }

    m_authenticated = true;
    qInfo() << "User authenticated:" << m_userUPN;
    emit authenticationSucceeded(m_userUPN);
    return true;
}

QString EnterpriseAuthManager::getUserUPN() const
{
    return m_userUPN;
}

QString EnterpriseAuthManager::getSettingsFolderPath() const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!m_userUPN.isEmpty()) {
        basePath = basePath + "/" + m_userUPN;
    }
    return basePath;
}

bool EnterpriseAuthManager::isAuthenticated() const
{
    return m_authenticated;
}

bool EnterpriseAuthManager::fetchPublicKeys()
{
    // In a real implementation, this would:
    // 1. Make an HTTP GET request to m_jwksUrl
    // 2. Parse the JWKS response
    // 3. Cache the public keys for token validation
    
    qDebug() << "Fetching public keys from:" << m_jwksUrl;
    // Simplified for this example
    return true;
}

bool EnterpriseAuthManager::validateToken(const QString &token)
{
    // In a real implementation, this would:
    // 1. Decode the JWT header and payload
    // 2. Verify the signature using the public key from JWKS
    // 3. Check token expiration
    // 4. Validate token claims
    
    qDebug() << "Validating JWT token...";
    // Simplified for this example
    return !token.isEmpty();
}

QString EnterpriseAuthManager::extractUPN(const QString &token)
{
    // In a real implementation, this would:
    // 1. Decode the JWT payload (second part)
    // 2. Base64 decode it
    // 3. Parse as JSON
    // 4. Extract the "upn" or "preferred_username" claim
    
    // Simplified for this example
    return "user@example.com";
}