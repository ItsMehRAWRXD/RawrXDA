#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ---------------------------------------------------------------------------
// VAllocBuffer — thin RAII wrapper around VirtualAlloc for large buffers that
// must bypass the debug CRT heap.  Provides the std::vector subset used by
// the KV cache (data/size/resize/assign/clear).
// VirtualAlloc returns zero-filled pages, so 0.0f fill is free.
// ---------------------------------------------------------------------------
template <typename T> struct VAllocBuffer
{
    T* m_ptr = nullptr;
    size_t m_size = 0;

    VAllocBuffer() = default;
    ~VAllocBuffer() { free_(); }
    VAllocBuffer(const VAllocBuffer&) = delete;
    VAllocBuffer& operator=(const VAllocBuffer&) = delete;
    VAllocBuffer(VAllocBuffer&& o) noexcept : m_ptr(o.m_ptr), m_size(o.m_size)
    {
        o.m_ptr = nullptr;
        o.m_size = 0;
    }
    VAllocBuffer& operator=(VAllocBuffer&& o) noexcept
    {
        if (this != &o)
        {
            free_();
            m_ptr = o.m_ptr;
            m_size = o.m_size;
            o.m_ptr = nullptr;
            o.m_size = 0;
        }
        return *this;
    }

    T* data() noexcept { return m_ptr; }
    const T* data() const noexcept { return m_ptr; }
    size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }

    // Allocate n elements, zero-filled.  val is accepted for interface compat
    // but must be zero (VirtualAlloc guarantees zero pages).
    void resize(size_t n, T /*val*/ = T{})
    {
        free_();
        if (n == 0)
            return;
        const size_t bytes = n * sizeof(T);
        m_ptr = static_cast<T*>(::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!m_ptr)
        {
            DWORD err = ::GetLastError();
            printf("[VAllocBuffer] VirtualAlloc FAILED: n=%zu bytes=%zu err=%lu\n", n, bytes, (unsigned long)err);
            throw std::bad_alloc();
        }
        m_size = n;
        // Pages are zero-filled by the OS — no memset needed for 0/0.0f.
    }

    void assign(size_t n, T val)
    {
        free_();
        if (n == 0)
            return;
        const size_t bytes = n * sizeof(T);
        m_ptr = static_cast<T*>(::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!m_ptr)
        {
            DWORD err = ::GetLastError();
            printf("[VAllocBuffer] VirtualAlloc FAILED (assign): n=%zu bytes=%zu err=%lu\n", n, bytes,
                   (unsigned long)err);
            throw std::bad_alloc();
        }
        m_size = n;
        // VirtualAlloc zeros pages.  If caller requests non-zero fill, do it.
        T zero{};
        if (std::memcmp(&val, &zero, sizeof(T)) != 0)
            for (size_t i = 0; i < n; ++i)
                m_ptr[i] = val;
    }

    void clear() noexcept { free_(); }
    void shrink_to_fit() noexcept { /* no-op — VirtualAlloc is always exact */ }

  private:
    void free_() noexcept
    {
        if (m_ptr)
        {
            ::VirtualFree(m_ptr, 0, MEM_RELEASE);
            m_ptr = nullptr;
        }
        m_size = 0;
    }
};

#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Standard Win32/CPU build - Vulkan handles not needed
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif
#endif
// Swarm (span, expected, scheduler types) before rawrxd_model_loader.h (<windows.h>) so clang/MSVC parse stays stable.
// Use path under src/ so TUs with only ${REPO}/src on the include path (e.g. some tests) still resolve this header.
#include "core/moe_down_project_policy.hpp"
#include "core/moe_plan_row_mixture_pack_cache.hpp"
#include "core/swarm_scheduler.hpp"
#include "rawrxd_model_loader.h"

/// Subset of swarm plan rows to pin for one layer (MoE: static early, experts after router logits).
enum class SwarmPinLayerParts : std::uint8_t
{
    Full = 0,         ///< Dense backbone + all selected experts (default).
    StaticOnly = 1,   ///< Only shared / dense slice group (`expertIndex == 0xFFFFFFFF`).
    ExpertsOnly = 2,  ///< Only expert slice groups (uses `activeExpertOrdinals` or plan fallback).
};

class RawrXDTransformer
{
  public:
    struct Config
    {
        int dim;
        int hidden_dim;
        int n_layers;
        int n_heads;
        int n_kv_heads;
        int vocab_size;
        int n_ctx;  // Context length
        int seq_len;
        float rope_theta;
        float rms_norm_eps;
        /// MoE swarm: caps how many experts to pin. `0` = pin every expert group in the plan (safe default).
        /// When router logits run (`ffn_gate_inp`), top‑K uses `min(metadata experts_used, moe_pin_top_k)` if
        /// `moe_pin_top_k > 0`, else metadata / small default.
        int moe_pin_top_k = 0;
        /// When false, MoE FFN uses only the first routed expert (full weight 1.0) for A/B vs weighted mixture.
        bool moe_weighted_mixture = true;
        /// Policy for MoE down-project path (looped vs grouped+cached pack); used when integrating \ref
        /// RawrXD::MoEAccumRef paths. Forward still runs per-expert SwiGLU until that integration lands.
        double moe_down_work_threshold = 1e6;
        double moe_down_min_reuse_for_grouped = 8.0;
        /// Default reuse estimate when no per-layer telemetry (e.g. heatmap); raise after prefetch is hot.
        double moe_down_expected_reuse_estimate = 8.0;
        /// When true and a swarm scheduler provides \ref RawrXD::Swarm::ISwarmScheduler::captureExpertHeatmapSnapshot,
        /// refresh per-layer resident ratio EMA (throttled) for \ref chooseMoEDownProjectPath.
        bool moe_down_reuse_from_heatmap = true;
        /// Capture heatmap every N forward token steps (minimum 1). Higher values reduce lock traffic.
        int moe_down_reuse_snapshot_stride = 8;
        /// EMA smoothing on resident ratio \(r \in [0,1]\) per layer.
        double moe_down_reuse_ema_alpha = 0.125;
        /// Maps EMA to reuse integer: \(R = \max(1, \lfloor \text{ema} \cdot R_{\max} \rfloor)\).
        double moe_down_reuse_r_max = 16.0;
        /// Layers need this many successful heatmap samples (after min-cells gate) before EMA drives policy.
        int moe_down_reuse_min_samples = 8;
        /// In one snapshot, ignore a layer if it has fewer than this many expert plan cells (noise gate).
        int moe_down_reuse_min_expert_cells_per_layer = 8;
        /// Model index for heatmap capture (`0` = first model). Ignored when \p moe_down_reuse_heatmap_all_models is
        /// true.
        int moe_down_reuse_heatmap_model_index = 0;
        /// If true, pass \ref RawrXD::Swarm::kExpertHeatmapAllModels to the capture API.
        bool moe_down_reuse_heatmap_all_models = false;
        /// When true, log when heatmap-driven reuse changes \ref chooseMoEDownProjectPath vs static default reuse
        /// (staging).
        bool moe_down_log_policy_divergence = false;
        /// Max divergence lines per rolling minute (0 = disable emission even if flag is true).
        int moe_down_policy_divergence_logs_per_minute_cap = 32;
        /// Experimental: run grouped down-pack cache probe in MoE mixture path (math still uses per-expert SwiGLU).
        bool moe_down_enable_grouped_integration = false;
        /// When true with grouped integration, use batched down-project on pack-cache hit (\ref
        /// MoEAccumRef::moeDownProjectBatchedExpertsFromPackedF32); shadow mode keeps this false.
        bool moe_down_use_grouped_down_math = false;
        /// LRU capacity for \ref MoEIntegr::MoEMixturePlanPackCache (minimum 1 when integration is enabled).
        int moe_down_grouped_pack_cache_max_entries = 16;
        /// Max total packed float payload bytes for the mixture pack cache (`0` = no byte cap, entry limit only).
        std::uint64_t moe_down_grouped_pack_cache_max_bytes = 512ull * 1024 * 1024;
        /// On cache miss, synchronously pack down weights via \ref RawrXDModelLoader::GetTensor when contiguous.
        bool moe_down_grouped_sync_pack_on_miss = false;
        /// Background thread: enqueue prepack jobs on cache miss (and optional heatmap hints) when integration is on.
        bool moe_down_grouped_async_prepack = false;
        /// Max queued prepack jobs (`tryPush` drops when full — see \ref moePrepackQueueDropped).
        int moe_down_grouped_prepack_queue_depth = 32;
        /// Worker wait timeout when the queue is idle (milliseconds).
        std::uint32_t moe_down_grouped_prepack_thread_poll_ms = 200;
        /// After a heatmap snapshot, enqueue up to N resident expert hints per tick (async prepack only).
        bool moe_down_grouped_prepack_hint_from_heatmap = false;
        int moe_down_grouped_prepack_heatmap_max_hints_per_tick = 16;

        /// Minimal expert prefetch: after router top‑K is known, materialize the expert gate/up/down tensors via
        /// RawrXDModelLoader hotpatch slots to avoid first-use stalls.
        bool moe_expert_prefetch = true;
        /// Cap prefetch fanout even if router returns more experts (0 = no cap).
        int moe_expert_prefetch_max = 8;
    };

    ~RawrXDTransformer();

    bool Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader* loader);
    std::vector<float> Forward(const std::vector<uint32_t>& tokens, int start_pos);

    /** Optional: layer forward progress (e.g. "[STEP] Layer …"). Safe to invoke from worker threads. */
    void SetProgressCallback(std::function<void(const std::string&)> cb) { m_layerProgressCb = std::move(cb); }

    /** Optional: dual-window swarm (onLayerCompute* on inference thread; prefetch I/O on scheduler thread). */
    void SetSwarmScheduler(RawrXD::Swarm::ISwarmScheduler* scheduler);

    /// Re-snapshot `SwarmPlanSliceIndex` from the scheduler after `submitPlan` (or any plan replacement).
    void refreshSwarmPlanSliceIndex();

    /// Chooses looped vs grouped+cached down-project for MoE math matching \ref moe_down_project_policy.hpp.
    /// Uses heatmap-driven reuse when \ref Config::moe_down_reuse_from_heatmap is true and enough samples exist;
    /// otherwise \ref Config::moe_down_expected_reuse_estimate.
    [[nodiscard]] RawrXD::MoEDownProject::Path chooseMoEDownProjectPath(std::uint32_t layer, std::size_t inDim,
                                                                        std::size_t outDim,
                                                                        int numExperts) const noexcept;

    /// Effective reuse scalar passed to \ref RawrXD::MoEDownProject::choosePath for \p layer (telemetry / tests).
    [[nodiscard]] double moeDownProjectExpectedReuse(std::uint32_t layer) const noexcept;

    /// Incremented when a policy divergence log is emitted (\ref Config::moe_down_log_policy_divergence).
    [[nodiscard]] std::uint64_t moePolicyDivergenceLogCount() const noexcept { return m_moePolicyDivergenceLogCount; }

    /// Grouped pack-cache telemetry (\ref Config::moe_down_enable_grouped_integration).
    [[nodiscard]] std::uint64_t moeGroupedPackCacheHits() const noexcept { return m_moeGroupedPackCacheHits; }
    [[nodiscard]] std::uint64_t moeGroupedPackCacheMisses() const noexcept { return m_moeGroupedPackCacheMisses; }
    [[nodiscard]] std::uint64_t moeGroupedFallbacks() const noexcept { return m_moeGroupedFallbacks; }
    [[nodiscard]] std::uint64_t moeGroupedSyncPackInserts() const noexcept { return m_moeGroupedSyncPackInserts; }
    [[nodiscard]] std::uint64_t moeGroupedWeightedApplies() const noexcept { return m_moeGroupedWeightedApplies; }
    [[nodiscard]] std::uint64_t moeGroupedSingleExpertApplies() const noexcept
    {
        return m_moeGroupedSingleExpertApplies;
    }
    [[nodiscard]] std::uint64_t moeGroupedWeightedFallbacks() const noexcept { return m_moeGroupedWeightedFallbacks; }
    [[nodiscard]] std::uint64_t moeGroupedSingleExpertFallbacks() const noexcept
    {
        return m_moeGroupedSingleExpertFallbacks;
    }

    /// Entries removed because a referenced swarm plan row's slice left the working set.
    [[nodiscard]] std::uint64_t moePackEvictedByPlanRow() const noexcept { return m_moePackEvictedByPlanRow; }
    /// Async prepack worker: jobs dropped when the bounded queue is full.
    [[nodiscard]] std::uint64_t moePrepackQueueDropped() const noexcept { return m_moePrepackQueueDropped; }
    /// Jobs skipped because listed plan rows were not all resident at execution time.
    [[nodiscard]] std::uint64_t moePrepackSkippedNotResident() const noexcept { return m_moePrepackSkippedNotResident; }
    /// Successful async prepack inserts into the mixture pack cache.
    [[nodiscard]] std::uint64_t moePrepackInserts() const noexcept { return m_moePrepackInserts; }
    /// Best-effort depth of the prepack queue (for HUD / tuning).
    [[nodiscard]] std::size_t moePrepackQueueDepthApprox() const noexcept;

    /// Mixture pack cache byte footprint (`0` when grouped integration cache is off).
    [[nodiscard]] std::size_t moeMixturePackCacheCurrentPackedBytes() const noexcept;
    /// LRU / byte-cap eviction count from the pack cache.
    [[nodiscard]] std::uint64_t moeMixturePackCacheEvictions() const noexcept;
    /// Entries removed via \ref MoEIntegr::MoEMixturePlanPackCache::invalidateEntriesReferencingPlanRows.
    [[nodiscard]] std::uint64_t moeMixturePackCacheSelectiveRowInvalidations() const noexcept;

    /// O(1) group lookup for the last plan known to the scheduler when SetSwarmScheduler ran (MoE-aware).
    [[nodiscard]] const RawrXD::Swarm::SwarmPlanSliceIndex& swarmPlanSliceIndex() const noexcept
    {
        return m_swarmPlanSliceIndex;
    }

  private:
    Config config;
    VkDevice device;
    RawrXDModelLoader* loader;
    std::function<void(const std::string&)> m_layerProgressCb;
    RawrXD::Swarm::ISwarmScheduler* m_swarmScheduler = nullptr;
    RawrXD::Swarm::SwarmPlanSliceIndex m_swarmPlanSliceIndex{};
    bool m_swarmPinningSuppressed = false;
    std::uint32_t m_swarmPinFailureStreak = 0;

    /// On success, appends plan row indices passed to `pinPlanRows` (for matching `unpinPlanRows`).
    /// When \p appendPinnedRows is false, \p outPinnedPlanRows is cleared first.
    [[nodiscard]] std::expected<void, RawrXD::Swarm::SchedulerError> pinSwarmSlicesForLayer(
        std::uint32_t modelIndex, std::uint32_t layer, const std::uint32_t* activeExpertOrdinals,
        std::size_t activeExpertOrdinalCount, std::vector<std::size_t>& outPinnedPlanRows,
        SwarmPinLayerParts parts = SwarmPinLayerParts::Full, bool appendPinnedRows = false);

    [[nodiscard]] bool swarmLayerHasExpertSlices(std::uint32_t layer) const;
    /// Router matmul (`ffn_gate_inp`) + top‑K among plan experts. Fills softmax weights over **all** experts,
    /// renormalized over the picked set (convex mixture). Returns false if router matmul cannot run (e.g. K not
    /// multiple of 256). On true, `outExpertOrdinals.size() == outMixtureWeights.size()`.
    [[nodiscard]] bool tryPickMoERouterExperts(std::uint32_t layer, const float* ffnNormedHidden,
                                               std::vector<std::uint32_t>& outExpertOrdinals,
                                               std::vector<float>& outMixtureWeights);

    [[nodiscard]] double moeLayerExpectedReuse(std::uint32_t layer) const noexcept;
    void maybeSampleMoEReuseFromHeatmap();
    void tryEmitMoEPathDivergenceLog(std::uint32_t layer, std::size_t inDim, std::size_t outDim, int numExperts,
                                     double dynamicReuse, RawrXD::MoEDownProject::Path chosen,
                                     RawrXD::MoEDownProject::Path staticChoice) const;

    [[nodiscard]] bool tryMoeSyncPackMixtureIntoCache(std::uint32_t layer, const std::string& blkPrefix,
                                                      const std::vector<std::uint32_t>& expertPickOrder, int hdim,
                                                      int dim, const std::string& cacheKey,
                                                      std::span<const std::size_t> planRows);

    struct MoEPrepackJob
    {
        std::uint32_t layer = 0;
        std::string blkPrefix;
        std::vector<std::uint32_t> expertPick;
        int hdim = 0;
        int dim = 0;
        std::string cacheKey;
        std::vector<std::size_t> planRows;
    };

    class MoEPrepackWorker;
    struct MoEPrepackWorkerDeleter
    {
        void operator()(MoEPrepackWorker* p) const noexcept;
    };

    void handleMoePrepackJob_(MoEPrepackJob&& job);
    void tryEnqueueMoePrepack_(MoEPrepackJob&& job);
    void shutdownMoePrepackWorker_();
    void startMoePrepackWorker_();
    void onSwarmPlanRowsEvicted_(std::span<const std::size_t> rows);
    void installSwarmPlanRowEvictionObserver_();

    void prefetchMoEExperts_(const std::string& blkPrefix, const std::vector<std::uint32_t>& expertOrdinals);

    // KV Cache — float buffers use VirtualAlloc to bypass debug CRT heap limits.
    VAllocBuffer<float> kv_cache_k;
    VAllocBuffer<float> kv_cache_v;
    std::vector<int64_t> kv_cache_pos;

    /// Resident ratio EMA per layer (expert rows only); reset on plan generation change.
    mutable std::vector<double> m_moeReuseResidentRatioEma;
    /// Count of heatmap samples that passed the per-layer min-cell gate.
    mutable std::vector<std::uint32_t> m_moeReuseSampleCount;
    mutable std::uint64_t m_moeReuseTrackedPlanGeneration = std::numeric_limits<std::uint64_t>::max();
    mutable std::uint64_t m_moeReuseForwardTick = 0;

    /// Last plan generation we logged a divergence for, per layer (dedupe within one plan).
    mutable std::vector<std::uint64_t> m_moePolicyDivLastPlanGenPerLayer;
    mutable std::uint64_t m_moePolicyDivRateWindowStartMs = 0;
    mutable unsigned m_moePolicyDivLogsInWindow = 0;
    mutable std::uint64_t m_moePolicyDivergenceLogCount = 0;

    std::unique_ptr<RawrXD::MoEIntegr::MoEMixturePlanPackCache> m_moeMixturePlanPackCache;
    /// Last plan generation used to invalidate mixture pack cache (vs \ref m_moeReuseTrackedPlanGeneration for EMA).
    mutable std::uint64_t m_moeMixturePackCachePlanGeneration = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t m_moeGroupedPackCacheHits = 0;
    std::uint64_t m_moeGroupedPackCacheMisses = 0;
    std::uint64_t m_moeGroupedFallbacks = 0;
    std::uint64_t m_moeGroupedSyncPackInserts = 0;
    std::uint64_t m_moeGroupedWeightedApplies = 0;
    std::uint64_t m_moeGroupedSingleExpertApplies = 0;
    std::uint64_t m_moeGroupedWeightedFallbacks = 0;
    std::uint64_t m_moeGroupedSingleExpertFallbacks = 0;

    std::uint64_t m_moePackEvictedByPlanRow = 0;
    std::uint64_t m_moePrepackQueueDropped = 0;
    std::uint64_t m_moePrepackSkippedNotResident = 0;
    std::uint64_t m_moePrepackInserts = 0;

    std::uint64_t m_moeExpertPrefetchAttempts = 0;
    std::uint64_t m_moeExpertPrefetchMaterialized = 0;
    std::uint64_t m_moeExpertPrefetchErrors = 0;

    std::unique_ptr<MoEPrepackWorker, MoEPrepackWorkerDeleter> m_moePrepackWorker;
};
