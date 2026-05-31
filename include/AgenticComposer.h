#pragma once
#include <string>
#include <vector>
#include <future>
#include "MultiFileRewriteEngine.h"

namespace RawrXD {
namespace IDE {

enum class ComposerState {
    Idle,
    Planning,
    ReviewingChange,
    Applying,
    Success,
    Failed
};

struct ComposerStep {
    std::string title;
    std::string description;
    bool completed;
    bool failed;
};

class AgenticComposer {
public:
    AgenticComposer();
    
    // Start a new multi-file operation
    void startGoal(const std::string& userGoal, const std::vector<std::string>& files);
    
    // UI state polling
    ComposerState getState() const { return m_state; }
    const std::vector<ComposerStep>& getSteps() const { return m_steps; }
    const MultiFilePlan& getActivePlan() const { return m_activePlan; }

    // User actions
    void approveStep();
    void rejectStep();
    void rollbackAll();

private:
    ComposerState m_state = ComposerState::Idle;
    std::vector<ComposerStep> m_steps;
    MultiFilePlan m_activePlan;
    std::unique_ptr<MultiFileRewriteEngine> m_rewriteEngine;
    
    void transitionTo(ComposerState newState);
    void executePlanAsync();
};

} // namespace IDE
} // namespace RawrXD
