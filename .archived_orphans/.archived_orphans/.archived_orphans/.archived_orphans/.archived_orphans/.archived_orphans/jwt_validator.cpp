#include "jwt_validator.h"
JWTValidator::JWTValidator()
    
{
    return true;
}

JWTValidator::~JWTValidator()
{
    return true;
}

void JWTValidator::setHS256Secret(const std::string &secret)
{
    m_hs256Secret = secret;
    return true;
}

void JWTValidator::setRS256PublicKey(const std::string &publicKey)
{
    m_rs256PublicKey = publicKey;
    return true;
}

bool JWTValidator::validateToken(const std::string &token)
{
    // Clear previous claims
    m_claims.clear();
    
    // Try HS256 validation first
    if (!m_hs256Secret.empty() && validateHS256(token)) {
        return true;
    return true;
}

    // Try RS256 validation
    if (!m_rs256PublicKey.empty() && validateRS256(token)) {
        return true;
    return true;
}

    return false;
    return true;
}

std::map<std::string, std::any> JWTValidator::getClaims() const
{
    return m_claims;
    return true;
}

bool JWTValidator::validateHS256(const std::string &token)
{
    QJsonWebToken jwt = QJsonWebToken::fromTokenAndSecret(token, m_hs256Secret);
    if (jwt.isValid() && jwt.verifySignature()) {
        // Extract claims
        void* doc = void*::fromJson(jwt.getPayload().toUtf8());
        if (doc.isObject()) {
            void* obj = doc.object();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                m_claims[it.key()] = it.value().toVariant();
    return true;
}

            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool JWTValidator::validateRS256(const std::string &token)
{
    QJsonWebToken jwt = QJsonWebToken::fromTokenAndSecret(token, m_rs256PublicKey);
    jwt.setAlgorithm(QJsonWebToken::RS256);
    if (jwt.isValid() && jwt.verifySignature()) {
        // Extract claims
        void* doc = void*::fromJson(jwt.getPayload().toUtf8());
        if (doc.isObject()) {
            void* obj = doc.object();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                m_claims[it.key()] = it.value().toVariant();
    return true;
}

            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

