// ============================================================================
// Win32IDE_Phase17_AgenticProfiler.h
// Phase 17 — Agentic Profiler API declarations
// ============================================================================
// Provides fine-grained per-tool cycle profiling layered on top of the
// Phase 16 state machine.  Functions attempt to call Titan bridge exports
// when present and gracefully fall back to local cycle sampling otherwise.
//
// Until Phase 17 is fully wired, all calls return 0/no-op — stub contracts
// are guaranteed by the ASM implementation.
//
// Legacy mapping references are intentionally omitted here to avoid drifting
// from Titan export ownership; runtime symbol resolution remains best-effort.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/// Returns the current agentic loop cycle counter (monotonic, QWORD).
/// Counter is scoped to the active Phase 16 controller session.
uint64_t RawrXD_Agentic_GetCycleCounter(void);

/// Reads/updates profile slot [0..15] with the current cycle counter.
/// Returns the previous value stored in that slot (for delta computation).
uint64_t RawrXD_Agentic_SampleProfileToken(uint32_t slot);

/// Drains the internal circular profile buffer into caller-supplied buf.
/// Returns the number of QWORD entries written (0 while stub-active).
uint32_t RawrXD_Agentic_FlushProfileBuffer(uint64_t* buf, uint32_t maxEntries);

/// Resets the cycle counter and zeroes all 16 profile slots.
void     RawrXD_Agentic_ResetCycleCounter(void);

// ============================================================================
// Phase 17 global free-function API (implemented in Win32IDE_Phase17_AgenticProfiler.cpp)
// ============================================================================

/// Call at the start of each agentic loop epoch to reset profiler state.
void     AgenticProfilerBeginEpoch(void);

/// Returns elapsed cycles since last BeginEpoch on this thread.
uint64_t AgenticProfilerGetElapsed(void);

/// Record a tool start timestamp in the profiler.
void     AgenticProfilerNotifyToolStart(const char* toolName);

/// Record a tool end timestamp; writes elapsed cycles to *outCycles if non-null.
void     AgenticProfilerNotifyToolEnd(const char* toolName, uint64_t* outCycles);

#ifdef __cplusplus
}  // extern "C"
#endif

// ============================================================================
// Phase 17 helper — thin C++ wrapper that integrates with Phase 16 notifiers
// ============================================================================
#ifdef __cplusplus

/// RAII guard that samples a profile token before/after a tool dispatch.
/// Slot is derived from a stable djb2 hash of the tool name (mod 16).
struct Phase17ProfileGuard
{
    uint32_t m_slot;
    uint64_t m_oldValue;

    explicit Phase17ProfileGuard(const std::string& toolName)
    {
        // Stable slot assignment: djb2 hash of tool name mod 16
        uint32_t h = 5381u;
        for (unsigned char c : toolName)
        {
            h = ((h << 5u) + h) ^ c;
        }
        m_slot     = h & 0x0Fu;  // mod 16
        m_oldValue = RawrXD_Agentic_SampleProfileToken(m_slot);
    }

    ~Phase17ProfileGuard()
    {
        // On destruction: sample again to record end-of-tool timestamp.
        RawrXD_Agentic_SampleProfileToken(m_slot);
    }

    // Non-copyable
    Phase17ProfileGuard(const Phase17ProfileGuard&)            = delete;
    Phase17ProfileGuard& operator=(const Phase17ProfileGuard&) = delete;
};

// ============================================================================
// Phase17Profiler — static C++ wrapper around the Phase 17 DLL exports
// ============================================================================
class Phase17Profiler
{
public:
    /// Reset RDTSC epoch and clear accumulated cycle map.
    static void     BeginAgenticEpoch();

    /// Sample the slot for a named tool; returns old slot value.
    static uint64_t SampleToolSlot(const std::string& toolName);

    /// Accumulate cycles for a named tool (called from NotifyToolEnd).
    static void     RecordToolCycles(const std::string& toolName, uint64_t cycles);

    /// Flush all 16 slots into buf; returns entry count.
    static uint32_t FlushBuffer(uint64_t* buf, uint32_t maxEntries);

    /// Returns elapsed cycles since last BeginAgenticEpoch on this thread.
    static uint64_t GetElapsedCycles();

    /// Returns total accumulated cycles for a named tool (sum across calls).
    static uint64_t GetAccumulatedCycles(const std::string& toolName);

    /// Returns the number of epochs begun.
    static uint32_t GetEpochCount();

    /// Returns a compact summary of top tools by accumulated cycles.
    /// Example: "epochs=2 | compile=12345c | dumpbin=9876c"
    static std::string GetTopToolsSummary(size_t maxTools = 3);

    /// Stable djb2 slot for a tool name (mod 16).
    static uint32_t ComputeSlot(const std::string& toolName);
};

/// Returns a compact profiler summary for telemetry/UI surfaces.
std::string AgenticProfilerTopSummary(uint32_t maxTools = 3);

#endif  // __cplusplus
