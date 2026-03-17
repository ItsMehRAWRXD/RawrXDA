#include "agentic_loop_state.h"
#include <iostream>

AgenticLoopState::AgenticLoopState()
    : m_currentPhase(ReasoningPhase::Analysis)
    , m_currentStatus(IterationStatus::NotStarted)
    , m_stateStartTime(std::chrono::system_clock::now())
    , m_lastUpdateTime(std::chrono::system_clock::now())
{
    std::cout << "[AgenticLoopState] Initialized - Ready" << std::endl;
}

AgenticLoopState::~AgenticLoopState()
{
}

void AgenticLoopState::startIteration(const std::string& goal)
{
    Iteration iteration;
    iteration.iterationNumber = (int)m_iterations.size() + 1;
    iteration.startTime = std::chrono::system_clock::now();
    iteration.currentPhase = ReasoningPhase::Analysis;
    iteration.status = IterationStatus::InProgress;
    iteration.goal = goal;
    
    m_iterations.push_back(iteration);
    m_currentStatus = IterationStatus::InProgress;
    m_lastUpdateTime = std::chrono::system_clock::now();
}

void AgenticLoopState::updatePhase(ReasoningPhase phase)
{
    m_currentPhase = phase;
    if (!m_iterations.empty()) {
        m_iterations.back().currentPhase = phase;
    }
    m_lastUpdateTime = std::chrono::system_clock::now();
}

void AgenticLoopState::updateStatus(IterationStatus status)
{
    m_currentStatus = status;
    if (!m_iterations.empty()) {
        m_iterations.back().status = status;
    }
    m_lastUpdateTime = std::chrono::system_clock::now();
}

void AgenticLoopState::completeIteration(const std::string& outcome)
{
    if (!m_iterations.empty()) {
        auto& it = m_iterations.back();
        it.status = IterationStatus::Completed;
        it.outcome = outcome;
        it.endTime = std::chrono::system_clock::now();
    }
    m_currentStatus = IterationStatus::Completed;
    m_lastUpdateTime = std::chrono::system_clock::now();
}

void AgenticLoopState::failIteration(const std::string& error)
{
    if (!m_iterations.empty()) {
        auto& it = m_iterations.back();
        it.status = IterationStatus::Failed;
        it.error = error;
        it.endTime = std::chrono::system_clock::now();
    }
    m_currentStatus = IterationStatus::Failed;
    m_lastUpdateTime = std::chrono::system_clock::now();
}

void AgenticLoopState::addMemory(const std::string& key, const std::string& value)
{
    m_memory[key] = value;
}

std::string AgenticLoopState::getMemory(const std::string& key) const
{
    auto it = m_memory.find(key);
    if (it != m_memory.end()) {
        return it->second;
    }
    return "";
}

std::map<std::string, std::string> AgenticLoopState::getAllMemory() const
{
    return m_memory;
}

void AgenticLoopState::clearMemory()
{
    m_memory.clear();
}
