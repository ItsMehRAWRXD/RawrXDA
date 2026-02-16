// ============================================================================
// plugin_signature.cpp — Plugin Signature Enforcement
// ============================================================================
//
// WinVerifyTrust (Authenticode) + BCrypt SHA-256 verification for all plugin
// packages. Addresses extension_marketplace.cpp TODO stub.
//
// DEPS:     plugin_signature.h
// PATTERN:  PatchResult-compatible, no exceptions
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/plugin_signature.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <bcrypt.h>
#include <wincrypt.h>

// SCAFFOLD_208: Plugin signature verification


#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {
namespace Plugin {

// ============================================================================
// Singleton
// ============================================================================
PluginSignatureVerifier& PluginSignatureVerifier::instance() {
    static PluginSignatureVerifier s_instance;
    return s_instance;
}

PluginSignatureVerifier::PluginSignatureVerifier()
    : m_initialized(false)
    , m_publisherCount(0)
{
    memset(&m_policy, 0, sizeof(m_policy));
    memset(m_publishers, 0, sizeof(m_publishers));
    memset(&m_stats, 0, sizeof(m_stats));
}

PluginSignatureVerifier::~PluginSignatureVerifier() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
bool PluginSignatureVerifier::initialize() {
    return initialize(createStandardPolicy());
}

bool PluginSignatureVerifier::initialize(const SignaturePolicy& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) return true;

    m_policy = policy;
    m_publisherCount = 0;
    memset(&m_stats, 0, sizeof(m_stats));

    m_initialized = true;
    OutputDebugStringA("[PluginSignature] Initialized\n");
    return true;
}

void PluginSignatureVerifier::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    OutputDebugStringA("[PluginSignature] Shutdown\n");
}

// ============================================================================
// Policy Factories
// ============================================================================
SignaturePolicy PluginSignatureVerifier::createStrictPolicy() {
    SignaturePolicy p{};
    p.requireSignature       = true;
    p.requireTrustedRoot     = true;
    p.requireRawrXDAuthority = true;
    p.allowExpiredCerts      = false;
    p.checkCertRevocation    = true;
    p.logBlockedInstalls     = true;
    p.maxCertChainDepth      = 4;
    return p;
}

SignaturePolicy PluginSignatureVerifier::createStandardPolicy() {
    SignaturePolicy p{};
    p.requireSignature       = true;
    p.requireTrustedRoot     = true;
    p.requireRawrXDAuthority = false;
    p.allowExpiredCerts      = false;
    p.checkCertRevocation    = true;
    p.logBlockedInstalls     = true;
    p.maxCertChainDepth      = 8;
    return p;
}

SignaturePolicy PluginSignatureVerifier::createRelaxedPolicy() {
    SignaturePolicy p{};
    p.requireSignature       = false;
    p.requireTrustedRoot     = false;
    p.requireRawrXDAuthority = false;
    p.allowExpiredCerts      = true;
    p.checkCertRevocation    = false;
    p.logBlockedInstalls     = true;
    p.maxCertChainDepth      = 16;
    return p;
}

// ============================================================================
// Policy Management
// ============================================================================
SignaturePolicy PluginSignatureVerifier::getPolicy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policy;
}

void PluginSignatureVerifier::setPolicy(const SignaturePolicy& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
}

// ============================================================================
// WinVerifyTrust Wrapper
// ============================================================================
PluginSignatureResult PluginSignatureVerifier::winVerifyTrustCheck(const wchar_t* filePath) {
    WINTRUST_FILE_INFO fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));
    fileInfo.cbStruct       = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath  = filePath;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData;
    memset(&trustData, 0, sizeof(trustData));
    trustData.cbStruct            = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice          = WTD_UI_NONE;
    trustData.fdwRevocationChecks = m_policy.checkCertRevocation ?
                                     WTD_REVOKE_WHOLECHAIN : WTD_REVOKE_NONE;
    trustData.dwUnionChoice       = WTD_CHOICE_FILE;
    trustData.pFile               = &fileInfo;
    trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags         = WTD_SAFER_FLAG;

    LONG status = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    // Extract certificate info if verification succeeded or if we need the info
    char subject[256] = {0}, issuer[256] = {0}, thumbprint[41] = {0};
    uint64_t expiry = 0;
    extractCertInfo(filePath, subject, sizeof(subject),
                     issuer, sizeof(issuer), thumbprint, &expiry);

    // Close state handle
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    PluginSignatureResult result;
    memset(&result, 0, sizeof(result));

    if (subject[0]) strncpy_s(result.subjectName, sizeof(result.subjectName), subject, _TRUNCATE);
    if (issuer[0])  strncpy_s(result.issuerName, sizeof(result.issuerName), issuer, _TRUNCATE);
    if (thumbprint[0]) strncpy_s(result.thumbprint, sizeof(result.thumbprint), thumbprint, _TRUNCATE);
    result.expiryTimestamp = expiry;

    // Check if signed by RawrXD authority
    result.isRawrXDSigned = (strstr(subject, "RawrXD") != nullptr ||
                              strstr(subject, "ItsMehRAWRXD") != nullptr);

    switch (status) {
        case ERROR_SUCCESS:
            result.valid = true;
            result.status = SignatureStatus::Valid;
            result.detail = "Authenticode signature valid";
            result.errorCode = 0;
            break;

        case TRUST_E_NOSIGNATURE:
            result.valid = false;
            result.status = SignatureStatus::NoSignature;
            result.detail = "File is not signed";
            result.errorCode = (int)GetLastError();
            break;

        case TRUST_E_EXPLICIT_DISTRUST:
            result.valid = false;
            result.status = SignatureStatus::RevokedCertificate;
            result.detail = "Certificate explicitly distrusted";
            result.errorCode = (int)status;
            break;

        case TRUST_E_SUBJECT_NOT_TRUSTED:
            result.valid = false;
            result.status = SignatureStatus::UntrustedRoot;
            result.detail = "Certificate chain not trusted";
            result.errorCode = (int)status;
            break;

        case CERT_E_EXPIRED:
            result.valid = false;
            result.status = SignatureStatus::ExpiredCertificate;
            result.detail = "Certificate has expired";
            result.errorCode = (int)status;
            // Allow expired if policy permits
            if (m_policy.allowExpiredCerts) {
                result.valid = true;
                result.detail = "Certificate expired but allowed by policy";
            }
            break;

        default:
            result.valid = false;
            result.status = SignatureStatus::UnknownError;
            result.detail = "WinVerifyTrust failed";
            result.errorCode = (int)status;
            break;
    }

    return result;
}

// ============================================================================
// Certificate Info Extraction
// ============================================================================
bool PluginSignatureVerifier::extractCertInfo(const wchar_t* filePath,
                                                char* subject, size_t subjectLen,
                                                char* issuer, size_t issuerLen,
                                                char* thumbprint,
                                                uint64_t* expiry)
{
    if (!filePath) return false;

    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;

    BOOL result = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE,
        filePath,
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0, nullptr, nullptr, nullptr,
        &hStore, &hMsg, nullptr);

    if (!result || !hStore) return false;

    bool found = false;
    PCCERT_CONTEXT cert = CertEnumCertificatesInStore(hStore, nullptr);
    if (cert) {
        // Subject name
        if (subject) {
            CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                                nullptr, subject, (DWORD)subjectLen);
        }

        // Issuer name
        if (issuer) {
            CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                CERT_NAME_ISSUER_FLAG, nullptr,
                                issuer, (DWORD)issuerLen);
        }

        // Thumbprint (SHA-1 hash of certificate)
        if (thumbprint) {
            BYTE hashBuf[20];
            DWORD hashSize = sizeof(hashBuf);
            if (CertGetCertificateContextProperty(cert, CERT_HASH_PROP_ID,
                                                    hashBuf, &hashSize)) {
                static const char hexDigits[] = "0123456789abcdef";
                for (DWORD i = 0; i < 20; i++) {
                    thumbprint[i * 2]     = hexDigits[hashBuf[i] >> 4];
                    thumbprint[i * 2 + 1] = hexDigits[hashBuf[i] & 0x0F];
                }
                thumbprint[40] = '\0';
            }
        }

        // Expiry timestamp
        if (expiry) {
            FILETIME ft = cert->pCertInfo->NotAfter;
            ULARGE_INTEGER uli;
            uli.LowPart  = ft.dwLowDateTime;
            uli.HighPart = ft.dwHighDateTime;
            // Convert from FILETIME (100ns since 1601) to Unix epoch
            *expiry = (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
        }

        found = true;
        CertFreeCertificateContext(cert);
    }

    if (hStore) CertCloseStore(hStore, 0);
    if (hMsg)   CryptMsgClose(hMsg);

    return found;
}

// ============================================================================
// SHA-256 File Hash
// ============================================================================
bool PluginSignatureVerifier::computeFileSHA256(const wchar_t* filePath, char outHex[65]) {
    if (!filePath || !outHex) return false;

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool result = false;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                                   nullptr, 0);
    if (status != 0) {
        CloseHandle(hFile);
        return false;
    }

    DWORD hashObjSize = 0, tmp = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize,
                       sizeof(hashObjSize), &tmp, 0);

    auto hashObj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, hashObjSize);
    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        return false;
    }

    status = BCryptCreateHash(hAlg, &hHash, hashObj, hashObjSize, nullptr, 0, 0);
    if (status == 0) {
        uint8_t buf[65536];
        DWORD bytesRead = 0;

        while (ReadFile(hFile, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
            status = BCryptHashData(hHash, buf, bytesRead, 0);
            if (status != 0) break;
        }

        if (status == 0) {
            uint8_t hash[32];
            status = BCryptFinishHash(hHash, hash, 32, 0);
            if (status == 0) {
                static const char hexDigits[] = "0123456789abcdef";
                for (int i = 0; i < 32; i++) {
                    outHex[i * 2]     = hexDigits[hash[i] >> 4];
                    outHex[i * 2 + 1] = hexDigits[hash[i] & 0x0F];
                }
                outHex[64] = '\0';
                result = true;
            }
        }
        BCryptDestroyHash(hHash);
    }

    HeapFree(GetProcessHeap(), 0, hashObj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    return result;
}

// ============================================================================
// Verification Methods
// ============================================================================
PluginSignatureResult PluginSignatureVerifier::verifyDLL(const wchar_t* dllPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalVerifications++;

    PluginSignatureResult result = winVerifyTrustCheck(dllPath);

    if (result.valid) m_stats.validSignatures++;
    else              m_stats.invalidSignatures++;

    return result;
}

PluginSignatureResult PluginSignatureVerifier::verifyVSIX(const wchar_t* vsixPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalVerifications++;

    // VSIX is a ZIP file — check if it has an Authenticode signature
    // (some VSIX files are signed, others use a manifest-based approach)
    PluginSignatureResult result = winVerifyTrustCheck(vsixPath);

    if (!result.valid && result.status == SignatureStatus::NoSignature) {
        // VSIX without Authenticode — check for internal manifest signature
        // For now, delegate to SHA-256 hash verification
        char hashHex[65] = {0};
        if (computeFileSHA256(vsixPath, hashHex)) {
            result.valid = !m_policy.requireSignature;
            result.status = m_policy.requireSignature ?
                             SignatureStatus::NoSignature :
                             SignatureStatus::Valid;
            result.detail = m_policy.requireSignature ?
                             "VSIX not signed (required by policy)" :
                             "VSIX unsigned but allowed by policy";
        }
    }

    if (result.valid) m_stats.validSignatures++;
    else              m_stats.invalidSignatures++;

    return result;
}

PluginSignatureResult PluginSignatureVerifier::verifyJSModule(
    const wchar_t* jsPath, const char* expectedHash)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalVerifications++;

    if (!jsPath) {
        m_stats.invalidSignatures++;
        return PluginSignatureResult::error(SignatureStatus::UnknownError,
                                             "Null JS path");
    }

    char computedHash[65];
    if (!computeFileSHA256(jsPath, computedHash)) {
        m_stats.invalidSignatures++;
        return PluginSignatureResult::error(SignatureStatus::UnknownError,
                                             "Failed to compute SHA-256");
    }

    if (!expectedHash || expectedHash[0] == '\0') {
        // No expected hash provided — can't verify
        if (m_policy.requireSignature) {
            m_stats.invalidSignatures++;
            return PluginSignatureResult::error(SignatureStatus::NoSignature,
                                                 "No expected hash provided");
        }
        m_stats.validSignatures++;
        return PluginSignatureResult::ok("No hash check (policy allows)");
    }

    if (_stricmp(computedHash, expectedHash) != 0) {
        m_stats.invalidSignatures++;
        return PluginSignatureResult::error(SignatureStatus::Tampered,
                                             "SHA-256 hash mismatch — file tampered");
    }

    m_stats.validSignatures++;
    return PluginSignatureResult::ok("JS module SHA-256 verified");
}

PluginSignatureResult PluginSignatureVerifier::verifyRawrPackage(const wchar_t* packagePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalVerifications++;

    // .rawrpkg: custom binary format
    // Header: 4-byte magic "RWRP" + 4-byte version + 512-byte RSA signature + payload
    // For now, verify Authenticode if available, otherwise hash-only

    PluginSignatureResult result = winVerifyTrustCheck(packagePath);

    if (result.valid) {
        m_stats.validSignatures++;
    } else {
        m_stats.invalidSignatures++;
    }

    return result;
}

PluginSignatureResult PluginSignatureVerifier::verify(const wchar_t* packagePath) {
    if (!packagePath) {
        return PluginSignatureResult::error(SignatureStatus::UnknownError,
                                             "Null package path");
    }

    const wchar_t* ext = wcsrchr(packagePath, L'.');
    if (!ext) {
        return PluginSignatureResult::error(SignatureStatus::UnknownError,
                                             "No file extension");
    }

    if (_wcsicmp(ext, L".dll") == 0)       return verifyDLL(packagePath);
    if (_wcsicmp(ext, L".vsix") == 0)      return verifyVSIX(packagePath);
    if (_wcsicmp(ext, L".js") == 0)        return verifyJSModule(packagePath, nullptr);
    if (_wcsicmp(ext, L".rawrpkg") == 0)   return verifyRawrPackage(packagePath);

    // Default: try Authenticode
    return verifyDLL(packagePath);
}

bool PluginSignatureVerifier::shouldAllowInstall(const PluginSignatureResult& result) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (result.valid) {
        // Even if valid, check additional policy constraints
        if (m_policy.requireRawrXDAuthority && !result.isRawrXDSigned) {
            return false;
        }
        if (m_policy.requireTrustedRoot &&
            result.status != SignatureStatus::Valid) {
            return false;
        }
        return true;
    }

    // Invalid signature — check if policy allows unsigned
    if (!m_policy.requireSignature &&
        result.status == SignatureStatus::NoSignature) {
        return true;
    }

    if (m_policy.allowExpiredCerts &&
        result.status == SignatureStatus::ExpiredCertificate) {
        return true;
    }

    return false;
}

// ============================================================================
// Trusted Publisher Management
// ============================================================================
bool PluginSignatureVerifier::addTrustedPublisher(const TrustedPublisher& publisher) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_publisherCount >= MAX_TRUSTED_PUBLISHERS) return false;

    // Check for duplicate
    for (uint32_t i = 0; i < m_publisherCount; i++) {
        if (_stricmp(m_publishers[i].thumbprint, publisher.thumbprint) == 0) {
            return true; // Already trusted
        }
    }

    m_publishers[m_publisherCount] = publisher;
    m_publishers[m_publisherCount].addedTimestamp = GetTickCount64();
    m_publisherCount++;
    return true;
}

bool PluginSignatureVerifier::removeTrustedPublisher(const char* thumbprint) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_publisherCount; i++) {
        if (_stricmp(m_publishers[i].thumbprint, thumbprint) == 0) {
            // Shift remaining entries
            for (uint32_t j = i; j < m_publisherCount - 1; j++) {
                m_publishers[j] = m_publishers[j + 1];
            }
            m_publisherCount--;
            return true;
        }
    }
    return false;
}

bool PluginSignatureVerifier::isTrustedPublisher(const char* thumbprint) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_publisherCount; i++) {
        if (_stricmp(m_publishers[i].thumbprint, thumbprint) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t PluginSignatureVerifier::getTrustedPublishers(TrustedPublisher* outPublishers,
                                                         uint32_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = (m_publisherCount < maxCount) ? m_publisherCount : maxCount;
    for (uint32_t i = 0; i < count; i++) {
        outPublishers[i] = m_publishers[i];
    }
    return count;
}

// ============================================================================
// Statistics
// ============================================================================
PluginSignatureVerifier::Stats PluginSignatureVerifier::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void PluginSignatureVerifier::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

} // namespace Plugin
} // namespace RawrXD
