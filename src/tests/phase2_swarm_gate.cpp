#include "../agentic/coordination/Phase2MultiAgentSwarm.h"

#include <cassert>
#include <chrono>
#include <iostream>

using RawrXD::Agentic::Phase2MultiAgentSwarm;
using RawrXD::Agentic::Phase2SwarmConfig;
using RawrXD::Agentic::SwarmRole;
using RawrXD::Agentic::SwarmStatus;

int main() {
    Phase2SwarmConfig cfg;
    cfg.maxAgents = 10;
    cfg.defaultTimeout = std::chrono::milliseconds(8000);
    cfg.quorumRatio = 0.60;

    Phase2MultiAgentSwarm swarm(cfg);
    bool ok = swarm.initialize(/*planner*/1, /*coder*/2, /*reviewer*/1, /*tester*/1, /*security*/1);
    assert(ok);

    // Deterministic role handlers for repeatable gate behavior.
    swarm.setRoleHandler(SwarmRole::Planner, [](const auto& sub) {
        return std::optional<std::string>("plan: scoped milestones for '" + sub.title + "'");
    });
    swarm.setRoleHandler(SwarmRole::Coder, [](const auto& sub) {
        return std::optional<std::string>("code: implementation landed for '" + sub.title + "'");
    });
    swarm.setRoleHandler(SwarmRole::Reviewer, [](const auto& sub) {
        return std::optional<std::string>("review: approved '" + sub.title + "' with no blockers");
    });
    swarm.setRoleHandler(SwarmRole::Tester, [](const auto& sub) {
        return std::optional<std::string>("tests: pass for '" + sub.title + "'");
    });
    swarm.setRoleHandler(SwarmRole::Security, [](const auto& sub) {
        return std::optional<std::string>("security: pass for '" + sub.title + "' (fail-closed)");
    });

    auto report = swarm.executeGoal("Phase 2 expansion into multi-agent swarms with full runtime checks");

    assert(report.status == SwarmStatus::Success);
    assert(report.totalSubtasks >= 5);
    assert(report.completedSubtasks >= 5);
    assert(report.failedSubtasks == 0);
    assert(report.confidence >= 0.8);
    assert(report.consensus.find("Consensus passed") != std::string::npos);
    assert(report.consensus.find("security") != std::string::npos || report.consensus.find("Security") != std::string::npos);

    // Invalid input gate
    auto bad = swarm.executeGoal("");
    assert(bad.status == SwarmStatus::InvalidInput);

    std::cout << "phase2_swarm_gate: PASS"
              << " subtasks=" << report.totalSubtasks
              << " completed=" << report.completedSubtasks
              << " confidence=" << report.confidence
              << " total_ms=" << report.totalDurationMs
              << "\n";

    swarm.shutdown();
    return 0;
}
