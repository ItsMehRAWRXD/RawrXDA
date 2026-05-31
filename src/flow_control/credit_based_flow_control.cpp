// ============================================================================
// credit_based_flow_control.cpp - Lock-Free Token Budget Implementation
// ============================================================================

#include "flow_control/credit_based_flow_control.hpp"
#include <cstdio>
#include <algorithm>

namespace RawrXD {
namespace FlowControl {

// ============================================================================
// CreditCounter Implementation
// ============================================================================
CreditCounter::CreditCounter() = default;
CreditCounter::~CreditCounter() = default;

bool CreditCounter::Initialize(const CreditConfig& config) {
    config_ = config;
    
    // Initialize state: high 32 bits = credits, low 32 bits = sequence
    uint64_t initialState = (static_cast<uint64_t>(config.initialCredits) << 32) | 0;
    creditState_.store(initialState, std::memory_order_relaxed);
    
    minObserved_.store(config.initialCredits, std::memory_order_relaxed);
    maxObserved_.store(config.initialCredits, std::memory_order_relaxed);
    pendingReturns_.store(0, std::memory_order_relaxed);
    
    if (!config.silent) {
        printf("[CreditCounter] Initialized: initial=%u, max=%u, min=%u\n",
               config.initialCredits, config.maxCredits, config.minCredits);
    }
    return true;
}

CreditResult CreditCounter::TryAcquire(uint32_t requestedCredits) {
    acquireAttempts_.fetch_add(1, std::memory_order_relaxed);
    
    // Fast path: check if we have enough credits
    uint64_t currentState = creditState_.load(std::memory_order_acquire);
    uint32_t currentCredits = static_cast<uint32_t>(currentState >> 32);
    
    // Check for partial batch reservation
    uint32_t effectiveMin = config_.minCredits;
    if (config_.reserveForPartial) {
        effectiveMin = std::max(effectiveMin, config_.partialReserve);
    }
    
    // Not enough credits - blocked
    if (currentCredits < requestedCredits + effectiveMin) {
        acquireBlocked_.fetch_add(1, std::memory_order_relaxed);
        return CreditResult::Blocked;
    }
    
    // Attempt CAS to acquire credits
    uint64_t newState = ((static_cast<uint64_t>(currentCredits - requestedCredits) << 32) | 
                         ((currentState + 1) & 0xFFFFFFFF));
    
    if (creditState_.compare_exchange_strong(currentState, newState,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
        // Success - update stats
        acquireSuccess_.fetch_add(1, std::memory_order_relaxed);
        
        uint32_t remaining = currentCredits - requestedCredits;
        
        // Update observed bounds
        uint32_t minObs = minObserved_.load(std::memory_order_relaxed);
        if (remaining < minObs) {
            minObserved_.store(remaining, std::memory_order_relaxed);
        }
        
        return CreditResult::Success;
    }
    
    // CAS failed - another thread modified state
    // For simplicity, treat as blocked (caller can retry)
    acquireBlocked_.fetch_add(1, std::memory_order_relaxed);
    return CreditResult::Blocked;
}

uint32_t CreditCounter::TryAcquirePartial(uint32_t requestedCredits) {
    acquireAttempts_.fetch_add(1, std::memory_order_relaxed);
    
    uint64_t currentState = creditState_.load(std::memory_order_acquire);
    uint32_t currentCredits = static_cast<uint32_t>(currentState >> 32);
    
    // Calculate how many credits we can actually grant
    uint32_t effectiveMin = config_.minCredits;
    if (config_.reserveForPartial) {
        effectiveMin = std::max(effectiveMin, config_.partialReserve);
    }
    
    uint32_t availableForUse = (currentCredits > effectiveMin) ? 
                                (currentCredits - effectiveMin) : 0;
    
    if (availableForUse == 0) {
        acquireBlocked_.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    
    uint32_t granted = std::min(requestedCredits, availableForUse);
    
    // Attempt CAS
    uint64_t newState = ((static_cast<uint64_t>(currentCredits - granted) << 32) | 
                         ((currentState + 1) & 0xFFFFFFFF));
    
    if (creditState_.compare_exchange_strong(currentState, newState,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
        acquirePartial_.fetch_add(1, std::memory_order_relaxed);
        
        uint32_t remaining = currentCredits - granted;
        uint32_t minObs = minObserved_.load(std::memory_order_relaxed);
        if (remaining < minObs) {
            minObserved_.store(remaining, std::memory_order_relaxed);
        }
        
        return granted;
    }
    
    // CAS failed
    acquireBlocked_.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

void CreditCounter::ReturnCredits(uint32_t credits) {
    // Atomically add credits back
    uint64_t currentState = creditState_.load(std::memory_order_acquire);
    uint32_t currentCredits = static_cast<uint32_t>(currentState >> 32);
    
    // Cap at max credits
    uint32_t newCredits = std::min(currentCredits + credits, config_.maxCredits);
    
    uint64_t newState = ((static_cast<uint64_t>(newCredits) << 32) | 
                         ((currentState + 1) & 0xFFFFFFFF));
    
    // Spin until CAS succeeds
    while (!creditState_.compare_exchange_weak(currentState, newState,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
        currentCredits = static_cast<uint32_t>(currentState >> 32);
        newCredits = std::min(currentCredits + credits, config_.maxCredits);
        newState = ((static_cast<uint64_t>(newCredits) << 32) | 
                    ((currentState + 1) & 0xFFFFFFFF));
    }
    
    creditsReturned_.fetch_add(credits, std::memory_order_relaxed);
    
    // Update max observed
    uint32_t maxObs = maxObserved_.load(std::memory_order_relaxed);
    if (newCredits > maxObs) {
        maxObserved_.store(newCredits, std::memory_order_relaxed);
    }
}

void CreditCounter::ReturnCreditsBatch(uint32_t credits) {
    // Accumulate in pending returns
    uint32_t pending = pendingReturns_.fetch_add(credits, std::memory_order_relaxed);
    
    // If we've reached batch size, flush
    if (pending + credits >= config_.returnBatchSize) {
        uint32_t toReturn = pendingReturns_.exchange(0, std::memory_order_relaxed);
        if (toReturn > 0) {
            ReturnCredits(toReturn);
            batchReturns_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

uint32_t CreditCounter::GetAvailableCredits() const {
    uint64_t state = creditState_.load(std::memory_order_acquire);
    return static_cast<uint32_t>(state >> 32);
}

bool CreditCounter::IsBackpressured() const {
    uint32_t credits = GetAvailableCredits();
    uint32_t effectiveMin = config_.minCredits;
    if (config_.reserveForPartial) {
        effectiveMin = std::max(effectiveMin, config_.partialReserve);
    }
    return credits <= effectiveMin;
}

CreditCounter::Stats CreditCounter::GetStats() const {
    Stats stats;
    stats.acquireAttempts = acquireAttempts_.load(std::memory_order_relaxed);
    stats.acquireSuccess = acquireSuccess_.load(std::memory_order_relaxed);
    stats.acquireBlocked = acquireBlocked_.load(std::memory_order_relaxed);
    stats.acquirePartial = acquirePartial_.load(std::memory_order_relaxed);
    stats.creditsReturned = creditsReturned_.load(std::memory_order_relaxed);
    stats.batchReturns = batchReturns_.load(std::memory_order_relaxed);
    stats.currentAvailable = GetAvailableCredits();
    stats.minObserved = minObserved_.load(std::memory_order_relaxed);
    stats.maxObserved = maxObserved_.load(std::memory_order_relaxed);
    return stats;
}

void CreditCounter::ResetStats() {
    acquireAttempts_.store(0, std::memory_order_relaxed);
    acquireSuccess_.store(0, std::memory_order_relaxed);
    acquireBlocked_.store(0, std::memory_order_relaxed);
    acquirePartial_.store(0, std::memory_order_relaxed);
    creditsReturned_.store(0, std::memory_order_relaxed);
    batchReturns_.store(0, std::memory_order_relaxed);
    minObserved_.store(config_.initialCredits, std::memory_order_relaxed);
    maxObserved_.store(config_.initialCredits, std::memory_order_relaxed);
}

// ============================================================================
// PipelineCreditBudget Implementation
// ============================================================================
PipelineCreditBudget::PipelineCreditBudget() = default;
PipelineCreditBudget::~PipelineCreditBudget() = default;

bool PipelineCreditBudget::Initialize(uint32_t ingressBudget, uint32_t decodeBudget, uint32_t egressBudget) {
    CreditConfig ingressConfig;
    ingressConfig.initialCredits = ingressBudget;
    ingressConfig.maxCredits = ingressBudget;
    ingressConfig.minCredits = ingressBudget / 8;  // 12.5% threshold
    
    CreditConfig decodeConfig;
    decodeConfig.initialCredits = decodeBudget;
    decodeConfig.maxCredits = decodeBudget;
    decodeConfig.minCredits = decodeBudget / 8;
    
    CreditConfig egressConfig;
    egressConfig.initialCredits = egressBudget;
    egressConfig.maxCredits = egressBudget;
    egressConfig.minCredits = egressBudget / 8;
    
    bool success = true;
    success &= ingressCredits_.Initialize(ingressConfig);
    success &= decodeCredits_.Initialize(decodeConfig);
    success &= egressCredits_.Initialize(egressConfig);
    
    printf("[PipelineCreditBudget] Initialized: ingress=%u, decode=%u, egress=%u\n",
           ingressBudget, decodeBudget, egressBudget);
    return success;
}

bool PipelineCreditBudget::AcquireIngressCredits(uint32_t count) {
    auto result = ingressCredits_.TryAcquire(count);
    if (result == CreditResult::Blocked) {
        ingressBlocked_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void PipelineCreditBudget::ReleaseIngressCredits(uint32_t count) {
    ingressCredits_.ReturnCredits(count);
}

bool PipelineCreditBudget::AcquireDecodeCredits(uint32_t count) {
    auto result = decodeCredits_.TryAcquire(count);
    if (result == CreditResult::Blocked) {
        decodeBlocked_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void PipelineCreditBudget::ReleaseDecodeCredits(uint32_t count) {
    decodeCredits_.ReturnCredits(count);
}

bool PipelineCreditBudget::AcquireEgressCredits(uint32_t count) {
    auto result = egressCredits_.TryAcquire(count);
    if (result == CreditResult::Blocked) {
        egressBlocked_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void PipelineCreditBudget::ReleaseEgressCredits(uint32_t count) {
    egressCredits_.ReturnCredits(count);
}

void PipelineCreditBudget::TransferIngressToDecode(uint32_t count) {
    // Release from ingress, acquire in decode
    ingressCredits_.ReturnCredits(count);
    decodeCredits_.TryAcquire(count);  // Should succeed since we just released
}

void PipelineCreditBudget::TransferDecodeToEgress(uint32_t count) {
    // Release from decode, acquire in egress
    decodeCredits_.ReturnCredits(count);
    egressCredits_.TryAcquire(count);
}

PipelineCreditBudget::BudgetStats PipelineCreditBudget::GetStats() const {
    BudgetStats stats;
    stats.ingressAvailable = ingressCredits_.GetAvailableCredits();
    stats.decodeAvailable = decodeCredits_.GetAvailableCredits();
    stats.egressAvailable = egressCredits_.GetAvailableCredits();
    stats.ingressBlocked = ingressBlocked_.load(std::memory_order_relaxed);
    stats.decodeBlocked = decodeBlocked_.load(std::memory_order_relaxed);
    stats.egressBlocked = egressBlocked_.load(std::memory_order_relaxed);
    return stats;
}

// ============================================================================
// Global Pipeline Budget
// ============================================================================
static PipelineCreditBudget* g_pipelineBudget = nullptr;

PipelineCreditBudget* GetGlobalPipelineBudget() {
    return g_pipelineBudget;
}

void InitializeGlobalPipelineBudget(uint32_t ingress, uint32_t decode, uint32_t egress) {
    if (!g_pipelineBudget) {
        g_pipelineBudget = new PipelineCreditBudget();
        g_pipelineBudget->Initialize(ingress, decode, egress);
    }
}

void ShutdownGlobalPipelineBudget() {
    if (g_pipelineBudget) {
        auto stats = g_pipelineBudget->GetStats();
        printf("\n[PipelineCreditBudget] Final Stats:\n");
        printf("  Ingress:  available=%u, blocked=%llu\n", 
               stats.ingressAvailable, (unsigned long long)stats.ingressBlocked);
        printf("  Decode:   available=%u, blocked=%llu\n",
               stats.decodeAvailable, (unsigned long long)stats.decodeBlocked);
        printf("  Egress:   available=%u, blocked=%llu\n",
               stats.egressAvailable, (unsigned long long)stats.egressBlocked);
        
        delete g_pipelineBudget;
        g_pipelineBudget = nullptr;
    }
}

} // namespace FlowControl
} // namespace RawrXD
