#pragma once


#include <memory>

class InferenceEngine;

/**
 * @brief Multi-model queue system for concurrent model management
 * 
 * Features:
 * - Priority-based scheduling (HIGH, NORMAL, LOW)
 * - Concurrent model loading (up to 2+ models)
 * - Memory-aware queue management
 * - Request throttling and backpressure
 * - Hot model swapping without blocking
 */
class ModelQueue : public void {

public:
    enum Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2
    };

    struct Request {
        int64_t id;
        std::string modelPath;
        std::string prompt;
        int maxTokens;
        float temperature;
        Priority priority;
        std::chrono::system_clock::time_point enqueueTime;
        
        bool operator<(const Request& other) const {
            if (priority != other.priority) {
                return priority > other.priority; // Higher priority first
            }
            return enqueueTime < other.enqueueTime; // FIFO for same priority
        }
    };

    explicit ModelQueue(void* parent = nullptr);
    ~ModelQueue();

    /**
     * @brief Enqueue an inference request
     * @param modelPath Path to GGUF model
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     * @param temperature Sampling temperature
     * @param priority Request priority
     * @return Request ID for tracking
     */
    int64_t enqueue(const std::string& modelPath, const std::string& prompt, 
                   int maxTokens = 256, float temperature = 0.7f,
                   Priority priority = NORMAL);

    /**
     * @brief Cancel a pending request
     */
    bool cancelRequest(int64_t requestId);

    /**
     * @brief Get queue status
     */
    int pendingRequests() const;
    int activeModels() const;
    
    /**
     * @brief Start processing queue
     */
    void start();
    
    /**
     * @brief Stop processing and clear queue
     */
    void stop();

    /**
     * @brief Set maximum concurrent models (default: 2)
     */
    void setMaxConcurrentModels(int max);

    void requestStarted(int64_t requestId);
    void requestCompleted(int64_t requestId, const std::string& result);
    void requestFailed(int64_t requestId, const std::string& error);
    void queueEmpty();
    void modelLoaded(const std::string& modelPath);
    void modelUnloaded(const std::string& modelPath);

private:
    void processQueue();
    void onInferenceComplete(int64_t reqId, const std::string& result);
    void onInferenceError(int64_t reqId, const std::string& error);

private:
    struct ModelSlot {
        std::string currentModel;
        InferenceEngine* engine = nullptr;
        bool busy = false;
        std::thread* thread = nullptr;
    };

    ModelSlot* allocateSlot(const std::string& modelPath);
    void releaseSlot(ModelSlot* slot);
    InferenceEngine* getOrLoadModel(const std::string& modelPath);

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    QQueue<Request> m_queue;
    std::unordered_map<int64_t, Request> m_activeRequests;
    std::vector<ModelSlot> m_slots;
    
    int64_t m_nextRequestId = 1;
    int m_maxConcurrentModels = 2;
    bool m_running = false;
    
    std::thread* m_processingThread = nullptr;
};


