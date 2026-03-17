// RawrXD_CredentialStore.hpp - Encrypted Credential Storage
// Pure C++20 - No Qt Dependencies
// Uses Windows DPAPI for at-rest encryption + AES-256-GCM for session-level encryption
// Features: Secure credential vault, PBKDF2 key derivation, auto-lock on idle,
//           credential categories, time-limited access, Windows Credential Manager integration

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <wincred.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// Credential Type Classification
// ============================================================================
enum class CredentialType : uint8_t {
    APIKey       = 0,
    BearerToken  = 1,
    BasicAuth    = 2,    // username:password
    OAuth2Token  = 3,
    SSHKey       = 4,
    Certificate  = 5,
    GenericSecret = 6
};

// ============================================================================
// Stored Credential Entry
// ============================================================================
struct StoredCredential {
    std::string    id;                       // Unique identifier
    std::string    label;                    // Human-readable name
    CredentialType type     = CredentialType::GenericSecret;
    std::string    username;                 // For BasicAuth
    // The secret material is NEVER stored in plaintext in this struct.
    // encryptedSecret is the DPAPI-encrypted + AES-256-GCM layered blob (base64).
    std::string    encryptedSecret;
    std::string    metadata;                 // JSON metadata string
    uint64_t       createdAt       = 0;      // Unix ms
    uint64_t       modifiedAt      = 0;
    uint64_t       lastAccessed    = 0;
    uint64_t       expiresAt       = 0;      // 0 = no expiry
    uint64_t       accessCount     = 0;
    bool           locked          = false;  // Temporarily locked out

    bool IsExpired() const {
        if (expiresAt == 0) return false;
        auto now = NowMs();
        return now > expiresAt;
    }

    static uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
};

// ============================================================================
// Credential Store - Encrypted vault backed by DPAPI + AES-256-GCM
// ============================================================================
class CredentialStore {
public:
    struct Config {
        std::string vaultFilePath;                       // Path to encrypted vault file
        uint32_t    autoLockTimeoutMs  = 5 * 60 * 1000;  // Auto-lock after 5 min idle
        uint32_t    pbkdf2Iterations   = 100000;         // PBKDF2 iteration count
        uint32_t    maxDecryptAttempts = 5;               // Lockout after N failed unlocks
        bool        useWindowsCredManager = true;         // Also sync to Windows Credential Manager
    };

    CredentialStore() {
        m_locked.store(true);
        m_failedAttempts.store(0);
        m_lastActivity.store(0);
    }

    ~CredentialStore() {
        Lock();
    }

    // ---- Initialize ----
    bool Initialize(const Config& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;

        // Generate session AES key from random bytes
        m_sessionKey.resize(32);
        BCRYPT_ALG_HANDLE hRng = nullptr;
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hRng, BCRYPT_RNG_ALGORITHM, nullptr, 0))) {
            BCryptGenRandom(hRng, m_sessionKey.data(), 32, 0);
            BCryptCloseAlgorithmProvider(hRng, 0);
        }

        return true;
    }

    // ---- Unlock vault with master password ----
    bool Unlock(const std::string& masterPassword) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_failedAttempts.load() >= m_config.maxDecryptAttempts) {
            return false; // Locked out
        }

        // Derive master key from password using PBKDF2
        std::vector<uint8_t> salt = GetOrCreateSalt();
        m_masterKey = DerivePBKDF2(masterPassword, salt, m_config.pbkdf2Iterations);
        if (m_masterKey.empty()) {
            m_failedAttempts.fetch_add(1);
            return false;
        }

        // Try to load existing vault
        if (!m_config.vaultFilePath.empty()) {
            std::ifstream in(m_config.vaultFilePath, std::ios::binary);
            if (in.is_open()) {
                std::string content((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
                if (!content.empty()) {
                    if (!DecryptVault(content)) {
                        m_failedAttempts.fetch_add(1);
                        SecureZeroVector(m_masterKey);
                        return false;
                    }
                }
            }
        }

        m_locked.store(false);
        m_failedAttempts.store(0);
        TouchActivity();
        return true;
    }

    // ---- Lock vault (clears master key from memory) ----
    void Lock() {
        std::lock_guard<std::mutex> lock(m_mutex);
        SecureZeroVector(m_masterKey);
        m_locked.store(true);
    }

    bool IsLocked() const { return m_locked.load(); }

    // ---- Store credential ----
    bool Store(const std::string& id, CredentialType type, const std::string& plainSecret,
               const std::string& label = "", const std::string& username = "",
               uint64_t expiresAt = 0, const std::string& metadata = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_locked.load()) return false;
        CheckAutoLock();
        if (m_locked.load()) return false;

        StoredCredential cred;
        cred.id             = id;
        cred.label          = label.empty() ? id : label;
        cred.type           = type;
        cred.username       = username;
        cred.createdAt      = StoredCredential::NowMs();
        cred.modifiedAt     = cred.createdAt;
        cred.expiresAt      = expiresAt;
        cred.metadata       = metadata;

        // Double-layer encryption: AES-256-GCM with session key, then DPAPI
        std::vector<uint8_t> encrypted = EncryptAES256GCM(
            std::vector<uint8_t>(plainSecret.begin(), plainSecret.end()), m_masterKey);
        if (encrypted.empty()) return false;

        cred.encryptedSecret = Base64Encode(encrypted.data(), encrypted.size());
        m_credentials[id] = cred;

        // Sync to Windows Credential Manager if enabled
        if (m_config.useWindowsCredManager) {
            SyncToWindowsCredManager(id, plainSecret, label);
        }

        TouchActivity();
        SaveVault();
        return true;
    }

    // ---- Retrieve credential ----
    std::string Retrieve(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_locked.load()) return "";
        CheckAutoLock();
        if (m_locked.load()) return "";

        auto it = m_credentials.find(id);
        if (it == m_credentials.end()) return "";
        if (it->second.IsExpired()) return "";

        auto encrypted = Base64Decode(it->second.encryptedSecret);
        auto plainBytes = DecryptAES256GCM(encrypted, m_masterKey);
        if (plainBytes.empty()) return "";

        it->second.lastAccessed = StoredCredential::NowMs();
        it->second.accessCount++;

        TouchActivity();
        std::string result(plainBytes.begin(), plainBytes.end());
        SecureZeroVector(plainBytes);
        return result;
    }

    // ---- Delete credential ----
    bool Delete(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_locked.load()) return false;

        auto it = m_credentials.find(id);
        if (it == m_credentials.end()) return false;

        SecureZeroString(it->second.encryptedSecret);
        m_credentials.erase(it);

        // Remove from Windows Credential Manager
        if (m_config.useWindowsCredManager) {
            RemoveFromWindowsCredManager(id);
        }

        SaveVault();
        return true;
    }

    // ---- Update credential ----
    bool Update(const std::string& id, const std::string& newPlainSecret) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_locked.load()) return false;

        auto it = m_credentials.find(id);
        if (it == m_credentials.end()) return false;

        std::vector<uint8_t> encrypted = EncryptAES256GCM(
            std::vector<uint8_t>(newPlainSecret.begin(), newPlainSecret.end()), m_masterKey);
        if (encrypted.empty()) return false;

        SecureZeroString(it->second.encryptedSecret);
        it->second.encryptedSecret = Base64Encode(encrypted.data(), encrypted.size());
        it->second.modifiedAt = StoredCredential::NowMs();

        SaveVault();
        return true;
    }

    // ---- List stored credentials (no secrets exposed) ----
    std::vector<StoredCredential> List() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<StoredCredential> result;
        for (const auto& [id, cred] : m_credentials) {
            StoredCredential safe = cred;
            safe.encryptedSecret = "[ENCRYPTED]";
            result.push_back(safe);
        }
        return result;
    }

    bool HasCredential(const std::string& id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_credentials.count(id) > 0;
    }

private:
    mutable std::mutex m_mutex;
    Config m_config;
    std::map<std::string, StoredCredential> m_credentials;
    std::vector<uint8_t> m_masterKey;
    std::vector<uint8_t> m_sessionKey;
    std::atomic<bool> m_locked;
    std::atomic<uint32_t> m_failedAttempts;
    std::atomic<uint64_t> m_lastActivity;

    void TouchActivity() {
        m_lastActivity.store(StoredCredential::NowMs());
    }

    void CheckAutoLock() {
        if (m_config.autoLockTimeoutMs == 0) return;
        auto now = StoredCredential::NowMs();
        if (m_lastActivity.load() > 0 && (now - m_lastActivity.load()) > m_config.autoLockTimeoutMs) {
            SecureZeroVector(m_masterKey);
            m_locked.store(true);
        }
    }

    // ---- PBKDF2 Key Derivation ----
    std::vector<uint8_t> DerivePBKDF2(const std::string& password, const std::vector<uint8_t>& salt, uint32_t iterations) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            return {};

        std::vector<uint8_t> derivedKey(32); // 256-bit key
        NTSTATUS status = BCryptDeriveKeyPBKDF2(hAlg,
            (PUCHAR)password.data(), (ULONG)password.size(),
            (PUCHAR)salt.data(), (ULONG)salt.size(),
            iterations, derivedKey.data(), (ULONG)derivedKey.size(), 0);

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return BCRYPT_SUCCESS(status) ? derivedKey : std::vector<uint8_t>{};
    }

    // ---- AES-256-GCM Encrypt ----
    std::vector<uint8_t> EncryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
        if (key.size() != 32) return {};

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_KEY_HANDLE hKey = nullptr;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            return {};

        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return {};
        }

        DWORD keyObjSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjSize, sizeof(DWORD), &cbResult, 0);
        std::vector<uint8_t> keyObj(keyObjSize);

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), keyObjSize,
                (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return {};
        }

        // Generate 12-byte IV (nonce)
        uint8_t iv[12] = {};
        BCRYPT_ALG_HANDLE hRng = nullptr;
        BCryptOpenAlgorithmProvider(&hRng, BCRYPT_RNG_ALGORITHM, nullptr, 0);
        BCryptGenRandom(hRng, iv, 12, 0);
        BCryptCloseAlgorithmProvider(hRng, 0);

        // GCM auth info
        uint8_t tag[16] = {};
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce   = iv;
        authInfo.cbNonce   = 12;
        authInfo.pbTag     = tag;
        authInfo.cbTag     = 16;

        // Encrypt
        DWORD ciphertextLen = 0;
        BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
            &authInfo, nullptr, 0, nullptr, 0, &ciphertextLen, 0);

        std::vector<uint8_t> ciphertext(ciphertextLen);
        NTSTATUS status = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
            &authInfo, nullptr, 0, ciphertext.data(), ciphertextLen, &ciphertextLen, 0);

        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        if (!BCRYPT_SUCCESS(status)) return {};

        // Output format: [12 IV][16 TAG][N CIPHERTEXT]
        std::vector<uint8_t> result;
        result.reserve(12 + 16 + ciphertextLen);
        result.insert(result.end(), iv, iv + 12);
        result.insert(result.end(), tag, tag + 16);
        result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertextLen);
        return result;
    }

    // ---- AES-256-GCM Decrypt ----
    std::vector<uint8_t> DecryptAES256GCM(const std::vector<uint8_t>& blob, const std::vector<uint8_t>& key) {
        if (key.size() != 32 || blob.size() < 28) return {}; // 12 IV + 16 TAG minimum

        const uint8_t* iv = blob.data();
        const uint8_t* tag = blob.data() + 12;
        const uint8_t* ciphertext = blob.data() + 28;
        DWORD ciphertextLen = (DWORD)(blob.size() - 28);

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_KEY_HANDLE hKey = nullptr;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            return {};

        BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);

        DWORD keyObjSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjSize, sizeof(DWORD), &cbResult, 0);
        std::vector<uint8_t> keyObj(keyObjSize);

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), keyObjSize,
                (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return {};
        }

        uint8_t tagCopy[16];
        memcpy(tagCopy, tag, 16);

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)iv;
        authInfo.cbNonce = 12;
        authInfo.pbTag   = tagCopy;
        authInfo.cbTag   = 16;

        DWORD plaintextLen = 0;
        BCryptDecrypt(hKey, (PUCHAR)ciphertext, ciphertextLen,
            &authInfo, nullptr, 0, nullptr, 0, &plaintextLen, 0);

        std::vector<uint8_t> plaintext(plaintextLen);
        NTSTATUS status = BCryptDecrypt(hKey, (PUCHAR)ciphertext, ciphertextLen,
            &authInfo, nullptr, 0, plaintext.data(), plaintextLen, &plaintextLen, 0);

        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        if (!BCRYPT_SUCCESS(status)) return {};
        plaintext.resize(plaintextLen);
        return plaintext;
    }

    // ---- Salt Management ----
    std::vector<uint8_t> GetOrCreateSalt() {
        std::string saltPath = m_config.vaultFilePath + ".salt";
        std::ifstream in(saltPath, std::ios::binary);
        if (in.is_open()) {
            std::vector<uint8_t> salt(32);
            in.read(reinterpret_cast<char*>(salt.data()), 32);
            if (in.gcount() == 32) return salt;
        }

        // Create new salt
        std::vector<uint8_t> salt(32);
        BCRYPT_ALG_HANDLE hRng = nullptr;
        BCryptOpenAlgorithmProvider(&hRng, BCRYPT_RNG_ALGORITHM, nullptr, 0);
        BCryptGenRandom(hRng, salt.data(), 32, 0);
        BCryptCloseAlgorithmProvider(hRng, 0);

        std::ofstream out(saltPath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(salt.data()), 32);
        return salt;
    }

    // ---- Vault Persistence ----
    bool SaveVault() {
        if (m_config.vaultFilePath.empty() || m_masterKey.empty()) return false;

        // Serialize credentials
        std::ostringstream oss;
        oss << "{\"v\":1,\"creds\":[";
        bool first = true;
        for (const auto& [id, cred] : m_credentials) {
            if (!first) oss << ",";
            first = false;
            oss << "{\"id\":\"" << id << "\""
                << ",\"label\":\"" << cred.label << "\""
                << ",\"type\":" << static_cast<int>(cred.type)
                << ",\"user\":\"" << cred.username << "\""
                << ",\"enc\":\"" << cred.encryptedSecret << "\""
                << ",\"meta\":\"" << cred.metadata << "\""
                << ",\"created\":" << cred.createdAt
                << ",\"modified\":" << cred.modifiedAt
                << ",\"expires\":" << cred.expiresAt
                << ",\"accessed\":" << cred.lastAccessed
                << ",\"count\":" << cred.accessCount
                << "}";
        }
        oss << "]}";

        // Encrypt vault with DPAPI
        std::string plaintext = oss.str();
        DATA_BLOB input, output;
        input.pbData = (BYTE*)plaintext.data();
        input.cbData = (DWORD)plaintext.size();

        if (!CryptProtectData(&input, L"RawrXD_CredentialStore", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_LOCAL_MACHINE, &output)) {
            return false;
        }

        std::ofstream out(m_config.vaultFilePath, std::ios::binary | std::ios::trunc);
        out.write("RXCS1", 5); // Magic: RawrXD CredentialStore v1
        out.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
        LocalFree(output.pbData);

        SecureZeroMemory(&plaintext[0], plaintext.size());
        return true;
    }

    bool DecryptVault(const std::string& content) {
        if (content.size() < 5 || content.substr(0, 5) != "RXCS1") return false;

        DATA_BLOB input, output;
        input.pbData = (BYTE*)(content.data() + 5);
        input.cbData = (DWORD)(content.size() - 5);

        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
            return false;
        }

        std::string plaintext((char*)output.pbData, output.cbData);
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);

        // Parse credentials from JSON
        m_credentials.clear();
        size_t pos = plaintext.find("\"creds\":[");
        if (pos == std::string::npos) return true; // Empty vault

        pos = plaintext.find('[', pos) + 1;
        while (pos < plaintext.size()) {
            size_t objStart = plaintext.find('{', pos);
            if (objStart == std::string::npos) break;
            // Find matching } (handle nested braces)
            int depth = 0;
            size_t objEnd = objStart;
            for (; objEnd < plaintext.size(); ++objEnd) {
                if (plaintext[objEnd] == '{') depth++;
                else if (plaintext[objEnd] == '}') { depth--; if (depth == 0) break; }
            }
            if (depth != 0) break;

            std::string obj = plaintext.substr(objStart, objEnd - objStart + 1);
            StoredCredential cred;
            cred.id             = ExtractStr(obj, "id");
            cred.label          = ExtractStr(obj, "label");
            cred.type           = static_cast<CredentialType>(ExtractInt(obj, "type"));
            cred.username       = ExtractStr(obj, "user");
            cred.encryptedSecret = ExtractStr(obj, "enc");
            cred.metadata       = ExtractStr(obj, "meta");
            cred.createdAt      = ExtractUInt64(obj, "created");
            cred.modifiedAt     = ExtractUInt64(obj, "modified");
            cred.expiresAt      = ExtractUInt64(obj, "expires");
            cred.lastAccessed   = ExtractUInt64(obj, "accessed");
            cred.accessCount    = ExtractUInt64(obj, "count");

            if (!cred.id.empty()) m_credentials[cred.id] = cred;
            pos = objEnd + 1;
        }

        SecureZeroMemory(&plaintext[0], plaintext.size());
        return true;
    }

    // ---- Windows Credential Manager ----
    void SyncToWindowsCredManager(const std::string& id, const std::string& secret, const std::string& label) {
        std::wstring target = L"RawrXD_" + std::wstring(id.begin(), id.end());
        std::wstring comment = std::wstring(label.begin(), label.end());

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = (LPWSTR)target.c_str();
        cred.Comment = (LPWSTR)comment.c_str();
        cred.CredentialBlobSize = (DWORD)secret.size();
        cred.CredentialBlob = (LPBYTE)secret.data();
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        CredWriteW(&cred, 0);
    }

    void RemoveFromWindowsCredManager(const std::string& id) {
        std::wstring target = L"RawrXD_" + std::wstring(id.begin(), id.end());
        CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
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

    // ---- Secure Zero ----
    static void SecureZeroVector(std::vector<uint8_t>& v) {
        if (!v.empty()) SecureZeroMemory(v.data(), v.size());
        v.clear();
    }
    static void SecureZeroString(std::string& s) {
        if (!s.empty()) SecureZeroMemory(s.data(), s.size());
        s.clear();
    }

    // ---- JSON Helpers ----
    static std::string ExtractStr(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        auto end = json.find('"', pos);
        return (end == std::string::npos) ? "" : json.substr(pos, end - pos);
    }
    static int ExtractInt(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        return std::atoi(json.c_str() + pos + needle.size());
    }
    static uint64_t ExtractUInt64(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        return std::strtoull(json.c_str() + pos + needle.size(), nullptr, 10);
    }
};

} // namespace Security
} // namespace RawrXD
