// ============================================================================
// gpu_warming_engine.h — GPU Anti-Lag / Hot-Model Loading
// ============================================================================
// Keeps the GPU "primed" so first inference doesn't pay initialization cost.
// Symmetric to PromptWarmingEngine (CPU) but for GPU compute state.
//
// Responsibilities:
//   1. Pre-allocate VRAM buffers (DEFAULT heap) — avoid allocation lag
//   2. Pre-compile compute PSOs — avoid shader compilation lag
//   3. Pre-upload constant tensors (skill context, weights) — avoid upload lag
//   4. Keep GPU command queue warm — avoid queue creation lag
//   5. Monitor GPU temperature and throttle if needed
//   6. Provide "warm" GPU handles that skip re-initialization
//
// Architecture:
//   - Singleton (like PromptWarmingEngine)
//   - Background thread for async warming
//   - Lock-free SPSC queue for warming requests
//   - Integration with GPUBackendBridge for DX12/Vulkan
//   - Telemetry integration for TPS measurement
//
// Build: Compiled into RawrXD-Win32IDE and RawrEngine
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <unordered_map>

namespace RawrXD {
namespace GPU {

// Forward declarations
class GPUBackendBridge;

// ============================================================================
// GPU WARM CACHE ENTRY
// ============================================================================
struct GPUWarmCacheEntry {
    uint64_t    id = 0;
    std::string fingerprint;       // Hash of model path + config
    uint64_t    vramHandle = 0;    // GPU VRAM allocation handle
    uint64_t    psoHandle = 0;      // Pipeline state object handle
    uint64_t    warmedAtMs = 0;
    uint64_t    lastUsedMs = 0;
    uint32_t    useCount = 0;
    bool        isValid = false;
    bool        isHot = false;       // Currently resident in GPU memory
};

// ============================================================================
// GPU WARMING CONFIG
// ============================================================================
struct GPUWarmingConfig {
    uint64_t    maxVRAMBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;  // 4GB default
    uint32_t    maxHotModels = 4;                                   // Keep 4 models hot
    uint32_t    warmupBatchSize = 1;                                // Models per batch
    uint64_t    vramReserveBytes = 512ULL * 1024ULL * 1024ULL;     // 512MB reserve
    bool        enableTemperatureThrottle = true;
    uint32_t    temperatureThrottleC = 85;                          // Throttle at 85C
    bool        enableAsyncWarming = true;
    uint32_t    asyncThreadCount = 1;
};

// ============================================================================
// GPU WARMING ENGINE
// ============================================================================
class GPUWarmingEngine {
public:
    static GPUWarmingEngine& Instance();

    // Initialize with GPU backend bridge
    bool Initialize(GPUBackendBridge* bridge, const GPUWarmingConfig& config = {});
    void Shutdown();

    // Warm a model (async — returns immediately, warms in background)
    uint64_t WarmModelAsync(const std::string& modelPath, const std::string& configJson = "");

    // Warm a model (sync — blocks until warmed)
    GPUWarmCacheEntry WarmModelSync(const std::string& modelPath, const std::string& configJson = "", uint32_t timeoutMs = 30000);

    // Check if a model is already warmed
    bool IsWarmed(const std::string& modelPath) const;
    bool IsWarmed(uint64_t handleId) const;

    // Get warmed entry (if available)
    GPUWarmCacheEntry GetWarmedEntry(uint64_t handleId);
    GPUWarmCacheEntry GetWarmedEntry(const std::string& modelPath);

    // Pre-seed with skill context (keeps skill tensors hot)
    void PreseedSkillContext(const std::string& skillContext);

    // Batch warm multiple models
    std::vector<GPUWarmCacheEntry> WarmModelBatch(const std::vector<std::string>& modelPaths);

    // Cache metrics
    size_t GetCacheEntryCount() const;
    size_t GetCacheTotalBytes() const;
    size_t GetCacheHitCount() const;
    size_t GetCacheMissCount() const;
    float GetCacheHitRate() const;

    // VRAM metrics
    uint64_t GetTotalAllocatedVRAM() const;
    uint64_t GetFreeVRAM() const;

    // Force cache eviction
    void EvictCache();
    void EvictStaleEntries(uint64_t maxAgeMs);

    // Temperature check
    float GetCurrentTemperatureC() const;
    bool IsThrottled() const;

    // TPS estimation for warmed vs cold models
    float EstimateTPS(const std::string& modelPath) const;

private:
    GPUWarmingEngine() = default;
    ~GPUWarmingEngine();

    // Non-copyable
    GPUWarmingEngine(const GPUWarmingEngine&) = delete;
    GPUWarmingEngine& operator=(const GPUWarmingEngine&) = delete;

    // Background warming loop
    void WarmingLoop();

    // Internal warming implementation
    bool WarmModelInternal(const std::string& modelPath, const std::string& configJson, GPUWarmCacheEntry& outEntry);

    // VRAM allocation helpers
    bool AllocateVRAMForModel(const std::string& modelPath, uint64_t& outHandle, uint64_t& outSize);
    void FreeVRAMAllocation(uint64_t handle);

    // PSO compilation
    bool CompilePSOForModel(const std::string& modelPath, uint64_t& outHandle);
    void FreePSO(uint64_t handle);

    // Cache management
    void EvictIfNeeded();
    std::string ComputeFingerprint(const std::string& modelPath, const std::string& configJson) const;

    // Temperature monitoring
    void UpdateTemperature();

    // Members
    GPUBackendBridge* m_bridge = nullptr;
    GPUWarmingConfig m_config;

    // Cache
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, GPUWarmCacheEntry> m_cache;
    std::unordered_map<uint64_t, std::string> m_idToFingerprint;

    // Async queue
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::vector<std::pair<std::string, std::string>> m_warmingQueue;  // (modelPath, configJson)

    // Threading
    std::atomic<bool> m_running{false};
    std::thread m_warmingThread;

    // Statistics
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    std::atomic<uint64_t> m_totalAllocatedVRAM{0};
    std::atomic<uint64_t> m_totalPSOCount{0};
    std::atomic<uint64_t> m_nextId{1};

    // Temperature
    std::atomic<float> m_currentTemperatureC{0.0f};
    std::atomic<bool> m_isThrottled{false};

    // Pre-seeded skill context
    std::string m_preseededSkillContext;
    uint64_t m_preseededVRAMHandle = 0;
};

} // namespace GPU
} // namespace RawrXD
