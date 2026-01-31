// RawrXD Model Loader - Pure Win32/C++ Implementation
// GGUF model loading and caching

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <map>

// ============================================================================
// MODEL STRUCTURES
// ============================================================================

struct ModelInfo {
    wchar_t name[256];
    wchar_t path[512];
    wchar_t version[64];
    SIZE_T file_size;
    int parameters;  // millions
    DWORD loaded_at;
    int ref_count;
    BOOL loaded;
};

struct ModelStats {
    int total_models;
    int loaded_models;
    SIZE_T total_memory;
    int total_loads;
};

// ============================================================================
// MODEL LOADER
// ============================================================================

class ModelLoader {
private:
    CRITICAL_SECTION m_cs;
    std::vector<ModelInfo> m_models;
    std::map<std::wstring, int> m_nameToIndex;
    int m_totalLoads;
    SIZE_T m_totalMemory;
    
public:
    ModelLoader()
        : m_totalLoads(0),
          m_totalMemory(0) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~ModelLoader() {
        Shutdown();
        DeleteCriticalSection(&m_cs);
    }
    
    void Initialize() {
        EnterCriticalSection(&m_cs);
        m_models.clear();
        m_nameToIndex.clear();
        m_totalLoads = 0;
        m_totalMemory = 0;
        LeaveCriticalSection(&m_cs);
        OutputDebugStringW(L"[ModelLoader] Initialized\n");
    }
    
    void Shutdown() {
        EnterCriticalSection(&m_cs);
        
        for (auto& model : m_models) {
            if (model.loaded) {
                m_totalMemory -= model.file_size;
            }
        }
        
        m_models.clear();
        m_nameToIndex.clear();
        
        LeaveCriticalSection(&m_cs);
    }
    
    // Register a model
    BOOL RegisterModel(const wchar_t* name, const wchar_t* path, 
                      const wchar_t* version, int parametersMM) {
        if (!name || !path) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        // Check if already exists
        if (m_nameToIndex.find(name) != m_nameToIndex.end()) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }
        
        ModelInfo model;
        wcscpy_s(model.name, 256, name);
        wcscpy_s(model.path, 512, path);
        wcscpy_s(model.version, 64, version ? version : L"1.0");
        model.parameters = parametersMM;
        model.loaded = FALSE;
        model.ref_count = 0;
        model.file_size = 0;
        model.loaded_at = 0;
        
        // Try to get file size
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
            model.file_size = ((SIZE_T)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
        }
        
        int index = (int)m_models.size();
        m_models.push_back(model);
        m_nameToIndex[name] = index;
        
        LeaveCriticalSection(&m_cs);
        
        wchar_t buf[256];
        swprintf_s(buf, 256, L"[ModelLoader] Model registered: %s (%d parameters)\n", name, parametersMM);
        OutputDebugStringW(buf);
        
        return TRUE;
    }
    
    // Load model into memory
    BOOL LoadModel(const wchar_t* name) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end()) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }
        
        int index = it->second;
        ModelInfo& model = m_models[index];
        
        if (model.loaded) {
            model.ref_count++;
            LeaveCriticalSection(&m_cs);
            return TRUE;
        }
        
        // Simulate loading
        model.loaded = TRUE;
        model.ref_count = 1;
        model.loaded_at = GetTickCount();
        m_totalLoads++;
        m_totalMemory += model.file_size;
        
        LeaveCriticalSection(&m_cs);
        
        wchar_t buf[256];
        swprintf_s(buf, 256, L"[ModelLoader] Model loaded: %s\n", name);
        OutputDebugStringW(buf);
        
        return TRUE;
    }
    
    // Unload model
    BOOL UnloadModel(const wchar_t* name) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end()) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }
        
        int index = it->second;
        ModelInfo& model = m_models[index];
        
        if (!model.loaded) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }
        
        model.ref_count--;
        if (model.ref_count <= 0) {
            model.loaded = FALSE;
            model.ref_count = 0;
            m_totalMemory -= model.file_size;
        }
        
        LeaveCriticalSection(&m_cs);
        return TRUE;
    }
    
    // Get model info
    ModelInfo GetModel(const wchar_t* name) {
        ModelInfo result;
        ZeroMemory(&result, sizeof(result));
        
        if (!name) return result;
        
        EnterCriticalSection(&m_cs);
        
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end()) {
            result = m_models[it->second];
        }
        
        LeaveCriticalSection(&m_cs);
        return result;
    }
    
    // Check if model is loaded
    BOOL IsModelLoaded(const wchar_t* name) {
        if (!name) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        auto it = m_nameToIndex.find(name);
        BOOL loaded = (it != m_nameToIndex.end()) && m_models[it->second].loaded;
        
        LeaveCriticalSection(&m_cs);
        return loaded;
    }
    
    // Get stats
    ModelStats GetStats() {
        ModelStats stats;
        stats.total_models = 0;
        stats.loaded_models = 0;
        stats.total_memory = 0;
        stats.total_loads = 0;
        
        EnterCriticalSection(&m_cs);
        
        stats.total_models = (int)m_models.size();
        stats.total_loads = m_totalLoads;
        stats.total_memory = m_totalMemory;
        
        for (const auto& model : m_models) {
            if (model.loaded) stats.loaded_models++;
        }
        
        LeaveCriticalSection(&m_cs);
        return stats;
    }
    
    const wchar_t* GetStatus() {
        static wchar_t status[512];
        ModelStats stats = GetStats();
        
        swprintf_s(status, 512,
            L"ModelLoader: %d models (%d loaded), Memory: %lld MB, Loads: %d",
            stats.total_models, stats.loaded_models,
            (long long)stats.total_memory / (1024 * 1024),
            stats.total_loads);
        
        return status;
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) ModelLoader* __stdcall CreateModelLoader(void) {
    return new ModelLoader();
}

__declspec(dllexport) void __stdcall DestroyModelLoader(ModelLoader* loader) {
    if (loader) delete loader;
}

__declspec(dllexport) void __stdcall ModelLoader_Initialize(ModelLoader* loader) {
    if (loader) loader->Initialize();
}

__declspec(dllexport) void __stdcall ModelLoader_Shutdown(ModelLoader* loader) {
    if (loader) loader->Shutdown();
}

__declspec(dllexport) BOOL __stdcall ModelLoader_RegisterModel(ModelLoader* loader,
    const wchar_t* name, const wchar_t* path, const wchar_t* version, int paramsMM) {
    return loader ? loader->RegisterModel(name, path, version, paramsMM) : FALSE;
}

__declspec(dllexport) BOOL __stdcall ModelLoader_LoadModel(ModelLoader* loader,
    const wchar_t* name) {
    return loader ? loader->LoadModel(name) : FALSE;
}

__declspec(dllexport) BOOL __stdcall ModelLoader_UnloadModel(ModelLoader* loader,
    const wchar_t* name) {
    return loader ? loader->UnloadModel(name) : FALSE;
}

__declspec(dllexport) BOOL __stdcall ModelLoader_IsLoaded(ModelLoader* loader,
    const wchar_t* name) {
    return loader ? loader->IsModelLoaded(name) : FALSE;
}

__declspec(dllexport) const wchar_t* __stdcall ModelLoader_GetStatus(ModelLoader* loader) {
    return loader ? loader->GetStatus() : L"Not initialized";
}

} // extern "C"

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_ModelLoader] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_ModelLoader] DLL unloaded\n");
        break;
    }
    return TRUE;
}
