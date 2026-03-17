#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iostream>

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
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    ReasoningPhase currentPhase;
    IterationStatus status;
    std::string goal;
    std::string outcome;
    std::string reasoning;
    std::string error;
};

class AgenticLoopState {
public:
    AgenticLoopState();
    ~AgenticLoopState();

    void startIteration(const std::string& goal);
    void updatePhase(ReasoningPhase phase);
    void updateStatus(IterationStatus status);
    void completeIteration(const std::string& outcome);
    void failIteration(const std::string& error);

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
    std::chrono::system_clock::time_point m_stateStartTime;
    std::chrono::system_clock::time_point m_lastUpdateTime;

    std::vector<Iteration> m_iterations;
    std::map<std::string, std::string> m_memory;
};
