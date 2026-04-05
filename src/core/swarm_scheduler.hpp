// ============================================================================
// swarm_scheduler.hpp — 600B-scale layer / slice orchestration (design contract)
// ============================================================================
// Header-only contract: working-set caps, prefetch queue, eviction policy hooks,
// and orchestration entry points. Does not call RawrXDModelLoader or the transformer.
//
// Inference: RawrXDInference wires SwarmScheduler + transformer layer hooks; I/O thread uses prefetchPinRange only.
// ============================================================================
#pragma once

// Global loader type (defined in rawrxd_model_loader.h); do not nest under RawrXD::Swarm.
class RawrXDModelLoader;

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// C++20/23 compatibility layer
#include "swarm_scheduler_compat.hpp"

namespace RawrXD
{
namespace Swarm
{

// ---------------------------------------------------------------------------
// Errors — stable contract for std::expected
// ---------------------------------------------------------------------------
enum class SchedulerError : std::uint8_t
{
    Ok = 0,
    NotImplemented,  // Stub phase: operation not yet backed
    InvalidArgument,
    WorkingSetFull,
    PrefetchBusy,
    BackendUnavailable,
    PinFailed,
    SliceNotFound,
    OutOfMemory,
    /// `submitPlan` refused: at least one resident slice still has an active compute pin (hold).
    PlanRowsHeld,
};

inline const char* schedulerErrorMessage(SchedulerError e)
{
    switch (e)
    {
        case SchedulerError::Ok:
            return "ok";
        case SchedulerError::NotImplemented:
            return "not_implemented";
        case SchedulerError::InvalidArgument:
            return "invalid_argument";
        case SchedulerError::WorkingSetFull:
            return "working_set_full";
        case SchedulerError::PrefetchBusy:
            return "prefetch_busy";
        case SchedulerError::BackendUnavailable:
            return "backend_unavailable";
        case SchedulerError::PinFailed:
            return "pin_failed";
        case SchedulerError::SliceNotFound:
            return "slice_not_found";
        case SchedulerError::OutOfMemory:
            return "out_of_memory";
        case SchedulerError::PlanRowsHeld:
            return "plan_rows_held";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Configuration — RAM budget, aperture slots, lookahead
// ---------------------------------------------------------------------------
struct SchedulerConfig
{
    /// Total bytes allowed resident across all models (e.g. 4 GiB working set).
    std::size_t maxWorkingSetBytes = 4ull * 1024ull * 1024ull * 1024ull;
    /// Max layers (or slice units) tracked simultaneously (soft cap; bytes win if lower).
    std::uint32_t maxResidentLayers = 256;
    /// Hints for reserved-aperture style loaders: concurrent 1 GiB-class windows.
    std::uint32_t apertureSlotCount = 4;
    /// Single reserved-aperture cap per slot (bytes); aligns with sovereign 1024 MB lock.
    std::size_t reservedApertureBytesPerSlot = 1024ull * 1024ull * 1024ull;
    /// How many layer slices to queue ahead of the current compute step.
    std::uint32_t prefetchAheadLayers = 2;
    /// Optional system RAM ceiling for admission control (0 = ignore).
    std::size_t systemRamLimitBytes = 0;
    /// Background I/O thread: prefetchPump + pruneWorkingSet (uses MapPrefetchWindow only).
    bool enableAsyncPrefetchThread = true;
    /// Sleep / wait timeout between pump iterations (milliseconds).
    std::uint32_t prefetchIoPollMs = 4;
    /// If true, executePlan pins m_plan[0] via MapWindow. Keep false when inference already owns the compute window.
    bool admitFirstSliceOnExecutePlan = false;
    /// Log each successful MapPrefetchWindow pin (can be noisy).
    bool verbosePrefetchLogging = false;
    /// When true, prefetch expert slices only if they appear in the recent pin history (temporal heuristic).
    bool enableExpertSpeculativePrefetch = true;
    /// Max (model, layer, expert) hints retained for speculation (FIFO deduped on re-pin).
    std::uint32_t expertSpeculationHistory = 64;
};

// ---------------------------------------------------------------------------
// Slice identity — one contiguous file region + logical layer span
// ---------------------------------------------------------------------------
struct ModelSliceId
{
    std::uint32_t modelIndex = 0;
    std::uint32_t layerStart = 0;
    std::uint32_t layerEnd = 0;               // exclusive
    std::uint32_t expertIndex = 0xFFFFFFFFu;  // 0xFFFFFFFF = dense / non-MoE
    /// Multiple file spans for the same logical layer (dense GGUF coalescing). MoE uses expertIndex instead.
    std::uint32_t planSpanOrdinal = 0;

    [[nodiscard]] bool valid() const { return layerEnd > layerStart; }
};

[[nodiscard]] inline bool operator==(const ModelSliceId& a, const ModelSliceId& b) noexcept
{
    return a.modelIndex == b.modelIndex && a.layerStart == b.layerStart && a.layerEnd == b.layerEnd &&
           a.expertIndex == b.expertIndex && a.planSpanOrdinal == b.planSpanOrdinal;
}

struct ModelSliceIdHash
{
    [[nodiscard]] std::size_t operator()(const ModelSliceId& id) const noexcept;
};

struct ModelSlice
{
    ModelSliceId id{};
    std::uint64_t fileOffsetBytes = 0;
    std::uint64_t byteSize = 0;
    /// Optional label for metrics / logging (not used by stub).
    std::string debugName;
};

// ---------------------------------------------------------------------------
// MoE / dense: O(1) lookup of plan row indices by (model, layer bucket, expert group)
// ---------------------------------------------------------------------------
/// Groups slices that share the same `modelIndex`, `layerStart` (layer bucket), and `expertIndex`.
/// Built from `submitPlan` order; indices sort by `planSpanOrdinal` then file offset.
struct PlanSliceGroupKey
{
    std::uint32_t modelIndex = 0;
    std::uint32_t layerBucket = 0;
    std::uint32_t expertIndex = 0xFFFFFFFFu;

    [[nodiscard]] bool operator==(const PlanSliceGroupKey& o) const noexcept
    {
        return modelIndex == o.modelIndex && layerBucket == o.layerBucket && expertIndex == o.expertIndex;
    }
};

struct PlanSliceGroupKeyHash
{
    [[nodiscard]] std::size_t operator()(const PlanSliceGroupKey& k) const noexcept;
};

class SwarmPlanSliceIndex
{
  public:
    SwarmPlanSliceIndex() = default;

    void clear() noexcept { m_indicesByGroup.clear(); }

    /// Rebuild from a submitted plan (call after every `submitPlan`).
    void rebuildFrom(const std::vector<ModelSlice>& plan);

    /// Indices into \p plan for all spans in the group; empty if unknown.
    [[nodiscard]] std::span<const std::size_t> indicesFor(std::uint32_t modelIndex, std::uint32_t layerBucket,
                                                          std::uint32_t expertIndex) const;

    [[nodiscard]] std::size_t groupCount() const noexcept { return m_indicesByGroup.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_indicesByGroup.empty(); }

  private:
    std::unordered_map<PlanSliceGroupKey, std::vector<std::size_t>, PlanSliceGroupKeyHash> m_indicesByGroup;
};

// ---------------------------------------------------------------------------
// Resident tracking — one row in the working set
// ---------------------------------------------------------------------------
struct ResidentSlice
{
    ModelSliceId id{};
    std::uint64_t residentBytes = 0;
    std::uint64_t fileOffsetBytes = 0;
    std::uint64_t lastTouchSequence = 0;
    bool computeFinished = false;
    /// Active forward-pass pins (`pinPlanRows` / `pinPlanRowsBlocking`); must be 0 for eviction.
    std::uint32_t holdCount = 0;
};

// ---------------------------------------------------------------------------
// Eviction — policy selector + strategy interface
// ---------------------------------------------------------------------------
enum class EvictionPolicyKind
{
    LRU,
    FinishedFirst,
    /// Evict slices whose layer index is farthest behind current compute cursor.
    FarthestBehindCursor,
};

class IEvictionPolicy
{
  public:
    virtual ~IEvictionPolicy() = default;
    /// Return a victim to evict, or nullopt if none (caller may refuse admission).
    [[nodiscard]] virtual std::optional<ModelSliceId> chooseVictim(const std::vector<ResidentSlice>& residents,
                                                                   std::uint32_t currentLayerCursor,
                                                                   std::size_t bytesToFreeHint) = 0;
};

// ---------------------------------------------------------------------------
// Prefetch queue — ordered hints for background / async map-ahead
// ---------------------------------------------------------------------------
struct PrefetchItem
{
    ModelSlice slice{};
    std::uint32_t priority = 0;  // higher = sooner
};

class IPrefetchQueue
{
  public:
    virtual ~IPrefetchQueue() = default;
    virtual void clear() = 0;
    virtual void enqueue(PrefetchItem item) = 0;
    [[nodiscard]] virtual std::optional<PrefetchItem> popNext() = 0;
    [[nodiscard]] virtual std::size_t pendingCount() const = 0;
    [[nodiscard]] virtual bool containsSliceId(const ModelSliceId& id) const
    {
        (void)id;
        return false;
    }
};

// ---------------------------------------------------------------------------
// Memory backend — compute slot (MapWindow) + optional prefetch slot (MapPrefetchWindow)
// ---------------------------------------------------------------------------
class ISwarmMemoryBackend
{
  public:
    virtual ~ISwarmMemoryBackend() = default;
    /// Pin or map a byte range for active compute; release when slice evicted.
    [[nodiscard]] virtual std::expected<void, SchedulerError> pinRange(std::uint32_t modelIndex, std::uint64_t offset,
                                                                       std::uint64_t size) = 0;
    virtual void unpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size) = 0;

    /// Warm a second mapping without unmapping the compute window (loader dual-window). Default: NotImplemented.
    [[nodiscard]] virtual std::expected<void, SchedulerError> prefetchPinRange(std::uint32_t modelIndex,
                                                                               std::uint64_t offset,
                                                                               std::uint64_t size);
    virtual void prefetchUnpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size);
    virtual void resetPrefetchPins();

    /// True if the compute MapWindow already fully covers this byte range (prefetch map would be redundant).
    [[nodiscard]] virtual bool sliceFullyCoveredByComputeMapping(std::uint32_t modelIndex, std::uint64_t offset,
                                                                 std::uint64_t size) const
    {
        (void)modelIndex;
        (void)offset;
        (void)size;
        return false;
    }
};

// ---------------------------------------------------------------------------
// Working set — capacity accounting (stub container)
// ---------------------------------------------------------------------------
class WorkingSet
{
  public:
    void configure(std::size_t maxBytes, std::uint32_t maxLayers)
    {
        m_maxBytes = maxBytes;
        m_maxLayers = maxLayers;
    }

    [[nodiscard]] std::size_t capacityBytes() const { return m_maxBytes; }
    [[nodiscard]] std::uint32_t capacityLayers() const { return m_maxLayers; }
    [[nodiscard]] std::size_t currentBytes() const { return m_currentBytes; }
    [[nodiscard]] const std::vector<ResidentSlice>& residents() const { return m_residents; }

    [[nodiscard]] std::expected<void, SchedulerError> tryAdmit(std::uint64_t sliceBytes)
    {
        if (sliceBytes == 0)
            return std::unexpected(SchedulerError::InvalidArgument);
        if (m_currentBytes + sliceBytes > m_maxBytes)
            return std::unexpected(SchedulerError::WorkingSetFull);
        if (m_residents.size() >= m_maxLayers)
            return std::unexpected(SchedulerError::WorkingSetFull);
        return {};
    }

    void recordAdmit(ResidentSlice row)
    {
        m_currentBytes += row.residentBytes;
        m_residents.push_back(std::move(row));
    }

    void recordEvict(const ModelSliceId& id)
    {
        for (auto it = m_residents.begin(); it != m_residents.end();)
        {
            if (it->id.modelIndex == id.modelIndex && it->id.layerStart == id.layerStart &&
                it->id.layerEnd == id.layerEnd && it->id.expertIndex == id.expertIndex &&
                it->id.planSpanOrdinal == id.planSpanOrdinal)
            {
                m_currentBytes -= it->residentBytes;
                it = m_residents.erase(it);
            }
            else
                ++it;
        }
    }

    void clear()
    {
        m_residents.clear();
        m_currentBytes = 0;
    }

    /// Non-const view for scheduler bookkeeping (touch / finish flags).
    [[nodiscard]] std::vector<ResidentSlice>& residentsMut() { return m_residents; }

  private:
    std::size_t m_maxBytes = 0;
    std::uint32_t m_maxLayers = 0;
    std::size_t m_currentBytes = 0;
    std::vector<ResidentSlice> m_residents;
};

// ---------------------------------------------------------------------------
// Lightweight counters (relaxed atomics; for tuning / overlap validation)
// ---------------------------------------------------------------------------
struct SwarmRuntimeStats
{
    std::uint64_t prefetchPinSuccess = 0;
    std::uint64_t prefetchSkippedDuplicate = 0;
    std::uint64_t prefetchSkippedComputeCovers = 0;
    std::uint64_t prefetchEnqueueSkippedDuplicate = 0;
    std::uint64_t evictions = 0;
    /// `pinPlanRows` / `pinPlanRow` saw a slice not yet in the working set (JIT / cold path).
    std::uint64_t jitPinNonResident = 0;
    /// Background prefetch items enqueued for temporally hinted experts.
    std::uint64_t speculativeExpertPrefetchEnqueued = 0;
    /// Admission needed eviction but every evictable finished slice was held (in-use).
    std::uint64_t evictionRejectedInUse = 0;
    /// Admission needed eviction but no victim was available (held, unfinished, or empty policy result).
    std::uint64_t evictStarvation = 0;
    /// `pinPlanRowsBlocking` calls (success or fail).
    std::uint64_t pinBlockAttempts = 0;
    /// Blocking pins that exceeded `timeoutMs`.
    std::uint64_t pinBlockTimeouts = 0;
    /// Sum of wall times for successful blocking pins (milliseconds).
    std::uint64_t pinBlockLatencyMsTotal = 0;
    /// Resident rows with `holdCount > 0` (snapshot under scheduler lock).
    std::uint32_t inUseSliceCount = 0;
};

/// `captureExpertHeatmapSnapshot`: include all models (ignore `modelIndex` filter).
inline constexpr std::uint32_t kExpertHeatmapAllModels = 0xFFFFFFFFu;

/// Low-overhead sampling policy for IDE heatmaps (Win32 / telemetry).
struct ExpertHeatmapCaptureParams
{
    /// When not `kExpertHeatmapAllModels`, only plan rows for this model index.
    std::uint32_t modelIndex = 0;
    /// Iterate plan rows `0, stride, 2*stride, ...` (minimum enforced: 1).
    std::uint32_t planRowStride = 1;
    /// Max cells in the snapshot (0 = unlimited; still capped internally at 1M rows).
    std::uint32_t maxCells = 0;
    /// If true, skip dense / non-MoE rows (`expertIndex == 0xFFFFFFFF`).
    bool expertsOnly = false;
};

/// One submitted plan row joined with residency / prefetch visibility (under one scheduler lock).
struct ExpertHeatmapCell
{
    std::size_t planRowIndex = 0;
    std::uint32_t modelIndex = 0;
    std::uint32_t layerStart = 0;
    std::uint32_t layerEnd = 0;
    std::uint32_t expertIndex = 0xFFFFFFFFu;
    std::uint32_t planSpanOrdinal = 0;
    bool resident = false;
    bool prefetchInFlight = false;
    std::uint32_t holdCount = 0;
    std::uint64_t residentBytes = 0;
    /// Monotonic scheduler touch sequence when resident (0 if not resident).
    std::uint64_t lastTouchSequence = 0;
    /// Intentionally left empty in `captureExpertHeatmapSnapshot` (avoids `std::string` copies under `m_schedMutex`).
    /// Off hot paths, the IDE may fill via `tryCopySubmittedPlanRow(planRowIndex, slice)` if names are needed.
    std::string debugName;
};

/// Point-in-time view for IDE grids (layer × expert) and overlap with `planGeneration`.
struct ExpertHeatmapSnapshot
{
    std::uint64_t snapshotId = 0;
    std::uint64_t planGeneration = 0;
    /// `steady_clock` ms since epoch (for relative UI timing).
    std::uint64_t sampleTsMs = 0;
    SwarmRuntimeStats statsAtSample{};
    std::vector<ExpertHeatmapCell> cells;
};

// ---------------------------------------------------------------------------
// Scheduler — orchestrates Execute / prefetch / eviction (stub)
// ---------------------------------------------------------------------------
class ISwarmScheduler
{
  public:
    virtual ~ISwarmScheduler() = default;

    [[nodiscard]] virtual std::expected<void, SchedulerError> configure(const SchedulerConfig& cfg) = 0;
    /// Plan for one forward segment: ordered slices (e.g. per-layer for 600B streaming).
    [[nodiscard]] virtual std::expected<void, SchedulerError> submitPlan(std::vector<ModelSlice> plan) = 0;
    /// Run admission + prefetch hints for the submitted plan (stub: NotImplemented).
    [[nodiscard]] virtual std::expected<void, SchedulerError> executePlan() = 0;
    /// Synchronize with transformer step begin (for LRU touch + prefetch pump).
    [[nodiscard]] virtual std::expected<void, SchedulerError> onLayerComputeStarted(std::uint32_t modelIndex,
                                                                                    std::uint32_t layerIndex) = 0;
    [[nodiscard]] virtual std::expected<void, SchedulerError> onLayerComputeFinished(std::uint32_t modelIndex,
                                                                                     std::uint32_t layerIndex) = 0;
    [[nodiscard]] virtual std::expected<void, SchedulerError> prefetchPump() = 0;
    [[nodiscard]] virtual std::expected<void, SchedulerError> pruneWorkingSet() = 0;

    [[nodiscard]] virtual const WorkingSet& workingSet() const = 0;
    [[nodiscard]] virtual std::uint64_t sequence() const = 0;

    /// Reset prefetch dedupe for a new token step (prefill index t or each decode Forward). Default: no-op.
    virtual void onForwardTokenStepBegin() {}

    /// Copy of the slice group index for the current plan (empty until `submitPlan`). Safe for executor cache.
    [[nodiscard]] virtual SwarmPlanSliceIndex planSliceIndexSnapshot() const { return {}; }

    /// Row count of the submitted plan (0 until `submitPlan`).
    [[nodiscard]] virtual std::size_t submittedPlanRowCount() const { return 0; }

    /// Pin one plan row by index (use with `SwarmPlanSliceIndex::indicesFor`). Holds scheduler lock.
    [[nodiscard]] virtual std::expected<void, SchedulerError> pinPlanRow(std::size_t planRowIndex)
    {
        (void)planRowIndex;
        return std::unexpected(SchedulerError::NotImplemented);
    }

    /// Pin several rows in one lock acquisition (gating: static + top‑K experts).
    /// Duplicate row indices in one call are merged: one hold per distinct plan row (symmetric with `unpinPlanRows`).
    [[nodiscard]] virtual std::expected<void, SchedulerError> pinPlanRows(std::span<const std::size_t> planRowIndices)
    {
        (void)planRowIndices;
        return std::unexpected(SchedulerError::NotImplemented);
    }

    /// Wait until the whole batch is admitted and compute-pinned, or until timeout.
    /// Default: `timeoutMs == 0` delegates to `pinPlanRows`; otherwise returns false unless overridden.
    [[nodiscard]] virtual bool pinPlanRowsBlocking(std::span<const std::size_t> planRowIndices, std::uint32_t timeoutMs)
    {
        if (timeoutMs != 0)
            return false;
        return pinPlanRows(planRowIndices).has_value();
    }

    /// Release compute pins acquired via `pinPlanRows` / `pinPlanRowsBlocking` for these plan rows.
    /// Duplicate row indices in one call decrement at most once per distinct plan row (symmetric with `pinPlanRows`).
    virtual void unpinPlanRows(std::span<const std::size_t> planRowIndices) { (void)planRowIndices; }

    /// Monotonic plan generation; increments on each successful `submitPlan` (staleness guard for cached indices).
    [[nodiscard]] virtual std::uint64_t planGeneration() const { return 0; }

    /// Invoked (under scheduler mutex) when resident slices are evicted; \p planRowIndices are rows in `m_plan`.
    virtual void setPlanRowEvictionObserver(std::function<void(std::span<const std::size_t> planRowIndices)> observer)
    {
        (void)observer;
    }

    /// True when every listed plan row's slice is currently resident in the working set.
    [[nodiscard]] virtual bool arePlanRowsResident(std::span<const std::size_t> planRowIndices) const
    {
        (void)planRowIndices;
        return false;
    }

    /// Copy one submitted plan row under lock (telemetry / IDE). Returns false if out of range.
    [[nodiscard]] virtual bool tryCopySubmittedPlanRow(std::size_t planRowIndex, ModelSlice& out) const
    {
        (void)planRowIndex;
        (void)out;
        return false;
    }

    /// Build layer×expert-style residency view (single `m_schedMutex` pass). Stub returns false.
    [[nodiscard]] virtual bool captureExpertHeatmapSnapshot(const ExpertHeatmapCaptureParams& params,
                                                            ExpertHeatmapSnapshot& out)
    {
        (void)params;
        out = {};
        return false;
    }
};

class SwarmScheduler final : public ISwarmScheduler
{
  public:
    SwarmScheduler();
    ~SwarmScheduler() override;

    SwarmScheduler(std::shared_ptr<ISwarmMemoryBackend> backend, std::unique_ptr<IPrefetchQueue> prefetchQueue,
                   std::unique_ptr<IEvictionPolicy> eviction);

    [[nodiscard]] std::expected<void, SchedulerError> configure(const SchedulerConfig& cfg) override;
    [[nodiscard]] std::expected<void, SchedulerError> submitPlan(std::vector<ModelSlice> plan) override;
    [[nodiscard]] std::expected<void, SchedulerError> executePlan() override;
    [[nodiscard]] std::expected<void, SchedulerError> onLayerComputeStarted(std::uint32_t modelIndex,
                                                                            std::uint32_t layerIndex) override;
    [[nodiscard]] std::expected<void, SchedulerError> onLayerComputeFinished(std::uint32_t modelIndex,
                                                                             std::uint32_t layerIndex) override;
    [[nodiscard]] std::expected<void, SchedulerError> prefetchPump() override;
    [[nodiscard]] std::expected<void, SchedulerError> pruneWorkingSet() override;

    void onForwardTokenStepBegin() override;

    [[nodiscard]] SwarmPlanSliceIndex planSliceIndexSnapshot() const override;

    [[nodiscard]] std::size_t submittedPlanRowCount() const override;
    [[nodiscard]] std::expected<void, SchedulerError> pinPlanRow(std::size_t planRowIndex) override;
    [[nodiscard]] std::expected<void, SchedulerError> pinPlanRows(std::span<const std::size_t> planRowIndices) override;
    [[nodiscard]] bool pinPlanRowsBlocking(std::span<const std::size_t> planRowIndices,
                                           std::uint32_t timeoutMs) override;
    void unpinPlanRows(std::span<const std::size_t> planRowIndices) override;
    [[nodiscard]] std::uint64_t planGeneration() const override;

    void setPlanRowEvictionObserver(std::function<void(std::span<const std::size_t> planRowIndices)> observer) override;

    [[nodiscard]] bool arePlanRowsResident(std::span<const std::size_t> planRowIndices) const override;

    [[nodiscard]] bool tryCopySubmittedPlanRow(std::size_t planRowIndex, ModelSlice& out) const override;

    [[nodiscard]] bool captureExpertHeatmapSnapshot(const ExpertHeatmapCaptureParams& params,
                                                    ExpertHeatmapSnapshot& out) override;

    [[nodiscard]] const WorkingSet& workingSet() const override { return m_workingSet; }
    [[nodiscard]] std::uint64_t sequence() const override { return m_sequence; }

    [[nodiscard]] const SchedulerConfig& config() const { return m_cfg; }
    [[nodiscard]] SwarmRuntimeStats runtimeStats() const;

  private:
    [[nodiscard]] std::expected<void, SchedulerError> admitOrEvictAndPin_(const ModelSlice& slice);
    void touchResidentForLayer_(std::uint32_t modelIndex, std::uint32_t layerIndex);
    void markFinishedForLayer_(std::uint32_t modelIndex, std::uint32_t layerIndex);
    void enqueuePrefetchAroundHead_(std::uint32_t modelIndex, std::uint32_t layerIndex);
    [[nodiscard]] bool isLikelySpeculativeExpert_(std::uint32_t modelIndex, std::uint32_t layerBucket,
                                                  std::uint32_t expertOrdinal) const;
    void recordExpertSpeculationHint_(const ModelSliceId& id);
    void enqueueUrgentPrefetchUnlocked_(const ModelSlice& slice);
    void unpinAllResidents_();
    void rollbackNewAdmits_(const std::vector<ModelSliceId>& ids);
    /// Admit + hold + expert hints for \p uniqueRows (already validated, each index at most once, stable order).
    [[nodiscard]] std::expected<void, SchedulerError> pinPlanRowsBatchUniqueLocked_(
        const std::vector<std::size_t>& uniqueRows);
    [[nodiscard]] std::expected<void, SchedulerError> pruneWorkingSetUnlocked_();
    void prefetchIoThreadMain_();
    void startPrefetchIoThread_();
    void stopPrefetchIoThread_();
    void notifyPrefetchIoThread_();

    [[nodiscard]] std::vector<std::size_t> planRowIndicesForSliceIdUnlocked_(const ModelSliceId& id) const;
    void notifyPlanRowEvictionsForSliceIdUnlocked_(const ModelSliceId& id);

    SchedulerConfig m_cfg{};
    WorkingSet m_workingSet{};
    std::vector<ModelSlice> m_plan;
    SwarmPlanSliceIndex m_planSliceIndex{};
    std::uint64_t m_sequence = 0;
    std::uint32_t m_currentComputeLayer = 0;
    std::uint32_t m_currentComputeModel = 0;

    std::shared_ptr<ISwarmMemoryBackend> m_backend;
    std::unique_ptr<IPrefetchQueue> m_prefetch;
    std::unique_ptr<IEvictionPolicy> m_eviction;

    std::thread m_prefetchIoThread;
    std::atomic<bool> m_prefetchIoStop{false};
    std::mutex m_prefetchIoMutex;
    std::condition_variable m_prefetchIoCv;
    /// Protects m_plan, m_workingSet, m_prefetch queue, and layer cursor fields vs prefetch I/O thread.
    mutable std::mutex m_schedMutex;
    std::condition_variable m_schedCv;

    std::uint64_t m_planGeneration = 0;
    std::uint64_t m_expertHeatmapSnapshotSeq = 0;

    std::function<void(std::span<const std::size_t>)> m_planRowEvictionObserver{};

    std::unordered_set<ModelSliceId, ModelSliceIdHash> m_prefetchCompletedIds;
    std::unordered_set<ModelSliceId, ModelSliceIdHash> m_prefetchInFlightIds;

    struct ExpertPinRecord
    {
        std::uint32_t modelIndex = 0;
        std::uint32_t layerStart = 0;
        std::uint32_t expertOrdinal = 0;
    };
    std::deque<ExpertPinRecord> m_recentExpertPins;

    /// Popped before the main prefetch queue (JIT / high-priority warm).
    std::deque<PrefetchItem> m_urgentPrefetch;

    std::atomic<std::uint64_t> m_statPrefetchPinSuccess{0};
    std::atomic<std::uint64_t> m_statPrefetchSkippedDuplicate{0};
    std::atomic<std::uint64_t> m_statPrefetchSkippedComputeCovers{0};
    std::atomic<std::uint64_t> m_statPrefetchEnqueueSkippedDuplicate{0};
    std::atomic<std::uint64_t> m_statEvictions{0};
    std::atomic<std::uint64_t> m_statJitPinNonResident{0};
    std::atomic<std::uint64_t> m_statSpeculativeExpertPrefetchEnqueued{0};
    std::atomic<std::uint64_t> m_statEvictionRejectedInUse{0};
    std::atomic<std::uint64_t> m_statEvictStarvation{0};
    std::atomic<std::uint64_t> m_statPinBlockAttempts{0};
    std::atomic<std::uint64_t> m_statPinBlockTimeouts{0};
    std::atomic<std::uint64_t> m_statPinBlockLatencyMsTotal{0};
};

// ---------------------------------------------------------------------------
// Default eviction: finished slices with lowest layerStart, else global LRU touch.
// ---------------------------------------------------------------------------
class LinearTransformerEvictionPolicy final : public IEvictionPolicy
{
  public:
    [[nodiscard]] std::optional<ModelSliceId> chooseVictim(const std::vector<ResidentSlice>& residents,
                                                           std::uint32_t currentLayerCursor,
                                                           std::size_t bytesToFreeHint) override;
};

// ---------------------------------------------------------------------------
// Min-heap on slice.id.layerStart (soonest layer first).
// ---------------------------------------------------------------------------
class LayerProximityPrefetchQueue final : public IPrefetchQueue
{
  public:
    void clear() override;
    void enqueue(PrefetchItem item) override;
    [[nodiscard]] std::optional<PrefetchItem> popNext() override;
    [[nodiscard]] std::size_t pendingCount() const override;
    [[nodiscard]] bool containsSliceId(const ModelSliceId& id) const override;

  private:
    std::vector<PrefetchItem> m_items;
};

// ---------------------------------------------------------------------------
// Loader adapter: compute = MapWindow, prefetch = MapPrefetchWindow (independent views).
// ---------------------------------------------------------------------------
class RawrXDModelLoaderMemoryBackend final : public ISwarmMemoryBackend
{
  public:
    explicit RawrXDModelLoaderMemoryBackend(RawrXDModelLoader* loader);

    [[nodiscard]] std::expected<void, SchedulerError> pinRange(std::uint32_t modelIndex, std::uint64_t offset,
                                                               std::uint64_t size) override;
    void unpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size) override;
    [[nodiscard]] std::expected<void, SchedulerError> prefetchPinRange(std::uint32_t modelIndex, std::uint64_t offset,
                                                                       std::uint64_t size) override;
    void prefetchUnpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size) override;
    void resetPrefetchPins() override;
    [[nodiscard]] bool sliceFullyCoveredByComputeMapping(std::uint32_t modelIndex, std::uint64_t offset,
                                                         std::uint64_t size) const override;

  private:
    RawrXDModelLoader* m_loader = nullptr;
    bool m_pinned = false;
    std::uint32_t m_pinnedModel = 0;
    std::uint64_t m_pinnedOffset = 0;
    std::uint64_t m_pinnedSize = 0;
    bool m_prefetchPinned = false;
    std::uint32_t m_prefetchModel = 0;
    std::uint64_t m_prefetchOffset = 0;
    std::uint64_t m_prefetchSize = 0;
};

// ---------------------------------------------------------------------------
// Factory hooks
// ---------------------------------------------------------------------------
[[nodiscard]] std::unique_ptr<SwarmScheduler> makeStubSwarmScheduler();
[[nodiscard]] std::unique_ptr<SwarmScheduler> makeSwarmSchedulerWithLoader(RawrXDModelLoader* loader);

/// Compact JSON for Win32IDE / HTTP bridges (`Content-Type: application/json`). Escapes `debugName` minimally.
[[nodiscard]] std::string expertHeatmapSnapshotToJson(const ExpertHeatmapSnapshot& snap);

}  // namespace Swarm
}  // namespace RawrXD
