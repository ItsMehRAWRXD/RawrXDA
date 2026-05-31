#include "agentic/agent_state_machine.h"

namespace RawrXD::Agentic {
    AgentStateMachine::AgentStateMachine() : m_state(AgentState::IDLE) {}

    void AgentStateMachine::transition(AgentState target) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!isValidTransition(m_state, target)) {
            throw std::runtime_error("Invalid agent state transition");
        }
        m_state = target;
        if (target == AgentState::IDLE) {
            m_context.clear();
        }
    }

    bool AgentStateMachine::canTransitionTo(AgentState target) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return isValidTransition(m_state, target);
    }

    AgentState AgentStateMachine::currentState() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    bool AgentStateMachine::isValidTransition(AgentState from, AgentState to) const {
        switch (from) {
            case AgentState::IDLE:
                return to == AgentState::THINKING || to == AgentState::TERMINATED;
            case AgentState::THINKING:
                return to == AgentState::TOOL_CALL || to == AgentState::RESPONDING || to == AgentState::IDLE;
            case AgentState::TOOL_CALL:
                return to == AgentState::THINKING || to == AgentState::ERROR;
            case AgentState::RESPONDING:
                return to == AgentState::IDLE || to == AgentState::ERROR;
            case AgentState::ERROR:
                return to == AgentState::IDLE || to == AgentState::TERMINATED;
            case AgentState::TERMINATED:
                return false;
        }
        return false;
    }

    void AgentStateMachine::setContext(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_context[key] = value;
    }

    std::string AgentStateMachine::getContext(const std::string& key) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_context.find(key);
        return (it != m_context.end()) ? it->second : "";
    }

    void AgentStateMachine::clearContext() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_context.clear();
    }

    void AgentStateMachine::startSession(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessionId = sessionId;
        m_sessionStart = std::chrono::steady_clock::now();
        m_state = AgentState::IDLE;
        m_context.clear();
    }

    void AgentStateMachine::endSession() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessionId.clear();
        m_state = AgentState::TERMINATED;
        m_context.clear();
    }

    std::string AgentStateMachine::getSessionId() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sessionId;
    }

    void AgentStateMachine::setTimeout(std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_timeout = timeout;
    }

    bool AgentStateMachine::isTimedOut() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto elapsed = std::chrono::steady_clock::now() - m_sessionStart;
        return elapsed > m_timeout;
    }

    void AgentStateMachine::resetTimeout() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessionStart = std::chrono::steady_clock::now();
    }
}
