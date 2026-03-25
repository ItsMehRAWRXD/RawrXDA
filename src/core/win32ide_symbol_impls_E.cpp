// win32ide_symbol_impls_E.cpp — RawrXD IDE debug agentic symbol implementations

#include <windows.h>
#include <cstdint>
#include <cstring>

#define PERF_MAX_SLOTS 64

struct PerfSlot {
    uint64_t total_cycles;
    uint64_t hit_count;
    uint64_t min_cycles;
    uint64_t max_cycles;
    uint64_t last_start;
    uint64_t reserved[3];
};

static PerfSlot      g_perf_table[PERF_MAX_SLOTS] = {};
static bool          g_perf_initialized = false;
static LARGE_INTEGER g_perf_freq = {};

extern "C" {

int asm_perf_init()
{
    if (!QueryPerformanceFrequency(&g_perf_freq))
        return 0;
    ZeroMemory(g_perf_table, sizeof(g_perf_table));
    g_perf_initialized = true;
    return 1;
}

uint64_t asm_perf_begin(uint32_t slotIndex)
{
    if (slotIndex >= PERF_MAX_SLOTS)
        return 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    g_perf_table[slotIndex].last_start = static_cast<uint64_t>(now.QuadPart);
    return static_cast<uint64_t>(now.QuadPart);
}

uint64_t asm_perf_end(uint32_t slotIndex, uint64_t startTSC)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t end   = static_cast<uint64_t>(now.QuadPart);
    uint64_t delta = (end >= startTSC) ? (end - startTSC) : 0;

    if (slotIndex < PERF_MAX_SLOTS)
    {
        PerfSlot& slot = g_perf_table[slotIndex];
        slot.total_cycles += delta;
        slot.hit_count    += 1;
        if (slot.hit_count == 1 || delta < slot.min_cycles)
            slot.min_cycles = delta;
        if (delta > slot.max_cycles)
            slot.max_cycles = delta;
    }

    return delta;
}

int asm_perf_read_slot(uint32_t slotIndex, void* buffer128)
{
    if (slotIndex >= PERF_MAX_SLOTS || buffer128 == nullptr)
        return -1;
    memcpy(buffer128, &g_perf_table[slotIndex], sizeof(PerfSlot));
    return 0;
}

int asm_perf_reset_slot(uint32_t slotIndex)
{
    if (slotIndex >= PERF_MAX_SLOTS)
        return -1;
    ZeroMemory(&g_perf_table[slotIndex], sizeof(PerfSlot));
    return 0;
}

uint32_t asm_perf_get_slot_count()
{
    return PERF_MAX_SLOTS;
}

void* asm_perf_get_table_base()
{
    return static_cast<void*>(g_perf_table);
}

} // extern "C"
