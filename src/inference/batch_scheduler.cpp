/**
 * @file batch_scheduler.cpp
 * @brief Dynamic batching for multi-request inference
 * 
 * Implements continuous batching with:
 * - Dynamic request scheduling
 * - KV cache slot management
 * - Preemption and priority handling
 * - Token budget allocation
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "batch_scheduler.h"
#include <algorithm>
#include <chrono>
#include <math>

namespace RawrXD::Inference {

// ============================================================================
// BatchScheduler Implementation
// ============================================================================

BatchScheduler::BatchScheduler(const Config& config)
    : m_config(config)
    , m_nextRequestId(1)
    , m_totalTokensProcessed(0)
    , m_totalRequestsCompleted(0)
{
    // Initialize KV cache slots
    m_slots.resize(config.maxBatchSize);
    for (size_t i = 0; i < m_slots.size(); ++i) {
        m_slots[i].slotId = static_cast<int32_t>(i);
        m_slots[i].isFree = true;
    }
}

BatchScheduler::~BatchScheduler() = default;

// ============================================================================
// Request Submission
// ============================================================================

int64_t BatchScheduler::submitRequest(const Request& request) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int64_t requestId = m_nextRequestId++;
    
    QueuedRequest queued;
    queued.requestId = requestId;
    queued.request = request;
    queued.submitTime = std::chrono::steady_clock::now();
    queued.priority = request.priority;
    queued.status = RequestStatus::Pending;
    
    // Add to pending queue
    m_pendingQueue.push_back(queued);
    
    // Sort by priority (higher priority first)
    std::sort(m_pendingQueue.begin(), m_pendingQueue.end(),
              [](const QueuedRequest& a, const QueuedRequest& b) {
                  return a.priority > b.priority;
              });
    
    return requestId;
}

bool BatchScheduler::cancelRequest(int64_t requestId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check pending queue
    auto it = std::find_if(m_pendingQueue.begin(), m_pendingQueue.end(),
                          [requestId](const QueuedRequest& r) {
                              return r.requestId == requestId;
                          });
    
    if (it != m_pendingQueue.end()) {
        m_pendingQueue.erase(it);
        return true;
    }
    
    // Check active requests
    auto activeIt = m_activeRequests.find(requestId);
    if (activeIt != m_activeRequests.end()) {
        activeIt->second.status = RequestStatus::Cancelled;
        return true;
    }
    
    return false;
}

// ============================================================================
// Batch Formation
// ============================================================================

Batch BatchScheduler::formBatch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Batch batch;
    batch.batchId = m_nextRequestId++; // Reuse as batch ID
    
    // Calculate available tokens for this batch
    int availableTokens = m_config.maxTokensPerBatch;
    
    // First, add continuing requests (already have KV cache slots)
    for (auto& [requestId, activeReq] : m_activeRequests) {
        if (activeReq.status != RequestStatus::Processing) continue;
        
        auto slotIt = std::find_if(m_slots.begin(), m_slots.end(),
                                  [requestId](const KVSlot& slot) {
                                      return slot.requestId == requestId && !slot.isFree;
                                  });
        
        if (slotIt != m_slots.end()) {
            int tokensNeeded = 1; // One new token per step
            if (availableTokens >= tokensNeeded) {
                BatchItem item;
                item.requestId = requestId;
                item.slotId = slotIt->slotId;
                item.tokens = activeReq.request.tokens;
                item.isContinuation = true;
                item.priority = activeReq.priority;
                
                batch.items.push_back(item);
                availableTokens -= tokensNeeded;
            }
        }
    }
    
    // Then, add new requests from pending queue
    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end();) {
        if (availableTokens <= 0) break;
        
        // Find a free slot
        auto slotIt = std::find_if(m_slots.begin(), m_slots.end(),
                                  [](const KVSlot& slot) { return slot.isFree; });
        
        if (slotIt == m_slots.end()) break; // No free slots
        
        int tokensNeeded = static_cast<int>(it->request.tokens.size());
        if (tokensNeeded > availableTokens) {
            // Request too large, skip for now
            ++it;
            continue;
        }
        
        // Allocate slot
        slotIt->isFree = false;
        slotIt->requestId = it->requestId;
        slotIt->tokensUsed = tokensNeeded;
        
        BatchItem item;
        item.requestId = it->requestId;
        item.slotId = slotIt->slotId;
        item.tokens = it->request.tokens;
        item.isContinuation = false;
        item.priority = it->priority;
        
        batch.items.push_back(item);
        availableTokens -= tokensNeeded;
        
        // Move to active
        ActiveRequest active;
        active.requestId = it->requestId;
        active.request = it->request;
        active.priority = it->priority;
        active.status = RequestStatus::Processing;
        active.startTime = std::chrono::steady_clock::now();
        active.tokensGenerated = 0;
        
        m_activeRequests[it->requestId] = active;
        it = m_pendingQueue.erase(it);
    }
    
    batch.totalTokens = m_config.maxTokensPerBatch - availableTokens;
    return batch;
}

// ============================================================================
// Batch Completion
// ============================================================================

void BatchScheduler::completeBatch(const Batch& batch,
                                  const std::vector<uint32_t>& generatedTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (generatedTokens.size() != batch.items.size()) {
        return; // Mismatch
    }
    
    for (size_t i = 0; i < batch.items.size(); ++i) {
        int64_t requestId = batch.items[i].requestId;
        uint32_t token = generatedTokens[i];
        
        auto it = m_activeRequests.find(requestId);
        if (it == m_activeRequests.end()) continue;
        
        auto& active = it->second;
        active.tokensGenerated++;
        active.generatedTokens.push_back(token);
        
        // Update slot
        auto slotIt = std::find_if(m_slots.begin(), m_slots.end(),
                                  [requestId](const KVSlot& slot) {
                                      return slot.requestId == requestId && !slot.isFree;
                                  });
        
        if (slotIt != m_slots.end()) {
            slotIt->tokensUsed++;
            
            // Check if request is complete
            if (active.tokensGenerated >= active.request.maxTokens ||
                token == active.request.eosToken) {
                // Request complete
                active.status = RequestStatus::Completed;
                active.completionTime = std::chrono::steady_clock::now();
                
                // Free slot
                slotIt->isFree = true;
                slotIt->requestId = -1;
                slotIt->tokensUsed = 0;
                
                m_totalRequestsCompleted++;
            }
        }
        
        m_totalTokensProcessed++;
    }
    
    // Clean up completed requests
    cleanupCompletedRequests();
}

// ============================================================================
// Preemption
// ============================================================================

bool BatchScheduler::preemptLowPriorityRequest(int minPriority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_config.allowPreemption) return false;
    
    // Find lowest priority active request
    int64_t victimId = -1;
    int lowestPriority = std::numeric_limits<int>::max();
    
    for (const auto& [requestId, active] : m_activeRequests) {
        if (active.status == RequestStatus::Processing &&
            active.priority < minPriority &&
            active.priority < lowestPriority) {
            lowestPriority = active.priority;
            victimId = requestId;
        }
    }
    
    if (victimId < 0) return false;
    
    // Preempt the victim
    auto it = m_activeRequests.find(victimId);
    if (it != m_activeRequests.end()) {
        it->second.status = RequestStatus::Preempted;
        
        // Free its slot
        auto slotIt = std::find_if(m_slots.begin(), m_slots.end(),
                                  [victimId](const KVSlot& slot) {
                                      return slot.requestId == victimId && !slot.isFree;
                                  });
        
        if (slotIt != m_slots.end()) {
            slotIt->isFree = true;
            slotIt->requestId = -1;
            slotIt->tokensUsed = 0;
        }
        
        // Move back to pending
        QueuedRequest queued;
        queued.requestId = victimId;
        queued.request = it->second.request;
        queued.priority = it->second.priority;
        queued.submitTime = std::chrono::steady_clock::now();
        queued.status = RequestStatus::Pending;
        m_pendingQueue.push_back(queued);
        
        m_activeRequests.erase(it);
        return true;
    }
    
    return false;
}

// ============================================================================
// Status Queries
// ============================================================================

RequestStatus BatchScheduler::getRequestStatus(int64_t requestId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_activeRequests.find(requestId);
    if (it != m_activeRequests.end()) {
        return it->second.status;
    }
    
    // Check pending
    auto pendingIt = std::find_if(m_pendingQueue.begin(), m_pendingQueue.end(),
                                 [requestId](const QueuedRequest& r) {
                                     return r.requestId == requestId;
                                 });
    
    if (pendingIt != m_pendingQueue.end()) {
        return RequestStatus::Pending;
    }
    
    return RequestStatus::Unknown;
}

std::vector<uint32_t> BatchScheduler::getGeneratedTokens(int64_t requestId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_activeRequests.find(requestId);
    if (it != m_activeRequests.end()) {
        return it->second.generatedTokens;
    }
    
    return {};
}

SchedulerStats BatchScheduler::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    SchedulerStats stats;
    stats.pendingRequests = static_cast<int>(m_pendingQueue.size());
    stats.activeRequests = static_cast<int>(m_activeRequests.size());
    stats.totalTokensProcessed = m_totalTokensProcessed;
    stats.totalRequestsCompleted = m_totalRequestsCompleted;
    
    // Count free slots
    stats.freeSlots = 0;
    for (const auto& slot : m_slots) {
        if (slot.isFree) stats.freeSlots++;
    }
    stats.totalSlots = static_cast<int>(m_slots.size());
    
    return stats;
}

// ============================================================================
// Cleanup
// ============================================================================

void BatchScheduler::cleanupCompletedRequests() {
    auto it = m_activeRequests.begin();
    while (it != m_activeRequests.end()) {
        if (it->second.status == RequestStatus::Completed ||
            it->second.status == RequestStatus::Cancelled) {
            it = m_activeRequests.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace RawrXD::Inference
