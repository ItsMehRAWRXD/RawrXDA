// Validates FP32 weighted expert down-project: looped vs grouped reference paths match (MoE mixture math).
// Optional: write JSON report path as argv[1]. Exit 0 on success.
#include "core/moe_expert_accumulation.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace
{
int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

void writeJsonReport(const char* path, bool ok, float maxDiff, std::size_t inDim, std::size_t outDim, int numExperts,
                     int trials, std::uint64_t seed)
{
    FILE* f = stdout;
    if (path && path[0])
    {
        f = std::fopen(path, "w");
        if (!f)
        {
            std::fprintf(stderr, "WARN: could not open %s for JSON report\n", path);
            f = stdout;
        }
    }
    std::fprintf(f,
                 "{\"ok\":%s,\"max_abs_diff\":%.9g,\"inDim\":%zu,\"outDim\":%zu,\"numExperts\":%d,\"trials\":%d,"
                 "\"seed\":%llu}\n",
                 ok ? "true" : "false", static_cast<double>(maxDiff), inDim, outDim, numExperts, trials,
                 static_cast<unsigned long long>(seed));
    if (f != stdout && f)
        std::fclose(f);
}
}  // namespace

int main(int argc, char** argv)
{
    const char* jsonPath = (argc >= 2) ? argv[1] : "";

    constexpr std::uint64_t kSeed = 0xC0FFEE600Bu;
    constexpr int kTrials = 64;
    constexpr std::size_t kInDim = 48;
    constexpr std::size_t kOutDim = 40;
    constexpr int kExperts = 5;
    constexpr float kTol = 5e-4f;

    std::mt19937 rng(static_cast<std::mt19937::result_type>(kSeed));
    std::uniform_real_distribution<float> dist(-0.25f, 0.25f);

    float worst = 0.f;

    for (int t = 0; t < kTrials; ++t)
    {
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

        std::vector<float> weights(static_cast<std::size_t>(kExperts), 0.f);
        float s = 0.f;
        for (int e = 0; e < kExperts; ++e)
        {
            const float x = std::abs(dist(rng)) + 0.01f;
            weights[static_cast<std::size_t>(e)] = x;
            s += x;
        }
        for (float& w : weights)
            w /= s;

        std::vector<float> accLo(kOutDim, 0.f);
        std::vector<float> accGrp(kOutDim, 0.f);
        std::vector<float> workspace;

        RawrXD::MoEAccumRef::moeDownProjectLoopedF32(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(),
                                                     weights.data(), accLo.data());
        RawrXD::MoEAccumRef::moeDownProjectGroupedF32(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(),
                                                      weights.data(), accGrp.data(), workspace);

        const float d = RawrXD::MoEAccumRef::maxAbsDiffF32(kOutDim, accLo.data(), accGrp.data());
        worst = std::max(worst, d);
        if (d > kTol)
        {
            std::fprintf(stderr, "trial %d diff %g exceeds tol %g\n", t, static_cast<double>(d),
                         static_cast<double>(kTol));
            writeJsonReport(jsonPath, false, worst, kInDim, kOutDim, kExperts, kTrials, kSeed);
            return 1;
        }
    }

    // First-expert-only path should differ from full mixture (sanity).
    {
        std::vector<float> hidden(kInDim);
        for (auto& v : hidden)
            v = 0.1f;
        std::vector<std::vector<float>> wStore(static_cast<std::size_t>(kExperts));
        std::vector<const float*> wPtrs(static_cast<std::size_t>(kExperts));
        for (int e = 0; e < kExperts; ++e)
        {
            wStore[static_cast<std::size_t>(e)].assign(kInDim * kOutDim, static_cast<float>(e + 1) * 0.01f);
            wPtrs[static_cast<std::size_t>(e)] = wStore[static_cast<std::size_t>(e)].data();
        }
        std::vector<float> mixW(static_cast<std::size_t>(kExperts), 1.f / static_cast<float>(kExperts));
        std::vector<float> accMix(kOutDim, 0.f);
        std::vector<float> accFirst(kOutDim, 0.f);
        std::vector<float> ws;
        RawrXD::MoEAccumRef::moeDownProjectLoopedF32(hidden.data(), kInDim, kOutDim, kExperts, wPtrs.data(),
                                                     mixW.data(), accMix.data());
        const float w0 = 1.f;
        RawrXD::MoEAccumRef::moeDownProjectLoopedF32(hidden.data(), kInDim, kOutDim, 1, wPtrs.data(), &w0,
                                                     accFirst.data());
        const float sanity = RawrXD::MoEAccumRef::maxAbsDiffF32(kOutDim, accMix.data(), accFirst.data());
        if (sanity < 1e-3f)
            return fail("mixture vs first-expert-only should differ for this synthetic data");
    }

    writeJsonReport(jsonPath, true, worst, kInDim, kOutDim, kExperts, kTrials, kSeed);
    std::printf("OK test_moe_mixture_reference max_abs_diff=%.6g trials=%d\n", static_cast<double>(worst), kTrials);
    return 0;
}
