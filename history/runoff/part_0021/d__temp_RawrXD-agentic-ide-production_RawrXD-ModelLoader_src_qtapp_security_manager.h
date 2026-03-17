#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSslCertificate>
#include <QSslKey>
#include <vector>
#include <QMap>
#include <QJsonArray>

class SecurityManager : public QObject {
    Q_OBJECT

public:
    // Singleton access
    static SecurityManager* getInstance();

public:
    explicit SecurityManager(QObject* parent = nullptr);
    ~SecurityManager();

    // Authentication
    bool authenticateUser(const QString& username, const QString& password);
    bool changePassword(const QString& username, const QString& newPassword);
    bool createUser(const QString& username, const QString& password, const QJsonObject& permissions);

    // Authorization
    bool checkPermission(const QString& username, const QString& permission);
    QJsonObject getUserPermissions(const QString& username) const;

    // Encryption
    QByteArray encryptData(const QByteArray& data, const QString& key);
    QByteArray decryptData(const QByteArray& encryptedData, const QString& key);

    // Certificate management
    bool loadCertificate(const QString& certPath, const QString& keyPath);
    bool generateSelfSignedCertificate(const QString& commonName);

    // Audit logging
    void logSecurityEvent(const QString& event, const QJsonObject& details);
    QJsonArray getSecurityLogs(int limit = 100) const;

    // Compliance
    bool checkCompliance(const QJsonObject& complianceRules);
    QJsonObject generateComplianceReport();

signals:
    void userAuthenticated(const QString& username, bool success);
    void securityEventLogged(const QString& event);

private:
    QMap<QString, QJsonObject> m_users;
    QSslCertificate m_certificate;
    QSslKey m_privateKey;
    std::vector<QJsonObject> m_securityLogs;
};
