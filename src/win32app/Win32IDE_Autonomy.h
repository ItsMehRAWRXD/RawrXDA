#pragma once

#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>
#include "IDELogger.h"
#include "Win32IDE_AgenticBridge.h"

// AutonomyManager: high-level autonomous orchestration layer.
// Responsibilities:
//  - Maintain goal & working memory
//  - Plan next action (goal + memory; prompt-driven tool loop)
//  - Rate limit actions (max actions per minute)
//  - Execute actions via AgenticBridge (tool / prompt)
//  - Background loop thread when auto loop enabled
class AutonomyManager {
public:
    explicit AutonomyManager(AgenticBridge* bridge);
    ~AutonomyManager();

    void start();            // start manual autonomy (no loop)
    void stop();             // stop loop & flush state
    bool isRunning() const { return m_running.load(); }

    // Compatibility wrappers (older call sites / command handlers).
    void Start() { start(); }
    void Stop() { stop(); }
    bool IsRunning() const { return isRunning(); }

    void enableAutoLoop(bool enable); // toggle background loop
    bool isAutoLoopEnabled() const { return m_autoLoop.load(); }

    void setGoal(const std::string& goal);
    std::string getGoal() const;

    void SetGoal(const std::string& goal) { setGoal(goal); }
    std::string GetGoal() const { return getGoal(); }

    void addObservation(const std::string& obs); // append memory item
    std::vector<std::string> getMemorySnapshot();
    std::vector<std::string> GetMemorySnapshot() { return getMemorySnapshot(); }

    void tick();              // single planning + execution step
    void setMaxActionsPerMinute(int v) { m_maxActionsPerMinute = v; }

    // Output callback: surfaces bridge failures and action events to the calling UI.
    void setOutputCallback(std::function<void(const std::string&)> cb) { m_onOutput = std::move(cb); }

    // Hook for external status surface
    std::string getStatus() const;
    std::string GetStatus() const { return getStatus(); }

    // Simple export surface used by UI commands.
    std::string ExportMemory() {
        std::ostringstream oss;
        auto snap = getMemorySnapshot();
        oss << "goal: " << getGoal() << "\n";
        oss << "items: " << snap.size() << "\n";
        for (const auto& s : snap) oss << "- " << s << "\n";
        return oss.str();
    }

private:
    void loop();
    std::string planNextAction();
    void executeAction(const std::string& action);
    bool rateLimitAllow();

    AgenticBridge* m_bridge;
    std::function<void(const std::string&)> m_onOutput;  // optional UI output path
    std::atomic<bool> m_running;
    std::atomic<bool> m_autoLoop;

    std::string m_goal;
    std::vector<std::string> m_memory;

    std::thread m_loopThread;
    mutable std::mutex m_mutex;

    // Rate limiting
    int m_maxActionsPerMinute;
    int m_actionsThisWindow;
    std::chrono::steady_clock::time_point m_windowStart;
    std::deque<std::chrono::steady_clock::time_point> m_actionTimes;

    // Health / backoff state
    int m_consecutiveErrors = 0;
    std::chrono::steady_clock::time_point m_nextAllowedAction;
    std::string m_lastAction;
    std::string m_lastError;
    std::chrono::steady_clock::time_point m_lastActionAt;
};
