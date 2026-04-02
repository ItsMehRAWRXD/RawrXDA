// Stress: concurrent pin/unpin + optional submitPlan vs captureExpertHeatmapSnapshot.
// Measures capture latency percentiles; validates monotonic planGeneration from the capturer's POV.
#include "core/swarm_scheduler.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
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
using RawrXD::Swarm::SwarmRuntimeStats;
using RawrXD::Swarm::SwarmScheduler;

ModelSlice mkExpertSlice(const std::uint32_t layer, const std::uint32_t expert, const std::uint64_t off,
                         const std::uint64_t sz)
{
    ModelSlice s;
    s.id.modelIndex = 0;
    s.id.layerStart = layer;
    s.id.layerEnd = layer + 1;
    s.id.expertIndex = expert;
    s.id.planSpanOrdinal = 0;
    s.fileOffsetBytes = off;
    s.byteSize = sz;
    s.debugName = "stress";
    return s;
}

[[nodiscard]] std::vector<ModelSlice> makePlanA()
{
    std::vector<ModelSlice> p;
    p.reserve(32);
    for (int e = 0; e < 32; ++e)
        p.push_back(
            mkExpertSlice(1, static_cast<std::uint32_t>(e), 100000ULL + static_cast<std::uint64_t>(e) * 64ULL, 64));
    return p;
}

[[nodiscard]] std::vector<ModelSlice> makePlanB()
{
    std::vector<ModelSlice> p;
    p.reserve(24);
    for (int e = 0; e < 24; ++e)
        p.push_back(
            mkExpertSlice(2, static_cast<std::uint32_t>(e), 200000ULL + static_cast<std::uint64_t>(e) * 64ULL, 64));
    return p;
}
}  // namespace

int main(int argc, char** argv)
{
    int captureIterations = 4000;
    std::uint64_t maxCaptureUs = 50'000;  // 50 ms — generous for CI VMs
    if (argc >= 2)
        captureIterations = std::max(100, std::atoi(argv[1]));
    if (argc >= 3)
        maxCaptureUs = static_cast<std::uint64_t>(std::strtoull(argv[2], nullptr, 10));

    SwarmScheduler sched;
    SchedulerConfig cfg;
    cfg.maxWorkingSetBytes = 64 * 1024;
    cfg.maxResidentLayers = 256;
    cfg.enableAsyncPrefetchThread = false;
    if (!sched.configure(cfg).has_value())
        return fail("configure");

    std::vector<ModelSlice> planA = makePlanA();
    if (!sched.submitPlan(std::move(planA)).has_value())
        return fail("submitPlan initial");

    const SwarmRuntimeStats statsStart = sched.runtimeStats();

    std::atomic<bool> stop{false};
    constexpr int kExperts = 32;

    // One fast pin worker + backoff so `submitPlan` occasionally sees `inUseSliceCount == 0`.
    std::vector<std::thread> workers;
    workers.emplace_back(
        [&sched, &stop]()
        {
            unsigned x = 7u;
            while (!stop.load(std::memory_order_acquire))
            {
                const std::size_t a = static_cast<std::size_t>(x % kExperts);
                const std::size_t b = static_cast<std::size_t>((x + 9u) % kExperts);
                x += 5u;
                const std::array<std::size_t, 2> rows{a, b};
                if (sched.pinPlanRowsBlocking(rows, 800))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    sched.unpinPlanRows(rows);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(400));
            }
        });

    // Occasional submitPlan (retries; PlanRowsHeld is normal under load).
    std::atomic<std::uint64_t> submitSuccess{0};
    std::thread submitter(
        [&sched, &stop, &submitSuccess]()
        {
            bool useA = false;
            while (!stop.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                useA = !useA;
                std::vector<ModelSlice> next = useA ? makePlanA() : makePlanB();
                for (int attempt = 0; attempt < 8; ++attempt)
                {
                    if (sched.runtimeStats().inUseSliceCount != 0)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(200));
                        continue;
                    }
                    if (sched.submitPlan(std::move(next)).has_value())
                    {
                        submitSuccess.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    next = useA ? makePlanA() : makePlanB();
                }
            }
        });

    ExpertHeatmapCaptureParams cap;
    cap.modelIndex = 0;
    cap.planRowStride = 2;
    cap.maxCells = 4096;
    cap.expertsOnly = true;

    std::vector<std::uint64_t> captureUs;
    captureUs.reserve(static_cast<std::size_t>(captureIterations));

    std::uint64_t lastPlanGen = 0;
    std::uint64_t genDriftAfterCapture = 0;

    using clock = std::chrono::steady_clock;
    for (int i = 0; i < captureIterations; ++i)
    {
        ExpertHeatmapSnapshot snap;
        const auto t0 = clock::now();
        if (!sched.captureExpertHeatmapSnapshot(cap, snap))
            return fail("captureExpertHeatmapSnapshot");
        const auto t1 = clock::now();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        captureUs.push_back(static_cast<std::uint64_t>(std::max<std::int64_t>(0, us)));

        if (snap.planGeneration < lastPlanGen)
            return fail("planGeneration decreased (capturer monotonicity)");
        lastPlanGen = snap.planGeneration;

        const std::uint64_t genAfter = sched.planGeneration();
        if (genAfter != snap.planGeneration)
            ++genDriftAfterCapture;

        // Row indices are only valid for the generation this snapshot was taken under.
        if (genAfter == snap.planGeneration)
        {
            const std::size_t nrows = sched.submittedPlanRowCount();
            for (const auto& c : snap.cells)
            {
                if (c.planRowIndex >= nrows)
                    return fail("cell planRowIndex out of range vs submittedPlanRowCount()");
            }
        }

        const std::string j = expertHeatmapSnapshotToJson(snap);
        if (j.size() < 64 || j.front() != '{' || j.back() != '}')
            return fail("JSON shape");
    }

    stop.store(true, std::memory_order_release);
    for (auto& w : workers)
        w.join();
    submitter.join();

    const SwarmRuntimeStats statsEnd = sched.runtimeStats();

    std::sort(captureUs.begin(), captureUs.end());
    const std::size_t n = captureUs.size();
    const std::uint64_t p50 = captureUs[n / 2];
    const std::uint64_t p95 = captureUs[std::min(n - 1, (n * 95) / 100)];
    const std::uint64_t p99 = captureUs[std::min(n - 1, (n * 99) / 100)];
    const std::uint64_t mx = captureUs.back();

    if (mx > maxCaptureUs)
    {
        std::fprintf(stderr, "FAIL: max capture %llu us > limit %llu us\n", static_cast<unsigned long long>(mx),
                     static_cast<unsigned long long>(maxCaptureUs));
        return 1;
    }

    const auto dStarve =
        static_cast<std::int64_t>(statsEnd.evictStarvation) - static_cast<std::int64_t>(statsStart.evictStarvation);
    const auto dRej = static_cast<std::int64_t>(statsEnd.evictionRejectedInUse) -
                      static_cast<std::int64_t>(statsStart.evictionRejectedInUse);

    std::printf("OK test_swarm_expert_heatmap_stress captures=%d p50=%lluus p95=%lluus p99=%lluus max=%lluus "
                "submit_ok=%llu gen_drift_after_capture=%llu d_evict_starvation=%lld d_eviction_rejected_in_use=%lld\n",
                captureIterations, static_cast<unsigned long long>(p50), static_cast<unsigned long long>(p95),
                static_cast<unsigned long long>(p99), static_cast<unsigned long long>(mx),
                static_cast<unsigned long long>(submitSuccess.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(genDriftAfterCapture), static_cast<long long>(dStarve),
                static_cast<long long>(dRej));
    return 0;
}
