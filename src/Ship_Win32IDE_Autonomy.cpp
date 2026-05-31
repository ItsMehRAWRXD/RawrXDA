// ============================================================================
// Ship_Win32IDE_Autonomy.cpp — Autonomy Manager Implementation
// ============================================================================
#include "Win32IDE_Autonomy.h"
#include <windows.h>
#include <sstream>
#include <chrono>

namespace RawrXD {

AutonomyManager::AutonomyManager() 
    : m_running(false)
    , m_autoLoop(false)
    , m_maxActionsPerMinute(10)
    , m_actionCount(0)
    , m_lastResetTime(std::chrono::steady_clock::now())
{
}

AutonomyManager::~AutonomyManager() {
    stop();
}

void AutonomyManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = true;
    m_autoLoop = true;
}

void AutonomyManager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
    m_autoLoop = false;
}

bool AutonomyManager::isRunning() const {
    return m_running.load();
}

void AutonomyManager::enableAutoLoop(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoLoop = enable;
}

bool AutonomyManager::isAutoLoopEnabled() const {
    return m_autoLoop.load();
}

void AutonomyManager::setGoal(const std::string& goal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_goal = goal;
}

std::string AutonomyManager::getGoal() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_goal;
}

void AutonomyManager::addObservation(const std::string& obs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memory.push_back(obs);
    if (m_memory.size() > 100) {
        m_memory.erase(m_memory.begin());
    }
}

std::vector<std::string> AutonomyManager::getMemorySnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memory;
}

void AutonomyManager::tick() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || !m_autoLoop) {
        return;
    }

    // Reset action count every minute
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_lastResetTime).count();
    if (elapsed >= 1) {
        m_actionCount = 0;
        m_lastResetTime = now;
    }

    // Check action rate limit
    if (m_actionCount >= m_maxActionsPerMinute) {
        return;
    }

    // Perform autonomous action
    if (!m_goal.empty() && !m_memory.empty()) {
        // Simple autonomous logic: process latest observation
        std::string latest = m_memory.back();
        m_memory.pop_back();
        
        // Log the action
        std::ostringstream oss;
        oss << "[Autonomy] Processing: " << latest << " for goal: " << m_goal;
        m_actionLog.push_back(oss.str());
        
        m_actionCount++;
    }
}

void AutonomyManager::setMaxActionsPerMinute(int v) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxActionsPerMinute = v;
}

std::string AutonomyManager::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return "Stopped";
    if (!m_autoLoop) return "Paused";
    return "Running (" + std::to_string(m_actionCount) + "/" + std::to_string(m_maxActionsPerMinute) + " actions/min)";
}

} // namespace RawrXD
