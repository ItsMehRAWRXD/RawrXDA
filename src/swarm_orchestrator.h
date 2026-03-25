#pragma once
#include <vector>
#include <string>
#include <future>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
// Check if compiler supports <expected>, if not rely on utils/Expected not matching std::expected
// But .cpp uses std::expected explicitly.
#if __has_include(<expected>)
#include <expected>
#else
// Fallback if needed, but MinGW 15 should have it
#endif
#include <nlohmann/json.hpp>
#include "CommonTypes.h"
#include "utils/Expected.h"

namespace RawrXD {

enum class SwarmError {
    Success = 0,
    InitializationFailed,
    TaskSubmissionFailed,
    ConsensusFailed,
    Timeout,
    InternalError
};

struct SwarmResult {
    std::string consensus;
    float confidence;
    std::vector<std::string> individualThoughts;
    double executionTimeMs;
};

struct OrchestratorTask {
    std::string id;
    std::string description;
    std::string context;
    int priority;
    std::promise<SwarmResult> promise;
};

struct WorkerQueue {
    std::mutex mutex;
    std::deque<std::unique_ptr<OrchestratorTask>> tasks;
};

class SwarmOrchestrator {
public:
    SwarmOrchestrator(size_t numWorkers = 0);
    ~SwarmOrchestrator();

    void shutdown();

    // Matching .cpp implementation
<<<<<<< HEAD
#if defined(__cpp_lib_expected) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202202L)
    std::expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context);
#else
    RawrXD::Expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context);
#endif
=======
    std::expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context);
>>>>>>> origin/main
    std::future<SwarmResult> submitTaskAsync(const std::string& taskDesc, const std::string& context); // .cpp name
    // Also provide alias for submitTask which might be used by others
    std::future<SwarmResult> submitTask(const std::string& desc, const std::string& ctx) {
        return submitTaskAsync(desc, ctx);
    }
    
    // Public for thread entry point or make private + friend
    void loop(int workerId);

    bool stealWork(int thiefId, OrchestratorTask& stolenTask);
    SwarmResult synthesizeConsensus(const std::vector<std::string>& results);
    
    nlohmann::json getStatus() const; 

    // Legacy initialize stub
    RawrXD::Expected<void, SwarmError> initialize() { return {}; }

private:
   std::vector<std::unique_ptr<WorkerQueue>> m_queues;
   std::vector<std::thread> m_threads;
   std::atomic<bool> m_running{false};
   std::atomic<size_t> m_totalTasksExecuted{0};
};

} // namespace RawrXD
