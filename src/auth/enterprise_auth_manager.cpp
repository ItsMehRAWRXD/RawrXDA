#include "enterprise_auth_manager.h"
EnterpriseAuthManager::EnterpriseAuthManager()
    
    , m_authenticated(false)
{
}

EnterpriseAuthManager::~EnterpriseAuthManager()
{
}

bool EnterpriseAuthManager::loadConfig(const std::string &configPath)
{
    // File operation removed;
    if (!configFile.open(std::iostream::ReadOnly)) {
        return false;
    }

    void* doc = void*::fromJson(configFile.readAll());
    configFile.close();

    if (!doc.isObject()) {
        return false;
    }

    void* obj = doc.object();
    m_provider = obj.value("provider").toString();
    m_clientId = obj.value("client_id").toString();
    m_jwksUrl = obj.value("jwks_url").toString();


    // Fetch public keys from JWKS endpoint
    if (!m_jwksUrl.empty()) {
        return fetchPublicKeys();
    }

    return true;
}

bool EnterpriseAuthManager::authenticateWithToken(const std::string &bearerToken)
{
    // Validate the JWT token
    if (!validateToken(bearerToken)) {
        authenticationFailed("Invalid token");
        return false;
    }

    // Extract UPN from token claims
    m_userUPN = extractUPN(bearerToken);
    if (m_userUPN.empty()) {
        authenticationFailed("Failed to extract UPN from token");
        return false;
    }

    m_authenticated = true;
    authenticationSucceeded(m_userUPN);
    return true;
}

std::string EnterpriseAuthManager::getUserUPN() const
{
    return m_userUPN;
}

std::string EnterpriseAuthManager::getSettingsFolderPath() const
{
    std::string basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!m_userUPN.empty()) {
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
    
    // Simplified for this example
    return true;
}

bool EnterpriseAuthManager::validateToken(const std::string &token)
{
    // In a real implementation, this would:
    // 1. Decode the JWT header and payload
    // 2. Verify the signature using the public key from JWKS
    // 3. Check token expiration
    // 4. Validate token claims
    
    // Simplified for this example
    return !token.empty();
}

std::string EnterpriseAuthManager::extractUPN(const std::string &token)
{
    // In a real implementation, this would:
    // 1. Decode the JWT payload (second part)
    // 2. Base64 decode it
    // 3. Parse as JSON
    // 4. Extract the "upn" or "preferred_username" claim
    
    // Simplified for this example
    return "user@example.com";
}

