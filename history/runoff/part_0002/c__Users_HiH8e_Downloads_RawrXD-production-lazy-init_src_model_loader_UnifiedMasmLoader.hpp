#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>
#include <atomic>
#include <mutex>

/**
 * @brief Unified MASM Loader Interface
 *
 * Provides hot-loadable, switchable access to three optimized MASM loaders:
 * 1. Sliding Window (305.34 TPS) - DEFAULT, constant 3MB memory
 * 2. GGUF Memory Map (298.99 TPS) - Zero-copy NT file mapping
 * 3. Beacon Manager (133.43 TPS) - Async lifecycle, multi-model, idle eviction
 *
 * API Features:
 * - Runtime loader selection (no recompile needed)
 * - Hot-switching between loaders
 * - Unified error handling
 * - Performance metrics per loader
 * - Full thread safety
 */

namespace MASM {

// ============================================================================
// PUBLIC API - Exported from MASM object files
// ============================================================================

// Sliding Window API
extern "C" {
    int SlidingWindow_Initialize(void);
    void* SlidingWindow_CreateForModel(const char* path, unsigned long long filesize);
    void SlidingWindow_SetActiveLayer(void* context, unsigned int layer);
    int SlidingWindow_EnsureNoLag(void* context);
    unsigned int SlidingWindow_GetResidentCount(void* context);
    void SlidingWindow_LockLayer(void* context, unsigned int layer);
    void SlidingWindow_UnlockLayer(void* context, unsigned int layer);
    void SlidingWindow_DestroyContext(void* context);

    // Beacon Manager API
    int Beacon_InitializeSystem(void);
    void* Beacon_CreateForModel(unsigned int modelId);
    void Beacon_LoadModelAsync(void* beacon);
    void Beacon_WaitLoadComplete(void* beacon);
    void Beacon_Touch(void* beacon);
    void Beacon_UnlockModel(void* beacon);
    int Beacon_GetStatus(void* beacon);
    void Beacon_ShutdownSystem(void);

    // GGUF Memory Map API
    int GgufMap_CreateMapping(const char* path, void** handle, unsigned long long mapsize);
    void* GgufMap_GetViewPtr(void* handle);
    unsigned long long GgufMap_GetFileSize(void* handle);
    void GgufMap_UnmapSection(void* handle);
    void GgufMap_CloseMapping(void* handle);
    int GgufMap_IsValid(void* handle);
}

// ============================================================================
// LOADER TYPES & ENUMS
// ============================================================================

enum class LoaderType {
    SlidingWindow = 0,  // 305.34 TPS, constant 3MB memory (DEFAULT)
    GgufMemoryMap = 1,  // 298.99 TPS, zero-copy, scalable
    BeaconManager = 2,  // 133.43 TPS, async load/unload, multi-model
};

enum class LoaderStatus {
    Uninitialized = 0,
    Ready = 1,
    Loading = 2,
    Loaded = 3,
    Error = 4,
};

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

struct LoaderMetrics {
    LoaderType type;
    double avgThroughput = 0.0;  // TPS (tokens/sec)
    double avgLatency = 0.0;      // milliseconds
    uint64_t totalTokens = 0;
    uint64_t totalInferences = 0;
    double memoryUsageMB = 0.0;
    uint32_t stallCount = 0;
    
    LoaderMetrics() = default;
    explicit LoaderMetrics(LoaderType t) : type(t) {}
};

// ============================================================================
// UNIFIED LOADER INTERFACE
// ============================================================================

class UnifiedMasmLoader {
public:
    /**
     * @brief Get singleton instance
     */
    static UnifiedMasmLoader& getInstance();

    /**
     * @brief Initialize the active loader
     * @param loaderType Which loader to initialize
     * @return true on success
     */
    bool initialize(LoaderType loaderType = LoaderType::SlidingWindow);

    /**
     * @brief Load a GGUF model using the active loader
     * @param modelPath Absolute path to .gguf file
     * @param async If true, use asynchronous loading (Beacon only)
     * @return Context handle or nullptr on error
     */
    void* loadModel(const std::string& modelPath, bool async = false);

    /**
     * @brief Hot-switch to a different loader
     * @param newLoaderType Target loader type
     * @return true on success; false if switch requires active model unload
     */
    bool switchLoader(LoaderType newLoaderType);

    /**
     * @brief Unload current model
     * @param context Context handle from loadModel()
     */
    void unloadModel(void* context);

    /**
     * @brief Set active layer for inference (Sliding Window, GGUF Map)
     * @param context Context handle
     * @param layerIndex Which layer to make active
     */
    void setActiveLayer(void* context, unsigned int layerIndex);

    /**
     * @brief Check if next layer is resident (streaming without stalls)
     * @param context Context handle
     * @return 0 if resident, 1 if stalled, <0 on error
     */
    int ensureNoLag(void* context);

    /**
     * @brief Lock layer to prevent eviction during inference
     * @param context Context handle
     * @param layerIndex Layer to lock
     */
    void lockLayer(void* context, unsigned int layerIndex);

    /**
     * @brief Unlock layer to allow eviction
     * @param context Context handle
     * @param layerIndex Layer to unlock
     */
    void unlockLayer(void* context, unsigned int layerIndex);

    /**
     * @brief Get resident layer count (Sliding Window only)
     * @param context Context handle
     * @return Number of layers currently in memory
     */
    unsigned int getResidentCount(void* context) const;

    /**
     * @brief Touch model to prevent idle eviction (Beacon only)
     * @param context Beacon context handle
     */
    void touchModel(void* context);

    /**
     * @brief Get current loader type
     */
    LoaderType getCurrentLoaderType() const;

    /**
     * @brief Get loader status
     */
    LoaderStatus getStatus() const;

    /**
     * @brief Get performance metrics for current loader
     */
    const LoaderMetrics& getMetrics() const;

    /**
     * @brief Update metrics (call after inference)
     * @param tokens Tokens generated
     * @param latencyMs Inference latency in milliseconds
     * @param stalled Whether inference stalled waiting for layer
     */
    void updateMetrics(uint64_t tokens, double latencyMs, bool stalled = false);

    /**
     * @brief Get loader description string
     */
    std::string getLoaderDescription(LoaderType type) const;

    /**
     * @brief Shutdown all loaders
     */
    void shutdown();

    // Delete copy operations
    UnifiedMasmLoader(const UnifiedMasmLoader&) = delete;
    UnifiedMasmLoader& operator=(const UnifiedMasmLoader&) = delete;

private:
    UnifiedMasmLoader() = default;
    ~UnifiedMasmLoader();

    std::atomic<LoaderType> m_currentLoader{LoaderType::SlidingWindow};
    std::atomic<LoaderStatus> m_status{LoaderStatus::Uninitialized};
    std::atomic<bool> m_initialized{false};
    
    LoaderMetrics m_slidingWindowMetrics{LoaderType::SlidingWindow};
    LoaderMetrics m_ggufMapMetrics{LoaderType::GgufMemoryMap};
    LoaderMetrics m_beaconMetrics{LoaderType::BeaconManager};
    
    mutable std::mutex m_metricsLock;
    std::atomic<uint64_t> m_activeContextCount{0};
};

// ============================================================================
// CONVENIENCE WRAPPER
// ============================================================================

/**
 * @brief Convenience wrapper for model contexts
 */
class LoadedModel {
public:
    LoadedModel(const std::string& path, LoaderType loaderType = LoaderType::SlidingWindow, bool async = false);
    ~LoadedModel();

    // Prevent copying
    LoadedModel(const LoadedModel&) = delete;
    LoadedModel& operator=(const LoadedModel&) = delete;

    // Allow moving
    LoadedModel(LoadedModel&& other) noexcept;
    LoadedModel& operator=(LoadedModel&& other) noexcept;

    /**
     * @brief Get the context handle
     */
    void* getContext() const { return m_context; }

    /**
     * @brief Check if model is loaded
     */
    bool isLoaded() const { return m_context != nullptr; }

    /**
     * @brief Set active layer for inference
     */
    void setActiveLayer(unsigned int layerIndex) {
        if (m_context) UnifiedMasmLoader::getInstance().setActiveLayer(m_context, layerIndex);
    }

    /**
     * @brief Check for stalls
     */
    int ensureNoLag() const {
        return m_context ? UnifiedMasmLoader::getInstance().ensureNoLag(m_context) : -1;
    }

    /**
     * @brief Get resident layers (Sliding Window)
     */
    unsigned int getResidentCount() const {
        return m_context ? UnifiedMasmLoader::getInstance().getResidentCount(m_context) : 0;
    }

private:
    void* m_context = nullptr;
    LoaderType m_loaderType;
    std::string m_modelPath;
};

} // namespace MASM
