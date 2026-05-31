// Verifies MoE HUD metric normalization keeps aggregate fallback >= split fallback counters.
#include "rawrxd_inference.h"

#include <cstdio>

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
    {
        MoEPackHudMetrics m{};
        m.groupedFallbacks = 3;
        m.groupedWeightedFallbacks = 2;
        m.groupedSingleExpertFallbacks = 5;
        NormalizeMoEPackHudMetrics(m);
        if (m.groupedFallbacks != 7)
            return fail("aggregate fallback should be raised to split sum");
    }

    {
        MoEPackHudMetrics m{};
        m.groupedFallbacks = 11;
        m.groupedWeightedFallbacks = 3;
        m.groupedSingleExpertFallbacks = 4;
        NormalizeMoEPackHudMetrics(m);
        if (m.groupedFallbacks != 11)
            return fail("aggregate fallback should be preserved when already larger");
    }

    {
        MoEPackHudMetrics m{};
        m.groupedFallbacks = 0;
        m.groupedWeightedFallbacks = 0;
        m.groupedSingleExpertFallbacks = 0;
        NormalizeMoEPackHudMetrics(m);
        if (m.groupedFallbacks != 0)
            return fail("zero counters should remain zero");
    }

    std::fprintf(stderr, "OK test_moe_pack_hud_metrics\n");
    return 0;
}
