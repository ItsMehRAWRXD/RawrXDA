// ============================================================================
// update_signature.h — Auto-Update Signature Verification & Atomic Swap
// ============================================================================
//
// PURPOSE:
//   Cryptographic verification of downloaded update binaries before applying.
//   Uses WinVerifyTrust (Authenticode) + embedded RSA-4096 manifest signature.
//   Atomic swap via SelfPatchEngine (asm_spengine_apply / asm_spengine_rollback).
//
// JSON Manifest Format (update_manifest.json):
//   {
//     "version": "7.5.0",
//     "tag": "v7.5.0",
//     "files": [
//       { "name": "RawrXD_Gold.exe", "sha256": "...", "size": 47185920 },
//       { "name": "RawrXD-Win32IDE.exe", "sha256": "...", "size": 46137344 }
//     ],
//     "signature": "<base64 RSA-4096 of SHA-256 of canonical JSON sans this field>",
//     "releaseNotes": "...",
//     "minVersion": "7.0.0"
//   }
//
// PATTERN:   PatchResult-compatible, no exceptions, no std::function
// THREADING: All operations on caller thread unless noted
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Update {

// ============================================================================
// Signature Verification Result
// ============================================================================
struct SignatureResult {
    bool        valid;           // Signature verified successfully
    bool        trusted;         // Certificate chain trusted (Authenticode)
    const char* detail;          // Human-readable status
    int         errorCode;       // 0 on success, Win32/NTSTATUS on failure
    uint64_t    fileHash[4];     // SHA-256 of verified file (4x uint64 = 256 bits)

    static SignatureResult ok(const char* msg = "Signature valid") {
        SignatureResult r{};
        r.valid   = true;
        r.trusted = true;
        r.detail  = msg;
        r.errorCode = 0;
        return r;
    }

    static SignatureResult error(const char* msg, int code = -1) {
        SignatureResult r{};
        r.valid   = false;
        r.trusted = false;
        r.detail  = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Manifest File Entry
// ============================================================================
struct ManifestFileEntry {
    char     name[260];       // Filename (e.g., "RawrXD_Gold.exe")
    char     sha256[65];      // Hex-encoded SHA-256 hash
    uint64_t fileSize;        // Expected file size in bytes
};

// ============================================================================
// Update Manifest (parsed from JSON)
// ============================================================================
static constexpr int MAX_MANIFEST_FILES = 16;

struct UpdateManifest {
    char     version[32];        // e.g., "7.5.0"
    char     tag[64];            // e.g., "v7.5.0"
    char     minVersion[32];     // Minimum version that can upgrade
    char     releaseNotes[2048]; // Truncated release notes
    char     signature[1024];    // Base64-encoded RSA-4096 signature

    ManifestFileEntry files[MAX_MANIFEST_FILES];
    int      fileCount;

    bool     parsed;             // Successfully parsed
    char     parseError[256];    // Error message if !parsed
};

// ============================================================================
// Atomic Swap State
// ============================================================================
enum class SwapState : uint32_t {
    Idle            = 0,
    Downloading     = 1,
    Verifying       = 2,
    Staging         = 3,
    Swapping        = 4,
    RollingBack     = 5,
    Complete        = 6,
    Failed          = 7
};

struct AtomicSwapResult {
    bool        success;
    SwapState   finalState;
    const char* detail;
    int         errorCode;
    int         filesSwapped;
    int         filesRolledBack;

    static AtomicSwapResult ok(int count) {
        AtomicSwapResult r{};
        r.success       = true;
        r.finalState    = SwapState::Complete;
        r.detail        = "Update applied successfully";
        r.errorCode     = 0;
        r.filesSwapped  = count;
        r.filesRolledBack = 0;
        return r;
    }

    static AtomicSwapResult error(const char* msg, SwapState state, int code = -1) {
        AtomicSwapResult r{};
        r.success       = false;
        r.finalState    = state;
        r.detail        = msg;
        r.errorCode     = code;
        r.filesSwapped  = 0;
        r.filesRolledBack = 0;
        return r;
    }
};

// ============================================================================
// Update Progress Callback — function pointer, NOT std::function
// ============================================================================
typedef void (*UpdateProgressCallback)(SwapState state, int progressPct,
                                        const char* detail, void* userData);

// ============================================================================
// UpdateSignatureVerifier — Singleton
// ============================================================================
class UpdateSignatureVerifier {
public:
    static UpdateSignatureVerifier& instance();

    // ======================================================================
    // Manifest Operations
    // ======================================================================

    /// Parse a JSON update manifest from a buffer
    UpdateManifest parseManifest(const char* json, size_t jsonLen);

    /// Fetch and parse the latest manifest from the update server
    /// Uses WinHTTP GET to the configured manifest URL
    UpdateManifest fetchManifest();

    /// Set the manifest URL (default: GitHub Releases API)
    void setManifestUrl(const char* url);

    // ======================================================================
    // Signature Verification
    // ======================================================================

    /// Verify the RSA-4096 signature of a manifest
    /// Checks: SHA-256 of canonical JSON → RSA verify with embedded public key
    SignatureResult verifyManifestSignature(const UpdateManifest& manifest,
                                            const char* rawJson, size_t rawJsonLen);

    /// Verify a downloaded file against its manifest entry
    /// Checks: SHA-256 file hash + Authenticode (WinVerifyTrust)
    SignatureResult verifyFile(const wchar_t* filePath,
                               const ManifestFileEntry& expected);

    /// Verify Authenticode signature on an executable (WinVerifyTrust)
    SignatureResult verifyAuthenticode(const wchar_t* filePath);

    /// Compute SHA-256 of a file and compare against expected hex
    SignatureResult verifySHA256(const wchar_t* filePath, const char* expectedHex);

    // ======================================================================
    // Atomic Swap via SelfPatchEngine
    // ======================================================================

    /// Apply a verified update atomically:
    ///   1. Stage new files to .new suffix
    ///   2. Rename current files to .bak suffix
    ///   3. Rename .new files to current names
    ///   4. On failure: rollback .bak → original
    ///   5. Uses asm_spengine_apply for live-memory patching if binary is running
    AtomicSwapResult applyUpdate(const UpdateManifest& manifest,
                                  const wchar_t* downloadDir,
                                  UpdateProgressCallback callback = nullptr,
                                  void* userData = nullptr);

    /// Rollback a failed update (restore .bak files)
    AtomicSwapResult rollbackUpdate();

    /// Get the current swap state
    SwapState getState() const { return m_state; }

    // ======================================================================
    // Configuration
    // ======================================================================

    /// Set the RSA-4096 public key for manifest verification (DER or PEM)
    void setPublicKey(const uint8_t* keyData, size_t keyLen);

    /// Set the staging directory for downloaded updates
    void setStagingDir(const wchar_t* dir);

private:
    UpdateSignatureVerifier();
    ~UpdateSignatureVerifier();
    UpdateSignatureVerifier(const UpdateSignatureVerifier&) = delete;
    UpdateSignatureVerifier& operator=(const UpdateSignatureVerifier&) = delete;

    // Internal SHA-256 using BCrypt (Windows CNG)
    bool computeSHA256(const void* data, size_t dataLen, uint8_t outHash[32]);
    bool computeFileSHA256(const wchar_t* filePath, uint8_t outHash[32]);

    // Internal RSA-4096 verify using BCrypt
    bool verifyRSA4096(const uint8_t* hash, size_t hashLen,
                        const uint8_t* signature, size_t sigLen);

    // Hex encoding/decoding
    static void hashToHex(const uint8_t hash[32], char outHex[65]);
    static bool hexToHash(const char* hex, uint8_t outHash[32]);

    SwapState       m_state;
    char            m_manifestUrl[512];
    wchar_t         m_stagingDir[260];
    uint8_t         m_publicKey[4096];
    size_t          m_publicKeyLen;
    CRITICAL_SECTION m_cs;

    // Backup tracking for rollback
    struct BackupEntry {
        wchar_t originalPath[260];
        wchar_t backupPath[260];
    };
    BackupEntry     m_backups[MAX_MANIFEST_FILES];
    int             m_backupCount;
};

} // namespace Update
} // namespace RawrXD
