/**
 * @file batch_scheduler.h
 * @brief Dynamic batching for multi-request inference
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <memory>

namespace RawrXD::Inference {

// ============================================================================
// Request Status
// ============================================================================

enum class RequestStatus {
    Unknown,
    Pending,
    Processing,
    Completed,
    Cancelled,
    Preempted,
    Failed
};

// ============================================================================
// Request Configuration
// ============================================================================

struct Request {
    std::vector<uint32_t> tokens;
    int maxTokens = 256;
    int priority = 0;
    uint32_t eosToken = 0;
    float temperature = 0.8f;
    int topK = 40;
    float topP = 0.95f;
};

// ============================================================================
// Batch Scheduler Configuration
// ============================================================================

struct BatchSchedulerConfig {
    int maxBatchSize = 16;
    int maxTokensPerBatch = 4096;
    int maxQueueDepth = 64;
    bool allowPreemption = true;
    float preemptionThreshold = 0.3f;
    int schedulingIntervalMs = 10;
};

// ============================================================================
// KV Cache Slot
// ============================================================================

struct KVSlot {
    int32_t slotId = -1;
    int64_t requestId = -1;
    int tokensUsed = 0;
    bool isFree = true;
};

// ============================================================================
// Batch Item
// ============================================================================

struct BatchItem {
    int64_t requestId = -1;
    int32_t slotId = -1;
    std::vector<uint32_t> tokens;
    bool isContinuation = false;
    int priority = 0;
};

// ============================================================================
// Batch
// ============================================================================

struct Batch {
    int64_t batchId = -1;
    std::vector<BatchItem> items;
    int totalTokens = 0;
};

// ============================================================================
// Scheduler Statistics
// ============================================================================

struct SchedulerStats {
    int pendingRequests = 0;
    int activeRequests = 0;
    int freeSlots = 0;
    int totalSlots = 0;
    uint64_t totalTokensProcessed = 0;
    uint64_t totalRequestsCompleted = 0;
};

// ============================================================================
// Internal Structures
// ============================================================================

struct QueuedRequest {
    int64_t requestId = -1;
    Request request;
    std::chrono::steady_clock::time_point submitTime;
    int priority = 0;
    RequestStatus status = RequestStatus::Pending;
};

struct ActiveRequest {
    int64_t requestId = -1;
    Request request;
    int priority = 0;
    RequestStatus status = RequestStatus::Processing;
    std::chrono::steady_clock::time_point startTime;
    int tokensGenerated = 0;
    std::vector<uint32_t> generatedTokens;
    std::chrono::steady_clock::time_point completionTime;
};

// ============================================================================
// Batch Scheduler
// ============================================================================

class BatchScheduler {
public:
    explicit BatchScheduler(const Config& config);
    ~BatchScheduler();
    
    // Request management
    int64_t submitRequest(const Request& request);
    bool cancelRequest(int64_t requestId);
    
    // Batch formation
    Batch formBatch();
    void completeBatch(const Batch& batch,
                      const std::vector<uint32_t>& generatedTokens);
    
    // Preemption
    bool preemptLowPriorityRequest(int minPriority);
    
    // Status queries
    RequestStatus getRequestStatus(int64_t requestId) const;
    std::vector<uint32_t> getGeneratedTokens(int64_t requestId) const;
    SchedulerStats getStats() const;
    
    // Configuration
    void setConfig(const Config& config) { m_config = config; }
    const Config& getConfig() const { return m_config; }
    
private:
    void cleanupCompletedRequests();
    
    Config m_config;
    mutable std::mutex m_mutex;
    
    int64_t m_nextRequestId;
    std::vector<KVSlot> m_slots;
    std::vector<QueuedRequest> m_pendingQueue;
    std::map<int64_t, ActiveRequest> m_activeRequests;
    
    uint64_t m_totalTokensProcessed;
    uint64_t m_totalRequestsCompleted;
};

} // namespace RawrXD::Inference
