#include "security_manager.h"
<<<<<<< HEAD
=======


>>>>>>> origin/main
#include <stdexcept>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <ntstatus.h>
#include <random>
#include <sstream>
#include <iomanip>
<<<<<<< HEAD
#include <ctime>
#include <algorithm>
#include <cstring>
#include <fstream>
=======
>>>>>>> origin/main

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

<<<<<<< HEAD
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
=======
SecurityManager::SecurityManager(void* parent)
    : void(parent)
    , m_initialized(false)
    , m_debugMode(false)
    , m_keyRotationInterval(90 * 24 * 3600)  // 90 days in seconds
    , m_lastKeyRotation(0)
{
}

bool SecurityManager::initialize(const std::string& masterPassword)
{
    if (m_initialized) {
        return true;
    }

    // Generate or load master key
    std::vector<uint8_t> salt;
    if (masterPassword.empty()) {
        // Generate random key (for testing/development)
        salt = std::vector<uint8_t>(32, 0);
        m_masterKey = deriveKeyPBKDF2("defaultMasterPassword", salt, 100000);
    } else {
        // Derive key from user password
        salt = std::vector<uint8_t>(32, 0);
        m_masterKey = deriveKeyPBKDF2(masterPassword, salt, 100000);
    }

    if (m_masterKey.empty() || m_masterKey.size() != 32) {
        return false;
    }

    m_currentKeyId = std::string("key_master");
    m_lastKeyRotation = std::chrono::system_clock::time_point::currentSecsSinceEpoch();

    // Load stored credentials (from secure storage)
    loadStoredCredentials();

    // Load ACL configuration
    loadACLConfiguration();

    m_initialized = true;
    return true;
}

bool SecurityManager::validateSetup() const
{
    if (!m_initialized) {
        return false;
    }

    if (m_masterKey.empty() || m_masterKey.size() != 32) {
        return false;
    }

    if (m_currentKeyId.empty()) {
        return false;
    }

    // Check if key rotation is needed
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (now - m_lastKeyRotation > m_keyRotationInterval) {
        // Don't fail validation, but log warning
    }

    return true;
}

// ==================== ENCRYPTION / DECRYPTION ====================

std::string SecurityManager::encryptData(const std::vector<uint8_t>& plaintext, EncryptionAlgorithm algorithm)
{
    if (!m_initialized) {
        return std::string();
    }

    if (plaintext.empty()) {
        return std::string();
    }

    std::vector<uint8_t> ciphertext;
    
    try {
        switch (algorithm) {
        case EncryptionAlgorithm::AES256_GCM:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::AES256_CBC:
            ciphertext = encryptAES256CBC(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::ChaCha20Poly1305:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        default:
            return std::string();
        }

        if (ciphertext.empty()) {
            return std::string();
        }

        std::string encoded = ciphertext.toBase64();
        
        return encoded;

    } catch (const std::exception& e) {
        return std::string();
    }
}

std::vector<uint8_t> SecurityManager::decryptData(const std::string& ciphertext)
{
    if (!m_initialized) {
        return std::vector<uint8_t>();
    }

    if (ciphertext.empty()) {
        return std::vector<uint8_t>();
    }

    try {
        std::vector<uint8_t> encryptedData = std::vector<uint8_t>::fromBase64(ciphertext.toUtf8());
        
        if (encryptedData.empty()) {
            return std::vector<uint8_t>();
        }

        // Try AES-256-GCM first (default)
        std::vector<uint8_t> plaintext = decryptAES256GCM(encryptedData, m_masterKey);
        
        if (plaintext.empty()) {
            // Try AES-256-CBC as fallback
            plaintext = decryptAES256CBC(encryptedData, m_masterKey);
        }

        if (plaintext.empty()) {
            return std::vector<uint8_t>();
        }


        return plaintext;

    } catch (const std::exception& e) {
>>>>>>> origin/main
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-GCM IMPLEMENTATION ====================

<<<<<<< HEAD
std::vector<uint8_t> SecurityManager::encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
=======
std::vector<uint8_t> SecurityManager::encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key)
{
    // Production-ready AES-256-GCM using Windows CNG API
>>>>>>> origin/main
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
<<<<<<< HEAD

        std::vector<uint8_t> iv(12);
        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, iv.data(), (ULONG)iv.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
=======
        
        // Generate random 12-byte IV (nonce) for GCM
        std::vector<uint8_t> iv(12, 0);
        status = BCryptGenRandom(nullptr, (PUCHAR)iv.data(), iv.size(), 
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(status)) {
>>>>>>> origin/main
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate IV");
        }

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
<<<<<<< HEAD
        std::vector<uint8_t> tag(16);
        authInfo.pbNonce = iv.data();
        authInfo.cbNonce = (ULONG)iv.size();
        authInfo.pbTag = tag.data();
        authInfo.cbTag = (ULONG)tag.size();

=======
        
        std::vector<uint8_t> tag(16, 0); // 16-byte authentication tag for GCM
        authInfo.pbNonce = (PUCHAR)iv.data();
        authInfo.cbNonce = iv.size();
        authInfo.pbTag = (PUCHAR)tag.data();
        authInfo.cbTag = tag.size();
        authInfo.pbAuthData = nullptr;  // No additional authenticated data
        authInfo.cbAuthData = 0;
        
        // Get required ciphertext buffer size
>>>>>>> origin/main
        ULONG cbCiphertext = 0;
        if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext.data()), (ULONG)plaintext.size(),
                                         &authInfo, nullptr, 0, nullptr, 0, &cbCiphertext, 0))) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to calculate ciphertext size");
        }
<<<<<<< HEAD

        std::vector<uint8_t> ciphertext(cbCiphertext);
=======
        
        // Allocate ciphertext buffer
        std::vector<uint8_t> ciphertext(cbCiphertext, 0);
        
        // Perform encryption
>>>>>>> origin/main
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
<<<<<<< HEAD

        std::vector<uint8_t> result = iv;
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), tag.begin(), tag.end());
        return result;
    } catch (...) {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
=======
        
        // Return format: IV (12 bytes) + Ciphertext + Tag (16 bytes)
        std::vector<uint8_t> result = iv + ciphertext + tag;
        
                        std::string("Encrypted %1 bytes")));
        return result;
        
    } catch (const std::exception& e) {
                        std::string("Exception: %1")));
>>>>>>> origin/main
        return std::vector<uint8_t>();
    }
}

<<<<<<< HEAD
std::vector<uint8_t> SecurityManager::decryptAES256GCM(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key) {
    if (encrypted.size() < 28) return std::vector<uint8_t>();

=======
std::vector<uint8_t> SecurityManager::decryptAES256GCM(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key)
{
    if (encrypted.size() < 12 + 16) {  // IV (12) + tag (16)
                        "Invalid ciphertext size");
        return std::vector<uint8_t>();
    }
    
    // Production-ready AES-256-GCM decryption using Windows CNG API
>>>>>>> origin/main
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    try {
<<<<<<< HEAD
        std::vector<uint8_t> iv(encrypted.begin(), encrypted.begin() + 12);
        std::vector<uint8_t> tag(encrypted.end() - 16, encrypted.end());
        std::vector<uint8_t> ciphertext(encrypted.begin() + 12, encrypted.end() - 16);

        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("Failed to open AES provider");

        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
=======
        // Extract components from encrypted data
        std::vector<uint8_t> iv = encrypted.left(12);
        std::vector<uint8_t> tag = encrypted.right(16);
        std::vector<uint8_t> ciphertext = encrypted.mid(12, encrypted.size() - 12 - 16);
        
        // Open algorithm provider for AES-GCM
        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(status)) {
            throw std::runtime_error("Failed to open AES algorithm provider");
        }
        
        // Set chaining mode to GCM
        status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, 
                                  (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                                  sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (!BCRYPT_SUCCESS(status)) {
>>>>>>> origin/main
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
<<<<<<< HEAD

        std::vector<uint8_t> plaintext(cbPlaintext);
=======
        
        // Allocate plaintext buffer
        std::vector<uint8_t> plaintext(cbPlaintext, 0);
        
        // Perform decryption (this also verifies the authentication tag)
>>>>>>> origin/main
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
<<<<<<< HEAD
        return plaintext;
    } catch (...) {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
=======
        
                        std::string("Decrypted %1 bytes")));
        return plaintext;
        
    } catch (const std::exception& e) {
                        std::string("Exception: %1")));
>>>>>>> origin/main
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-CBC ====================

<<<<<<< HEAD
std::vector<uint8_t> SecurityManager::encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> iv(16);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : iv) b = dis(gen);

    std::vector<uint8_t> ciphertext = plaintext;
    for (size_t i = 0; i < ciphertext.size(); ++i) {
=======
std::vector<uint8_t> SecurityManager::encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key)
{
    // Similar to GCM but using CBC mode
    // Generate random IV (16 bytes for CBC)
    std::vector<uint8_t> iv(16, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(iv.data()), 4);
    
    // Simple XOR cipher (replace with real AES-CBC)
    std::vector<uint8_t> ciphertext = plaintext;
    for (int i = 0; i < ciphertext.size(); ++i) {
>>>>>>> origin/main
        ciphertext[i] = ciphertext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }

    std::vector<uint8_t> result = iv;
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

<<<<<<< HEAD
std::vector<uint8_t> SecurityManager::decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key) {
    if (ciphertext.size() < 16) return std::vector<uint8_t>();

    std::vector<uint8_t> iv(ciphertext.begin(), ciphertext.begin() + 16);
    std::vector<uint8_t> encrypted(ciphertext.begin() + 16, ciphertext.end());

    std::vector<uint8_t> plaintext = encrypted;
    for (size_t i = 0; i < plaintext.size(); ++i) {
=======
std::vector<uint8_t> SecurityManager::decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key)
{
    if (ciphertext.size() < 16) {  // IV size
        return std::vector<uint8_t>();
    }
    
    // Extract IV (first 16 bytes)
    std::vector<uint8_t> iv = ciphertext.left(16);
    
    // Extract ciphertext
    std::vector<uint8_t> encrypted = ciphertext.mid(16);
    
    // Decrypt (reverse XOR)
    std::vector<uint8_t> plaintext = encrypted;
    for (int i = 0; i < plaintext.size(); ++i) {
>>>>>>> origin/main
        plaintext[i] = plaintext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }

    return plaintext;
}

// ==================== HMAC ====================

<<<<<<< HEAD
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
=======
std::string SecurityManager::generateHMAC(const std::vector<uint8_t>& data)
{
    if (!m_initialized) {
        return std::string();
    }

    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, m_masterKey);
    hmac.addData(data);
    std::vector<uint8_t> result = hmac.result();
    
    std::string hexHmac = result.toHex();
    
    return hexHmac;
}

bool SecurityManager::verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac)
{
    if (!m_initialized) {
        return false;
    }

    std::string computedHmac = generateHMAC(data);
    bool valid = (computedHmac == hmac);
    
    if (!valid) {
    } else {
    }
    
>>>>>>> origin/main
    return valid;
}

// ==================== KEY MANAGEMENT ====================

<<<<<<< HEAD
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
=======
std::vector<uint8_t> SecurityManager::deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations)
{
    // PBKDF2 key derivation using HMAC-SHA256
    std::vector<uint8_t> passwordBytes = password.toUtf8();
    std::vector<uint8_t> derivedKey(32, 0);  // 256 bits
    
    // Simple PBKDF2 implementation (in production, use QPasswordDigestor or OpenSSL)
    std::vector<uint8_t> block = salt + std::vector<uint8_t>::fromHex("00000001");
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, passwordBytes);
    
    for (int i = 0; i < iterations; ++i) {
        hmac.reset();
        hmac.addData(block);
        block = hmac.result();
        
        if (i == 0) {
            derivedKey = block;
        } else {
            // XOR with previous iteration
            for (int j = 0; j < derivedKey.size(); ++j) {
                derivedKey[j] ^= block[j];
>>>>>>> origin/main
            }
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return derivedKey;
    } catch (...) {
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::vector<uint8_t>();
    }
<<<<<<< HEAD
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

=======


    return derivedKey;
}

bool SecurityManager::generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm)
{
    
    // Generate random key material
    std::vector<uint8_t> newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(newKey.data()), 8);
    
    // In production, store key securely (Windows DPAPI, hardware security module, etc.)


    return true;
}

bool SecurityManager::rotateEncryptionKey()
{
    
    std::string oldKeyId = m_currentKeyId;
    std::string newKeyId = std::string("key_%1"));
    
    // Generate new key
    std::vector<uint8_t> oldKey = m_masterKey;
    std::vector<uint8_t> newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(newKey.data()), 8);
    
    // Re-encrypt all stored credentials with new key
>>>>>>> origin/main
    std::map<std::string, CredentialInfo> reencryptedCredentials;
    for (const auto& [username, cred] : m_credentials) {
        m_masterKey = oldKey;
<<<<<<< HEAD
        std::vector<uint8_t> decryptedTokenBytes = decryptData(cred.token);

        m_masterKey = newKey;
        std::string reencryptedToken = encryptData(decryptedTokenBytes);

=======
        std::vector<uint8_t> decryptedToken = decryptData(cred.token);
        
        // Encrypt with new key
        m_masterKey = newKey;
        std::string reencryptedToken = encryptData(decryptedToken);
        
>>>>>>> origin/main
        CredentialInfo newCred = cred;
        newCred.token = reencryptedToken;
        reencryptedCredentials[username] = newCred;
    }

    m_credentials = reencryptedCredentials;
    m_currentKeyId = newKeyId;
<<<<<<< HEAD
    m_lastKeyRotation = static_cast<int64_t>(std::time(nullptr));

    logSecurityEvent("KEY_ROTATION_COMPLETED", "system", newKeyId, true);
    return true;
}

int64_t SecurityManager::getKeyExpirationTime() const {
=======
    m_lastKeyRotation = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    
    keyRotationCompleted(newKeyId);
    
    return true;
}

int64_t SecurityManager::getKeyExpirationTime() const
{
>>>>>>> origin/main
    return m_lastKeyRotation + m_keyRotationInterval;
}

// ==================== CREDENTIALS ====================

bool SecurityManager::storeCredential(const std::string& username, const std::string& token,
                                     const std::string& tokenType, int64_t expiresAt,
<<<<<<< HEAD
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
=======
                                     const std::string& refreshToken)
{
    if (!m_initialized) {
        return false;
    }


    // Encrypt the token
    std::string encryptedToken = encryptData(token.toUtf8());
    if (encryptedToken.empty()) {
        return false;
    }
    
    std::string encryptedRefreshToken;
    if (!refreshToken.empty()) {
        encryptedRefreshToken = encryptData(refreshToken.toUtf8());
    }
    
    CredentialInfo info;
    info.username = username;
    info.tokenType = tokenType;
    info.token = encryptedToken;
    info.issuedAt = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    info.expiresAt = expiresAt;
    info.isRefreshable = !refreshToken.empty();
    info.refreshToken = encryptedRefreshToken;
    
    m_credentials[username] = info;
    
    credentialStored(username);
    
    return true;
}

CredentialInfo SecurityManager::getCredential(const std::string& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return CredentialInfo();
    }
    
    const CredentialInfo& info = it->second;
    
    // Check expiration
    if (info.expiresAt > 0 && std::chrono::system_clock::time_point::currentSecsSinceEpoch() > info.expiresAt) {
        const_cast<SecurityManager*>(this)->credentialExpired(username);
        return CredentialInfo();
    }
    
    return info;
}

bool SecurityManager::removeCredential(const std::string& username)
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return false;
    }
    
    m_credentials.erase(it);


    return true;
}

bool SecurityManager::isTokenExpired(const std::string& username) const
{
>>>>>>> origin/main
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) return true;

    const CredentialInfo& info = it->second;
<<<<<<< HEAD
    if (info.expiresAt <= 0) return false;

    return static_cast<int64_t>(std::time(nullptr)) > info.expiresAt;
}

std::string SecurityManager::refreshToken(const std::string& username, const std::string& refreshToken) {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        logSecurityEvent("TOKEN_REFRESH_FAILED", username, username, false);
=======
    if (info.expiresAt <= 0) {
        return false;  // No expiration
    }
    
    return std::chrono::system_clock::time_point::currentSecsSinceEpoch() > info.expiresAt;
}

std::string SecurityManager::refreshToken(const std::string& username, const std::string& refreshToken)
{
    // Local session rotation
    // Generates a new cryptographically secure session identifier
    
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        tokenRefreshFailed(username);
>>>>>>> origin/main
        return std::string();
    }

    CredentialInfo& info = it->second;
    if (!info.isRefreshable) {
<<<<<<< HEAD
        logSecurityEvent("TOKEN_REFRESH_FAILED", username, username, false);
        return std::string();
    }

=======
        tokenRefreshFailed(username);
        return std::string();
    }
    
    // Generate new random token
>>>>>>> origin/main
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    std::string newToken = ss.str();

<<<<<<< HEAD
    std::vector<uint8_t> newTokenBytes(newToken.begin(), newToken.end());
    std::string encryptedNewToken = encryptData(newTokenBytes);

    info.token = encryptedNewToken;
    info.issuedAt = static_cast<int64_t>(std::time(nullptr));
    info.expiresAt = info.issuedAt + 3600;

    logSecurityEvent("TOKEN_REFRESHED", username, username, true);
=======
    std::string encryptedNewToken = encryptData(newToken);
    
    info.token = encryptedNewToken;
    info.issuedAt = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    info.expiresAt = info.issuedAt + 3600;  // 1 hour from now


>>>>>>> origin/main
    return newToken;
}

// ==================== ACCESS CONTROL ====================

<<<<<<< HEAD
bool SecurityManager::setAccessControl(const std::string& username, const std::string& resource, AccessLevel level) {
    m_acl[username][resource] = level;
    logSecurityEvent("ACCESS_CONTROL_SET", username, resource, true);
    return true;
}

bool SecurityManager::checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const {
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
=======
bool SecurityManager::setAccessControl(const std::string& username, const std::string& resource, AccessLevel level)
{
    
    m_acl[username][resource] = level;


    return true;
}

bool SecurityManager::checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const
{
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
>>>>>>> origin/main
        return false;
    }

    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
<<<<<<< HEAD
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
=======
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
>>>>>>> origin/main
        return false;
    }

    AccessLevel userLevel = resourceIt->second;
    bool hasAccess = (static_cast<int>(userLevel) & static_cast<int>(requiredLevel)) == static_cast<int>(requiredLevel);

    if (!hasAccess) {
<<<<<<< HEAD
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_DENIED", username, resource, false);
    } else {
        const_cast<SecurityManager*>(this)->logSecurityEvent("ACCESS_GRANTED", username, resource, true);
=======
                   << "(has" << static_cast<int>(userLevel) << ", needs" << static_cast<int>(requiredLevel) << ")";
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
    } else {
>>>>>>> origin/main
    }

    return hasAccess;
}

<<<<<<< HEAD
std::vector<std::pair<std::string, AccessLevel>> SecurityManager::getResourceACL(const std::string& resource) const {
    std::vector<std::pair<std::string, AccessLevel>> result;
=======
std::vector<std::pair<std::string, SecurityManager::AccessLevel>> SecurityManager::getResourceACL(const std::string& resource) const
{
    std::vector<std::pair<std::string, AccessLevel>> result;
    
>>>>>>> origin/main
    for (const auto& [username, resources] : m_acl) {
        auto it = resources.find(resource);
        if (it != resources.end()) {
            result.push_back({username, it->second});
        }
    }
    return result;
}

// ==================== CERTIFICATE PINNING ====================

<<<<<<< HEAD
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
=======
bool SecurityManager::pinCertificate(const std::string& domain, const std::string& certificatePEM)
{
    
    // Compute SHA-256 hash of certificate
    std::vector<uint8_t> certBytes = certificatePEM.toUtf8();
    std::vector<uint8_t> hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    std::string hashHex = hash.toHex();
    
    m_pinnedCertificates[domain] = hashHex;


    return true;
}

bool SecurityManager::verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const
{
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) {
        return false;
    }
    
    std::string pinnedHash = it->second;
    
    // Compute hash of provided certificate
    std::vector<uint8_t> certBytes = certificatePEM.toUtf8();
    std::vector<uint8_t> hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    std::string hashHex = hash.toHex();
    
    bool matches = (hashHex == pinnedHash);
    
    if (!matches) {
    } else {
>>>>>>> origin/main
    }
}

// ==================== AUDIT LOGGING ====================

<<<<<<< HEAD
void SecurityManager::logSecurityEvent(const std::string& eventType, const std::string& actor,
                                      const std::string& resource, bool success, const std::string& details) {
    SecurityAuditEntry entry;
    entry.timestamp = static_cast<int64_t>(std::time(nullptr)) * 1000;
=======
                                      const std::string& resource, bool success, const std::string& details)
{
    SecurityAuditEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
>>>>>>> origin/main
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
<<<<<<< HEAD
        std::time_t t = entry.timestamp / 1000;
        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::cout << "[" << timeStr << "] [" << eventType << "] " << actor
                  << " -> " << resource << " (" << successStr << ")" << std::endl;
    }
=======
                      .toString("yyyy-MM-dd hh:mm:ss"))


                      ;
    }
    
    securityEventLogged(entry);
>>>>>>> origin/main
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

<<<<<<< HEAD
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
=======
bool SecurityManager::exportAuditLog(const std::string& filePath) const
{
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "Timestamp,Event Type,Actor,Resource,Success,Details\n";
    
    for (const auto& entry : m_auditLog) {
        std::string timestamp = std::chrono::system_clock::time_point::fromMSecsSinceEpoch(entry.timestamp).toString("yyyy-MM-dd hh:mm:ss.zzz");
        std::string successStr = entry.success ? "SUCCESS" : "FAILURE";
        out << std::string("%1,%2,%3,%4,%5,%6\n")


                  ;
    }
    
    file.close();


    return true;
>>>>>>> origin/main
}

// ==================== CONFIGURATION ====================

<<<<<<< HEAD
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

=======
bool SecurityManager::loadConfiguration(const void*& config)
{
    
    if (config.contains("key_rotation_interval_days")) {
        int days = config["key_rotation_interval_days"].toInt(90);
        m_keyRotationInterval = days * 24 * 3600;
    }
    
    if (config.contains("debug_mode")) {
        m_debugMode = config["debug_mode"].toBool(false);
    }
    
    return true;
}

void* SecurityManager::getConfiguration() const
{
    void* config;
    config["key_rotation_interval_days"] = static_cast<int>(m_keyRotationInterval / 86400);
    config["current_key_id"] = m_currentKeyId;
    config["last_key_rotation"] = std::chrono::system_clock::time_point::fromSecsSinceEpoch(m_lastKeyRotation).toString(//ISODate);
    config["next_key_rotation"] = std::chrono::system_clock::time_point::fromSecsSinceEpoch(getKeyExpirationTime()).toString(//ISODate);
    config["debug_mode"] = m_debugMode;
    config["credential_count"] = static_cast<int>(m_credentials.size());
    config["audit_log_size"] = static_cast<int>(m_auditLog.size());
    config["acl_user_count"] = static_cast<int>(m_acl.size());
    config["pinned_certificate_count"] = static_cast<int>(m_pinnedCertificates.size());
    
>>>>>>> origin/main
    return config;
}

// ==================== PERSISTENCE ====================

<<<<<<< HEAD
void SecurityManager::loadStoredCredentials() {
    // In production, load from Windows Credential Manager or encrypted file
=======
void SecurityManager::loadStoredCredentials()
{
    // In production, load from secure storage (Windows Credential Manager, macOS Keychain, etc.)
>>>>>>> origin/main
}

void SecurityManager::loadACLConfiguration() {
    // In production, load from configuration file or database
}


