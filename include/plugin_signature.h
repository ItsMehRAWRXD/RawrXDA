<<<<<<< HEAD
// ============================================================================
// plugin_signature.h — Plugin Signature Enforcement
// ============================================================================
//
// PURPOSE:
//   Enforce cryptographic signature verification for all installable plugins,
//   extensions, and marketplace packages. Wraps WinVerifyTrust (Authenticode)
//   and RSA-4096 manifest verification.
//
//   Addresses the TODO stub at extension_marketplace.cpp L710:
//     "Signature verification not implemented"
//
// PATTERN:  PatchResult-compatible, no exceptions, no std::function
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Plugin {

// ============================================================================
// Signature Status
// ============================================================================
enum class SignatureStatus : uint32_t {
    Valid                = 0,  // Signature valid and trusted
    InvalidSignature     = 1,  // Cryptographic signature invalid
    ExpiredCertificate   = 2,  // Certificate has expired
    UntrustedRoot        = 3,  // Certificate chain leads to untrusted root
    NoSignature          = 4,  // Binary/package is not signed
    Tampered             = 5,  // Content modified after signing
    RevokedCertificate   = 6,  // Certificate has been revoked
    UnknownError         = 7,  // WinVerifyTrust or CNG error
};

// ============================================================================
// Signature Verification Result
// ============================================================================
struct PluginSignatureResult {
    bool            valid;
    SignatureStatus status;
    const char*     detail;
    int             errorCode;
    char            subjectName[256];     // Certificate subject (CN)
    char            issuerName[256];      // Certificate issuer
    uint64_t        expiryTimestamp;      // Certificate expiry (Unix epoch)
    char            thumbprint[41];       // SHA-1 certificate thumbprint (hex)
    bool            isRawrXDSigned;       // Signed by RawrXD authority

    static PluginSignatureResult ok(const char* subject) {
        PluginSignatureResult r{};
        r.valid = true;
        r.status = SignatureStatus::Valid;
        r.detail = "Signature verified";
        r.errorCode = 0;
        if (subject) strncpy_s(r.subjectName, sizeof(r.subjectName), subject, _TRUNCATE);
        return r;
    }

    static PluginSignatureResult error(SignatureStatus st, const char* msg, int code = -1) {
        PluginSignatureResult r{};
        r.valid = false;
        r.status = st;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Plugin Package Type
// ============================================================================
enum class PackageType : uint32_t {
    NativeDLL       = 0,   // .dll — Authenticode verification
    VSIX            = 1,   // .vsix — ZIP with signature manifest
    JSModule        = 2,   // .js — SHA-256 hash check against manifest
    PythonPackage   = 3,   // .whl/.egg — PGP/GPG signature
    RawrPlugin      = 4,   // .rawrpkg — Custom format with RSA-4096
};

// ============================================================================
// Enforcement Policy
// ============================================================================
struct SignaturePolicy {
    bool     requireSignature;         // Block unsigned packages
    bool     requireTrustedRoot;       // Require trusted CA chain
    bool     requireRawrXDAuthority;   // Only accept RawrXD-signed packages
    bool     allowExpiredCerts;        // Allow expired but valid signatures
    bool     checkCertRevocation;      // Online CRL/OCSP check
    bool     logBlockedInstalls;       // Log attempted unsigned installs
    uint32_t maxCertChainDepth;        // Maximum certificate chain depth
};

// ============================================================================
// Trusted Publisher Entry
// ============================================================================
struct TrustedPublisher {
    char     subjectName[256];         // Certificate CN
    char     thumbprint[41];           // SHA-1 thumbprint (hex)
    bool     allowAllExtensions;       // Trust all packages from this publisher
    uint64_t addedTimestamp;           // When this publisher was trusted
};

static constexpr uint32_t MAX_TRUSTED_PUBLISHERS = 64;

// ============================================================================
// PluginSignatureVerifier — Singleton
// ============================================================================
class PluginSignatureVerifier {
public:
    static PluginSignatureVerifier& instance();

    // ======================================================================
    // Lifecycle
    // ======================================================================

    /// Initialize the verifier with default policy
    bool initialize();

    /// Initialize with custom policy
    bool initialize(const SignaturePolicy& policy);

    /// Shutdown
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // ======================================================================
    // Verification
    // ======================================================================

    /// Verify a native DLL (Authenticode via WinVerifyTrust)
    PluginSignatureResult verifyDLL(const wchar_t* dllPath);

    /// Verify a VSIX package
    PluginSignatureResult verifyVSIX(const wchar_t* vsixPath);

    /// Verify a JS module against a signed manifest
    PluginSignatureResult verifyJSModule(const wchar_t* jsPath,
                                          const char* expectedHash);

    /// Verify a .rawrpkg custom package
    PluginSignatureResult verifyRawrPackage(const wchar_t* packagePath);

    /// Generic verify — auto-detects package type from extension
    PluginSignatureResult verify(const wchar_t* packagePath);

    /// Check if installation should be allowed based on policy
    bool shouldAllowInstall(const PluginSignatureResult& result) const;

    // ======================================================================
    // Policy Management
    // ======================================================================

    /// Get current policy
    SignaturePolicy getPolicy() const;

    /// Update policy
    void setPolicy(const SignaturePolicy& policy);

    /// Create default policy for each security level
    static SignaturePolicy createStrictPolicy();       // Enterprise
    static SignaturePolicy createStandardPolicy();     // Pro
    static SignaturePolicy createRelaxedPolicy();      // Community

    // ======================================================================
    // Trusted Publisher Management
    // ======================================================================

    /// Add a trusted publisher by certificate thumbprint
    bool addTrustedPublisher(const TrustedPublisher& publisher);

    /// Remove a trusted publisher
    bool removeTrustedPublisher(const char* thumbprint);

    /// Check if a certificate thumbprint is trusted
    bool isTrustedPublisher(const char* thumbprint) const;

    /// Get list of trusted publishers
    uint32_t getTrustedPublishers(TrustedPublisher* outPublishers,
                                   uint32_t maxCount) const;

    // ======================================================================
    // Statistics
    // ======================================================================

    struct Stats {
        uint64_t totalVerifications;
        uint64_t validSignatures;
        uint64_t invalidSignatures;
        uint64_t blockedInstalls;
        uint64_t allowedInstalls;
    };

    Stats getStats() const;
    void resetStats();

private:
    PluginSignatureVerifier();
    ~PluginSignatureVerifier();
    PluginSignatureVerifier(const PluginSignatureVerifier&) = delete;
    PluginSignatureVerifier& operator=(const PluginSignatureVerifier&) = delete;

    // Internal WinVerifyTrust wrapper
    PluginSignatureResult winVerifyTrustCheck(const wchar_t* filePath);

    // Extract certificate info from a signed file
    bool extractCertInfo(const wchar_t* filePath, char* subject, size_t subjectLen,
                          char* issuer, size_t issuerLen, char* thumbprint,
                          uint64_t* expiry);

    // SHA-256 hash computation (BCrypt CNG)
    bool computeFileSHA256(const wchar_t* filePath, char outHex[65]);

    bool                    m_initialized;
    mutable std::mutex      m_mutex;
    SignaturePolicy         m_policy;
    TrustedPublisher        m_publishers[MAX_TRUSTED_PUBLISHERS];
    uint32_t                m_publisherCount;
    Stats                   m_stats;
};

} // namespace Plugin
} // namespace RawrXD
=======
// ============================================================================
// plugin_signature.h — Plugin Signature Enforcement
// ============================================================================
//
// PURPOSE:
//   Enforce cryptographic signature verification for all installable plugins,
//   extensions, and marketplace packages. Wraps WinVerifyTrust (Authenticode)
//   and RSA-4096 manifest verification.
//
//   Addresses signature verification at extension_marketplace.cpp L710.
//
// PATTERN:  PatchResult-compatible, no exceptions, no std::function
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Plugin {

// ============================================================================
// Signature Status
// ============================================================================
enum class SignatureStatus : uint32_t {
    Valid                = 0,  // Signature valid and trusted
    InvalidSignature     = 1,  // Cryptographic signature invalid
    ExpiredCertificate   = 2,  // Certificate has expired
    UntrustedRoot        = 3,  // Certificate chain leads to untrusted root
    NoSignature          = 4,  // Binary/package is not signed
    Tampered             = 5,  // Content modified after signing
    RevokedCertificate   = 6,  // Certificate has been revoked
    UnknownError         = 7,  // WinVerifyTrust or CNG error
};

// ============================================================================
// Signature Verification Result
// ============================================================================
struct PluginSignatureResult {
    bool            valid;
    SignatureStatus status;
    const char*     detail;
    int             errorCode;
    char            subjectName[256];     // Certificate subject (CN)
    char            issuerName[256];      // Certificate issuer
    uint64_t        expiryTimestamp;      // Certificate expiry (Unix epoch)
    char            thumbprint[41];       // SHA-1 certificate thumbprint (hex)
    bool            isRawrXDSigned;       // Signed by RawrXD authority

    static PluginSignatureResult ok(const char* subject) {
        PluginSignatureResult r{};
        r.valid = true;
        r.status = SignatureStatus::Valid;
        r.detail = "Signature verified";
        r.errorCode = 0;
        if (subject) strncpy_s(r.subjectName, sizeof(r.subjectName), subject, _TRUNCATE);
        return r;
    }

    static PluginSignatureResult error(SignatureStatus st, const char* msg, int code = -1) {
        PluginSignatureResult r{};
        r.valid = false;
        r.status = st;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Plugin Package Type
// ============================================================================
enum class PackageType : uint32_t {
    NativeDLL       = 0,   // .dll — Authenticode verification
    VSIX            = 1,   // .vsix — ZIP with signature manifest
    JSModule        = 2,   // .js — SHA-256 hash check against manifest
    PythonPackage   = 3,   // .whl/.egg — PGP/GPG signature
    RawrPlugin      = 4,   // .rawrpkg — Custom format with RSA-4096
};

// ============================================================================
// Enforcement Policy
// ============================================================================
struct SignaturePolicy {
    bool     requireSignature;         // Block unsigned packages
    bool     requireTrustedRoot;       // Require trusted CA chain
    bool     requireRawrXDAuthority;   // Only accept RawrXD-signed packages
    bool     allowExpiredCerts;        // Allow expired but valid signatures
    bool     checkCertRevocation;      // Online CRL/OCSP check
    bool     logBlockedInstalls;       // Log attempted unsigned installs
    uint32_t maxCertChainDepth;        // Maximum certificate chain depth
};

// ============================================================================
// Trusted Publisher Entry
// ============================================================================
struct TrustedPublisher {
    char     subjectName[256];         // Certificate CN
    char     thumbprint[41];           // SHA-1 thumbprint (hex)
    bool     allowAllExtensions;       // Trust all packages from this publisher
    uint64_t addedTimestamp;           // When this publisher was trusted
};

static constexpr uint32_t MAX_TRUSTED_PUBLISHERS = 64;

// ============================================================================
// PluginSignatureVerifier — Singleton
// ============================================================================
class PluginSignatureVerifier {
public:
    static PluginSignatureVerifier& instance();

    // ======================================================================
    // Lifecycle
    // ======================================================================

    /// Initialize the verifier with default policy
    bool initialize();

    /// Initialize with custom policy
    bool initialize(const SignaturePolicy& policy);

    /// Shutdown
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // ======================================================================
    // Verification
    // ======================================================================

    /// Verify a native DLL (Authenticode via WinVerifyTrust)
    PluginSignatureResult verifyDLL(const wchar_t* dllPath);

    /// Verify a VSIX package
    PluginSignatureResult verifyVSIX(const wchar_t* vsixPath);

    /// Verify a JS module against a signed manifest
    PluginSignatureResult verifyJSModule(const wchar_t* jsPath,
                                          const char* expectedHash);

    /// Verify a .rawrpkg custom package
    PluginSignatureResult verifyRawrPackage(const wchar_t* packagePath);

    /// Generic verify — auto-detects package type from extension
    PluginSignatureResult verify(const wchar_t* packagePath);

    /// Check if installation should be allowed based on policy
    bool shouldAllowInstall(const PluginSignatureResult& result) const;

    // ======================================================================
    // Policy Management
    // ======================================================================

    /// Get current policy
    SignaturePolicy getPolicy() const;

    /// Update policy
    void setPolicy(const SignaturePolicy& policy);

    /// Create default policy for each security level
    static SignaturePolicy createStrictPolicy();       // Enterprise
    static SignaturePolicy createStandardPolicy();     // Pro
    static SignaturePolicy createRelaxedPolicy();      // Community

    // ======================================================================
    // Trusted Publisher Management
    // ======================================================================

    /// Add a trusted publisher by certificate thumbprint
    bool addTrustedPublisher(const TrustedPublisher& publisher);

    /// Remove a trusted publisher
    bool removeTrustedPublisher(const char* thumbprint);

    /// Check if a certificate thumbprint is trusted
    bool isTrustedPublisher(const char* thumbprint) const;

    /// Get list of trusted publishers
    uint32_t getTrustedPublishers(TrustedPublisher* outPublishers,
                                   uint32_t maxCount) const;

    // ======================================================================
    // Statistics
    // ======================================================================

    struct Stats {
        uint64_t totalVerifications;
        uint64_t validSignatures;
        uint64_t invalidSignatures;
        uint64_t blockedInstalls;
        uint64_t allowedInstalls;
    };

    Stats getStats() const;
    void resetStats();

private:
    PluginSignatureVerifier();
    ~PluginSignatureVerifier();
    PluginSignatureVerifier(const PluginSignatureVerifier&) = delete;
    PluginSignatureVerifier& operator=(const PluginSignatureVerifier&) = delete;

    // Internal WinVerifyTrust wrapper
    PluginSignatureResult winVerifyTrustCheck(const wchar_t* filePath);

    // Extract certificate info from a signed file
    bool extractCertInfo(const wchar_t* filePath, char* subject, size_t subjectLen,
                          char* issuer, size_t issuerLen, char* thumbprint,
                          uint64_t* expiry);

    // SHA-256 hash computation (BCrypt CNG)
    bool computeFileSHA256(const wchar_t* filePath, char outHex[65]);

    bool                    m_initialized;
    mutable std::mutex      m_mutex;
    SignaturePolicy         m_policy;
    TrustedPublisher        m_publishers[MAX_TRUSTED_PUBLISHERS];
    uint32_t                m_publisherCount;
    Stats                   m_stats;
};

} // namespace Plugin
} // namespace RawrXD
>>>>>>> origin/main
