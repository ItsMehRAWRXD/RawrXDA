// ============================================================================
// camellia256_bridge.hpp — C++ Bridge for MASM Camellia-256 Encryption Engine
// ============================================================================
//
// PURPOSE:
//   Provides C++ interface to the pure MASM x64 Camellia-256 encryption
//   engine (RawrXD_Camellia256.asm). Used for workspace encryption on IDE
//   startup and for the Enterprise Airgapped Workspace Encryption feature.
//
//   The MASM engine implements:
//     - Full Camellia-256 block cipher (24-round Feistel, 4 S-box layers)
//     - CTR (Counter) mode for streaming encryption
//     - HWID-derived key generation (computer name + volume serial)
//     - File-level encrypt/decrypt with 16-byte CTR nonce header
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// ============================================================================
//  MASM extern declarations (RawrXD_Camellia256.asm exports)
// ============================================================================

extern "C" {
    // Initialize engine with machine-identity-derived 256-bit key
    int asm_camellia256_init();

    // Expand an explicit 256-bit key into subkeys
    int asm_camellia256_set_key(const uint8_t* key32);

    // Single-block encrypt/decrypt (128-bit = 16 bytes)
    int asm_camellia256_encrypt_block(const uint8_t* plaintext16,
                                      uint8_t* ciphertext16);
    int asm_camellia256_decrypt_block(const uint8_t* ciphertext16,
                                      uint8_t* plaintext16);

    // CTR-mode encrypt/decrypt in-place
    // nonce16 is updated (incremented) on return
    int asm_camellia256_encrypt_ctr(uint8_t* buffer, size_t length,
                                    uint8_t* nonce16);
    int asm_camellia256_decrypt_ctr(uint8_t* buffer, size_t length,
                                    uint8_t* nonce16);

    // File-level encrypt/decrypt (CTR mode, nonce prepended to output)
    int asm_camellia256_encrypt_file(const char* inputPath,
                                     const char* outputPath);
    int asm_camellia256_decrypt_file(const char* inputPath,
                                     const char* outputPath);

    // Engine status query
    int asm_camellia256_get_status(void* status32);

    // Secure shutdown (zeros all key material)
    int asm_camellia256_shutdown();

    // RFC 3713 self-test (known-answer verification)
    int asm_camellia256_self_test();

    // Copy HMAC authentication key to caller buffer (32 bytes)
    int asm_camellia256_get_hmac_key(uint8_t* hmacKey32);
}

// ============================================================================
//  Status codes (must match RawrXD_Camellia256.asm constants)
// ============================================================================

namespace RawrXD {
namespace Crypto {

enum class CamelliaStatus : int {
    OK            =  0,
    ErrNoKey      = -1,
    ErrNullPtr    = -2,
    ErrFileOpen   = -3,
    ErrFileRead   = -4,
    ErrFileWrite  = -5,
    ErrAlloc      = -6,
    ErrAuth       = -7,
    ErrSelfTest   = -8
};

// ============================================================================
//  Engine status structure (matches MASM layout)
// ============================================================================

struct CamelliaEngineStatus {
    uint32_t initialized;
    uint32_t reserved;
    uint64_t blocksEncrypted;
    uint64_t blocksDecrypted;
    uint64_t filesProcessed;
};

// ============================================================================
//  PatchResult-compatible result type
// ============================================================================

struct CamelliaResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static CamelliaResult ok(const char* msg) {
        return { true, msg, 0 };
    }
    static CamelliaResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
//  Camellia256Bridge — High-level C++ wrapper
// ============================================================================

class Camellia256Bridge {
public:
    // Singleton access (engine state lives in MASM BSS section)
    static Camellia256Bridge& instance();

    // Initialize with HWID-derived key (auto-generates from machine identity)
    CamelliaResult initialize();

    // Initialize with explicit 256-bit key
    CamelliaResult initializeWithKey(const uint8_t* key32);

    // Check if engine is ready
    bool isInitialized() const;

    // Encrypt/decrypt workspace directory (all files recursively)
    // Creates .camellia files alongside originals, or replaces in-place
    CamelliaResult encryptWorkspace(const std::string& workspacePath,
                                     bool inPlace = false);
    CamelliaResult decryptWorkspace(const std::string& workspacePath,
                                     bool inPlace = false);

    // Encrypt/decrypt a single file
    CamelliaResult encryptFile(const std::string& inputPath,
                                const std::string& outputPath);
    CamelliaResult decryptFile(const std::string& inputPath,
                                const std::string& outputPath);

    // Authenticated encrypt/decrypt (Encrypt-then-MAC with HMAC-SHA256)
    // File format: [4B magic][4B version][16B nonce][32B HMAC][ciphertext]
    // HMAC covers: magic + version + nonce + ciphertext
    CamelliaResult encryptFileAuthenticated(const std::string& inputPath,
                                             const std::string& outputPath);
    CamelliaResult decryptFileAuthenticated(const std::string& inputPath,
                                             const std::string& outputPath);

    // Run RFC 3713 self-test
    CamelliaResult selfTest();

    // Encrypt/decrypt a memory buffer in-place (CTR mode)
    CamelliaResult encryptBuffer(uint8_t* data, size_t length);
    CamelliaResult decryptBuffer(uint8_t* data, size_t length);

    // Get engine statistics
    CamelliaEngineStatus getStatus() const;

    // Secure shutdown — zeros all keying material
    CamelliaResult shutdown();

    // Get human-readable status string for UI display
    std::string getStatusString() const;

private:
    Camellia256Bridge() = default;
    ~Camellia256Bridge() = default;
    Camellia256Bridge(const Camellia256Bridge&) = delete;
    Camellia256Bridge& operator=(const Camellia256Bridge&) = delete;

    bool m_initialized = false;

    // HMAC key cache (32 bytes, obtained from MASM layer)
    uint8_t m_hmacKey[32] = {};
    bool m_hmacKeyLoaded = false;

    // Load HMAC key from MASM layer (cached)
    bool loadHmacKey();

    // Compute HMAC-SHA256 using BCrypt
    bool computeHMACSHA256(const uint8_t* data, uint32_t dataLen,
                           uint8_t* hmacOut32) const;

    // Constant-time comparison (32 bytes)
    static bool constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len);

    // Recursively enumerate files in a directory
    void enumerateFiles(const std::string& dirPath,
                        std::vector<std::string>& outFiles) const;
};

} // namespace Crypto
} // namespace RawrXD
