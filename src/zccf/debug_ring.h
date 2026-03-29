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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

// ============================================================================
// Inline implementation (kept here to avoid target-link drift across trees)
// ============================================================================

namespace detail {
struct DebugRingSpinGuard {
    uint32_t* word;
    explicit DebugRingSpinGuard(uint32_t* w) : word(w) {
        while (InterlockedCompareExchange(reinterpret_cast<volatile long*>(word), 1L, 0L) != 0L) {
            YieldProcessor();
        }
    }
    ~DebugRingSpinGuard() {
        InterlockedExchange(reinterpret_cast<volatile long*>(word), 0L);
    }
};
} // namespace detail

inline DebugRing::DebugRing(uint32_t capacity)
    : m_capacity(capacity == 0 ? kDefaultCapacity : capacity),
      m_slots(m_capacity) {}

inline DebugRing::~DebugRing() = default;

inline void DebugRing::Push(DebuggerFramePayload payload) noexcept {
    detail::DebugRingSpinGuard g(&m_lock_word);
    uint64_t seq = m_seq.fetch_add(1) + 1;
    payload.seq = seq;
    uint64_t head = m_head.load();
    m_slots[head % m_capacity] = payload;
    m_head.store(head + 1);
}

inline std::optional<DebuggerFramePayload> DebugRing::Latest() const noexcept {
    detail::DebugRingSpinGuard g(&m_lock_word);
    uint64_t head = m_head.load();
    if (head == 0) return std::nullopt;
    return m_slots[(head - 1) % m_capacity];
}

inline std::vector<DebuggerFramePayload> DebugRing::LatestN(uint32_t count) const {
    detail::DebugRingSpinGuard g(&m_lock_word);
    std::vector<DebuggerFramePayload> out;
    uint64_t head = m_head.load();
    if (head == 0 || count == 0) return out;

    uint64_t avail = (head < m_capacity) ? head : m_capacity;
    uint64_t take = (count < avail) ? count : avail;
    out.reserve(static_cast<size_t>(take));
    for (uint64_t i = 0; i < take; ++i) {
        out.push_back(m_slots[(head - 1 - i) % m_capacity]);
    }
    return out;
}

inline std::vector<DebuggerFramePayload> DebugRing::Since(uint64_t afterSeq) const {
    detail::DebugRingSpinGuard g(&m_lock_word);
    std::vector<DebuggerFramePayload> out;
    uint64_t head = m_head.load();
    if (head == 0) return out;

    uint64_t avail = (head < m_capacity) ? head : m_capacity;
    uint64_t start = head - avail;
    for (uint64_t i = 0; i < avail; ++i) {
        const auto& p = m_slots[(start + i) % m_capacity];
        if (p.seq > afterSeq) out.push_back(p);
    }
    return out;
}

inline uint32_t DebugRing::Capacity() const noexcept {
    return m_capacity;
}

inline uint32_t DebugRing::Count() const noexcept {
    uint64_t head = m_head.load();
    return static_cast<uint32_t>((head < m_capacity) ? head : m_capacity);
}

inline uint64_t DebugRing::LastSeq() const noexcept {
    return m_seq.load();
}

inline void DebugRing::Clear() noexcept {
    detail::DebugRingSpinGuard g(&m_lock_word);
    m_head.store(0);
    m_seq.store(0);
    for (auto& s : m_slots) s = DebuggerFramePayload{};
}

} // namespace ZCCF
} // namespace RawrXD
