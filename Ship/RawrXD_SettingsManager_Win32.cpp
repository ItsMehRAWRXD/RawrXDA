/*
 * RawrXD_SettingsManager_Win32.cpp
 * Pure Win32 replacement for Qt QSettings
 * Uses: Win32 Registry API
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>

class RawrXDSettingsManager {
private:
    HKEY m_rootKey;
    wchar_t m_appKey[256];
    static const wchar_t REGISTRY_ROOT[];
    
public:
    RawrXDSettingsManager() : m_rootKey(nullptr) {
        wcscpy_s(m_appKey, L"Software\\RawrXD");
        
        LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, m_appKey, 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &m_rootKey, nullptr);
        
        if (result != ERROR_SUCCESS) {
            m_rootKey = nullptr;
            OutputDebugStringW(L"[SettingsManager] Failed to create registry key\n");
        }
    }
    
    ~RawrXDSettingsManager() {
        if (m_rootKey) {
            RegCloseKey(m_rootKey);
        }
    }
    
    bool SetString(const wchar_t* key, const wchar_t* value) {
        if (!m_rootKey) return false;
        
        LONG result = RegSetValueExW(m_rootKey, key, 0, REG_SZ,
            (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
        
        return result == ERROR_SUCCESS;
    }
    
    bool GetString(const wchar_t* key, wchar_t* buffer, size_t bufSize) {
        if (!m_rootKey) return false;
        
        DWORD size = (DWORD)(bufSize * sizeof(wchar_t));
        LONG result = RegQueryValueExW(m_rootKey, key, nullptr, nullptr,
            (LPBYTE)buffer, &size);
        
        return result == ERROR_SUCCESS;
    }
    
    bool SetInt(const wchar_t* key, int value) {
        if (!m_rootKey) return false;
        
        LONG result = RegSetValueExW(m_rootKey, key, 0, REG_DWORD,
            (const BYTE*)&value, sizeof(int));
        
        return result == ERROR_SUCCESS;
    }
    
    int GetInt(const wchar_t* key, int defaultValue = 0) {
        if (!m_rootKey) return defaultValue;
        
        int value = defaultValue;
        DWORD size = sizeof(int);
        LONG result = RegQueryValueExW(m_rootKey, key, nullptr, nullptr,
            (LPBYTE)&value, &size);
        
        return result == ERROR_SUCCESS ? value : defaultValue;
    }
    
    bool SetFloat(const wchar_t* key, float value) {
        return SetInt(key, (int)(value * 10000));
    }
    
    float GetFloat(const wchar_t* key, float defaultValue = 0.0f) {
        return GetInt(key, (int)(defaultValue * 10000)) / 10000.0f;
    }
    
    bool DeleteKey(const wchar_t* key) {
        if (!m_rootKey) return false;
        
        LONG result = RegDeleteValueW(m_rootKey, key);
        return result == ERROR_SUCCESS;
    }
    
    void Clear() {
        if (m_rootKey) {
            RegCloseKey(m_rootKey);
            m_rootKey = nullptr;
            
            RegDeleteTreeW(HKEY_CURRENT_USER, m_appKey);
            
            RegCreateKeyExW(HKEY_CURRENT_USER, m_appKey, 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &m_rootKey, nullptr);
        }
    }
};

// Global instance
static RawrXDSettingsManager* g_settings = nullptr;

extern "C" {
    __declspec(dllexport) void* __stdcall CreateSettingsManager() {
        if (!g_settings) {
            g_settings = new RawrXDSettingsManager();
        }
        return g_settings;
    }
    
    __declspec(dllexport) void __stdcall DestroySettingsManager(void* mgr) {
        if (mgr && mgr == g_settings) {
            delete g_settings;
            g_settings = nullptr;
        }
    }
    
    __declspec(dllexport) bool __stdcall Settings_SetString(void* mgr, const wchar_t* key, const wchar_t* value) {
        RawrXDSettingsManager* m = static_cast<RawrXDSettingsManager*>(mgr);
        return m ? m->SetString(key, value) : false;
    }
    
    __declspec(dllexport) bool __stdcall Settings_GetString(void* mgr, const wchar_t* key, wchar_t* buffer, size_t bufSize) {
        RawrXDSettingsManager* m = static_cast<RawrXDSettingsManager*>(mgr);
        return m ? m->GetString(key, buffer, bufSize) : false;
    }
    
    __declspec(dllexport) bool __stdcall Settings_SetInt(void* mgr, const wchar_t* key, int value) {
        RawrXDSettingsManager* m = static_cast<RawrXDSettingsManager*>(mgr);
        return m ? m->SetInt(key, value) : false;
    }
    
    __declspec(dllexport) int __stdcall Settings_GetInt(void* mgr, const wchar_t* key, int defaultValue) {
        RawrXDSettingsManager* m = static_cast<RawrXDSettingsManager*>(mgr);
        return m ? m->GetInt(key, defaultValue) : defaultValue;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_SettingsManager_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_settings) {
        delete g_settings;
    }
    return TRUE;
}
