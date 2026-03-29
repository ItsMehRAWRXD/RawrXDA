// ============================================================================
// debug_ring.h — ZCCF Debug Snapshot Ring Buffer
// ============================================================================
// Maintains a circular buffer of DebuggerFramePayload snapshots captured
// during live debugging. The agent receives structured frame data (not text)
// and resolves symbol/module details on demand via ZCCF handles.
//
// Architecture:
//   ┌────────────────────────────────────────────────────┐
//   │                   DebugRing                         │
//   │                                                     │
//   │  [ Slot 0 ] [ Slot 1 ] ... [ Slot N-1 ]            │
//   │       ▲ head (oldest)          ▲ tail (newest)      │
//   │                                                     │
//   │  Push() overwrites head when full (overwrite policy)│
//   │  Watermark: minimum slot age before overwrite       │
//   └────────────────────────────────────────────────────┘
//
// Capture modes:
//   CaptureMode::Stopped     — thread is halted; full CONTEXT struct available
//   CaptureMode::SoftSample  — async sample via SuspendThread/ResumeThread;
//                              minimal interference, ~10µs window
//   CaptureMode::Breakpoint  — triggered at a debug event breakpoint
//
// Thread safety:
//   Push() — lock-free via atomic head/tail (single producer assumption).
//   Snapshot() — takes a copy under shared lock for consumer safety.
//
// No exceptions.
// ============================================================================

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <optional>
#include <atomic>
#include <expected>

namespace RawrXD {
namespace ZCCF {

// ============================================================================
// Capture mode
// ============================================================================

enum class CaptureMode : uint8_t {
    Unknown    = 0,
    Stopped    = 1,
    SoftSample = 2,
    Breakpoint = 3,
};

// ============================================================================
// DebuggerFramePayload — structured frame state for agent consumption
// ============================================================================
// Deliberately mirrors the struct described in the DAE+ZCCF blueprint so that
// the DebuggerFrameTracker (src/win32app/debugger_frame_tracker.h) can produce
// these payloads directly and hand them to the ring.

struct DebuggerFramePayload {
    // Core register state
    uint64_t rip  = 0;
    uint64_t rsp  = 0;
    uint64_t rbp  = 0;

    // Return address chain (from WalkRbpReturnAddresses baseline)
    uint64_t returnIps[8] = {};

    // Raw stack peek at RSP
    uint8_t  stackPeek[256] = {};
    uint32_t stackPeekBytes = 0;  // How many bytes of stackPeek are valid

    // ZCCF handles (32-bit, resolve on demand — zero = not resolved)
    uint32_t moduleHandle = 0;   // FileCache handle for the owning module
    uint32_t symbolHandle = 0;   // SymbolTable handle for current function

    // Unwind confidence [0.0, 1.0]
    float    confidence   = 0.0f;

    // Capture metadata
    CaptureMode captureMode = CaptureMode::Unknown;
    uint64_t    timestampUs = 0;  // Microseconds since session start
    uint32_t    threadId    = 0;
    uint32_t    processId   = 0;

    // Slot sequence number (monotonic within the ring)
    uint64_t    seq         = 0;
};

static_assert(sizeof(DebuggerFramePayload) <= 512,
              "DebuggerFramePayload exceeds ring slot budget");

// ============================================================================
// Error type
// ============================================================================

enum class DebugRingError : uint32_t {
    None        = 0,
    RingEmpty   = 1,
    InvalidSlot = 2,
};

template<typename T>
using DebugRingResult = std::expected<T, DebugRingError>;

// ============================================================================
// DebugRing
// ============================================================================

class DebugRing {
public:
    static constexpr uint32_t kDefaultCapacity = 256;

    explicit DebugRing(uint32_t capacity = kDefaultCapacity);
    ~DebugRing();

    DebugRing(const DebugRing&)            = delete;
    DebugRing& operator=(const DebugRing&) = delete;

    // -------------------------------------------------------------------------
    // Producer (debugger capture path — single producer)
    // -------------------------------------------------------------------------

    /// Push a new payload. Overwrites oldest slot if full.
    void Push(DebuggerFramePayload payload) noexcept;

    // -------------------------------------------------------------------------
    // Consumer (agent reads — any thread)
    // -------------------------------------------------------------------------

    /// Return the most recent payload. Empty optional if ring is empty.
    std::optional<DebuggerFramePayload> Latest() const noexcept;

    /// Return up to `count` most recent payloads (newest first).
    std::vector<DebuggerFramePayload> LatestN(uint32_t count) const;

    /// Return all payloads since the given sequence number (exclusive).
    std::vector<DebuggerFramePayload> Since(uint64_t afterSeq) const;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    uint32_t Capacity()   const noexcept;
    uint32_t Count()      const noexcept;
    uint64_t LastSeq()    const noexcept;

    /// Drain all entries (call when reattaching to a new debug session).
    void Clear() noexcept;

private:
    uint32_t                   m_capacity;
    std::vector<DebuggerFramePayload> m_slots;
    std::atomic<uint64_t>      m_head { 0 };   // next write position (mod capacity)
    std::atomic<uint64_t>      m_seq  { 0 };   // monotonic seq counter
    mutable uint32_t           m_lock_word { 0 }; // simple spinlock word
};

} // namespace ZCCF
} // namespace RawrXD
