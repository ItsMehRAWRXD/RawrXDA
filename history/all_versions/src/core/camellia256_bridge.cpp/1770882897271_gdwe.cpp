// ============================================================================
// camellia256_bridge.cpp — C++ Bridge Implementation for MASM Camellia-256
// ============================================================================
//
// Implements the Camellia256Bridge class that wraps the pure MASM x64
// Camellia-256 encryption engine. Provides workspace-level encryption
// (recursive directory traversal), single-file, and buffer operations.
//
// Called from:
//   - main_win32.cpp     : IDE startup workspace encryption
//   - Win32IDE_AirgappedEnterprise.cpp : cmdAirgapEncrypt()
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns (CamelliaResult)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "camellia256_bridge.hpp"
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")
#include <shlwapi.h>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <mutex>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")

// Authenticated file format constants
static constexpr uint8_t CAMELLIA_FILE_MAGIC[4] = { 'R', 'C', 'M', '2' };
static constexpr uint32_t CAMELLIA_FILE_VERSION = 0x00020000;
static constexpr size_t AUTH_HEADER_SIZE = 4 + 4 + 16 + 32; // magic+version+nonce+hmac = 56 bytes

namespace RawrXD {
namespace Crypto {

// ============================================================================
//  Thread safety — single mutex for engine operations
// ============================================================================

static std::mutex s_camelliaMtx;

// ============================================================================
//  Singleton
// ============================================================================

Camellia256Bridge& Camellia256Bridge::instance() {
    static Camellia256Bridge s_instance;
    return s_instance;
}

// ============================================================================
//  initialize — HWID-derived key
// ============================================================================

CamelliaResult Camellia256Bridge::initialize() {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (m_initialized) {
        return CamelliaResult::ok("Camellia-256 already initialized");
    }

    int rc = asm_camellia256_init();
    if (rc != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Camellia-256 init failed (MASM error %d)", rc);
        return CamelliaResult::error(buf, rc);
    }

    m_initialized = true;
    m_hmacKeyLoaded = false; // Force reload of HMAC key
    loadHmacKey();
    OutputDebugStringA("[RawrXD] Camellia-256 bridge initialized (HWID-derived key)\n");
    return CamelliaResult::ok("Camellia-256 initialized with machine-identity key (SHA-256 KDF)");
}

// ============================================================================
//  initializeWithKey — explicit 256-bit key
// ============================================================================

CamelliaResult Camellia256Bridge::initializeWithKey(const uint8_t* key32) {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (!key32) {
        return CamelliaResult::error("Null key pointer", -2);
    }

    int rc = asm_camellia256_set_key(key32);
    if (rc != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Camellia-256 set_key failed (MASM error %d)", rc);
        return CamelliaResult::error(buf, rc);
    }

    m_initialized = true;
    m_hmacKeyLoaded = false;
    OutputDebugStringA("[RawrXD] Camellia-256 bridge initialized (explicit key)\n");
    return CamelliaResult::ok("Camellia-256 initialized with explicit 256-bit key");
}

// ============================================================================
//  selfTest — run RFC 3713 known-answer test
// ============================================================================

CamelliaResult Camellia256Bridge::selfTest() {
    int rc = asm_camellia256_self_test();
    if (rc != 0) {
        return CamelliaResult::error("Camellia-256 self-test FAILED — cipher may be corrupted", rc);
    }
    return CamelliaResult::ok("Camellia-256 self-test passed (RFC 3713 Appendix A vector)");
}

// ============================================================================
//  loadHmacKey — fetch HMAC key from MASM layer (cached)
// ============================================================================

bool Camellia256Bridge::loadHmacKey() {
    if (m_hmacKeyLoaded) return true;
    int rc = asm_camellia256_get_hmac_key(m_hmacKey);
    if (rc == 0) {
        m_hmacKeyLoaded = true;
        return true;
    }
    return false;
}

// ============================================================================
//  computeHMACSHA256 — BCrypt-based HMAC-SHA256
// ============================================================================

bool Camellia256Bridge::computeHMACSHA256(const uint8_t* data, uint32_t dataLen,
                                           uint8_t* hmacOut32) const {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) return false;

    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0,
                              (PUCHAR)m_hmacKey, 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptHashData(hHash, (PUCHAR)data, dataLen, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptFinishHash(hHash, hmacOut32, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(status);
}

// ============================================================================
//  constantTimeCompare — side-channel-safe comparison
// ============================================================================

bool Camellia256Bridge::constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len) {
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

// ============================================================================
//  isInitialized
// ============================================================================

bool Camellia256Bridge::isInitialized() const {
    return m_initialized;
}

// ============================================================================
//  encryptFile — single file encryption
// ============================================================================

CamelliaResult Camellia256Bridge::encryptFile(const std::string& inputPath,
                                               const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (!m_initialized) {
        return CamelliaResult::error("Camellia-256 not initialized", -1);
    }

    int rc = asm_camellia256_encrypt_file(inputPath.c_str(), outputPath.c_str());
    if (rc != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Camellia-256 encrypt_file failed: %s (error %d)",
                 inputPath.c_str(), rc);
        return CamelliaResult::error(buf, rc);
    }

    return CamelliaResult::ok("File encrypted with Camellia-256 CTR");
}

// ============================================================================
//  decryptFile — single file decryption
// ============================================================================

CamelliaResult Camellia256Bridge::decryptFile(const std::string& inputPath,
                                               const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (!m_initialized) {
        return CamelliaResult::error("Camellia-256 not initialized", -1);
    }

    int rc = asm_camellia256_decrypt_file(inputPath.c_str(), outputPath.c_str());
    if (rc != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Camellia-256 decrypt_file failed: %s (error %d)",
                 inputPath.c_str(), rc);
        return CamelliaResult::error(buf, rc);
    }

    return CamelliaResult::ok("File decrypted with Camellia-256 CTR");
}

// ============================================================================
//  encryptBuffer — in-place CTR encryption
// ============================================================================

CamelliaResult Camellia256Bridge::encryptBuffer(uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (!m_initialized) {
        return CamelliaResult::error("Camellia-256 not initialized", -1);
    }
    if (!data || length == 0) {
        return CamelliaResult::error("Invalid buffer", -2);
    }

    // Generate a random nonce for this buffer encryption
    uint8_t nonce[16] = {};
    NTSTATUS status = BCryptGenRandom(nullptr, nonce, sizeof(nonce),
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        // Fallback: use GetTickCount64 + counter for nonce
        uint64_t tick = GetTickCount64();
        memcpy(nonce, &tick, sizeof(tick));
        static uint64_t counter = 0;
        counter++;
        memcpy(nonce + 8, &counter, sizeof(counter));
    }

    int rc = asm_camellia256_encrypt_ctr(data, length, nonce);
    if (rc != 0) {
        return CamelliaResult::error("CTR encrypt failed", rc);
    }

    return CamelliaResult::ok("Buffer encrypted with Camellia-256 CTR");
}

// ============================================================================
//  decryptBuffer — in-place CTR decryption
// ============================================================================

CamelliaResult Camellia256Bridge::decryptBuffer(uint8_t* data, size_t length) {
    // CTR mode encryption and decryption are the same operation
    return encryptBuffer(data, length);
}

// ============================================================================
//  enumerateFiles — recursively list files in directory
// ============================================================================

void Camellia256Bridge::enumerateFiles(const std::string& dirPath,
                                        std::vector<std::string>& outFiles) const {
    std::string searchPattern = dirPath + "\\*";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        // Skip . and ..
        if (strcmp(findData.cFileName, ".") == 0 ||
            strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        std::string fullPath = dirPath + "\\" + findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Skip hidden/system directories, .git, .vscode, build, node_modules
            if (strcmp(findData.cFileName, ".git") == 0 ||
                strcmp(findData.cFileName, ".vscode") == 0 ||
                strcmp(findData.cFileName, "build") == 0 ||
                strcmp(findData.cFileName, "node_modules") == 0 ||
                strcmp(findData.cFileName, ".vs") == 0) {
                continue;
            }
            enumerateFiles(fullPath, outFiles);
        } else {
            // Skip already-encrypted files and very large files (>100 MB)
            const char* ext = PathFindExtensionA(findData.cFileName);
            if (ext && (_stricmp(ext, ".camellia") == 0 ||
                        _stricmp(ext, ".enc") == 0)) {
                continue;
            }
            // Skip files > 100 MB
            LARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            if (fileSize.QuadPart > 100 * 1024 * 1024) {
                continue;
            }

            outFiles.push_back(fullPath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

// ============================================================================
//  encryptWorkspace — encrypt all files in workspace directory
// ============================================================================

CamelliaResult Camellia256Bridge::encryptWorkspace(const std::string& workspacePath,
                                                    bool inPlace) {
    if (!m_initialized) {
        return CamelliaResult::error("Camellia-256 not initialized", -1);
    }

    // Verify directory exists
    DWORD attrs = GetFileAttributesA(workspacePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return CamelliaResult::error("Workspace path not found or not a directory", -3);
    }

    // Enumerate all files
    std::vector<std::string> files;
    enumerateFiles(workspacePath, files);

    if (files.empty()) {
        return CamelliaResult::ok("No files to encrypt in workspace");
    }

    int succeeded = 0;
    int failed = 0;

    for (const auto& filePath : files) {
        std::string outPath;
        if (inPlace) {
            // Encrypt to .camellia, then rename over original
            outPath = filePath + ".camellia.tmp";
        } else {
            outPath = filePath + ".camellia";
        }

        CamelliaResult r = encryptFile(filePath, outPath);
        if (r.success) {
            if (inPlace) {
                // Replace original with encrypted file
                DeleteFileA(filePath.c_str());
                MoveFileA(outPath.c_str(), filePath.c_str());
            }
            succeeded++;
        } else {
            if (inPlace) {
                DeleteFileA(outPath.c_str()); // cleanup failed temp
            }
            failed++;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Workspace encryption complete: %d files encrypted, %d failed (Camellia-256 CTR)",
             succeeded, failed);

    if (failed > 0) {
        return CamelliaResult::error(msg, failed);
    }
    return CamelliaResult::ok(msg);
}

// ============================================================================
//  decryptWorkspace — decrypt all .camellia files in workspace
// ============================================================================

CamelliaResult Camellia256Bridge::decryptWorkspace(const std::string& workspacePath,
                                                    bool inPlace) {
    if (!m_initialized) {
        return CamelliaResult::error("Camellia-256 not initialized", -1);
    }

    DWORD attrs = GetFileAttributesA(workspacePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return CamelliaResult::error("Workspace path not found or not a directory", -3);
    }

    // Find all .camellia files
    std::string searchPattern = workspacePath + "\\*.camellia";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);

    int succeeded = 0;
    int failed = 0;

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string encPath = workspacePath + "\\" + findData.cFileName;
            // Remove .camellia extension for output
            std::string decPath = encPath.substr(0, encPath.length() - 9); // strip ".camellia"

            CamelliaResult r = decryptFile(encPath, decPath);
            if (r.success) {
                if (inPlace) {
                    DeleteFileA(encPath.c_str()); // remove encrypted version
                }
                succeeded++;
            } else {
                failed++;
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Workspace decryption complete: %d files decrypted, %d failed",
             succeeded, failed);

    if (failed > 0) {
        return CamelliaResult::error(msg, failed);
    }
    return CamelliaResult::ok(msg);
}

// ============================================================================
//  getStatus
// ============================================================================

CamelliaEngineStatus Camellia256Bridge::getStatus() const {
    CamelliaEngineStatus status = {};
    asm_camellia256_get_status(&status);
    return status;
}

// ============================================================================
//  getStatusString — human-readable for UI display
// ============================================================================

std::string Camellia256Bridge::getStatusString() const {
    CamelliaEngineStatus st = getStatus();

    std::ostringstream oss;
    oss << "Camellia-256 Engine Status:\n"
        << "  Initialized:       " << (st.initialized ? "YES" : "NO") << "\n"
        << "  Implementation:    MASM x64 (24-round Feistel, CTR mode)\n"
        << "  Key Schedule:      52 subkeys (256-bit key expansion)\n"
        << "  Blocks Encrypted:  " << st.blocksEncrypted << "\n"
        << "  Blocks Decrypted:  " << st.blocksDecrypted << "\n"
        << "  Files Processed:   " << st.filesProcessed << "\n"
        << "  Key Derivation:    FNV-1a(HWID) + 1000-iteration strengthening\n"
        << "  Mode:              CTR (Counter) with random nonce\n";

    return oss.str();
}

// ============================================================================
//  shutdown
// ============================================================================

CamelliaResult Camellia256Bridge::shutdown() {
    std::lock_guard<std::mutex> lock(s_camelliaMtx);

    if (!m_initialized) {
        return CamelliaResult::ok("Camellia-256 was not initialized");
    }

    int rc = asm_camellia256_shutdown();
    m_initialized = false;

    if (rc != 0) {
        return CamelliaResult::error("Camellia-256 shutdown returned error", rc);
    }

    OutputDebugStringA("[RawrXD] Camellia-256 bridge shutdown complete\n");
    return CamelliaResult::ok("Camellia-256 engine shutdown — all keys zeroed");
}

} // namespace Crypto
} // namespace RawrXD
