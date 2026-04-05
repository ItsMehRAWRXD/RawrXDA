// ============================================================================
// swarm_scheduler.cpp — Swarm working-set pump (eviction, prefetch, loader VMM)
// ============================================================================

#include "swarm_scheduler.hpp"

#include "../rawrxd_model_loader.h"

#ifdef Logging
#undef Logging
#endif
#include "../logging/Logger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <limits>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

namespace RawrXD
{
namespace Swarm
{

std::size_t ModelSliceIdHash::operator()(const ModelSliceId& id) const noexcept
{
    std::size_t h = static_cast<std::size_t>(id.modelIndex);
    h ^= static_cast<std::size_t>(id.layerStart) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(id.layerEnd) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(id.expertIndex) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(id.planSpanOrdinal) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

std::size_t PlanSliceGroupKeyHash::operator()(const PlanSliceGroupKey& k) const noexcept
{
    std::size_t h = static_cast<std::size_t>(k.modelIndex);
    h ^= static_cast<std::size_t>(k.layerBucket) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(k.expertIndex) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

void SwarmPlanSliceIndex::rebuildFrom(const std::vector<ModelSlice>& plan)
{
    m_indicesByGroup.clear();
    m_indicesByGroup.reserve(plan.size());

    for (std::size_t i = 0; i < plan.size(); ++i)
    {
        const ModelSlice& s = plan[i];
        if (!s.id.valid() || s.byteSize == 0)
            continue;

        const PlanSliceGroupKey key{s.id.modelIndex, s.id.layerStart, s.id.expertIndex};
        m_indicesByGroup[key].push_back(i);
    }

    for (auto& kv : m_indicesByGroup)
    {
        std::vector<std::size_t>& ix = kv.second;
        std::sort(ix.begin(), ix.end(),
                  [&plan](std::size_t a, std::size_t b)
                  {
                      const ModelSliceId& ia = plan[a].id;
                      const ModelSliceId& ib = plan[b].id;
                      if (ia.planSpanOrdinal != ib.planSpanOrdinal)
                          return ia.planSpanOrdinal < ib.planSpanOrdinal;
                      return plan[a].fileOffsetBytes < plan[b].fileOffsetBytes;
                  });
    }
}

std::span<const std::size_t> SwarmPlanSliceIndex::indicesFor(std::uint32_t modelIndex, std::uint32_t layerBucket,
                                                             std::uint32_t expertIndex) const
{
    const PlanSliceGroupKey key{modelIndex, layerBucket, expertIndex};
    const auto it = m_indicesByGroup.find(key);
    if (it == m_indicesByGroup.end())
        return {};
    const std::vector<std::size_t>& v = it->second;
    return std::span<const std::size_t>(v.data(), v.size());
}

std::expected<void, SchedulerError> ISwarmMemoryBackend::prefetchPinRange(std::uint32_t modelIndex,
                                                                          std::uint64_t offset, std::uint64_t size)
{
    (void)modelIndex;
    (void)offset;
    (void)size;
    return {};
}

void ISwarmMemoryBackend::prefetchUnpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size)
{
    (void)modelIndex;
    (void)offset;
    (void)size;
}

void ISwarmMemoryBackend::resetPrefetchPins() {}

namespace
{
[[nodiscard]] bool sliceIdsEqual(const ModelSliceId& a, const ModelSliceId& b)
{
    return a.modelIndex == b.modelIndex && a.layerStart == b.layerStart && a.layerEnd == b.layerEnd &&
           a.expertIndex == b.expertIndex && a.planSpanOrdinal == b.planSpanOrdinal;
}

[[nodiscard]] bool prefetchBetter(const PrefetchItem& a, const PrefetchItem& b)
{
    if (a.priority != b.priority)
        return a.priority > b.priority;
    if (a.slice.id.layerStart != b.slice.id.layerStart)
        return a.slice.id.layerStart < b.slice.id.layerStart;
    if (a.slice.id.planSpanOrdinal != b.slice.id.planSpanOrdinal)
        return a.slice.id.planSpanOrdinal < b.slice.id.planSpanOrdinal;
    return a.slice.fileOffsetBytes < b.slice.fileOffsetBytes;
}

[[nodiscard]] bool isSliceResident(const WorkingSet& ws, const ModelSliceId& id)
{
    for (const auto& r : ws.residents())
    {
        if (sliceIdsEqual(r.id, id))
            return true;
    }
    return false;
}

[[nodiscard]] const ResidentSlice* findResidentRow(const WorkingSet& ws, const ModelSliceId& id)
{
    for (const auto& r : ws.residents())
    {
        if (sliceIdsEqual(r.id, id))
            return &r;
    }
    return nullptr;
}

static void appendJsonEscaped(std::string& o, const std::string& s)
{
    for (const unsigned char c : s)
    {
        switch (c)
        {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\b':
                o += "\\b";
                break;
            case '\f':
                o += "\\f";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (c < 0x20u)
                {
                    char buf[16];
                    (void)std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    o += buf;
                }
                else
                    o += static_cast<char>(c);
                break;
        }
    }
}

/// Validates indices, then stable dedupe: first occurrence order, one entry per distinct plan row.
[[nodiscard]] std::expected<std::vector<std::size_t>, SchedulerError> dedupeValidatedPlanRowIndices(
    const std::vector<ModelSlice>& plan, const std::span<const std::size_t> rows)
{
    for (const std::size_t row : rows)
    {
        if (row >= plan.size())
            return std::unexpected(SchedulerError::InvalidArgument);
    }
    std::vector<std::size_t> out;
    out.reserve(rows.size());
    std::unordered_set<std::size_t> seen;
    for (const std::size_t row : rows)
    {
        if (seen.insert(row).second)
            out.push_back(row);
    }
    return out;
}

[[nodiscard]] bool sliceRangeIsValid(const ModelSlice& slice)
{
    if (!slice.id.valid() || slice.byteSize == 0)
        return false;
    if (slice.fileOffsetBytes > std::numeric_limits<std::uint64_t>::max() - slice.byteSize)
        return false;
    return true;
}
}  // namespace

// ---------------------------------------------------------------------------
// LinearTransformerEvictionPolicy
// ---------------------------------------------------------------------------
std::optional<ModelSliceId> LinearTransformerEvictionPolicy::chooseVictim(const std::vector<ResidentSlice>& residents,
                                                                          std::uint32_t currentLayerCursor,
                                                                          std::size_t bytesToFreeHint)
{
    (void)bytesToFreeHint;
    if (residents.empty())
        return std::nullopt;

    std::optional<ModelSliceId> bestFinished;
    std::uint32_t bestLayerStart = std::numeric_limits<std::uint32_t>::max();
    std::uint64_t bestTouch = std::numeric_limits<std::uint64_t>::max();

    for (const auto& r : residents)
    {
        if (!r.computeFinished)
            continue;
        if (r.holdCount != 0)
            continue;
        if (r.id.layerStart < bestLayerStart || (r.id.layerStart == bestLayerStart && r.lastTouchSequence < bestTouch))
        {
            bestLayerStart = r.id.layerStart;
            bestTouch = r.lastTouchSequence;
            bestFinished = r.id;
        }
    }
    if (bestFinished)
        return bestFinished;

    // No finished slice: prefer a finished slice entirely before the compute cursor.
    std::optional<ModelSliceId> behind;
    std::uint32_t minBehindStart = std::numeric_limits<std::uint32_t>::max();
    bestTouch = std::numeric_limits<std::uint64_t>::max();
    for (const auto& r : residents)
    {
        if (!r.computeFinished)
            continue;
        if (r.holdCount != 0)
            continue;
        if (r.id.layerEnd > currentLayerCursor)
            continue;
        if (r.id.layerStart < minBehindStart || (r.id.layerStart == minBehindStart && r.lastTouchSequence < bestTouch))
        {
            minBehindStart = r.id.layerStart;
            bestTouch = r.lastTouchSequence;
            behind = r.id;
        }
    }
    if (behind)
        return behind;

    // Fallback: oldest touch among finished slices only (never evict active / unfinished work).
    std::optional<ModelSliceId> lru;
    bestTouch = std::numeric_limits<std::uint64_t>::max();
    for (const auto& r : residents)
    {
        if (!r.computeFinished)
            continue;
        if (r.holdCount != 0)
            continue;
        if (r.lastTouchSequence < bestTouch)
        {
            bestTouch = r.lastTouchSequence;
            lru = r.id;
        }
    }
    return lru;
}

// ---------------------------------------------------------------------------
// LayerProximityPrefetchQueue
// ---------------------------------------------------------------------------
void LayerProximityPrefetchQueue::clear()
{
    m_items.clear();
}

void LayerProximityPrefetchQueue::enqueue(PrefetchItem item)
{
    m_items.push_back(std::move(item));
}

std::optional<PrefetchItem> LayerProximityPrefetchQueue::popNext()
{
    if (m_items.empty())
        return std::nullopt;
    auto best = m_items.begin();
    for (auto it = m_items.begin() + 1; it != m_items.end(); ++it)
    {
        if (prefetchBetter(*it, *best))
            best = it;
    }
    PrefetchItem out = std::move(*best);
    *best = std::move(m_items.back());
    m_items.pop_back();
    return out;
}

std::size_t LayerProximityPrefetchQueue::pendingCount() const
{
    return m_items.size();
}

bool LayerProximityPrefetchQueue::containsSliceId(const ModelSliceId& id) const
{
    for (const auto& pi : m_items)
    {
        if (pi.slice.id == id)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// RawrXDModelLoaderMemoryBackend (compute MapWindow + prefetch MapPrefetchWindow)
// ---------------------------------------------------------------------------
RawrXDModelLoaderMemoryBackend::RawrXDModelLoaderMemoryBackend(RawrXDModelLoader* loader) : m_loader(loader) {}

std::expected<void, SchedulerError> RawrXDModelLoaderMemoryBackend::pinRange(std::uint32_t modelIndex,
                                                                             std::uint64_t offset, std::uint64_t size)
{
    if (!m_loader)
        return std::unexpected(SchedulerError::BackendUnavailable);
    if (modelIndex != 0)
        return std::unexpected(SchedulerError::InvalidArgument);
    if (size == 0)
        return std::unexpected(SchedulerError::InvalidArgument);
#if SIZE_MAX < UINT64_MAX
    if (size > static_cast<std::uint64_t>(SIZE_MAX))
        return std::unexpected(SchedulerError::InvalidArgument);
#endif
    if (offset > std::numeric_limits<std::uint64_t>::max() - size)
        return std::unexpected(SchedulerError::InvalidArgument);
    const std::size_t sz = static_cast<std::size_t>(size);

    if (m_pinned && (m_pinnedModel != modelIndex || m_pinnedOffset != offset || m_pinnedSize != size))
        unpinRange(m_pinnedModel, m_pinnedOffset, m_pinnedSize);

    if (m_pinned && m_pinnedModel == modelIndex && m_pinnedOffset == offset && m_pinnedSize == size)
        return {};

    constexpr int kMaxMapPinAttempts = 4;
    void* p = nullptr;
    for (int attempt = 0; attempt < kMaxMapPinAttempts; ++attempt)
    {
        p = m_loader->MapWindow(offset, sz);
        if (p)
            break;
        if (attempt + 1 < kMaxMapPinAttempts)
        {
            m_loader->recordSwarmPinBackoffCycle();
            int backoffUs = 50 * (1 << attempt);
            std::uint64_t mix = static_cast<std::uint64_t>(offset) ^ (static_cast<std::uint64_t>(sz) << 1u) ^
                                (static_cast<std::uint64_t>(attempt) << 33);
            mix ^= mix >> 17;
            mix *= 0xD6E8FEB8669FD5B5ULL;
            backoffUs += static_cast<int>(mix % 11u) - 5;
            if (backoffUs < 1)
                backoffUs = 1;
            std::this_thread::sleep_for(std::chrono::microseconds(backoffUs));
        }
    }
    if (!p)
    {
        RawrXD::Logging::Logger::instance().error(std::string("[Swarm] MapWindow failed offset=") +
                                                  std::to_string(offset) + " size=" + std::to_string(size));
        return std::unexpected(SchedulerError::PinFailed);
    }
    m_loader->markComputeRangeInUse(offset, size);
    if (m_prefetchPinned && !m_loader->HasActivePrefetchMapping())
        m_prefetchPinned = false;
    m_pinned = true;
    m_pinnedModel = modelIndex;
    m_pinnedOffset = offset;
    m_pinnedSize = size;
    return {};
}

void RawrXDModelLoaderMemoryBackend::unpinRange(std::uint32_t modelIndex, std::uint64_t offset, std::uint64_t size)
{
    if (!m_loader || !m_pinned)
        return;
    if (m_pinnedModel != modelIndex || m_pinnedOffset != offset || m_pinnedSize != size)
        return;
    m_loader->unmarkComputeRangeInUse(offset, size);
    m_loader->UnmapWindow();
    m_pinned = false;
}

std::expected<void, SchedulerError> RawrXDModelLoaderMemoryBackend::prefetchPinRange(std::uint32_t modelIndex,
                                                                                     std::uint64_t offset,
                                                                                     std::uint64_t size)
{
    if (!m_loader)
        return std::unexpected(SchedulerError::BackendUnavailable);
    if (modelIndex != 0)
        return std::unexpected(SchedulerError::InvalidArgument);
    if (size == 0)
        return std::unexpected(SchedulerError::InvalidArgument);
#if SIZE_MAX < UINT64_MAX
    if (size > static_cast<std::uint64_t>(SIZE_MAX))
        return std::unexpected(SchedulerError::InvalidArgument);
#endif
    if (offset > std::numeric_limits<std::uint64_t>::max() - size)
        return std::unexpected(SchedulerError::InvalidArgument);
    const std::size_t sz = static_cast<std::size_t>(size);

    if (m_loader->ComputeMappingCovers(offset, size))
        return {};

    if (m_prefetchPinned && m_prefetchModel == modelIndex && m_prefetchOffset == offset && m_prefetchSize == size)
    {
        if (m_loader->HasActivePrefetchMapping())
            return {};
        m_prefetchPinned = false;
    }

    // Prefetch is opportunistic; when disabled we treat it as a no-op, not an error.
    if (!m_loader->IsPrefetchEnabled())
        return {};

    void* p = m_loader->MapPrefetchWindow(offset, sz);
    if (!p)
    {
        return std::unexpected(SchedulerError::PinFailed);
    }
    m_prefetchPinned = true;
    m_prefetchModel = modelIndex;
    m_prefetchOffset = offset;
    m_prefetchSize = size;
    return {};
}

void RawrXDModelLoaderMemoryBackend::prefetchUnpinRange(std::uint32_t modelIndex, std::uint64_t offset,
                                                        std::uint64_t size)
{
    if (!m_loader || !m_prefetchPinned)
        return;
    if (m_prefetchModel != modelIndex || m_prefetchOffset != offset || m_prefetchSize != size)
        return;
    m_loader->UnmapPrefetchWindow();
    m_prefetchPinned = false;
}

void RawrXDModelLoaderMemoryBackend::resetPrefetchPins()
{
    if (m_loader && m_prefetchPinned)
        m_loader->UnmapPrefetchWindow();
    m_prefetchPinned = false;
}

bool RawrXDModelLoaderMemoryBackend::sliceFullyCoveredByComputeMapping(std::uint32_t modelIndex, std::uint64_t offset,
                                                                       std::uint64_t size) const
{
    if (!m_loader || modelIndex != 0 || size == 0)
        return false;
    return m_loader->ComputeMappingCovers(offset, size);
}

// ---------------------------------------------------------------------------
// SwarmScheduler
// ---------------------------------------------------------------------------
SwarmScheduler::SwarmScheduler()
    : m_backend(nullptr), m_prefetch(std::make_unique<LayerProximityPrefetchQueue>()),
      m_eviction(std::make_unique<LinearTransformerEvictionPolicy>())
{
}

SwarmScheduler::SwarmScheduler(std::shared_ptr<ISwarmMemoryBackend> backend,
                               std::unique_ptr<IPrefetchQueue> prefetchQueue, std::unique_ptr<IEvictionPolicy> eviction)
    : m_backend(std::move(backend)),
      m_prefetch(prefetchQueue ? std::move(prefetchQueue) : std::make_unique<LayerProximityPrefetchQueue>()),
      m_eviction(eviction ? std::move(eviction) : std::make_unique<LinearTransformerEvictionPolicy>())
{
}

SwarmScheduler::~SwarmScheduler()
{
    stopPrefetchIoThread_();
}

void SwarmScheduler::prefetchIoThreadMain_()
{
    while (!m_prefetchIoStop.load(std::memory_order_acquire))
    {
        std::unique_lock<std::mutex> lk(m_prefetchIoMutex);
        const auto ms = std::chrono::milliseconds(std::max<std::uint32_t>(1u, m_cfg.prefetchIoPollMs));
        m_prefetchIoCv.wait_for(lk, ms, [this] { return m_prefetchIoStop.load(std::memory_order_acquire); });
        lk.unlock();

        if (m_prefetchIoStop.load(std::memory_order_acquire))
            break;

        (void)prefetchPump();
        (void)pruneWorkingSet();
    }
}

void SwarmScheduler::startPrefetchIoThread_()
{
    stopPrefetchIoThread_();
    m_prefetchIoStop.store(false, std::memory_order_release);
    m_prefetchIoThread = std::thread([this] { prefetchIoThreadMain_(); });
}

void SwarmScheduler::stopPrefetchIoThread_()
{
    if (!m_prefetchIoThread.joinable())
        return;
    m_prefetchIoStop.store(true, std::memory_order_release);
    m_prefetchIoCv.notify_all();
    m_prefetchIoThread.join();
    m_prefetchIoStop.store(false, std::memory_order_release);
}

void SwarmScheduler::notifyPrefetchIoThread_()
{
    m_prefetchIoCv.notify_one();
}

std::expected<void, SchedulerError> SwarmScheduler::configure(const SchedulerConfig& cfg)
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    m_cfg = cfg;
    m_workingSet.configure(m_cfg.maxWorkingSetBytes, m_cfg.maxResidentLayers);
    return {};
}

std::expected<void, SchedulerError> SwarmScheduler::submitPlan(std::vector<ModelSlice> plan)
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    for (const ModelSlice& slice : plan)
    {
        if (!sliceRangeIsValid(slice))
            return std::unexpected(SchedulerError::InvalidArgument);
    }
    for (const auto& r : m_workingSet.residents())
    {
        if (r.holdCount != 0)
            return std::unexpected(SchedulerError::PlanRowsHeld);
    }
    m_plan = std::move(plan);
    m_planSliceIndex.rebuildFrom(m_plan);
    m_recentExpertPins.clear();
    ++m_planGeneration;
    return {};
}

std::uint64_t SwarmScheduler::planGeneration() const
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    return m_planGeneration;
}

void SwarmScheduler::setPlanRowEvictionObserver(
    std::function<void(std::span<const std::size_t> planRowIndices)> observer)
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    m_planRowEvictionObserver = std::move(observer);
}

bool SwarmScheduler::arePlanRowsResident(std::span<const std::size_t> planRowIndices) const
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    for (const std::size_t row : planRowIndices)
    {
        if (row >= m_plan.size())
            return false;
        if (!isSliceResident(m_workingSet, m_plan[row].id))
            return false;
    }
    return true;
}

std::vector<std::size_t> SwarmScheduler::planRowIndicesForSliceIdUnlocked_(const ModelSliceId& id) const
{
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < m_plan.size(); ++i)
    {
        if (sliceIdsEqual(m_plan[i].id, id))
            out.push_back(i);
    }
    return out;
}

void SwarmScheduler::notifyPlanRowEvictionsForSliceIdUnlocked_(const ModelSliceId& id)
{
    if (!m_planRowEvictionObserver)
        return;
    const std::vector<std::size_t> rows = planRowIndicesForSliceIdUnlocked_(id);
    if (!rows.empty())
        m_planRowEvictionObserver(std::span<const std::size_t>(rows.data(), rows.size()));
}

SwarmPlanSliceIndex SwarmScheduler::planSliceIndexSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    return m_planSliceIndex;
}

std::size_t SwarmScheduler::submittedPlanRowCount() const
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    return m_plan.size();
}

bool SwarmScheduler::tryCopySubmittedPlanRow(const std::size_t planRowIndex, ModelSlice& out) const
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    if (planRowIndex >= m_plan.size())
        return false;
    out = m_plan[planRowIndex];
    return true;
}

bool SwarmScheduler::captureExpertHeatmapSnapshot(const ExpertHeatmapCaptureParams& params, ExpertHeatmapSnapshot& out)
{
    out = {};
    constexpr std::size_t kHardMaxCells = 1000000;
    const std::uint32_t stride = std::max<std::uint32_t>(1u, params.planRowStride);
    const std::size_t userCap = params.maxCells == 0
                                    ? kHardMaxCells
                                    : static_cast<std::size_t>(std::min<std::uint64_t>(params.maxCells, kHardMaxCells));

    std::lock_guard<std::mutex> lock(m_schedMutex);
    ++m_expertHeatmapSnapshotSeq;
    out.snapshotId = m_expertHeatmapSnapshotSeq;
    out.planGeneration = m_planGeneration;
    out.sampleTsMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());

    out.statsAtSample.prefetchPinSuccess = m_statPrefetchPinSuccess.load(std::memory_order_relaxed);
    out.statsAtSample.prefetchSkippedDuplicate = m_statPrefetchSkippedDuplicate.load(std::memory_order_relaxed);
    out.statsAtSample.prefetchSkippedComputeCovers = m_statPrefetchSkippedComputeCovers.load(std::memory_order_relaxed);
    out.statsAtSample.prefetchEnqueueSkippedDuplicate =
        m_statPrefetchEnqueueSkippedDuplicate.load(std::memory_order_relaxed);
    out.statsAtSample.evictions = m_statEvictions.load(std::memory_order_relaxed);
    out.statsAtSample.jitPinNonResident = m_statJitPinNonResident.load(std::memory_order_relaxed);
    out.statsAtSample.speculativeExpertPrefetchEnqueued =
        m_statSpeculativeExpertPrefetchEnqueued.load(std::memory_order_relaxed);
    out.statsAtSample.evictionRejectedInUse = m_statEvictionRejectedInUse.load(std::memory_order_relaxed);
    out.statsAtSample.evictStarvation = m_statEvictStarvation.load(std::memory_order_relaxed);
    out.statsAtSample.pinBlockAttempts = m_statPinBlockAttempts.load(std::memory_order_relaxed);
    out.statsAtSample.pinBlockTimeouts = m_statPinBlockTimeouts.load(std::memory_order_relaxed);
    out.statsAtSample.pinBlockLatencyMsTotal = m_statPinBlockLatencyMsTotal.load(std::memory_order_relaxed);
    for (const auto& r : m_workingSet.residents())
    {
        if (r.holdCount != 0)
            ++out.statsAtSample.inUseSliceCount;
    }

    out.cells.reserve(std::min(m_plan.size(), userCap));
    std::size_t added = 0;
    for (std::size_t i = 0; i < m_plan.size(); i += static_cast<std::size_t>(stride))
    {
        if (added >= userCap)
            break;
        const ModelSlice& slice = m_plan[i];
        if (params.modelIndex != kExpertHeatmapAllModels && slice.id.modelIndex != params.modelIndex)
            continue;
        if (params.expertsOnly && slice.id.expertIndex == 0xFFFFFFFFu)
            continue;

        ExpertHeatmapCell cell;
        cell.planRowIndex = i;
        cell.modelIndex = slice.id.modelIndex;
        cell.layerStart = slice.id.layerStart;
        cell.layerEnd = slice.id.layerEnd;
        cell.expertIndex = slice.id.expertIndex;
        cell.planSpanOrdinal = slice.id.planSpanOrdinal;
        // Omit slice.debugName here: copying arbitrary strings per row under m_schedMutex hurts p95 hold time.

        const ResidentSlice* row = findResidentRow(m_workingSet, slice.id);
        cell.resident = row != nullptr;
        if (row)
        {
            cell.holdCount = row->holdCount;
            cell.residentBytes = row->residentBytes;
            cell.lastTouchSequence = row->lastTouchSequence;
        }
        cell.prefetchInFlight = m_prefetchInFlightIds.find(slice.id) != m_prefetchInFlightIds.end();

        out.cells.push_back(std::move(cell));
        ++added;
    }

    return true;
}

std::string expertHeatmapSnapshotToJson(const ExpertHeatmapSnapshot& snap)
{
    std::string o;
    o.reserve(256 + snap.cells.size() * 96);
    o += '{';
    {
        char b[160];
        (void)std::snprintf(b, sizeof(b), "\"snapshotId\":%llu,\"planGeneration\":%llu,\"sampleTsMs\":%llu,",
                            static_cast<unsigned long long>(snap.snapshotId),
                            static_cast<unsigned long long>(snap.planGeneration),
                            static_cast<unsigned long long>(snap.sampleTsMs));
        o += b;
    }
    o += "\"stats\":{";
    {
        char b[512];
        (void)std::snprintf(
            b, sizeof(b),
            "\"prefetchPinSuccess\":%llu,\"prefetchSkippedDuplicate\":%llu,\"prefetchSkippedComputeCovers\":%llu,"
            "\"prefetchEnqueueSkippedDuplicate\":%llu,\"evictions\":%llu,\"jitPinNonResident\":%llu,"
            "\"speculativeExpertPrefetchEnqueued\":%llu,\"evictionRejectedInUse\":%llu,\"evictStarvation\":%llu,"
            "\"pinBlockAttempts\":%llu,\"pinBlockTimeouts\":%llu,\"pinBlockLatencyMsTotal\":%llu,\"inUseSliceCount\":%u"
            "}",
            static_cast<unsigned long long>(snap.statsAtSample.prefetchPinSuccess),
            static_cast<unsigned long long>(snap.statsAtSample.prefetchSkippedDuplicate),
            static_cast<unsigned long long>(snap.statsAtSample.prefetchSkippedComputeCovers),
            static_cast<unsigned long long>(snap.statsAtSample.prefetchEnqueueSkippedDuplicate),
            static_cast<unsigned long long>(snap.statsAtSample.evictions),
            static_cast<unsigned long long>(snap.statsAtSample.jitPinNonResident),
            static_cast<unsigned long long>(snap.statsAtSample.speculativeExpertPrefetchEnqueued),
            static_cast<unsigned long long>(snap.statsAtSample.evictionRejectedInUse),
            static_cast<unsigned long long>(snap.statsAtSample.evictStarvation),
            static_cast<unsigned long long>(snap.statsAtSample.pinBlockAttempts),
            static_cast<unsigned long long>(snap.statsAtSample.pinBlockTimeouts),
            static_cast<unsigned long long>(snap.statsAtSample.pinBlockLatencyMsTotal),
            static_cast<unsigned>(snap.statsAtSample.inUseSliceCount));
        o += b;
    }
    o += ",\"cells\":[";
    for (std::size_t i = 0; i < snap.cells.size(); ++i)
    {
        if (i != 0)
            o += ',';
        const ExpertHeatmapCell& c = snap.cells[i];
        char b[256];
        (void)std::snprintf(
            b, sizeof(b),
            "{\"row\":%zu,\"model\":%u,\"layer\":%u,\"layerEnd\":%u,\"expert\":%u,\"span\":%u,"
            "\"resident\":%s,\"pfInflight\":%s,\"hold\":%u,\"bytes\":%llu,\"touchSeq\":%llu,\"name\":\"",
            c.planRowIndex, static_cast<unsigned>(c.modelIndex), static_cast<unsigned>(c.layerStart),
            static_cast<unsigned>(c.layerEnd),
            c.expertIndex == 0xFFFFFFFFu ? static_cast<unsigned>(-1) : static_cast<unsigned>(c.expertIndex),
            static_cast<unsigned>(c.planSpanOrdinal), c.resident ? "true" : "false",
            c.prefetchInFlight ? "true" : "false", static_cast<unsigned>(c.holdCount),
            static_cast<unsigned long long>(c.residentBytes), static_cast<unsigned long long>(c.lastTouchSequence));
        o += b;
        appendJsonEscaped(o, c.debugName);
        o += "\"}";
    }
    o += "]}";
    return o;
}

std::expected<void, SchedulerError> SwarmScheduler::pinPlanRow(const std::size_t planRowIndex)
{
    const std::array<std::size_t, 1> one = {planRowIndex};
    return pinPlanRows(std::span<const std::size_t>(one.data(), one.size()));
}

void SwarmScheduler::rollbackNewAdmits_(const std::vector<ModelSliceId>& ids)
{
    std::unordered_set<std::size_t> notifyRows;
    for (const ModelSliceId& id : ids)
    {
        const ResidentSlice* row = nullptr;
        for (const auto& r : m_workingSet.residents())
        {
            if (sliceIdsEqual(r.id, id))
            {
                row = &r;
                break;
            }
        }
        if (!row)
            continue;
        if (m_backend)
            m_backend->unpinRange(id.modelIndex, row->fileOffsetBytes, row->residentBytes);
        m_workingSet.recordEvict(id);
        for (const std::size_t pr : planRowIndicesForSliceIdUnlocked_(id))
            notifyRows.insert(pr);
    }
    if (!notifyRows.empty() && m_planRowEvictionObserver)
    {
        std::vector<std::size_t> v(notifyRows.begin(), notifyRows.end());
        std::sort(v.begin(), v.end());
        m_planRowEvictionObserver(std::span<const std::size_t>(v.data(), v.size()));
    }
}

std::expected<void, SchedulerError> SwarmScheduler::pinPlanRowsBatchUniqueLocked_(
    const std::vector<std::size_t>& uniqueRows)
{
    for (const std::size_t row : uniqueRows)
    {
        const ModelSliceId& id = m_plan[row].id;
        if (!isSliceResident(m_workingSet, id))
            m_statJitPinNonResident.fetch_add(1, std::memory_order_relaxed);
    }

    std::vector<ModelSliceId> newlyAdmitted;
    newlyAdmitted.reserve(uniqueRows.size());

    for (const std::size_t row : uniqueRows)
    {
        const ModelSlice& s = m_plan[row];
        if (isSliceResident(m_workingSet, s.id))
            continue;
        const std::expected<void, SchedulerError> st = admitOrEvictAndPin_(s);
        if (!st)
        {
            rollbackNewAdmits_(newlyAdmitted);
            return st;
        }
        newlyAdmitted.push_back(s.id);
    }

    for (const std::size_t row : uniqueRows)
    {
        const ModelSliceId& id = m_plan[row].id;
        bool ok = false;
        for (const auto& r : m_workingSet.residents())
        {
            if (sliceIdsEqual(r.id, id))
            {
                ok = true;
                break;
            }
        }
        if (!ok)
        {
            rollbackNewAdmits_(newlyAdmitted);
            return std::unexpected(SchedulerError::SliceNotFound);
        }
    }

    for (const std::size_t row : uniqueRows)
    {
        const ModelSliceId& id = m_plan[row].id;
        for (auto& r : m_workingSet.residentsMut())
        {
            if (sliceIdsEqual(r.id, id))
            {
                r.holdCount++;
                break;
            }
        }
    }

    for (const std::size_t row : uniqueRows)
        recordExpertSpeculationHint_(m_plan[row].id);
    return {};
}

std::expected<void, SchedulerError> SwarmScheduler::pinPlanRows(const std::span<const std::size_t> planRowIndices)
{
    {
        std::lock_guard<std::mutex> lock(m_schedMutex);
        const auto uniqueRows = dedupeValidatedPlanRowIndices(m_plan, planRowIndices);
        if (!uniqueRows)
            return std::unexpected(uniqueRows.error());
        for (const std::size_t row : *uniqueRows)
            enqueueUrgentPrefetchUnlocked_(m_plan[row]);
        const auto st = pinPlanRowsBatchUniqueLocked_(*uniqueRows);
        if (!st)
            return st;
        m_schedCv.notify_all();
    }
    notifyPrefetchIoThread_();
    return {};
}

bool SwarmScheduler::pinPlanRowsBlocking(const std::span<const std::size_t> planRowIndices,
                                         const std::uint32_t timeoutMs)
{
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    const auto deadline = t0 + std::chrono::milliseconds(timeoutMs);

    std::unique_lock<std::mutex> lk(m_schedMutex);
    m_statPinBlockAttempts.fetch_add(1, std::memory_order_relaxed);

    const auto uniqueRows = dedupeValidatedPlanRowIndices(m_plan, planRowIndices);
    if (!uniqueRows)
    {
        lk.unlock();
        return false;
    }
    const std::vector<std::size_t>& u = *uniqueRows;

    while (true)
    {
        for (const std::size_t row : u)
            enqueueUrgentPrefetchUnlocked_(m_plan[row]);

        const auto st = pinPlanRowsBatchUniqueLocked_(u);
        if (st)
        {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
            m_statPinBlockLatencyMsTotal.fetch_add(static_cast<std::uint64_t>(std::max<std::int64_t>(0, ms)),
                                                   std::memory_order_relaxed);
            lk.unlock();
            notifyPrefetchIoThread_();
            return true;
        }

        if (st.error() != SchedulerError::WorkingSetFull)
        {
            lk.unlock();
            notifyPrefetchIoThread_();
            return false;
        }

        if (timeoutMs == 0)
        {
            m_statPinBlockTimeouts.fetch_add(1, std::memory_order_relaxed);
            lk.unlock();
            notifyPrefetchIoThread_();
            return false;
        }

        if (m_schedCv.wait_until(lk, deadline) == std::cv_status::timeout)
        {
            m_statPinBlockTimeouts.fetch_add(1, std::memory_order_relaxed);
            lk.unlock();
            notifyPrefetchIoThread_();
            return false;
        }
    }
}

void SwarmScheduler::unpinPlanRows(const std::span<const std::size_t> planRowIndices)
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    std::unordered_set<std::size_t> seen;
    for (const std::size_t row : planRowIndices)
    {
        if (row >= m_plan.size())
            continue;
        if (!seen.insert(row).second)
            continue;
        const ModelSliceId& id = m_plan[row].id;
        for (auto& r : m_workingSet.residentsMut())
        {
            if (sliceIdsEqual(r.id, id))
            {
                if (r.holdCount > 0)
                    r.holdCount--;
                break;
            }
        }
    }
    m_schedCv.notify_all();
}

void SwarmScheduler::unpinAllResidents_()
{
    if (m_backend)
        m_backend->resetPrefetchPins();
    if (m_backend)
    {
        for (const auto& r : m_workingSet.residents())
            m_backend->unpinRange(r.id.modelIndex, r.fileOffsetBytes, r.residentBytes);
    }
    m_workingSet.clear();
}

std::expected<void, SchedulerError> SwarmScheduler::admitOrEvictAndPin_(const ModelSlice& slice)
{
    if (slice.byteSize == 0 || !slice.id.valid())
        return std::unexpected(SchedulerError::InvalidArgument);

    if (isSliceResident(m_workingSet, slice.id))
        return {};

    auto admit = m_workingSet.tryAdmit(slice.byteSize);
    while (!admit)
    {
        if (!m_eviction)
            return std::unexpected(SchedulerError::NotImplemented);
        auto vic = m_eviction->chooseVictim(m_workingSet.residents(), m_currentComputeLayer, slice.byteSize);
        if (!vic)
        {
            m_statEvictStarvation.fetch_add(1, std::memory_order_relaxed);
            bool anyHeldFinished = false;
            bool anyEvictableUnheld = false;
            for (const auto& r : m_workingSet.residents())
            {
                if (!r.computeFinished)
                    continue;
                if (r.holdCount != 0)
                    anyHeldFinished = true;
                else
                    anyEvictableUnheld = true;
            }
            if (anyHeldFinished && !anyEvictableUnheld)
                m_statEvictionRejectedInUse.fetch_add(1, std::memory_order_relaxed);
            return std::unexpected(SchedulerError::WorkingSetFull);
        }

        const ResidentSlice* row = nullptr;
        for (const auto& r : m_workingSet.residents())
        {
            if (sliceIdsEqual(r.id, *vic))
            {
                row = &r;
                break;
            }
        }
        if (!row)
            return std::unexpected(SchedulerError::SliceNotFound);

        if (row->holdCount != 0)
        {
            m_statEvictStarvation.fetch_add(1, std::memory_order_relaxed);
            m_statEvictionRejectedInUse.fetch_add(1, std::memory_order_relaxed);
            return std::unexpected(SchedulerError::WorkingSetFull);
        }

        if (m_backend)
            m_backend->unpinRange(vic->modelIndex, row->fileOffsetBytes, row->residentBytes);
        m_workingSet.recordEvict(*vic);
        notifyPlanRowEvictionsForSliceIdUnlocked_(*vic);
        m_statEvictions.fetch_add(1, std::memory_order_relaxed);
        admit = m_workingSet.tryAdmit(slice.byteSize);
        m_schedCv.notify_all();
    }

    if (m_backend)
    {
        auto pinned = m_backend->pinRange(slice.id.modelIndex, slice.fileOffsetBytes, slice.byteSize);
        if (!pinned)
            return pinned;
    }

    ResidentSlice rs;
    rs.id = slice.id;
    rs.residentBytes = slice.byteSize;
    rs.fileOffsetBytes = slice.fileOffsetBytes;
    rs.lastTouchSequence = ++m_sequence;
    rs.computeFinished = false;
    m_workingSet.recordAdmit(std::move(rs));
    return {};
}

void SwarmScheduler::touchResidentForLayer_(std::uint32_t modelIndex, std::uint32_t layerIndex)
{
    for (auto& r : m_workingSet.residentsMut())
    {
        if (r.id.modelIndex != modelIndex)
            continue;
        if (layerIndex < r.id.layerStart || layerIndex >= r.id.layerEnd)
            continue;
        r.lastTouchSequence = ++m_sequence;
    }
}

void SwarmScheduler::markFinishedForLayer_(std::uint32_t modelIndex, std::uint32_t layerIndex)
{
    for (auto& r : m_workingSet.residentsMut())
    {
        if (r.id.modelIndex != modelIndex)
            continue;
        if (layerIndex < r.id.layerStart || layerIndex >= r.id.layerEnd)
            continue;
        if (layerIndex + 1u >= r.id.layerEnd)
            r.computeFinished = true;
    }
}

void SwarmScheduler::enqueueUrgentPrefetchUnlocked_(const ModelSlice& slice)
{
    if (!m_backend)
        return;
    if (isSliceResident(m_workingSet, slice.id))
        return;
    if (m_prefetchCompletedIds.count(slice.id) != 0 || m_prefetchInFlightIds.count(slice.id) != 0)
        return;
    for (const PrefetchItem& u : m_urgentPrefetch)
    {
        if (u.slice.id == slice.id)
            return;
    }

    PrefetchItem pi;
    pi.slice = slice;
    pi.priority = 0x7FFFF000u;
    m_urgentPrefetch.push_back(std::move(pi));
    constexpr std::size_t kMaxUrgent = 48;
    while (m_urgentPrefetch.size() > kMaxUrgent)
        m_urgentPrefetch.pop_front();
}

void SwarmScheduler::recordExpertSpeculationHint_(const ModelSliceId& id)
{
    if (id.expertIndex == 0xFFFFFFFFu)
        return;

    const ExpertPinRecord rec{id.modelIndex, id.layerStart, id.expertIndex};
    m_recentExpertPins.erase(std::remove_if(m_recentExpertPins.begin(), m_recentExpertPins.end(),
                                            [&rec](const ExpertPinRecord& x)
                                            {
                                                return x.modelIndex == rec.modelIndex &&
                                                       x.layerStart == rec.layerStart &&
                                                       x.expertOrdinal == rec.expertOrdinal;
                                            }),
                             m_recentExpertPins.end());
    m_recentExpertPins.push_back(rec);
    const std::uint32_t cap = std::max<std::uint32_t>(4u, m_cfg.expertSpeculationHistory);
    while (m_recentExpertPins.size() > static_cast<std::size_t>(cap))
        m_recentExpertPins.pop_front();
}

bool SwarmScheduler::isLikelySpeculativeExpert_(const std::uint32_t modelIndex, const std::uint32_t layerBucket,
                                                const std::uint32_t expertOrdinal) const
{
    for (const ExpertPinRecord& r : m_recentExpertPins)
    {
        if (r.modelIndex == modelIndex && r.layerStart == layerBucket && r.expertOrdinal == expertOrdinal)
            return true;
    }
    return false;
}

void SwarmScheduler::enqueuePrefetchAroundHead_(std::uint32_t modelIndex, std::uint32_t layerIndex)
{
    if (!m_prefetch)
        return;
    const std::uint32_t ahead = m_cfg.prefetchAheadLayers;

    const auto shouldConsider = [&](const ModelSlice& s) -> bool
    {
        if (s.id.modelIndex != modelIndex)
            return false;
        if (s.id.layerStart <= layerIndex)
            return false;
        if (s.id.layerStart > layerIndex + ahead)
            return false;
        if (isSliceResident(m_workingSet, s.id))
            return false;
        if (m_prefetchCompletedIds.count(s.id) != 0 || m_prefetchInFlightIds.count(s.id) != 0)
        {
            m_statPrefetchEnqueueSkippedDuplicate.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (m_prefetch->containsSliceId(s.id))
        {
            m_statPrefetchEnqueueSkippedDuplicate.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    };

    const auto doEnqueue = [&](const ModelSlice& s, const std::uint32_t tier)
    {
        if (!shouldConsider(s))
            return;
        PrefetchItem pi;
        pi.slice = s;
        const std::uint32_t dist = s.id.layerStart - layerIndex;
        const std::uint32_t ord = std::min(s.id.planSpanOrdinal, 255u);
        const std::uint32_t tierBoost = (tier == 0u) ? 0x400000u : 0x200000u;
        pi.priority = tierBoost + (ahead + 1u - dist) * 256u - ord;
        m_prefetch->enqueue(std::move(pi));
        if (tier == 1u)
            m_statSpeculativeExpertPrefetchEnqueued.fetch_add(1, std::memory_order_relaxed);
    };

    for (const ModelSlice& s : m_plan)
    {
        if (s.id.expertIndex == 0xFFFFFFFFu)
            doEnqueue(s, 0u);
    }

    if (m_cfg.enableExpertSpeculativePrefetch)
    {
        for (const ModelSlice& s : m_plan)
        {
            if (s.id.expertIndex == 0xFFFFFFFFu)
                continue;
            if (!isLikelySpeculativeExpert_(modelIndex, s.id.layerStart, s.id.expertIndex))
                continue;
            doEnqueue(s, 1u);
        }
    }
}

std::expected<void, SchedulerError> SwarmScheduler::executePlan()
{
    stopPrefetchIoThread_();

    {
        std::lock_guard<std::mutex> lock(m_schedMutex);
        for (const auto& r : m_workingSet.residents())
        {
            if (r.holdCount != 0)
                return std::unexpected(SchedulerError::PlanRowsHeld);
        }
        unpinAllResidents_();
        if (m_prefetch)
            m_prefetch->clear();
        m_prefetchCompletedIds.clear();
        m_prefetchInFlightIds.clear();
        m_urgentPrefetch.clear();

        m_workingSet.configure(m_cfg.maxWorkingSetBytes, m_cfg.maxResidentLayers);

        if (!m_plan.empty() && m_cfg.admitFirstSliceOnExecutePlan)
        {
            const auto first = admitOrEvictAndPin_(m_plan[0]);
            if (!first)
                return first;
            m_currentComputeModel = m_plan[0].id.modelIndex;
            m_currentComputeLayer = m_plan[0].id.layerStart;
            enqueuePrefetchAroundHead_(m_currentComputeModel, m_currentComputeLayer);
        }
        else if (!m_plan.empty())
        {
            m_currentComputeModel = m_plan[0].id.modelIndex;
            m_currentComputeLayer = m_plan[0].id.layerStart;
            enqueuePrefetchAroundHead_(m_currentComputeModel, m_currentComputeLayer);
        }
        else
        {
            m_currentComputeModel = 0;
            m_currentComputeLayer = 0;
        }
    }

    if (m_cfg.enableAsyncPrefetchThread && m_backend)
        startPrefetchIoThread_();
    else
        (void)prefetchPump();

    return {};
}

std::expected<void, SchedulerError> SwarmScheduler::onLayerComputeStarted(std::uint32_t modelIndex,
                                                                          std::uint32_t layerIndex)
{
    {
        std::lock_guard<std::mutex> lock(m_schedMutex);
        m_currentComputeModel = modelIndex;
        m_currentComputeLayer = layerIndex;
        touchResidentForLayer_(modelIndex, layerIndex);
        enqueuePrefetchAroundHead_(modelIndex, layerIndex);
    }

    if (m_cfg.enableAsyncPrefetchThread && m_backend && m_prefetchIoThread.joinable())
    {
        notifyPrefetchIoThread_();
        return {};
    }
    return prefetchPump();
}

void SwarmScheduler::onForwardTokenStepBegin()
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    m_prefetchCompletedIds.clear();
    m_prefetchInFlightIds.clear();
}

std::expected<void, SchedulerError> SwarmScheduler::onLayerComputeFinished(std::uint32_t modelIndex,
                                                                           std::uint32_t layerIndex)
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    markFinishedForLayer_(modelIndex, layerIndex);
    return pruneWorkingSetUnlocked_();
}

std::expected<void, SchedulerError> SwarmScheduler::prefetchPump()
{
    for (;;)
    {
        std::optional<PrefetchItem> item;
        ModelSliceId sid{};
        {
            std::lock_guard<std::mutex> lock(m_schedMutex);
            if (!m_urgentPrefetch.empty())
            {
                item = std::move(m_urgentPrefetch.front());
                m_urgentPrefetch.pop_front();
            }
            else if (m_prefetch && m_prefetch->pendingCount() != 0)
            {
                item = m_prefetch->popNext();
            }
            else
            {
                return {};
            }
            if (!item)
                return {};
            sid = item->slice.id;
            if (m_prefetchInFlightIds.count(sid) != 0 || m_prefetchCompletedIds.count(sid) != 0)
            {
                m_statPrefetchSkippedDuplicate.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (m_backend && m_backend->sliceFullyCoveredByComputeMapping(
                                 item->slice.id.modelIndex, item->slice.fileOffsetBytes, item->slice.byteSize))
            {
                m_prefetchCompletedIds.insert(sid);
                m_statPrefetchSkippedComputeCovers.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            m_prefetchInFlightIds.insert(sid);
        }

        if (!m_backend)
        {
            std::lock_guard<std::mutex> lock(m_schedMutex);
            m_prefetchInFlightIds.erase(sid);
            continue;
        }

        auto pf =
            m_backend->prefetchPinRange(item->slice.id.modelIndex, item->slice.fileOffsetBytes, item->slice.byteSize);

        {
            std::lock_guard<std::mutex> lock(m_schedMutex);
            m_prefetchInFlightIds.erase(sid);
            if (pf)
            {
                m_prefetchCompletedIds.insert(sid);
                m_statPrefetchPinSuccess.fetch_add(1, std::memory_order_relaxed);
                if (m_cfg.verbosePrefetchLogging)
                {
                    RawrXD::Logging::Logger::instance().info(std::string("[Swarm] prefetch pin model=") +
                                                             std::to_string(item->slice.id.modelIndex) + " layers=[" +
                                                             std::to_string(item->slice.id.layerStart) + "," +
                                                             std::to_string(item->slice.id.layerEnd) +
                                                             ") off=" + std::to_string(item->slice.fileOffsetBytes) +
                                                             " sz=" + std::to_string(item->slice.byteSize));
                }
            }
        }

        if (pf)
            continue;

        if (pf.error() == SchedulerError::NotImplemented)
        {
            std::lock_guard<std::mutex> lock(m_schedMutex);
            auto st = admitOrEvictAndPin_(item->slice);
            if (!st)
                return st;
            continue;
        }
        return pf;
    }
}

std::expected<void, SchedulerError> SwarmScheduler::pruneWorkingSetUnlocked_()
{
    const std::size_t cap = m_workingSet.capacityBytes();
    if (cap == 0)
        return {};
    if (m_workingSet.currentBytes() * 10 <= cap * 9)
        return {};

    if (!m_eviction)
        return std::unexpected(SchedulerError::NotImplemented);

    auto vic = m_eviction->chooseVictim(m_workingSet.residents(), m_currentComputeLayer, 0);
    if (!vic)
        return {};

    const ResidentSlice* row = nullptr;
    for (const auto& r : m_workingSet.residents())
    {
        if (sliceIdsEqual(r.id, *vic))
        {
            row = &r;
            break;
        }
    }
    if (!row)
        return std::unexpected(SchedulerError::SliceNotFound);

    if (!row->computeFinished)
        return {};

    if (row->holdCount != 0)
        return {};

    if (m_backend)
        m_backend->unpinRange(vic->modelIndex, row->fileOffsetBytes, row->residentBytes);
    m_workingSet.recordEvict(*vic);
    notifyPlanRowEvictionsForSliceIdUnlocked_(*vic);
    m_statEvictions.fetch_add(1, std::memory_order_relaxed);
    m_schedCv.notify_all();
    return {};
}

SwarmRuntimeStats SwarmScheduler::runtimeStats() const
{
    std::uint32_t inUse = 0;
    {
        std::lock_guard<std::mutex> lock(m_schedMutex);
        for (const auto& r : m_workingSet.residents())
        {
            if (r.holdCount != 0)
                ++inUse;
        }
    }
    return {m_statPrefetchPinSuccess.load(std::memory_order_relaxed),
            m_statPrefetchSkippedDuplicate.load(std::memory_order_relaxed),
            m_statPrefetchSkippedComputeCovers.load(std::memory_order_relaxed),
            m_statPrefetchEnqueueSkippedDuplicate.load(std::memory_order_relaxed),
            m_statEvictions.load(std::memory_order_relaxed),
            m_statJitPinNonResident.load(std::memory_order_relaxed),
            m_statSpeculativeExpertPrefetchEnqueued.load(std::memory_order_relaxed),
            m_statEvictionRejectedInUse.load(std::memory_order_relaxed),
            m_statEvictStarvation.load(std::memory_order_relaxed),
            m_statPinBlockAttempts.load(std::memory_order_relaxed),
            m_statPinBlockTimeouts.load(std::memory_order_relaxed),
            m_statPinBlockLatencyMsTotal.load(std::memory_order_relaxed),
            inUse};
}

std::expected<void, SchedulerError> SwarmScheduler::pruneWorkingSet()
{
    std::lock_guard<std::mutex> lock(m_schedMutex);
    return pruneWorkingSetUnlocked_();
}

std::unique_ptr<SwarmScheduler> makeStubSwarmScheduler()
{
    return std::make_unique<SwarmScheduler>();
}

std::unique_ptr<SwarmScheduler> makeSwarmSchedulerWithLoader(RawrXDModelLoader* loader)
{
    auto backend = std::make_shared<RawrXDModelLoaderMemoryBackend>(loader);
    return std::make_unique<SwarmScheduler>(std::move(backend), std::make_unique<LayerProximityPrefetchQueue>(),
                                            std::make_unique<LinearTransformerEvictionPolicy>());
}

}  // namespace Swarm
}  // namespace RawrXD
