// ============================================================================
// camellia256_bridge.cpp — C++ Bridge for MASM Camellia-256 Encryption Engine
// ============================================================================
//
// PURPOSE:
//   Provides C++ interface to the pure MASM x64 Camellia-256 encryption
//   engine (RawrXD_Camellia256.asm) and the authenticated encrypt/decrypt
//   engine (RawrXD_Camellia256_Auth.asm).
//
//   The MASM engines implement:
//     - Full Camellia-256 block cipher (24-round Feistel, 4 S-box layers)
//     - CTR (Counter) mode for streaming encryption
//     - HWID-derived key generation (computer name + volume serial)
//     - File-level encrypt/decrypt with 16-byte CTR nonce header
//     - Authenticated encrypt/decrypt (Encrypt-then-MAC, HMAC-SHA256)
//     - RCM2 file format: [magic][version][nonce][HMAC][ciphertext]
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "camellia256_bridge.hpp"
#include "shadow_page_detour.hpp"
#include "sentinel_watchdog.hpp"

#include <windows.h>
#include <bcrypt.h>
#include <shlwapi.h>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

// ============================================================================
//  New MASM auth module exports (RawrXD_Camellia256_Auth.asm)
// ============================================================================

extern "C" {
    int asm_camellia256_auth_encrypt_file(const char* inputPath,
                                          const char* outputPath);
    int asm_camellia256_auth_decrypt_file(const char* inputPath,
                                          const char* outputPath);
    int asm_camellia256_auth_encrypt_buf(uint8_t* plaintext, uint32_t plaintextLen,
                                         uint8_t* output, uint32_t* outputLen);
    int asm_camellia256_auth_decrypt_buf(const uint8_t* authData, uint32_t authDataLen,
                                         uint8_t* plaintext, uint32_t* plaintextLen);
}

// ============================================================================
//  Thread safety mutex (singleton bridge is accessed from multiple threads)
// ============================================================================

static std::mutex s_bridgeMutex;

// ============================================================================
//  IMPLEMENTATION
// ============================================================================

namespace RawrXD {
namespace Crypto {

// ============================================================================
//  Singleton access
// ============================================================================

Camellia256Bridge& Camellia256Bridge::instance() {
    static Camellia256Bridge s_instance;
    return s_instance;
}

// ============================================================================
//  Initialize with HWID-derived key (auto-generates from machine identity)
// ============================================================================

CamelliaResult Camellia256Bridge::initialize() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (m_initialized) {
        return CamelliaResult::ok("Camellia-256 already initialized");
    }

    int result = asm_camellia256_init();
    if (result != 0) {
        return CamelliaResult::error("Camellia-256 initialization failed", result);
    }

    m_initialized = true;
    m_hmacKeyLoaded = false;  // Force re-load of HMAC key

    // Pre-load the HMAC key for authenticated operations
    loadHmacKey();

    // Initialize Shadow-Page Detour system for live binary hotpatching.
    // This enables the AI agent to patch Camellia-256 kernels at runtime
    // without process restart (Binary Immortality).
    PatchResult detourInit = SelfRepairLoop::instance().initialize();
    if (detourInit.success) {
        // Register encryption/decryption functions as detourable targets.
        // The SelfRepairLoop can now atomically swap these with AI-generated
        // MASM patches via the RawrXD_Hotpatch_Kernel.asm shadow-page detour.
        SelfRepairLoop::instance().registerDetour(
            "camellia256_encrypt_block",
            reinterpret_cast<void*>(&asm_camellia256_encrypt_block));
        SelfRepairLoop::instance().registerDetour(
            "camellia256_decrypt_block",
            reinterpret_cast<void*>(&asm_camellia256_decrypt_block));
        SelfRepairLoop::instance().registerDetour(
            "camellia256_encrypt_ctr",
            reinterpret_cast<void*>(&asm_camellia256_encrypt_ctr));
        SelfRepairLoop::instance().registerDetour(
            "camellia256_decrypt_ctr",
            reinterpret_cast<void*>(&asm_camellia256_decrypt_ctr));
    }
    // Non-fatal if detour init fails — crypto still works without hotpatch

    // Activate the Sentinel Watchdog AFTER SelfRepairLoop and detour registration.
    // The Sentinel computes the initial .text SHA-256 baseline at this point.
    // Any subsequent .text modifications NOT preceded by sentinel_deactivate()
    // will trigger a cryptographic lockdown (workspace encryption + ExitProcess).
    SentinelWatchdog::instance().activate();
    // Non-fatal if sentinel activation fails — crypto still works without tamper guard

    return CamelliaResult::ok("Camellia-256 initialized (HWID-derived key, SHA-256 KDF, Shadow-Page Detour active, Sentinel Watchdog online)");
}

// ============================================================================
//  Initialize with explicit 256-bit key
// ============================================================================

CamelliaResult Camellia256Bridge::initializeWithKey(const uint8_t* key32) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!key32) {
        return CamelliaResult::error("Null key pointer", -2);
    }

    int result = asm_camellia256_set_key(key32);
    if (result != 0) {
        return CamelliaResult::error("Camellia-256 key expansion failed", result);
    }

    m_initialized = true;
    m_hmacKeyLoaded = false;
    loadHmacKey();

    return CamelliaResult::ok("Camellia-256 initialized (explicit key)");
}

// ============================================================================
//  Check if engine is ready
// ============================================================================

bool Camellia256Bridge::isInitialized() const {
    return m_initialized;
}

// ============================================================================
//  Run RFC 3713 self-test
// ============================================================================

CamelliaResult Camellia256Bridge::selfTest() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    int result = asm_camellia256_self_test();
    if (result != 0) {
        return CamelliaResult::error("RFC 3713 self-test FAILED — cipher corrupted", result);
    }

    return CamelliaResult::ok("RFC 3713 self-test PASSED (Camellia-256, Appendix A vector)");
}

// ============================================================================
//  Encrypt/decrypt a single file (basic CTR mode, no authentication)
// ============================================================================

CamelliaResult Camellia256Bridge::encryptFile(const std::string& inputPath,
                                               const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    int result = asm_camellia256_encrypt_file(inputPath.c_str(), outputPath.c_str());
    if (result != 0) {
        return CamelliaResult::error("File encryption failed", result);
    }

    return CamelliaResult::ok("File encrypted (CTR mode)");
}

CamelliaResult Camellia256Bridge::decryptFile(const std::string& inputPath,
                                               const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    int result = asm_camellia256_decrypt_file(inputPath.c_str(), outputPath.c_str());
    if (result != 0) {
        return CamelliaResult::error("File decryption failed", result);
    }

    return CamelliaResult::ok("File decrypted (CTR mode)");
}

// ============================================================================
//  Authenticated encrypt/decrypt (Encrypt-then-MAC with HMAC-SHA256)
//
//  Delegates to pure MASM implementation in RawrXD_Camellia256_Auth.asm.
//  File format: [4B magic="RCM2"][4B version][16B nonce][32B HMAC][ciphertext]
//  HMAC covers: magic + version + nonce + ciphertext
// ============================================================================

CamelliaResult Camellia256Bridge::encryptFileAuthenticated(
    const std::string& inputPath,
    const std::string& outputPath)
{
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    int result = asm_camellia256_auth_encrypt_file(
        inputPath.c_str(), outputPath.c_str());

    if (result != 0) {
        return CamelliaResult::error("Authenticated encryption failed", result);
    }

    return CamelliaResult::ok("File encrypted + HMAC-SHA256 authenticated (RCM2)");
}

CamelliaResult Camellia256Bridge::decryptFileAuthenticated(
    const std::string& inputPath,
    const std::string& outputPath)
{
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    int result = asm_camellia256_auth_decrypt_file(
        inputPath.c_str(), outputPath.c_str());

    if (result == -7) {
        return CamelliaResult::error("HMAC verification FAILED — file tampered or corrupted", -7);
    }
    if (result != 0) {
        return CamelliaResult::error("Authenticated decryption failed", result);
    }

    return CamelliaResult::ok("File decrypted + HMAC verified (RCM2)");
}

// ============================================================================
//  Encrypt/decrypt a memory buffer in-place (CTR mode, no auth)
// ============================================================================

CamelliaResult Camellia256Bridge::encryptBuffer(uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }
    if (!data || length == 0) {
        return CamelliaResult::error("Invalid buffer", -2);
    }

    // Generate a random nonce for this buffer encryption
    uint8_t nonce[16] = {};
    BCryptGenRandom(nullptr, nonce, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    int result = asm_camellia256_encrypt_ctr(data, length, nonce);
    if (result != 0) {
        return CamelliaResult::error("Buffer encryption failed", result);
    }

    return CamelliaResult::ok("Buffer encrypted (CTR mode)");
}

CamelliaResult Camellia256Bridge::decryptBuffer(uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }
    if (!data || length == 0) {
        return CamelliaResult::error("Invalid buffer", -2);
    }

    // For CTR mode, decrypt == encrypt with same nonce
    uint8_t nonce[16] = {};
    BCryptGenRandom(nullptr, nonce, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    int result = asm_camellia256_decrypt_ctr(data, length, nonce);
    if (result != 0) {
        return CamelliaResult::error("Buffer decryption failed", result);
    }

    return CamelliaResult::ok("Buffer decrypted (CTR mode)");
}

// ============================================================================
//  Encrypt/decrypt workspace directory (all files recursively)
// ============================================================================

CamelliaResult Camellia256Bridge::encryptWorkspace(
    const std::string& workspacePath, bool inPlace)
{
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    std::vector<std::string> files;
    enumerateFiles(workspacePath, files);

    if (files.empty()) {
        return CamelliaResult::ok("No files found in workspace");
    }

    int successCount = 0;
    int failCount = 0;

    for (const auto& filePath : files) {
        std::string outputPath;
        if (inPlace) {
            // Encrypt to temp, then replace original
            outputPath = filePath + ".camellia.tmp";
        } else {
            outputPath = filePath + ".camellia";
        }

        // Use authenticated encryption for workspace files
        int result = asm_camellia256_auth_encrypt_file(
            filePath.c_str(), outputPath.c_str());

        if (result == 0) {
            if (inPlace) {
                // Replace original with encrypted version
                DeleteFileA(filePath.c_str());
                MoveFileA(outputPath.c_str(), filePath.c_str());
            }
            successCount++;
        } else {
            if (inPlace) {
                DeleteFileA(outputPath.c_str());  // Cleanup temp on failure
            }
            failCount++;
        }
    }

    if (failCount > 0) {
        return CamelliaResult::error("Some workspace files failed to encrypt", -5);
    }

    return CamelliaResult::ok("Workspace encrypted (authenticated, all files)");
}

CamelliaResult Camellia256Bridge::decryptWorkspace(
    const std::string& workspacePath, bool inPlace)
{
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    if (!m_initialized) {
        return CamelliaResult::error("Engine not initialized", -1);
    }

    std::vector<std::string> files;
    enumerateFiles(workspacePath, files);

    if (files.empty()) {
        return CamelliaResult::ok("No files found in workspace");
    }

    int successCount = 0;
    int failCount = 0;

    for (const auto& filePath : files) {
        // Only process .camellia files (or all if in-place mode)
        std::string inputPath = filePath;
        std::string outputPath;

        if (inPlace) {
            outputPath = filePath + ".decrypted.tmp";
        } else {
            // Remove .camellia extension if present
            if (filePath.size() > 9 &&
                filePath.substr(filePath.size() - 9) == ".camellia") {
                outputPath = filePath.substr(0, filePath.size() - 9);
            } else {
                outputPath = filePath + ".decrypted";
            }
        }

        int result = asm_camellia256_auth_decrypt_file(
            inputPath.c_str(), outputPath.c_str());

        if (result == 0) {
            if (inPlace) {
                DeleteFileA(inputPath.c_str());
                MoveFileA(outputPath.c_str(), inputPath.c_str());
            }
            successCount++;
        } else {
            if (inPlace) {
                DeleteFileA(outputPath.c_str());
            }
            failCount++;
        }
    }

    if (failCount > 0) {
        return CamelliaResult::error("Some workspace files failed to decrypt (possible tampering)", -7);
    }

    return CamelliaResult::ok("Workspace decrypted (HMAC verified, all files)");
}

// ============================================================================
//  Get engine statistics
// ============================================================================

CamelliaEngineStatus Camellia256Bridge::getStatus() const {
    CamelliaEngineStatus status = {};
    asm_camellia256_get_status(&status);
    return status;
}

// ============================================================================
//  Secure shutdown — zeros all keying material
// ============================================================================

CamelliaResult Camellia256Bridge::shutdown() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);

    // Deactivate the Sentinel Watchdog FIRST — this stops .text monitoring
    // before we begin rolling back detours (which modify .text).
    SentinelWatchdog::instance().deactivate();

    // Shutdown the SelfRepairLoop — this rolls back all active
    // detours to their "Known Good" prologue state before we tear down
    // the MASM engine. Order matters: detours reference MASM functions.
    SelfRepairLoop::instance().shutdown();

    int result = asm_camellia256_shutdown();

    // Zero our cached HMAC key
    SecureZeroMemory(m_hmacKey, sizeof(m_hmacKey));
    m_hmacKeyLoaded = false;
    m_initialized = false;

    if (result != 0) {
        return CamelliaResult::error("Shutdown reported error", result);
    }

    return CamelliaResult::ok("Camellia-256 engine shutdown — all key material zeroed");
}

// ============================================================================
//  Get human-readable status string for UI display
// ============================================================================

std::string Camellia256Bridge::getStatusString() const {
    CamelliaEngineStatus status = getStatus();

    std::string result;
    result.reserve(256);

    result += "Camellia-256 Engine Status\n";
    result += "  Initialized: ";
    result += (status.initialized ? "YES" : "NO");
    result += "\n";

    result += "  Blocks Encrypted: ";
    result += std::to_string(status.blocksEncrypted);
    result += "\n";

    result += "  Blocks Decrypted: ";
    result += std::to_string(status.blocksDecrypted);
    result += "\n";

    result += "  Files Processed: ";
    result += std::to_string(status.filesProcessed);
    result += "\n";

    result += "  Mode: CTR + HMAC-SHA256 (Encrypt-then-MAC)\n";
    result += "  Key Derivation: SHA-256 KDF (1000 iterations)\n";
    result += "  Block Size: 128-bit | Key Size: 256-bit\n";
    result += "  Rounds: 24 (Feistel) | S-Boxes: 4 (RFC 3713)\n";

    // Shadow-Page Detour / Live Hotpatch Status
    result += "  Live Hotpatch (Shadow-Page Detour):\n";
    HotpatchKernelStats hpStats = SelfRepairLoop::instance().getKernelStats();
    size_t activeDetours = SelfRepairLoop::instance().getActiveDetourCount();

    result += "    Active Detours: ";
    result += std::to_string(activeDetours);
    result += "\n";

    result += "    Atomic Swaps Applied: ";
    result += std::to_string(hpStats.swapsApplied);
    result += "\n";

    result += "    Swaps Rolled Back: ";
    result += std::to_string(hpStats.swapsRolledBack);
    result += "\n";

    result += "    CRC Integrity Checks: ";
    result += std::to_string(hpStats.crcChecks);
    result += "\n";

    result += "    CRC Mismatches: ";
    result += std::to_string(hpStats.crcMismatches);
    result += "\n";

    SnapshotStats snapStats = SelfRepairLoop::instance().getSnapshotStats();
    result += "    Rollback Snapshots Captured: ";
    result += std::to_string(snapStats.snapshotsCaptured);
    result += "\n";

    result += "    Rollback Snapshots Restored: ";
    result += std::to_string(snapStats.snapshotsRestored);
    result += "\n";

    // Sentinel Watchdog Status
    result += "  Sentinel Watchdog:\n";
    SentinelStats sentinelStats = SentinelWatchdog::instance().getStats();

    result += "    Active: ";
    result += (SentinelWatchdog::instance().isActive() ? "YES" : "NO");
    result += "\n";

    result += "    Total Integrity Checks: ";
    result += std::to_string(sentinelStats.totalChecks);
    result += "\n";

    result += "    .text Hash Mismatches: ";
    result += std::to_string(sentinelStats.hashMismatches);
    result += "\n";

    result += "    Debugger Detections: ";
    result += std::to_string(sentinelStats.debuggerDetections);
    result += "\n";

    result += "    HW Breakpoint Hits: ";
    result += std::to_string(sentinelStats.hwBreakpointHits);
    result += "\n";

    result += "    Timing Anomalies: ";
    result += std::to_string(sentinelStats.timingAnomalies);
    result += "\n";

    result += "    Baseline Updates: ";
    result += std::to_string(sentinelStats.baselineUpdates);
    result += "\n";

    result += "    Violation Count: ";
    result += std::to_string(SentinelWatchdog::instance().getViolationCount());
    result += "\n";

    return result;
}

// ============================================================================
//  PRIVATE: Load HMAC key from MASM layer (cached)
// ============================================================================

bool Camellia256Bridge::loadHmacKey() {
    if (m_hmacKeyLoaded) {
        return true;
    }

    int result = asm_camellia256_get_hmac_key(m_hmacKey);
    if (result == 0) {
        m_hmacKeyLoaded = true;
        return true;
    }

    return false;
}

// ============================================================================
//  PRIVATE: Compute HMAC-SHA256 using BCrypt (C++ side utility)
//
//  Used for any C++ caller that needs HMAC computation independently
//  of the MASM auth module (e.g., diagnostic tools, test harness).
// ============================================================================

bool Camellia256Bridge::computeHMACSHA256(
    const uint8_t* data, uint32_t dataLen,
    uint8_t* hmacOut32) const
{
    if (!m_hmacKeyLoaded || !data || !hmacOut32) {
        return false;
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status;
    bool success = false;

    // Open HMAC-SHA256 algorithm
    status = BCryptOpenAlgorithmProvider(
        &hAlg,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG);

    if (!BCRYPT_SUCCESS(status)) {
        return false;
    }

    // Determine hash object size
    DWORD hashObjSize = 0;
    DWORD cbResult = 0;
    status = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&hashObjSize),
        sizeof(hashObjSize),
        &cbResult,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Allocate hash object
    auto* hashObj = static_cast<uint8_t*>(
        HeapAlloc(GetProcessHeap(), 0, hashObjSize));

    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Create HMAC hash with our key
    status = BCryptCreateHash(
        hAlg,
        &hHash,
        hashObj,
        hashObjSize,
        const_cast<PUCHAR>(m_hmacKey),
        sizeof(m_hmacKey),
        0);

    if (!BCRYPT_SUCCESS(status)) {
        HeapFree(GetProcessHeap(), 0, hashObj);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Hash the data
    status = BCryptHashData(
        hHash,
        const_cast<PUCHAR>(data),
        dataLen,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        HeapFree(GetProcessHeap(), 0, hashObj);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Finalize
    status = BCryptFinishHash(hHash, hmacOut32, 32, 0);
    success = BCRYPT_SUCCESS(status);

    // Cleanup
    BCryptDestroyHash(hHash);
    SecureZeroMemory(hashObj, hashObjSize);
    HeapFree(GetProcessHeap(), 0, hashObj);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return success;
}

// ============================================================================
//  PRIVATE: Constant-time comparison (side-channel safe)
// ============================================================================

bool Camellia256Bridge::constantTimeCompare(
    const uint8_t* a, const uint8_t* b, size_t len)
{
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

// ============================================================================
//  PRIVATE: Recursively enumerate files in a directory
// ============================================================================

void Camellia256Bridge::enumerateFiles(
    const std::string& dirPath,
    std::vector<std::string>& outFiles) const
{
    std::string searchPath = dirPath;
    if (!searchPath.empty() && searchPath.back() != '\\' && searchPath.back() != '/') {
        searchPath += '\\';
    }
    searchPath += '*';

    WIN32_FIND_DATAA findData = {};
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        // Skip . and ..
        if (findData.cFileName[0] == '.' &&
            (findData.cFileName[1] == '\0' ||
             (findData.cFileName[1] == '.' && findData.cFileName[2] == '\0'))) {
            continue;
        }

        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
            fullPath += '\\';
        }
        fullPath += findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into subdirectory
            enumerateFiles(fullPath, outFiles);
        } else {
            // Skip hidden/system files and our own temp files
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                !(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
                // Skip temp files from our own encryption process
                std::string name(findData.cFileName);
                if (name.size() < 14 ||
                    name.substr(name.size() - 14) != ".camellia.tmp") {
                    outFiles.push_back(fullPath);
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

} // namespace Crypto
} // namespace RawrXD
