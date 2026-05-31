// Satisfies enterprise_feature_manager / license.h when RawrXD_Gold does not link
// MASM enterprise objects that define Enterprise_DevUnlock.
#include <cstdint>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::atomic<std::int64_t> g_cachedResult{0};
std::atomic<uint64_t> g_lastProbeTick{0};
std::atomic<uint64_t> g_totalCalls{0};
std::atomic<uint64_t> g_probeCalls{0};
std::atomic<uint64_t> g_cacheHitCalls{0};
std::atomic<uint64_t> g_unlockDecisions{0};
std::atomic<uint64_t> g_lockDecisions{0};
std::atomic<uint64_t> g_fileTokenReads{0};
std::atomic<uint64_t> g_invalidTokenCount{0};
std::atomic<uint64_t> g_lastModeMask{0};

constexpr uint64_t kDefaultProbeMs = 1000;

std::string trimCopy(const std::string& in)
{
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start])) != 0) {
        ++start;
    }

    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1])) != 0) {
        --end;
    }
    return in.substr(start, end - start);
}

std::string toUpperCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

uint64_t probeIntervalMs()
{
    if (const char* env = std::getenv("RAWRXD_ENTERPRISE_PROBE_MS")) {
        try {
            const uint64_t parsed = static_cast<uint64_t>(std::stoull(env));
            return (parsed > 10000) ? 10000 : parsed;
        } catch (...) {
            return kDefaultProbeMs;
        }
    }
    return kDefaultProbeMs;
}

uint64_t monotonicMs()
{
#if defined(_WIN32)
    return static_cast<uint64_t>(GetTickCount64());
#else
    return 0;
#endif
}

uint64_t machineFingerprint()
{
#if defined(_WIN32)
    uint64_t acc = static_cast<uint64_t>(GetCurrentProcessId()) ^ 0x9E3779B97F4A7C15ULL;
    const char* keys[] = {"COMPUTERNAME", "PROCESSOR_IDENTIFIER", "USERNAME"};
    for (const char* key : keys) {
        if (const char* value = std::getenv(key)) {
            for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
                acc ^= static_cast<uint64_t>(*p);
                acc *= 0xff51afd7ed558ccdULL;
                acc ^= (acc >> 33);
            }
        }
    }
    return acc;
#else
    return 0;
#endif
}

bool isTruthy(const char* v)
{
    if (!v) return false;
    const std::string t = toUpperCopy(trimCopy(v));
    return (t == "1" || t == "TRUE" || t == "ON" || t == "YES");
}

bool isFalsy(const char* v)
{
    if (!v) return false;
    const std::string t = toUpperCopy(trimCopy(v));
    return (t == "0" || t == "FALSE" || t == "OFF" || t == "NO");
}

bool tokenMatchesFingerprint(const std::string& token)
{
    const uint64_t expected = machineFingerprint() & 0xFFFFu;
    char expectedHex[17] = {0};
    std::snprintf(expectedHex, sizeof(expectedHex), "%llX", static_cast<unsigned long long>(expected));

    const std::string normalized = toUpperCopy(trimCopy(token));
    if (normalized == expectedHex) {
        return true;
    }

    // Also support FP:<hex> for explicit tagged tokens.
    if (normalized.rfind("FP:", 0) == 0) {
        return normalized.substr(3) == expectedHex;
    }
    return false;
}

bool tokenFromFileMatches()
{
    if (const char* path = std::getenv("RAWRXD_ENTERPRISE_UNLOCK_FILE")) {
        g_fileTokenReads.fetch_add(1, std::memory_order_relaxed);
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in.is_open()) {
            return false;
        }

        std::string line;
        std::getline(in, line);
        if (line.empty()) {
            return false;
        }

        if (isTruthy(line.c_str())) {
            return true;
        }
        return tokenMatchesFingerprint(line);
    }
    return false;
}
} // namespace

extern "C" std::int64_t Enterprise_DevUnlock()
{
    g_totalCalls.fetch_add(1, std::memory_order_relaxed);
    uint64_t modeMask = 0;

    // Explicit force-lock wins over all other knobs.
    if (isTruthy(std::getenv("RAWRXD_ENTERPRISE_FORCE_LOCK")) || isFalsy(std::getenv("RAWRXD_ENTERPRISE_DEV"))) {
        modeMask |= 0x1ULL;
        g_lockDecisions.fetch_add(1, std::memory_order_relaxed);
        g_cachedResult.store(-1, std::memory_order_relaxed);
        g_lastModeMask.store(modeMask, std::memory_order_relaxed);
        return 0;
    }

    const uint64_t now = monotonicMs();
    const uint64_t last = g_lastProbeTick.load(std::memory_order_relaxed);
    const std::int64_t cached = g_cachedResult.load(std::memory_order_relaxed);

    if (!isTruthy(std::getenv("RAWRXD_ENTERPRISE_DEV_REFRESH")) &&
        cached != 0 && last != 0 && now >= last && (now - last) < probeIntervalMs()) {
        modeMask |= 0x2ULL;
        g_cacheHitCalls.fetch_add(1, std::memory_order_relaxed);
        if (cached > 0) {
            g_unlockDecisions.fetch_add(1, std::memory_order_relaxed);
            modeMask |= 0x20ULL;
        } else {
            g_lockDecisions.fetch_add(1, std::memory_order_relaxed);
        }
        g_lastModeMask.store(modeMask, std::memory_order_relaxed);
        return (cached > 0) ? 1 : 0;
    }

    g_probeCalls.fetch_add(1, std::memory_order_relaxed);
    modeMask |= 0x4ULL;

    std::int64_t result = -1;

    if (isTruthy(std::getenv("RAWRXD_ENTERPRISE_FORCE_UNLOCK")) || isTruthy(std::getenv("RAWRXD_ENTERPRISE_DEV"))) {
        modeMask |= 0x8ULL;
        result = 1;
    } else if (const char* token = std::getenv("RAWRXD_ENTERPRISE_UNLOCK_TOKEN")) {
        modeMask |= 0x10ULL;
        result = tokenMatchesFingerprint(token) ? 1 : -1;
        if (result <= 0) {
            g_invalidTokenCount.fetch_add(1, std::memory_order_relaxed);
        }
    } else if (tokenFromFileMatches()) {
        modeMask |= 0x40ULL;
        result = 1;
    }

    g_lastProbeTick.store(now, std::memory_order_relaxed);
    if (result > 0) {
        g_unlockDecisions.fetch_add(1, std::memory_order_relaxed);
        modeMask |= 0x20ULL;
        g_cachedResult.store(1, std::memory_order_relaxed);
        g_lastModeMask.store(modeMask, std::memory_order_relaxed);
        return 1;
    }
    g_lockDecisions.fetch_add(1, std::memory_order_relaxed);
    g_cachedResult.store(-1, std::memory_order_relaxed);
    g_lastModeMask.store(modeMask, std::memory_order_relaxed);
    return 0;
}

extern "C" unsigned __int64 rawrxd_enterprise_stub_stats()
{
    // [63:56] invalid_token, [55:48] file_reads, [47:40] lock_decisions,
    // [39:32] unlock_decisions, [31:24] cache_hits, [23:16] probes,
    // [15:8] total_calls, [7:0] last_mode.
    const uint64_t invalid = g_invalidTokenCount.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t fileReads = g_fileTokenReads.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t locks = g_lockDecisions.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t unlocks = g_unlockDecisions.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t cacheHits = g_cacheHitCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t probes = g_probeCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t calls = g_totalCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_lastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    return (invalid << 56) | (fileReads << 48) | (locks << 40) | (unlocks << 32) |
           (cacheHits << 24) | (probes << 16) | (calls << 8) | mode;
}

extern "C" unsigned __int64 rawrxd_enterprise_stub_cached_state()
{
    // low byte: cached result state (0 unknown, 1 unlock, 2 lock), high bits: last probe tick low 56.
    const std::int64_t cached = g_cachedResult.load(std::memory_order_relaxed);
    uint64_t state = 0;
    if (cached > 0) {
        state = 1;
    } else if (cached < 0) {
        state = 2;
    }
    const uint64_t tick = g_lastProbeTick.load(std::memory_order_relaxed) & 0x00FFFFFFFFFFFFFFULL;
    return (tick << 8) | state;
}
