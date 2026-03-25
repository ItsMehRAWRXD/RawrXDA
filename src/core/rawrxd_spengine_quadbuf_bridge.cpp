// rawrxd_spengine_quadbuf_bridge.cpp
// Signature Patching Engine (asm_spengine) and Quad-Buffered Rendering (asm_quadbuf)
// 14 extern "C" symbols for RawrXD-Win32IDE linker

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace {

// --- SPEngine state ---
std::mutex g_spMtx;
uint32_t g_spMaxPatches = 0;
uint32_t g_spCaveSizeBytes = 0;
std::atomic<uint32_t> g_spPatchCount{0};
std::atomic<uint64_t> g_spApplyCount{0};
std::unordered_map<uint32_t, std::vector<unsigned char>> g_spPatchMap;
std::unordered_map<uint32_t, std::vector<unsigned char>> g_spOriginalMap;
bool g_spInitialized = false;

static uint32_t g_spQuantMode = 0;   // current quant mode
static uint32_t g_spCoreCount = 0;
static uint32_t g_spCacheHintKB = 0;

// --- QuadBuf state ---
std::mutex g_qbMtx;
void* g_qbHwnd = nullptr;
uint32_t g_qbWidth = 0;
uint32_t g_qbHeight = 0;
uint32_t g_qbBufferCount = 0;
std::atomic<uint64_t> g_qbFrameCount{0};
std::atomic<uint64_t> g_qbTokenCount{0};
uint32_t g_qbFlags = 0;
bool g_qbInitialized = false;

} // namespace

// =============================================================================
// SPEngine — Signature Patching Engine
// =============================================================================

extern "C" {

int asm_spengine_init(uint32_t maxPatches, uint32_t caveSizeBytes)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    g_spMaxPatches     = maxPatches;
    g_spCaveSizeBytes  = caveSizeBytes;
    g_spPatchCount.store(0u);
    g_spApplyCount.store(0ull);
    g_spPatchMap.clear();
    g_spOriginalMap.clear();
    g_spQuantMode      = 0;
    g_spInitialized    = true;
    return 1;
}

int asm_spengine_register(const void* originalAddr, const void* patchBytes, uint32_t patchLen)
{
    if (patchBytes == nullptr || patchLen == 0)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_spMtx);
    const uint32_t patchId = g_spPatchCount.fetch_add(1u);

    const unsigned char* pb = static_cast<const unsigned char*>(patchBytes);
    g_spPatchMap[patchId].assign(pb, pb + patchLen);

    // Snapshot original bytes at originalAddr for rollback
    if (originalAddr != nullptr)
    {
        const unsigned char* ob = static_cast<const unsigned char*>(originalAddr);
        g_spOriginalMap[patchId].assign(ob, ob + patchLen);
    }

    return static_cast<int>(patchId);
}

int asm_spengine_apply(uint32_t patchId)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    auto it = g_spPatchMap.find(patchId);
    if (it == g_spPatchMap.end())
    {
        return -1;
    }
    auto oit = g_spOriginalMap.find(patchId);
    if (oit == g_spOriginalMap.end())
    {
        return -1;
    }

    void* target = const_cast<void*>(static_cast<const void*>(oit->second.data()));
    SIZE_T sz    = static_cast<SIZE_T>(it->second.size());

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sz, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return -1;
    }

    SIZE_T written = 0;
    WriteProcessMemory(GetCurrentProcess(), target, it->second.data(), sz, &written);

    DWORD dummy = 0;
    VirtualProtect(target, sz, oldProtect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target, sz);

    g_spApplyCount.fetch_add(1ull);
    return 1;
}

int asm_spengine_rollback(uint32_t patchId)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    auto oit = g_spOriginalMap.find(patchId);
    if (oit == g_spOriginalMap.end())
    {
        return -1;
    }
    auto pit = g_spPatchMap.find(patchId);
    if (pit == g_spPatchMap.end())
    {
        return -1;
    }

    // The original bytes were captured at register time; restore them
    void* target = const_cast<void*>(static_cast<const void*>(oit->second.data()));
    SIZE_T sz    = static_cast<SIZE_T>(oit->second.size());

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sz, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return -1;
    }

    SIZE_T written = 0;
    WriteProcessMemory(GetCurrentProcess(), target, oit->second.data(), sz, &written);

    DWORD dummy = 0;
    VirtualProtect(target, sz, oldProtect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target, sz);

    return 1;
}

int asm_spengine_quant_switch(uint32_t fromQuant, uint32_t toQuant)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    (void)fromQuant;
    g_spQuantMode = toQuant;
    return 1;
}

int asm_spengine_quant_switch_adaptive(uint32_t memAvailMB, uint32_t targetMs)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    (void)targetMs;
    // Q4_K=4, Q4_0=3, Q2_K=2 (internal encoding)
    if (memAvailMB > 8192u)
    {
        g_spQuantMode = 4u; // Q4_K
    }
    else if (memAvailMB > 4096u)
    {
        g_spQuantMode = 3u; // Q4_0
    }
    else
    {
        g_spQuantMode = 2u; // Q2_K
    }
    return 1;
}

void asm_spengine_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_spMtx);
    uint64_t* out = static_cast<uint64_t*>(statsOut);
    out[0] = static_cast<uint64_t>(g_spMaxPatches);
    out[1] = static_cast<uint64_t>(g_spPatchCount.load());
    out[2] = g_spApplyCount.load();
}

void asm_spengine_cpu_optimize(uint32_t coreCount, uint32_t cacheHintKB)
{
    std::lock_guard<std::mutex> lock(g_spMtx);
    g_spCoreCount    = coreCount;
    g_spCacheHintKB  = cacheHintKB;
    // No hardware action; hints stored for future use.
}

// =============================================================================
// QuadBuf — Quad-Buffered Rendering
// =============================================================================

int asm_quadbuf_init(void* hwnd, uint32_t width, uint32_t height, uint32_t bufferCount)
{
    std::lock_guard<std::mutex> lock(g_qbMtx);
    g_qbHwnd        = hwnd;
    g_qbWidth       = width;
    g_qbHeight      = height;
    g_qbBufferCount = bufferCount;
    g_qbFrameCount.store(0ull);
    g_qbTokenCount.store(0ull);
    g_qbFlags       = 0u;
    g_qbInitialized = true;
    return 1;
}

int asm_quadbuf_push_token(const void* tokenData, uint32_t tokenLen)
{
    if (tokenData == nullptr || tokenLen == 0)
    {
        return -1;
    }
    g_qbTokenCount.fetch_add(1ull);
    return 1;
}

int asm_quadbuf_render_frame(uint32_t frameFlags)
{
    (void)frameFlags;
    g_qbFrameCount.fetch_add(1ull);
    return 1;
}

int asm_quadbuf_resize(uint32_t newWidth, uint32_t newHeight)
{
    std::lock_guard<std::mutex> lock(g_qbMtx);
    g_qbWidth  = newWidth;
    g_qbHeight = newHeight;
    return 1;
}

void asm_quadbuf_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_qbMtx);
    uint64_t* out = static_cast<uint64_t*>(statsOut);
    out[0] = g_qbFrameCount.load();
    out[1] = g_qbTokenCount.load();
    out[2] = static_cast<uint64_t>(g_qbWidth);
    out[3] = static_cast<uint64_t>(g_qbHeight);
}

int asm_quadbuf_set_flags(uint32_t flags)
{
    std::lock_guard<std::mutex> lock(g_qbMtx);
    g_qbFlags = flags;
    return 1;
}

} // extern "C"
