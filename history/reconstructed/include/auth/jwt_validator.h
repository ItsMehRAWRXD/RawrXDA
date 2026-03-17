#ifndef JWT_VALIDATOR_H
#define JWT_VALIDATOR_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QJsonWebToken>

// JWT validator middleware (HS256 + RS256) – plug into LLMHttpClient.
class JWTValidator : public QObject
{
    Q_OBJECT

public:
    explicit JWTValidator(QObject *parent = nullptr);
    ~JWTValidator();

    // Set the secret key for HS256 validation
    void setHS256Secret(const QString &secret);

    // Set the public key for RS256 validation
    void setRS256PublicKey(const QString &publicKey);

    // Validate a JWT token
    bool validateToken(const QString &token);

    // Extract claims from a validated token
    QMap<QString, QVariant> getClaims() const;

private:
    QString m_hs256Secret;
    QString m_rs256PublicKey;
    QMap<QString, QVariant> m_claims;
    
    // Validate HS256 token
    bool validateHS256(const QString &token);
    
    // Validate RS256 token
    bool validateRS256(const QString &token);
};

#endif // JWT_VALIDATOR_H