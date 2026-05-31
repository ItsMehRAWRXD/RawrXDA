#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <algorithm>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::atomic<bool> g_cachedFull{false};
std::atomic<uint64_t> g_lastProbeMs{0};
std::atomic<uint64_t> g_probeCount{0};
std::atomic<uint64_t> g_fileProbeCount{0};
std::atomic<uint64_t> g_cacheHitCount{0};
std::atomic<uint64_t> g_cacheRefreshCount{0};
std::atomic<uint64_t> g_forceOverrideCount{0};
std::atomic<uint64_t> g_invalidCacheMsCount{0};
std::atomic<uint64_t> g_lastResolvedCacheMs{0};
std::atomic<uint64_t> g_lastModeMask{0};

constexpr uint64_t kDefaultCacheMs = 250;

std::string trimCopy(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(start, end - start);
}

uint64_t parseCacheMs()
{
    if (const char* env = std::getenv("RAWRXD_BEACON_CACHE_MS")) {
        try {
            const uint64_t parsed = static_cast<uint64_t>(std::stoull(env));
            const uint64_t resolved = (parsed > 5000) ? 5000 : parsed;
            g_lastResolvedCacheMs.store(resolved, std::memory_order_relaxed);
            return resolved;
        } catch (...) {
            g_invalidCacheMsCount.fetch_add(1, std::memory_order_relaxed);
            g_lastResolvedCacheMs.store(kDefaultCacheMs, std::memory_order_relaxed);
            return kDefaultCacheMs;
        }
    }
    g_lastResolvedCacheMs.store(kDefaultCacheMs, std::memory_order_relaxed);
    return kDefaultCacheMs;
}

bool isTruthyToken(const std::string& token)
{
    const std::string v = trimCopy(token);
    if (v == "1" || v == "on" || v == "ON" || v == "true" || v == "TRUE" || v == "yes" || v == "YES") {
        return true;
    }
    if (v == "0" || v == "off" || v == "OFF" || v == "false" || v == "FALSE" || v == "no" || v == "NO") {
        return false;
    }

    // Tolerate kv-style values in state files: beacon=on / full=true
    const size_t eq = v.find('=');
    if (eq != std::string::npos && eq + 1 < v.size()) {
        const std::string rhs = trimCopy(v.substr(eq + 1));
        return rhs == "1" || rhs == "on" || rhs == "ON" || rhs == "true" || rhs == "TRUE" || rhs == "yes" || rhs == "YES";
    }

    return false;
}

bool probeBeaconState()
{
    g_probeCount.fetch_add(1, std::memory_order_relaxed);

    if (const char* env = std::getenv("RAWRXD_BEACON_FULL")) {
        return isTruthyToken(std::string(env));
    }

    if (const char* path = std::getenv("RAWRXD_BEACON_STATE_FILE")) {
        g_fileProbeCount.fetch_add(1, std::memory_order_relaxed);
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (in.is_open()) {
            std::string value;
            std::getline(in, value);
            return isTruthyToken(value);
        }
    }

    return false;
}

bool parseForceOverride(bool* hasOverride)
{
    if (hasOverride) {
        *hasOverride = false;
    }
    const char* env = std::getenv("RAWRXD_BEACON_FORCE");
    if (!env || env[0] == '\0') {
        return false;
    }

    if (hasOverride) {
        *hasOverride = true;
    }
    return isTruthyToken(std::string(env));
}

uint64_t monotonicMs()
{
#if defined(_WIN32)
    return static_cast<uint64_t>(GetTickCount64());
#else
    return g_probeCount.load(std::memory_order_relaxed);
#endif
}
} // namespace

bool isBeaconFullActive() {
    uint64_t modeMask = 0;
    bool hasOverride = false;
    const bool overrideValue = parseForceOverride(&hasOverride);
    if (hasOverride) {
        g_forceOverrideCount.fetch_add(1, std::memory_order_relaxed);
        g_cachedFull.store(overrideValue, std::memory_order_relaxed);
        g_lastProbeMs.store(monotonicMs(), std::memory_order_relaxed);
        modeMask |= 0x1ULL;
        if (overrideValue) {
            modeMask |= 0x2ULL;
        }
        g_lastModeMask.store(modeMask, std::memory_order_relaxed);
        return overrideValue;
    }

    const uint64_t now = monotonicMs();
    const uint64_t last = g_lastProbeMs.load(std::memory_order_relaxed);
    const uint64_t cacheMs = parseCacheMs();
    const bool forceRefresh = (std::getenv("RAWRXD_BEACON_FORCE_REFRESH") != nullptr);

    if (forceRefresh || last == 0 || now < last || (now - last) > cacheMs) {
        g_cachedFull.store(probeBeaconState(), std::memory_order_relaxed);
        g_lastProbeMs.store(now, std::memory_order_relaxed);
        g_cacheRefreshCount.fetch_add(1, std::memory_order_relaxed);
        modeMask |= 0x4ULL;
        if (forceRefresh) {
            modeMask |= 0x8ULL;
        }
    } else {
        g_cacheHitCount.fetch_add(1, std::memory_order_relaxed);
        modeMask |= 0x10ULL;
    }

    const bool value = g_cachedFull.load(std::memory_order_relaxed);
    if (value) {
        modeMask |= 0x20ULL;
    }
    g_lastModeMask.store(modeMask, std::memory_order_relaxed);
    return value;
}

extern "C" unsigned __int64 rawrxd_beacon_stub_stats()
{
    // [63:56] last_mode, [55:48] force_overrides, [47:40] file_probes,
    // [39:32] probes, [31:24] refreshes, [23:16] cache_hits, [15:8] cache_ms(low8), [7:0] cached_value.
    const uint64_t mode = g_lastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t overrides = g_forceOverrideCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t fileProbes = g_fileProbeCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t probes = g_probeCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t refreshes = g_cacheRefreshCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_cacheHitCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t cacheMs = parseCacheMs() & 0xFFu;
    const uint64_t cached = g_cachedFull.load(std::memory_order_relaxed) ? 1u : 0u;
    return (mode << 56) | (overrides << 48) | (fileProbes << 40) | (probes << 32) |
           (refreshes << 24) | (hits << 16) | (cacheMs << 8) | cached;
}

extern "C" unsigned __int64 rawrxd_beacon_stub_extended_stats()
{
    // [63:56] invalid_cache_ms, [55:48] force_overrides, [47:40] file_probes,
    // [39:32] refreshes, [31:24] cache_hits, [23:16] probes, [15:8] cache_ms(low8), [7:0] mode.
    const uint64_t invalidCache = g_invalidCacheMsCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t overrides = g_forceOverrideCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t fileProbes = g_fileProbeCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t refreshes = g_cacheRefreshCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hits = g_cacheHitCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t probes = g_probeCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t cacheMs = g_lastResolvedCacheMs.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_lastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    return (invalidCache << 56) | (overrides << 48) | (fileProbes << 40) | (refreshes << 32) |
           (hits << 24) | (probes << 16) | (cacheMs << 8) | mode;
}

extern "C" void rawrxd_beacon_stub_reset_stats()
{
    g_cachedFull.store(false, std::memory_order_relaxed);
    g_lastProbeMs.store(0, std::memory_order_relaxed);
    g_probeCount.store(0, std::memory_order_relaxed);
    g_fileProbeCount.store(0, std::memory_order_relaxed);
    g_cacheHitCount.store(0, std::memory_order_relaxed);
    g_cacheRefreshCount.store(0, std::memory_order_relaxed);
    g_forceOverrideCount.store(0, std::memory_order_relaxed);
    g_invalidCacheMsCount.store(0, std::memory_order_relaxed);
    g_lastResolvedCacheMs.store(0, std::memory_order_relaxed);
    g_lastModeMask.store(0, std::memory_order_relaxed);
}
