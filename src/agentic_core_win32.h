/*
 * agentic_core_win32.h
 * 
 * Win32 wrapper layer that replaces Qt-based agentic components
 * with DLL-based implementations.
 * 
 * This eliminates the need for void, std::thread, Qt signals/s
 * by wrapping DLL exports in a clean C++ interface.
 */

#pragma once

#include <windows.h>
#include <stdint.h>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// COMPONENT HANDLES - Opaque pointers to DLL-managed objects
// ============================================================================

struct ComponentHandle {
    HMODULE module;
    void* instance;
    
    ComponentHandle() : module(nullptr), instance(nullptr) {}
    ~ComponentHandle() {
        if (module) FreeLibrary(module);
    }
};

// ============================================================================
// EXECUTOR - Replaces agentic_executor.cpp + std::thread
// ============================================================================

class ExecutorEngine {
public:
    static ExecutorEngine& instance() {
        static ExecutorEngine inst;
        return inst;
    }
    
    bool initialize(const wchar_t* basePath) {
        module_ = LoadLibraryW(L"RawrXD_Executor.dll");
        return module_ != nullptr;
    }
    
    // Execute command asynchronously
    bool executeCommand(const std::string& command, 
                       std::function<void(int, const std::string&)> callback) {
        // Delegate to RawrXD_Executor.dll export
        // The DLL handles threading internally (no std::thread needed)
        if (!module_) return false;
        
        typedef bool(__stdcall *PFN_Execute)(const char*, void*);
        PFN_Execute pfnExecute = (PFN_Execute)GetProcAddress(module_, "ExecuteCommand");
        return pfnExecute ? pfnExecute(command.c_str(), nullptr) : false;
    }
    
    void shutdown() {
        if (module_) FreeLibrary(module_);
        module_ = nullptr;
    }
    
private:
    ExecutorEngine() : module_(nullptr) {}
    HMODULE module_;
};

// ============================================================================
// AGENT COORDINATOR - Replaces src/agentic_agent_coordinator.cpp
// ============================================================================

class AgentCoordinator {
public:
    static AgentCoordinator& instance() {
        static AgentCoordinator inst;
        return inst;
    }
    
    bool initialize(const wchar_t* basePath) {
        module_ = LoadLibraryW(L"RawrXD_AgentCoordinator.dll");
        return module_ != nullptr;
    }
    
    // Schedule a task across agent pool
    bool scheduleTask(const std::string& taskId, const std::string& plan) {
        if (!module_) return false;
        typedef bool(__stdcall *PFN_Schedule)(const char*, const char*);
        PFN_Schedule pfnSchedule = (PFN_Schedule)GetProcAddress(module_, "ScheduleTask");
        return pfnSchedule ? pfnSchedule(taskId.c_str(), plan.c_str()) : false;
    }
    
    // Get task status
    std::string getTaskStatus(const std::string& taskId) {
        if (!module_) return "error";
        typedef const char*(__stdcall *PFN_GetStatus)(const char*);
        PFN_GetStatus pfnStatus = (PFN_GetStatus)GetProcAddress(module_, "GetTaskStatus");
        return pfnStatus ? pfnStatus(taskId.c_str()) : "error";
    }
    
    void shutdown() {
        if (module_) FreeLibrary(module_);
        module_ = nullptr;
    }
    
private:
    AgentCoordinator() : module_(nullptr) {}
    HMODULE module_;
};

// ============================================================================
// INFERENCE ENGINE - Replaces src/agentic_ide.cpp inference calls
// ============================================================================

class InferenceEngine {
public:
    static InferenceEngine& instance() {
        static InferenceEngine inst;
        return inst;
    }
    
    bool initialize(const wchar_t* basePath) {
        module_ = LoadLibraryW(L"RawrXD_InferenceEngine.dll");
        if (!module_) return false;
        
        // Also initialize Win32 inference variant
        module_win32_ = LoadLibraryW(L"RawrXD_InferenceEngine_Win32.dll");
        return true;
    }
    
    // Submit inference request (async - returns immediately)
    bool submitInference(const std::string& prompt, uint64_t& requestId) {
        if (!module_) return false;
        typedef bool(__stdcall *PFN_Submit)(const char*, uint64_t*);
        PFN_Submit pfnSubmit = (PFN_Submit)GetProcAddress(module_, "SubmitInference");
        return pfnSubmit ? pfnSubmit(prompt.c_str(), &requestId) : false;
    }
    
    // Get inference result (non-blocking)
    bool getResult(uint64_t requestId, std::string& output) {
        if (!module_) return false;
        typedef bool(__stdcall *PFN_GetResult)(uint64_t, char*, uint32_t);
        PFN_GetResult pfnGetResult = (PFN_GetResult)GetProcAddress(module_, "GetResult");
        
        if (!pfnGetResult) return false;
        
        char buffer[4096] = {0};
        if (pfnGetResult(requestId, buffer, sizeof(buffer))) {
            output = buffer;
            return true;
        }
        return false;
    }
    
    void shutdown() {
        if (module_) FreeLibrary(module_);
        if (module_win32_) FreeLibrary(module_win32_);
        module_ = nullptr;
        module_win32_ = nullptr;
    }
    
private:
    InferenceEngine() : module_(nullptr), module_win32_(nullptr) {}
    HMODULE module_;
    HMODULE module_win32_;
};

// ============================================================================
// CONFIGURATION - Replaces src/agentic_configuration.cpp + // Settings initialization removed
        return inst;
    }
    
    bool initialize(const wchar_t* basePath) {
        module_ = LoadLibraryW(L"RawrXD_Configuration.dll");
        return module_ != nullptr;
    }
    
    // Get string setting from Win32 Registry
    std::string getString(const std::string& key, const std::string& defaultValue) {
        if (!module_) return defaultValue;
        typedef const char*(__stdcall *PFN_GetString)(const char*, const char*);
        PFN_GetString pfnGet = (PFN_GetString)GetProcAddress(module_, "GetString");
        const char* result = pfnGet ? pfnGet(key.c_str(), defaultValue.c_str()) : nullptr;
        return result ? result : defaultValue;
    }
    
    void shutdown() {
        if (module_) FreeLibrary(module_);
        module_ = nullptr;
    }
    
private:
    ConfigurationManager() : module_(nullptr) {}
    HMODULE module_;
};

// ============================================================================
// INITIALIZATION HELPER
// ============================================================================

inline bool InitializeAgenticComponents(const wchar_t* basePath) {
    return ExecutorEngine::instance().initialize(basePath) &&
           AgentCoordinator::instance().initialize(basePath) &&
           InferenceEngine::instance().initialize(basePath) &&
           ConfigurationManager::instance().initialize(basePath);
}

inline void ShutdownAgenticComponents() {
    ExecutorEngine::instance().shutdown();
    AgentCoordinator::instance().shutdown();
    InferenceEngine::instance().shutdown();
    ConfigurationManager::instance().shutdown();
}

} // namespace Agentic
} // namespace RawrXD

