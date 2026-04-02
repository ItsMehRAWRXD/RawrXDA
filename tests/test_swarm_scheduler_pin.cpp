// Unit + light stress: batch pin / unpin, submitPlan while held, concurrent pin/unpin.
#include "core/swarm_scheduler.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace
{
int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

using RawrXD::Swarm::ExpertHeatmapCaptureParams;
using RawrXD::Swarm::ExpertHeatmapSnapshot;
using RawrXD::Swarm::expertHeatmapSnapshotToJson;
using RawrXD::Swarm::ModelSlice;
using RawrXD::Swarm::SchedulerConfig;
using RawrXD::Swarm::SchedulerError;
using RawrXD::Swarm::SwarmScheduler;

ModelSlice mkSlice(const std::uint32_t layer, const std::uint32_t span, const std::uint64_t off, const std::uint64_t sz,
                   const std::uint32_t expert = 0xFFFFFFFFu)
{
    ModelSlice s;
    s.id.modelIndex = 0;
    s.id.layerStart = layer;
    s.id.layerEnd = layer + 1;
    s.id.expertIndex = expert;
    s.id.planSpanOrdinal = span;
    s.fileOffsetBytes = off;
    s.byteSize = sz;
    return s;
}
}  // namespace

int main()
{
    SwarmScheduler sched;
    SchedulerConfig cfg;
    cfg.maxWorkingSetBytes = 3000;
    cfg.maxResidentLayers = 64;
    cfg.enableAsyncPrefetchThread = false;
    if (!sched.configure(cfg).has_value())
        return fail("configure");

    std::vector<ModelSlice> plan;
    for (int i = 0; i < 5; ++i)
        plan.push_back(mkSlice(static_cast<std::uint32_t>(i), 0, static_cast<std::uint64_t>(i) * 1000ULL, 1000));
    if (!sched.submitPlan(std::move(plan)).has_value())
        return fail("submitPlan");

    // Duplicate indices in one pin call: single hold per row (symmetric unpin).
    {
        const std::array<std::size_t, 3> dup{1, 1, 1};
        if (!sched.pinPlanRows(std::span<const std::size_t>(dup.data(), dup.size())).has_value())
            return fail("pin duplicate rows");
        sched.unpinPlanRows(std::span<const std::size_t>(dup.data(), dup.size()));
        if (sched.runtimeStats().inUseSliceCount != 0)
            return fail("unpin duplicate rows should clear holds");
    }

    const std::array<std::size_t, 3> batch{0, 1, 2};
    if (!sched.pinPlanRowsBlocking(batch, 5000))
        return fail("pinPlanRowsBlocking 0..2");

    const auto stPinned = sched.runtimeStats();
    if (stPinned.inUseSliceCount != 3)
        return fail("inUseSliceCount after pin");

    const std::array<std::size_t, 1> p3{3};
    if (sched.pinPlanRows(p3).has_value())
        return fail("pin row 3 should fail (budget + held residents)");

    const std::array<std::size_t, 1> b0{0};
    if (!sched.pinPlanRows(b0).has_value())
        return fail("re-pin row 0 while held should succeed (already resident)");

    std::vector<ModelSlice> planHeld;
    planHeld.push_back(mkSlice(10, 0, 50000, 500));
    if (sched.submitPlan(std::move(planHeld)).error() != SchedulerError::PlanRowsHeld)
        return fail("submitPlan must return PlanRowsHeld when slices pinned");

    sched.unpinPlanRows(std::span<const std::size_t>(batch.data(), batch.size()));
    sched.unpinPlanRows(b0);

    if (sched.runtimeStats().inUseSliceCount != 0)
        return fail("inUseSliceCount after unpin");

    std::vector<ModelSlice> plan2;
    plan2.push_back(mkSlice(10, 0, 50000, 500));
    if (!sched.submitPlan(std::move(plan2)).has_value())
        return fail("submitPlan after full unpin");

    // --- stress: many small expert rows, overlapping pins ---
    SwarmScheduler stress;
    (void)stress.configure(cfg);
    std::vector<ModelSlice> stressPlan;
    constexpr int kExperts = 48;
    stressPlan.reserve(static_cast<std::size_t>(kExperts));
    for (int i = 0; i < kExperts; ++i)
        stressPlan.push_back(mkSlice(0, static_cast<std::uint32_t>(i), 20000ULL + static_cast<std::uint64_t>(i) * 32ULL,
                                     32, static_cast<std::uint32_t>(i)));
    if (!stress.submitPlan(std::move(stressPlan)).has_value())
        return fail("stress submitPlan");

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;
    threads.reserve(6);
    for (int t = 0; t < 6; ++t)
    {
        threads.emplace_back(
            [&stress, t, &stop]()
            {
                unsigned x = static_cast<unsigned>(t) * 7u + 1u;
                while (!stop.load(std::memory_order_acquire))
                {
                    const std::size_t a = static_cast<std::size_t>(x % kExperts);
                    const std::size_t b = static_cast<std::size_t>((x + 3u) % kExperts);
                    x += 11u;
                    const std::array<std::size_t, 2> rows{a, b};
                    if (stress.pinPlanRowsBlocking(rows, 500))
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(30));
                        stress.unpinPlanRows(rows);
                    }
                }
            });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop.store(true, std::memory_order_release);
    for (auto& th : threads)
        th.join();

    const auto stEnd = stress.runtimeStats();
    (void)stEnd;

    // --- expert heatmap snapshot + JSON (multi-expert / MoE grid backend) ---
    {
        SwarmScheduler hm;
        SchedulerConfig hmCfg = cfg;
        hmCfg.maxWorkingSetBytes = 8000;
        (void)hm.configure(hmCfg);
        std::vector<ModelSlice> hmPlan;
        for (int e = 0; e < 8; ++e)
        {
            ModelSlice s =
                mkSlice(3, 0, 90000ULL + static_cast<std::uint64_t>(e) * 400ULL, 400, static_cast<std::uint32_t>(e));
            s.debugName = "expert_test";
            hmPlan.push_back(std::move(s));
        }
        if (!hm.submitPlan(std::move(hmPlan)).has_value())
            return fail("heatmap submitPlan");
        const std::array<std::size_t, 2> pinE{0, 5};
        if (!hm.pinPlanRows(pinE).has_value())
            return fail("heatmap pin experts");

        ExpertHeatmapSnapshot snap;
        ExpertHeatmapCaptureParams cap;
        cap.modelIndex = 0;
        cap.expertsOnly = true;
        if (!hm.captureExpertHeatmapSnapshot(cap, snap))
            return fail("captureExpertHeatmapSnapshot");
        if (snap.planGeneration == 0 || snap.cells.size() != 8)
            return fail("heatmap cell count");
        if (!snap.cells[0].resident || snap.cells[0].holdCount != 1)
            return fail("heatmap row0 resident/hold");
        if (!snap.cells[5].resident || snap.cells[5].holdCount != 1)
            return fail("heatmap row5 resident/hold");
        if (snap.cells[1].resident || snap.cells[1].holdCount != 0)
            return fail("heatmap row1 should not be pinned");
        if (snap.statsAtSample.inUseSliceCount != 2)
            return fail("heatmap inUseSliceCount");

        const std::string j = expertHeatmapSnapshotToJson(snap);
        if (j.find("\"snapshotId\"") == std::string::npos || j.find("\"cells\"") == std::string::npos)
            return fail("heatmap JSON shape");
    }

    std::printf("OK test_swarm_scheduler_pin (ev_rej_in_use=%llu ev_starve=%llu)\n",
                static_cast<unsigned long long>(stEnd.evictionRejectedInUse),
                static_cast<unsigned long long>(stEnd.evictStarvation));
    return 0;
}
