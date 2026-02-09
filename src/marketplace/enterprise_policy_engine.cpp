#include "marketplace/enterprise_policy_engine.h"
#include <QDateTime>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDebug>

EnterprisePolicyEngine::EnterprisePolicyEngine(QObject* parent)
    : QObject(parent)
    , m_compliant(true)
{
    m_settings.requireSignature = false;
    qDebug() << "[EnterprisePolicyEngine] Initialized";
}

EnterprisePolicyEngine::~EnterprisePolicyEngine() {
    // Save audit log if needed
}

void EnterprisePolicyEngine::setAllowList(const QStringList& extensionIds) {
    m_settings.allowList = extensionIds;
}

void EnterprisePolicyEngine::setDenyList(const QStringList& extensionIds) {
    m_settings.denyList = extensionIds;
}

void EnterprisePolicyEngine::setRequireSignature(bool require) {
    m_settings.requireSignature = require;
}

void EnterprisePolicyEngine::setJwtSecret(const QString& secret) {
    m_settings.jwtSecret = secret;
}

bool EnterprisePolicyEngine::isExtensionAllowed(const QString& extensionId) {
    // Check deny list first
    if (checkDenyList(extensionId)) {
        addToAuditLog("system", extensionId, "block", "Extension in deny list");
        emit policyViolation(extensionId, "Extension is in deny list");
        return false;
    }
    
    // Check allow list if it's not empty
    if (!m_settings.allowList.isEmpty()) {
        if (!checkAllowList(extensionId)) {
            addToAuditLog("system", extensionId, "block", "Extension not in allow list");
            emit policyViolation(extensionId, "Extension not in allow list");
            return false;
        }
    }
    
    // If we get here, the extension is allowed
    return true;
}

bool EnterprisePolicyEngine::verifyExtensionSignature(const QString& extensionId, const QString& signature) {
    // In a real implementation, this would verify the digital signature of the extension
    // For now, we'll just return true if signature verification is not required
    if (!m_settings.requireSignature) {
        return true;
    }
    
    // Simulate signature verification
    qDebug() << "[EnterprisePolicyEngine] Verifying signature for:" << extensionId;
    return !signature.isEmpty(); // Simple check for demo
}

bool EnterprisePolicyEngine::validateUserAccess(const QString& userId, const QString& jwtToken) {
    if (!isJwtValid(jwtToken)) {
        return false;
    }
    
    // In a real implementation, this would check the user's permissions
    // For now, we'll just assume valid JWT means access is granted
    return true;
}

void EnterprisePolicyEngine::logExtensionInstallation(const QString& extensionId, const QString& userId) {
    addToAuditLog(userId, extensionId, "install", "Extension installed");
}

void EnterprisePolicyEngine::logExtensionUninstallation(const QString& extensionId, const QString& userId) {
    addToAuditLog(userId, extensionId, "uninstall", "Extension uninstalled");
}

QList<QJsonObject> EnterprisePolicyEngine::getAuditLog(int limit) {
    QList<QJsonObject> result;
    
    int start = qMax(0, m_auditLog.size() - limit);
    for (int i = start; i < m_auditLog.size(); ++i) {
        const AuditEntry& entry = m_auditLog[i];
        QJsonObject obj;
        obj["timestamp"] = entry.timestamp;
        obj["userId"] = entry.userId;
        obj["extensionId"] = entry.extensionId;
        obj["action"] = entry.action;
        obj["details"] = entry.details;
        result.append(obj);
    }
    
    return result;
}

bool EnterprisePolicyEngine::isJwtValid(const QString& token) {
    if (m_settings.jwtSecret.isEmpty()) {
        return true; // No secret configured, assume valid
    }
    
    return verifyJwtSignature(token);
}

QString EnterprisePolicyEngine::generateJwtToken(const QString& userId, const QStringList& permissions) {
    // Generate a real JWT (HS256) using the configured secret
    // JWT = base64url(header) + "." + base64url(payload) + "." + base64url(signature)

    auto base64url = [](const QByteArray& data) -> QString {
        return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };

    // Header: {"alg":"HS256","typ":"JWT"}
    QJsonObject header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    QString headerB64 = base64url(QJsonDocument(header).toJson(QJsonDocument::Compact));

    // Payload
    QJsonObject payload;
    payload["sub"] = userId;
    payload["iat"] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());
    payload["exp"] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch() + 3600); // 1 hour
    payload["jti"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonArray permsArray;
    for (const QString& perm : permissions) {
        permsArray.append(perm);
    }
    payload["permissions"] = permsArray;
    QString payloadB64 = base64url(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    // Signature: HMAC-SHA256(header.payload, secret)
    QString signingInput = headerB64 + "." + payloadB64;
    QByteArray key = m_settings.jwtSecret.toUtf8();
    QByteArray data = signingInput.toUtf8();

    // HMAC-SHA256 using QMessageAuthenticationCode (Qt 5.1+)
    QByteArray signature = QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
    QString signatureB64 = base64url(signature);

    return signingInput + "." + signatureB64;
}

bool EnterprisePolicyEngine::checkAllowList(const QString& extensionId) {
    return m_settings.allowList.contains(extensionId);
}

bool EnterprisePolicyEngine::checkDenyList(const QString& extensionId) {
    return m_settings.denyList.contains(extensionId);
}

void EnterprisePolicyEngine::addToAuditLog(const QString& userId, const QString& extensionId, const QString& action, const QString& details) {
    AuditEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.userId = userId;
    entry.extensionId = extensionId;
    entry.action = action;
    entry.details = details;
    
    m_auditLog.append(entry);
    
    // Emit audit log entry signal
    QJsonObject obj;
    obj["timestamp"] = entry.timestamp;
    obj["userId"] = entry.userId;
    obj["extensionId"] = entry.extensionId;
    obj["action"] = entry.action;
    obj["details"] = entry.details;
    
    emit auditLogEntry(obj);
}

QString EnterprisePolicyEngine::getCurrentTimestamp() {
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

bool EnterprisePolicyEngine::verifyJwtSignature(const QString& token) {
    // In a real implementation, this would verify the JWT signature
    // For now, we'll just check if the token has the basic structure
    QStringList parts = token.split('.');
    return parts.size() == 3;
}