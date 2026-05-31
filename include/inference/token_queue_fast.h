#pragma once
// =============================================================================
// token_queue_fast.h — Cache-line-padded SPSC ring buffer for inference dispatch
// =============================================================================
//
// Provides TokenQueueFast: a single-producer / single-consumer (SPSC) ring
// buffer optimised for the inference decode hot-path.  Cache lines are fully
// isolated between head and tail to prevent false sharing.
//
// Companion: src/asm/token_queue_dequeue.asm exports IC_TokenBatchDequeue,
// which bulk-pops up to N tokens per call using AVX2 vmovdqu.
//
// Layout (must match token_queue_dequeue.asm offsets TQ_*):
//   +0   : head      (DWORD, consume pointer, exclusive cache line)
//   +64  : tail      (DWORD, produce pointer, exclusive cache line)
//   +128 : capacity  (DWORD, power-of-2 ring size)
//   +192 : tokens[]  (int32_t ring data, capacity elements)
//
// Usage:
//   // Allocate aligned queue + ring data together:
//   auto* q = TokenQueueFast::create(4096);  // capacity must be power-of-2
//   q->push(tokenId);                        // producer thread
//   int32_t batch[32]; int n = q->dequeue(batch, 32); // consumer thread
// =============================================================================

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cassert>

extern "C" {
    // AVX2 batch dequeue (token_queue_dequeue.asm)
    // Returns: number of tokens actually dequeued (0..min(available, maxCount))
    int32_t IC_TokenBatchDequeue(void* queue, int32_t* dst, int32_t maxCount);
}

namespace RawrXD {
namespace Inference {

// Offset constants — must match token_queue_dequeue.asm
constexpr size_t kTQ_HeadOffset     = 0;
constexpr size_t kTQ_TailOffset     = 64;
constexpr size_t kTQ_CapacityOffset = 128;
constexpr size_t kTQ_TokensOffset   = 192;

#pragma pack(push, 1)
struct TokenQueueFast {
    // ---- Consumer cache line ----
    alignas(64) volatile int32_t head;
    int32_t _pad0[15];

    // ---- Producer cache line ----
    alignas(64) volatile int32_t tail;
    int32_t _pad1[15];

    // ---- Descriptor cache line ----
    alignas(64) int32_t capacity;          // power-of-2
    int32_t _pad2[15];

    // ---- Ring data (capacity × int32_t) ----
    alignas(64) int32_t tokens[1];         // variable-length tail; use create()

    // ---- Factory: heap-allocates queue + ring data together ----
    static TokenQueueFast* create(int32_t cap) {
        // cap must be a power-of-2 and >= 1
        assert(cap > 0 && (cap & (cap - 1)) == 0);
        const size_t bytes = kTQ_TokensOffset + static_cast<size_t>(cap) * sizeof(int32_t);
#if defined(_WIN32)
        auto* q = static_cast<TokenQueueFast*>(_aligned_malloc(bytes, 64));
#else
        void* raw = nullptr;
        if (posix_memalign(&raw, 64, bytes) != 0) return nullptr;
        auto* q = static_cast<TokenQueueFast*>(raw);
#endif
        if (!q) return nullptr;
        std::memset(q, 0, bytes);
        q->capacity = cap;
        return q;
    }

    static void destroy(TokenQueueFast* q) {
#if defined(_WIN32)
        _aligned_free(q);
#else
        free(q);
#endif
    }

    // ---- Inline push (single producer) ----
    // Returns false if ring is full.
    bool push(int32_t token) noexcept {
        const int32_t t = tail;
        const int32_t h = head;
        if ((t - h) >= capacity) return false;   // full
        tokens[t & (capacity - 1)] = token;
        // Release store: visible to consumer after data write
        std::atomic_thread_fence(std::memory_order_release);
        const_cast<volatile int32_t&>(tail) = t + 1;
        return true;
    }

    // ---- Batch push (single producer) ----
    // Returns number of tokens actually pushed.
    int32_t pushBatch(const int32_t* src, int32_t count) noexcept {
        int32_t pushed = 0;
        while (pushed < count) {
            if (!push(src[pushed])) break;
            ++pushed;
        }
        return pushed;
    }

    // ---- Batch dequeue: delegates to AVX2 MASM kernel ----
    // Returns number of tokens dequeued.
    int32_t dequeue(int32_t* dst, int32_t maxCount) noexcept {
        return IC_TokenBatchDequeue(this, dst, maxCount);
    }

    // ---- Query available count (approximate — may be stale for MPMC) ----
    int32_t available() const noexcept {
        return tail - head;    // unsigned arithmetic wraps correctly
    }

    // ---- Reset (only safe when both sides are quiesced) ----
    void reset() noexcept {
        head = 0;
        tail = 0;
    }

private:
    TokenQueueFast() = delete;
    ~TokenQueueFast() = delete;
};
#pragma pack(pop)

// Compile-time offset assertions (catch layout drift early)
static_assert(offsetof(TokenQueueFast, head)     == kTQ_HeadOffset,     "TQ head offset mismatch");
static_assert(offsetof(TokenQueueFast, tail)     == kTQ_TailOffset,     "TQ tail offset mismatch");
static_assert(offsetof(TokenQueueFast, capacity) == kTQ_CapacityOffset, "TQ capacity offset mismatch");
static_assert(offsetof(TokenQueueFast, tokens)   == kTQ_TokensOffset,   "TQ tokens offset mismatch");

} // namespace Inference
} // namespace RawrXD
