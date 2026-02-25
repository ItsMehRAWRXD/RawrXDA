#ifndef QUANTUM_SAFE_SECURITY_HPP
#define QUANTUM_SAFE_SECURITY_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QJsonObject>

class QuantumSafeSecurityPrivate;

class QuantumSafeSecurity : public QObject {
    Q_OBJECT
public:
    static QuantumSafeSecurity* instance();

    // Kyber-like (educational) KEM interfaces
    QJsonObject generateKyberKeyPair();
    QByteArray kyberEncapsulate(const QJsonObject& publicKey, const QByteArray& sharedSecret);
    QByteArray kyberDecapsulate(const QJsonObject& privateKey, const QByteArray& ciphertext);

    // Dilithium-like (educational) signature interfaces
    QJsonObject generateDilithiumKeyPair();
    QByteArray dilithiumSign(const QJsonObject& signingKey, const QByteArray& message);
    bool dilithiumVerify(const QJsonObject& verificationKey, const QByteArray& message, const QByteArray& signature);

    // Enterprise utility flows
    QByteArray secureToolExecution(const QString& toolName, const QStringList& parameters);
    bool verifyToolExecution(const QByteArray& executionToken, const QString& toolName);

private:
    explicit QuantumSafeSecurity(QObject* parent = nullptr);
    ~QuantumSafeSecurity();

    QScopedPointer<QuantumSafeSecurityPrivate> d_ptr;

    Q_DISABLE_COPY(QuantumSafeSecurity)
};

#endif // QUANTUM_SAFE_SECURITY_HPP
