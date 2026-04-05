#include "rawrxd_transformer.h"
#include "core/moe_expert_accumulation.hpp"
#include "core/swarm_scheduler.hpp"
#include "logging/Logger.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <windows.h>

namespace
{
[[nodiscard]] const char* moeDownProjectPathTag(RawrXD::MoEDownProject::Path p) noexcept
{
    return p == RawrXD::MoEDownProject::Path::GroupedCached ? "GroupedCached" : "Looped";
}

void collectMoeMixturePlanRowRefs(const std::uint32_t modelIndex, const std::uint32_t layer,
                                  const std::vector<std::uint32_t>& pickOrder,
                                  const RawrXD::Swarm::SwarmPlanSliceIndex& idx, std::vector<std::size_t>& outRows)
{
    outRows.clear();
    outRows.reserve(pickOrder.size());
    for (const std::uint32_t e : pickOrder)
    {
        for (const std::size_t r : idx.indicesFor(modelIndex, layer, e))
        {
            outRows.push_back(r);
            break;
        }
    }
    std::sort(outRows.begin(), outRows.end());
}

[[nodiscard]] std::string makeMoeMixturePackCacheKey(const std::uint32_t modelIndex, const std::uint32_t layer,
                                                     const std::vector<std::uint32_t>& pickOrder,
                                                     const RawrXD::Swarm::SwarmPlanSliceIndex& idx)
{
    std::string k = std::to_string(modelIndex);
    k.push_back(':');
    k += std::to_string(layer);
    k.push_back('|');
    for (std::size_t i = 0; i < pickOrder.size(); ++i)
    {
        if (i != 0)
            k.push_back(',');
        k += std::to_string(pickOrder[i]);
    }
    k += "|pr";
    std::vector<std::size_t> rows;
    collectMoeMixturePlanRowRefs(modelIndex, layer, pickOrder, idx, rows);
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        k.push_back(i == 0 ? '_' : ',');
        k += std::to_string(rows[i]);
    }
    return k;
}

[[nodiscard]] float* tryGetExpertDownWeightTensor(RawrXDModelLoader* loader, const std::string& blkPrefix,
                                                  const std::uint32_t expert)
{
    if (!loader)
        return nullptr;
    const std::string ep = blkPrefix + "ffn_experts." + std::to_string(expert) + ".";
    const std::array<const char*, 2> names = {"ffn_down.weight", "w2.weight"};
    for (const char* n : names)
    {
        const std::string full = ep + n;
        if (loader->hasTensorNamed(full))
            return loader->GetTensor(full);
    }
    return nullptr;
}

[[nodiscard]] bool checkedMulSizeT(std::size_t a, std::size_t b, std::size_t& out) noexcept
{
    if (a == 0 || b == 0)
    {
        out = 0;
        return true;
    }
    if (a > (std::numeric_limits<std::size_t>::max() / b))
    {
        return false;
    }
    out = a * b;
    return true;
}
}  // namespace

class RawrXDTransformer::MoEPrepackWorker
{
  public:
    MoEPrepackWorker(std::size_t maxDepth, std::uint32_t pollMs, RawrXDTransformer* owner)
        : m_cap(maxDepth > 0 ? maxDepth : 1u), m_pollMs(pollMs), m_owner(owner)
    {
        m_thread = std::thread([this] { threadMain_(); });
    }

    ~MoEPrepackWorker()
    {
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_stop = true;
        }
        m_cv.notify_one();
        if (m_thread.joinable())
            m_thread.join();
    }

    [[nodiscard]] bool tryPush(MoEPrepackJob&& job)
    {
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (m_q.size() >= m_cap)
                return false;
            m_q.push_back(std::move(job));
        }
        m_cv.notify_one();
        return true;
    }

    [[nodiscard]] std::size_t queueDepthApprox() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_q.size();
    }

  private:
    void threadMain_()
    {
        while (true)
        {
            MoEPrepackJob job;
            {
                std::unique_lock<std::mutex> lk(m_mu);
                const auto relTime = std::chrono::milliseconds(m_pollMs > 0 ? m_pollMs : 200u);
                m_cv.wait_for(lk, relTime, [this] { return m_stop || !m_q.empty(); });
                if (m_stop && m_q.empty())
                    return;
                if (m_q.empty())
                    continue;
                job = std::move(m_q.front());
                m_q.pop_front();
            }
            if (m_owner)
                m_owner->handleMoePrepackJob_(std::move(job));
        }
    }

    const std::size_t m_cap;
    const std::uint32_t m_pollMs;
    RawrXDTransformer* m_owner = nullptr;
    std::deque<MoEPrepackJob> m_q;
    mutable std::mutex m_mu;
    std::condition_variable m_cv;
    std::thread m_thread;
    bool m_stop = false;
};

void RawrXDTransformer::MoEPrepackWorkerDeleter::operator()(MoEPrepackWorker* p) const noexcept
{
    delete p;
}

RawrXDTransformer::~RawrXDTransformer()
{
    shutdownMoePrepackWorker_();
    if (m_swarmScheduler)
        m_swarmScheduler->setPlanRowEvictionObserver({});
}

void RawrXDTransformer::shutdownMoePrepackWorker_()
{
    m_moePrepackWorker.reset();
}

void RawrXDTransformer::startMoePrepackWorker_()
{
    if (!config.moe_down_grouped_async_prepack || !m_moeMixturePlanPackCache)
        return;
    const std::size_t depth = static_cast<std::size_t>(std::max(1, config.moe_down_grouped_prepack_queue_depth));
    const std::uint32_t pollMs = config.moe_down_grouped_prepack_thread_poll_ms;
    m_moePrepackWorker =
        std::unique_ptr<MoEPrepackWorker, MoEPrepackWorkerDeleter>(new MoEPrepackWorker(depth, pollMs, this));
}

void RawrXDTransformer::onSwarmPlanRowsEvicted_(std::span<const std::size_t> rows)
{
    if (!m_moeMixturePlanPackCache || rows.empty())
        return;
    const std::uint32_t n = m_moeMixturePlanPackCache->invalidateEntriesReferencingPlanRows(rows);
    m_moePackEvictedByPlanRow += static_cast<std::uint64_t>(n);
    if (n > 0)
    {
        char buf[160];
        (void)std::snprintf(buf, sizeof(buf),
                            "MoE mixture pack cache: invalidated %u entries on working-set slice eviction (plan rows)",
                            static_cast<unsigned>(n));
        RawrXD::Logging::Logger::instance().warning(std::string(buf), "MoE");
    }
}

void RawrXDTransformer::installSwarmPlanRowEvictionObserver_()
{
    if (!m_swarmScheduler)
        return;
    m_swarmScheduler->setPlanRowEvictionObserver([this](std::span<const std::size_t> planRows)
                                                 { onSwarmPlanRowsEvicted_(planRows); });
}

void RawrXDTransformer::handleMoePrepackJob_(MoEPrepackJob&& job)
{
    if (!loader || !m_moeMixturePlanPackCache)
        return;
    if (m_moeMixturePlanPackCache->contains(job.cacheKey))
        return;
    if (m_swarmScheduler && !job.planRows.empty())
    {
        if (!m_swarmScheduler->arePlanRowsResident(
                std::span<const std::size_t>(job.planRows.data(), job.planRows.size())))
        {
            ++m_moePrepackSkippedNotResident;
            return;
        }
    }
    const std::span<const std::size_t> pr =
        job.planRows.empty() ? std::span<const std::size_t>{} : std::span<const std::size_t>(job.planRows);
    if (tryMoeSyncPackMixtureIntoCache(job.layer, job.blkPrefix, job.expertPick, job.hdim, job.dim, job.cacheKey, pr))
        ++m_moePrepackInserts;
}

void RawrXDTransformer::tryEnqueueMoePrepack_(MoEPrepackJob&& job)
{
    if (!m_moePrepackWorker)
        return;
    if (!m_moePrepackWorker->tryPush(std::move(job)))
        ++m_moePrepackQueueDropped;
}

std::size_t RawrXDTransformer::moePrepackQueueDepthApprox() const noexcept
{
    if (!m_moePrepackWorker)
        return 0;
    return m_moePrepackWorker->queueDepthApprox();
}

std::size_t RawrXDTransformer::moeMixturePackCacheCurrentPackedBytes() const noexcept
{
    if (!m_moeMixturePlanPackCache)
        return 0;
    return m_moeMixturePlanPackCache->currentPackedBytes();
}

std::uint64_t RawrXDTransformer::moeMixturePackCacheEvictions() const noexcept
{
    if (!m_moeMixturePlanPackCache)
        return 0;
    return m_moeMixturePlanPackCache->evictions();
}

std::uint64_t RawrXDTransformer::moeMixturePackCacheSelectiveRowInvalidations() const noexcept
{
    if (!m_moeMixturePlanPackCache)
        return 0;
    return m_moeMixturePlanPackCache->rowInvalidationEvictions();
}

void RawrXDTransformer::SetSwarmScheduler(RawrXD::Swarm::ISwarmScheduler* scheduler)
{
    if (m_swarmScheduler)
        m_swarmScheduler->setPlanRowEvictionObserver({});
    m_swarmScheduler = scheduler;
    m_swarmPinningSuppressed = false;
    m_swarmPinFailureStreak = 0;
    refreshSwarmPlanSliceIndex();
    installSwarmPlanRowEvictionObserver_();
}

void RawrXDTransformer::refreshSwarmPlanSliceIndex()
{
    if (m_swarmScheduler)
        m_swarmPlanSliceIndex = m_swarmScheduler->planSliceIndexSnapshot();
    else
        m_swarmPlanSliceIndex.clear();
}

RawrXD::MoEDownProject::Path RawrXDTransformer::chooseMoEDownProjectPath(const std::uint32_t layer,
                                                                         const std::size_t inDim,
                                                                         const std::size_t outDim,
                                                                         const int numExperts) const noexcept
{
    RawrXD::MoEDownProject::PolicyParams p;
    p.workThreshold = config.moe_down_work_threshold;
    p.minReuseForGrouped = config.moe_down_min_reuse_for_grouped;
    const double dynamicReuse = moeLayerExpectedReuse(layer);
    const RawrXD::MoEDownProject::Path chosen =
        RawrXD::MoEDownProject::choosePath(inDim, outDim, numExperts, dynamicReuse, p);
    if (config.moe_down_log_policy_divergence)
    {
        const RawrXD::MoEDownProject::Path staticChoice =
            RawrXD::MoEDownProject::choosePath(inDim, outDim, numExperts, config.moe_down_expected_reuse_estimate, p);
        if (staticChoice != chosen)
            tryEmitMoEPathDivergenceLog(layer, inDim, outDim, numExperts, dynamicReuse, chosen, staticChoice);
    }
    return chosen;
}

void RawrXDTransformer::tryEmitMoEPathDivergenceLog(const std::uint32_t layer, const std::size_t inDim,
                                                    const std::size_t outDim, const int numExperts,
                                                    const double dynamicReuse,
                                                    const RawrXD::MoEDownProject::Path chosen,
                                                    const RawrXD::MoEDownProject::Path staticChoice) const
{
    const int cap = std::max(0, config.moe_down_policy_divergence_logs_per_minute_cap);
    if (cap == 0)
        return;

    const int nl = config.n_layers;
    if (nl <= 0 || layer >= static_cast<std::uint32_t>(nl))
        return;
    const std::size_t li = static_cast<std::size_t>(layer);
    if (m_moePolicyDivLastPlanGenPerLayer.size() <= li)
        return;

    const std::uint64_t planGen = m_swarmScheduler ? m_swarmScheduler->planGeneration() : 0ull;
    if (m_moePolicyDivLastPlanGenPerLayer[li] == planGen)
        return;

    const auto now = std::chrono::steady_clock::now();
    const std::uint64_t nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    if (m_moePolicyDivRateWindowStartMs == 0ull || (nowMs - m_moePolicyDivRateWindowStartMs) > 60000ull)
    {
        m_moePolicyDivRateWindowStartMs = nowMs;
        m_moePolicyDivLogsInWindow = 0;
    }
    if (m_moePolicyDivLogsInWindow >= static_cast<unsigned>(cap))
        return;

    const std::uint32_t emaSamples = (m_moeReuseSampleCount.size() > li) ? m_moeReuseSampleCount[li] : 0u;
    const double work = RawrXD::MoEDownProject::workProduct(inDim, outDim, numExperts);

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "[MoEPathDiverge] planGen=%llu layer=%u D=%zu H=%zu K=%d work=%.6g R_dyn=%.6g chosen=%s "
                  "staticR=%.6g staticChoice=%s emaSamples=%u tsMs=%llu policy_divergence_count=%llu",
                  static_cast<unsigned long long>(planGen), static_cast<unsigned>(layer), inDim, outDim, numExperts,
                  work, dynamicReuse, moeDownProjectPathTag(chosen), config.moe_down_expected_reuse_estimate,
                  moeDownProjectPathTag(staticChoice), static_cast<unsigned>(emaSamples),
                  static_cast<unsigned long long>(nowMs),
                  static_cast<unsigned long long>(m_moePolicyDivergenceLogCount + 1ull));

    RawrXD::Logging::Logger::instance().debug(std::string(buf), "MoE");
    ++m_moePolicyDivergenceLogCount;
    ++m_moePolicyDivLogsInWindow;
    m_moePolicyDivLastPlanGenPerLayer[li] = planGen;
}

double RawrXDTransformer::moeDownProjectExpectedReuse(const std::uint32_t layer) const noexcept
{
    return moeLayerExpectedReuse(layer);
}

double RawrXDTransformer::moeLayerExpectedReuse(const std::uint32_t layer) const noexcept
{
    const int nl = config.n_layers;
    if (nl <= 0 || layer >= static_cast<std::uint32_t>(nl))
        return config.moe_down_expected_reuse_estimate;
    if (!config.moe_down_reuse_from_heatmap || m_moeReuseSampleCount.size() != static_cast<std::size_t>(nl) ||
        m_moeReuseSampleCount[static_cast<std::size_t>(layer)] <
            static_cast<std::uint32_t>(std::max(0, config.moe_down_reuse_min_samples)))
        return config.moe_down_expected_reuse_estimate;

    const double ema = m_moeReuseResidentRatioEma[static_cast<std::size_t>(layer)];
    const double rMax = config.moe_down_reuse_r_max > 0.0 ? config.moe_down_reuse_r_max : 16.0;
    const double r = std::floor(ema * rMax);
    return std::max(1.0, r);
}

void RawrXDTransformer::maybeSampleMoEReuseFromHeatmap()
{
    ++m_moeReuseForwardTick;

    if (m_swarmScheduler && m_moeMixturePlanPackCache)
    {
        const std::uint64_t pg = m_swarmScheduler->planGeneration();
        if (pg != m_moeMixturePackCachePlanGeneration)
        {
            m_moeMixturePackCachePlanGeneration = pg;
            m_moeMixturePlanPackCache->clearEntriesOnly();
        }
    }

    if (!m_swarmScheduler || !config.moe_down_reuse_from_heatmap)
        return;

    const int stride = std::max(1, config.moe_down_reuse_snapshot_stride);
    if ((m_moeReuseForwardTick % static_cast<std::uint64_t>(stride)) != 0ull)
        return;

    const int nl = config.n_layers;
    if (nl <= 0 || m_moeReuseResidentRatioEma.size() != static_cast<std::size_t>(nl))
        return;

    RawrXD::Swarm::ExpertHeatmapCaptureParams cap;
    cap.modelIndex = config.moe_down_reuse_heatmap_all_models
                         ? RawrXD::Swarm::kExpertHeatmapAllModels
                         : static_cast<std::uint32_t>(std::max(0, config.moe_down_reuse_heatmap_model_index));
    cap.expertsOnly = true;
    cap.planRowStride = 1;

    RawrXD::Swarm::ExpertHeatmapSnapshot snap;
    if (!m_swarmScheduler->captureExpertHeatmapSnapshot(cap, snap))
        return;

    if (snap.planGeneration != m_moeReuseTrackedPlanGeneration)
    {
        m_moeReuseTrackedPlanGeneration = snap.planGeneration;
        std::fill(m_moeReuseResidentRatioEma.begin(), m_moeReuseResidentRatioEma.end(), 0.0);
        std::fill(m_moeReuseSampleCount.begin(), m_moeReuseSampleCount.end(), 0u);
        if (!m_moePolicyDivLastPlanGenPerLayer.empty())
            std::fill(m_moePolicyDivLastPlanGenPerLayer.begin(), m_moePolicyDivLastPlanGenPerLayer.end(),
                      std::numeric_limits<std::uint64_t>::max());
    }

    std::vector<std::uint32_t> resident(static_cast<std::size_t>(nl), 0u);
    std::vector<std::uint32_t> sampled(static_cast<std::size_t>(nl), 0u);
    for (const RawrXD::Swarm::ExpertHeatmapCell& c : snap.cells)
    {
        if (c.expertIndex == 0xFFFFFFFFu)
            continue;
        const std::uint32_t ls = c.layerStart;
        const std::uint32_t le = c.layerEnd < ls ? ls : c.layerEnd;
        for (std::uint32_t L = ls; L <= le && static_cast<int>(L) < nl; ++L)
        {
            const std::size_t li = static_cast<std::size_t>(L);
            ++sampled[li];
            if (c.resident)
                ++resident[li];
        }
    }

    const double alpha = std::clamp(config.moe_down_reuse_ema_alpha, 0.0, 1.0);
    const int minCells = std::max(1, config.moe_down_reuse_min_expert_cells_per_layer);

    for (int L = 0; L < nl; ++L)
    {
        const std::size_t li = static_cast<std::size_t>(L);
        if (sampled[li] < static_cast<std::uint32_t>(minCells))
            continue;
        const double r = static_cast<double>(resident[li]) / static_cast<double>(sampled[li]);
        m_moeReuseResidentRatioEma[li] = alpha * r + (1.0 - alpha) * m_moeReuseResidentRatioEma[li];
        if (m_moeReuseSampleCount[li] < std::numeric_limits<std::uint32_t>::max())
            ++m_moeReuseSampleCount[li];
    }

    if (config.moe_down_enable_grouped_integration && config.moe_down_grouped_async_prepack &&
        config.moe_down_grouped_prepack_hint_from_heatmap && m_moeMixturePlanPackCache && m_moePrepackWorker)
    {
        const int maxHints = std::max(0, config.moe_down_grouped_prepack_heatmap_max_hints_per_tick);
        int hints = 0;
        constexpr std::uint32_t kModel0 = 0u;
        for (const RawrXD::Swarm::ExpertHeatmapCell& c : snap.cells)
        {
            if (hints >= maxHints)
                break;
            if (c.expertIndex == 0xFFFFFFFFu || !c.resident || c.prefetchInFlight)
                continue;
            if (static_cast<int>(c.layerStart) < 0 || static_cast<int>(c.layerStart) >= nl)
                continue;
            const std::uint32_t L = c.layerStart;
            std::vector<std::uint32_t> pick = {c.expertIndex};
            const std::string mixKey = makeMoeMixturePackCacheKey(kModel0, L, pick, m_swarmPlanSliceIndex);
            if (m_moeMixturePlanPackCache->contains(mixKey))
                continue;
            MoEPrepackJob j;
            j.layer = L;
            j.blkPrefix = "blk." + std::to_string(L) + ".";
            j.expertPick = std::move(pick);
            j.hdim = config.hidden_dim;
            j.dim = config.dim;
            j.cacheKey = mixKey;
            collectMoeMixturePlanRowRefs(kModel0, L, j.expertPick, m_swarmPlanSliceIndex, j.planRows);
            tryEnqueueMoePrepack_(std::move(j));
            ++hints;
        }
    }
}

std::expected<void, RawrXD::Swarm::SchedulerError> RawrXDTransformer::pinSwarmSlicesForLayer(
    const std::uint32_t modelIndex, const std::uint32_t layer, const std::uint32_t* activeExpertOrdinals,
    const std::size_t activeExpertOrdinalCount, std::vector<std::size_t>& outPinnedPlanRows,
    const SwarmPinLayerParts parts, const bool appendPinnedRows)
{
    if (!appendPinnedRows)
        outPinnedPlanRows.clear();
    if (!m_swarmScheduler || m_swarmPinningSuppressed)
        return {};

    constexpr std::uint32_t kStaticExpert = 0xFFFFFFFFu;
    std::vector<std::size_t> rows;
    rows.reserve(64);

    const auto appendGroup = [this, modelIndex, layer, &rows](const std::uint32_t expertOrd)
    {
        for (const std::size_t r : m_swarmPlanSliceIndex.indicesFor(modelIndex, layer, expertOrd))
            rows.push_back(r);
    };

    if (parts == SwarmPinLayerParts::Full || parts == SwarmPinLayerParts::StaticOnly)
        appendGroup(kStaticExpert);

    std::vector<std::uint32_t> presentExperts;
    presentExperts.reserve(32);
    constexpr std::uint32_t kMaxExpertOrdinalScan = 256u;
    for (std::uint32_t e = 0; e < kMaxExpertOrdinalScan; ++e)
    {
        if (!m_swarmPlanSliceIndex.indicesFor(modelIndex, layer, e).empty())
            presentExperts.push_back(e);
    }

    const bool wantExperts = (parts == SwarmPinLayerParts::Full || parts == SwarmPinLayerParts::ExpertsOnly);
    if (wantExperts && !presentExperts.empty())
    {
        std::vector<std::uint32_t> pick;
        pick.reserve(presentExperts.size());
        if (activeExpertOrdinalCount != 0 && activeExpertOrdinals != nullptr)
        {
            pick.assign(activeExpertOrdinals, activeExpertOrdinals + activeExpertOrdinalCount);
        }
        else if (config.moe_pin_top_k > 0)
        {
            const int cap = std::min(config.moe_pin_top_k, static_cast<int>(presentExperts.size()));
            for (int i = 0; i < cap; ++i)
                pick.push_back(presentExperts[static_cast<std::size_t>(i)]);
        }
        else
        {
            pick = presentExperts;
        }

        for (const std::uint32_t e : pick)
            appendGroup(e);
    }

    if (rows.empty())
        return {};

    const auto pinned = m_swarmScheduler->pinPlanRows(std::span<const std::size_t>(rows.data(), rows.size()));
    if (!pinned)
    {
        if (m_swarmPinFailureStreak < std::numeric_limits<std::uint32_t>::max())
            ++m_swarmPinFailureStreak;
        constexpr std::uint32_t kPinFailureTrip = 8u;
        if (!m_swarmPinningSuppressed && m_swarmPinFailureStreak >= kPinFailureTrip)
        {
            m_swarmPinningSuppressed = true;
            std::printf("[Forward] WARN: suppressing swarm pin hooks after %u consecutive pin failures\n",
                        static_cast<unsigned>(m_swarmPinFailureStreak));
        }
        return pinned;
    }
    m_swarmPinFailureStreak = 0;
    outPinnedPlanRows.insert(outPinnedPlanRows.end(), rows.begin(), rows.end());
    return {};
}

bool RawrXDTransformer::swarmLayerHasExpertSlices(const std::uint32_t layer) const
{
    if (!loader || loader->getExperts() <= 0)
        return false;
    constexpr std::uint32_t kMaxExpertOrdinalScan = 256u;
    const int nExp = loader->getExperts();
    const std::uint32_t limit = static_cast<std::uint32_t>(std::min(nExp, static_cast<int>(kMaxExpertOrdinalScan)));
    for (std::uint32_t e = 0; e < limit; ++e)
    {
        if (!m_swarmPlanSliceIndex.indicesFor(0u, layer, e).empty())
            return true;
    }
    return false;
}

bool RawrXDTransformer::tryPickMoERouterExperts(const std::uint32_t layer, const float* ffnNormedHidden,
                                                std::vector<std::uint32_t>& outExpertOrdinals,
                                                std::vector<float>& outMixtureWeights)
{
    outExpertOrdinals.clear();
    outMixtureWeights.clear();
    if (!loader || loader->getExperts() <= 0 || !ffnNormedHidden)
        return false;
    const int dim = config.dim;
    if (dim <= 0 || static_cast<std::size_t>(dim) % 256u != 0)
        return false;

    const int nExp = loader->getExperts();
    int kPick = loader->getExpertsUsedCount();
    if (kPick <= 0)
        kPick = std::min(2, std::max(1, nExp));
    kPick = std::min(kPick, nExp);
    if (config.moe_pin_top_k > 0)
        kPick = std::min(kPick, config.moe_pin_top_k);

    std::vector<std::uint32_t> presentExperts;
    presentExperts.reserve(32);
    constexpr std::uint32_t kMaxScan = 256u;
    for (std::uint32_t e = 0; e < kMaxScan; ++e)
    {
        if (!m_swarmPlanSliceIndex.indicesFor(0u, layer, e).empty())
            presentExperts.push_back(e);
    }
    if (presentExperts.empty())
        return false;

    const std::string prefix = "blk." + std::to_string(layer) + ".";
    static const char* kRouterSuffix[] = {"ffn_gate_inp.weight", "ffn_gate_inp"};
    std::vector<float> logits(static_cast<std::size_t>(nExp));
    bool routed = false;
    for (const char* suf : kRouterSuffix)
    {
        const std::string name = prefix + suf;
        if (!loader->hasTensorNamed(name))
            continue;
        if (loader->StreamingMatMul(name, ffnNormedHidden, logits.data(), static_cast<std::size_t>(dim),
                                    static_cast<std::size_t>(nExp)))
        {
            routed = true;
            break;
        }
    }
    if (!routed)
        return false;

    std::vector<int> order(static_cast<std::size_t>(nExp));
    for (int i = 0; i < nExp; ++i)
        order[static_cast<std::size_t>(i)] = i;
    const int kTake = std::min(kPick, nExp);
    std::partial_sort(order.begin(), order.begin() + kTake, order.end(), [&logits](int a, int b)
                      { return logits[static_cast<std::size_t>(a)] > logits[static_cast<std::size_t>(b)]; });

    std::vector<char> seen(static_cast<std::size_t>(nExp), 0);
    for (const std::uint32_t pe : presentExperts)
    {
        if (pe < static_cast<std::uint32_t>(nExp))
            seen[pe] = 1;
    }

    for (int i = 0; i < kTake; ++i)
    {
        const int e = order[static_cast<std::size_t>(i)];
        if (e < 0 || e >= nExp)
            continue;
        if (!seen[static_cast<std::size_t>(e)])
            continue;
        outExpertOrdinals.push_back(static_cast<std::uint32_t>(e));
    }

    for (const std::uint32_t pe : presentExperts)
    {
        if (static_cast<int>(outExpertOrdinals.size()) >= kPick)
            break;
        if (std::find(outExpertOrdinals.begin(), outExpertOrdinals.end(), pe) != outExpertOrdinals.end())
            continue;
        outExpertOrdinals.push_back(pe);
    }

    if (outExpertOrdinals.empty())
        return false;

    std::vector<float> probs(static_cast<std::size_t>(nExp));
    for (int i = 0; i < nExp; ++i)
        probs[static_cast<std::size_t>(i)] = logits[static_cast<std::size_t>(i)];
    {
        float mx = probs[0];
        for (int i = 1; i < nExp; ++i)
            mx = std::max(mx, probs[static_cast<std::size_t>(i)]);
        double sumAll = 0;
        for (int i = 0; i < nExp; ++i)
        {
            const float e = std::exp(static_cast<double>(probs[static_cast<std::size_t>(i)] - mx));
            probs[static_cast<std::size_t>(i)] = static_cast<float>(e);
            sumAll += e;
        }
        if (sumAll <= 1e-20)
        {
            const float inv = 1.f / static_cast<float>(std::max(1, nExp));
            for (int i = 0; i < nExp; ++i)
                probs[static_cast<std::size_t>(i)] = inv;
        }
        else
        {
            const float inv = static_cast<float>(1.0 / sumAll);
            for (int i = 0; i < nExp; ++i)
                probs[static_cast<std::size_t>(i)] *= inv;
        }
    }

    outMixtureWeights.reserve(outExpertOrdinals.size());
    float sumSel = 0.f;
    for (const std::uint32_t e : outExpertOrdinals)
    {
        const float p = (e < static_cast<std::uint32_t>(nExp)) ? probs[static_cast<std::size_t>(e)] : 0.f;
        outMixtureWeights.push_back(p);
        sumSel += p;
    }
    if (sumSel > 1e-12f)
    {
        const float inv = 1.f / sumSel;
        for (float& w : outMixtureWeights)
            w *= inv;
    }
    else
    {
        const float u = 1.f / static_cast<float>(outMixtureWeights.size());
        for (float& w : outMixtureWeights)
            w = u;
    }

    return true;
}

#ifdef __AVX512F__
#include <immintrin.h>
#endif

// C++ Implementations of Kernels (Ensuring Real Logic Execution)
#ifdef __AVX512F__
void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N)
{
#pragma omp parallel for collapse(2)
    for (uint64_t i = 0; i < M; i++)
    {
        for (uint64_t j = 0; j < N; j++)
        {
            __m512 sum_vec = _mm512_setzero_ps();
            uint64_t k = 0;
            for (; k + 15 < K; k += 16)
            {
                __m512 a_vec = _mm512_loadu_ps(A + i * K + k);
                __m512 b_vec = _mm512_loadu_ps(B + j * K + k);
                sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
            }
            float sum = _mm512_reduce_add_ps(sum_vec);
            for (; k < K; k++)
            {
                sum += A[i * K + k] * B[j * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}
#else
void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N)
{
#pragma omp parallel for collapse(2)
    for (uint64_t i = 0; i < M; i++)
    {
        for (uint64_t j = 0; j < N; j++)
        {
            float sum = 0.0f;
            for (uint64_t k = 0; k < K; k++)
            {
                sum += A[i * K + k] * B[j * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}
#endif

#ifdef __AVX512F__
void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps)
{
    if (!out || !in || !weight || size <= 0)
        return;
    if (!std::isfinite(eps) || eps <= 0.0f)
        eps = 1e-5f;

    __m512 sum_vec = _mm512_setzero_ps();
    int i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 in_vec = _mm512_loadu_ps(in + i);
        sum_vec = _mm512_fmadd_ps(in_vec, in_vec, sum_vec);
    }
    float ss = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++)
    {
        ss += in[i] * in[i];
    }
    ss /= size;
    ss += eps;
    if (!std::isfinite(ss) || ss <= 0.0f)
        ss = eps;
    float inv_rms = 1.0f / sqrtf(ss);
    if (!std::isfinite(inv_rms) || inv_rms <= 0.0f)
        inv_rms = 1.0f;

    __m512 inv_rms_vec = _mm512_set1_ps(inv_rms);
    i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 in_vec = _mm512_loadu_ps(in + i);
        __m512 weight_vec = _mm512_loadu_ps(weight + i);
        __m512 out_vec = _mm512_mul_ps(_mm512_mul_ps(in_vec, weight_vec), inv_rms_vec);
        _mm512_storeu_ps(out + i, out_vec);
    }
    for (; i < size; i++)
    {
        out[i] = in[i] * weight[i] * inv_rms;
    }
}
#else
void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps)
{
    if (!out || !in || !weight || size <= 0)
        return;
    if (!std::isfinite(eps) || eps <= 0.0f)
        eps = 1e-5f;

    float ss = 0.0f;
    for (int i = 0; i < size; i++)
    {
        ss += in[i] * in[i];
    }
    ss /= size;
    ss += eps;
    if (!std::isfinite(ss) || ss <= 0.0f)
        ss = eps;
    float inv_rms = 1.0f / sqrtf(ss);
    if (!std::isfinite(inv_rms) || inv_rms <= 0.0f)
        inv_rms = 1.0f;
    for (int i = 0; i < size; i++)
    {
        out[i] = in[i] * weight[i] * inv_rms;
    }
}
#endif

// =============================================================================
// Portable AVX-512 exp approximation — replaces SVML _mm512_exp_ps
// 6th-order minimax polynomial on [-87.3, 88.7], ~1e-6 relative error.
// Works on ANY AVX-512F capable compiler (GCC, Clang, MSVC) without SVML.
// =============================================================================
#ifdef __AVX512F__
static inline __m512 _rawrxd_exp_ps_avx512(__m512 x)
{
    // Clamp to prevent overflow/underflow
    const __m512 hi = _mm512_set1_ps(88.3762626647949f);
    const __m512 lo = _mm512_set1_ps(-87.3365447504f);
    x = _mm512_min_ps(x, hi);
    x = _mm512_max_ps(x, lo);

    // exp(x) = 2^(x * log2(e))  →  2^(n + f)  where n=floor, f=fraction
    const __m512 log2e = _mm512_set1_ps(1.44269504088896341f);
    const __m512 half = _mm512_set1_ps(0.5f);
    __m512 t = _mm512_fmadd_ps(x, log2e, half);  // t = x*log2e + 0.5

    // n = floor(t)
    __m512 n = _mm512_roundscale_ps(t, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);

    // f = x - n * ln(2)
    const __m512 c1 = _mm512_set1_ps(0.693359375f);     // ln(2) upper
    const __m512 c2 = _mm512_set1_ps(-2.12194440e-4f);  // ln(2) lower (Cody-Waite)
    __m512 f = _mm512_fnmadd_ps(n, c1, x);
    f = _mm512_fnmadd_ps(n, c2, f);

    // Polynomial approximation of 2^f - 1 on [0, ln2)
    const __m512 p0 = _mm512_set1_ps(1.0f);
    const __m512 p1 = _mm512_set1_ps(1.0f);
    const __m512 p2 = _mm512_set1_ps(0.5f);
    const __m512 p3 = _mm512_set1_ps(0.1666666666666f);
    const __m512 p4 = _mm512_set1_ps(0.0416666666666f);
    const __m512 p5 = _mm512_set1_ps(0.0083333333333f);
    const __m512 p6 = _mm512_set1_ps(0.0013888888888f);

    __m512 y = _mm512_fmadd_ps(p6, f, p5);
    y = _mm512_fmadd_ps(y, f, p4);
    y = _mm512_fmadd_ps(y, f, p3);
    y = _mm512_fmadd_ps(y, f, p2);
    y = _mm512_fmadd_ps(y, f, p1);
    y = _mm512_fmadd_ps(y, f, p0);

    // Scale by 2^n: convert n to integer, shift into float exponent bits
    __m512i ni = _mm512_cvtps_epi32(n);
    ni = _mm512_slli_epi32(ni, 23);  // Shift into exponent field
    __m512 pow2n = _mm512_castsi512_ps(_mm512_add_epi32(ni, _mm512_set1_epi32(0x3F800000)));

    return _mm512_mul_ps(y, pow2n);
}

void Softmax_AVX512(float* x, int size)
{
    if (!x || size <= 0)
        return;
    if (size == 1)
    {
        x[0] = 1.0f;
        return;
    }
    for (int i = 0; i < size; ++i)
    {
        if (!std::isfinite(x[i]))
            x[i] = -1e9f;
    }

    float max_val = x[0];
    int i = 1;
    if (size >= 16)
    {
        __m512 max_vec = _mm512_loadu_ps(x);
        i = 16;
        for (; i + 15 < size; i += 16)
        {
            __m512 curr_vec = _mm512_loadu_ps(x + i);
            max_vec = _mm512_max_ps(max_vec, curr_vec);
        }
        max_val = _mm512_reduce_max_ps(max_vec);
    }
    for (; i < size; i++)
    {
        if (x[i] > max_val)
            max_val = x[i];
    }

    __m512 max_val_vec = _mm512_set1_ps(max_val);
    __m512 sum_vec = _mm512_setzero_ps();
    i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        curr_vec = _mm512_sub_ps(curr_vec, max_val_vec);
        curr_vec = _rawrxd_exp_ps_avx512(curr_vec);  // Portable replacement
        _mm512_storeu_ps(x + i, curr_vec);
        sum_vec = _mm512_add_ps(sum_vec, curr_vec);
    }
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++)
    {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    if (!std::isfinite(sum) || sum <= 0.0f)
    {
        const float uni = 1.0f / static_cast<float>(size);
        for (int j = 0; j < size; ++j)
            x[j] = uni;
        return;
    }

    __m512 sum_inv_vec = _mm512_set1_ps(1.0f / sum);
    i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        curr_vec = _mm512_mul_ps(curr_vec, sum_inv_vec);
        _mm512_storeu_ps(x + i, curr_vec);
    }
    for (; i < size; i++)
    {
        x[i] /= sum;
    }
}
#else
void Softmax_AVX512(float* x, int size)
{
    if (!x || size <= 0)
        return;
    if (size == 1)
    {
        x[0] = 1.0f;
        return;
    }
    for (int i = 0; i < size; ++i)
    {
        if (!std::isfinite(x[i]))
            x[i] = -1e9f;
    }
    float max_val = x[0];
    for (int i = 1; i < size; i++)
    {
        if (x[i] > max_val)
            max_val = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    if (!std::isfinite(sum) || sum <= 0.0f)
    {
        const float uni = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; ++i)
            x[i] = uni;
        return;
    }
    for (int i = 0; i < size; i++)
    {
        x[i] /= sum;
    }
}
#endif

void RoPE_AVX512(float* q, float* k, int pos, int head_dim, int num_heads)
{
    // Simple scalar implementation of RoPE
    for (int h = 0; h < num_heads; h++)
    {
        for (int i = 0; i < head_dim; i += 2)
        {
            float theta = powf(10000.0f, -float(i) / head_dim);
            float alpha = pos * theta;
            float cos_a = cosf(alpha);
            float sin_a = sinf(alpha);

            float* q_ptr = q + h * head_dim + i;
            float* k_ptr = k ? (k + h * head_dim + i) : nullptr;

            float q0 = q_ptr[0];
            float q1 = q_ptr[1];
            q_ptr[0] = q0 * cos_a - q1 * sin_a;
            q_ptr[1] = q0 * sin_a + q1 * cos_a;

            if (k)
            {  // Support cases where k is rotated separately
                float k0 = k_ptr[0];
                float k1 = k_ptr[1];
                k_ptr[0] = k0 * cos_a - k1 * sin_a;
                k_ptr[1] = k0 * sin_a + k1 * cos_a;
            }
        }
    }
}

#ifdef __AVX512F__
void Silu_AVX512(float* x, int size)
{
    int i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 val_vec = _mm512_loadu_ps(x + i);
        __m512 neg_val_vec = _mm512_sub_ps(_mm512_setzero_ps(), val_vec);
        __m512 exp_vec = _rawrxd_exp_ps_avx512(neg_val_vec);  // Portable replacement
        __m512 one_vec = _mm512_set1_ps(1.0f);
        __m512 denom_vec = _mm512_add_ps(one_vec, exp_vec);
        __m512 result_vec = _mm512_div_ps(val_vec, denom_vec);
        _mm512_storeu_ps(x + i, result_vec);
    }
    for (; i < size; i++)
    {
        float val = x[i];
        x[i] = val / (1.0f + expf(-val));
    }
}
#else
void Silu_AVX512(float* x, int size)
{
    for (int i = 0; i < size; i++)
    {
        float val = x[i];
        x[i] = val / (1.0f + expf(-val));
    }
}
#endif

// AVX-512 optimized dot product
#ifdef __AVX512F__
float DotProduct_AVX512(const float* a, const float* b, int size)
{
    __m512 sum_vec = _mm512_setzero_ps();
    int i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 a_vec = _mm512_loadu_ps(a + i);
        __m512 b_vec = _mm512_loadu_ps(b + i);
        sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
    }
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++)
    {
        sum += a[i] * b[i];
    }
    return sum;
}
#else
float DotProduct_AVX512(const float* a, const float* b, int size)
{
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        sum += a[i] * b[i];
    }
    return sum;
}
#endif

// AVX-512 optimized vector addition with scalar multiplier
#ifdef __AVX512F__
void VectorAddScaled_AVX512(float* out, const float* in, float scale, int size)
{
    __m512 scale_vec = _mm512_set1_ps(scale);
    int i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 out_vec = _mm512_loadu_ps(out + i);
        __m512 in_vec = _mm512_loadu_ps(in + i);
        __m512 scaled_vec = _mm512_mul_ps(in_vec, scale_vec);
        out_vec = _mm512_add_ps(out_vec, scaled_vec);
        _mm512_storeu_ps(out + i, out_vec);
    }
    for (; i < size; i++)
    {
        out[i] += scale * in[i];
    }
}
#else
void VectorAddScaled_AVX512(float* out, const float* in, float scale, int size)
{
    for (int i = 0; i < size; i++)
    {
        out[i] += scale * in[i];
    }
}
#endif

// AVX-512 optimized vector addition
#ifdef __AVX512F__
void VectorAdd_AVX512(float* out, const float* a, const float* b, int size)
{
    int i = 0;
    for (; i + 15 < size; i += 16)
    {
        __m512 a_vec = _mm512_loadu_ps(a + i);
        __m512 b_vec = _mm512_loadu_ps(b + i);
        __m512 sum_vec = _mm512_add_ps(a_vec, b_vec);
        _mm512_storeu_ps(out + i, sum_vec);
    }
    for (; i < size; i++)
    {
        out[i] = a[i] + b[i];
    }
}
#else
void VectorAdd_AVX512(float* out, const float* a, const float* b, int size)
{
    for (int i = 0; i < size; i++)
    {
        out[i] = a[i] + b[i];
    }
}
#endif

namespace
{
/// Shared MoE expert gate/up/SiLU path; optional down into \p finalFfn when \p applyDown.
[[nodiscard]] bool forwardMoEExpertSwiGLUImpl(RawrXDModelLoader* loader, const std::string& blkPrefix,
                                              std::uint32_t expert, float* xNormed, std::vector<float>& h1,
                                              std::vector<float>& h3, std::vector<float>* finalFfn, int dim, int hdim,
                                              bool applyDown)
{
    if (!loader)
        return false;
    const std::string ep = blkPrefix + "ffn_experts." + std::to_string(expert) + ".";
    const std::array<std::array<const char*, 3>, 2> kPat = {{
        {"ffn_gate.weight", "ffn_up.weight", "ffn_down.weight"},
        {"w1.weight", "w3.weight", "w2.weight"},
    }};
    for (const auto& tri : kPat)
    {
        const std::string g = ep + tri[0];
        const std::string u = ep + tri[1];
        const std::string d = ep + tri[2];
        if (!loader->hasTensorNamed(g) || !loader->hasTensorNamed(u) || !loader->hasTensorNamed(d))
            continue;
        if (!loader->StreamingMatMul(g, xNormed, h1.data(), static_cast<size_t>(dim), static_cast<size_t>(hdim)))
            continue;
        if (!loader->StreamingMatMul(u, xNormed, h3.data(), static_cast<size_t>(dim), static_cast<size_t>(hdim)))
            continue;
        Silu_AVX512(h1.data(), hdim);
#ifdef __AVX512F__
        {
            int i = 0;
            for (; i + 15 < hdim; i += 16)
            {
                __m512 h1v = _mm512_loadu_ps(h1.data() + i);
                __m512 h3v = _mm512_loadu_ps(h3.data() + i);
                _mm512_storeu_ps(h1.data() + i, _mm512_mul_ps(h1v, h3v));
            }
            for (; i < hdim; i++)
                h1[i] *= h3[i];
        }
#else
        for (int i = 0; i < hdim; i++)
            h1[i] *= h3[i];
#endif
        if (!applyDown)
            return true;
        if (!finalFfn)
            return false;
        if (!loader->StreamingMatMul(d, h1.data(), finalFfn->data(), static_cast<size_t>(hdim),
                                     static_cast<size_t>(dim)))
            continue;
        return true;
    }
    return false;
}

[[nodiscard]] bool forwardMoEExpertSwiGLUPostActivation(RawrXDModelLoader* loader, const std::string& blkPrefix,
                                                        std::uint32_t expert, float* xNormed, std::vector<float>& h1,
                                                        std::vector<float>& h3, int dim, int hdim)
{
    return forwardMoEExpertSwiGLUImpl(loader, blkPrefix, expert, xNormed, h1, h3, nullptr, dim, hdim, false);
}

/// One Mixtral-style expert: try `ffn_gate`/`ffn_up`/`ffn_down` then `w1`/`w3`/`w2` naming.
[[nodiscard]] bool forwardMoEExpertSwiGLU(RawrXDModelLoader* loader, const std::string& blkPrefix, std::uint32_t expert,
                                          float* xNormed, std::vector<float>& h1, std::vector<float>& h3,
                                          std::vector<float>& finalFfn, int dim, int hdim)
{
    return forwardMoEExpertSwiGLUImpl(loader, blkPrefix, expert, xNormed, h1, h3, &finalFfn, dim, hdim, true);
}
}  // namespace

void RawrXDTransformer::Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader* loader)
{
    this->device = device;
    this->config = cfg;
    this->loader = loader;

    if (config.n_layers <= 0 || config.n_heads <= 0 || config.dim <= 0)
    {
        printf("[RawrXD] FATAL: invalid transformer config (layers=%d heads=%d dim=%d)\n", config.n_layers,
               config.n_heads, config.dim);
        kv_cache_k.clear();
        kv_cache_v.clear();
        kv_cache_pos.clear();
        return;
    }
    if (config.dim % config.n_heads != 0)
    {
        printf("[RawrXD] FATAL: dim %d not divisible by n_heads %d\n", config.dim, config.n_heads);
        kv_cache_k.clear();
        kv_cache_v.clear();
        kv_cache_pos.clear();
        return;
    }

    // Initialize KV Cache — use seq_len if n_ctx wasn't set
    int ctx = config.n_ctx > 0 ? config.n_ctx : (config.seq_len > 0 ? config.seq_len : 2048);
    // Use n_kv_heads dimension for KV cache (GQA/MQA support)
    int kv_dim = (config.n_kv_heads > 0 ? config.n_kv_heads : config.n_heads) * (config.dim / config.n_heads);
    if (ctx <= 0 || kv_dim <= 0)
    {
        printf("[RawrXD] FATAL: invalid cache dimensions (ctx=%d kv_dim=%d)\n", ctx, kv_dim);
        kv_cache_k.clear();
        kv_cache_v.clear();
        kv_cache_pos.clear();
        return;
    }

    std::size_t kv_size = 0;
    std::size_t layCtx = 0;
    if (!checkedMulSizeT(static_cast<std::size_t>(config.n_layers), static_cast<std::size_t>(ctx), layCtx) ||
        !checkedMulSizeT(layCtx, static_cast<std::size_t>(kv_dim), kv_size))
    {
        printf("[RawrXD] FATAL: KV cache size overflow risk (layers=%d ctx=%d kv_dim=%d)\n", config.n_layers, ctx,
               kv_dim);
        kv_cache_k.clear();
        kv_cache_v.clear();
        kv_cache_pos.clear();
        return;
    }
    // Hard cap: never allocate more than 256 MB per K/V buffer to stay within
    // process memory budgets on systems with aperture reservation constraints.
    constexpr std::size_t kMaxKvBytesPerBuffer = 256ull * 1024 * 1024;
    const std::size_t kMaxKvFloatsPerBuffer = kMaxKvBytesPerBuffer / sizeof(float);
    if (kv_size > kMaxKvFloatsPerBuffer)
    {
        const int capped_ctx = static_cast<int>(kMaxKvFloatsPerBuffer / static_cast<std::size_t>(std::max(1, kv_dim * config.n_layers)));
        printf("[RawrXD] KV cache budget cap: ctx %d -> %d (kv_size %zu -> budget %zu floats)\n",
               ctx, capped_ctx, kv_size, kMaxKvFloatsPerBuffer);
        ctx = std::max(32, capped_ctx);
        kv_size = static_cast<std::size_t>(config.n_layers) * static_cast<std::size_t>(ctx) * static_cast<std::size_t>(kv_dim);
    }
    printf("[RawrXD] KV cache: %zu floats (%.1f MB per cache)\n", kv_size, kv_size * 4.0 / 1e6);
    // Diagnose available memory before heap probe
    {
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        printf("[RawrXD] MEMSTATUS pre-probe: phys_avail=%llu MB, virt_avail=%llu MB, commit_avail=%llu MB\n",
               ms.ullAvailPhys >> 20, ms.ullAvailVirtual >> 20,
               (ms.ullTotalPageFile - ms.ullTotalPhys + ms.ullAvailPhys) >> 20);
    }
    // Heap probe: validate both K+V allocations are feasible before vector resize
    {
        // We need TWO allocations of kv_size*sizeof(float) for K and V caches
        while (ctx >= 32) {
            kv_size = static_cast<std::size_t>(config.n_layers) * static_cast<std::size_t>(ctx) * static_cast<std::size_t>(kv_dim);
            const std::size_t probeBytes = kv_size * sizeof(float);
            void* probeK = malloc(probeBytes);
            void* probeV = probeK ? malloc(probeBytes) : nullptr;
            const bool ok = (probeK != nullptr && probeV != nullptr);
            if (probeK) free(probeK);
            if (probeV) free(probeV);
            if (ok) {
                printf("[RawrXD] HEAP PROBE OK: ctx=%d (2x %.0f MB = %.0f MB available)\n",
                       ctx, probeBytes / 1048576.0, 2.0 * probeBytes / 1048576.0);
                break;
            }
            printf("[RawrXD] HEAP PROBE FAIL at ctx=%d (2x %.0f MB), halving\n", ctx, probeBytes / 1048576.0);
            ctx /= 2;
        }
        if (ctx < 32) { printf("[RawrXD] FATAL: KV cache heap exhausted even at minimum ctx\n"); return; }
    }
    kv_size = static_cast<std::size_t>(config.n_layers) * static_cast<std::size_t>(ctx) * static_cast<std::size_t>(kv_dim);
    printf("[RawrXD] KV cache final: ctx=%d, %zu floats (%.1f MB per cache)\n", ctx, kv_size, kv_size * 4.0 / 1e6);

    // Keep runtime config synchronized with the probed cache geometry.
    // Forward-path cache indexing derives from config.n_ctx.
    config.n_ctx = ctx;

    kv_cache_k.resize(kv_size, 0.0f);
    kv_cache_v.resize(kv_size, 0.0f);
    kv_cache_pos.assign(static_cast<size_t>(config.n_layers) * static_cast<size_t>(ctx), -1);

    const std::size_t nLay = static_cast<std::size_t>(std::max(1, config.n_layers));
    m_moeReuseResidentRatioEma.assign(nLay, 0.0);
    m_moeReuseSampleCount.assign(nLay, 0u);
    m_moePolicyDivLastPlanGenPerLayer.assign(nLay, std::numeric_limits<std::uint64_t>::max());
    m_moePolicyDivRateWindowStartMs = 0;
    m_moePolicyDivLogsInWindow = 0;
    m_moePolicyDivergenceLogCount = 0;
    m_moeReuseTrackedPlanGeneration = std::numeric_limits<std::uint64_t>::max();
    m_moeReuseForwardTick = 0;
    m_moeMixturePackCachePlanGeneration = std::numeric_limits<std::uint64_t>::max();

    m_moeGroupedPackCacheHits = 0;
    m_moeGroupedPackCacheMisses = 0;
    m_moeGroupedFallbacks = 0;
    m_moeGroupedSyncPackInserts = 0;
    m_moeGroupedWeightedApplies = 0;
    m_moeGroupedSingleExpertApplies = 0;
    m_moeGroupedWeightedFallbacks = 0;
    m_moeGroupedSingleExpertFallbacks = 0;
    m_moePackEvictedByPlanRow = 0;
    m_moePrepackQueueDropped = 0;
    m_moePrepackSkippedNotResident = 0;
    m_moePrepackInserts = 0;
    m_swarmPinningSuppressed = false;
    m_swarmPinFailureStreak = 0;
    shutdownMoePrepackWorker_();
    m_moeMixturePlanPackCache.reset();
    if (config.moe_down_enable_grouped_integration && config.moe_down_grouped_pack_cache_max_entries > 0)
    {
        const std::size_t cap = static_cast<std::size_t>(std::max(1, config.moe_down_grouped_pack_cache_max_entries));
        const std::size_t byteCap = static_cast<std::size_t>(config.moe_down_grouped_pack_cache_max_bytes);
        m_moeMixturePlanPackCache = std::make_unique<RawrXD::MoEIntegr::MoEMixturePlanPackCache>(cap, byteCap);
        startMoePrepackWorker_();
    }
    installSwarmPlanRowEvictionObserver_();

    // Precompute RoPE tables if needed (usually just done on fly in kernels)
    printf("[RawrXD] Transformer Initialized. AVX-512 Kernels Linked.\n");
}

bool RawrXDTransformer::tryMoeSyncPackMixtureIntoCache(const std::uint32_t layer, const std::string& blkPrefix,
                                                       const std::vector<std::uint32_t>& expertPickOrder,
                                                       const int hdim, const int dim, const std::string& cacheKey,
                                                       const std::span<const std::size_t> planRows)
{
    (void)layer;
    if (!loader || !m_moeMixturePlanPackCache)
        return false;
    const int kExperts = static_cast<int>(expertPickOrder.size());
    if (kExperts <= 0 || hdim <= 0 || dim <= 0)
        return false;
    std::vector<const float*> wptrs;
    wptrs.reserve(expertPickOrder.size());
    for (const std::uint32_t e : expertPickOrder)
    {
        float* p = tryGetExpertDownWeightTensor(loader, blkPrefix, e);
        if (!p)
            return false;
        wptrs.push_back(p);
    }
    const std::size_t inD = static_cast<std::size_t>(hdim);
    const std::size_t outD = static_cast<std::size_t>(dim);
    std::size_t nTot = 0;
    std::size_t packedElems = 0;
    if (!checkedMulSizeT(static_cast<std::size_t>(kExperts), outD, nTot) || !checkedMulSizeT(inD, nTot, packedElems))
    {
        return false;
    }
    std::vector<float> packed(packedElems);
    RawrXD::MoEAccumRef::packExpertDownWeightsF32(inD, outD, kExperts, wptrs.data(), packed.data());
    if (planRows.empty())
        m_moeMixturePlanPackCache->put(cacheKey, std::move(packed));
    else
        m_moeMixturePlanPackCache->put(cacheKey, std::move(packed), planRows);
    return true;
}

std::vector<float> RawrXDTransformer::Forward(const std::vector<uint32_t>& tokens, int start_pos)
{
    if (tokens.empty())
        return {};

    // Available physical memory in MB (cheap syscall, used for OOM diagnosis)
    auto AvailPhysMB = []() -> size_t
    {
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        return static_cast<size_t>(ms.ullAvailPhys >> 20);
    };

    if (!loader)
    {
        printf("[Forward] FATAL: loader is null\n");
        return {};
    }

    if (config.dim <= 0 || config.n_layers <= 0 || config.hidden_dim <= 0 || config.n_heads <= 0)
    {
        printf("[Forward] FATAL: invalid config (dim=%d layers=%d hidden=%d heads=%d)\n", config.dim, config.n_layers,
               config.hidden_dim, config.n_heads);
        return {};
    }

    const int dim = config.dim;
    const int n_heads = config.n_heads;
    if (dim % n_heads != 0)
    {
        printf("[Forward] FATAL: dim %d not divisible by n_heads %d\n", dim, n_heads);
        return {};
    }

    const int cache_ctx = std::max(1, config.n_ctx > 0 ? config.n_ctx : (config.seq_len > 0 ? config.seq_len : 2048));
    const int max_tokens_per_call = std::max(1, std::min(cache_ctx, config.seq_len > 0 ? config.seq_len : cache_ctx));
    const size_t incoming_tokens = tokens.size();
    const size_t capped_tokens = std::min(incoming_tokens, static_cast<size_t>(max_tokens_per_call));
    const int T = static_cast<int>(capped_tokens);
    if (incoming_tokens > capped_tokens)
    {
        printf("[Forward] WARN: truncating token batch %zu -> %d\n", incoming_tokens, T);
    }
    int64_t current_pos = static_cast<int64_t>(std::max(0, start_pos));

    int n_kv_heads = config.n_kv_heads > 0 ? config.n_kv_heads : n_heads;
    n_kv_heads = std::max(1, std::min(n_kv_heads, n_heads));
    while (n_heads % n_kv_heads != 0 && n_kv_heads > 1)
    {
        --n_kv_heads;
    }
    if (n_heads % n_kv_heads != 0)
    {
        printf("[Forward] FATAL: unable to map n_heads=%d to n_kv_heads=%d\n", n_heads, n_kv_heads);
        return {};
    }

    const int head_dim = dim / n_heads;
    if (head_dim <= 0)
    {
        printf("[Forward] FATAL: invalid head_dim=%d\n", head_dim);
        return {};
    }

    const int kv_dim = n_kv_heads * head_dim;
    const int heads_per_kv = std::max(1, n_heads / n_kv_heads);

    const size_t layers_u = static_cast<size_t>(config.n_layers);
    const size_t ctx_u = static_cast<size_t>(cache_ctx);
    const size_t kv_u = static_cast<size_t>(kv_dim);
    if (layers_u == 0 || ctx_u == 0 || kv_u == 0 || layers_u > (std::numeric_limits<size_t>::max() / ctx_u) ||
        (layers_u * ctx_u) > (std::numeric_limits<size_t>::max() / kv_u))
    {
        printf("[Forward] FATAL: KV cache size overflow risk (layers=%zu ctx=%zu kv=%zu)\n", layers_u, ctx_u, kv_u);
        return {};
    }
    const size_t expected_cache = layers_u * ctx_u * kv_u;
    if (kv_cache_k.size() < expected_cache || kv_cache_v.size() < expected_cache)
    {
        const size_t kv_bytes_each = expected_cache * sizeof(float);
        const size_t total_kv_mb = (kv_bytes_each * 2) >> 20;
        const size_t avail_mb = AvailPhysMB();
        printf("[MEM] KV resize: each=%.1f MB x2 = %zu MB total, avail_phys=%zu MB "
               "(layers=%zu ctx=%zu kv_dim=%zu)\n",
               kv_bytes_each / (1024.0 * 1024.0), total_kv_mb, avail_mb, layers_u, ctx_u, kv_u);
        if (total_kv_mb + 256 > avail_mb)
        {
            printf("[Forward] FATAL: KV cache needs %zu MB but only %zu MB available. "
                   "Shrinking ctx from %zu to fit.\n",
                   total_kv_mb, avail_mb, ctx_u);
            // Scale down ctx until it fits (keep at least 64 slots)
            size_t reduced_ctx = ctx_u;
            while (reduced_ctx > 64)
            {
                reduced_ctx /= 2;
                const size_t reduced_bytes = layers_u * reduced_ctx * kv_u * sizeof(float);
                if ((reduced_bytes * 2 >> 20) + 256 <= avail_mb)
                    break;
            }
            const size_t reduced_bytes = layers_u * reduced_ctx * kv_u;
            printf("[Forward] WARN: Reduced KV ctx %zu -> %zu (%.1f MB each)\n", ctx_u, reduced_ctx,
                   reduced_bytes * sizeof(float) / (1024.0 * 1024.0));
            try
            {
                kv_cache_k.assign(reduced_bytes, 0.0f);
                kv_cache_v.assign(reduced_bytes, 0.0f);
                kv_cache_pos.assign(layers_u * reduced_ctx, -1);
            }
            catch (const std::bad_alloc&)
            {
                printf("[Forward] FATAL: std::bad_alloc even at reduced KV ctx=%zu\n", reduced_ctx);
                return {};
            }
        }
        else
        {
            try
            {
                kv_cache_k.assign(expected_cache, 0.0f);
                kv_cache_v.assign(expected_cache, 0.0f);
                kv_cache_pos.assign(layers_u * ctx_u, -1);
            }
            catch (const std::bad_alloc&)
            {
                printf("[Forward] FATAL: std::bad_alloc allocating KV cache (%zu MB)\n", total_kv_mb);
                kv_cache_k.clear();
                kv_cache_k.shrink_to_fit();
                kv_cache_v.clear();
                kv_cache_v.shrink_to_fit();
                return {};
            }
        }
    }
    else if (kv_cache_pos.size() < (layers_u * ctx_u))
    {
        kv_cache_pos.assign(layers_u * ctx_u, -1);
    }

    static bool printed_config = false;
    if (!printed_config)
    {
        printf("[Forward] GQA: dim=%d heads=%d kv_heads=%d head_dim=%d kv_dim=%d hidden=%d\n", dim, n_heads, n_kv_heads,
               head_dim, kv_dim, config.hidden_dim);
        printed_config = true;
    }

    // One-time memory snapshot at generation start
    static bool printed_tok_mem = false;
    if (!printed_tok_mem && start_pos == 0)
    {
        printed_tok_mem = true;
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        const size_t tile_peak_mb = 16;
        printf("[MEM] Generation start: avail_phys=%llu MB, avail_virt=%llu MB\n", ms.ullAvailPhys >> 20,
               ms.ullAvailVirtual >> 20);
        printf("[MEM] KV footprint: k=%zu MB, v=%zu MB; tile_buf peak=%zu MB\n",
               kv_cache_k.size() * sizeof(float) >> 20, kv_cache_v.size() * sizeof(float) >> 20, tile_peak_mb);
        printf("[MEM] output.weight shard est: "
               "vocab=%d * row_bytes(Q2_K)=%zu = %zu MB\n",
               config.vocab_size, (static_cast<size_t>(dim) / 256) * 84,
               (static_cast<size_t>(config.vocab_size) * (static_cast<size_t>(dim) / 256) * 84) >> 20);
    }

    // Upfront token ID bounds validation — reject before embedding lookup
    for (int t = 0; t < T; t++)
    {
        const uint32_t& token = tokens[t];
        if (token >= static_cast<uint32_t>(config.vocab_size))
        {
            printf("[Forward] FATAL: Token ID %u out of bounds [0, %d) at position %d. Rejecting batch.\n", 
                   token, config.vocab_size, t);
            return {};
        }
    }

    // Current hidden state and reusable per-layer buffers.
    std::vector<float> x(dim);
    std::vector<float> residual(dim);
    std::vector<float> q(dim), k(kv_dim), v(kv_dim);
    std::vector<float> packed_qkv(static_cast<size_t>(dim) + static_cast<size_t>(kv_dim) * 2U);
    std::vector<float> att_out(dim), attn_final(dim);
    std::vector<float> packed_ffn_gate_up(static_cast<size_t>(config.hidden_dim) * 2U);
    std::vector<float> h1(config.hidden_dim), h3(config.hidden_dim), final_ffn(dim), moe_ffn_acc(dim);
    std::vector<float> scores(cache_ctx);
    std::vector<uint8_t> score_valid(cache_ctx, 0);

    for (int t = 0; t < T; t++)
    {
        const bool swarmTokenActive = (m_swarmScheduler != nullptr) && (loader != nullptr) && (loader->getExperts() > 0) &&
                                      !m_swarmPinningSuppressed;
        if (swarmTokenActive)
            m_swarmScheduler->onForwardTokenStepBegin();

        maybeSampleMoEReuseFromHeatmap();

        uint32_t token = tokens[t];

        // 1. Embedding lookup
        std::string emb_name = "token_embd.weight";

        if (config.vocab_size <= 0)
        {
            printf("[Forward] FATAL: vocab_size=%d\n", config.vocab_size);
            return {};
        }

        // Token ID already validated upfront — no need to clamp
        bool row_ok = loader->GetTensorRow(emb_name, static_cast<size_t>(token), x.data(), static_cast<size_t>(dim));
        if (!row_ok)
        {
            emb_name = "model.embed_tokens.weight";
            row_ok = loader->GetTensorRow(emb_name, static_cast<size_t>(token), x.data(), static_cast<size_t>(dim));
        }
        if (!row_ok)
        {
            printf("[Forward] FATAL: Missing token embedding tensor\n");
            return {};
        }

        // 2. Transformer Layers
        for (int l = 0; l < config.n_layers; l++)
        {
            const bool layerSwarmActive = (m_swarmScheduler != nullptr) && !m_swarmPinningSuppressed &&
                                          (loader != nullptr) && (loader->getExperts() > 0);
            std::vector<std::size_t> layerPinnedPlanRows;
            struct SwarmLayerPinGuard
            {
                RawrXD::Swarm::ISwarmScheduler* sched{};
                std::vector<std::size_t>* rows{};
                ~SwarmLayerPinGuard()
                {
                    if (sched && rows && !rows->empty())
                    {
                        sched->unpinPlanRows(std::span<const std::size_t>(rows->data(), rows->size()));
                        rows->clear();
                    }
                }
            } swarmPinGuard{layerSwarmActive ? m_swarmScheduler : nullptr, &layerPinnedPlanRows};

            const bool moeTwoPhasePin = layerSwarmActive && loader->getExperts() > 0 &&
                                        swarmLayerHasExpertSlices(static_cast<std::uint32_t>(l));

            if (layerSwarmActive)
            {
                (void)m_swarmScheduler->onLayerComputeStarted(0u, static_cast<std::uint32_t>(l));
                const SwarmPinLayerParts pinPart =
                    moeTwoPhasePin ? SwarmPinLayerParts::StaticOnly : SwarmPinLayerParts::Full;
                if (const auto pinned = pinSwarmSlicesForLayer(0u, static_cast<std::uint32_t>(l), nullptr, 0,
                                                               layerPinnedPlanRows, pinPart, false);
                    !pinned)
                {
                    printf("[Forward] WARN: pinSwarmSlicesForLayer layer %d failed: %s\n", l,
                           RawrXD::Swarm::schedulerErrorMessage(pinned.error()));
                }
            }

            if ((l % 5) == 0 || l == config.n_layers - 1)
            {
                char stepBuf[192];
                const int n = std::snprintf(stepBuf, sizeof(stepBuf), "[STEP] Layer %d/%d (pos=%lld token=%d/%d)\n",
                                            l + 1, config.n_layers, static_cast<long long>(current_pos + t), t + 1, T);
                if (n > 0 && static_cast<size_t>(n) < sizeof(stepBuf))
                {
                    (void)std::fwrite(stepBuf, 1, static_cast<size_t>(n), stdout);
                    std::fflush(stdout);
                    OutputDebugStringA(stepBuf);
                    if (m_layerProgressCb)
                    {
                        m_layerProgressCb(std::string(stepBuf, static_cast<size_t>(n)));
                    }
                }
            }
            residual = x;

            // --- ATTENTION ---
            std::string prefix = "blk." + std::to_string(l) + ".";

            float* attn_norm = loader->GetTensor(prefix + "attn_norm.weight");
            if (!attn_norm)
            {
                printf("[Forward] FATAL: Missing %sattn_norm.weight\n", prefix.c_str());
                return {};
            }
            RMSNorm_AVX512(x.data(), x.data(), attn_norm, dim, config.rms_norm_eps);

            const std::string wq_name = prefix + "attn_q.weight";
            const std::string wk_name = prefix + "attn_k.weight";
            const std::string wv_name = prefix + "attn_v.weight";
            const std::string wqkv_name = prefix + "attn_qkv.weight";
            const std::string wo_name = prefix + "attn_output.weight";

            const bool has_split_qkv = loader->hasTensorNamed(wq_name) && loader->hasTensorNamed(wk_name) &&
                                       loader->hasTensorNamed(wv_name);
            if (has_split_qkv)
            {
                if (!loader->StreamingMatMul(wq_name, x.data(), q.data(), dim, dim))
                {
                    printf("[Forward] FATAL: StreamingMatMul failed for %s\n", wq_name.c_str());
                    return {};
                }

                if (!loader->StreamingMatMul(wk_name, x.data(), k.data(), dim, kv_dim))
                {
                    printf("[Forward] FATAL: StreamingMatMul failed for %s\n", wk_name.c_str());
                    return {};
                }

                if (!loader->StreamingMatMul(wv_name, x.data(), v.data(), dim, kv_dim))
                {
                    printf("[Forward] FATAL: StreamingMatMul failed for %s\n", wv_name.c_str());
                    return {};
                }
            }
            else
            {
                const size_t packed_qkv_dim = static_cast<size_t>(dim) + static_cast<size_t>(kv_dim) * 2U;
                if (!loader->hasTensorNamed(wqkv_name) ||
                    !loader->StreamingMatMul(wqkv_name, x.data(), packed_qkv.data(), dim, packed_qkv_dim))
                {
                    printf("[Forward] FATAL: Missing compatible attention projection for %s (expected split q/k/v or packed qkv)\n",
                           prefix.c_str());
                    return {};
                }

                std::memcpy(q.data(), packed_qkv.data(), static_cast<size_t>(dim) * sizeof(float));
                std::memcpy(k.data(), packed_qkv.data() + static_cast<size_t>(dim),
                            static_cast<size_t>(kv_dim) * sizeof(float));
                std::memcpy(v.data(), packed_qkv.data() + static_cast<size_t>(dim) + static_cast<size_t>(kv_dim),
                            static_cast<size_t>(kv_dim) * sizeof(float));
            }

            // RoPE — apply separately for Q (n_heads) and K (n_kv_heads)
            RoPE_AVX512(q.data(), nullptr, current_pos + t, head_dim, n_heads);
            RoPE_AVX512(k.data(), nullptr, current_pos + t, head_dim, n_kv_heads);

            // KV Cache Update with ring-buffer indexing.
            const int64_t abs_pos = current_pos + static_cast<int64_t>(t);
            const int slot = static_cast<int>(abs_pos % static_cast<int64_t>(cache_ctx));
            const size_t layer_base =
                static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) * static_cast<size_t>(kv_dim);
            const size_t cache_offset = layer_base + static_cast<size_t>(slot) * static_cast<size_t>(kv_dim);
            if (cache_offset > kv_cache_k.size() || cache_offset > kv_cache_v.size() ||
                static_cast<size_t>(kv_dim) > (kv_cache_k.size() - cache_offset) ||
                static_cast<size_t>(kv_dim) > (kv_cache_v.size() - cache_offset))
            {
                printf("[Forward] FATAL: KV cache write OOB risk (layer=%d slot=%d kv_dim=%d)\n", l, slot, kv_dim);
                return {};
            }
            memcpy(kv_cache_k.data() + cache_offset, k.data(), static_cast<size_t>(kv_dim) * sizeof(float));
            memcpy(kv_cache_v.data() + cache_offset, v.data(), static_cast<size_t>(kv_dim) * sizeof(float));
            kv_cache_pos[static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) + static_cast<size_t>(slot)] = abs_pos;

            // Multi-head attention with GQA
            std::fill(att_out.begin(), att_out.end(), 0.0f);
            const int64_t seq_len_total = abs_pos + 1;
            const int attn_len = static_cast<int>(std::min<int64_t>(seq_len_total, static_cast<int64_t>(cache_ctx)));
            const int64_t window_start = seq_len_total - static_cast<int64_t>(attn_len);
            const float inv_scale = 1.0f / sqrtf(static_cast<float>(head_dim));

            for (int h = 0; h < n_heads; h++)
            {
                const int kv_h = std::min(n_kv_heads - 1, h / heads_per_kv);
                const float* q_head = q.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                int valid_count = 0;

                for (int p = 0; p < attn_len; p++)
                {
                    const int64_t abs_p = window_start + static_cast<int64_t>(p);
                    const int p_slot = static_cast<int>(abs_p % static_cast<int64_t>(cache_ctx));
                    const size_t pos_idx =
                        static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) + static_cast<size_t>(p_slot);
                    if (kv_cache_pos[pos_idx] != abs_p)
                    {
                        scores[p] = -1e9f;
                        score_valid[p] = 0;
                        continue;
                    }
                    const size_t k_off = layer_base + static_cast<size_t>(p_slot) * static_cast<size_t>(kv_dim) +
                                         static_cast<size_t>(kv_h) * static_cast<size_t>(head_dim);
                    const float* k_past = kv_cache_k.data() + k_off;
                    float score = DotProduct_AVX512(q_head, k_past, head_dim);
                    float scaled = std::isfinite(score) ? (score * inv_scale) : -1e9f;
                    scores[p] = std::max(-80.0f, std::min(80.0f, scaled));
                    score_valid[p] = 1;
                    ++valid_count;
                }
                if (valid_count == 0)
                {
                    float* out_head = att_out.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                    std::fill(out_head, out_head + head_dim, 0.0f);
                    continue;
                }

                Softmax_AVX512(scores.data(), attn_len);
                for (int p = 0; p < attn_len; ++p)
                {
                    if (!std::isfinite(scores[p]))
                        scores[p] = 0.0f;
                }

                float* out_head = att_out.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                for (int p = 0; p < attn_len; p++)
                {
                    if (!score_valid[p])
                        continue;
                    const int64_t abs_p = window_start + static_cast<int64_t>(p);
                    const int p_slot = static_cast<int>(abs_p % static_cast<int64_t>(cache_ctx));
                    const size_t v_off = layer_base + static_cast<size_t>(p_slot) * static_cast<size_t>(kv_dim) +
                                         static_cast<size_t>(kv_h) * static_cast<size_t>(head_dim);
                    const float* v_past = kv_cache_v.data() + v_off;
                    VectorAddScaled_AVX512(out_head, v_past, scores[p], head_dim);
                }
            }

            // Output projection: dim → dim
            if (!loader->StreamingMatMul(wo_name, att_out.data(), attn_final.data(), dim, dim))
            {
                printf("[Forward] FATAL: StreamingMatMul failed for %s\n", wo_name.c_str());
                return {};
            }

            // Residual add
            VectorAdd_AVX512(x.data(), residual.data(), attn_final.data(), dim);
            for (int i = 0; i < dim; ++i)
            {
                if (!std::isfinite(x[i]))
                    x[i] = 0.0f;
            }

            // --- FFN (SwiGLU) ---
            residual = x;
            std::string ffn_prefix = prefix + "ffn_";

            float* ffn_norm = loader->GetTensor(prefix + "ffn_norm.weight");
            if (!ffn_norm)
            {
                printf("[Forward] FATAL: Missing %sffn_norm.weight\n", prefix.c_str());
                return {};
            }
            RMSNorm_AVX512(x.data(), x.data(), ffn_norm, dim, config.rms_norm_eps);

            std::vector<std::uint32_t> moeExpertPick;
            std::vector<float> moeMixtureWeights;
            if (moeTwoPhasePin && layerSwarmActive)
            {
                (void)tryPickMoERouterExperts(static_cast<std::uint32_t>(l), x.data(), moeExpertPick,
                                              moeMixtureWeights);
                const std::uint32_t* const pickData = moeExpertPick.empty() ? nullptr : moeExpertPick.data();
                const std::size_t pickCount = moeExpertPick.size();
                if (const auto pe = pinSwarmSlicesForLayer(0u, static_cast<std::uint32_t>(l), pickData, pickCount,
                                                           layerPinnedPlanRows, SwarmPinLayerParts::ExpertsOnly, true);
                    !pe)
                {
                    printf("[Forward] WARN: pinSwarmSlicesForLayer experts layer %d failed: %s\n", l,
                           RawrXD::Swarm::schedulerErrorMessage(pe.error()));
                }
            }

            const std::string w1_name = ffn_prefix + "gate.weight";
            const std::string w2_name = ffn_prefix + "down.weight";
            const std::string w3_name = ffn_prefix + "up.weight";

            int hdim = config.hidden_dim;

            if (config.moe_down_log_policy_divergence && loader->getExperts() > 0)
            {
                const int kPol =
                    moeExpertPick.empty() ? std::max(1, loader->getExperts()) : static_cast<int>(moeExpertPick.size());
                (void)chooseMoEDownProjectPath(static_cast<std::uint32_t>(l), static_cast<std::size_t>(hdim),
                                               static_cast<std::size_t>(dim), kPol);
            }

            const bool denseFfn = loader->hasTensorNamed(w1_name);
            const bool packedDenseFfn = !denseFfn && loader->hasTensorNamed(w2_name) && loader->hasTensorNamed(w3_name);
            if (denseFfn || packedDenseFfn)
            {
                if (denseFfn)
                {
                    if (!loader->StreamingMatMul(w1_name, x.data(), h1.data(), dim, hdim))
                    {
                        printf("[Forward] FATAL: StreamingMatMul failed for %s\n", w1_name.c_str());
                        return {};
                    }

                    if (!loader->StreamingMatMul(w3_name, x.data(), h3.data(), dim, hdim))
                    {
                        printf("[Forward] FATAL: StreamingMatMul failed for %s\n", w3_name.c_str());
                        return {};
                    }
                }
                else
                {
                    const size_t packedFfnDim = static_cast<size_t>(hdim) * 2U;
                    if (!loader->StreamingMatMul(w3_name, x.data(), packed_ffn_gate_up.data(), dim, packedFfnDim))
                    {
                        printf("[Forward] FATAL: StreamingMatMul failed for packed FFN %s\n", w3_name.c_str());
                        return {};
                    }
                    std::memcpy(h1.data(), packed_ffn_gate_up.data(), static_cast<size_t>(hdim) * sizeof(float));
                    std::memcpy(h3.data(), packed_ffn_gate_up.data() + static_cast<size_t>(hdim),
                                static_cast<size_t>(hdim) * sizeof(float));
                }

                Silu_AVX512(h1.data(), hdim);
#ifdef __AVX512F__
                {
                    int i = 0;
                    for (; i + 15 < hdim; i += 16)
                    {
                        __m512 h1v = _mm512_loadu_ps(h1.data() + i);
                        __m512 h3v = _mm512_loadu_ps(h3.data() + i);
                        _mm512_storeu_ps(h1.data() + i, _mm512_mul_ps(h1v, h3v));
                    }
                    for (; i < hdim; i++)
                        h1[i] *= h3[i];
                }
#else
                for (int i = 0; i < hdim; i++)
                    h1[i] *= h3[i];
#endif

                if (!loader->StreamingMatMul(w2_name, h1.data(), final_ffn.data(), hdim, dim))
                {
                    printf("[Forward] FATAL: StreamingMatMul failed for %s\n", w2_name.c_str());
                    return {};
                }
            }
            else
            {
                auto tryRunGroupedMoEDown = [&](const std::vector<std::uint32_t>& expertPick,
                                                const std::vector<float>& mixtureWeights,
                                                const bool weightedMode) -> bool
                {
                    const int kPick = static_cast<int>(expertPick.size());
                    if (kPick <= 0 || mixtureWeights.size() != expertPick.size())
                        return false;
                    if (!config.moe_down_enable_grouped_integration || !m_moeMixturePlanPackCache)
                        return false;
                    if (chooseMoEDownProjectPath(static_cast<std::uint32_t>(l), static_cast<std::size_t>(hdim),
                                                 static_cast<std::size_t>(dim),
                                                 kPick) != RawrXD::MoEDownProject::Path::GroupedCached)
                        return false;

                    const std::string mixKey = makeMoeMixturePackCacheKey(0u, static_cast<std::uint32_t>(l),
                                                                          expertPick, m_swarmPlanSliceIndex);
                    std::vector<std::size_t> mixturePlanRows;
                    collectMoeMixturePlanRowRefs(0u, static_cast<std::uint32_t>(l), expertPick, m_swarmPlanSliceIndex,
                                                 mixturePlanRows);
                    const std::span<const std::size_t> mixtureRowsSpan(mixturePlanRows.data(), mixturePlanRows.size());

                    auto tryApplyGroupedPacked = [&](const float* packedPtr, std::size_t packedCount)
                    {
                        const std::size_t expectPacked = static_cast<std::size_t>(hdim) *
                                                         static_cast<std::size_t>(dim) *
                                                         static_cast<std::size_t>(kPick);
                        if (!config.moe_down_use_grouped_down_math || !packedPtr || packedCount != expectPacked)
                            return false;

                        std::vector<float> h1Rows(static_cast<std::size_t>(kPick) * static_cast<std::size_t>(hdim));
                        bool hidOk = true;
                        for (int ii = 0; ii < kPick; ++ii)
                        {
                            if (mixtureWeights[static_cast<std::size_t>(ii)] <= 0.f)
                            {
                                std::fill_n(h1Rows.data() +
                                                static_cast<std::size_t>(ii) * static_cast<std::size_t>(hdim),
                                            static_cast<std::size_t>(hdim), 0.f);
                                continue;
                            }
                            if (!forwardMoEExpertSwiGLUPostActivation(loader, prefix,
                                                                      expertPick[static_cast<std::size_t>(ii)],
                                                                      x.data(), h1, h3, dim, hdim))
                            {
                                hidOk = false;
                                break;
                            }
                            std::memcpy(h1Rows.data() + static_cast<std::size_t>(ii) * static_cast<std::size_t>(hdim),
                                        h1.data(), static_cast<std::size_t>(hdim) * sizeof(float));
                        }
                        if (!hidOk)
                            return false;

                        std::fill(moe_ffn_acc.begin(), moe_ffn_acc.end(), 0.f);
                        RawrXD::MoEAccumRef::moeDownProjectBatchedExpertsFromPackedF32(
                            h1Rows.data(), static_cast<std::size_t>(hdim), static_cast<std::size_t>(dim), kPick,
                            packedPtr, mixtureWeights.data(), moe_ffn_acc.data());

                        bool anyPositiveW = false;
                        for (float w : mixtureWeights)
                        {
                            if (w > 0.f)
                            {
                                anyPositiveW = true;
                                break;
                            }
                        }
                        if (!anyPositiveW)
                            return false;

                        for (int j = 0; j < dim; ++j)
                            final_ffn[static_cast<std::size_t>(j)] = moe_ffn_acc[static_cast<std::size_t>(j)];
                        if (weightedMode)
                            ++m_moeGroupedWeightedApplies;
                        else
                            ++m_moeGroupedSingleExpertApplies;
                        return true;
                    };

                    const float* packedPtr = nullptr;
                    std::size_t packedCount = 0;
                    if (m_moeMixturePlanPackCache->tryGet(mixKey, &packedPtr, &packedCount))
                    {
                        ++m_moeGroupedPackCacheHits;
                        if (tryApplyGroupedPacked(packedPtr, packedCount))
                            return true;
                        ++m_moeGroupedFallbacks;
                        if (weightedMode)
                            ++m_moeGroupedWeightedFallbacks;
                        else
                            ++m_moeGroupedSingleExpertFallbacks;
                        return false;
                    }

                    ++m_moeGroupedPackCacheMisses;
                    bool syncInserted = false;
                    if (config.moe_down_grouped_sync_pack_on_miss &&
                        tryMoeSyncPackMixtureIntoCache(static_cast<std::uint32_t>(l), prefix, expertPick, hdim, dim,
                                                       mixKey, mixtureRowsSpan))
                    {
                        ++m_moeGroupedSyncPackInserts;
                        syncInserted = true;
                    }

                    if (syncInserted && m_moeMixturePlanPackCache->tryGet(mixKey, &packedPtr, &packedCount))
                    {
                        ++m_moeGroupedPackCacheHits;
                        if (tryApplyGroupedPacked(packedPtr, packedCount))
                            return true;
                    }

                    if (config.moe_down_grouped_async_prepack)
                    {
                        MoEPrepackJob job;
                        job.layer = static_cast<std::uint32_t>(l);
                        job.blkPrefix = prefix;
                        job.expertPick = expertPick;
                        job.hdim = hdim;
                        job.dim = dim;
                        job.cacheKey = mixKey;
                        job.planRows = mixturePlanRows;
                        tryEnqueueMoePrepack_(std::move(job));
                    }
                    ++m_moeGroupedFallbacks;
                    if (weightedMode)
                        ++m_moeGroupedWeightedFallbacks;
                    else
                        ++m_moeGroupedSingleExpertFallbacks;
                    return false;
                };

                bool usedMixture = false;
                if (config.moe_weighted_mixture && !moeExpertPick.empty() &&
                    moeMixtureWeights.size() == moeExpertPick.size())
                {
                    if (tryRunGroupedMoEDown(moeExpertPick, moeMixtureWeights, true))
                        usedMixture = true;
                    if (!usedMixture)
                    {
                        std::fill(moe_ffn_acc.begin(), moe_ffn_acc.end(), 0.f);
                        bool anyPositiveW = false;
                        for (std::size_t ii = 0; ii < moeExpertPick.size(); ++ii)
                        {
                            const float w = moeMixtureWeights[ii];
                            if (w <= 0.f)
                                continue;
                            anyPositiveW = true;
                            if (!forwardMoEExpertSwiGLU(loader, prefix, moeExpertPick[ii], x.data(), h1, h3, final_ffn,
                                                        dim, hdim))
                            {
                                printf("[Forward] FATAL: MoE expert FFN failed blk.%d expert %u\n", l,
                                       static_cast<unsigned>(moeExpertPick[ii]));
                                return {};
                            }
                            for (int j = 0; j < dim; ++j)
                                moe_ffn_acc[static_cast<std::size_t>(j)] += w * final_ffn[static_cast<std::size_t>(j)];
                        }
                        if (anyPositiveW)
                        {
                            for (int j = 0; j < dim; ++j)
                                final_ffn[static_cast<std::size_t>(j)] = moe_ffn_acc[static_cast<std::size_t>(j)];
                            usedMixture = true;
                        }
                    }
                }
                if (!usedMixture)
                {
                    std::uint32_t expertRun = 0;
                    if (!moeExpertPick.empty())
                        expertRun = moeExpertPick[0];
                    if (!moeExpertPick.empty())
                    {
                        const std::vector<std::uint32_t> groupedPick = {expertRun};
                        const std::vector<float> groupedWeights = {1.0f};
                        usedMixture = tryRunGroupedMoEDown(groupedPick, groupedWeights, false);
                    }
                    if (usedMixture)
                        continue;
                    if (!forwardMoEExpertSwiGLU(loader, prefix, expertRun, x.data(), h1, h3, final_ffn, dim, hdim))
                    {
                        printf("[Forward] FATAL: MoE FFN has no dense gate.weight and no ffn_experts.* tensors for "
                               "blk.%d expert %u\n",
                               l, static_cast<unsigned>(expertRun));
                        return {};
                    }
                }
            }

            VectorAdd_AVX512(x.data(), residual.data(), final_ffn.data(), dim);
            for (int i = 0; i < dim; ++i)
            {
                if (!std::isfinite(x[i]))
                    x[i] = 0.0f;
            }

            if (layerSwarmActive)
            {
                if (!layerPinnedPlanRows.empty())
                {
                    m_swarmScheduler->unpinPlanRows(
                        std::span<const std::size_t>(layerPinnedPlanRows.data(), layerPinnedPlanRows.size()));
                    layerPinnedPlanRows.clear();
                }
                (void)m_swarmScheduler->onLayerComputeFinished(0u, static_cast<std::uint32_t>(l));
            }
        }
    }

    // Final norm + output projection
    float* out_norm = loader->GetTensor("output_norm.weight");
    if (!out_norm)
    {
        printf("[Forward] FATAL: Missing output_norm.weight\n");
        return {};
    }
    RMSNorm_AVX512(x.data(), x.data(), out_norm, dim, config.rms_norm_eps);

    if (config.vocab_size <= 0)
    {
        printf("[Forward] FATAL: vocab_size=%d\n", config.vocab_size);
        return {};
    }
    std::vector<float> logits(config.vocab_size);
    if (!loader->StreamingMatMul("output.weight", x.data(), logits.data(), dim, config.vocab_size))
    {
        MEMORYSTATUSEX ms2{};
        ms2.dwLength = sizeof(ms2);
        GlobalMemoryStatusEx(&ms2);
        const size_t shard_mb = (static_cast<size_t>(config.vocab_size) * (static_cast<size_t>(dim) / 256) * 84) >> 20;
        printf("[Forward] FATAL: output.weight StreamingMatMul failed. "
               "vocab=%d dim=%d shard_est=%zu MB avail_phys=%llu MB avail_virt=%llu MB\n",
               config.vocab_size, dim, shard_mb, ms2.ullAvailPhys >> 20, ms2.ullAvailVirtual >> 20);
        return {};
    }
    for (int i = 0; i < config.vocab_size; ++i)
    {
        if (!std::isfinite(logits[i]))
            logits[i] = -std::numeric_limits<float>::max();
    }

    return logits;
}
