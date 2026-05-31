// ============================================================================
// pipeline_stage3_credit_based.cpp - Stage 3 with Credit-Based Flow Control
// ============================================================================
// Replaces timeout-based backpressure with deterministic credit accounting
//
// Key changes from original:
// - No more _mm_pause() spin loops
// - No more timeout-based partial flush
// - Credit acquisition determines emission eligibility
// - Deterministic backpressure instead of temporal heuristics
// ============================================================================

#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <printf>
#include <immintrin.h>

#include "flow_control/credit_based_flow_control.hpp"

// External MASM FP8 kernel
extern "C" void SovereignQuantizeE4M3(float* input, uint8_t* output, size_t count, float scale);

using namespace std::chrono;
using namespace RawrXD::FlowControl;

// Stage 3 configuration
constexpr size_t FP8_BATCH_SIZE = 64;

// Stage 3 metrics with credit tracking
struct Stage3CreditMetrics {
    std::atomic<size_t> items_processed{0};
    std::atomic<size_t> batches_processed{0};
    std::atomic<size_t> credits_acquired{0};
    std::atomic<size_t> credits_blocked{0};
    std::atomic<size_t> partial_batches{0};
    std::atomic<size_t> empty_pops{0};
    
    void RecordBatch(size_t items) {
        items_processed += items;
        batches_processed++;
    }
    
    void RecordCreditAcquire(size_t credits) {
        credits_acquired += credits;
    }
    
    void RecordCreditBlocked() {
        credits_blocked++;
    }
    
    void RecordPartialBatch() { partial_batches++; }
    void RecordEmptyPop() { empty_pops++; }
};

// Lock-free SPSC queue (simplified)
template<typename T, size_t Capacity>
class LockFreeSPSC {
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    T buffer_[Capacity];
    
public:
    bool TryPush(const T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next = (tail + 1) % Capacity;
        if (next == head_.load(std::memory_order_acquire)) {
            return false;  // Full
        }
        buffer_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }
    
    bool TryPop(T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // Empty
        }
        item = buffer_[head];
        head_.store((head + 1) % Capacity, std::memory_order_release);
        return true;
    }
    
    bool Empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
};

// ============================================================================
// Stage 3: Consumer with CREDIT-BASED flow control
// ============================================================================
void Stage3_Egress_CreditBased(
    LockFreeSPSC<uint32_t, 65536>& egress_buffer,
    std::atomic<bool>& decoder_done,
    Stage3CreditMetrics& metrics) {
    
    printf("[Stage3] Starting with CREDIT-BASED flow control\n");
    
    // Initialize credit-based flow control
    // Stage 3 needs egress credits for FP8 writes
    CreditConfig egressConfig;
    egressConfig.initialCredits = 1024;    // Start with 1024 tokens budget
    egressConfig.maxCredits = 4096;       // Max 4096 tokens in flight
    egressConfig.minCredits = 64;         // Backpressure threshold
    egressConfig.returnBatchSize = 16;    // Amortize credit returns
    egressConfig.reserveForPartial = true;
    egressConfig.partialReserve = 32;     // Reserve for partial batches
    
    CreditCounter egressCredits;
    egressCredits.Initialize(egressConfig);
    
    // Output buffer
    std::vector<uint32_t> output_tokens;
    output_tokens.reserve(100000);
    
    // FP8 batch accumulation
    alignas(64) float fp8_input[FP8_BATCH_SIZE];
    alignas(64) uint8_t fp8_output[FP8_BATCH_SIZE];
    size_t fp8_accumulated = 0;
    
    bool running = true;
    uint64_t batch_id = 0;
    
    // Credit return batching
    uint32_t creditsToReturn = 0;
    
    while (running) {
        // Try to pop from egress (NON-BLOCKING)
        uint32_t token;
        bool got_token = egress_buffer.TryPop(token);
        
        if (got_token) {
            // Accumulate for FP8 batch
            fp8_input[fp8_accumulated] = static_cast<float>(token);
            fp8_accumulated++;
            
            // Check if we should flush
            bool shouldFlush = (fp8_accumulated >= FP8_BATCH_SIZE);
            
            // CREDIT-BASED DECISION:
            // Only flush if we have credits for the output
            if (shouldFlush) {
                auto creditResult = egressCredits.TryAcquire(FP8_BATCH_SIZE);
                
                if (creditResult == CreditResult::Success) {
                    // We have credits - proceed with FP8 quantization
                    metrics.RecordCreditAcquire(FP8_BATCH_SIZE);
                    
                    auto t1 = high_resolution_clock::now();
                    
                    // Run FP8 kernel
                    SovereignQuantizeE4M3(fp8_input, fp8_output, FP8_BATCH_SIZE, 1.0f);
                    
                    // Store quantized output
                    for (size_t i = 0; i < FP8_BATCH_SIZE; i++) {
                        output_tokens.push_back(static_cast<uint32_t>(fp8_output[i]));
                    }
                    
                    auto t2 = high_resolution_clock::now();
                    metrics.RecordBatch(FP8_BATCH_SIZE);
                    
                    fp8_accumulated = 0;
                    batch_id++;
                    
                    // Return credits for consumed tokens (batch amortized)
                    creditsToReturn += FP8_BATCH_SIZE;
                    if (creditsToReturn >= egressConfig.returnBatchSize) {
                        egressCredits.ReturnCredits(creditsToReturn);
                        creditsToReturn = 0;
                    }
                    
                } else {
                    // CREDIT BACKPRESSURE:
                    // No credits available - wait without spinning
                    metrics.RecordCreditBlocked();
                    
                    // Instead of _mm_pause() loop, we do cooperative yield
                    // The producer will return credits as it consumes
                    std::this_thread::yield();
                    
                    // Decrement accumulation counter since we didn't process
                    fp8_accumulated--;
                }
            }
        } else {
            metrics.RecordEmptyPop();
            
            // CREDIT-BASED PARTIAL FLUSH:
            // If we have accumulated tokens and decoder is done,
            // try to acquire partial credits
            if (fp8_accumulated > 0 && decoder_done.load()) {
                auto partialCredits = egressCredits.TryAcquirePartial(fp8_accumulated);
                
                if (partialCredits > 0) {
                    // Zero-pad to FP8_BATCH_SIZE
                    for (size_t i = fp8_accumulated; i < FP8_BATCH_SIZE; i++) {
                        fp8_input[i] = 0.0f;
                    }
                    
                    SovereignQuantizeE4M3(fp8_input, fp8_output, FP8_BATCH_SIZE, 1.0f);
                    
                    // Only store actual tokens
                    for (size_t i = 0; i < fp8_accumulated; i++) {
                        output_tokens.push_back(static_cast<uint32_t>(fp8_output[i]));
                    }
                    
                    metrics.RecordPartialBatch();
                    fp8_accumulated = 0;
                    
                    // Return credits
                    egressCredits.ReturnCredits(partialCredits);
                }
            }
            
            // Check termination
            if (decoder_done.load() && egress_buffer.Empty() && fp8_accumulated == 0) {
                running = false;
            } else {
                // No work available - yield instead of spin
                std::this_thread::yield();
            }
        }
        
        // Periodic progress with credit status
        if (output_tokens.size() % 50000 == 0 && output_tokens.size() > 0) {
            auto stats = egressCredits.GetStats();
            printf("[Stage3] Processed %zu tokens | "
                   "credits=%u (min=%u, max=%u) | blocked=%llu\n",
                   output_tokens.size(),
                   stats.currentAvailable,
                   stats.minObserved,
                   stats.maxObserved,
                   (unsigned long long)stats.acquireBlocked);
        }
    }
    
    // Final flush of any remaining tokens
    if (fp8_accumulated > 0) {
        printf("[Stage3] Final flush: %zu tokens\n", fp8_accumulated);
        
        // Try to acquire partial credits for final flush
        auto partialCredits = egressCredits.TryAcquirePartial(fp8_accumulated);
        if (partialCredits > 0) {
            for (size_t i = fp8_accumulated; i < FP8_BATCH_SIZE; i++) {
                fp8_input[i] = 0.0f;
            }
            SovereignQuantizeE4M3(fp8_input, fp8_output, FP8_BATCH_SIZE, 1.0f);
            for (size_t i = 0; i < fp8_accumulated; i++) {
                output_tokens.push_back(static_cast<uint32_t>(fp8_output[i]));
            }
        }
    }
    
    // Print final credit stats
    auto stats = egressCredits.GetStats();
    printf("\n[Stage3] DONE - Output tokens: %zu\n", output_tokens.size());
    printf("[Stage3] Credit Stats:\n");
    printf("  Acquire attempts: %llu\n", (unsigned long long)stats.acquireAttempts);
    printf("  Success: %llu (%.1f%%)\n", 
           (unsigned long long)stats.acquireSuccess,
           stats.acquireAttempts > 0 ? (100.0 * stats.acquireSuccess / stats.acquireAttempts) : 0.0);
    printf("  Blocked: %llu\n", (unsigned long long)stats.acquireBlocked);
    printf("  Credits returned: %llu\n", (unsigned long long)stats.creditsReturned);
    printf("  Min observed: %u, Max observed: %u\n", stats.minObserved, stats.maxObserved);
}

// ============================================================================
// Comparison: Original vs Credit-Based
// ============================================================================
/*

ORIGINAL (Timeout-Based):
-------------------------
while (running) {
    if (got_token) {
        accumulate();
        if (batch_full) {
            flush();  // Always flushes
        }
    } else {
        if (timeout_expired) {
            partial_flush();  // Time-based heuristic
        }
        if (consecutive_empty > 1000) {
            _mm_pause();  // Spin loop
        }
    }
}

Problems:
- Spin loops waste CPU
- Timeout heuristics are non-deterministic
- Partial flush timing is arbitrary
- No bounded memory guarantee

CREDIT-BASED:
-------------
while (running) {
    if (got_token) {
        accumulate();
        if (batch_full) {
            if (acquire_credits(batch_size)) {  // Bounded check
                flush();  // Only with credits
            } else {
                yield();  // Cooperative, not spin
            }
        }
    } else {
        if (decoder_done && accumulated > 0) {
            partial = acquire_partial_credits();  // Bounded
            if (partial > 0) {
                partial_flush(partial);  // Credit-determined
            }
        }
        if (no_work) {
            yield();  // Always yield, never spin
        }
    }
}

Advantages:
- No spin loops (cooperative scheduling)
- Deterministic memory bounds (credits = capacity)
- No arbitrary timeouts
- Provable stability (credits never exceed max)
- Backpressure is explicit, not heuristic

*/

// ============================================================================
// Integration Guide
// ============================================================================
/*

To integrate credit-based flow control:

1. Replace LockFreeSPSC with CreditBasedSPSC:
   CreditBasedSPSC<uint32_t, 65536> queue;
   queue.Initialize(creditConfig);

2. In producer (Stage 1):
   if (queue.TryPush(token, credits=1)) {
       // Success - credits acquired automatically
   } else {
       // Backpressure - queue full or no credits
   }

3. In consumer (Stage 3):
   uint32_t creditsToReturn = 0;
   while (running) {
       if (queue.TryPop(token, creditsToReturn)) {
           process(token);
           creditsToReturn += tokens_consumed;
       }
   }

4. For cross-stage flow control:
   InitializeGlobalPipelineBudget(ingress=4096, decode=8192, egress=4096);
   
   // Stage 1
   if (ACQUIRE_INGRESS_CREDITS(count)) {
       inject_tokens(count);
       TRANSFER_INGRESS_TO_DECODE(count);  // Release ingress, acquire decode
   }
   
   // Stage 3
   if (ACQUIRE_EGRESS_CREDITS(batch_size)) {
       quantize_and_emit(batch);
       RELEASE_EGRESS_CREDITS(batch_size);  // Return to pool
   }

Key insight: Credits = capacity contracts
- You cannot emit without capacity
- Capacity is returned when consumed
- System is always bounded
- No temporal uncertainty

*/
