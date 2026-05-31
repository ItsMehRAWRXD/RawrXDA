// Win32IDE sovereign GPU link stubs.
// Purpose: keep RawrXD-Win32IDE linkable when RAWR_HAS_SOVEREIGN_ENGINES=0.
// These symbols are referenced by Win32IDE_TabManager.cpp but implemented in optional GPU ASM.

#include <cstdint>
#include <atomic>

#include <windows.h>

#if !defined(RAWR_HAS_SOVEREIGN_ENGINES) || (RAWR_HAS_SOVEREIGN_ENGINES == 0)

extern "C"
{

    namespace {
    std::atomic<uint64_t> g_state{0};
    std::atomic<uint64_t> g_lastPuf{0};
    std::atomic<uint64_t> g_hugePageCounter{1};
    std::atomic<uint64_t> g_totalCalls{0};
    std::atomic<uint64_t> g_mmioReads{0};
    std::atomic<uint64_t> g_telemetryReads{0};
    std::atomic<uint64_t> g_allocCalls{0};
    std::atomic<uint64_t> g_authPass{0};
    std::atomic<uint64_t> g_authFail{0};

    uint64_t mix64(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
    }

    uint64_t seedState()
    {
        const uint64_t ticks = static_cast<uint64_t>(GetTickCount64());
        const uint64_t pid = static_cast<uint64_t>(GetCurrentProcessId());
        return mix64((ticks << 16) ^ pid ^ 0x9e3779b97f4a7c15ULL);
    }

    uint64_t nextState()
    {
        uint64_t expected = g_state.load(std::memory_order_relaxed);
        if (expected == 0) {
            const uint64_t seeded = seedState();
            g_state.compare_exchange_strong(expected, seeded, std::memory_order_relaxed);
            expected = g_state.load(std::memory_order_relaxed);
        }
        const uint64_t next = mix64(expected + 0x9e3779b97f4a7c15ULL);
        g_state.store(next, std::memory_order_relaxed);
        return next;
    }
    }  // namespace

    uint64_t KFD_Get_Driver_Version()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        const uint64_t rev = (nextState() & 0xFFFFULL);
        // Fallback reports a synthetic but valid non-zero version.
        return (1ULL << 48) | (2ULL << 32) | (3ULL << 16) | rev;
    }

    void KFD_Ring_Hardware_Doorbell()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        (void)nextState();
    }

    void RDNA3_Shadow_Pager_Init()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        (void)nextState();
    }

    void RDNA3_Power_Pulse()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        (void)nextState();
    }

    void RDNA3_Speculative_Preload()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        (void)nextState();
    }

    void Neural_Entropy_Generate()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        (void)nextState();
    }

    uint64_t RDNA3_MMIO_Read(uint64_t)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        g_mmioReads.fetch_add(1, std::memory_order_relaxed);
        return mix64(nextState() ^ 0x1A20ULL);
    }

    uint64_t RDNA3_Telemetry_Read()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        g_telemetryReads.fetch_add(1, std::memory_order_relaxed);
        const uint64_t entropy = nextState();
        const uint64_t pages = g_hugePageCounter.load(std::memory_order_relaxed);
        return mix64(entropy ^ (pages << 8));
    }

    uint64_t RDNA3_HugePage_Allocate()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        g_allocCalls.fetch_add(1, std::memory_order_relaxed);
        const uint64_t id = g_hugePageCounter.fetch_add(1, std::memory_order_relaxed);
        return 0x100000000ULL + (id << 21);  // synthetic 2MB-aligned token
    }

    uint64_t RDNA3_3X_Virtualize(uint64_t)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        return mix64(nextState() ^ 0x3ULL);
    }

    uint64_t RDNA3_Elastic_Scale(uint64_t)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        return mix64(nextState() ^ 0x5AULL);
    }

    void RDNA3_Sovereign_Deflate(uint8_t* begin, uint8_t* end)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        if (!begin || !end || begin >= end) {
            return;
        }
        const uint64_t delta = static_cast<uint64_t>(end - begin);
        g_state.store(mix64(nextState() ^ delta), std::memory_order_relaxed);
    }

    void RDNA3_3x_Expand(uint8_t* begin, uint8_t* end)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        if (!begin || !end || begin >= end) {
            return;
        }
        const uint64_t delta = static_cast<uint64_t>(end - begin);
        g_state.store(mix64(nextState() ^ (delta << 1)), std::memory_order_relaxed);
    }

    void RDNA3_Custom_Inflate(uint8_t* begin, uint8_t* end)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        if (!begin || !end || begin >= end) {
            return;
        }
        const uint64_t delta = static_cast<uint64_t>(end - begin);
        g_state.store(mix64(nextState() ^ (delta << 2)), std::memory_order_relaxed);
    }

    uint64_t Silicon_PUF_Generate()
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        const uint64_t puf = mix64(nextState() ^ 0xA5A5A5A5A5A5A5A5ULL);
        g_lastPuf.store(puf, std::memory_order_relaxed);
        return puf;
    }

    bool RDNA3_Silicon_Authenticate(uint64_t signature)
    {
        g_totalCalls.fetch_add(1, std::memory_order_relaxed);
        const uint64_t expected = g_lastPuf.load(std::memory_order_relaxed);
        if (expected == 0) {
            g_authFail.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        const bool ok = (signature == expected);
        if (ok) {
            g_authPass.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_authFail.fetch_add(1, std::memory_order_relaxed);
        }
        return ok;
    }

    uint64_t RDNA3_Fallback_GetStats()
    {
        // [63:48] auth_fail, [47:32] auth_pass, [31:24] alloc, [23:16] telemetry,
        // [15:8] mmio, [7:0] total_calls (low byte)
        const uint64_t total = g_totalCalls.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t mmio = g_mmioReads.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t telemetry = g_telemetryReads.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t alloc = g_allocCalls.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t pass = g_authPass.load(std::memory_order_relaxed) & 0xFFFFu;
        const uint64_t fail = g_authFail.load(std::memory_order_relaxed) & 0xFFFFu;
        return total | (mmio << 8) | (telemetry << 16) | (alloc << 24) | (pass << 32) | (fail << 48);
    }

    uint64_t RDNA3_Fallback_GetStatsEx()
    {
        // [63:56] hugepage_counter(low8), [55:48] state(low8), [47:32] last_puf(low16),
        // [31:24] alloc, [23:16] telemetry, [15:8] mmio, [7:0] total_calls
        const uint64_t pages = g_hugePageCounter.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t state = g_state.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t puf = g_lastPuf.load(std::memory_order_relaxed) & 0xFFFFu;
        const uint64_t total = g_totalCalls.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t mmio = g_mmioReads.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t telemetry = g_telemetryReads.load(std::memory_order_relaxed) & 0xFFu;
        const uint64_t alloc = g_allocCalls.load(std::memory_order_relaxed) & 0xFFu;
        return (pages << 56) | (state << 48) | (puf << 32) | (alloc << 24) | (telemetry << 16) |
               (mmio << 8) | total;
    }

    void RDNA3_Fallback_ResetStats()
    {
        g_state.store(0, std::memory_order_relaxed);
        g_lastPuf.store(0, std::memory_order_relaxed);
        g_hugePageCounter.store(1, std::memory_order_relaxed);
        g_totalCalls.store(0, std::memory_order_relaxed);
        g_mmioReads.store(0, std::memory_order_relaxed);
        g_telemetryReads.store(0, std::memory_order_relaxed);
        g_allocCalls.store(0, std::memory_order_relaxed);
        g_authPass.store(0, std::memory_order_relaxed);
        g_authFail.store(0, std::memory_order_relaxed);
    }

}  // extern "C"

#endif
