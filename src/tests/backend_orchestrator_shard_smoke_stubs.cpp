#include <cstdint>
#include <cstring>
#include <limits>
#include <atomic>
#include <cstdlib>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace codec {

namespace {
constexpr uint8_t kMagic0 = 'B';
constexpr uint8_t kMagic1 = 'X';
constexpr uint8_t kMagic2 = 'R';
constexpr uint8_t kVersion = 1;

std::atomic<uint64_t> g_deflateCalls{0};
std::atomic<uint64_t> g_inflateCalls{0};
std::atomic<uint64_t> g_inflateRejects{0};
std::atomic<uint64_t> g_accumulateCalls{0};
std::atomic<uint64_t> g_accumulateScaledCalls{0};
std::atomic<uint64_t> g_invalidArgs{0};
std::atomic<uint64_t> g_avxDetectCalls{0};
std::atomic<uint64_t> g_avxDetected{0};
std::atomic<uint64_t> g_lastInputBytes{0};
std::atomic<uint64_t> g_totalInputBytes{0};
std::atomic<uint64_t> g_clampedCountCalls{0};
std::atomic<uint64_t> g_lastStatusCode{0};

int clampCountByPolicy(int count)
{
    if (count <= 0) {
        return count;
    }

    int cap = 65536;
    if (const char* env = std::getenv("RAWRXD_SHARD_STUB_MAX_COUNT")) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(env, &end, 10);
        if (end != env && parsed > 0) {
            cap = static_cast<int>(parsed > 1000000ULL ? 1000000ULL : parsed);
        }
    }

    if (count > cap) {
        g_clampedCountCalls.fetch_add(1, std::memory_order_relaxed);
        g_lastStatusCode.store(2, std::memory_order_relaxed); // clamped
        return cap;
    }
    return count;
}
}

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success)
{
    g_deflateCalls.fetch_add(1, std::memory_order_relaxed);
    std::vector<uint8_t> out;
    out.reserve(data.size() + 4);
    out.push_back(kMagic0);
    out.push_back(kMagic1);
    out.push_back(kMagic2);
    out.push_back(kVersion);

    size_t i = 0;
    while (i < data.size()) {
        const uint8_t value = data[i];
        uint8_t run = 1;
        while (i + run < data.size() && data[i + run] == value && run < std::numeric_limits<uint8_t>::max()) {
            ++run;
        }

        out.push_back(run);
        out.push_back(value);
        i += run;
    }

    if (success) {
        *success = true;
    }
    return out;
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success)
{
    g_inflateCalls.fetch_add(1, std::memory_order_relaxed);
    if (data.size() < 4 || data[0] != kMagic0 || data[1] != kMagic1 || data[2] != kMagic2 || data[3] != kVersion) {
        g_inflateRejects.fetch_add(1, std::memory_order_relaxed);
        if (success) {
            *success = false;
        }
        return {};
    }

    if (((data.size() - 4) & 1u) != 0u) {
        g_inflateRejects.fetch_add(1, std::memory_order_relaxed);
        if (success) {
            *success = false;
        }
        return {};
    }

    std::vector<uint8_t> out;
    for (size_t i = 4; i + 1 < data.size(); i += 2) {
        const uint8_t run = data[i];
        const uint8_t value = data[i + 1];
        if (run == 0) {
            g_inflateRejects.fetch_add(1, std::memory_order_relaxed);
            if (success) {
                *success = false;
            }
            return {};
        }
        out.insert(out.end(), run, value);
    }

    if (success) {
        *success = true;
    }
    return out;
}

}  // namespace codec

namespace brutal {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data)
{
    bool ok = false;
    std::vector<uint8_t> out = codec::deflate(data, &ok);
    return ok ? out : std::vector<uint8_t>{};
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& data)
{
    bool ok = false;
    std::vector<uint8_t> out = codec::inflate(data, &ok);
    return ok ? out : std::vector<uint8_t>{};
}

}  // namespace brutal

extern "C" int rawr_cpu_has_avx512()
{
    g_avxDetectCalls.fetch_add(1, std::memory_order_relaxed);
#if defined(_MSC_VER) && defined(_M_X64)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 0, 0);
    if (regs[0] < 7) {
        return 0;
    }

    __cpuidex(regs, 1, 0);
    const bool osxsave = (regs[2] & (1 << 27)) != 0;
    if (!osxsave) {
        return 0;
    }

    const unsigned __int64 xcr0 = _xgetbv(0);
    const unsigned __int64 xmmYmmMask = 0x6;
    const unsigned __int64 opmaskZmmMask = 0xE0;
    if ((xcr0 & xmmYmmMask) != xmmYmmMask || (xcr0 & opmaskZmmMask) != opmaskZmmMask) {
        return 0;
    }

    __cpuidex(regs, 7, 0);
    const bool avx512f = (regs[1] & (1 << 16)) != 0;
    if (avx512f) {
        g_avxDetected.fetch_add(1, std::memory_order_relaxed);
    }
    return avx512f ? 1 : 0;
#else
    return 0;
#endif
}

extern "C" int kv_accumulate_avx512(const float* src, float* dst, int count)
{
    g_accumulateCalls.fetch_add(1, std::memory_order_relaxed);
    if (src == nullptr || dst == nullptr || count <= 0) {
        g_invalidArgs.fetch_add(1, std::memory_order_relaxed);
        g_lastStatusCode.store(1, std::memory_order_relaxed); // invalid args
        return 0;
    }

    count = clampCountByPolicy(count);
    const uint64_t bytes = static_cast<uint64_t>(count) * sizeof(float);
    g_lastInputBytes.store(bytes, std::memory_order_relaxed);
    g_totalInputBytes.fetch_add(bytes, std::memory_order_relaxed);

    for (int i = 0; i < count; ++i) {
        dst[i] += src[i];
    }
    g_lastStatusCode.store(3, std::memory_order_relaxed); // success
    return 1;
}

extern "C" int kv_accumulate_scaled_avx512(const float* src, float* dst, int count, float scale)
{
    g_accumulateScaledCalls.fetch_add(1, std::memory_order_relaxed);
    if (src == nullptr || dst == nullptr || count <= 0) {
        g_invalidArgs.fetch_add(1, std::memory_order_relaxed);
        g_lastStatusCode.store(1, std::memory_order_relaxed); // invalid args
        return 0;
    }

    count = clampCountByPolicy(count);
    const uint64_t bytes = static_cast<uint64_t>(count) * sizeof(float);
    g_lastInputBytes.store(bytes, std::memory_order_relaxed);
    g_totalInputBytes.fetch_add(bytes, std::memory_order_relaxed);

    for (int i = 0; i < count; ++i) {
        dst[i] += src[i] * scale;
    }
    g_lastStatusCode.store(3, std::memory_order_relaxed); // success
    return 1;
}

extern "C" unsigned __int64 backend_orchestrator_shard_smoke_stub_stats()
{
    // [63:56] avx_detected, [55:48] avx_detect_calls, [47:40] invalid_args,
    // [39:32] inflate_rejects, [31:24] inflate_calls, [23:16] deflate_calls,
    // [15:8] accumulate_scaled_calls, [7:0] accumulate_calls
    const uint64_t avxDetected = g_avxDetected.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t avxDetectCalls = g_avxDetectCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t invalidArgs = g_invalidArgs.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t inflateRejects = g_inflateRejects.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t inflateCalls = g_inflateCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t deflateCalls = g_deflateCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t accumulateScaled = g_accumulateScaledCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t accumulate = g_accumulateCalls.load(std::memory_order_relaxed) & 0xFFu;
    return (avxDetected << 56) | (avxDetectCalls << 48) | (invalidArgs << 40) | (inflateRejects << 32) |
           (inflateCalls << 24) | (deflateCalls << 16) | (accumulateScaled << 8) | accumulate;
}

extern "C" unsigned __int64 backend_orchestrator_shard_smoke_stub_stats_ext()
{
    // [63:56] last_status, [55:48] clamped_calls, [47:40] invalid_args,
    // [39:32] last_input_kib, [31:0] total_input_bytes(low32)
    const uint64_t status = g_lastStatusCode.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t clamped = g_clampedCountCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t invalid = g_invalidArgs.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t lastKiB = (g_lastInputBytes.load(std::memory_order_relaxed) >> 10) & 0xFFu;
    const uint64_t totalBytes = g_totalInputBytes.load(std::memory_order_relaxed) & 0xFFFFFFFFu;
    return (status << 56) | (clamped << 48) | (invalid << 40) | (lastKiB << 32) | totalBytes;
}

extern "C" void backend_orchestrator_shard_smoke_stub_reset_stats()
{
    g_deflateCalls.store(0, std::memory_order_relaxed);
    g_inflateCalls.store(0, std::memory_order_relaxed);
    g_inflateRejects.store(0, std::memory_order_relaxed);
    g_accumulateCalls.store(0, std::memory_order_relaxed);
    g_accumulateScaledCalls.store(0, std::memory_order_relaxed);
    g_invalidArgs.store(0, std::memory_order_relaxed);
    g_avxDetectCalls.store(0, std::memory_order_relaxed);
    g_avxDetected.store(0, std::memory_order_relaxed);
    g_lastInputBytes.store(0, std::memory_order_relaxed);
    g_totalInputBytes.store(0, std::memory_order_relaxed);
    g_clampedCountCalls.store(0, std::memory_order_relaxed);
    g_lastStatusCode.store(0, std::memory_order_relaxed);
}
