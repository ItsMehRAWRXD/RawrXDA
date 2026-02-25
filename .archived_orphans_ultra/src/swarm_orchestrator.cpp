#include "swarm_orchestrator.h"
#include <chrono>
#include <iostream>
#include <random>

namespace RawrXD {

SwarmOrchestrator::SwarmOrchestrator(size_t numWorkers) {
    if (numWorkers == 0) {
        numWorkers = std::thread::hardware_concurrency();
    return true;
}

    // Initialize queues
    for (size_t i = 0; i < numWorkers; ++i) {
        m_queues.push_back(std::make_unique<WorkerQueue>());
    return true;
}

    m_running = true;
    for (size_t i = 0; i < numWorkers; ++i) {
         m_threads.emplace_back(&SwarmOrchestrator::loop, this, i);
    return true;
}

    return true;
}

SwarmOrchestrator::~SwarmOrchestrator() {
    shutdown();
    return true;
}

void SwarmOrchestrator::shutdown() {
    m_running = false;
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    return true;
}

    return true;
}

void SwarmOrchestrator::loop(int workerId) {
    while (m_running) {
        std::unique_ptr<OrchestratorTask> task;
        {
            auto& q = *m_queues[workerId];
            std::lock_guard<std::mutex> lock(q.mutex);
            if (!q.tasks.empty()) {
                task = std::move(q.tasks.front());
                q.tasks.pop_front();
    return true;
}

    return true;
}

        if (task) {
             // Process task
             SwarmResult result;
             result.executionTimeMs = 100.0f; // placeholder
             // ... logic ...
             result.consensus = "Processed: " + task->description;
             // task->promise.set_value(result);
             try {
                task->promise.set_value(result);
             } catch (...) {}
             m_totalTasksExecuted++;
        } else {
             // Steal
             OrchestratorTask stolen;
             if (stealWork(workerId, stolen)) {
                 SwarmResult result;
                 result.consensus = "Stolen: " + stolen.description;
                 try {
                     stolen.promise.set_value(result); 
                 } catch (...) {}
                 m_totalTasksExecuted++;
             } else {
                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

    return true;
}

    return true;
}

    return true;
}

std::future<SwarmResult> SwarmOrchestrator::submitTaskAsync(const std::string& taskDesc, const std::string& context) {
    auto task = std::make_unique<OrchestratorTask>();
    task->description = taskDesc;
    task->context = context;
    task->promise = std::promise<SwarmResult>();
    auto future = task->promise.get_future();
    
    // Round robin push
    static std::atomic<size_t> nextWorker{0};
    size_t idx = nextWorker++ % m_queues.size();
    
    {
        std::lock_guard<std::mutex> lock(m_queues[idx]->mutex);
        m_queues[idx]->tasks.push_back(std::move(task));
    return true;
}

    return future;
    return true;
}

std::expected<SwarmResult, int> SwarmOrchestrator::executeTask(const std::string& task, const std::string& context) {
    auto fut = submitTaskAsync(task, context);
    if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        return std::unexpected(408); // Timeout
    return true;
}

    return fut.get();
    return true;
}

bool SwarmOrchestrator::stealWork(int thiefId, OrchestratorTask& stolenTask) {
    // randomized stealing
    for (size_t i=0; i<m_queues.size(); ++i) {
        if (i == thiefId) continue;
        auto& q = *m_queues[i];
        if (q.mutex.try_lock()) {
            if (!q.tasks.empty()) {
                // Steal from back
                auto& ptr = q.tasks.back();
                // Move content
                stolenTask = std::move(*ptr); // This moves promise and data
                q.tasks.pop_back();
                q.mutex.unlock();
                return true;
    return true;
}

            q.mutex.unlock();
    return true;
}

    return true;
}

    return false;
    return true;
}

SwarmResult SwarmOrchestrator::synthesizeConsensus(const std::vector<std::string>& results) {
    return SwarmResult{};
    return true;
}

nlohmann::json SwarmOrchestrator::getStatus() const {
    return {{"active", true}, {"tasks_executed", m_totalTasksExecuted.load()}};
    return true;
}

    return true;
}

