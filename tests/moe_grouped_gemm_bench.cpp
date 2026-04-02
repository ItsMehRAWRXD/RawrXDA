// Microbenchmark: looped vs grouped MoE down-project (FP32 reference) with pack/GEMM split timing.
//
// Usage:
//   moe_grouped_gemm_bench                          → metrics to stdout (default small shape)
//   moe_grouped_gemm_bench out.csv                  → single case, default 256³/4 experts
//   moe_grouped_gemm_bench out.csv IN OUT K REP IT WARM
//   moe_grouped_gemm_bench --sweep [sweep.csv]      → grid 5×5×4×3 = 300 cases (dims × K × repeat)
//
#include "core/moe_expert_accumulation.hpp"
#include "core/moe_expert_accumulation_cache.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

using BenchClock = std::chrono::high_resolution_clock;

static double elapsedMs(BenchClock::time_point a, BenchClock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

struct BenchRow
{
    std::size_t inDim = 0;
    std::size_t outDim = 0;
    int numExperts = 0;
    int repeat = 0;
    int iters = 0;
    int warmup = 0;
    double avgMsLooped = 0;
    double avgMsPack = 0;
    double avgMsGemm = 0;
    double avgMsGroupedTotal = 0;
    double avgMsCachedPerToken = 0;
    double ratioLoopOverGroup = 0;
    double cachedPerTokOverGroupedRepeat = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    const char* policyTimed = "";
    const char* policyHeuristic = "";
};

[[nodiscard]] static const char* heuristicPolicy(std::size_t inDim, std::size_t outDim, int K)
{
    const std::uint64_t elems = inDim * outDim * static_cast<std::uint64_t>(std::max(1, K));
    // Tunable thresholds: small cached expert blocks → prefer looped; large → grouped candidate.
    if (inDim <= 512u && outDim <= 512u && K <= 8)
        return "looped_small_shape";
    if (elems >= 16ull * 1024 * 1024)
        return "grouped_large_payload";
    return "grouped_mid_shape";
}

[[nodiscard]] static const char* timedWinner(double avgLoop, double avgGroup)
{
    if (avgGroup <= 1e-12)
        return "unknown";
    return avgLoop <= avgGroup ? "looped_faster" : "grouped_faster";
}

static void printUsage()
{
    std::fprintf(stderr, "moe_grouped_gemm_bench [out.csv]\n"
                         "moe_grouped_gemm_bench [out.csv] <inDim> <outDim> <K> <repeat> <iters> <warmup>\n"
                         "moe_grouped_gemm_bench --sweep [sweep.csv]\n");
}

[[nodiscard]] static std::uint64_t estBytesPack(std::size_t inDim, std::size_t outDim, int K)
{
    if (K <= 0 || inDim == 0 || outDim == 0)
        return 0;
    const std::uint64_t ku = static_cast<std::uint64_t>(K);
    const std::uint64_t expertBytes = inDim * outDim * sizeof(float);
    const std::uint64_t packedBytes = inDim * ku * outDim * sizeof(float);
    return expertBytes * ku + packedBytes;
}

[[nodiscard]] static std::uint64_t estBytesGemm(std::size_t inDim, std::size_t outDim, int K)
{
    if (K <= 0 || inDim == 0 || outDim == 0)
        return 0;
    const std::uint64_t ku = static_cast<std::uint64_t>(K);
    const std::uint64_t n = ku * outDim;
    const std::uint64_t a = inDim * sizeof(float);
    const std::uint64_t b = inDim * n * sizeof(float);
    const std::uint64_t c = n * sizeof(float);
    return a + b + c;
}

[[nodiscard]] static int scaleItersForSize(std::size_t inDim, std::size_t outDim, int K)
{
    const std::uint64_t bytes = inDim * outDim * static_cast<std::uint64_t>(std::max(1, K)) * sizeof(float);
    if (bytes < 4ull * 1024 * 1024)
        return 200;
    if (bytes < 64ull * 1024 * 1024)
        return 50;
    if (bytes < 256ull * 1024 * 1024)
        return 20;
    return 10;
}

[[nodiscard]] static BenchRow runCase(std::size_t kInDim, std::size_t kOutDim, int kExperts, int kRepeatSameExperts,
                                      int kIters, int kWarmup)
{
    BenchRow row;
    row.inDim = kInDim;
    row.outDim = kOutDim;
    row.numExperts = kExperts;
    row.repeat = kRepeatSameExperts;
    row.iters = kIters;
    row.warmup = kWarmup;

    if (kExperts <= 0 || kInDim == 0 || kOutDim == 0 || kIters <= 0 || kWarmup < 0 || kRepeatSameExperts < 1)
    {
        row.policyTimed = "invalid_params";
        return row;
    }

    std::mt19937 rng(0xDEADBEEFu);
    std::uniform_real_distribution<float> dist(-0.05f, 0.05f);

    std::vector<float> hidden(kInDim);
    for (auto& v : hidden)
        v = dist(rng);

    std::vector<std::vector<float>> wStore(static_cast<std::size_t>(kExperts));
    std::vector<const float*> wPtrs(static_cast<std::size_t>(kExperts));
    for (int e = 0; e < kExperts; ++e)
    {
        wStore[static_cast<std::size_t>(e)].resize(kInDim * kOutDim);
        for (auto& v : wStore[static_cast<std::size_t>(e)])
            v = dist(rng);
        wPtrs[static_cast<std::size_t>(e)] = wStore[static_cast<std::size_t>(e)].data();
    }

    std::vector<float> weights(static_cast<std::size_t>(kExperts), 1.f / static_cast<float>(kExperts));
    std::vector<float> acc(kOutDim);
    std::vector<float> workspace;
    const std::size_t Ntot = static_cast<std::size_t>(kExperts) * kOutDim;
    std::vector<float> packed(kInDim * Ntot);

    auto benchLooped = [&]()
    {
        std::fill(acc.begin(), acc.end(), 0.f);
        RawrXD::MoEAccumRef::moeDownProjectLoopedF32(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(),
                                                     weights.data(), acc.data());
    };

    auto benchGroupedNoCache = [&]()
    {
        std::fill(acc.begin(), acc.end(), 0.f);
        RawrXD::MoEAccumRef::moeDownProjectGroupedF32(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(),
                                                      weights.data(), acc.data(), workspace);
    };

    auto benchSplitTimed = [&](double& msPack, double& msGemm)
    {
        std::fill(acc.begin(), acc.end(), 0.f);
        auto t0 = BenchClock::now();
        RawrXD::MoEAccumRef::packExpertDownWeightsF32(kInDim, kOutDim, kExperts, wPtrs.data(), packed.data());
        msPack += elapsedMs(t0, BenchClock::now());

        workspace.resize(Ntot);
        std::fill(workspace.begin(), workspace.end(), 0.f);
        t0 = BenchClock::now();
        RawrXD::MoEAccumRef::gemmRowMajorAccumF32(1, Ntot, kInDim, hidden.data(), packed.data(), workspace.data());
        msGemm += elapsedMs(t0, BenchClock::now());

        for (int e = 0; e < kExperts; ++e)
        {
            const float w = weights[static_cast<std::size_t>(e)];
            if (w == 0.f)
                continue;
            const float* block = workspace.data() + static_cast<std::size_t>(e) * kOutDim;
            for (std::size_t j = 0; j < kOutDim; ++j)
                acc[j] += w * block[j];
        }
    };

    RawrXD::MoEAccumRef::ExpertDownPackCache cache(8);
    auto benchGroupedCached = [&]()
    {
        std::fill(acc.begin(), acc.end(), 0.f);
        cache.downProjectGroupedCached(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(), weights.data(),
                                       acc.data(), workspace);
    };

    for (int w = 0; w < kWarmup; ++w)
    {
        benchLooped();
        benchGroupedNoCache();
        double wp = 0, wg = 0;
        benchSplitTimed(wp, wg);
        (void)wp;
        (void)wg;
        cache.clear();
        for (int r = 0; r < kRepeatSameExperts; ++r)
            benchGroupedCached();
    }

    double msLoop = 0, msGroup = 0, msPack = 0, msGemm = 0, msCached = 0;
    cache.clear();
    for (int i = 0; i < kIters; ++i)
    {
        auto t0 = BenchClock::now();
        benchLooped();
        msLoop += elapsedMs(t0, BenchClock::now());

        t0 = BenchClock::now();
        benchGroupedNoCache();
        msGroup += elapsedMs(t0, BenchClock::now());

        benchSplitTimed(msPack, msGemm);

        cache.clearEntriesOnly();
        t0 = BenchClock::now();
        for (int r = 0; r < kRepeatSameExperts; ++r)
            benchGroupedCached();
        msCached += elapsedMs(t0, BenchClock::now());
    }

    row.avgMsLooped = msLoop / static_cast<double>(kIters);
    row.avgMsPack = msPack / static_cast<double>(kIters);
    row.avgMsGemm = msGemm / static_cast<double>(kIters);
    row.avgMsGroupedTotal = msGroup / static_cast<double>(kIters);
    row.avgMsCachedPerToken = msCached / static_cast<double>(kIters * kRepeatSameExperts);
    row.ratioLoopOverGroup = row.avgMsGroupedTotal > 1e-12 ? row.avgMsLooped / row.avgMsGroupedTotal : 0.;
    const double avgGroupedPerTokenIfRepeated = row.avgMsGroupedTotal / static_cast<double>(kRepeatSameExperts);
    row.cachedPerTokOverGroupedRepeat =
        avgGroupedPerTokenIfRepeated > 1e-12 ? row.avgMsCachedPerToken / avgGroupedPerTokenIfRepeated : 0.;
    row.cacheHits = cache.hits();
    row.cacheMisses = cache.misses();
    row.policyTimed = timedWinner(row.avgMsLooped, row.avgMsGroupedTotal);
    row.policyHeuristic = heuristicPolicy(kInDim, kOutDim, kExperts);
    return row;
}

static void writeCsvHeader(FILE* out)
{
    std::fprintf(out, "inDim,outDim,numExperts,repeat,iters,warmup,"
                      "avg_ms_looped,avg_ms_pack,avg_ms_gemm,avg_ms_grouped_total,avg_ms_cached_per_token,"
                      "ratio_loop_over_group,cached_per_token_over_grouped_repeat,"
                      "cache_hits,cache_misses,policy_timed,policy_heuristic,"
                      "avg_us_looped,avg_us_pack,avg_us_gemm,avg_us_split_total,avg_us_grouped_total,"
                      "avg_us_cached_per_token,est_bytes_pack,est_bytes_gemm\n");
}

static void writeCsvDataRow(FILE* out, const BenchRow& r)
{
    const double usLoop = r.avgMsLooped * 1000.0;
    const double usPack = r.avgMsPack * 1000.0;
    const double usGemm = r.avgMsGemm * 1000.0;
    const double usSplit = usPack + usGemm;
    const double usGroup = r.avgMsGroupedTotal * 1000.0;
    const double usCachedTok = r.avgMsCachedPerToken * 1000.0;
    const std::uint64_t bPack = estBytesPack(r.inDim, r.outDim, r.numExperts);
    const std::uint64_t bGemm = estBytesGemm(r.inDim, r.outDim, r.numExperts);

    std::fprintf(out,
                 "%zu,%zu,%d,%d,%d,%d,%g,%g,%g,%g,%g,%g,%g,%llu,%llu,%s,%s,"
                 "%g,%g,%g,%g,%g,%g,%llu,%llu\n",
                 r.inDim, r.outDim, r.numExperts, r.repeat, r.iters, r.warmup, r.avgMsLooped, r.avgMsPack, r.avgMsGemm,
                 r.avgMsGroupedTotal, r.avgMsCachedPerToken, r.ratioLoopOverGroup, r.cachedPerTokOverGroupedRepeat,
                 static_cast<unsigned long long>(r.cacheHits), static_cast<unsigned long long>(r.cacheMisses),
                 r.policyTimed, r.policyHeuristic, usLoop, usPack, usGemm, usSplit, usGroup, usCachedTok,
                 static_cast<unsigned long long>(bPack), static_cast<unsigned long long>(bGemm));
}

static constexpr int kSweepCaseCount = 5 * 5 * 4 * 3;

int runSweep(FILE* out)
{
    static const std::size_t kDims[] = {256, 512, 1024, 2048, 4096};
    static const int kKs[] = {2, 4, 8, 16};
    static const int kRepeats[] = {1, 8, 32};

    writeCsvHeader(out);
    int n = 0;
    for (std::size_t inD : kDims)
    {
        for (std::size_t outD : kDims)
        {
            for (int K : kKs)
            {
                for (int rep : kRepeats)
                {
                    const int iters = scaleItersForSize(inD, outD, K);
                    const int warm = std::max(2, iters / 10);
                    std::fprintf(stderr, "sweep case %d: in=%zu out=%zu K=%d rep=%d it=%d warm=%d\n", ++n, inD, outD, K,
                                 rep, iters, warm);
                    BenchRow r = runCase(inD, outD, K, rep, iters, warm);
                    writeCsvDataRow(out, r);
                    std::fflush(out);
                }
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc >= 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0))
    {
        printUsage();
        return 0;
    }

    if (argc >= 2 && std::strcmp(argv[1], "--sweep") == 0)
    {
        const char* path = (argc >= 3 && argv[2][0]) ? argv[2] : nullptr;
        FILE* out = stdout;
        if (path)
        {
            out = std::fopen(path, "w");
            if (!out)
            {
                std::fprintf(stderr, "Failed to open %s\n", path);
                return 1;
            }
        }
        const int rc = runSweep(out);
        if (out != stdout)
            std::fclose(out);
        std::fprintf(stderr, "OK moe_grouped_gemm_bench --sweep (%d cases)\n", kSweepCaseCount);
        return rc;
    }

    const char* csvPath = nullptr;
    std::size_t inD = 256, outD = 256;
    int K = 4, rep = 8, iters = 200, warm = 20;

    int argi = 1;
    if (argc >= 2 && argv[1][0] != '\0' && std::strcmp(argv[1], "--sweep") != 0)
    {
        csvPath = argv[1];
        argi = 2;
    }
    if (argc >= argi + 5)
    {
        inD = static_cast<std::size_t>(std::strtoull(argv[argi], nullptr, 10));
        outD = static_cast<std::size_t>(std::strtoull(argv[argi + 1], nullptr, 10));
        K = std::atoi(argv[argi + 2]);
        rep = std::atoi(argv[argi + 3]);
        iters = std::atoi(argv[argi + 4]);
        warm = std::atoi(argv[argi + 5]);
    }

    BenchRow r = runCase(inD, outD, K, rep, iters, warm);

    FILE* out = stdout;
    if (csvPath)
    {
        out = std::fopen(csvPath, "w");
        if (!out)
        {
            std::fprintf(stderr, "Failed to open %s\n", csvPath);
            return 1;
        }
    }

    writeCsvHeader(out);
    writeCsvDataRow(out, r);

    if (out != stdout)
        std::fclose(out);

    std::printf("OK moe_grouped_gemm_bench in=%zu out=%zu K=%d looped=%.4gms pack=%.4gms gemm=%.4gms grouped=%.4gms "
                "cached/tok~=%.4gms ratio_l/g=%.4g timed=%s heuristic=%s (pack+gemm vs grouped_total sanity: %g/%g)\n",
                inD, outD, K, r.avgMsLooped, r.avgMsPack, r.avgMsGemm, r.avgMsGroupedTotal, r.avgMsCachedPerToken,
                r.ratioLoopOverGroup, r.policyTimed, r.policyHeuristic, r.avgMsPack + r.avgMsGemm, r.avgMsGroupedTotal);
    return 0;
}
