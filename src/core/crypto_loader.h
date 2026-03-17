#ifndef CRYPTO_LOADER_H
#define CRYPTO_LOADER_H

#include <windows.h>
#include <string>

// Function pointer typedefs for RawrXD-Crypto.dll exports
typedef void (*EncryptFunc)(const char* input, char* output, size_t size);
typedef void (*DecryptFunc)(const char* input, char* output, size_t size);
typedef bool (*UACBypassFunc)();

// Dynamic loader class for RawrXD-Crypto.dll
// Loads the DLL on-demand to avoid polluting the main IDE build with FUD/UAC code
class CryptoLoader {
private:
    HMODULE hModule = nullptr;
    EncryptFunc encryptFunc = nullptr;
    DecryptFunc decryptFunc = nullptr;
    UACBypassFunc uacBypassFunc = nullptr;

public:
    CryptoLoader() = default;
    ~CryptoLoader() { Unload(); }

    // Load RawrXD-Crypto.dll from the same directory as the IDE executable
    bool Load() {
        if (hModule) return true;  // Already loaded

        // Get the directory of the current executable
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dllPath = std::string(exePath);
        size_t lastSlash = dllPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            dllPath = dllPath.substr(0, lastSlash + 1) + "RawrXD-Crypto.dll";
        } else {
            dllPath = "RawrXD-Crypto.dll";  // Fallback
        }

        hModule = LoadLibraryA(dllPath.c_str());
        if (!hModule) {
            // Log error: GetLastError()
            return false;
        }

        // Get function pointers
        encryptFunc = (EncryptFunc)GetProcAddress(hModule, "Encrypt");
        decryptFunc = (DecryptFunc)GetProcAddress(hModule, "Decrypt");
        uacBypassFunc = (UACBypassFunc)GetProcAddress(hModule, "UACBypass");

        if (!encryptFunc || !decryptFunc || !uacBypassFunc) {
            // Log error: one or more functions not found
            Unload();
            return false;
        }

        return true;
    }

    // Unload the DLL
    void Unload() {
        if (hModule) {
            FreeLibrary(hModule);
            hModule = nullptr;
            encryptFunc = nullptr;
            decryptFunc = nullptr;
            uacBypassFunc = nullptr;
        }
    }

    // Public interface: call the DLL functions if loaded
    void Encrypt(const char* input, char* output, size_t size) {
        if (encryptFunc) {
            encryptFunc(input, output, size);
        } else {
            // Fallback or error: crypto not available
        }
    }

    void Decrypt(const char* input, char* output, size_t size) {
        if (decryptFunc) {
            decryptFunc(input, output, size);
        } else {
            // Fallback or error
        }
    }

    bool UACBypass() {
        if (uacBypassFunc) {
            return uacBypassFunc();
        } else {
            return false;  // Fallback: bypass not available
        }
    }

    // Check if crypto is loaded
    bool IsLoaded() const {
        return hModule != nullptr;
    }
};

// Global instance for the IDE to use
extern CryptoLoader g_CryptoLoader;

#endif // CRYPTO_LOADER_H