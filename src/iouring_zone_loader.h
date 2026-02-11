// src/iouring_zone_loader.h
#pragma once
#include <windows.h>
#include <ioringapi.h>  // Windows 11 SDK
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace RawrXD::IO {

struct IORingConfig {
    uint32_t queueDepth = 64;
    uint32_t maxBatch = 32;
    bool useRegisteredBuffers = true;  // For RDMA-like zero-copy
};

struct ZoneRequest {
    uint64_t fileOffset;
    void* buffer;
    uint32_t size;
};

struct CompletionResult {
    void* userContext;
    HRESULT result;
    uint32_t bytesTransferred;
    double latencyUs;
};

class IORingZoneLoader {
public:
    explicit IORingZoneLoader(const IORingConfig& config = {});
    ~IORingZoneLoader();
    
    // Initialize with file handle
    bool Initialize(HANDLE hFile);
    
    // Submit single zone read (async)
    bool SubmitZoneRead(uint64_t fileOffset, 
                        void* buffer, 
                        uint32_t size,
                        void* userContext);
    
    // Submit batch (key for 5ms target)
    bool SubmitZoneBatch(const ZoneRequest requests[], 
                         uint32_t count);
    
    // Poll completions (non-blocking)
    uint32_t PollCompletions(uint32_t maxCompletions,
                             CompletionResult results[]);
    
    // Wait for specific completion (blocking)
    bool WaitForCompletion(void* userContext, 
                           uint32_t timeoutMs);
    
    // Registered buffers for true zero-copy
    bool RegisterBufferPool(void* buffers[], 
                            uint32_t bufferSize,
                            uint32_t count);
    
    // Performance metrics
    struct Stats {
        uint64_t submittedOps;
        uint64_t completedOps;
        uint64_t batchSubmissions;
        double avgLatencyUs;
        double p99LatencyUs;
    };
    Stats GetStats() const;

private:
    HIORING hRing_ = nullptr;
    HANDLE hFile_ = nullptr;
    HANDLE hCompletionEvent_ = nullptr;
    
    // Registered buffer pool
    struct BufferRef {
        IORING_BUFFER_REF ref;
        void* ptr;
    };
    std::vector<BufferRef> registeredBuffers_;
    std::vector<void*> bufferPool_;
    
    // Statistics
    mutable std::atomic<uint64_t> submittedOps_{0};
    mutable std::atomic<uint64_t> completedOps_{0};
    
    // Completion tracking
    struct PendingOp {
        void* userContext;
        uint64_t submitTime;
        uint32_t size;
    };
    std::unordered_map<uint64_t, PendingOp> pendingOps_; // Use uint64_t for UserData
    mutable std::mutex pendingMutex_;
};

} // namespace RawrXD::IO
