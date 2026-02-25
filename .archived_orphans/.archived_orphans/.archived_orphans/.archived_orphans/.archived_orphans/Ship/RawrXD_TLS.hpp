// RawrXD_TLS.hpp - HTTPS Enforcement & Certificate Pinning
// Pure C++20 - No Qt Dependencies
// Uses WinHTTP / Windows CNG for TLS enforcement
// Features: HTTPS-only policy, HPKP-style certificate pinning, SPKI hash validation,
//           TLS version enforcement (1.2+), connection security audit, pin rotation

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// TLS Policy Configuration
// ============================================================================
enum class TLSMinVersion : uint8_t {
    TLS_1_0 = 0,   // Insecure - should NEVER be used
    TLS_1_1 = 1,   // Deprecated
    TLS_1_2 = 2,   // Minimum acceptable
    TLS_1_3 = 3    // Recommended
};

enum class PinValidationResult : uint8_t {
    Success           = 0,
    NoPinConfigured   = 1,
    PinMismatch       = 2,   // CRITICAL: Possible MITM attack
    CertificateError  = 3,
    ConnectionError   = 4,
    BackupPinMatch    = 5    // Matched backup pin (primary may need rotation)
};

// ============================================================================
// Certificate Pin Entry
// ============================================================================
struct CertificatePin {
    std::string domain;           // e.g., "api.openai.com"
    std::string primarySPKIHash;  // SHA-256 of SubjectPublicKeyInfo (base64)
    std::vector<std::string> backupSPKIHashes; // Backup pins for rotation
    uint64_t    pinnedAt    = 0;  // When pin was established (unix ms)
    uint64_t    expiresAt   = 0;  // After this, re-pin required (0 = permanent)
    bool        reportOnly  = false; // If true, log violation but don't block
    uint64_t    lastVerified = 0;
    uint32_t    verifyCount  = 0;
    uint32_t    failCount    = 0;

    bool IsExpired() const {
        if (expiresAt == 0) return false;
        auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return now > expiresAt;
    }
};

// ============================================================================
// TLS Connection Result
// ============================================================================
struct TLSConnectionInfo {
    bool          secure        = false;
    std::string   protocol;     // "TLS 1.2", "TLS 1.3"
    std::string   cipherSuite;
    std::string   serverCertSubject;
    std::string   serverCertIssuer;
    std::string   serverCertSPKIHash;
    uint64_t      certExpiresAt = 0;
    PinValidationResult pinResult = PinValidationResult::NoPinConfigured;
    std::string   error;
};

// ============================================================================
// TLSEnforcer - HTTPS enforcement + certificate pinning engine
// ============================================================================
class TLSEnforcer {
public:
    struct Config {
        TLSMinVersion       minTLSVersion    = TLSMinVersion::TLS_1_2;
        bool                enforceHTTPS     = true;    // Block all HTTP connections
        bool                enablePinning    = true;
        bool                autoPin          = false;   // Automatically pin on first connection (TOFU)
        uint64_t            autoPinLifetimeMs = 30ULL * 24 * 60 * 60 * 1000; // 30 days
        bool                enableRevocationCheck = true;
    };

    TLSEnforcer() = default;

    void SetConfig(const Config& cfg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = cfg;
    }

    // ---- Pin Management ----
    void PinDomain(const std::string& domain, const std::string& spkiHashBase64,
                   const std::vector<std::string>& backupHashes = {},
                   uint64_t lifetimeMs = 0, bool reportOnly = false) {
        std::lock_guard<std::mutex> lock(m_mutex);
        CertificatePin pin;
        pin.domain           = NormalizeDomain(domain);
        pin.primarySPKIHash  = spkiHashBase64;
        pin.backupSPKIHashes = backupHashes;
        pin.pinnedAt         = NowMs();
        pin.expiresAt        = lifetimeMs > 0 ? (pin.pinnedAt + lifetimeMs) : 0;
        pin.reportOnly       = reportOnly;
        m_pins[pin.domain]   = pin;
    }

    void UnpinDomain(const std::string& domain) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pins.erase(NormalizeDomain(domain));
    }

    bool IsPinned(const std::string& domain) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pins.find(NormalizeDomain(domain));
        return it != m_pins.end() && !it->second.IsExpired();
    }

    // ---- URL Validation (HTTPS enforcement) ----
    bool IsURLAllowed(const std::string& url) const {
        if (!m_config.enforceHTTPS) return true;

        // Block plaintext HTTP
        if (url.substr(0, 7) == "http://" && url.substr(0, 8) != "http://localhost" &&
            url.substr(0, 14) != "http://127.0.0" && url.substr(0, 12) != "http://[::1]") {
            return false;
        }

        return true;
    }

    // Block http:// URLs and return sanitized https:// version
    std::string EnforceHTTPS(const std::string& url) const {
        if (url.substr(0, 7) == "http://") {
            // Exempt localhost
            if (url.substr(7, 9) == "localhost" || url.substr(7, 7) == "127.0.0" || url.substr(7, 5) == "[::1]") {
                return url;
            }
            // Upgrade to HTTPS
            return "https://" + url.substr(7);
        }
        return url;
    }

    // ---- Apply TLS Policy to WinHTTP Session ----
    bool ApplyToWinHTTPSession(HINTERNET hSession) const {
        if (!hSession) return false;

        // Set minimum TLS version
        DWORD protocols = 0;
        switch (m_config.minTLSVersion) {
            case TLSMinVersion::TLS_1_3:
                protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
                break;
            case TLSMinVersion::TLS_1_2:
                protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
                break;
            case TLSMinVersion::TLS_1_1:
                protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
                            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
                break;
            default:
                protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
                            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
                            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
                break;
        }

        if (!WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
            return false;

        // Enable revocation checking
        if (m_config.enableRevocationCheck) {
            DWORD flags = WINHTTP_ENABLE_SSL_REVOCATION;
            WinHttpSetOption(hSession, WINHTTP_OPTION_ENABLE_FEATURE, &flags, sizeof(flags));
        }

        return true;
    }

    // ---- Validate Server Certificate Against Pins ----
    PinValidationResult ValidateConnection(HINTERNET hRequest, const std::string& domain) {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string normDomain = NormalizeDomain(domain);

        // Get server certificate
        WINHTTP_CERTIFICATE_INFO certInfo = {};
        DWORD certInfoSize = sizeof(certInfo);
        if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT, &certInfo, &certInfoSize)) {
            return PinValidationResult::CertificateError;
        }

        // Get raw certificate for SPKI hash
        LPSTR certChain = nullptr;
        DWORD chainSize = 0;
        WinHttpQueryOption(hRequest, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &certChain, &chainSize);

        // Compute SPKI hash from server certificate
        std::string serverSPKIHash = ComputeSPKIHashFromRequest(hRequest);

        // Clean up cert info strings
        if (certInfo.lpszSubjectInfo) LocalFree(certInfo.lpszSubjectInfo);
        if (certInfo.lpszIssuerInfo) LocalFree(certInfo.lpszIssuerInfo);
        if (certInfo.lpszProtocolName) LocalFree(certInfo.lpszProtocolName);
        if (certInfo.lpszSignatureAlgName) LocalFree(certInfo.lpszSignatureAlgName);
        if (certInfo.lpszEncryptionAlgName) LocalFree(certInfo.lpszEncryptionAlgName);

        // Check against pin
        auto it = m_pins.find(normDomain);
        if (it == m_pins.end()) {
            // No pin configured
            if (m_config.autoPin && !serverSPKIHash.empty()) {
                // Trust On First Use (TOFU) - auto-pin
                PinDomainInternal(normDomain, serverSPKIHash, m_config.autoPinLifetimeMs);
                return PinValidationResult::Success;
            }
            return PinValidationResult::NoPinConfigured;
        }

        auto& pin = it->second;
        if (pin.IsExpired()) {
            return PinValidationResult::NoPinConfigured;
        }

        // Check primary pin
        if (serverSPKIHash == pin.primarySPKIHash) {
            pin.lastVerified = NowMs();
            pin.verifyCount++;
            return PinValidationResult::Success;
        }

        // Check backup pins
        for (const auto& backup : pin.backupSPKIHashes) {
            if (serverSPKIHash == backup) {
                pin.lastVerified = NowMs();
                pin.verifyCount++;
                return PinValidationResult::BackupPinMatch;
            }
        }

        // PIN MISMATCH - potential MITM attack!
        pin.failCount++;
        return PinValidationResult::PinMismatch;
    }

    // ---- Compute SPKI Hash (for initial pinning) ----
    std::string ComputeSPKIHashFromCertPEM(const std::string& certPEM) const {
        // Decode PEM to DER
        std::string stripped;
        std::istringstream iss(certPEM);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("-----") != std::string::npos) continue;
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            stripped += line;
        }

        auto der = Base64Decode(stripped);
        if (der.empty()) return "";

        // Parse certificate to extract SubjectPublicKeyInfo
        PCCERT_CONTEXT pCert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            der.data(), (DWORD)der.size());
        if (!pCert) return "";

        // The SPKI is at pCert->pCertInfo->SubjectPublicKeyInfo
        // We need to encode it and hash it
        DWORD spkiSize = 0;
        CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
            &pCert->pCertInfo->SubjectPublicKeyInfo, nullptr, &spkiSize);

        std::vector<uint8_t> spkiDER(spkiSize);
        CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
            &pCert->pCertInfo->SubjectPublicKeyInfo, spkiDER.data(), &spkiSize);

        CertFreeCertificateContext(pCert);

        // SHA-256 hash
        return ComputeSHA256Base64(spkiDER.data(), spkiSize);
    }

    // ---- Get Pin Info ----
    std::vector<CertificatePin> GetAllPins() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CertificatePin> result;
        for (const auto& [domain, pin] : m_pins) {
            result.push_back(pin);
        }
        return result;
    }

    // ---- Secure WinHTTP Request Helper ----
    // Creates a WinHTTP session with full TLS enforcement applied
    HINTERNET CreateSecureSession(const std::wstring& userAgent = L"RawrXD/1.0") const {
        HINTERNET hSession = WinHttpOpen(userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (hSession) {
            ApplyToWinHTTPSession(hSession);
        }
        return hSession;
    }

private:
    mutable std::mutex m_mutex;
    Config m_config;
    std::map<std::string, CertificatePin> m_pins;

    void PinDomainInternal(const std::string& domain, const std::string& spkiHash, uint64_t lifetimeMs) {
        CertificatePin pin;
        pin.domain          = domain;
        pin.primarySPKIHash = spkiHash;
        pin.pinnedAt        = NowMs();
        pin.expiresAt       = lifetimeMs > 0 ? (pin.pinnedAt + lifetimeMs) : 0;
        m_pins[domain]      = pin;
    }

    std::string ComputeSPKIHashFromRequest(HINTERNET hRequest) const {
        // Get the server certificate context
        const CERT_CONTEXT* pCert = nullptr;
        DWORD certSize = sizeof(pCert);
        if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &pCert, &certSize)) {
            return "";
        }
        if (!pCert) return "";

        // Encode SubjectPublicKeyInfo
        DWORD spkiSize = 0;
        CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
            &pCert->pCertInfo->SubjectPublicKeyInfo, nullptr, &spkiSize);

        std::vector<uint8_t> spkiDER(spkiSize);
        CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
            &pCert->pCertInfo->SubjectPublicKeyInfo, spkiDER.data(), &spkiSize);

        CertFreeCertificateContext(pCert);
        return ComputeSHA256Base64(spkiDER.data(), spkiSize);
    }

    // ---- SHA-256 Base64 Hash ----
    std::string ComputeSHA256Base64(const uint8_t* data, size_t len) const {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            return "";

        DWORD hashObjSize = 0, hashSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize, sizeof(DWORD), &cbResult, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(DWORD), &cbResult, 0);

        std::vector<uint8_t> hashObj(hashObjSize), hashVal(hashSize);
        BCRYPT_HASH_HANDLE hHash = nullptr;

        std::string result;
        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize, nullptr, 0, 0))) {
            BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
            BCryptFinishHash(hHash, hashVal.data(), hashSize, 0);
            BCryptDestroyHash(hHash);
            result = Base64Encode(hashVal.data(), hashSize);
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return result;
    }

    static std::string NormalizeDomain(const std::string& domain) {
        std::string d = domain;
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        // Strip trailing dot
        if (!d.empty() && d.back() == '.') d.pop_back();
        return d;
    }

    static uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    // ---- Base64 ----
    static std::string Base64Encode(const uint8_t* data, size_t len) {
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3) {
            uint32_t n = (uint32_t)data[i] << 16;
            if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
            if (i + 2 < len) n |= (uint32_t)data[i + 2];
            out += b64[(n >> 18) & 63];
            out += b64[(n >> 12) & 63];
            out += (i + 1 < len) ? b64[(n >> 6) & 63] : '=';
            out += (i + 2 < len) ? b64[n & 63] : '=';
        }
        return out;
    }

    static std::vector<uint8_t> Base64Decode(const std::string& input) {
        static const int8_t t[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
            -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };
        std::vector<uint8_t> out;
        out.reserve(input.size() * 3 / 4);
        uint32_t buf = 0; int bits = 0;
        for (unsigned char c : input) {
            if (c == '=' || t[c] < 0) continue;
            buf = (buf << 6) | t[c]; bits += 6;
            if (bits >= 8) { bits -= 8; out.push_back((buf >> bits) & 0xFF); }
        }
        return out;
    }
};

} // namespace Security
} // namespace RawrXD
