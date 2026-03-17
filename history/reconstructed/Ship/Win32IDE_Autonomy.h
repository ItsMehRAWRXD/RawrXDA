#pragma once
// Win32IDE_Autonomy.h - Stub version without deep agentic dependencies

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include "IDELogger.h"

class AgenticBridge;

class AutonomyManager {
public:
    explicit AutonomyManager(AgenticBridge* bridge) : m_bridge(bridge) {}
    ~AutonomyManager() { stop(); }

    void start() { m_running = true; }
    void stop() { m_running = false; m_autoLoop = false; }
    bool isRunning() const { return m_running.load(); }

    void enableAutoLoop(bool enable) { m_autoLoop = enable; }
    bool isAutoLoopEnabled() const { return m_autoLoop.load(); }

    void setGoal(const std::string& goal) { 
        std::lock_guard<std::mutex> lock(m_mutex);
        m_goal = goal; 
    }
    std::string getGoal() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_goal; 
    }

    void addObservation(const std::string& obs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_memory.push_back(obs);
        if (m_memory.size() > 100) m_memory.erase(m_memory.begin());
    }
    
    std::vector<std::string> getMemorySnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_memory;
    }

    void tick() {}
    void setMaxActionsPerMinute(int v) { m_maxActionsPerMinute = v; }

    std::string getStatus() const {
        return m_running ? "Running" : "Stopped";
    }

private:
    AgenticBridge* m_bridge = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_autoLoop{false};
    mutable std::mutex m_mutex;
    std::string m_goal;
    std::vector<std::string> m_memory;
    int m_maxActionsPerMinute = 10;
};
