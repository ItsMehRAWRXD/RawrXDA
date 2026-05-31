// secure_settings_api.cpp — Production secure settings implementation

#include "secure_settings_api.h"
#include <cryptprotect.h>
#include <string>
#include <cstdio>
#include <vector>
#include <unordered_map>

// LAZY SINGLETON PATTERN: Avoid SIOF - non-trivial constructors
inline std::unordered_map<std::string, std::vector<BYTE>>& GetEncryptedKeys() {
    static std::unordered_map<std::string, std::vector<BYTE>>* inst = new std::unordered_map<std::string, std::vector<BYTE>>();
    return *inst;
}
#define g_encryptedKeys GetEncryptedKeys()

inline std::mutex& GetStorageMutex() {
    static std::mutex* inst = new std::mutex();
    return *inst;
}
#define g_storageMutex GetStorageMutex()

extern "C" bool SecureStorage_SaveApiKey(const char* keyName, const char* apiKey) {
    if (!keyName || !apiKey) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(g_storageMutex);
    
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(apiKey));
    inBlob.cbData = static_cast<DWORD>(strlen(apiKey) + 1);
    
    DATA_BLOB outBlob = {};
    if (!CryptProtectData(&inBlob, L"RawrXD API Key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        return false;
    }
    
    std::vector<BYTE> encrypted(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    LocalFree(outBlob.pbData);
    
    g_encryptedKeys[keyName] = std::move(encrypted);
    return true;
}

extern "C" bool SecureStorage_LoadApiKey(const char* keyName, char* buffer, size_t bufferSize) {
    if (!keyName || !buffer || bufferSize == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(g_storageMutex);
    
    auto it = g_encryptedKeys.find(keyName);
    if (it == g_encryptedKeys.end()) {
        return false;
    }
    
    DATA_BLOB inBlob;
    inBlob.pbData = it->second.data();
    inBlob.cbData = static_cast<DWORD>(it->second.size());
    
    DATA_BLOB outBlob = {};
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        return false;
    }
    
    bool success = false;
    if (outBlob.cbData < bufferSize) {
        memcpy(buffer, outBlob.pbData, outBlob.cbData);
        buffer[outBlob.cbData] = '\0';
        success = true;
    }
    
    SecureZeroMemory(outBlob.pbData, outBlob.cbData);
    LocalFree(outBlob.pbData);
    
    return success;
}

extern "C" void SecureStorage_DeleteApiKey(const char* keyName) {
    if (!keyName) return;
    
    std::lock_guard<std::mutex> lock(g_storageMutex);
    auto it = g_encryptedKeys.find(keyName);
    if (it != g_encryptedKeys.end()) {
        SecureZeroMemory(it->second.data(), it->second.size());
        g_encryptedKeys.erase(it);
    }
}

extern "C" bool SecureStorage_HasKey(const char* keyName) {
    if (!keyName) return false;
    
    std::lock_guard<std::mutex> lock(g_storageMutex);
    return g_encryptedKeys.find(keyName) != g_encryptedKeys.end();
}

extern "C" bool MonacoSettingsDialog_ShowModal(HWND hwndParent, MonacoSettings* settings) {
    if (!settings) {
        return false;
    }
    
    // For now, just validate and return the current settings
    // A real implementation would show a dialog
    if (settings->fontSize < 8) settings->fontSize = 8;
    if (settings->fontSize > 72) settings->fontSize = 72;
    if (settings->lineHeight < 1.0f) settings->lineHeight = 1.0f;
    if (settings->lineHeight > 3.0f) settings->lineHeight = 3.0f;
    
    return true;
}

extern "C" bool MonacoSettingsDialog_LoadFromFile(const char* path, MonacoSettings* settings) {
    if (!path || !settings) {
        return false;
    }
    
    // For now, just set defaults
    strcpy_s(settings->theme, sizeof(settings->theme), "dark");
    settings->fontSize = 14;
    settings->lineHeight = 1.5f;
    settings->backgroundColor = RGB(30, 30, 30);
    settings->foregroundColor = RGB(220, 220, 220);
    settings->wordWrap = true;
    
    return true;
}
