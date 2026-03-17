#include "jwt_validator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonWebToken>
#include <QDebug>

JWTValidator::JWTValidator(QObject *parent)
    : QObject(parent)
{
}

JWTValidator::~JWTValidator()
{
}

void JWTValidator::setHS256Secret(const QString &secret)
{
    m_hs256Secret = secret;
}

void JWTValidator::setRS256PublicKey(const QString &publicKey)
{
    m_rs256PublicKey = publicKey;
}

bool JWTValidator::validateToken(const QString &token)
{
    // Clear previous claims
    m_claims.clear();
    
    // Try HS256 validation first
    if (!m_hs256Secret.isEmpty() && validateHS256(token)) {
        return true;
    }
    
    // Try RS256 validation
    if (!m_rs256PublicKey.isEmpty() && validateRS256(token)) {
        return true;
    }
    
    return false;
}

QMap<QString, QVariant> JWTValidator::getClaims() const
{
    return m_claims;
}

bool JWTValidator::validateHS256(const QString &token)
{
    QJsonWebToken jwt = QJsonWebToken::fromTokenAndSecret(token, m_hs256Secret);
    if (jwt.isValid() && jwt.verifySignature()) {
        // Extract claims
        QJsonDocument doc = QJsonDocument::fromJson(jwt.getPayload().toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                m_claims[it.key()] = it.value().toVariant();
            }
            return true;
        }
    }
    return false;
}

bool JWTValidator::validateRS256(const QString &token)
{
    QJsonWebToken jwt = QJsonWebToken::fromTokenAndSecret(token, m_rs256PublicKey);
    jwt.setAlgorithm(QJsonWebToken::RS256);
    if (jwt.isValid() && jwt.verifySignature()) {
        // Extract claims
        QJsonDocument doc = QJsonDocument::fromJson(jwt.getPayload().toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                m_claims[it.key()] = it.value().toVariant();
            }
            return true;
        }
    }
    return false;
}