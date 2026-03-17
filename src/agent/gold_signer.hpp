#pragma once
// =============================================================================
// gold_signer.hpp — GOLD_SIGN: EV Code Signing Engine (C++20 / Win32)
// =============================================================================
// Production-grade EV (Extended Validation) certificate signing via:
//   1. Certificate store thumbprint (hardware-bound private key)
//   2. Azure Trusted Signing (cloud HSM)
//   3. Hardware CSP token (SafeNet eToken / YubiKey)
//
// EV certs never exist as PFX files — the private key is always on hardware.
// This class wraps signtool.exe invocations with proper EV parameters.
// =============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace RawrXD {

// ── Signing mode ─────────────────────────────────────────────────────────────
enum class GoldSignMode : uint8_t {
    Thumbprint,          // /sha1 <thumbprint> /s <store>
    SubjectName,         // /n <subject> /s <store>
    HardwareToken,       // /csp <provider> /kc <container>
    AzureTrustedSigning, // AzureSignTool with cloud HSM
    AutoDetect           // Scan cert store for EV OID
};

// ── Configuration ────────────────────────────────────────────────────────────
struct GoldSignConfig {
    GoldSignMode mode          = GoldSignMode::AutoDetect;

    // Certificate store
    std::string  thumbprint;          // SHA-1 thumbprint of EV cert
    std::string  subjectName;         // Subject CN for /n selection
    std::string  certStore    = "My"; // Certificate store name

    // Hardware token (SafeNet / YubiKey)
    std::string  cspName;             // Cryptographic Service Provider
    std::string  keyContainer;        // Key container on the CSP
    std::string  tokenPin;            // Hardware token PIN

    // Azure Trusted Signing
    std::string  azureTenantId;
    std::string  azureEndpoint;
    std::string  azureProfile;

    // Signing parameters
    std::string  digestAlgorithm = "SHA256";
    std::string  timestampUrl    = "http://timestamp.digicert.com";
    std::string  crossCertPath;       // Optional cross-cert for kernel drivers
    std::string  signtoolPath;        // Explicit signtool.exe override

    // Behavior
    bool         dualSign      = false;  // SHA1 + SHA256 (legacy compat)
    bool         skipSigned    = true;   // Skip already-signed binaries
    bool         verifyAfter   = true;   // Verify each binary after signing
};

// ── Per-file result ──────────────────────────────────────────────────────────
struct GoldSignResult {
    std::string  filePath;
    bool         signed_ok     = false;
    bool         verified_ok   = false;
    std::string  sha256;             // SHA-256 of the file post-signing
    std::string  signerSubject;      // Subject from the signing certificate
    uint64_t     fileSizeBytes = 0;
    std::string  errorDetail;
};

// ── Attestation manifest ─────────────────────────────────────────────────────
struct GoldSignAttestation {
    std::string              timestamp;
    std::string              hostname;
    std::string              digest;
    std::string              thumbprint;
    std::string              timestampServer;
    bool                     dualSign = false;
    std::vector<GoldSignResult> files;
};

// ── GOLD_SIGN Engine ─────────────────────────────────────────────────────────
class GoldSigner {
public:
    explicit GoldSigner(const GoldSignConfig& config);
    ~GoldSigner() = default;

    // Non-copyable
    GoldSigner(const GoldSigner&) = delete;
    GoldSigner& operator=(const GoldSigner&) = delete;

    // ── Core operations ──────────────────────────────────────────────────
    /// Sign a single binary. Returns result with signed_ok / verified_ok.
    GoldSignResult signFile(const std::string& filePath);

    /// Batch-sign all EXE/DLL/MSI in a directory (recursive).
    std::vector<GoldSignResult> signDirectory(const std::string& dirPath);

    /// Verify an existing signature without signing.
    bool verifyFile(const std::string& filePath);

    /// Write GOLD_SIGN_ATTESTATION.json to the given directory.
    bool writeAttestation(const std::string& directory,
                          const std::vector<GoldSignResult>& results);

    // ── Introspection ────────────────────────────────────────────────────
    /// Auto-detect an EV certificate in the cert store. Returns thumbprint.
    std::string detectEVCertificate();

    /// Resolve signtool.exe path. Empty string if not found.
    std::string resolveSignTool();

    /// Get human-readable mode string.
    const char* modeName() const;

    /// Get last error (set on failure).
    const std::string& lastError() const { return m_lastError; }

    // ── Callbacks ────────────────────────────────────────────────────────
    std::function<void(const std::string& file, bool ok)> onFileSigned;
    std::function<void(const std::string& file, bool ok)> onFileVerified;
    std::function<void(const std::string& msg)>           onError;

private:
    GoldSignConfig m_config;
    std::string    m_signtoolPath;
    std::string    m_lastError;

    // Internal helpers
    bool           execProcess(const std::string& exe,
                               const std::vector<std::string>& args,
                               int timeoutMs = 120000);
    std::string    buildSignArgs(const std::string& filePath,
                                 const std::string& algo);
    bool           isAlreadySigned(const std::string& filePath);
    std::string    computeSHA256(const std::string& filePath);
    void           setError(const std::string& msg);
};

} // namespace RawrXD
