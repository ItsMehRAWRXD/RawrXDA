/*
 * RawrXD_ResourceManager_Win32.cpp
 * Pure Win32 replacement for Qt resource system
 * Uses: Win32 Resource API, memory mapping
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef RAWRXD_WIN32_STATIC_BUILD
#define RAWRXD_SHIP_EXPORT __declspec(dllexport)
#else
#define RAWRXD_SHIP_EXPORT
#endif
#include <map>
#include <string>
#include <vector>
#include <memory>

struct ResourceData {
    std::vector<char> data;
    std::string type;
    size_t size;
};

class RawrXDResourceManager {
private:
    std::map<std::string, ResourceData> m_resources;
    HMODULE m_hModule;
    mutable CRITICAL_SECTION m_criticalSection;
    
public:
    RawrXDResourceManager(HMODULE hModule = nullptr) : m_hModule(hModule) {
        InitializeCriticalSection(&m_criticalSection);
    }
    
    ~RawrXDResourceManager() {
        DeleteCriticalSection(&m_criticalSection);
    }
    
    void LoadResourceFromFile(const wchar_t* filePath, const char* resourceId) {
        HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }
        
        DWORD fileSize = GetFileSize(hFile, nullptr);
        std::vector<char> buffer(fileSize);
        
        DWORD bytesRead;
        if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr)) {
            CloseHandle(hFile);
            return;
        }
        
        CloseHandle(hFile);
        
        EnterCriticalSection(&m_criticalSection);
        
        ResourceData data;
        data.data = buffer;
        data.size = fileSize;
        data.type = "file";
        
        std::string key(resourceId);
        m_resources[key] = data;
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void RegisterResource(const char* resourceId, const char* data, size_t size) {
        EnterCriticalSection(&m_criticalSection);
        
        ResourceData res;
        res.data.assign(data, data + size);
        res.size = size;
        res.type = "memory";
        
        m_resources[resourceId] = res;
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    bool GetResource(const char* resourceId, char** outData, size_t* outSize) {
        EnterCriticalSection(&m_criticalSection);
        
        auto it = m_resources.find(resourceId);
        if (it == m_resources.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        *outData = it->second.data.data();
        *outSize = it->second.size;
        
        LeaveCriticalSection(&m_criticalSection);
        return true;
    }
    
    bool ResourceExists(const char* resourceId) const {
        EnterCriticalSection(&m_criticalSection);
        bool exists = m_resources.find(resourceId) != m_resources.end();
        LeaveCriticalSection(&m_criticalSection);
        return exists;
    }
    
    void ClearResources() {
        EnterCriticalSection(&m_criticalSection);
        m_resources.clear();
        LeaveCriticalSection(&m_criticalSection);
    }
    
    size_t GetResourceCount() const {
        EnterCriticalSection(&m_criticalSection);
        size_t count = m_resources.size();
        LeaveCriticalSection(&m_criticalSection);
        return count;
    }
};

static RawrXDResourceManager* g_resourceManager = nullptr;

extern "C" {
    RAWRXD_SHIP_EXPORT void* __stdcall CreateResourceManager(void* hModule) {
        if (!g_resourceManager) {
            g_resourceManager = new RawrXDResourceManager((HMODULE)hModule);
        }
        return g_resourceManager;
    }
    
    RAWRXD_SHIP_EXPORT void __stdcall DestroyResourceManager(void* mgr) {
        if (mgr && mgr == g_resourceManager) {
            delete g_resourceManager;
            g_resourceManager = nullptr;
        }
    }
    
    RAWRXD_SHIP_EXPORT void __stdcall ResourceManager_LoadFile(void* mgr, const wchar_t* filePath, const char* resourceId) {
        RawrXDResourceManager* m = static_cast<RawrXDResourceManager*>(mgr);
        if (m) m->LoadResourceFromFile(filePath, resourceId);
    }
    
    RAWRXD_SHIP_EXPORT void __stdcall ResourceManager_RegisterResource(void* mgr, const char* resourceId, const char* data, size_t size) {
        RawrXDResourceManager* m = static_cast<RawrXDResourceManager*>(mgr);
        if (m) m->RegisterResource(resourceId, data, size);
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall ResourceManager_GetResource(void* mgr, const char* resourceId, char** outData, size_t* outSize) {
        RawrXDResourceManager* m = static_cast<RawrXDResourceManager*>(mgr);
        return m ? m->GetResource(resourceId, outData, outSize) : false;
    }
    
    RAWRXD_SHIP_EXPORT bool __stdcall ResourceManager_ResourceExists(void* mgr, const char* resourceId) {
        RawrXDResourceManager* m = static_cast<RawrXDResourceManager*>(mgr);
        return m ? m->ResourceExists(resourceId) : false;
    }
    
    RAWRXD_SHIP_EXPORT size_t __stdcall ResourceManager_GetResourceCount(void* mgr) {
        RawrXDResourceManager* m = static_cast<RawrXDResourceManager*>(mgr);
        return m ? m->GetResourceCount() : 0;
    }
}

#ifndef RAWRXD_WIN32_STATIC_BUILD
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_ResourceManager_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_resourceManager) {
        delete g_resourceManager;
        g_resourceManager = nullptr;
    }
    return TRUE;
}
#endif
