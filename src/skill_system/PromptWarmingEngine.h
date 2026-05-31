// ============================================================================
// PromptWarmingEngine.h — TPS Optimization via Prompt Pre-Processing
// ============================================================================
// When models are too large for real-time TPS, pre-process prompts to:
//   1. Tokenize and cache the prompt prefix (warmup)
//   2. Pre-compute KV cache for static skill context (520-line injection)
//   3. Batch-process multiple prompts for throughput over latency
//   4. Provide "warm" prompt handles that skip re-tokenization
//
// Architecture:
//   - Lightweight tokenization pass (no full inference)
//   - KV cache seeding for repeated skill context
//   - Prompt fingerprinting for cache hits
//   - Async warming pipeline (background thread)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <windows.h>

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// CONSTANTS
// ============================================================================
static constexpr size_t PROMPT_WARM_CACHE_MAX_ENTRIES = 256;
static constexpr size_t PROMPT_WARM_CACHE_MAX_BYTES = 8 * 1024 * 1024;  // 8MB
static constexpr uint64_t PROMPT_WARM_CACHE_TTL_MS = 300000;  // 5 minutes
static constexpr size_t PROMPT_WARM_BATCH_SIZE = 8;
static constexpr uint64_t PROMPT_WARM_HEARTBEAT_MS = 1000;

// ============================================================================
// WARMED PROMPT HANDLE
// ============================================================================
struct WarmedPromptHandle {
    uint64_t id = 0;
    std::string fingerprint;      // Hash of original prompt
    std::string warmedContent;    // Pre-processed content
    size_t tokenCount = 0;
    size_t kvCacheOffset = 0;     // Where in KV cache this starts
    uint64_t warmedAtMs = 0;
    uint64_t lastUsedMs = 0;
    uint32_t useCount = 0;
    bool isValid = false;
};

// ============================================================================
// PROMPT WARMING ENGINE
// ============================================================================
class PromptWarmingEngine {
public:
    static PromptWarmingEngine& Instance();

    // Initialize the warming pipeline
    bool Initialize();
    void Shutdown();

    // Warm a prompt (async — returns immediately, warms in background)
    uint64_t WarmPromptAsync(const std::string& prompt);

    // Warm a prompt (sync — blocks until warmed)
    WarmedPromptHandle WarmPromptSync(const std::string& prompt, uint32_t timeoutMs = 5000);

    // Check if a prompt is already warmed
    bool IsWarmed(const std::string& prompt) const;
    bool IsWarmed(uint64_t handleId) const;

    // Get warmed content (if available)
    std::string GetWarmedContent(uint64_t handleId);
    std::string GetWarmedContent(const std::string& prompt);

    // Pre-seed the cache with skill context (called at startup)
    void PreseedSkillContext(const std::string& skillContext);

    // Batch warm multiple prompts (for bulk operations)
    std::vector<WarmedPromptHandle> WarmPromptBatch(const std::vector<std::string>& prompts);

    // Cache metrics
    size_t GetCacheEntryCount() const;
    size_t GetCacheTotalBytes() const;
    size_t GetCacheHitCount() const;
    size_t GetCacheMissCount() const;
    float GetCacheHitRate() const;

    // Force cache eviction
    void EvictCache();
    void EvictStaleEntries(uint64_t maxAgeMs);

    // TPS estimation
    float EstimateTPS(const std::string& prompt) const;
    float EstimateTPS(uint64_t handleId) const;

private:
    PromptWarmingEngine() = default;
    ~PromptWarmingEngine();

    std::string ComputeFingerprint(const std::string& prompt) const;
    void WarmingLoop();
    void ProcessWarmingQueue();
    void MaintainCache();
    void EvictIfNeeded();

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, WarmedPromptHandle> m_cache;
    std::unordered_map<uint64_t, std::string> m_idToFingerprint;
    std::vector<std::string> m_warmingQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;

    std::atomic<uint64_t> m_nextId{1};
    std::atomic<size_t> m_totalCacheBytes{0};
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    std::atomic<bool> m_running{false};
    std::thread m_warmingThread;

    // Pre-seeded skill context
    std::string m_preseededSkillContext;
    size_t m_preseededTokenCount = 0;

    PromptWarmingEngine(const PromptWarmingEngine&) = delete;
    PromptWarmingEngine& operator=(const PromptWarmingEngine&) = delete;
};

// ============================================================================
// C-API for inference pipeline integration
// ============================================================================
extern "C" {
    __declspec(dllexport) uint64_t __stdcall PromptWarm_WarmAsync(const char* prompt);
    __declspec(dllexport) bool __stdcall PromptWarm_IsReady(uint64_t handle);
    __declspec(dllexport) const char* __stdcall PromptWarm_GetContent(uint64_t handle);
    __declspec(dllexport) void __stdcall PromptWarm_PreseedSkills(const char* skillContext);
    __declspec(dllexport) float __stdcall PromptWarm_EstimateTPS(uint64_t handle);
    __declspec(dllexport) void __stdcall PromptWarm_EvictCache();
}

} // namespace SkillSystem
} // namespace RawrXD
