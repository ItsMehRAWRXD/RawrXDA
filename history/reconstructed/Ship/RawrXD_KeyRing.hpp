// RawrXD_KeyRing.hpp - API Key Ring Management
// Pure C++20 - No Qt Dependencies
// Features: Multi-provider key storage, automatic rotation tracking, usage quotas,
//           rate-limit-aware key selection, DPAPI-encrypted persistence, key health monitoring

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dpapi.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <atomic>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// API Key Metadata
// ============================================================================
enum class KeyProvider : uint8_t {
    OpenAI      = 0,
    Anthropic   = 1,
    Groq        = 2,
    Local       = 3,
    Azure       = 4,
    Custom      = 5
};

enum class KeyStatus : uint8_t {
    Active      = 0,
    RateLimited = 1,
    Expired     = 2,
    Revoked     = 3,
    Suspended   = 4,
    Rotating    = 5
};

struct APIKeyEntry {
    std::string id;                  // Unique ID (e.g., "openai_prod_1")
    KeyProvider provider;
    KeyStatus   status        = KeyStatus::Active;
    std::string encryptedKey;        // DPAPI-encrypted key material (base64)
    std::string label;               // Human-readable label
    std::string environment;         // "production", "staging", "development"
    uint64_t    createdAt     = 0;   // Unix epoch ms
    uint64_t    expiresAt     = 0;   // 0 = no expiry
    uint64_t    lastUsed      = 0;
    uint64_t    useCount      = 0;
    uint64_t    maxUses       = 0;   // 0 = unlimited
    uint64_t    rateLimitResetAt = 0;// When rate limit clears (unix ms)
    double      dailyBudget   = 0.0; // Dollar budget per day (0 = unlimited)
    double      dailySpent    = 0.0;
    int         priority      = 0;   // Higher = preferred for selection
    std::string sha256Hash;          // SHA-256 of plaintext key (for identity without decryption)

    bool IsUsable() const {
        if (status != KeyStatus::Active) return false;
        auto now = NowMs();
        if (expiresAt > 0 && now > expiresAt) return false;
        if (maxUses > 0 && useCount >= maxUses) return false;
        if (dailyBudget > 0 && dailySpent >= dailyBudget) return false;
        if (rateLimitResetAt > 0 && now < rateLimitResetAt) return false;
        return true;
    }

    static uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
};

// ============================================================================
// Key Ring - Manages a collection of API keys with selection logic
// ============================================================================
class KeyRing {
public:
    KeyRing() = default;

    // ---- Add / Remove ----
    bool AddKey(const std::string& id, KeyProvider provider, const std::string& plaintextKey,
                const std::string& label = "", const std::string& env = "production",
                int priority = 0, uint64_t expiresAt = 0, uint64_t maxUses = 0, double dailyBudget = 0.0) {
        std::lock_guard<std::mutex> lock(m_mutex);

        APIKeyEntry entry;
        entry.id          = id;
        entry.provider    = provider;
        entry.status      = KeyStatus::Active;
        entry.label       = label;
        entry.environment = env;
        entry.priority    = priority;
        entry.createdAt   = APIKeyEntry::NowMs();
        entry.expiresAt   = expiresAt;
        entry.maxUses     = maxUses;
        entry.dailyBudget = dailyBudget;
        entry.sha256Hash  = ComputeSHA256(plaintextKey);

        // Encrypt key with DPAPI
        entry.encryptedKey = EncryptWithDPAPI(plaintextKey);
        if (entry.encryptedKey.empty()) return false;

        m_keys[id] = entry;
        return true;
    }

    bool RemoveKey(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Securely zero the encrypted key
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return false;
        SecureZeroString(it->second.encryptedKey);
        m_keys.erase(it);
        return true;
    }

    // ---- Retrieve Decrypted Key ----
    std::string GetDecryptedKey(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return "";
        return DecryptWithDPAPI(it->second.encryptedKey);
    }

    // ---- Select Best Key for Provider ----
    // Returns key ID of the best usable key for the given provider, considering priority,
    // rate limits, usage quotas, and budget. Returns empty if none available.
    std::string SelectKey(KeyProvider provider, const std::string& env = "") {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<std::pair<std::string, int>> candidates;
        for (auto& [id, key] : m_keys) {
            if (key.provider != provider) continue;
            if (!env.empty() && key.environment != env) continue;
            if (!key.IsUsable()) continue;
            candidates.push_back({id, key.priority});
        }

        if (candidates.empty()) return "";

        // Sort by priority (descending), then by least usage
        std::sort(candidates.begin(), candidates.end(),
            [this](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return m_keys[a.first].useCount < m_keys[b.first].useCount;
            });

        return candidates[0].first;
    }

    // ---- Select and Decrypt (convenience) ----
    std::string SelectAndDecryptKey(KeyProvider provider, const std::string& env = "") {
        std::string id = SelectKey(provider, env);
        if (id.empty()) return "";
        return GetDecryptedKey(id);
    }

    // ---- Record Usage ----
    void RecordUsage(const std::string& id, double cost = 0.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return;
        it->second.useCount++;
        it->second.lastUsed = APIKeyEntry::NowMs();
        it->second.dailySpent += cost;
    }

    void MarkRateLimited(const std::string& id, uint64_t resetAtMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return;
        it->second.status = KeyStatus::RateLimited;
        it->second.rateLimitResetAt = resetAtMs;
    }

    void MarkActive(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return;
        it->second.status = KeyStatus::Active;
        it->second.rateLimitResetAt = 0;
    }

    void RevokeKey(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(id);
        if (it == m_keys.end()) return;
        it->second.status = KeyStatus::Revoked;
    }

    // ---- Reset Daily Counters (call once per day) ----
    void ResetDailyCounters() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, key] : m_keys) {
            key.dailySpent = 0.0;
            // Re-activate rate-limited keys whose reset time has passed
            if (key.status == KeyStatus::RateLimited && key.rateLimitResetAt <= APIKeyEntry::NowMs()) {
                key.status = KeyStatus::Active;
                key.rateLimitResetAt = 0;
            }
        }
    }

    // ---- Bulk Query ----
    std::vector<APIKeyEntry> ListKeys(KeyProvider provider = KeyProvider::Custom, bool allProviders = true) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<APIKeyEntry> result;
        for (const auto& [id, key] : m_keys) {
            if (allProviders || key.provider == provider) {
                APIKeyEntry safe = key;
                safe.encryptedKey = "[REDACTED]"; // Never expose encrypted blob in listings
                result.push_back(safe);
            }
        }
        return result;
    }

    size_t GetActiveKeyCount(KeyProvider provider) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        for (const auto& [id, key] : m_keys) {
            if (key.provider == provider && key.IsUsable()) count++;
        }
        return count;
    }

    // ---- Persistence (DPAPI-wrapped file) ----
    bool SaveToFile(const std::string& filePath) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::ostringstream oss;
        oss << "{\"version\":1,\"keys\":[";
        bool first = true;
        for (const auto& [id, key] : m_keys) {
            if (!first) oss << ",";
            first = false;
            oss << "{\"id\":\"" << id << "\""
                << ",\"provider\":" << static_cast<int>(key.provider)
                << ",\"status\":" << static_cast<int>(key.status)
                << ",\"enc\":\"" << key.encryptedKey << "\""
                << ",\"label\":\"" << key.label << "\""
                << ",\"env\":\"" << key.environment << "\""
                << ",\"created\":" << key.createdAt
                << ",\"expires\":" << key.expiresAt
                << ",\"lastUsed\":" << key.lastUsed
                << ",\"useCount\":" << key.useCount
                << ",\"maxUses\":" << key.maxUses
                << ",\"priority\":" << key.priority
                << ",\"budget\":" << key.dailyBudget
                << ",\"hash\":\"" << key.sha256Hash << "\""
                << "}";
        }
        oss << "]}";

        // Encrypt entire keyring file with DPAPI
        std::string plaintext = oss.str();
        std::string encrypted = EncryptWithDPAPI(plaintext);
        if (encrypted.empty()) return false;

        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << "RXKR1"; // Magic header: RawrXD KeyRing v1
        out << encrypted;
        return true;
    }

    bool LoadFromFile(const std::string& filePath) {
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        if (content.size() < 5 || content.substr(0, 5) != "RXKR1") return false;
        std::string encrypted = content.substr(5);
        std::string plaintext = DecryptWithDPAPI(encrypted);
        if (plaintext.empty()) return false;

        // Parse the JSON (simplified — integrates with RawrXD_JSON.hpp for production)
        std::lock_guard<std::mutex> lock(m_mutex);
        m_keys.clear();

        // Minimal parse: find each key object { ... } in the "keys" array
        size_t pos = plaintext.find("\"keys\":[");
        if (pos == std::string::npos) return false;
        pos = plaintext.find('[', pos) + 1;

        while (pos < plaintext.size()) {
            size_t objStart = plaintext.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = plaintext.find('}', objStart);
            if (objEnd == std::string::npos) break;

            std::string obj = plaintext.substr(objStart, objEnd - objStart + 1);
            APIKeyEntry entry;
            entry.id           = ExtractStr(obj, "id");
            entry.provider     = static_cast<KeyProvider>(ExtractInt(obj, "provider"));
            entry.status       = static_cast<KeyStatus>(ExtractInt(obj, "status"));
            entry.encryptedKey = ExtractStr(obj, "enc");
            entry.label        = ExtractStr(obj, "label");
            entry.environment  = ExtractStr(obj, "env");
            entry.createdAt    = ExtractUInt64(obj, "created");
            entry.expiresAt    = ExtractUInt64(obj, "expires");
            entry.lastUsed     = ExtractUInt64(obj, "lastUsed");
            entry.useCount     = ExtractUInt64(obj, "useCount");
            entry.maxUses      = ExtractUInt64(obj, "maxUses");
            entry.priority     = ExtractInt(obj, "priority");
            entry.dailyBudget  = ExtractDouble(obj, "budget");
            entry.sha256Hash   = ExtractStr(obj, "hash");

            if (!entry.id.empty()) {
                m_keys[entry.id] = entry;
            }

            pos = objEnd + 1;
        }

        return true;
    }

private:
    mutable std::mutex m_mutex;
    std::map<std::string, APIKeyEntry> m_keys;

    // ---- DPAPI Encrypt/Decrypt ----
    static std::string EncryptWithDPAPI(const std::string& plaintext) {
        DATA_BLOB input, output;
        input.pbData = (BYTE*)plaintext.data();
        input.cbData = (DWORD)plaintext.size();

        if (!CryptProtectData(&input, L"RawrXD_KeyRing", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_LOCAL_MACHINE, &output)) {
            return "";
        }

        std::string result = Base64Encode(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }

    static std::string DecryptWithDPAPI(const std::string& base64Encrypted) {
        auto encrypted = Base64Decode(base64Encrypted);
        if (encrypted.empty()) return "";

        DATA_BLOB input, output;
        input.pbData = encrypted.data();
        input.cbData = (DWORD)encrypted.size();

        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
            return "";
        }

        std::string result((char*)output.pbData, output.cbData);
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }

    // ---- SHA-256 Hash ----
    static std::string ComputeSHA256(const std::string& data) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            return "";

        DWORD hashObjSize = 0, hashSize = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize, sizeof(DWORD), &cbResult, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(DWORD), &cbResult, 0);

        std::vector<UCHAR> hashObj(hashObjSize), hashVal(hashSize);
        std::string result;

        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize, nullptr, 0, 0))) {
            BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0);
            BCryptFinishHash(hHash, hashVal.data(), hashSize, 0);
            BCryptDestroyHash(hHash);

            static const char hex[] = "0123456789abcdef";
            result.reserve(hashSize * 2);
            for (DWORD i = 0; i < hashSize; ++i) {
                result += hex[hashVal[i] >> 4];
                result += hex[hashVal[i] & 0x0F];
            }
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return result;
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
    static void SecureZeroString(std::string& s) {
        if (!s.empty()) {
            SecureZeroMemory(s.data(), s.size());
            s.clear();
        }
    }

    // ---- Minimal JSON Extraction ----
    static std::string ExtractStr(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        auto end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }

    static int ExtractInt(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        pos += needle.size();
        return std::atoi(json.c_str() + pos);
    }

    static uint64_t ExtractUInt64(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        pos += needle.size();
        return std::strtoull(json.c_str() + pos, nullptr, 10);
    }

    static double ExtractDouble(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\":";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0.0;
        pos += needle.size();
        return std::strtod(json.c_str() + pos, nullptr);
    }
};

} // namespace Security
} // namespace RawrXD
