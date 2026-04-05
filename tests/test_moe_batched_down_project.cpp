// Parity: batched packed down-project vs per-expert looped down (MoEAccumRef).
#include "core/moe_expert_accumulation.hpp"

#include <cstdio>
#include <random>
#include <vector>

namespace
{
int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}
}  // namespace

int main()
{
    using RawrXD::MoEAccumRef::maxAbsDiffF32;
    using RawrXD::MoEAccumRef::moeDownProjectBatchedExpertsFromPackedF32;
    using RawrXD::MoEAccumRef::packExpertDownWeightsF32;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    const int cases[][4] = {
        {8, 12, 3, 0},
        {32, 48, 4, 0},
        {64, 64, 2, 0},
        {16, 24, 8, 0},
        {24, 32, 1, 1},
    };

    for (const auto& c : cases)
    {
        const int inDim = c[0];
        const int outDim = c[1];
        const int K = c[2];
        const bool forceUnitWeight = (c[3] != 0);
        const std::size_t inD = static_cast<std::size_t>(inDim);
        const std::size_t outD = static_cast<std::size_t>(outDim);

        std::vector<std::vector<float>> wRows(static_cast<std::size_t>(K));
        std::vector<const float*> wPtrs(static_cast<std::size_t>(K));
        for (int e = 0; e < K; ++e)
        {
            wRows[static_cast<std::size_t>(e)].resize(inD * outD);
            for (auto& v : wRows[static_cast<std::size_t>(e)])
                v = dist(rng);
            wPtrs[static_cast<std::size_t>(e)] = wRows[static_cast<std::size_t>(e)].data();
        }

        std::vector<float> packed(inD * outD * static_cast<std::size_t>(K));
        packExpertDownWeightsF32(inD, outD, K, wPtrs.data(), packed.data());

        std::vector<float> hiddens(static_cast<std::size_t>(K) * inD);
        for (auto& v : hiddens)
            v = dist(rng);

        std::vector<float> weights(static_cast<std::size_t>(K));
        if (forceUnitWeight)
        {
            weights[0] = 1.f;
        }
        else
        {
            weights[0] = 0.f;  // zero-weight expert
            for (int e = 1; e < K; ++e)
                weights[static_cast<std::size_t>(e)] = dist(rng) + 0.5f;
        }

        // Reference: per-expert hidden (moeDownProjectLoopedF32 uses one shared hidden — not applicable here).
        std::vector<float> accRef(outD, 0.f);
        for (int e = 0; e < K; ++e)
        {
            const float w = weights[static_cast<std::size_t>(e)];
            if (w == 0.f)
                continue;
            const float* h = hiddens.data() + static_cast<std::size_t>(e) * inD;
            const float* W = wPtrs[static_cast<std::size_t>(e)];
            for (std::size_t j = 0; j < outD; ++j)
            {
                float s = 0.f;
                for (std::size_t i = 0; i < inD; ++i)
                    s += h[i] * W[i * outD + j];
                accRef[j] += w * s;
            }
        }

        std::vector<float> accBatch(outD, 0.f);
        moeDownProjectBatchedExpertsFromPackedF32(hiddens.data(), inD, outD, K, packed.data(), weights.data(),
                                                  accBatch.data());

        const float err = maxAbsDiffF32(outD, accRef.data(), accBatch.data());
        if (err > 1e-3f)
            return fail("batched vs reference mismatch");
    }

    std::fprintf(stderr, "OK test_moe_batched_down_project\n");
    return 0;
}
