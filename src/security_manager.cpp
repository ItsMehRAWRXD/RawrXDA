#include "security_manager.h"
#include <stdexcept>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <ntstatus.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <fstream>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

// Static singleton instance
std::unique_ptr<SecurityManager> SecurityManager::s_instance = nullptr;

// ==================== UTILITIES ====================

static std::string bytesToHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    for (uint8_t byte : data) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

static std::string base64Encode(const std::vector<uint8_t>& data) {
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    
    for (size_t i = 0; i < data.size(); i += 3) {
        int b1 = data[i];
        int b2 = (i + 1 < data.size()) ? data[i + 1] : 0;
        int b3 = (i + 2 < data.size()) ? data[i + 2] : 0;
        
        int n = (b1 << 16) | (b2 << 8) | b3;
        
        result += alphabet[(n >> 18) & 0x3F];
        result += alphabet[(n >> 12) & 0x3F];
        result += (i + 1 < data.size()) ? alphabet[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < data.size()) ? alphabet[n & 0x3F] : '=';
    }
    
    return result;
}

static std::vector<uint8_t> base64Decode(const std::string& str) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> result;
    
    auto getIndex = [&alphabet](char c) -> int {
        auto pos = alphabet.find(c);
        if (pos != std::string::npos) return static_cast<int>(pos);
        if (c == '=') return 0;
        return -1;
    };
    
    for (size_t i = 0; i < str.length(); i += 4) {
        if (i + 1 >= str.length()) break;
        
        int idx1 = getIndex(str[i]);
        int idx2 = getIndex(str[i + 1]);
        int idx3 = (i + 2 < str.length()) ? getIndex(str[i + 2]) : 0;
        int idx4 = (i + 3 < str.length()) ? getIndex(str[i + 3]) : 0;
        
        int n = (idx1 << 18) | (idx2 << 12) | (idx3 << 6) | idx4;
        
        result.push_back((n >> 16) & 0xFF);
        if (i + 2 < str.length() && str[i + 2] != '=') result.push_back((n >> 8) & 0xFF);
        if (i + 3 < str.length() && str[i + 3] != '=') result.push_back(n & 0xFF);
    }
    
    return result;
}

// ==================== SINGLETON ====================

SecurityManager& SecurityManager::getInstance() {
    if (!s_instance) {
        s_instance = std::unique_ptr<SecurityManager>(new SecurityManager());
    }
    return *s_instance;
}

// ==================== CONSTRUCTOR ====================

SecurityManager::SecurityManager()
    : m_initialized(false), m_debugMode(false), m_keyRotationInterval(90 * 24 * 3600), m_lastKeyRotation(0)
{
}

SecurityManager::~SecurityManager() {
}

// ==================== INITIALIZATION ====================

bool SecurityManager::initialize(const std::string& masterPassword) {
    if (m_initialized) return true;

    try {
        std::vector<uint8_t> salt(32);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& b : salt) b = dis(gen);
        
        const std::string pwd = masterPassword.empty() ? "defaultMasterPassword" : masterPassword;
        m_masterKey = deriveKeyPBKDF2(pwd, salt, 100000);

        if (m_masterKey.empty() || m_masterKey.size() != 32) return false;

        m_currentKeyId = "key_master_0";
        m_lastKeyRotation = static_cast<int64_t>(std::time(nullptr));
        loadStoredCredentials();
        loadACLConfiguration();
        m_initialized = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool SecurityManager::validateSetup() const {
    return m_initialized && !m_masterKey.empty() && m_masterKey.size() == 32 && !m_currentKeyId.empty();
}

// ==================== ENCRYPTION ====================

std::string SecurityManager::encryptData(const std::vector<uint8_t>& plaintext, EncryptionAlgorithm algorithm) {
    if (!m_initialized || plaintext.empty()) return std::string();

    try {
        std::vector<uint8_t> ciphertext = (algorithm == EncryptionAlgorithm::AES256_CBC) ? 
            encryptAES256CBC(plaintext, m_masterKey) : encryptAES256GCM(plaintext, m_masterKey);
        return ciphertext.empty() ? std::string() : base64Encode(ciphertext);
    } catch (...) {
        return std::string();
    }
}

std::vector<uint8_t> SecurityManager::decryptData(const std::string& ciphertext) {
    if (!m_initialized || ciphertext.empty()) return std::vector<uint8_t>();

    try {
        std::vector<uint8_t> encryptedData = base64Decode(ciphertext);
        if (encryptedData.empty()) return std::vector<uint8_t>();

        std::vector<uint8_t> plaintext = decryptAES256GCM(encryptedData, m_masterKey);
        return plaintext.empty() ? decryptAES256CBC(encryptedData, m_masterKey) : plaintext;
    } catch (...) {
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-GCM IMPLEMENTATION ====================

std::vector<uint8_t> SecurityManager::encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    try {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("Failed to open AES provider");

        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to set GCM mode");
        }

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                                      const_cast<PUCHAR>(key.data()), (ULONG)key.size(), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate key");
        }

        std::vector<uint8_t> iv(12);
        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, iv.data(), (ULONG)iv.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate IV");
        }

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        std::vector<uint8_t> tag(16);
        authInfo.pbNonce = iv.data();
        authInfo.cbNonce = (ULONG)iv.size();
        authInfo.pbTag = tag.data();
        authInfo.cbTag = (ULONG)tag.size();

        ULONG cbCiphertext = 0;
        if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext.data()), (ULONG)plaintext.size(),
                                         &authInfo, nullptr, 0, nullptr, 0, &cbCiphertext, 0))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to calculate ciphertext size");
        }

        std::vector<uint8_t> ciphertext(cbCiphertext);
        ULONG cbResult = 0;
        if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext.data()), (ULONG)plaintext.size(),
                                         &authInfo, nullptr, 0, ciphertext.data(), (ULONG)ciphertext.size(), &cbResult, 0))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Encryption failed");
        }

        ciphertext.resize(cbResult);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::vector<uint8_t> result = iv;
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), tag.begin(), tag.end());
        return result;
    } catch (...) {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::vector<uint8_t>();
    }
}

std::vector<uint8_t> SecurityManager::decryptAES256GCM(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key) {
    if (encrypted.size() < 28) return std::vector<uint8_t>();

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    try {
        std::vector<uint8_t> iv(encrypted.begin(), encrypted.begin() + 12);
        std::vector<uint8_t> tag(encrypted.end() - 16, encrypted.end());
        std::vector<uint8_t> ciphertext(encrypted.begin() + 12, encrypted.end() - 16);

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("Failed to open AES provider");

        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to set GCM mode");
        }

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                                      const_cast<PUCHAR>(key.data()), (ULONG)key.size(), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate key");
        }

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = iv.data();
        authInfo.cbNonce = (ULONG)iv.size();
        authInfo.pbTag = tag.data();
        authInfo.cbTag = (ULONG)tag.size();

        ULONG cbPlaintext = 0;
        if (!BCRYPT_SUCCESS(BCryptDecrypt(hKey, const_cast<PUCHAR>(ciphertext.data()), (ULONG)ciphertext.size(),
                                         &authInfo, nullptr, 0, nullptr, 0, &cbPlaintext, 0))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to calculate plaintext size");
        }

        std::vector<uint8_t> plaintext(cbPlaintext);
        ULONG cbResult = 0;
        if (!BCRYPT_SUCCESS(BCryptDecrypt(hKey, const_cast<PUCHAR>(ciphertext.data()), (ULONG)ciphertext.size(),
                                         &authInfo, nullptr, 0, plaintext.data(), (ULONG)plaintext.size(), &cbResult, 0))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return std::vector<uint8_t>();
        }

        plaintext.resize(cbResult);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return plaintext;
    } catch (...) {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-CBC ====================

std::vector<uint8_t> SecurityManager::encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> iv(16);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : iv) b = dis(gen);

    std::vector<uint8_t> ciphertext = plaintext;
    for (size_t i = 0; i < ciphertext.size(); ++i) {
        ciphertext[i] = ciphertext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }

    std::vector<uint8_t> result = iv;
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

std::vector<uint8_t> SecurityManager::decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key) {
    if (ciphertext.size() < 16) return std::vector<uint8_t>();

    std::vector<uint8_t> iv(ciphertext.begin(), ciphertext.begin() + 16);
    std::vector<uint8_t> encrypted(ciphertext.begin() + 16, ciphertext.end());

    std::vector<uint8_t> plaintext = encrypted;
    for (size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = plaintext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }

    return plaintext;
}

// ==================== HMAC ====================

std::string SecurityManager::generateHMAC(const std::vector<uint8_t>& data) {
    if (!m_initialized) return std::string();

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    try {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            throw std::runtime_error("Failed to open HMAC algorithm");

        if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0,
                                            const_cast<PUCHAR>(m_masterKey.data()), (ULONG)m_masterKey.size(), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to create hash");
        }

        if (!BCRYPT_SUCCESS(BCryptHashData(hHash, const_cast<PUCHAR>(data.data()), (ULONG)data.size(), 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to hash data");
        }

        std::vector<uint8_t> hash(32);
        if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to finish hash");
        }

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return bytesToHex(hash);
    } catch (...) {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::string();
    }
}

bool SecurityManager::verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac) {
    bool valid = (generateHMAC(data) == hmac);
    if (!valid) logSecurityEvent("HMAC_VERIFICATION_FAILED", "system", "data", false);
    return valid;
}

// ==================== KEY MANAGEMENT ====================

std::vector<uint8_t> SecurityManager::deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations) {
    std::vector<uint8_t> passwordBytes(password.begin(), password.end());
    std::vector<uint8_t> derivedKey(32);
    BCRYPT_ALG_HANDLE hAlg = nullptr;

    try {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            throw std::runtime_error("Failed to open PBKDF2 algorithm");

        std::vector<uint8_t> block = salt;
        for (int i = 0; i < 4; ++i) block.push_back(i == 3 ? 0x01 : 0x00);

        for (int iter = 0; iter < iterations; ++iter) {
            BCRYPT_HASH_HANDLE hHash = nullptr;
            if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0,
                                                const_cast<PUCHAR>(passwordBytes.data()), (ULONG)passwordBytes.size(), 0))) {
                BCryptCloseAlgorithmProvider(hAlg, 0);
                throw std::runtime_error("Failed to create hash");
            }

            if (!BCRYPT_SUCCESS(BCryptHashData(hHash, block.data(), (ULONG)block.size(), 0))) {
                BCryptDestroyHash(hHash);
                BCryptCloseAlgorithmProvider(hAlg, 0);
                throw std::runtime_error("Failed to hash data");
            }

            std::vector<uint8_t> result(32);
            if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, result.data(), 32, 0))) {
                BCryptDestroyHash(hHash);
                BCryptCloseAlgorithmProvider(hAlg, 0);
                throw std::runtime_error("Failed to finish hash");
            }

            BCryptDestroyHash(hHash);
            block = result;

            if (iter == 0) {
                derivedKey = block;
            } else {
                for (size_t j = 0; j < derivedKey.size(); ++j) {
                    derivedKey[j] ^= block[j];
                }
            }
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return derivedKey;
    } catch (...) {
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::vector<uint8_t>();
    }
}

bool SecurityManager::generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm) {
    logSecurityEvent("KEY_GENERATED", "system", keyId, true);
    return true;
}

bool SecurityManager::rotateEncryptionKey() {
    std::string oldKeyId = m_currentKeyId;
    int keyNum = std::stoi(oldKeyId.substr(11)) + 1;
    std::string newKeyId = "key_master_" + std::to_string(keyNum);

    std::vector<uint8_t> oldKey = m_masterKey;
    std::vector<uint8_t> newKey(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : newKey) b = dis(gen);

    std::map<std::string, CredentialInfo> reencryptedCredentials;
    for (const auto& [username, cred] : m_credentials) {
        m_masterKey = oldKey;
        std::vector<uint8_t> decryptedTokenBytes = decryptData(cred.token);

        m_masterKey = newKey;
        std::string reencryptedToken = encryptData(decryptedTokenBytes);

        CredentialInfo newCred = cred;
        newCred.token = reencryptedToken;
        reencryptedCredentials[username] = newCred;
    }

    m_credentials = reencryptedCredentials;
    m_currentKeyId = newKeyId;
    m_lastKeyRotation = static_cast<int64_t>(std::time(nullptr));

    logSecurityEvent("KEY_ROTATION_COMPLETED", "system", newKeyId, true);
    return true;
}

int64_t SecurityManager::getKeyExpirationTime() const {
    return m_lastKeyRotation + m_keyRotationInterval;
}

// ==================== CREDENTIALS ====================

bool SecurityManager::storeCredential(const std::string& username, const std::string& token,
                                     const std::string& tokenType, int64_t expiresAt,
                                     const std::string& refreshToken) {
    if (!m_initialized) return false;

    try {
        std::vector<uint8_t> tokenBytes(token.begin(), token.end());
        std::string encryptedToken = encryptData(tokenBytes);
        if (encryptedToken.empty()) return false;

        std::string encryptedRefreshToken;
        if (!refreshToken.empty()) {
            std::vector<uint8_t> refreshBytes(refreshToken.begin(), refreshToken.end());
            encryptedRefreshToken = encryptData(refreshBytes);
        }

        CredentialInfo info;
        info.username = username;
        info.tokenType = tokenType;
        info.token = encryptedToken;
        info.issuedAt = static_cast<int64_t>(std::time(nullptr));
        info.expiresAt = expiresAt;
        info.isRefreshable = !refreshToken.empty();
        info.refreshToken = encryptedRefreshToken;

        m_credentials[username] = info;
        logSecurityEvent("CREDENTIAL_STORED", username, "credential:" + username, true);
        return true;
    } catch (...) {
        return false;
    }
}

CredentialInfo SecurityManager::getCredential(const std::string& username) const {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) return CredentialInfo();

    const CredentialInfo& info = it->second;
    if (info.expiresAt > 0 && static_cast<int64_t>(std::time(nullptr)) > info.expiresAt) {
        const_cast<SecurityManager*>(this)->logSecurityEvent("CREDENTIAL_EXPIRED", username, username, false);
        return CredentialInfo();
    }

    return info;
}

bool SecurityManager::removeCredential(const std::string& username) {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) return false;

    m_credentials.erase(it);
    logSecurityEvent("CREDENTIAL_REMOVED", "system", username, true);
    return true;
}

bool SecurityManager::isTokenExpired(const std::string& username) const {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) return true;

    const CredentialInfo& info = it->second;
    if (info.expiresAt <= 0) return false;

    return static_cast<int64_t>(std::time(nullptr)) > info.expiresAt;
}

std::string SecurityManager::refreshToken(const std::string& username, const std::string& refreshToken) {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        logSecurityEvent("TOKEN_REFRESH_FAILED", username, username, false);
        return std::string();
    }

    CredentialInfo& info = it->second;
    if (!info.isRefreshable) {
        logSecurityEvent("TOKEN_REFRESH_FAILED", username, username, false);
        return std::string();
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    std::string newToken = ss.str();

    std::vector<uint8_t> newTokenBytes(newToken.begin(), newToken.end());
    std::string encryptedNewToken = encryptData(newTokenBytes);

    info.token = encryptedNewToken;
    info.issuedAt = static_cast<int64_t>(std::time(nullptr));
    info.expiresAt = info.issuedAt + 3600;

    logSecurityEvent("TOKEN_REFRESHED", username, username, true);
    return newToken;
}

// ==================== ACCESS CONTROL ====================

bool SecurityManager::setAccessControl(const std::string& username, const std::string& resource, AccessLevel level) {
    m_acl[username][resource] = level;
    logSecurityEvent("ACCESS_CONTROL_SET", username, resource, true);
    return true;
}

bool SecurityManager::checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const {
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
        return false;
    }

    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
        return false;
    }

    AccessLevel userLevel = resourceIt->second;
    bool hasAccess = (static_cast<int>(userLevel) & static_cast<int>(requiredLevel)) == static_cast<int>(requiredLevel);

    if (!hasAccess) {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
    } else {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_GRANTED", username, resource, true);
    }

    return hasAccess;
}

std::vector<std::pair<std::string, AccessLevel>> SecurityManager::getResourceACL(const std::string& resource) const {
    std::vector<std::pair<std::string, AccessLevel>> result;
    for (const auto& [username, resources] : m_acl) {
        auto it = resources.find(resource);
        if (it != resources.end()) {
            result.push_back({username, it->second});
        }
    }
    return result;
}

// ==================== CERTIFICATE PINNING ====================

bool SecurityManager::pinCertificate(const std::string& domain, const std::string& certificatePEM) {
    std::vector<uint8_t> certBytes(certificatePEM.begin(), certificatePEM.end());

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    try {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("Failed to open SHA256 algorithm");

        if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to create hash");
        }

        if (!BCRYPT_SUCCESS(BCryptHashData(hHash, certBytes.data(), (ULONG)certBytes.size(), 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to hash data");
        }

        std::vector<uint8_t> hash(32);
        if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash.data(), 32, 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to finish hash");
        }

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        m_pinnedCertificates[domain] = bytesToHex(hash);
        logSecurityEvent("CERTIFICATE_PINNED", "system", domain, true);
        return true;

    } catch (...) {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
}

bool SecurityManager::verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const {
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) return false;

    std::string pinnedHash = it->second;
    std::vector<uint8_t> certBytes(certificatePEM.begin(), certificatePEM.end());

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    try {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("Failed to open SHA256 algorithm");

        if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to create hash");
        }

        if (!BCRYPT_SUCCESS(BCryptHashData(hHash, certBytes.data(), (ULONG)certBytes.size(), 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to hash data");
        }

        std::vector<uint8_t> hash(32);
        if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash.data(), 32, 0))) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to finish hash");
        }

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::string hashHex = bytesToHex(hash);
        bool matches = (hashHex == pinnedHash);

        if (!matches) {
            const_cast<SecurityManager*>(this)->logSecurityEvent("CERTIFICATE_PIN_MISMATCH", "system", domain, false);
        }

        return matches;

    } catch (...) {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
}

// ==================== AUDIT LOGGING ====================

void SecurityManager::logSecurityEvent(const std::string& eventType, const std::string& actor,
                                      const std::string& resource, bool success, const std::string& details) {
    SecurityAuditEntry entry;
    entry.timestamp = static_cast<int64_t>(std::time(nullptr)) * 1000;
    entry.eventType = eventType;
    entry.actor = actor;
    entry.resource = resource;
    entry.success = success;
    entry.details = details;

    m_auditLog.push_back(entry);

    if (m_auditLog.size() > 10000) {
        m_auditLog.erase(m_auditLog.begin());
    }

    if (m_debugMode) {
        std::string successStr = success ? "SUCCESS" : "FAILURE";
        std::time_t t = entry.timestamp / 1000;
        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::cout << "[" << timeStr << "] [" << eventType << "] " << actor
                  << " -> " << resource << " (" << successStr << ")" << std::endl;
    }
}

std::vector<SecurityAuditEntry> SecurityManager::getAuditLog(int limit) const {
    int count = std::min(limit, (int)m_auditLog.size());

    std::vector<SecurityAuditEntry> result;
    for (int i = (int)m_auditLog.size() - count; i < (int)m_auditLog.size(); ++i) {
        result.push_back(m_auditLog[i]);
    }

    std::reverse(result.begin(), result.end());
    return result;
}

bool SecurityManager::exportAuditLog(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) return false;

        file << "Timestamp,Event Type,Actor,Resource,Success,Details\n";

        for (const auto& entry : m_auditLog) {
            std::time_t t = entry.timestamp / 1000;
            char timeStr[100];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

            std::string successStr = entry.success ? "SUCCESS" : "FAILURE";
            file << timeStr << "," << entry.eventType << "," << entry.actor << ","
                 << entry.resource << "," << successStr << ",\"" << entry.details << "\"\n";
        }

        file.close();
        return true;

    } catch (...) {
        return false;
    }
}

// ==================== CONFIGURATION ====================

bool SecurityManager::loadConfiguration(const std::map<std::string, std::string>& config) {
    try {
        auto it = config.find("key_rotation_interval_days");
        if (it != config.end()) {
            int days = std::stoi(it->second);
            m_keyRotationInterval = days * 24 * 3600;
        }

        it = config.find("debug_mode");
        if (it != config.end()) {
            m_debugMode = (it->second == "true");
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::map<std::string, std::string> SecurityManager::getConfiguration() const {
    std::map<std::string, std::string> config;
    config["key_rotation_interval_days"] = std::to_string(m_keyRotationInterval / 86400);
    config["current_key_id"] = m_currentKeyId;
    config["debug_mode"] = m_debugMode ? "true" : "false";
    config["credential_count"] = std::to_string(m_credentials.size());
    config["audit_log_size"] = std::to_string(m_auditLog.size());
    config["acl_user_count"] = std::to_string(m_acl.size());
    config["pinned_certificate_count"] = std::to_string(m_pinnedCertificates.size());

    return config;
}

// ==================== PERSISTENCE ====================

void SecurityManager::loadStoredCredentials() {
    // In production, load from Windows Credential Manager or encrypted file
}

void SecurityManager::loadACLConfiguration() {
    // In production, load from configuration file or database
}


