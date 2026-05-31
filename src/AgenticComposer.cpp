#include "AgenticComposer.h"
#include <iostream>
#include <thread>

namespace RawrXD {
namespace IDE {

AgenticComposer::AgenticComposer() {
    m_rewriteEngine = std::make_unique<MultiFileRewriteEngine>();
}

void AgenticComposer::startGoal(const std::string& userGoal, const std::vector<std::string>& files) {
    m_state = ComposerState::Planning;
    m_steps.clear();
    
    // Add planning step
    m_steps.push_back({"Goal Analysis", "Analyze cross-file dependencies for: " + userGoal, false, false});

    // Simulate async planning task
    std::thread([this, userGoal, files]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        m_activePlan = m_rewriteEngine->planCoordinatedEdits(userGoal, files);
        
        m_steps[0].completed = true;
        m_steps.push_back({"Review Plan", "Coordinated edits generated for " + std::to_string(files.size()) + " files.", false, false});
        
        m_state = ComposerState::ReviewingChange;
    }).detach();
}

void AgenticComposer::approveStep() {
    if (m_state != ComposerState::ReviewingChange) return;

    m_state = ComposerState::Applying;
    m_steps.back().completed = true;
    m_steps.push_back({"Applying Edits", "Committing atomic changes to disk...", false, false});

    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (m_rewriteEngine->applyPlan(m_activePlan)) {
            m_steps.back().completed = true;
            m_state = ComposerState::Success;
        } else {
            m_steps.back().failed = true;
            m_state = ComposerState::Failed;
            m_rewriteEngine->rollback(m_activePlan);
        }
    }).detach();
}

void AgenticComposer::rejectStep() {
    m_state = ComposerState::Idle;
    m_steps.back().failed = true;
    m_steps.push_back({"Rejected", "User rejected the plan.", true, true});
}

void AgenticComposer::rollbackAll() {
    m_rewriteEngine->rollback(m_activePlan);
}

} // namespace IDE
} // namespace RawrXD
