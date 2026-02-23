// ============================================================================
// priority_queuing.cpp — Enterprise Priority Request Queue Implementation
// ============================================================================

#include "priority_queuing.hpp"
#include <algorithm>

namespace RawrXD {

PriorityQueuingEngine::PriorityQueuingEngine() = default;

int64_t PriorityQueuingEngine::nowMs() const {
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint64_t PriorityQueuingEngine::enqueue(RequestPriority priority, const std::string& tag, void* userContext) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = nextId_++;
    QueuedRequest req{ id, priority, tag, nowMs(), userContext };
    queue_.push(Item{ req });
    cv_.notify_one();
    return id;
}

bool PriorityQueuingEngine::dequeue(QueuedRequest* out, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeoutMs <= 0) {
        cv_.wait(lock, [this] { return !queue_.empty(); });
    } else if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return !queue_.empty(); })) {
        return false;
    }
    if (queue_.empty()) return false;
    Item top = queue_.top();
    queue_.pop();
    *out = top.req;
    return true;
}

bool PriorityQueuingEngine::dispatchOne(RequestHandler handler, int timeoutMs) {
    QueuedRequest req;
    if (!dequeue(&req, timeoutMs)) return false;
    if (handler) handler(req.userContext, req);
    return true;
}

size_t PriorityQueuingEngine::depth(RequestPriority p) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Item> copy;
    auto q = queue_;
    while (!q.empty()) {
        copy.push_back(q.top());
        q.pop();
    }
    return (size_t)std::count_if(copy.begin(), copy.end(),
        [p](const Item& i) { return i.req.priority == p; });
}

size_t PriorityQueuingEngine::totalDepth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool PriorityQueuingEngine::cancel(uint64_t requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Item> kept;
    bool found = false;
    while (!queue_.empty()) {
        Item i = queue_.top();
        queue_.pop();
        if (i.req.id == requestId) found = true;
        else kept.push_back(i);
    }
    for (const Item& i : kept) queue_.push(i);
    return found;
}

} // namespace RawrXD
