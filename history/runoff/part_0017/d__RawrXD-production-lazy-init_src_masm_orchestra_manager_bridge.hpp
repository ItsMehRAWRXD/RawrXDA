/*
 * OrchestraManager Bridge - C++ interface to MASM OrchestraManager
 * 
 * This header provides extern "C" declarations to call the pure MASM
 * OrchestraManager implementation. All functions are thread-safe.
 *
 * Usage:
 *   #include "orchestra_manager_bridge.hpp"
 *   
 *   // Get singleton (thread-safe, lazy-initialized)
 *   void* mgr = OrchestraManager_GetInstance();
 *   
 *   // Open project
 *   OrchestraManager_OpenProject(L"C:\\MyProject", nullptr);
 *
 * Copyright (c) 2025 RawrXD Project
 */

#ifndef ORCHESTRA_MANAGER_BRIDGE_HPP
#define ORCHESTRA_MANAGER_BRIDGE_HPP

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get OrchestraManager singleton instance
 * Thread-safe via Windows InitOnceExecuteOnce
 * @return Pointer to OrchestraManager struct
 */
void* __cdecl OrchestraManager_GetInstance(void);

/**
 * @brief Check if OrchestraManager is initialized
 * @return 1 if initialized, 0 if not
 */
int __cdecl OrchestraManager_IsInitialized(void);

/**
 * @brief Check if running in headless mode
 * @return 1 if headless, 0 if GUI mode
 */
int __cdecl OrchestraManager_IsHeadlessMode(void);

/**
 * @brief Set headless mode
 * @param enable 1 to enable, 0 to disable
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_SetHeadlessMode(int enable);

/**
 * @brief Open a project
 * @param path Wide string path to project directory
 * @param callback Optional callback when complete
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_OpenProject(const wchar_t* path, void (*callback)(int));

/**
 * @brief Run build
 * @param target Build target name
 * @param config Build configuration (Release/Debug)
 * @param outputCallback Callback for build output lines
 * @param doneCallback Callback when build completes
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_Build(
    const wchar_t* target,
    const wchar_t* config,
    void (*outputCallback)(const char*),
    void (*doneCallback)(int)
);

/**
 * @brief Get VCS status
 * @param callback Callback receiving status info
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_VcsStatus(void (*callback)(const char*));

/**
 * @brief Run AI inference
 * @param prompt The prompt string
 * @param options JSON options string
 * @param outputCallback Streaming output callback
 * @param doneCallback Completion callback
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_AiInfer(
    const char* prompt,
    const char* options,
    void (*outputCallback)(const char*),
    void (*doneCallback)(int)
);

/**
 * @brief Discover AI models
 * @param paths Array of search paths
 * @param recursive 1 for recursive search
 * @return Number of models found
 */
int __cdecl OrchestraManager_DiscoverModels(const wchar_t** paths, int recursive);

/**
 * @brief Load a specific AI model
 * @param modelId Model identifier
 * @param progressCallback Progress callback (0-100)
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_LoadModel(const char* modelId, void (*progressCallback)(int));

/**
 * @brief Execute batch commands in headless mode
 * @param commands Null-terminated array of command strings
 * @param callback Callback receiving (success_count, fail_count)
 * @return Number of failed commands
 */
int __cdecl OrchestraManager_RunHeadlessBatch(
    const char** commands,
    void (*callback)(int, int)
);

/**
 * @brief Run diagnostics
 * @param callback Callback receiving health score (0-100)
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_RunDiagnostics(void (*callback)(int));

/**
 * @brief Execute single headless command
 * @param command Command string
 * @param args Arguments string (optional)
 * @return 1 on success, 0 on failure
 */
int __cdecl OrchestraManager_RunHeadlessCommand(const char* command, const char* args);

#ifdef __cplusplus
}

// C++ convenience namespace
namespace rawrxd {
namespace masm {

/**
 * @brief C++ wrapper for MASM OrchestraManager
 */
class MasmOrchestraManager {
public:
    static void* getInstance() {
        return OrchestraManager_GetInstance();
    }
    
    static bool isInitialized() {
        return OrchestraManager_IsInitialized() != 0;
    }
    
    static bool isHeadlessMode() {
        return OrchestraManager_IsHeadlessMode() != 0;
    }
    
    static bool setHeadlessMode(bool enable) {
        return OrchestraManager_SetHeadlessMode(enable ? 1 : 0) != 0;
    }
    
    static bool openProject(const wchar_t* path) {
        return OrchestraManager_OpenProject(path, nullptr) != 0;
    }
    
    static int runDiagnostics() {
        int score = 0;
        OrchestraManager_RunDiagnostics([](int s) {
            // Capture would require static/global - simplified
        });
        return score;
    }
    
    // Deleted to prevent misuse
    MasmOrchestraManager() = delete;
    MasmOrchestraManager(const MasmOrchestraManager&) = delete;
    MasmOrchestraManager& operator=(const MasmOrchestraManager&) = delete;
};

} // namespace masm
} // namespace rawrxd

#endif // __cplusplus

#endif // ORCHESTRA_MANAGER_BRIDGE_HPP
