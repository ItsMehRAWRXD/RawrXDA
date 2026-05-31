#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::Agentic {

enum class AgentState {
    IDLE,
    THINKING,
    TOOL_CALL,
    RESPONDING,
    ERROR,
    TERMINATED
};

class AgentStateMachine {
public:
    AgentStateMachine();
    ~AgentStateMachine() = default;
    
    // State transitions
    void transition(AgentState target);
    bool canTransitionTo(AgentState target) const;
    AgentState currentState() const;
    
    // Context management
    void setContext(const std::string& key, const std::string& value);
    std::string getContext(const std::string& key) const;
    void clearContext();
    
    // Lifecycle
    void startSession(const std::string& sessionId);
    void endSession();
    std::string getSessionId() const;
    
    // Timeout handling
    void setTimeout(std::chrono::milliseconds timeout);
    bool isTimedOut() const;
    void resetTimeout();

private:
    bool isValidTransition(AgentState from, AgentState to) const;
    
    mutable std::mutex m_mutex;
    AgentState m_state = AgentState::IDLE;
    std::map<std::string, std::string> m_context;
    std::string m_sessionId;
    std::chrono::steady_clock::time_point m_sessionStart;
    std::chrono::milliseconds m_timeout{300000}; // 5 minutes default
};

} // namespace RawrXD::Agentic
