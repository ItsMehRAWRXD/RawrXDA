#ifndef ENTERPRISE_SECURITY_HPP
#define ENTERPRISE_SECURITY_HPP

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QMap>

struct SecurityEvent {
    QString eventType;
    QString severity;
    QString description;
    QDateTime timestamp;
    QJsonObject details;
    QString source;
    bool handled;
};

struct SecurityPolicy {
    QString policyName;
    QString description;
    QJsonObject rules;
    bool enabled;
    QString enforcementLevel;
};

class EnterpriseSecurity : public QObject {
    Q_OBJECT
    
public:
    static EnterpriseSecurity* instance();
    
    // Authentication and authorization
    bool authenticateUser(const QString& username, const QString& password);
    bool authorizeToolExecution(const QString& toolName, const QString& user);
    bool validateMissionPermissions(const QString& missionId, const QString& user);
    
    // Security auditing
    void auditToolExecution(const QString& toolName, const QStringList& parameters);
    void auditMissionExecution(const QString& missionId);
    void auditFileAccess(const QString& filePath, const QString& operation);
    
    // Threat detection
    bool detectMaliciousPatterns(const QString& code);
    bool detectSuspiciousBehavior(const QJsonObject& behaviorData);
    bool detectAnomalies(const QJsonObject& metrics);
    
    // Encryption and data protection
    QByteArray encryptSensitiveData(const QByteArray& data);
    QByteArray decryptSensitiveData(const QByteArray& encryptedData);
    bool validateDataIntegrity(const QByteArray& data, const QByteArray& signature);
    
    // Security policies
    void addSecurityPolicy(const SecurityPolicy& policy);
    bool enforceSecurityPolicy(const QString& policyName, const QJsonObject& context);
    QJsonObject getSecurityPolicyStatus();
    
    // Incident response
    void handleSecurityIncident(const SecurityEvent& incident);
    QJsonObject generateIncidentReport(const QString& incidentId);
    void escalateSecurityAlert(const QString& alertType, const QJsonObject& details);
    
    // Compliance and reporting
    QJsonObject generateSecurityComplianceReport();
    QJsonObject generateAuditTrail(const QDateTime& startTime, const QDateTime& endTime);
    bool meetsComplianceStandard(const QString& standard);
    
    // Real-time monitoring
    void monitorToolExecution(const QString& toolName, const QStringList& parameters);
    void monitorMissionExecution(const QString& missionId);
    void monitorSystemResources();
    
    // Security configuration
    void configureSecuritySettings(const QJsonObject& settings);
    QJsonObject getSecurityConfiguration();
    void updateSecurityRules(const QJsonObject& newRules);
    
    // Emergency procedures
    void emergencyLockdown();
    void isolateSuspiciousActivity(const QString& activityId);
    void revokeAllPermissions();
    
signals:
    void securityEventDetected(const SecurityEvent& event);
    void securityPolicyViolation(const QString& policyName, const QJsonObject& context);
    void authenticationRequired(const QString& resource);
    void authorizationGranted(const QString& user, const QString& resource);
    void securityAlert(const QString& alertType, const QJsonObject& details);
    void incidentReportGenerated(const QString& incidentId, const QJsonObject& report);
    
private:
    explicit EnterpriseSecurity(QObject *parent = nullptr);
    ~EnterpriseSecurity();
    
    class Private;
    QScopedPointer<Private> d_ptr;
    
    Q_DISABLE_COPY(EnterpriseSecurity)
};

#endif // ENTERPRISE_SECURITY_HPP