// ============================================================================
// Win32IDE_Phase17_AgenticProfiler.cpp
// Phase 17 — Agentic Profiler (per-tool RDTSC cycle accounting)
// ============================================================================
// Implements the Phase 17 profiler API for the Win32IDE process.
// Wraps ordinals 92-95 via the RawrXD_Phase17_* bridge exports from Titan.dll.
// Integrates with Phase 16: each AgenticNotifyToolStart/End pair is bracketed
// by a Phase17ProfileGuard to record cycle stamps per tool name.
// ============================================================================

#include "Win32IDE_Phase17_AgenticProfiler.h"
#include "Win32IDE_Phase16_AgenticController.h"

#include <windows.h>
#include <intrin.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cstdio>

// ============================================================================
// Titan.dll Phase 17 function pointers (loaded lazily at first use)
// ============================================================================
namespace {

using PFN_BeginEpoch         = void     (__stdcall*)(void);
using PFN_SampleSlot         = uint64_t (__stdcall*)(uint32_t slot, LPCSTR label);
using PFN_FlushBuffer        = uint32_t (__stdcall*)(uint64_t* buf, uint32_t max);
using PFN_GetElapsedCycles   = uint64_t (__stdcall*)(void);
using PFN_GetLastEvent       = const char* (__stdcall*)(void);

struct Phase17Procs {
    PFN_BeginEpoch       BeginEpoch       = nullptr;
    PFN_SampleSlot       SampleSlot       = nullptr;
    PFN_FlushBuffer      FlushBuffer      = nullptr;
    PFN_GetElapsedCycles GetElapsedCycles = nullptr;
    PFN_GetLastEvent     GetLastEvent     = nullptr;
    bool                 loaded           = false;
};

static Phase17Procs g_procs;
static std::once_flag g_loadFlag;

static void LoadPhase17Procs() {
    HMODULE hTitan = GetModuleHandleW(L"RawrXD_Titan.dll");
    if (!hTitan) hTitan = GetModuleHandleW(L"Titan.dll");
    if (!hTitan) return;

    g_procs.BeginEpoch       = reinterpret_cast<PFN_BeginEpoch>(
                                    GetProcAddress(hTitan, "RawrXD_Phase17_BeginEpoch"));
    g_procs.SampleSlot       = reinterpret_cast<PFN_SampleSlot>(
                                    GetProcAddress(hTitan, "RawrXD_Phase17_SampleSlot"));
    g_procs.FlushBuffer      = reinterpret_cast<PFN_FlushBuffer>(
                                    GetProcAddress(hTitan, "RawrXD_Phase17_FlushBuffer"));
    g_procs.GetElapsedCycles = reinterpret_cast<PFN_GetElapsedCycles>(
                                    GetProcAddress(hTitan, "RawrXD_Phase17_GetElapsedCycles"));
    g_procs.GetLastEvent     = reinterpret_cast<PFN_GetLastEvent>(
                                    GetProcAddress(hTitan, "RawrXD_Phase17_GetLastProfilerEvent"));

    g_procs.loaded = (g_procs.BeginEpoch     != nullptr &&
                      g_procs.SampleSlot     != nullptr &&
                      g_procs.FlushBuffer    != nullptr &&
                      g_procs.GetElapsedCycles != nullptr);
}

static Phase17Procs& GetProcs() {
    std::call_once(g_loadFlag, LoadPhase17Procs);
    return g_procs;
}

// ============================================================================
// Per-tool cycle accumulation (tool name -> total cycles)
// ============================================================================
static std::mutex                             g_accumMutex;
static std::unordered_map<std::string, uint64_t> g_toolCycleAccum;
static std::unordered_map<std::string, uint64_t> g_toolStartStamp;
static uint32_t g_epochCount = 0;
static uint64_t g_localEpochStart = 0;

static uint64_t ReadLocalCycleCounter()
{
#if defined(_M_X64) || defined(_M_IX86)
    return __rdtsc();
#else
    LARGE_INTEGER now = {};
    LARGE_INTEGER freq = {};
    if (QueryPerformanceCounter(&now) && QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
        return static_cast<uint64_t>(now.QuadPart);
    }
    return 0;
#endif
}

}  // namespace

// ============================================================================
// Phase17Profiler — public API
// ============================================================================

void Phase17Profiler::BeginAgenticEpoch()
{
    auto& p = GetProcs();
    if (p.loaded) {
        p.BeginEpoch();
    }
    {
        std::lock_guard<std::mutex> lock(g_accumMutex);
        ++g_epochCount;
        g_toolCycleAccum.clear();
        g_toolStartStamp.clear();
        g_localEpochStart = ReadLocalCycleCounter();
    }
}

uint64_t Phase17Profiler::SampleToolSlot(const std::string& toolName)
{
    auto& p = GetProcs();
    if (!p.loaded) return 0;

    const uint32_t slot = ComputeSlot(toolName);
    return p.SampleSlot(slot, toolName.c_str());
}

void Phase17Profiler::RecordToolCycles(const std::string& toolName, uint64_t cycles)
{
    std::lock_guard<std::mutex> lock(g_accumMutex);
    g_toolCycleAccum[toolName] += cycles;
}

uint32_t Phase17Profiler::FlushBuffer(uint64_t* buf, uint32_t maxEntries)
{
    auto& p = GetProcs();
    if (!p.loaded) return 0;
    return p.FlushBuffer(buf, maxEntries);
}

uint64_t Phase17Profiler::GetElapsedCycles()
{
    auto& p = GetProcs();
    if (p.loaded) {
        return p.GetElapsedCycles();
    }

    const uint64_t now = ReadLocalCycleCounter();
    std::lock_guard<std::mutex> lock(g_accumMutex);
    if (g_localEpochStart == 0 || now < g_localEpochStart) {
        return 0;
    }
    return now - g_localEpochStart;
}

uint32_t Phase17Profiler::ComputeSlot(const std::string& toolName)
{
    uint32_t h = 5381u;
    for (unsigned char c : toolName) {
        h = ((h << 5u) + h) ^ c;
    }
    return h & 0x0Fu;  // mod 16
}

uint64_t Phase17Profiler::GetAccumulatedCycles(const std::string& toolName)
{
    std::lock_guard<std::mutex> lock(g_accumMutex);
    auto it = g_toolCycleAccum.find(toolName);
    return (it != g_toolCycleAccum.end()) ? it->second : 0ULL;
}

uint32_t Phase17Profiler::GetEpochCount()
{
    std::lock_guard<std::mutex> lock(g_accumMutex);
    return g_epochCount;
}

// ============================================================================
// Win32IDE-local C ABI shims matching the raw Phase 17 ordinal contract
// ============================================================================

extern "C" uint64_t RawrXD_Agentic_GetCycleCounter(void)
{
    return Phase17Profiler::GetElapsedCycles();
}

extern "C" uint64_t RawrXD_Agentic_SampleProfileToken(uint32_t slot)
{
    auto& p = GetProcs();
    if (!p.loaded) {
        return 0;
    }

    return p.SampleSlot(slot & 0x0Fu, nullptr);
}

extern "C" uint32_t RawrXD_Agentic_FlushProfileBuffer(uint64_t* buf, uint32_t maxEntries)
{
    return Phase17Profiler::FlushBuffer(buf, maxEntries);
}

extern "C" void RawrXD_Agentic_ResetCycleCounter(void)
{
    Phase17Profiler::BeginAgenticEpoch();
}

std::string Phase17Profiler::GetTopToolsSummary(size_t maxTools)
{
    std::vector<std::pair<std::string, uint64_t>> snapshot;
    uint32_t epochCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_accumMutex);
        snapshot.reserve(g_toolCycleAccum.size());
        for (const auto& kv : g_toolCycleAccum)
        {
            snapshot.push_back(kv);
        }
        epochCount = g_epochCount;
    }

    std::sort(snapshot.begin(), snapshot.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return a.first < b.first;
              });

    if (maxTools == 0)
    {
        maxTools = 1;
    }
    if (snapshot.size() > maxTools)
    {
        snapshot.resize(maxTools);
    }

    std::ostringstream oss;
    oss << "epochs=" << epochCount;
    if (snapshot.empty())
    {
        oss << " | no-tools";
        return oss.str();
    }

    for (const auto& [tool, cycles] : snapshot)
    {
        oss << " | " << tool << "=" << cycles << "c";
    }
    return oss.str();
}

// ============================================================================
// Global free-function API (matches Win32IDE_Phase17_AgenticProfiler.h)
// ============================================================================

void AgenticProfilerNotifyToolStart(const char* toolName)
{
    if (!toolName) return;

    uint64_t startStamp = Phase17Profiler::GetElapsedCycles();
    if (startStamp == 0) {
        // Fall back to raw local cycles when epoch elapsed is unavailable.
        startStamp = ReadLocalCycleCounter();
    }

    {
        std::lock_guard<std::mutex> lock(g_accumMutex);
        g_toolStartStamp[toolName] = startStamp;
    }

    // Keep Titan profile slot stream populated when available.
    Phase17Profiler::SampleToolSlot(toolName);
}

void AgenticProfilerNotifyToolEnd(const char* toolName, uint64_t* outCycles)
{
    if (!toolName) return;

    Phase17Profiler::SampleToolSlot(toolName);

    uint64_t endStamp = Phase17Profiler::GetElapsedCycles();
    if (endStamp == 0) {
        endStamp = ReadLocalCycleCounter();
    }

    uint64_t delta = endStamp;
    {
        std::lock_guard<std::mutex> lock(g_accumMutex);
        auto it = g_toolStartStamp.find(toolName);
        if (it != g_toolStartStamp.end() && endStamp >= it->second) {
            delta = endStamp - it->second;
            g_toolStartStamp.erase(it);
        }
    }

    Phase17Profiler::RecordToolCycles(toolName, delta);

    if (outCycles) {
        *outCycles = delta;
    }
}

void AgenticProfilerBeginEpoch()  { Phase17Profiler::BeginAgenticEpoch(); }
uint64_t AgenticProfilerGetElapsed()  { return Phase17Profiler::GetElapsedCycles(); }
std::string AgenticProfilerTopSummary(uint32_t maxTools)
{
    return Phase17Profiler::GetTopToolsSummary(maxTools);
}
