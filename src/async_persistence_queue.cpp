/**
 * AsyncPersistenceQueue Implementation
 * Enhancement #4: Non-Blocking Persistence
 */

#include "async_persistence_queue.h"
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>

namespace AsyncPersistenceQueue {

    // ===== PersistenceQueue Implementation =====

    class PersistenceQueue::Impl {
    public:
        std::priority_queue<
            std::pair<int, std::shared_ptr<Operation>>,
            std::vector<std::pair<int, std::shared_ptr<Operation>>>,
            std::greater<>> queue;
        std::unordered_map<std::string, std::shared_ptr<Operation>> operations;
        
        std::vector<std::thread> workers;
        std::mutex mutex;
        std::condition_variable cv;
        std::atomic<bool> running{false};
        std::atomic<bool> paused{false};
        
        std::atomic<size_t> completedCount{0};
        std::atomic<size_t> failedCount{0};
        std::atomic<size_t> cancelledCount{0};
        
        bool batchingEnabled = false;
        int batchIntervalMs = APQ_BATCH_INTERVAL_MS;
        
        void workerLoop();
        void processOperation(std::shared_ptr<Operation> op);
    };

    void PersistenceQueue::Impl::workerLoop() {
        while (running) {
            std::shared_ptr<Operation> op;
            
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] { 
                    return !running || (!paused && !queue.empty()); 
                });
                
                if (!running) break;
                if (paused) continue;
                if (queue.empty()) continue;
                
                op = queue.top().second;
                queue.pop();
            }
            
            if (op) {
                processOperation(op);
            }
        }
    }

    void PersistenceQueue::Impl::processOperation(std::shared_ptr<Operation> op) {
        // Simulate persistence operation
        bool success = true;
        std::string error;
        
        // In production: actual file I/O here
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            operations.erase(op->operationId);
        }
        
        if (success) {
            completedCount++;
        } else {
            failedCount++;
        }
        
        if (op->callback) {
            op->callback(success, error);
        }
    }

    PersistenceQueue::PersistenceQueue() 
        : m_impl(std::make_unique<Impl>()) {
    }

    PersistenceQueue::~PersistenceQueue() {
        shutdown();
    }

    bool PersistenceQueue::initialize(size_t numWorkers) {
        if (m_impl->running) return false;
        
        m_impl->running = true;
        
        for (size_t i = 0; i < numWorkers; i++) {
            m_impl->workers.emplace_back(&Impl::workerLoop, m_impl.get());
        }
        
        return true;
    }

    void PersistenceQueue::shutdown() {
        if (!m_impl->running) return;
        
        m_impl->running = false;
        m_impl->cv.notify_all();
        
        for (auto& t : m_impl->workers) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        m_impl->workers.clear();
    }

    std::string PersistenceQueue::enqueue(Operation& op) {
        if (!m_impl->running) return "";
        
        op.operationId = "op_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        op.enqueueTime = std::chrono::steady_clock::now();
        
        auto opPtr = std::make_shared<Operation>(std::move(op));
        
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            
            if (m_impl->queue.size() >= APQ_MAX_QUEUE_DEPTH) {
                return ""; // Queue full
            }
            
            m_impl->queue.push({opPtr->priority, opPtr});
            m_impl->operations[opPtr->operationId] = opPtr;
        }
        
        m_impl->cv.notify_one();
        return opPtr->operationId;
    }

    std::string PersistenceQueue::persistExecution(
        const std::string& executionId,
        const nlohmann::json& state,
        int priority) {
        
        Operation op;
        op.type = OperationType::PersistExecution;
        op.priority = priority;
        op.executionId = executionId;
        op.payload = state;
        
        return enqueue(op);
    }

    std::string PersistenceQueue::createCheckpoint(
        const std::string& executionId,
        const std::string& label,
        const nlohmann::json& state,
        int priority) {
        
        Operation op;
        op.type = OperationType::CreateCheckpoint;
        op.priority = priority;
        op.executionId = executionId;
        op.payload["label"] = label;
        op.payload["state"] = state;
        
        return enqueue(op);
    }

    bool PersistenceQueue::waitFor(const std::string& operationId, int timeoutMs) {
        auto deadline = std::chrono::steady_clock::now() + 
            std::chrono::milliseconds(timeoutMs);
        
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                if (m_impl->operations.find(operationId) == m_impl->operations.end()) {
                    return true; // Completed or never existed
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return false;
    }

    bool PersistenceQueue::waitForAll(int timeoutMs) {
        auto deadline = std::chrono::steady_clock::now() + 
            std::chrono::milliseconds(timeoutMs);
        
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                if (m_impl->queue.empty() && m_impl->operations.empty()) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return false;
    }

    bool PersistenceQueue::cancel(const std::string& operationId) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        auto it = m_impl->operations.find(operationId);
        if (it == m_impl->operations.end()) {
            return false; // Already processed or doesn't exist
        }
        
        // Mark as cancelled (will be removed when processed)
        m_impl->cancelledCount++;
        m_impl->operations.erase(it);
        
        return true;
    }

    PersistenceQueue::Status PersistenceQueue::getStatus() const {
        Status s;
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        s.pendingCount = m_impl->queue.size();
        s.inProgressCount = m_impl->operations.size();
        s.completedCount = m_impl->completedCount.load();
        s.failedCount = m_impl->failedCount.load();
        s.cancelledCount = m_impl->cancelledCount.load();
        
        return s;
    }

    void PersistenceQueue::setBatchingEnabled(bool enabled) {
        m_impl->batchingEnabled = enabled;
    }

    void PersistenceQueue::setBatchInterval(int intervalMs) {
        m_impl->batchIntervalMs = intervalMs;
    }

    void PersistenceQueue::emergencyFlush() {
        waitForAll(30000);
    }

    void PersistenceQueue::pause() {
        m_impl->paused = true;
    }

    void PersistenceQueue::resume() {
        m_impl->paused = false;
        m_impl->cv.notify_all();
    }

    // ===== Global Queue =====

    PersistenceQueue& getGlobalQueue() {
        static PersistenceQueue instance;
        return instance;
    }

    bool initializeGlobal(size_t numWorkers) {
        return getGlobalQueue().initialize(numWorkers);
    }

    void shutdownGlobal() {
        getGlobalQueue().shutdown();
    }

} // namespace AsyncPersistenceQueue
