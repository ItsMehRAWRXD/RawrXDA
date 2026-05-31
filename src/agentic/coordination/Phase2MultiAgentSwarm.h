#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace RawrXD::Agentic {

enum class SwarmRole : uint8_t {
    Planner = 0,
    Coder,
    Reviewer,
    Tester,
    Security
};

enum class SwarmStatus : uint8_t {
    Success = 0,
    InvalidInput,
    NoAgents,
    Timeout,
    InternalError
};

struct SwarmSubtask {
    std::string id;
    std::string title;
    std::string payload;
    SwarmRole role = SwarmRole::Coder;
    uint32_t maxRetries = 2;
};

struct SwarmSubtaskResult {
    std::string subtaskId;
    std::string agentId;
    SwarmRole role = SwarmRole::Coder;
    bool success = false;
    std::string output;
    double durationMs = 0.0;
    uint32_t attempts = 0;
};

struct SwarmExecutionReport {
    SwarmStatus status = SwarmStatus::InternalError;
    std::string goal;
    std::string consensus;
    double totalDurationMs = 0.0;
    double confidence = 0.0;
    uint32_t agentCount = 0;
    uint32_t totalSubtasks = 0;
    uint32_t completedSubtasks = 0;
    uint32_t failedSubtasks = 0;
    std::vector<SwarmSubtaskResult> results;
};

struct Phase2SwarmConfig {
    size_t maxAgents = 8;
    uint32_t queueLimit = 512;
    uint32_t maxGoalBytes = 4096;
    std::chrono::milliseconds defaultTimeout{30000};
    double quorumRatio = 0.60;
};

using SwarmRoleHandler = std::function<std::optional<std::string>(const SwarmSubtask&)>;

class Phase2MultiAgentSwarm {
public:
    explicit Phase2MultiAgentSwarm(Phase2SwarmConfig cfg = {});
    ~Phase2MultiAgentSwarm();

    Phase2MultiAgentSwarm(const Phase2MultiAgentSwarm&) = delete;
    Phase2MultiAgentSwarm& operator=(const Phase2MultiAgentSwarm&) = delete;

    bool initialize(size_t plannerCount, size_t coderCount, size_t reviewerCount, size_t testerCount, size_t securityCount);
    void shutdown();

    void setRoleHandler(SwarmRole role, SwarmRoleHandler handler);

    SwarmExecutionReport executeGoal(
        const std::string& goal,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    uint64_t getTotalExecutions() const { return m_totalExecutions.load(std::memory_order_relaxed); }
    uint64_t getTotalFailures() const { return m_totalFailures.load(std::memory_order_relaxed); }

private:
    struct Agent {
        std::string id;
        SwarmRole role = SwarmRole::Coder;
        std::atomic<uint64_t> completed{0};
        std::atomic<uint64_t> failed{0};
    };

    Phase2SwarmConfig m_cfg;
    std::atomic<bool> m_initialized{false};

    mutable std::mutex m_mu;
    std::vector<std::unique_ptr<Agent>> m_agents;
    std::unordered_map<SwarmRole, SwarmRoleHandler> m_handlers;

    std::atomic<uint64_t> m_totalExecutions{0};
    std::atomic<uint64_t> m_totalFailures{0};

    bool validateGoal(const std::string& goal) const;
    std::vector<SwarmSubtask> decomposeGoal(const std::string& goal) const;
    std::vector<Agent*> selectAgentsFor(const std::vector<SwarmSubtask>& subtasks);
    std::optional<SwarmSubtaskResult> runSubtask(Agent* agent, const SwarmSubtask& subtask);
    std::string synthesizeConsensus(const std::vector<SwarmSubtaskResult>& results, double* confidenceOut) const;

    static const char* roleName(SwarmRole role);
};

} // namespace RawrXD::Agentic
