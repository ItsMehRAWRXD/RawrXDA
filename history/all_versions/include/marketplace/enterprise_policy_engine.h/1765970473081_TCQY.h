#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

/**
 * @class EnterprisePolicyEngine
 * @brief Manages enterprise policies for extension installation
 * 
 * This class handles:
 * - JWT-based Single Sign-On (SSO)
 * - Extension allow-list/deny-list enforcement
 * - Digital signature verification
 * - Compliance checking
 * - Audit logging
 */
class EnterprisePolicyEngine : public QObject {
    Q_OBJECT

public:
    explicit EnterprisePolicyEngine(QObject* parent = nullptr);
    ~EnterprisePolicyEngine();

    // Policy configuration
    void setAllowList(const QStringList& extensionIds);
    void setDenyList(const QStringList& extensionIds);
    void setRequireSignature(bool require);
    void setJwtSecret(const QString& secret);
    
    // Policy enforcement
    bool isExtensionAllowed(const QString& extensionId);
    bool verifyExtensionSignature(const QString& extensionId, const QString& signature);
    bool validateUserAccess(const QString& userId, const QString& jwtToken);
    
    // Compliance and audit
    void logExtensionInstallation(const QString& extensionId, const QString& userId);
    void logExtensionUninstallation(const QString& extensionId, const QString& userId);
    QList<QJsonObject> getAuditLog(int limit = 100);
    
    // Utility methods
    bool isJwtValid(const QString& token);
    QString generateJwtToken(const QString& userId, const QStringList& permissions);

signals:
    void policyViolation(const QString& extensionId, const QString& reason);
    void auditLogEntry(const QJsonObject& entry);
    void complianceStatusChanged(bool compliant);

private:
    struct PolicySettings {
        QStringList allowList;
        QStringList denyList;
        bool requireSignature;
        QString jwtSecret;
    };
    
    struct AuditEntry {
        QString timestamp;
        QString userId;
        QString extensionId;
        QString action; // install, uninstall, block
        QString details;
    };

    PolicySettings m_settings;
    QList<AuditEntry> m_auditLog;
    bool m_compliant;

    bool checkAllowList(const QString& extensionId);
    bool checkDenyList(const QString& extensionId);
    void addToAuditLog(const QString& userId, const QString& extensionId, const QString& action, const QString& details);
    QString getCurrentTimestamp();
    bool verifyJwtSignature(const QString& token);
};