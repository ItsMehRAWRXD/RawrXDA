// ============================================================================
// multi_agent_parallel.cpp — Multi-Agent Parallel Execution Engine
// ============================================================================
// Runs multiple AI agents in parallel with work distribution and result merging
// ============================================================================

#include "multi_agent_parallel.h"
#include "logging/logger.h"
#include <thread>
#include <future>
#include <algorithm>

static Logger s_logger("MultiAgent");

class MultiAgentEngine::Impl {
public:
    struct AgentWorker {
        std::thread thread;
        std::atomic<bool> busy{false};
        std::string agentId;
        int workerId;
    };
    
    std::vector<AgentWorker> m_workers;
    std::mutex m_taskMutex;
    std::queue<AgentTask> m_taskQueue;
    std::condition_variable m_taskCV;
    std::atomic<bool> m_running{true};
    int m_numAgents;
    
    Impl(int numAgents) : m_numAgents(numAgents) {
        m_workers.resize(numAgents);
        
        for (int i = 0; i < numAgents; ++i) {
            m_workers[i].workerId = i;
            m_workers[i].agentId = "agent_" + std::to_string(i);
            m_workers[i].thread = std::thread([this, i]() { workerLoop(i); });
        }
        
        s_logger.info("Multi-agent engine initialized with {} parallel agents", numAgents);
    }
    
    ~Impl() {
        m_running = false;
        m_taskCV.notify_all();
        
        for (auto& worker : m_workers) {
            if (worker.thread.joinable()) {
                worker.thread.join();
            }
        }
        
        s_logger.info("Multi-agent engine shutdown complete");
    }
    
    void workerLoop(int workerId) {
        s_logger.debug("Agent worker {} started", workerId);
        
        while (m_running) {
            AgentTask task;
            
            {
                std::unique_lock<std::mutex> lock(m_taskMutex);
                m_taskCV.wait(lock, [this]() {
                    return !m_taskQueue.empty() || !m_running;
                });
                
                if (!m_running) break;
                if (m_taskQueue.empty()) continue;
                
                task = m_taskQueue.front();
                m_taskQueue.pop();
            }
            
            m_workers[workerId].busy = true;
            
            // Execute task
            s_logger.debug("Agent {} executing task: {}", workerId, task.taskId);
            
            AgentResult result;
            result.taskId = task.taskId;
            result.agentId = m_workers[workerId].agentId;
            result.success = true;
            
            try {
                if (task.executor) {
                    result.output = task.executor(task.input);
                } else {
                    result.output = "Task completed by agent " + std::to_string(workerId);
                }
            } catch (const std::exception& e) {
                result.success = false;
                result.error = e.what();
                s_logger.error("Agent {} task failed: {}", workerId, e.what());
            }
            
            if (task.callback) {
                task.callback(result);
            }
            
            m_workers[workerId].busy = false;
            s_logger.debug("Agent {} task complete", workerId);
        }
        
        s_logger.debug("Agent worker {} stopped", workerId);
    }
    
    void submitTask(const AgentTask& task) {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
        m_taskCV.notify_one();
        s_logger.debug("Task queued: {}", task.taskId);
    }
    
    std::vector<AgentResult> executeParallel(const std::vector<AgentTask>& tasks) {
        std::vector<std::future<AgentResult>> futures;
        std::vector<AgentResult> results;
        
        // Submit all tasks
        for (const auto& task : tasks) {
            auto promise = std::make_shared<std::promise<AgentResult>>();
            futures.push_back(promise->get_future());
            
            AgentTask taskCopy = task;
            taskCopy.callback = [promise](const AgentResult& result) {
                promise->set_value(result);
            };
            
            submitTask(taskCopy);
        }
        
        // Wait for all results
        for (auto& future : futures) {
            results.push_back(future.get());
        }
        
        s_logger.info("Parallel execution complete: {} tasks", tasks.size());
        return results;
    }
    
    int getActiveAgents() const {
        int count = 0;
        for (const auto& worker : m_workers) {
            if (worker.busy.load()) count++;
        }
        return count;
    }
};

// ============================================================================
// Public API
// ============================================================================

MultiAgentEngine::MultiAgentEngine(int numAgents) 
    : m_impl(new Impl(numAgents)) {}

MultiAgentEngine::~MultiAgentEngine() {
    delete m_impl;
}

void MultiAgentEngine::submitTask(const AgentTask& task) {
    m_impl->submitTask(task);
}

std::vector<AgentResult> MultiAgentEngine::executeParallel(
    const std::vector<AgentTask>& tasks) {
    return m_impl->executeParallel(tasks);
}

AgentResult MultiAgentEngine::executeSingle(const AgentTask& task) {
    auto results = executeParallel({task});
    return results.empty() ? AgentResult{} : results[0];
}

int MultiAgentEngine::getActiveAgents() const {
    return m_impl->getActiveAgents();
}

int MultiAgentEngine::getTotalAgents() const {
    return m_impl->m_numAgents;
}
