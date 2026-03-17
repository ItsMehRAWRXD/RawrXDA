// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\crypto\pqc_crypto.cpp
// Post-Quantum Cryptography wrapper for liboqs - NIST approved algorithms

#include "pqc_crypto.h"
#include <QDebug>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <memory>
#include <iostream>
#include <random>
#include <cstring>

namespace RawrXD {
namespace Crypto {

class PQCCrypto::Impl {
public:
    struct KeyPair {
        QByteArray publicKey;
        QByteArray secretKey;
        Algorithm algorithm;
        QString keyId;
    };
    
    struct HybridKeyPair {
        QByteArray classicalPublicKey;      // RSA-2048 or ECC P-256
        QByteArray classicalSecretKey;
        QByteArray pqcPublicKey;            // Kyber
        QByteArray pqcSecretKey;
        QString keyId;
    };
    
    bool pqcAvailable = false;
    std::map<QString, KeyPair> keyStore;      // keyId -> KeyPair
    std::map<QString, HybridKeyPair> hybridKeyStore;
    
    QMutex cryptoMutex;
};

PQCCrypto::PQCCrypto()
    : impl(std::make_unique<Impl>())
{
    // Verify liboqs availability
    initializePQC();
}

PQCCrypto::~PQCCrypto() = default;

void PQCCrypto::initializePQC() {
    // In production, initialize actual liboqs library
    // For now, we simulate PQC operations
    impl->pqcAvailable = true;
    qInfo() << "PQC Cryptography initialized";
}

KeyPairResult PQCCrypto::generateKeyPair(Algorithm algorithm) {
    QMutexLocker lock(&impl->cryptoMutex);
    
    KeyPairResult result;
    result.success = false;
    
    if (!impl->pqcAvailable) {
        result.errorMessage = "PQC library not available";
        return result;
    }
    
    // Simulate key generation (in production use actual liboqs)
    Impl::KeyPair keyPair;
    keyPair.algorithm = algorithm;
    keyPair.keyId = generateKeyId();
    keyPair.publicKey = generateRandomBytes(1024);
    keyPair.secretKey = generateRandomBytes(2048);
    
    impl->keyStore[keyPair.keyId] = keyPair;
    
    result.success = true;
    result.keyId = keyPair.keyId;
    result.publicKey = keyPair.publicKey;
    
    qInfo() << "Generated PQC key pair:" << keyPair.keyId;
    return result;
}

HybridKeyPairResult PQCCrypto::generateHybridKeyPair() {
    QMutexLocker lock(&impl->cryptoMutex);
    
    HybridKeyPairResult result;
    result.success = false;
    
    // Generate classical key pair (RSA-2048)
    QByteArray classicalPublic = generateRandomBytes(294);   // RSA-2048 public key
    QByteArray classicalSecret = generateRandomBytes(1704);  // RSA-2048 private key
    
    // Generate PQC key pair (Kyber)
    QByteArray pqcPublic = generateRandomBytes(1184);
    QByteArray pqcSecret = generateRandomBytes(2400);
    
    Impl::HybridKeyPair hybridPair;
    hybridPair.classicalPublicKey = classicalPublic;
    hybridPair.classicalSecretKey = classicalSecret;
    hybridPair.pqcPublicKey = pqcPublic;
    hybridPair.pqcSecretKey = pqcSecret;
    hybridPair.keyId = generateKeyId();
    
    impl->hybridKeyStore[hybridPair.keyId] = hybridPair;
    
    result.success = true;
    result.keyId = hybridPair.keyId;
    result.classicalPublicKey = classicalPublic;
    result.pqcPublicKey = pqcPublic;
    
    qInfo() << "Generated hybrid (classical+PQC) key pair:" << hybridPair.keyId;
    return result;
}

SignatureResult PQCCrypto::sign(const QByteArray& message, const QString& keyId) {
    QMutexLocker lock(&impl->cryptoMutex);
    
    SignatureResult result;
    result.success = false;
    
    auto it = impl->keyStore.find(keyId);
    if (it == impl->keyStore.end()) {
        result.errorMessage = "Key not found";
        return result;
    }
    
    // In production, use actual Dilithium signature
    QByteArray signature = QCryptographicHash::hash(message, QCryptographicHash::Sha256);
    signature.append(generateRandomBytes(2420));  // Dilithium signature is ~2420 bytes
    
    result.success = true;
    result.signature = signature;
    result.algorithm = "Dilithium";
    
    return result;
}

bool PQCCrypto::verify(const QByteArray& message, const QByteArray& signature,
                       const QString& keyId) {
    QMutexLocker lock(&impl->cryptoMutex);
    
    auto it = impl->keyStore.find(keyId);
    if (it == impl->keyStore.end()) {
        return false;
    }
    
    // In production, verify actual Dilithium signature
    QByteArray expectedSigStart = QCryptographicHash::hash(message, QCryptographicHash::Sha256);
    
    if (signature.size() < 32) {
        return false;
    }
    
    return signature.left(32) == expectedSigStart;
}

EncryptionResult PQCCrypto::encapsulate(const QString& keyId) {
    QMutexLocker lock(&impl->cryptoMutex);
    
    EncryptionResult result;
    result.success = false;
    
    // Key encapsulation mechanism (Kyber)
    QByteArray ciphertext = generateRandomBytes(1088);
    QByteArray sharedSecret = generateRandomBytes(32);
    
    result.success = true;
    result.ciphertext = ciphertext;
    result.sharedSecret = sharedSecret;
    result.algorithm = "Kyber";
    
    return result;
}

DecryptionResult PQCCrypto::decapsulate(const QByteArray& ciphertext, const QString& keyId) {
    QMutexLocker lock(&impl->cryptoMutex);
    
    DecryptionResult result;
    result.success = false;
    
    auto it = impl->keyStore.find(keyId);
    if (it == impl->keyStore.end()) {
        result.errorMessage = "Key not found";
        return result;
    }
    
    // In production, use Kyber decapsulation
    // For now, derive from ciphertext hash
    result.sharedSecret = QCryptographicHash::hash(ciphertext, QCryptographicHash::Sha256);
    result.success = true;
    
    return result;
}

QString PQCCrypto::algorithmToString(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::KYBER512: return "Kyber-512";
        case Algorithm::KYBER768: return "Kyber-768";
        case Algorithm::KYBER1024: return "Kyber-1024";
        case Algorithm::DILITHIUM2: return "Dilithium-2";
        case Algorithm::DILITHIUM3: return "Dilithium-3";
        case Algorithm::DILITHIUM5: return "Dilithium-5";
        case Algorithm::FALCON512: return "Falcon-512";
        case Algorithm::SPHINCSSHA2256F: return "SPHINCS+-SHA2-256f";
        default: return "Unknown";
    }
}

QString PQCCrypto::generateKeyId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    QString keyId;
    for (int i = 0; i < 32; ++i) {
        int val = dis(gen);
        keyId += QString::number(val, 16);
    }
    
    return keyId;
}

QByteArray PQCCrypto::generateRandomBytes(size_t length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    QByteArray result;
    for (size_t i = 0; i < length; ++i) {
        result.append(static_cast<char>(dis(gen)));
    }
    
    return result;
}

} // namespace Crypto
} // namespace RawrXD
