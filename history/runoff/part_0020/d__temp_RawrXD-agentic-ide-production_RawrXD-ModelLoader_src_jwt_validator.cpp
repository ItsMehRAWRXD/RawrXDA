#include "jwt_validator.h"
#include "../src/crypto/rawrxd_crypto.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <ctime>

namespace RawrXD {
namespace Auth {

// ============================================================
// JWKS MANAGER IMPLEMENTATION
// ============================================================

JWKSManager::JWKSManager() {}

JWKSManager::~JWKSManager() {}

bool JWKSManager::loadFromJson(const QJsonObject& jwksJson) {
    if (!jwksJson.contains("keys")) {
        return false;
    }

    QJsonArray keysArray = jwksJson["keys"].toArray();
    keys.clear();

    for (const auto& keyValue : keysArray) {
        if (keyValue.isObject()) {
            auto jwk = std::make_shared<JWK>(JWK::fromJson(keyValue.toObject()));
            keys.push_back(jwk);
        }
    }

    return !keys.empty();
}

std::shared_ptr<JWK> JWKSManager::findKey(const QString& keyId) {
    for (const auto& key : keys) {
        if (key->keyId == keyId) {
            return key;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<JWK>> JWKSManager::getAllKeys() const {
    return keys;
}

bool JWKSManager::verifyJWTSignature(const JWT& token, const std::shared_ptr<JWK>& key) {
    if (!key || !token.isValid) {
        return false;
    }

    // Use our custom crypto library (RawrXD::Crypto)
    const QStringList parts = token.rawToken.split('.');
    if (parts.size() != 3) {
        return false;
    }
    
    // Get signing input (header.payload)
    QByteArray signingInput = parts[0].toUtf8() + '.' + parts[1].toUtf8();
    std::vector<uint8_t> message(signingInput.begin(), signingInput.end());
    
    // Decode signature
    std::vector<uint8_t> signature = Crypto::Base64Url::decode(token.signature.toStdString());
    if (signature.empty()) {
        return false;
    }
    
    // Determine algorithm and verify
    QString alg = token.header.value("alg").toString();
    
    try {
        // Handle RSA algorithms (RS256, RS384, RS512, PS256, PS384, PS512)
        if (alg.startsWith("RS") || alg.startsWith("PS")) {
            // Load RSA public key from JWK
            Crypto::RSAPublicKey rsaKey;
            if (!rsaKey.loadFromJWK(key->publicKeyModulus.toStdString(),
                                    key->publicKeyExponent.toStdString())) {
                return false;
            }
            
            // Determine hash algorithm
            std::string hashAlg = "SHA-256";
            if (alg.endsWith("384")) {
                hashAlg = "SHA-384";
            } else if (alg.endsWith("512")) {
                hashAlg = "SHA-512";
            }
            
            // Verify signature (PKCS#1 v1.5 or PSS)
            if (alg.startsWith("RS")) {
                return rsaKey.verifyPKCS1(message, signature, hashAlg);
            } else {
                return rsaKey.verifyPSS(message, signature, hashAlg);
            }
        }
        // Handle ECDSA algorithms (ES256, ES384, ES512)
        else if (alg.startsWith("ES")) {
            Crypto::ECCurve::CurveType curve = Crypto::ECCurve::CurveType::P256;
            std::string hashAlg = "SHA-256";
            
            if (alg == "ES384") {
                curve = Crypto::ECCurve::CurveType::P384;
                hashAlg = "SHA-384";
            } else if (alg == "ES512") {
                curve = Crypto::ECCurve::CurveType::P521;
                hashAlg = "SHA-512";
            }
            
            Crypto::ECDSAPublicKey ecKey(curve);
            // Note: JWK should have 'x', 'y', 'crv' fields for EC keys
            QString x = key->header.value("x").toString();
            QString y = key->header.value("y").toString();
            QString crv = key->header.value("crv").toString();
            
            if (!ecKey.loadFromJWK(x.toStdString(), y.toStdString(), crv.toStdString())) {
                return false;
            }
            
            return ecKey.verify(message, signature, hashAlg);
        }
        // Handle HMAC algorithms (HS256, HS384, HS512)
        else if (alg.startsWith("HS")) {
            // HMAC requires a shared secret, typically not used with public JWKs
            // For completeness, implement if secret is available
            return false; // Not supported via JWK (requires shared secret)
        }
        
        return false; // Unsupported algorithm
    } catch (...) {
        return false; // Verification failed
    }
}

bool JWKSManager::validateJWT(const JWT& token,
                             const QString& expectedIssuer,
                             const QString& expectedAudience,
                             long long nowSeconds) {
    if (!token.isValid) {
        return false;
    }

    // Validate issuer
    if (!expectedIssuer.isEmpty() && token.claims.issuer != expectedIssuer) {
        return false;
    }

    // Validate audience
    if (!expectedAudience.isEmpty() && token.claims.audience != expectedAudience) {
        return false;
    }

    // Validate expiration
    if (token.claims.isExpired(nowSeconds)) {
        return false;
    }

    // Validate nbf (not before)
    if (nowSeconds > 0 && token.claims.notBefore > 0 && nowSeconds < token.claims.notBefore) {
        return false;
    }

    return true;
}

// ============================================================
// TOKEN PROVIDER IMPLEMENTATION
// ============================================================

TokenProvider::TokenProvider() 
    : jwksManager(std::make_unique<JWKSManager>())
{
}

TokenProvider::~TokenProvider() = default;

QString TokenProvider::createToken(const TokenClaims& claims, const QString& secret) {
    // Create header
    QJsonObject header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    // Create payload
    QJsonObject payload = claims.toJson();

    // Encode header and payload
    QByteArray headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray payloadJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QString headerBase64 = QString::fromUtf8(headerJson.toBase64(QByteArray::OmitTrailingEquals));
    QString payloadBase64 = QString::fromUtf8(payloadJson.toBase64(QByteArray::OmitTrailingEquals));

    // Create signature
    QString signingInput = headerBase64 + "." + payloadBase64;
    QByteArray signature = QMessageAuthenticationCode::hash(
        signingInput.toUtf8(),
        secret.toUtf8(),
        QCryptographicHash::Sha256
    );

    QString signatureBase64 = QString::fromUtf8(signature.toBase64(QByteArray::OmitTrailingEquals));

    return signingInput + "." + signatureBase64;
}

bool TokenProvider::validateToken(const QString& token, TokenClaims& outClaims, const QString& secret) {
    JWT jwt = JWT::decode(token);
    if (!jwt.isValid) {
        return false;
    }

    // Verify signature
    QStringList parts = token.split(".");
    if (parts.size() != 3) {
        return false;
    }

    QString signingInput = parts[0] + "." + parts[1];
    QByteArray signature = QMessageAuthenticationCode::hash(
        signingInput.toUtf8(),
        secret.toUtf8(),
        QCryptographicHash::Sha256
    );

    QString expectedSignature = QString::fromUtf8(signature.toBase64(QByteArray::OmitTrailingEquals));

    if (parts[2] != expectedSignature) {
        return false;
    }

    // Basic validation
    if (jwt.claims.isExpired()) {
        return false;
    }

    outClaims = jwt.claims;
    return true;
}

TokenClaims TokenProvider::extractClaims(const QString& token) {
    JWT jwt = JWT::decode(token);
    return jwt.claims;
}

} // namespace Auth
} // namespace RawrXD
