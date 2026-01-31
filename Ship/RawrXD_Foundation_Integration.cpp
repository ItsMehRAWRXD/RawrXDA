/*
 * RawrXD_Foundation_Integration.cpp
 * Master Orchestrator for All 31 Components
 * Dependency Resolution + Component Lifecycle Management
 * Zero Qt, Pure Win32, Production-Ready
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <shlwapi.h>

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")

// ═════════════════════════════════════════════════════════════════════════════
// COMPONENT MANIFEST - All 31 Components Registered
// ═════════════════════════════════════════════════════════════════════════════

struct ComponentManifest {
    const wchar_t* id;
    const wchar_t* name;
    const wchar_t* dllName;
    const wchar_t* version;
    const wchar_t** dependencies;
    size_t depCount;
    bool critical;
};

// Dependency lists
static const wchar_t* no_deps[] = {nullptr};
static const wchar_t* deps_core[] = {nullptr};
static const wchar_t* deps_memory[] = {L"Core", nullptr};
static const wchar_t* deps_error[] = {L"Core", nullptr};
static const wchar_t* deps_scheduler[] = {L"Core", L"Memory", nullptr};
static const wchar_t* deps_model[] = {L"Core", L"Memory", nullptr};
static const wchar_t* deps_monitor[] = {L"Core", nullptr};
static const wchar_t* deps_pool[] = {L"Core", L"Scheduler", L"Memory", nullptr};
static const wchar_t* deps_coord[] = {L"Core", L"Scheduler", nullptr};
static const wchar_t* deps_engine[] = {L"Core", L"Scheduler", nullptr};
static const wchar_t* deps_advanced[] = {L"Core", L"Engine", nullptr};
static const wchar_t* deps_resource[] = {L"Core", nullptr};
static const wchar_t* deps_terminal[] = {L"Core", nullptr};
static const wchar_t* deps_file[] = {L"Core", nullptr};
static const wchar_t* deps_editor[] = {L"Core", L"File", nullptr};
static const wchar_t* deps_window[] = {L"Core", L"Resource", nullptr};
static const wchar_t* deps_settings[] = {L"Core", L"Configuration", nullptr};
static const wchar_t* deps_search[] = {L"Core", nullptr};
static const wchar_t* deps_lsp[] = {L"Core", L"ModelRouter", nullptr};
static const wchar_t* deps_aicomp[] = {L"Core", L"Engine", nullptr};
static const wchar_t* deps_syntax[] = {L"Core", L"Editor", nullptr};
static const wchar_t* deps_browse[] = {L"Core", L"File", nullptr};
static const wchar_t* deps_router[] = {L"Core", L"Engine", nullptr};
static const wchar_t* deps_plan[] = {L"Core", L"Coordinator", nullptr};
static const wchar_t* deps_termgr[] = {L"Core", nullptr};

static const ComponentManifest COMPONENT_REGISTRY[] = {
    // Generation 1 (6 components - DLLs only, excluding IDE.exe and CLI.exe)
    {L"InferenceEngine", L"Inference Engine", L"RawrXD_InferenceEngine.dll", L"1.0.0", deps_engine, 1, true},
    {L"AgenticEngine", L"Agentic Engine", L"RawrXD_AgenticEngine.dll", L"1.0.0", deps_coord, 1, true},
    {L"Configuration", L"Configuration", L"RawrXD_Configuration.dll", L"1.0.0", deps_core, 1, false},
    {L"AgenticController", L"Controller", L"RawrXD_AgenticController.dll", L"1.0.0", deps_coord, 1, true},
    {L"CopilotBridge", L"Copilot Bridge", L"RawrXD_CopilotBridge.dll", L"1.0.0", deps_engine, 1, false},
    {L"Executor", L"Executor", L"RawrXD_Executor.dll", L"1.0.0", deps_terminal, 1, true},
    
    // Generation 2 (8 components)
    {L"AgentCoordinator", L"Agent Coordinator", L"RawrXD_AgentCoordinator.dll", L"2.0.0", deps_coord, 1, true},
    {L"ErrorHandler", L"Error Handler", L"RawrXD_ErrorHandler.dll", L"2.0.0", deps_error, 1, true},
    {L"Core", L"Core Services", L"RawrXD_Core.dll", L"2.0.0", deps_core, 0, true},
    {L"AdvancedCodingAgent", L"Advanced Agent", L"RawrXD_AdvancedCodingAgent.dll", L"2.0.0", deps_advanced, 2, true},
    {L"AgentPool", L"Agent Pool", L"RawrXD_AgentPool.dll", L"2.0.0", deps_pool, 3, true},
    {L"MemoryManager", L"Memory Manager", L"RawrXD_MemoryManager.dll", L"2.0.0", deps_memory, 1, true},
    {L"ModelLoader", L"Model Loader", L"RawrXD_ModelLoader.dll", L"2.0.0", deps_model, 2, true},
    {L"TaskScheduler", L"Task Scheduler", L"RawrXD_TaskScheduler.dll", L"2.0.0", deps_scheduler, 2, true},
    
    // Generation 3a (8 components)
    {L"SystemMonitor", L"System Monitor", L"RawrXD_SystemMonitor.dll", L"3.0.0", deps_monitor, 1, false},
    {L"InferenceEngine_Win32", L"Inference Win32", L"RawrXD_InferenceEngine_Win32.dll", L"3.0.0", deps_engine, 1, true},
    {L"MainWindow_Win32", L"Main Window", L"RawrXD_MainWindow_Win32.dll", L"3.0.0", deps_window, 2, true},
    {L"SettingsManager_Win32", L"Settings Manager", L"RawrXD_SettingsManager_Win32.dll", L"3.0.0", deps_core, 1, false},
    {L"TerminalManager_Win32", L"Terminal Manager", L"RawrXD_TerminalManager_Win32.dll", L"3.0.0", deps_terminal, 1, true},
    {L"FileManager_Win32", L"File Manager", L"RawrXD_FileManager_Win32.dll", L"3.0.0", deps_file, 1, true},
    {L"TextEditor_Win32", L"Text Editor", L"RawrXD_TextEditor_Win32.dll", L"3.0.0", deps_editor, 2, true},
    {L"ResourceManager_Win32", L"Resource Manager", L"RawrXD_ResourceManager_Win32.dll", L"3.0.0", deps_resource, 1, true},
    
    // Generation 3b (9 additional components)
    {L"Settings", L"Settings Module", L"RawrXD_Settings.dll", L"3.0.0", deps_settings, 2, false},
    {L"Search", L"Search Engine", L"RawrXD_Search.dll", L"3.0.0", deps_search, 1, false},
    {L"LSPClient", L"LSP Client", L"RawrXD_LSPClient.dll", L"3.0.0", deps_lsp, 2, false},
    {L"AICompletion", L"AI Completion", L"RawrXD_AICompletion.dll", L"3.0.0", deps_aicomp, 2, false},
    {L"SyntaxHL", L"Syntax Highlighter", L"RawrXD_SyntaxHL.dll", L"3.0.0", deps_syntax, 2, false},
    {L"FileBrowser", L"File Browser", L"RawrXD_FileBrowser.dll", L"3.0.0", deps_browse, 2, false},
    {L"ModelRouter", L"Model Router", L"RawrXD_ModelRouter.dll", L"3.0.0", deps_router, 2, false},
    {L"PlanOrchestrator", L"Plan Orchestrator", L"RawrXD_PlanOrchestrator.dll", L"3.0.0", deps_plan, 2, false},
    {L"TerminalMgr", L"Terminal Manager Alt", L"RawrXD_TerminalMgr.dll", L"3.0.0", deps_termgr, 1, false},
};

constexpr size_t COMPONENT_COUNT = sizeof(COMPONENT_REGISTRY) / sizeof(COMPONENT_REGISTRY[0]);

// ═════════════════════════════════════════════════════════════════════════════
// COMPONENT STATE TRACKING
// ═════════════════════════════════════════════════════════════════════════════

enum class ComponentStatus {
    Uninitialized,
    Loading,
    Ready,
    Error,
    ShuttingDown
};

struct ComponentState {
    HMODULE hModule = nullptr;
    ComponentStatus status = ComponentStatus::Uninitialized;
    wchar_t errorMessage[256] = {};
    ULONGLONG loadTimeMs = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
// FOUNDATION ORCHESTRATOR
// ═════════════════════════════════════════════════════════════════════════════

class RawrXDFoundation {
private:
    std::map<std::wstring, ComponentState> components_;
    mutable CRITICAL_SECTION mutex_;
    ComponentStatus systemStatus_;
    wchar_t basePath_[MAX_PATH];
    
    struct {
        ULONGLONG totalLoadTimeMs;
        size_t successfulComponents;
        size_t failedComponents;
    } stats_;

public:
    static RawrXDFoundation& Instance() {
        static RawrXDFoundation instance;
        return instance;
    }

    RawrXDFoundation() : systemStatus_(ComponentStatus::Uninitialized), stats_{} {
        InitializeCriticalSection(&mutex_);
        wmemset(basePath_, 0, MAX_PATH);
    }

    ~RawrXDFoundation() {
        Shutdown();
        DeleteCriticalSection(&mutex_);
    }

    bool Initialize(const wchar_t* basePath = L"D:\\RawrXD\\Ship") {
        EnterCriticalSection(&mutex_);
        
        if (systemStatus_ != ComponentStatus::Uninitialized) {
            LeaveCriticalSection(&mutex_);
            return systemStatus_ == ComponentStatus::Ready;
        }
        
        systemStatus_ = ComponentStatus::Loading;
        wcscpy_s(basePath_, MAX_PATH, basePath);
        
        ULONGLONG startTime = GetTickCount64();
        
        // Resolve dependency order
        std::vector<std::wstring> loadOrder;
        if (!ResolveDependencies(loadOrder)) {
            systemStatus_ = ComponentStatus::Error;
            LeaveCriticalSection(&mutex_);
            return false;
        }
        
        // Load components
        for (const auto& id : loadOrder) {
            LoadComponent(id);  // Don't abort on failure - try to load all
        }
        
        // Check if critical components loaded successfully
        bool hasCriticalFailures = false;
        for (const auto& [id, state] : components_) {
            if (state.status == ComponentStatus::Error) {
                const ComponentManifest* manifest = FindManifest(id);
                if (manifest && manifest->critical) {
                    hasCriticalFailures = true;
                    break;
                }
            }
        }
        
        if (hasCriticalFailures) {
            ShutdownAllLocked();
            systemStatus_ = ComponentStatus::Error;
            LeaveCriticalSection(&mutex_);
            return false;
        }
        
        stats_.totalLoadTimeMs = GetTickCount64() - startTime;
        systemStatus_ = ComponentStatus::Ready;
        
        LeaveCriticalSection(&mutex_);
        return true;
    }

    void Shutdown() {
        EnterCriticalSection(&mutex_);
        ShutdownAllLocked();
        LeaveCriticalSection(&mutex_);
    }

    bool IsReady() const {
        EnterCriticalSection(&mutex_);
        bool ready = systemStatus_ == ComponentStatus::Ready;
        LeaveCriticalSection(&mutex_);
        return ready;
    }

    void PrintStatus() const {
        EnterCriticalSection(&mutex_);
        
        OutputDebugStringW(L"\n╔════════════════════════════════════════════════════════╗\n");
        OutputDebugStringW(L"║ RawrXD Foundation - Component Status Report             ║\n");
        OutputDebugStringW(L"╚════════════════════════════════════════════════════════╝\n");
        
        wchar_t buffer[512];
        swprintf_s(buffer, L"System Status: %s\n", StatusToString(systemStatus_));
        OutputDebugStringW(buffer);
        
        swprintf_s(buffer, L"Total Components: %zu/%zu\n", 
            components_.size(), COMPONENT_COUNT);
        OutputDebugStringW(buffer);
        
        swprintf_s(buffer, L"Successful: %zu | Failed: %zu\n",
            stats_.successfulComponents, stats_.failedComponents);
        OutputDebugStringW(buffer);
        
        swprintf_s(buffer, L"Total Load Time: %llums\n", stats_.totalLoadTimeMs);
        OutputDebugStringW(buffer);
        
        OutputDebugStringW(L"\nComponent Details:\n");
        for (const auto& [id, state] : components_) {
            swprintf_s(buffer, L"  %ws: %s (Load: %llums)\n",
                id.c_str(), StatusToString(state.status), state.loadTimeMs);
            OutputDebugStringW(buffer);
            
            if (state.status == ComponentStatus::Error && wcslen(state.errorMessage) > 0) {
                swprintf_s(buffer, L"    Error: %ws\n", state.errorMessage);
                OutputDebugStringW(buffer);
            }
        }
        
        OutputDebugStringW(L"\n╚════════════════════════════════════════════════════════╝\n");
        
        LeaveCriticalSection(&mutex_);
    }

private:
    bool ResolveDependencies(std::vector<std::wstring>& result) {
        std::set<std::wstring> visited;
        std::set<std::wstring> visiting;
        
        std::function<bool(const std::wstring&)> visit = 
            [&](const std::wstring& id) -> bool {
            
            if (visited.find(id) != visited.end()) {
                return true;
            }
            if (visiting.find(id) != visiting.end()) {
                return false; // Circular dependency
            }
            
            visiting.insert(id);
            
            const ComponentManifest* manifest = FindManifest(id);
            if (manifest) {
                for (size_t i = 0; i < manifest->depCount; ++i) {
                    std::wstring depId(manifest->dependencies[i]);
                    if (!visit(depId)) {
                        return false;
                    }
                }
            }
            
            visiting.erase(id);
            visited.insert(id);
            result.push_back(id);
            return true;
        };
        
        // Start from all roots
        for (const auto& manifest : COMPONENT_REGISTRY) {
            std::wstring id(manifest.id);
            if (!visit(id)) {
                return false; // Circular dependency detected
            }
        }
        
        return true;
    }

    bool LoadComponent(const std::wstring& id) {
        const ComponentManifest* manifest = FindManifest(id);
        if (!manifest) {
            return false;
        }
        
        ComponentState state;
        ULONGLONG startTime = GetTickCount64();
        
        // Build full DLL path
        wchar_t dllPath[MAX_PATH];
        wcscpy_s(dllPath, MAX_PATH, basePath_);
        wcscat_s(dllPath, MAX_PATH, L"\\");
        wcscat_s(dllPath, MAX_PATH, manifest->dllName);
        
        // Load DLL
        state.hModule = LoadLibraryW(dllPath);
        if (!state.hModule) {
            state.status = ComponentStatus::Error;
            swprintf_s(state.errorMessage, 256, L"LoadLibrary failed: %s", dllPath);
            components_[id] = state;
            stats_.failedComponents++;
            return false;
        }
        
        state.status = ComponentStatus::Ready;
        state.loadTimeMs = GetTickCount64() - startTime;
        components_[id] = state;
        stats_.successfulComponents++;
        
        return true;
    }

    void ShutdownAllLocked() {
        if (systemStatus_ == ComponentStatus::ShuttingDown || 
            systemStatus_ == ComponentStatus::Uninitialized) {
            return;
        }
        
        systemStatus_ = ComponentStatus::ShuttingDown;
        
        // Unload DLLs in reverse order
        for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
            if (it->second.hModule) {
                FreeLibrary(it->second.hModule);
                it->second.hModule = nullptr;
                it->second.status = ComponentStatus::Uninitialized;
            }
        }
        
        systemStatus_ = ComponentStatus::Uninitialized;
    }

    const ComponentManifest* FindManifest(const std::wstring& id) const {
        for (const auto& manifest : COMPONENT_REGISTRY) {
            if (id == manifest.id) {
                return &manifest;
            }
        }
        return nullptr;
    }

    static const wchar_t* StatusToString(ComponentStatus status) {
        switch (status) {
            case ComponentStatus::Uninitialized: return L"Uninitialized";
            case ComponentStatus::Loading: return L"Loading";
            case ComponentStatus::Ready: return L"Ready";
            case ComponentStatus::Error: return L"Error";
            case ComponentStatus::ShuttingDown: return L"ShuttingDown";
        }
        return L"Unknown";
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// EXPORTED FUNCTIONS
// ═════════════════════════════════════════════════════════════════════════════

extern "C" {
    __declspec(dllexport) void* __stdcall CreateFoundation() {
        return &RawrXDFoundation::Instance();
    }

    __declspec(dllexport) bool __stdcall Foundation_Initialize(void* foundation, const wchar_t* basePath) {
        RawrXDFoundation* f = static_cast<RawrXDFoundation*>(foundation);
        return f ? f->Initialize(basePath) : false;
    }

    __declspec(dllexport) void __stdcall Foundation_Shutdown(void* foundation) {
        RawrXDFoundation* f = static_cast<RawrXDFoundation*>(foundation);
        if (f) f->Shutdown();
    }

    __declspec(dllexport) bool __stdcall Foundation_IsReady(void* foundation) {
        RawrXDFoundation* f = static_cast<RawrXDFoundation*>(foundation);
        return f ? f->IsReady() : false;
    }

    __declspec(dllexport) void __stdcall Foundation_PrintStatus(void* foundation) {
        RawrXDFoundation* f = static_cast<RawrXDFoundation*>(foundation);
        if (f) f->PrintStatus();
    }

    __declspec(dllexport) size_t __stdcall Foundation_GetComponentCount() {
        return COMPONENT_COUNT;
    }
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_Foundation_Integration loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        RawrXDFoundation::Instance().Shutdown();
    }
    return TRUE;
}
