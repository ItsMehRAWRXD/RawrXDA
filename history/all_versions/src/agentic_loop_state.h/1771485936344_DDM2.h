#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>

using TimePoint = std::chrono::system_clock::time_point;

enum class ReasoningPhase {
    Analysis,
    Planning,
    Execution,
    Verification,
    Refinement,
    Completed
};

enum class IterationStatus {
    NotStarted,
    InProgress,
    Completed,
    Failed,
    Skipped
};

struct Iteration {
    int iterationNumber;
    TimePoint startTime;
    TimePoint endTime;
    ReasoningPhase currentPhase;
    IterationStatus status;
    std::string goal;
    std::string goalStatement;
    std::string contextSnapshot;
    float successScore = 0.0f;
    int errorCount = 0;
    std::string outcome;
    std::string reasoning;
    std::string error;
};

class AgenticLoopState {
public:
    AgenticLoopState();
    ~AgenticLoopState();

    void startIteration(const std::string& goal);
    void endIteration(const std::string& outcome);
    void updatePhase(ReasoningPhase phase);
    void updateStatus(IterationStatus status);
    void completeIteration(const std::string& outcome);
    void failIteration(const std::string& error);

    // Time helpers
    std::string timePointToISO(const TimePoint& tp) const;
    std::string timePointToHMS(const TimePoint& tp) const;

    // Memory management
    void addMemory(const std::string& key, const std::string& value);
    std::string getMemory(const std::string& key) const;
    std::map<std::string, std::string> getAllMemory() const;
    void clearMemory();

    // Getters
    ReasoningPhase getCurrentPhase() const { return m_currentPhase; }
    IterationStatus getCurrentStatus() const { return m_currentStatus; }
    int getIterationCount() const { return (int)m_iterations.size(); }
    const std::vector<Iteration>& getIterations() const { return m_iterations; }

private:
    ReasoningPhase m_currentPhase;
    IterationStatus m_currentStatus;
    TimePoint m_stateStartTime;
    TimePoint m_lastUpdateTime;
    bool m_debugMode = false;

    nlohmann::json m_constraints;
    nlohmann::json m_lastSnapshot;

    std::vector<Iteration> m_iterations;
    std::map<std::string, std::string> m_memory;
};
