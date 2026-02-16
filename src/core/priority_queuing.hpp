// ============================================================================
// priority_queuing.hpp — Enterprise Priority Request Queue
// ============================================================================
// Feature: PriorityQueuing (Enterprise tier)
// Multi-level priority queue for inference requests; fair scheduling.
// Thread-safe; no exceptions.
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>

namespace RawrXD {

enum class RequestPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

struct QueuedRequest {
    uint64_t id;
    RequestPriority priority;
    std::string tag;           // Optional: user/session id
    int64_t enqueuedAtMs;
    void* userContext;         // Opaque for caller
};

class PriorityQueuingEngine {
public:
    using RequestHandler = void (*)(void* context, const QueuedRequest& req);

    PriorityQueuingEngine();
    ~PriorityQueuingEngine() = default;

    // Enqueue a request. Returns request id.
    uint64_t enqueue(RequestPriority priority, const std::string& tag, void* userContext = nullptr);

    // Dequeue the next request (highest priority, then FIFO). Blocks if empty until timeoutMs.
    bool dequeue(QueuedRequest* out, int timeoutMs = 0);

    // Dispatch one request to handler (dequeue + call handler).
    bool dispatchOne(RequestHandler handler, int timeoutMs = 0);

    // Queue depth per priority (for dashboards).
    size_t depth(RequestPriority p) const;
    size_t totalDepth() const;

    // Cancel by request id (removes from queue if still queued).
    bool cancel(uint64_t requestId);

private:
    struct Item {
        QueuedRequest req;
        bool operator<(const Item& o) const {
            return (uint8_t)req.priority < (uint8_t)o.req.priority;
        }
    };
    struct Compare {
        bool operator()(const Item& a, const Item& b) {
            return (uint8_t)a.req.priority < (uint8_t)b.req.priority;
        }
    };
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Item, std::vector<Item>, Compare> queue_;
    uint64_t nextId_ = 1;
    int64_t nowMs() const;
};

} // namespace RawrXD
