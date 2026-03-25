// rawrxd_watchdog_bridge.cpp
// C++ linker fallbacks for RawrXD-Win32IDE.
//
// Group 1 — asm_watchdog (5 symbols)
//   Provides CRC-based code-integrity watchdog when RawrXD_Watchdog.asm/.obj is absent.
//
// Group 2 — Model router (3 symbols, const char* ABI)
//   Provides LoadModel / ModelLoaderInit / HotSwapModel for callers that use the
//   narrow-char (UTF-8 / ANSI) interface rather than the wchar_t interface in
//   universal_model_router.cpp / model_loader_fallbacks.cpp.

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <windows.h>

// ─────────────────────────────────────────────────────────────────────────────
// Group 1: asm_watchdog state
// ─────────────────────────────────────────────────────────────────────────────

static std::mutex            g_watchdogMutex;
static void*                 g_moduleBase  = nullptr;
static uint32_t              g_moduleSize  = 0;
static std::atomic<uint64_t> g_verifyCount{0};
static uint32_t              g_baselineCRC = 0;
static int                   g_lastVerifyResult = 0; // 1 = pass, 0 = fail/uninit

namespace
{
    // FNV-1a 32-bit — same algorithm used in win32ide_asm_kernel_bridge.cpp snapshots.
    uint32_t fnv1a32(const void* data, uint32_t size)
    {
        const uint8_t* p   = static_cast<const uint8_t*>(data);
        uint32_t       crc = 2166136261u;
        for (uint32_t i = 0; i < size; ++i)
        {
            crc ^= p[i];
            crc *= 16777619u;
        }
        return crc;
    }
}

// Initialize watchdog: store region, compute CRC baseline, return 1 on success.
extern "C" int asm_watchdog_init(void* moduleBase, uint32_t moduleSize)
{
    if (moduleBase == nullptr || moduleSize == 0)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    g_moduleBase       = moduleBase;
    g_moduleSize       = moduleSize;
    g_baselineCRC      = fnv1a32(moduleBase, moduleSize);
    g_lastVerifyResult = 1;
    g_verifyCount.store(0, std::memory_order_relaxed);
    return 1;
}

// Recompute CRC and compare to baseline. Returns 1 if intact, 0 if mismatch/uninit.
extern "C" int asm_watchdog_verify(void)
{
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    if (g_moduleBase == nullptr || g_moduleSize == 0)
    {
        return 0;
    }
    const uint32_t current = fnv1a32(g_moduleBase, g_moduleSize);
    g_verifyCount.fetch_add(1, std::memory_order_relaxed);
    g_lastVerifyResult = (current == g_baselineCRC) ? 1 : 0;
    return g_lastVerifyResult;
}

// Copy the 4-byte baseline CRC to baselineOut if non-null.
extern "C" void asm_watchdog_get_baseline(void* baselineOut)
{
    if (baselineOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    std::memcpy(baselineOut, &g_baselineCRC, sizeof(g_baselineCRC));
}

// Write status struct to statusOut if non-null.
// Layout: uint64_t[0] = verifyCount, uint64_t[1] = lastVerifyResult, uint64_t[2] = baselineCRC
extern "C" void asm_watchdog_get_status(void* statusOut)
{
    if (statusOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    uint64_t* out = static_cast<uint64_t*>(statusOut);
    out[0] = g_verifyCount.load(std::memory_order_relaxed);
    out[1] = static_cast<uint64_t>(g_lastVerifyResult);
    out[2] = static_cast<uint64_t>(g_baselineCRC);
}

// Reset all watchdog state.
extern "C" void asm_watchdog_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_watchdogMutex);
    g_moduleBase       = nullptr;
    g_moduleSize       = 0;
    g_baselineCRC      = 0;
    g_lastVerifyResult = 0;
    g_verifyCount.store(0, std::memory_order_relaxed);
}
// LoadModel / ModelLoaderInit / HotSwapModel are provided by model_loader_fallbacks.cpp
// when RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON. Omitted here to prevent LNK2005.
