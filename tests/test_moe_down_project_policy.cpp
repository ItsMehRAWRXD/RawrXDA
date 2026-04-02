// Unit checks for MoE down-project path policy (matches RawrXDTransformer::chooseMoEDownProjectPath thresholds).
#include "core/moe_down_project_policy.hpp"

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
    using RawrXD::MoEDownProject::choosePath;
    using RawrXD::MoEDownProject::Path;
    using RawrXD::MoEDownProject::PolicyParams;
    using RawrXD::MoEDownProject::workProduct;

    PolicyParams def;
    if (workProduct(100, 100, 4) != 40000.0)
        return fail("workProduct 100,100,4");
    if (choosePath(100, 100, 4, 100.0, def) != Path::Looped)
        return fail("small work should be looped");
    if (choosePath(1024, 1024, 4, 100.0, def) != Path::GroupedCached)
        return fail("large work + high reuse should be grouped+cached");
    if (choosePath(1024, 1024, 4, 1.0, def) != Path::Looped)
        return fail("large work + low reuse should stay looped");

    PolicyParams tight{50000.0, 4.0};
    if (choosePath(200, 200, 4, 4.0, tight) != Path::GroupedCached)
        return fail("tight threshold: 200*200*4=160k >= 50k and reuse>=4");

    // Defaults documented on RawrXDTransformer::Config (1e6, 8.0, 8.0)
    if (choosePath(256, 256, 4, 8.0, def) != Path::Looped)
        return fail("256*256*4 < 1e6 -> looped (transformer default thresholds)");
    if (choosePath(1024, 1024, 4, 8.0, def) != Path::GroupedCached)
        return fail("1024*1024*4 > 1e6 and reuse 8 -> grouped+cached");

    PolicyParams lowReuse = def;
    lowReuse.minReuseForGrouped = 8.0;
    if (choosePath(1024, 1024, 4, 1.0, lowReuse) != Path::Looped)
        return fail("moe_down_expected_reuse_estimate=1 analog -> looped");

    std::fprintf(stderr, "OK test_moe_down_project_policy\n");
    return 0;
}
