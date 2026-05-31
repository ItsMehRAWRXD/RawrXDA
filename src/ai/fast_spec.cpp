// d:/rawrxd/src/ai/fast_spec.cpp
//
// FastSpec implementation. See fast_spec.h for design rationale.

#include "fast_spec.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace RawrXD {

namespace {

// Round capacity up to the next power of two and clamp to a sensible
// minimum. The SPSC ring relies on a power-of-two mask for branch-free
// modulo arithmetic.
constexpr size_t kMinRingLog2 = 8;   // 256 entries
constexpr size_t kMaxRingLog2 = 24;  // 16M entries

size_t ResolveRingCapacity(uint32_t requested_log2) noexcept {
    uint32_t lg = requested_log2;
    if (lg < kMinRingLog2) lg = kMinRingLog2;
    if (lg > kMaxRingLog2) lg = kMaxRingLog2;
    return static_cast<size_t>(1) << lg;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FastSpec::FastSpec(const Config& cfg)
    : m_cfg(cfg),
      m_ring_capacity(ResolveRingCapacity(cfg.ring_log2)),
      m_ring_mask(m_ring_capacity - 1)
{
    if (m_cfg.vocab_size == 0 || m_cfg.top_k == 0) {
        throw std::invalid_argument("FastSpec: vocab_size and top_k must be > 0");
    }

    // Bound the bigram table at ~1 GiB worst case to avoid runaway allocation
    // on misconfigured callers (e.g. vocab_size=1e9).
    const size_t entries = static_cast<size_t>(m_cfg.vocab_size) * m_cfg.top_k;
    const size_t budget_bytes =
        entries * (sizeof(std::atomic<uint32_t>) + sizeof(std::atomic<uint16_t>));
    if (budget_bytes > (size_t{1} << 30)) {
        throw std::invalid_argument("FastSpec: bigram table exceeds 1 GiB budget");
    }

    m_table_ids.reset(new std::atomic<uint32_t>[entries]);
    m_table_freq.reset(new std::atomic<uint16_t>[entries]);
    for (size_t i = 0; i < entries; ++i) {
        m_table_ids[i].store(kNoToken, std::memory_order_relaxed);
        m_table_freq[i].store(0,        std::memory_order_relaxed);
    }

    m_ring.reset(new ValidationRecord[m_ring_capacity]);
    for (size_t i = 0; i < m_ring_capacity; ++i) {
        m_ring[i] = ValidationRecord{kNoToken, kNoToken, kNoToken, 0};
    }

    if (m_cfg.start_drain) {
        m_drain = std::thread([this] { DrainLoop_(); });
    }
}

FastSpec::~FastSpec() {
    StopDrain();
}

// ---------------------------------------------------------------------------
// Hot path
// ---------------------------------------------------------------------------

size_t FastSpec::SpeculateFast(uint32_t anchor,
                               uint32_t* out_buf,
                               size_t   max_out) noexcept
{
    if (out_buf == nullptr || max_out == 0) {
        return 0;
    }
    if (anchor >= m_cfg.vocab_size) {
        // Out-of-vocab anchor: fail closed — emit nothing and increment a
        // dropped speculation. Caller will fall back to GPU sample.
        for (size_t i = 0; i < max_out; ++i) out_buf[i] = kNoToken;
        return 0;
    }

    const size_t k = std::min<size_t>(max_out, m_cfg.top_k);
    const size_t base = static_cast<size_t>(anchor) * m_cfg.top_k;

    size_t emitted = 0;
    for (size_t s = 0; s < k; ++s) {
        // memory_order_acquire pairs with release in ApplyValidation_.
        const uint32_t id = m_table_ids[base + s].load(std::memory_order_acquire);
        if (id == kNoToken) {
            out_buf[s] = kNoToken;
        } else {
            out_buf[s] = id;
            ++emitted;
        }
    }

    // Pad the tail with kNoToken so the caller can rely on the buffer shape.
    for (size_t s = k; s < max_out; ++s) {
        out_buf[s] = kNoToken;
    }

    // Stats: relaxed — readers tolerate eventual consistency.
    m_hot_speculations.fetch_add(1, std::memory_order_relaxed);
    m_hot_candidates_emit.fetch_add(emitted, std::memory_order_relaxed);
    return emitted;
}

bool FastSpec::EnqueueValidation(uint32_t anchor,
                                 uint32_t predicted,
                                 uint32_t actual) noexcept
{
    // SPSC produce. Single producer assumption: the inference hot loop.
    // Multiple producers must serialize externally.
    const uint64_t head = m_ring_head.load(std::memory_order_relaxed);
    const uint64_t tail = m_ring_tail.load(std::memory_order_acquire);
    if (head - tail >= m_ring_capacity) {
        m_validations_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const size_t slot = static_cast<size_t>(head & m_ring_mask);
    m_ring[slot].anchor    = anchor;
    m_ring[slot].predicted = predicted;
    m_ring[slot].actual    = actual;
    m_ring[slot]._pad      = 0;
    m_ring_head.store(head + 1, std::memory_order_release);
    m_validations_pushed.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------
// Off-path / cold paths
// ---------------------------------------------------------------------------

FastSpec::Stats FastSpec::GetStats() const noexcept {
    Stats s;
    s.hot_speculations    = m_hot_speculations.load(std::memory_order_relaxed);
    s.hot_candidates_emit = m_hot_candidates_emit.load(std::memory_order_relaxed);
    s.validations_pushed  = m_validations_pushed.load(std::memory_order_relaxed);
    s.validations_dropped = m_validations_dropped.load(std::memory_order_relaxed);
    s.validations_drained = m_validations_drained.load(std::memory_order_relaxed);
    s.correct_predictions = m_correct_predictions.load(std::memory_order_relaxed);
    const uint64_t total = s.validations_drained;
    s.accuracy_rate = (total > 0)
        ? static_cast<double>(s.correct_predictions) / static_cast<double>(total)
        : 0.0;
    return s;
}

void FastSpec::ResetStats() noexcept {
    m_hot_speculations.store(0,    std::memory_order_relaxed);
    m_hot_candidates_emit.store(0, std::memory_order_relaxed);
    m_validations_pushed.store(0,  std::memory_order_relaxed);
    m_validations_dropped.store(0, std::memory_order_relaxed);
    m_validations_drained.store(0, std::memory_order_relaxed);
    m_correct_predictions.store(0, std::memory_order_relaxed);
}

size_t FastSpec::DrainNow() noexcept {
    size_t drained = 0;
    for (;;) {
        const uint64_t head = m_ring_head.load(std::memory_order_acquire);
        const uint64_t tail = m_ring_tail.load(std::memory_order_relaxed);
        if (tail == head) break;
        const size_t slot = static_cast<size_t>(tail & m_ring_mask);
        ApplyValidation_(m_ring[slot]);
        m_ring_tail.store(tail + 1, std::memory_order_release);
        ++drained;
    }
    if (drained > 0) {
        m_validations_drained.fetch_add(drained, std::memory_order_relaxed);
    }
    return drained;
}

void FastSpec::StopDrain() noexcept {
    if (!m_drain_stop.exchange(true, std::memory_order_acq_rel)) {
        if (m_drain.joinable()) {
            m_drain.join();
        }
    }
    // Final drain so tests/observers see all enqueued records reflected.
    DrainNow();
}

// ---------------------------------------------------------------------------
// Validation worker
// ---------------------------------------------------------------------------

void FastSpec::DrainLoop_() {
    using namespace std::chrono_literals;
    while (!m_drain_stop.load(std::memory_order_acquire)) {
        const size_t n = DrainNow();
        if (n == 0) {
            std::this_thread::sleep_for(100us);
        }
    }
}

void FastSpec::ApplyValidation_(const ValidationRecord& v) noexcept {
    if (v.anchor >= m_cfg.vocab_size || v.actual == kNoToken) {
        return;
    }

    const bool correct = (v.predicted == v.actual);
    if (correct) {
        m_correct_predictions.fetch_add(1, std::memory_order_relaxed);
    }

    const size_t base = static_cast<size_t>(v.anchor) * m_cfg.top_k;

    // Find existing slot for `actual` or the lowest-frequency slot to evict.
    size_t match_slot   = m_cfg.top_k;
    size_t weakest_slot = 0;
    uint16_t weakest_freq = UINT16_MAX;
    for (size_t s = 0; s < m_cfg.top_k; ++s) {
        const uint32_t id   = m_table_ids[base + s].load(std::memory_order_relaxed);
        const uint16_t freq = m_table_freq[base + s].load(std::memory_order_relaxed);
        if (id == v.actual) {
            match_slot = s;
            break;
        }
        if (id == kNoToken) {
            // Empty slot is the weakest possible candidate for eviction.
            weakest_slot = s;
            weakest_freq = 0;
            // Keep scanning in case `actual` already exists later.
            continue;
        }
        if (freq < weakest_freq) {
            weakest_freq = freq;
            weakest_slot = s;
        }
    }

    if (match_slot < m_cfg.top_k) {
        // Bump existing entry, saturating at freq_clamp.
        const size_t idx = base + match_slot;
        uint16_t cur = m_table_freq[idx].load(std::memory_order_relaxed);
        const uint16_t inc = correct ? 2 : 1;
        const uint16_t cap = m_cfg.freq_clamp;
        const uint16_t next = static_cast<uint16_t>(
            (cur > cap - inc) ? cap : (cur + inc));
        m_table_freq[idx].store(next, std::memory_order_relaxed);
    } else {
        // Evict the weakest slot. Write the id with release so hot-path
        // readers using acquire see a coherent (id, freq) pair.
        const size_t idx = base + weakest_slot;
        m_table_freq[idx].store(1, std::memory_order_relaxed);
        m_table_ids[idx].store(v.actual, std::memory_order_release);
    }

    // Periodic decay: every 4 evictions, halve the weakest entry's freq
    // toward zero so cold tokens drop out over time. This is amortized
    // and only happens off the hot path.
    static thread_local uint32_t decay_tick = 0;
    if ((++decay_tick & 0x3) == 0) {
        for (size_t s = 0; s < m_cfg.top_k; ++s) {
            const size_t idx = base + s;
            uint16_t f = m_table_freq[idx].load(std::memory_order_relaxed);
            if (f > 1) {
                m_table_freq[idx].store(static_cast<uint16_t>(f - 1),
                                        std::memory_order_relaxed);
            }
        }
    }
}

} // namespace RawrXD
