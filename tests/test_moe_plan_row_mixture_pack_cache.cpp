// Unit tests for MoEMixturePlanPackCache (LRU + hit/miss/eviction stats).
#include "core/moe_plan_row_mixture_pack_cache.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>

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
    using RawrXD::MoEIntegr::MoEMixturePlanPackCache;

    MoEMixturePlanPackCache cache(2);
    const float* p = nullptr;
    std::size_t n = 0;
    if (cache.tryGet("a", &p, &n))
        return fail("empty cache should miss");
    if (cache.hits() != 0 || cache.misses() != 1)
        return fail("miss count");

    std::vector<float> v1(6, 1.f);
    std::vector<float> v2(4, 2.f);
    cache.put("a", std::move(v1));
    if (!cache.tryGet("a", &p, &n) || n != 6 || p == nullptr || std::fabs(p[0] - 1.f) > 1e-6f)
        return fail("get a");
    if (cache.hits() != 1 || cache.misses() != 1)
        return fail("hits after a");

    cache.put("b", std::vector<float>(8, 3.f));
    cache.put("c", std::vector<float>(2, 4.f));
    if (cache.evictions() < 1)
        return fail("capacity 2 should evict when inserting third");

    if (cache.tryGet("a", &p, &n))
        return fail("a should be evicted");

    if (!cache.tryGet("c", &p, &n) || n != 2)
        return fail("c should exist");

    const std::uint64_t hitsBefore = cache.hits();
    cache.clearEntriesOnly();
    if (cache.tryGet("c", &p, &n))
        return fail("clearEntriesOnly should drop entries");
    if (cache.hits() != hitsBefore)
        return fail("clearEntriesOnly must preserve hit counter");
    if (cache.currentPackedBytes() != 0)
        return fail("clearEntriesOnly should zero byte accounting");

    // Byte cap: 20 bytes => at most 5 floats total; entry limit high so eviction is byte-driven.
    MoEMixturePlanPackCache byteCache(64, 20);
    byteCache.put("x", std::vector<float>(3, 1.f));  // 12 B
    byteCache.put("y", std::vector<float>(3, 2.f));  // evict x, keep 12 B
    if (byteCache.tryGet("x", &p, &n))
        return fail("byte cap should evict LRU when adding second same-size pack");
    if (!byteCache.tryGet("y", &p, &n) || n != 3)
        return fail("y should remain after byte eviction");
    if (byteCache.currentPackedBytes() != 12)
        return fail("currentPackedBytes after one entry of 3 floats");

    // In-place grow: MRU entry grows over cap => evict older LRU tail until under cap.
    MoEMixturePlanPackCache growCache(8, 32);
    growCache.put("a", std::vector<float>(2, 0.f));  // 8 B
    growCache.put("b", std::vector<float>(2, 0.f));  // 8 B, total 16
    growCache.put("a", std::vector<float>(6, 7.f));  // MRU a now 24 B; total would be 32 — OK at edge
    if (growCache.currentPackedBytes() != 32)
        return fail("two entries 8+24 should equal 32 bytes");
    growCache.put("a", std::vector<float>(7, 7.f));  // a = 28 B, total 28+8=36 > 32 → evict b
    if (growCache.tryGet("b", &p, &n))
        return fail("growing MRU entry should evict LRU to satisfy byte cap");
    if (!growCache.tryGet("a", &p, &n) || n != 7)
        return fail("a should hold grown buffer");

    // Reverse index: row-targeted invalidation removes only matching keys.
    MoEMixturePlanPackCache rowCache(16, 0);
    const std::size_t rowsA[] = {10u, 20u};
    const std::size_t rowsB[] = {20u, 30u};
    rowCache.put("ka", std::vector<float>(4, 1.f), std::span<const std::size_t>(rowsA, 2u));
    rowCache.put("kb", std::vector<float>(4, 2.f), std::span<const std::size_t>(rowsB, 2u));
    const std::size_t evict20[] = {20u};
    const std::uint32_t inv = rowCache.invalidateEntriesReferencingPlanRows(std::span<const std::size_t>(evict20, 1u));
    if (inv != 2u)
        return fail("row 20 should invalidate both keys");
    if (rowCache.tryGet("ka", &p, &n) || rowCache.tryGet("kb", &p, &n))
        return fail("both keys should be gone after row invalidation");
    if (rowCache.rowInvalidationEvictions() < 2u)
        return fail("rowInvalidationEvictions should count selective removals");

    std::fprintf(stderr, "OK test_moe_plan_row_mixture_pack_cache hits=%llu misses=%llu evictions=%llu\n",
                 static_cast<unsigned long long>(cache.hits()), static_cast<unsigned long long>(cache.misses()),
                 static_cast<unsigned long long>(cache.evictions()));
    return 0;
}
