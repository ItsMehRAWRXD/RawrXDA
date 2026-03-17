#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

class AgenticBridge;

class AutonomyManager {
public:
    AutonomyManager(AgenticBridge* bridge);
    ~AutonomyManager();
    void start();
    void stop();
    void enableAutoLoop(bool enable);
    void setGoal(const std::string& goal);
    std::string getGoal() const;
    bool isRunning() const;
    bool isAutoLoopEnabled() const;
    void setMaxActionsPerMinute(int max);
    int getMaxActionsPerMinute() const;
    void addObservation(const std::string& obs);
    std::vector<std::string> getMemorySnapshot();
    std::string getStatus() const;
    void tick();

private:
    void loop();
    std::string planNextAction();
    void executeAction(const std::string& action);
    bool rateLimitAllow();

    AgenticBridge* m_bridge;
    std::string m_goal;
    std::thread m_loopThread;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_running;
    std::atomic<bool> m_autoLoop;
    int m_maxActionsPerMinute;
    int m_actionsThisWindow;
    std::chrono::steady_clock::time_point m_windowStart;
    std::vector<std::string> m_memory;
};