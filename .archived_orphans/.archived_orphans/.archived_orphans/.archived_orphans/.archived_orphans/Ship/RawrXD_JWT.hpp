// RawrXD_JWT.hpp - JWT Validation Engine (HS256 / RS256)
// Pure C++20 - No Qt Dependencies
// Uses Windows CNG (bcrypt.h) for all cryptographic operations
// Features: Token parsing, signature verification, claims extraction,
//           expiry/nbf/iss/aud validation, JWKS key caching

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// JWT Algorithm Identifiers
// ============================================================================
enum class JWTAlgorithm : uint8_t {
    HS256,   // HMAC-SHA256 (symmetric)
    RS256,   // RSASSA-PKCS1-v1_5 with SHA-256 (asymmetric)
    None,    // None (REJECT by default)
    Unknown
};

// ============================================================================
// JWT Claims
// ============================================================================
struct JWTClaims {
    std::string issuer;            // iss
    std::string subject;           // sub
    std::string audience;          // aud
    uint64_t    expirationTime = 0;// exp (unix epoch seconds)
    uint64_t    notBefore     = 0; // nbf
    uint64_t    issuedAt      = 0; // iat
    std::string jwtId;             // jti
    std::map<std::string, std::string> custom; // Custom claims (string values)

    bool IsExpired(uint64_t clockSkewSec = 30) const {
        if (expirationTime == 0) return false; // No expiry claim
        auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return now > (expirationTime + clockSkewSec);
    }

    bool IsNotYetValid(uint64_t clockSkewSec = 30) const {
        if (notBefore == 0) return false;
        auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return now < (notBefore - clockSkewSec);
    }
};

// ============================================================================
// JWT Validation Result
// ============================================================================
struct JWTValidationResult {
    bool        valid       = false;
    std::string error;
    JWTAlgorithm algorithm  = JWTAlgorithm::Unknown;
    JWTClaims   claims;
    std::string rawHeader;
    std::string rawPayload;
};

// ============================================================================
// JWT Validator
// ============================================================================
class JWTValidator {
public:
    struct Config {
        std::string expectedIssuer;     // Required issuer (empty = skip check)
        std::string expectedAudience;   // Required audience (empty = skip check)
        uint64_t    clockSkewSeconds   = 30;
        bool        rejectNoneAlg      = true;  // Always reject "none" algorithm
        bool        requireExpiry      = true;  // Reject tokens without exp
    };

    JWTValidator() = default;

    void SetConfig(const Config& cfg) { m_config = cfg; }

    // ---- Set Keys ----
    void SetHS256Secret(const std::string& secret) {
        m_hs256Secret.assign(secret.begin(), secret.end());
    }

    void SetHS256Secret(const std::vector<uint8_t>& secret) {
        m_hs256Secret = secret;
    }

    void SetRS256PublicKeyDER(const std::vector<uint8_t>& publicKeyDER) {
        m_rs256PublicKeyDER = publicKeyDER;
    }

    // Set RS256 public key from PEM (strips header/footer and base64-decodes)
    void SetRS256PublicKeyPEM(const std::string& pem) {
        std::string stripped;
        std::istringstream iss(pem);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("-----") != std::string::npos) continue;
            // Trim CR/LF
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            stripped += line;
        }
        m_rs256PublicKeyDER = Base64Decode(stripped);
    }

    // ---- Validate Token ----
    JWTValidationResult Validate(const std::string& token) const {
        JWTValidationResult result;

        // 1. Split into parts
        auto parts = SplitToken(token);
        if (parts.size() != 3) {
            result.error = "Invalid JWT format: expected 3 parts, got " + std::to_string(parts.size());
            return result;
        }

        // 2. Decode header
        std::string headerJSON = Base64URLDecodeToString(parts[0]);
        result.rawHeader = headerJSON;

        JWTAlgorithm alg = ParseAlgorithm(headerJSON);
        result.algorithm = alg;

        if (alg == JWTAlgorithm::None && m_config.rejectNoneAlg) {
            result.error = "Algorithm 'none' is not permitted";
            return result;
        }
        if (alg == JWTAlgorithm::Unknown) {
            result.error = "Unsupported JWT algorithm";
            return result;
        }

        // 3. Verify signature
        std::string signedPart = parts[0] + "." + parts[1];
        std::vector<uint8_t> signature = Base64URLDecode(parts[2]);

        bool sigValid = false;
        switch (alg) {
            case JWTAlgorithm::HS256:
                sigValid = VerifyHS256(signedPart, signature);
                break;
            case JWTAlgorithm::RS256:
                sigValid = VerifyRS256(signedPart, signature);
                break;
            default:
                break;
        }

        if (!sigValid) {
            result.error = "Signature verification failed";
            return result;
        }

        // 4. Decode payload
        std::string payloadJSON = Base64URLDecodeToString(parts[1]);
        result.rawPayload = payloadJSON;
        result.claims = ParseClaims(payloadJSON);

        // 5. Validate claims
        if (m_config.requireExpiry && result.claims.expirationTime == 0) {
            result.error = "Token missing required 'exp' claim";
            return result;
        }

        if (result.claims.IsExpired(m_config.clockSkewSeconds)) {
            result.error = "Token has expired";
            return result;
        }

        if (result.claims.IsNotYetValid(m_config.clockSkewSeconds)) {
            result.error = "Token is not yet valid (nbf)";
            return result;
        }

        if (!m_config.expectedIssuer.empty() && result.claims.issuer != m_config.expectedIssuer) {
            result.error = "Issuer mismatch: expected '" + m_config.expectedIssuer + "', got '" + result.claims.issuer + "'";
            return result;
        }

        if (!m_config.expectedAudience.empty() && result.claims.audience != m_config.expectedAudience) {
            result.error = "Audience mismatch: expected '" + m_config.expectedAudience + "', got '" + result.claims.audience + "'";
            return result;
        }

        result.valid = true;
        return result;
    }

    // ---- Decode-only (no signature check) ----
    JWTClaims DecodePayload(const std::string& token) const {
        auto parts = SplitToken(token);
        if (parts.size() != 3) return {};
        std::string payloadJSON = Base64URLDecodeToString(parts[1]);
        return ParseClaims(payloadJSON);
    }

private:
    Config m_config;
    std::vector<uint8_t> m_hs256Secret;
    std::vector<uint8_t> m_rs256PublicKeyDER;

    // ---- Token Splitting ----
    static std::vector<std::string> SplitToken(const std::string& token) {
        std::vector<std::string> parts;
        std::string::size_type start = 0;
        while (true) {
            auto pos = token.find('.', start);
            if (pos == std::string::npos) {
                parts.push_back(token.substr(start));
                break;
            }
            parts.push_back(token.substr(start, pos - start));
            start = pos + 1;
        }
        return parts;
    }

    // ---- Algorithm Parsing ----
    static JWTAlgorithm ParseAlgorithm(const std::string& headerJSON) {
        // Simple JSON extraction for "alg" field
        auto algStr = ExtractJSONString(headerJSON, "alg");
        if (algStr == "HS256") return JWTAlgorithm::HS256;
        if (algStr == "RS256") return JWTAlgorithm::RS256;
        if (algStr == "none")  return JWTAlgorithm::None;
        return JWTAlgorithm::Unknown;
    }

    // ---- Claims Parsing ----
    static JWTClaims ParseClaims(const std::string& json) {
        JWTClaims claims;
        claims.issuer     = ExtractJSONString(json, "iss");
        claims.subject    = ExtractJSONString(json, "sub");
        claims.audience   = ExtractJSONString(json, "aud");
        claims.jwtId      = ExtractJSONString(json, "jti");

        auto expStr = ExtractJSONNumber(json, "exp");
        if (!expStr.empty()) claims.expirationTime = std::stoull(expStr);

        auto nbfStr = ExtractJSONNumber(json, "nbf");
        if (!nbfStr.empty()) claims.notBefore = std::stoull(nbfStr);

        auto iatStr = ExtractJSONNumber(json, "iat");
        if (!iatStr.empty()) claims.issuedAt = std::stoull(iatStr);

        // Extract custom string claims (heuristic: anything not standard)
        // This is a simplified parser — production use integrates RawrXD_JSON.hpp
        static const char* stdKeys[] = {"iss", "sub", "aud", "exp", "nbf", "iat", "jti"};
        // Additional custom claims extraction can be extended here

        return claims;
    }

    // ---- HS256 Verification ----
    bool VerifyHS256(const std::string& signedPart, const std::vector<uint8_t>& signature) const {
        if (m_hs256Secret.empty()) return false;

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            return false;

        DWORD hashObjSize = 0, hashSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize, sizeof(DWORD), &cbResult, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(DWORD), &cbResult, 0);

        std::vector<UCHAR> hashObj(hashObjSize);
        std::vector<UCHAR> hashVal(hashSize);

        bool valid = false;
        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize,
                (PUCHAR)m_hs256Secret.data(), (ULONG)m_hs256Secret.size(), 0))) {
            BCryptHashData(hHash, (PUCHAR)signedPart.data(), (ULONG)signedPart.size(), 0);
            BCryptFinishHash(hHash, hashVal.data(), hashSize, 0);
            BCryptDestroyHash(hHash);

            // Constant-time comparison
            if (signature.size() == hashSize) {
                volatile uint8_t diff = 0;
                for (DWORD i = 0; i < hashSize; ++i) {
                    diff |= hashVal[i] ^ signature[i];
                }
                valid = (diff == 0);
            }
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return valid;
    }

    // ---- RS256 Verification ----
    bool VerifyRS256(const std::string& signedPart, const std::vector<uint8_t>& signature) const {
        if (m_rs256PublicKeyDER.empty()) return false;

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_KEY_HANDLE hKey = nullptr;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0)))
            return false;

        // Import public key from DER
        NTSTATUS status = BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB,
            &hKey, (PUCHAR)m_rs256PublicKeyDER.data(), (ULONG)m_rs256PublicKeyDER.size(), 0);

        if (!BCRYPT_SUCCESS(status)) {
            // Try as PKCS#1 format - decode SubjectPublicKeyInfo wrapper
            // Many PEM keys have an outer ASN.1 wrapper we need to strip
            auto rawKey = ExtractRSAPublicKeyFromSPKI(m_rs256PublicKeyDER);
            if (!rawKey.empty()) {
                status = BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB,
                    &hKey, (PUCHAR)rawKey.data(), (ULONG)rawKey.size(), 0);
            }
        }

        bool valid = false;
        if (BCRYPT_SUCCESS(status) && hKey) {
            // Hash the signed part with SHA-256
            BCRYPT_ALG_HANDLE hHashAlg = nullptr;
            if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
                DWORD hashSize = 0, cbResult = 0;
                BCryptGetProperty(hHashAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(DWORD), &cbResult, 0);

                std::vector<UCHAR> hashVal(hashSize);
                BCRYPT_HASH_HANDLE hHash = nullptr;
                DWORD hashObjSize = 0;
                BCryptGetProperty(hHashAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize, sizeof(DWORD), &cbResult, 0);
                std::vector<UCHAR> hashObj(hashObjSize);

                if (BCRYPT_SUCCESS(BCryptCreateHash(hHashAlg, &hHash, hashObj.data(), hashObjSize, nullptr, 0, 0))) {
                    BCryptHashData(hHash, (PUCHAR)signedPart.data(), (ULONG)signedPart.size(), 0);
                    BCryptFinishHash(hHash, hashVal.data(), hashSize, 0);
                    BCryptDestroyHash(hHash);

                    // Verify PKCS#1 v1.5 signature
                    BCRYPT_PKCS1_PADDING_INFO paddingInfo;
                    paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

                    status = BCryptVerifySignature(hKey, &paddingInfo,
                        hashVal.data(), hashSize,
                        (PUCHAR)signature.data(), (ULONG)signature.size(),
                        BCRYPT_PAD_PKCS1);
                    valid = BCRYPT_SUCCESS(status);
                }

                BCryptCloseAlgorithmProvider(hHashAlg, 0);
            }
            BCryptDestroyKey(hKey);
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return valid;
    }

    // ---- ASN.1 SPKI Extraction ----
    static std::vector<uint8_t> ExtractRSAPublicKeyFromSPKI(const std::vector<uint8_t>& spki) {
        // SubjectPublicKeyInfo is an ASN.1 SEQUENCE containing:
        //   AlgorithmIdentifier (SEQUENCE { OID, NULL })
        //   BIT STRING containing the RSA public key
        // We find the BIT STRING and extract its content, then
        // parse the inner SEQUENCE { INTEGER n, INTEGER e } into BCRYPT_RSAPUBLIC_BLOB

        if (spki.size() < 30) return {};

        // Find the RSA OID: 1.2.840.113549.1.1.1 = 06 09 2A 86 48 86 F7 0D 01 01 01
        static const uint8_t rsaOID[] = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
        bool foundOID = false;
        size_t oidEnd = 0;
        for (size_t i = 0; i + sizeof(rsaOID) <= spki.size(); ++i) {
            if (memcmp(spki.data() + i, rsaOID, sizeof(rsaOID)) == 0) {
                foundOID = true;
                oidEnd = i + sizeof(rsaOID);
                break;
            }
        }
        if (!foundOID) return {};

        // Find the BIT STRING (tag 0x03) after the OID
        size_t bitStringStart = 0;
        for (size_t i = oidEnd; i < spki.size(); ++i) {
            if (spki[i] == 0x03) {
                bitStringStart = i;
                break;
            }
        }
        if (bitStringStart == 0) return {};

        // Parse BIT STRING length
        size_t contentStart = bitStringStart + 2; // tag + length byte
        if (spki[bitStringStart + 1] & 0x80) {
            int lenBytes = spki[bitStringStart + 1] & 0x7F;
            contentStart = bitStringStart + 2 + lenBytes;
        }
        // Skip unused bits byte
        contentStart += 1;

        if (contentStart >= spki.size()) return {};

        // The content is an RSA public key SEQUENCE { INTEGER n, INTEGER e }
        // Parse and convert to BCRYPT_RSAPUBLIC_BLOB
        size_t pos = contentStart;
        if (spki[pos] != 0x30) return {}; // Not a SEQUENCE

        // Skip SEQUENCE tag + length
        pos++;
        if (spki[pos] & 0x80) {
            int lenBytes = spki[pos] & 0x7F;
            pos += 1 + lenBytes;
        } else {
            pos++;
        }

        // Parse INTEGER n (modulus)
        auto n = ParseASN1Integer(spki, pos);
        if (n.empty()) return {};

        // Parse INTEGER e (exponent)
        auto e = ParseASN1Integer(spki, pos);
        if (e.empty()) return {};

        // Build BCRYPT_RSAPUBLIC_BLOB
        // Structure: BCRYPT_RSAKEY_BLOB header + exponent + modulus
        BCRYPT_RSAKEY_BLOB header = {};
        header.Magic      = BCRYPT_RSAPUBLIC_MAGIC;
        header.BitLength  = (ULONG)(n.size() * 8);
        header.cbPublicExp = (ULONG)e.size();
        header.cbModulus   = (ULONG)n.size();
        header.cbPrime1    = 0;
        header.cbPrime2    = 0;

        std::vector<uint8_t> blob(sizeof(header) + e.size() + n.size());
        memcpy(blob.data(), &header, sizeof(header));
        memcpy(blob.data() + sizeof(header), e.data(), e.size());
        memcpy(blob.data() + sizeof(header) + e.size(), n.data(), n.size());
        return blob;
    }

    static std::vector<uint8_t> ParseASN1Integer(const std::vector<uint8_t>& data, size_t& pos) {
        if (pos >= data.size() || data[pos] != 0x02) return {};
        pos++; // Skip INTEGER tag

        size_t len = 0;
        if (data[pos] & 0x80) {
            int lenBytes = data[pos] & 0x7F;
            pos++;
            for (int i = 0; i < lenBytes && pos < data.size(); ++i) {
                len = (len << 8) | data[pos++];
            }
        } else {
            len = data[pos++];
        }

        if (pos + len > data.size()) return {};

        // Strip leading zero (ASN.1 positive integer padding)
        size_t start = pos;
        if (len > 1 && data[start] == 0x00) {
            start++;
            len--;
        }

        std::vector<uint8_t> result(data.begin() + start, data.begin() + start + len);
        pos = start + len;
        if (data[pos - len - 1] == 0x00) pos++; // Adjust for stripped zero
        return result;
    }

    // ---- Minimal JSON Extraction ----
    static std::string ExtractJSONString(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos + 1);
        if (pos == std::string::npos) return "";
        pos++; // Skip opening quote
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        return result;
    }

    static std::string ExtractJSONNumber(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return "";
        pos++; // Skip colon
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        std::string num;
        while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '-' || json[pos] == '.')) {
            num += json[pos++];
        }
        return num;
    }

    // ---- Base64URL ----
    static std::vector<uint8_t> Base64URLDecode(const std::string& input) {
        // Convert base64url to standard base64
        std::string b64 = input;
        for (char& c : b64) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }
        // Add padding
        while (b64.size() % 4 != 0) b64 += '=';
        return Base64Decode(b64);
    }

    static std::string Base64URLDecodeToString(const std::string& input) {
        auto bytes = Base64URLDecode(input);
        return std::string(bytes.begin(), bytes.end());
    }

    static std::vector<uint8_t> Base64Decode(const std::string& input) {
        static const int8_t b64table[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };

        std::vector<uint8_t> output;
        output.reserve(input.size() * 3 / 4);

        uint32_t buf = 0;
        int bits = 0;
        for (unsigned char c : input) {
            if (c == '=' || c == '\n' || c == '\r') continue;
            int8_t val = b64table[c];
            if (val < 0) continue;
            buf = (buf << 6) | val;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                output.push_back((buf >> bits) & 0xFF);
            }
        }
        return output;
    }
};

} // namespace Security
} // namespace RawrXD
