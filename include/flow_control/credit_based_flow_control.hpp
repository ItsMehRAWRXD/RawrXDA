// ============================================================================
// credit_based_flow_control.hpp - Lock-Free Token Budget System
// ============================================================================
// Replaces timeout-based backpressure with deterministic credit accounting
// 
// Key insight: Instead of waiting for buffer space, stages emit only when
// they have credits. This eliminates polling, timeouts, and spin loops.
//
// Architecture:
//   Producer (credits) → Consumer (debits)
//   Each token transfer = 1 credit consumed
//   Credits replenish asynchronously via completion notifications
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <optional>

namespace RawrXD {
namespace FlowControl {

// Credit budget configuration
struct CreditConfig {
    // Initial credits allocated to producer
    uint32_t initialCredits = 1024;
    
    // Maximum credits (backpressure threshold)
    uint32_t maxCredits = 4096;
    
    // Minimum credits before blocking (flow control threshold)
    uint32_t minCredits = 64;
    
    // Credit return batch size (amortize notification overhead)
    uint32_t returnBatchSize = 16;
    
    // Enable starvation prevention (reserve credits for partial batches)
    bool reserveForPartial = true;
    uint32_t partialReserve = 32;
    
    // Suppress diagnostic output (for TUI/dashboard use)
    bool silent = false;
};

// Credit transaction result
enum class CreditResult {
    Success,        // Credits acquired, proceed with emission
    Blocked,        // Insufficient credits, backpressure applied
    Partial,        // Partial credits available (for flush scenarios)
    Error           // Invalid state or configuration
};

// Token with credit metadata
struct CreditToken {
    uint64_t sequenceId;
    uint32_t creditsConsumed;
    bool isPartial;
    
    CreditToken(uint64_t seq = 0, uint32_t credits = 1, bool partial = false)
        : sequenceId(seq), creditsConsumed(credits), isPartial(partial) {}
};

// ============================================================================
// Lock-Free Credit Counter
// Single-producer, single-consumer credit accounting
// ============================================================================
class CreditCounter {
public:
    CreditCounter();
    ~CreditCounter();
    
    // Initialize with configuration
    bool Initialize(const CreditConfig& config);
    
    // Producer side: Attempt to acquire credits for emission
    // Non-blocking, returns immediately with result
    CreditResult TryAcquire(uint32_t requestedCredits);
    
    // Producer side: Acquire with partial allowance
    // Returns actual credits granted (may be less than requested)
    uint32_t TryAcquirePartial(uint32_t requestedCredits);
    
    // Consumer side: Return credits after processing
    // Called when tokens are consumed
    void ReturnCredits(uint32_t credits);
    
    // Consumer side: Return credits in batch (amortized)
    void ReturnCreditsBatch(uint32_t credits);
    
    // Get current available credits (approximate, for monitoring)
    uint32_t GetAvailableCredits() const;
    
    // Get total credits in circulation
    uint32_t GetTotalCredits() const { return config_.maxCredits; }
    
    // Check if system is in backpressure state
    bool IsBackpressured() const;
    
    // Get flow control statistics
    struct Stats {
        uint64_t acquireAttempts = 0;
        uint64_t acquireSuccess = 0;
        uint64_t acquireBlocked = 0;
        uint64_t acquirePartial = 0;
        uint64_t creditsReturned = 0;
        uint64_t batchReturns = 0;
        uint32_t currentAvailable = 0;
        uint32_t minObserved = 0;
        uint32_t maxObserved = 0;
    };
    
    Stats GetStats() const;
    void ResetStats();

private:
    CreditConfig config_;
    
    // Lock-free credit accounting
    // High 32 bits: credits available
    // Low 32 bits: sequence counter (for ABA protection)
    alignas(64) std::atomic<uint64_t> creditState_{0};
    
    // Statistics (relaxed memory order for performance)
    alignas(64) std::atomic<uint64_t> acquireAttempts_{0};
    alignas(64) std::atomic<uint64_t> acquireSuccess_{0};
    alignas(64) std::atomic<uint64_t> acquireBlocked_{0};
    alignas(64) std::atomic<uint64_t> acquirePartial_{0};
    alignas(64) std::atomic<uint64_t> creditsReturned_{0};
    alignas(64) std::atomic<uint64_t> batchReturns_{0};
    
    // Observed bounds
    std::atomic<uint32_t> minObserved_{0};
    std::atomic<uint32_t> maxObserved_{0};
    
    // Batch return accumulation
    alignas(64) std::atomic<uint32_t> pendingReturns_{0};
};

// ============================================================================
// Credit-Based SPSC Queue
// Drop-in replacement for LockFreeSPSC with credit-based flow control
// ============================================================================
template<typename T, size_t Capacity>
class CreditBasedSPSC {
public:
    CreditBasedSPSC();
    ~CreditBasedSPSC();
    
    // Initialize with credit configuration
    bool Initialize(const CreditConfig& config = CreditConfig{});
    
    // Producer: Try to push with credit acquisition
    // Returns true if pushed, false if blocked (insufficient credits)
    bool TryPush(const T& item, uint32_t credits = 1);
    
    // Producer: Force push (bypass credits, for emergency flush)
    bool ForcePush(const T& item);
    
    // Consumer: Pop item and return credits
    bool TryPop(T& item, uint32_t creditsToReturn = 1);
    
    // Consumer: Pop with batch credit return
    bool TryPopBatch(T& item, uint32_t batchSize);
    
    // Check if queue is empty
    bool Empty() const;
    
    // Get current utilization
    double Utilization() const;
    
    // Get available credits (for monitoring)
    uint32_t AvailableCredits() const { return credits_.GetAvailableCredits(); }
    
    // Check backpressure status
    bool IsBackpressured() const { return credits_.IsBackpressured(); }
    
    // Get credit statistics
    CreditCounter::Stats GetCreditStats() const { return credits_.GetStats(); }

private:
    // Ring buffer (same as LockFreeSPSC)
    alignas(64) T buffer_[Capacity];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    
    // Credit-based flow control
    CreditCounter credits_;
    
    // Statistics
    std::atomic<size_t> pushed_{0};
    std::atomic<size_t> popped_{0};
    std::atomic<size_t> blocked_{0};
};

// ============================================================================
// Pipeline Stage Credit Budget
// Multi-producer, multi-consumer credit allocation for pipeline stages
// ============================================================================
class PipelineCreditBudget {
public:
    PipelineCreditBudget();
    ~PipelineCreditBudget();
    
    // Initialize stage budgets
    // ingressBudget: tokens Stage 1 can inject
    // decodeBudget: speculative expansion budget for Stage 2
    // egressBudget: FP8 write budget for Stage 3
    bool Initialize(uint32_t ingressBudget, uint32_t decodeBudget, uint32_t egressBudget);
    
    // Stage 1: Acquire injection credits
    bool AcquireIngressCredits(uint32_t count);
    void ReleaseIngressCredits(uint32_t count);
    
    // Stage 2: Acquire decode credits (for speculative expansion)
    bool AcquireDecodeCredits(uint32_t count);
    void ReleaseDecodeCredits(uint32_t count);
    
    // Stage 3: Acquire egress credits (for FP8 writes)
    bool AcquireEgressCredits(uint32_t count);
    void ReleaseEgressCredits(uint32_t count);
    
    // Cross-stage credit transfer
    // When Stage 1 produces token, it transfers credits to Stage 2
    void TransferIngressToDecode(uint32_t count);
    
    // When Stage 2 produces token, it transfers credits to Stage 3
    void TransferDecodeToEgress(uint32_t count);
    
    // Get budget statistics
    struct BudgetStats {
        uint32_t ingressAvailable;
        uint32_t decodeAvailable;
        uint32_t egressAvailable;
        uint64_t ingressBlocked;
        uint64_t decodeBlocked;
        uint64_t egressBlocked;
    };
    
    BudgetStats GetStats() const;

private:
    CreditCounter ingressCredits_;
    CreditCounter decodeCredits_;
    CreditCounter egressCredits_;
    
    // Block counters
    std::atomic<uint64_t> ingressBlocked_{0};
    std::atomic<uint64_t> decodeBlocked_{0};
    std::atomic<uint64_t> egressBlocked_{0};
};

// ============================================================================
// Credit-Based Pipeline Integration
// ============================================================================

// Global pipeline budget (singleton access)
PipelineCreditBudget* GetGlobalPipelineBudget();
void InitializeGlobalPipelineBudget(uint32_t ingress, uint32_t decode, uint32_t egress);
void ShutdownGlobalPipelineBudget();

// Convenience macros for stage integration
#define ACQUIRE_INGRESS_CREDITS(count) \
    (RawrXD::FlowControl::GetGlobalPipelineBudget() ? \
     RawrXD::FlowControl::GetGlobalPipelineBudget()->AcquireIngressCredits(count) : false)

#define ACQUIRE_EGRESS_CREDITS(count) \
    (RawrXD::FlowControl::GetGlobalPipelineBudget() ? \
     RawrXD::FlowControl::GetGlobalPipelineBudget()->AcquireEgressCredits(count) : false)

#define RELEASE_EGRESS_CREDITS(count) \
    do { if (RawrXD::FlowControl::GetGlobalPipelineBudget()) \
         RawrXD::FlowControl::GetGlobalPipelineBudget()->ReleaseEgressCredits(count); } while(0)

} // namespace FlowControl
} // namespace RawrXD
