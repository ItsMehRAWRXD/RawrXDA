// d:/rawrxd/src/ai/fast_spec.h
//
// FastSpec — high-throughput speculative draft generator.
//
// Goal: 10K–50K+ speculative TPS with zero added latency on the inference
// hot path. Two rules make it possible:
//
//   1. Avoid real model pressure in the hot loop.
//      The hot path performs ONE bounded array read indexed by the last
//      token id and copies up to K candidate ids into a caller-provided
//      buffer. No allocations, no locks, no sorts, no softmax.
//
//   2. Move validation off-path.
//      Callers enqueue (predicted, actual) pairs into a lock-free SPSC ring;
//      a background drain thread updates the bigram table and accuracy
//      counters asynchronously. The hot loop never blocks on the trainer.
//
// The bigram table is a flat array of size [vocab_size * top_k] of
// `uint32_t` ids, plus a sibling [vocab_size * top_k] array of `uint16_t`
// frequency counters. Lookup is a single multiply + memcpy. All public
// hot-path methods are noexcept and lock-free.
//
// Stats are exposed via std::atomic; readers see eventually-consistent
// counters without taking a lock.
//
// This module is GPU-mandatory-friendly: it never substitutes for a real
// forward pass. It only proposes candidates; the GPU verifier still runs.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace RawrXD {

class FastSpec {
public:
    // Sentinel meaning "no candidate".
    static constexpr uint32_t kNoToken = UINT32_MAX;

    struct Config {
        uint32_t vocab_size   = 32000;  // model vocab
        uint32_t top_k        = 4;      // candidates per anchor token
        uint32_t ring_log2    = 16;     // SPSC ring capacity = 1 << ring_log2
        uint16_t freq_clamp   = 4096;   // saturate counters before drain
        bool     start_drain  = true;   // launch the validation thread
    };

    struct Stats {
        // All counters are eventually consistent across threads.
        uint64_t hot_speculations    = 0;  // SpeculateFast calls served
        uint64_t hot_candidates_emit = 0;  // total candidate ids emitted
        uint64_t validations_pushed  = 0;  // pushes into ring (success)
        uint64_t validations_dropped = 0;  // pushes into ring (full)
        uint64_t validations_drained = 0;  // pulled by drain thread
        uint64_t correct_predictions = 0;  // predicted == actual
        double   accuracy_rate       = 0.0;
    };

    explicit FastSpec(const Config& cfg);
    ~FastSpec();

    FastSpec(const FastSpec&)            = delete;
    FastSpec& operator=(const FastSpec&) = delete;

    // ------------------------------------------------------------------
    // Hot path. No locks, no allocations, noexcept.
    // ------------------------------------------------------------------

    // Fill out_buf[0..min(max_out, top_k)) with candidate token ids for
    // the given anchor (last token). Unfilled slots are kNoToken.
    // Returns the number of candidates written.
    size_t SpeculateFast(uint32_t anchor,
                         uint32_t* out_buf,
                         size_t   max_out) noexcept;

    // Off-path enqueue. Returns true on success, false if the ring is
    // full (caller may discard or retry). Lock-free SPSC.
    bool EnqueueValidation(uint32_t anchor,
                           uint32_t predicted,
                           uint32_t actual) noexcept;

    // ------------------------------------------------------------------
    // Cold path.
    // ------------------------------------------------------------------

    // Read a snapshot of stats. Lock-free.
    Stats GetStats() const noexcept;

    // Reset stats counters (does not clear the bigram table).
    void ResetStats() noexcept;

    // Drain pending validations synchronously. Used by tests to observe
    // training without waiting for the background thread.
    size_t DrainNow() noexcept;

    // Stop the drain thread (called automatically by destructor).
    void StopDrain() noexcept;

    const Config& GetConfig() const noexcept { return m_cfg; }

private:
    struct ValidationRecord {
        uint32_t anchor;
        uint32_t predicted;
        uint32_t actual;
        uint32_t _pad;
    };

    void DrainLoop_();
    void ApplyValidation_(const ValidationRecord& v) noexcept;
    size_t IndexFor_(uint32_t anchor, uint32_t slot) const noexcept {
        return static_cast<size_t>(anchor) * m_cfg.top_k + slot;
    }

    Config m_cfg;

    // Bigram table — flat [vocab_size * top_k] arrays.
    std::unique_ptr<std::atomic<uint32_t>[]> m_table_ids;
    std::unique_ptr<std::atomic<uint16_t>[]> m_table_freq;

    // SPSC ring of validation records.
    size_t m_ring_capacity;
    size_t m_ring_mask;
    std::unique_ptr<ValidationRecord[]> m_ring;
    alignas(64) std::atomic<uint64_t> m_ring_head{0}; // producer cursor
    alignas(64) std::atomic<uint64_t> m_ring_tail{0}; // consumer cursor

    // Stats — atomic, eventually consistent.
    alignas(64) std::atomic<uint64_t> m_hot_speculations{0};
    std::atomic<uint64_t> m_hot_candidates_emit{0};
    std::atomic<uint64_t> m_validations_pushed{0};
    std::atomic<uint64_t> m_validations_dropped{0};
    std::atomic<uint64_t> m_validations_drained{0};
    std::atomic<uint64_t> m_correct_predictions{0};

    // Drain thread.
    std::thread m_drain;
    std::atomic<bool> m_drain_stop{false};
};

} // namespace RawrXD
