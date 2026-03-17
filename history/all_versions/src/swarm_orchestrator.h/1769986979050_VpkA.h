#pragma once
#include <string>
#include <vector>
#include <expected>
#include <nlohmann/json.hpp>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <optional>

namespace RawrXD {

struct SwarmResult {
    std::string consensus;
    float confidence;
    std::vector<std::string> individualThoughts;
    double executionTimeMs;
};

struct SwarmTask {
    std::string id;
    std::string description;
    std::string context;
    int priority;
    std::promise<SwarmResult> promise;
};

class SwarmOrchestrator {
public:
    SwarmOrchestrator(size_t numWorkers = std::thread::hardware_concurrency());
    virtual ~SwarmOrchestrator();

    // Prevent copying
    SwarmOrchestrator(const SwarmOrchestrator&) = delete;
    SwarmOrchestrator& operator=(const SwarmOrchestrator&) = delete;

    std::expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context = "");
    
    // Async submission
    std::future<SwarmResult> submitTaskAsync(const std::string& task, const std::string& context = "");

    void shutdown();
    
    nlohmann::json getStatus() const;
    
private:
    void loop(int workerId);
    bool stealWork(int thiefId, SwarmTask& stolenTask);
    
    // Work Stealing Queues: 1 per worker
    struct WorkerQueue {
        std::deque<std::unique_ptr<SwarmTask>> tasks;
        std::mutex mutex;
    };
    
    std::vector<std::unique_ptr<WorkerQueue>> m_queues;
    std::vector<std::thread> m_threads;
    
    std::atomic<bool> m_running{true};
    std::atomic<size_t> m_totalTasksExecuted{0};
    
    // Consensus Logic
    SwarmResult synthesizeConsensus(const std::vector<std::string>& results);
};

} // namespace RawrXD
