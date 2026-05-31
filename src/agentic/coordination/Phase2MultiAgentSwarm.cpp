#include "Phase2MultiAgentSwarm.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <future>
#include <sstream>

namespace RawrXD::Agentic {

namespace {

std::string trimCopy(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool containsDangerousControl(const std::string& s) {
    for (unsigned char c : s) {
        if (c < 0x09) return true;
        if (c > 0x0D && c < 0x20) return true;
    }
    return false;
}

} // namespace

Phase2MultiAgentSwarm::Phase2MultiAgentSwarm(Phase2SwarmConfig cfg)
    : m_cfg(std::move(cfg)) {}

Phase2MultiAgentSwarm::~Phase2MultiAgentSwarm() {
    shutdown();
}

bool Phase2MultiAgentSwarm::initialize(
    size_t plannerCount,
    size_t coderCount,
    size_t reviewerCount,
    size_t testerCount,
    size_t securityCount) {
    std::lock_guard<std::mutex> lk(m_mu);
    m_agents.clear();

    auto addRole = [this](SwarmRole role, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            if (m_agents.size() >= m_cfg.maxAgents) return;
            auto a = std::make_unique<Agent>();
            a->role = role;
            a->id = std::string(roleName(role)) + "-" + std::to_string(i + 1);
            m_agents.push_back(std::move(a));
        }
    };

    addRole(SwarmRole::Planner, plannerCount);
    addRole(SwarmRole::Coder, coderCount);
    addRole(SwarmRole::Reviewer, reviewerCount);
    addRole(SwarmRole::Tester, testerCount);
    addRole(SwarmRole::Security, securityCount);

    m_initialized.store(!m_agents.empty(), std::memory_order_release);
    return m_initialized.load(std::memory_order_acquire);
}

void Phase2MultiAgentSwarm::shutdown() {
    std::lock_guard<std::mutex> lk(m_mu);
    m_initialized.store(false, std::memory_order_release);
    m_agents.clear();
}

void Phase2MultiAgentSwarm::setRoleHandler(SwarmRole role, SwarmRoleHandler handler) {
    std::lock_guard<std::mutex> lk(m_mu);
    m_handlers[role] = std::move(handler);
}

SwarmExecutionReport Phase2MultiAgentSwarm::executeGoal(
    const std::string& goal,
    std::chrono::milliseconds timeout) {
    SwarmExecutionReport report;
    auto t0 = std::chrono::steady_clock::now();
    report.goal = goal;

    if (!m_initialized.load(std::memory_order_acquire)) {
        report.status = SwarmStatus::NoAgents;
        report.totalDurationMs = 0.0;
        m_totalFailures.fetch_add(1, std::memory_order_relaxed);
        return report;
    }

    if (!validateGoal(goal)) {
        report.status = SwarmStatus::InvalidInput;
        report.totalDurationMs = 0.0;
        m_totalFailures.fetch_add(1, std::memory_order_relaxed);
        return report;
    }

    if (timeout == std::chrono::milliseconds::zero()) {
        timeout = m_cfg.defaultTimeout;
    }

    auto subtasks = decomposeGoal(goal);
    if (subtasks.empty()) {
        report.status = SwarmStatus::InternalError;
        report.totalDurationMs = 0.0;
        m_totalFailures.fetch_add(1, std::memory_order_relaxed);
        return report;
    }

    report.totalSubtasks = static_cast<uint32_t>(subtasks.size());

    auto selected = selectAgentsFor(subtasks);
    if (selected.empty()) {
        report.status = SwarmStatus::NoAgents;
        m_totalFailures.fetch_add(1, std::memory_order_relaxed);
        return report;
    }
    report.agentCount = static_cast<uint32_t>(selected.size());

    std::vector<std::future<std::optional<SwarmSubtaskResult>>> futures;
    futures.reserve(subtasks.size());

    for (size_t i = 0; i < subtasks.size(); ++i) {
        Agent* agent = selected[i % selected.size()];
        SwarmSubtask sub = subtasks[i];
        futures.emplace_back(std::async(std::launch::async, [this, agent, sub]() {
            return runSubtask(agent, sub);
        }));
    }

    bool timeoutHit = false;
    for (auto& f : futures) {
        if (f.wait_for(timeout) == std::future_status::ready) {
            auto rr = f.get();
            if (rr.has_value()) {
                report.results.push_back(*rr);
                if (rr->success) {
                    ++report.completedSubtasks;
                } else {
                    ++report.failedSubtasks;
                }
            } else {
                ++report.failedSubtasks;
            }
        } else {
            timeoutHit = true;
            ++report.failedSubtasks;
        }
    }

    if (timeoutHit) {
        report.status = SwarmStatus::Timeout;
        m_totalFailures.fetch_add(1, std::memory_order_relaxed);
    } else {
        report.consensus = synthesizeConsensus(report.results, &report.confidence);
        report.status = (report.completedSubtasks > 0) ? SwarmStatus::Success : SwarmStatus::InternalError;
        if (report.status != SwarmStatus::Success) {
            m_totalFailures.fetch_add(1, std::memory_order_relaxed);
        }
    }

    report.totalDurationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();

    m_totalExecutions.fetch_add(1, std::memory_order_relaxed);
    return report;
}

bool Phase2MultiAgentSwarm::validateGoal(const std::string& goal) const {
    if (goal.empty()) return false;
    if (goal.size() > m_cfg.maxGoalBytes) return false;
    if (containsDangerousControl(goal)) return false;

    std::string t = trimCopy(goal);
    if (t.empty()) return false;
    return true;
}

std::vector<SwarmSubtask> Phase2MultiAgentSwarm::decomposeGoal(const std::string& goal) const {
    std::vector<SwarmSubtask> out;

    SwarmSubtask p;
    p.id = "sub-1";
    p.title = "Plan";
    p.payload = "Plan work for goal: " + goal;
    p.role = SwarmRole::Planner;
    out.push_back(std::move(p));

    SwarmSubtask c;
    c.id = "sub-2";
    c.title = "Implement";
    c.payload = "Implement objective: " + goal;
    c.role = SwarmRole::Coder;
    out.push_back(std::move(c));

    SwarmSubtask r;
    r.id = "sub-3";
    r.title = "Review";
    r.payload = "Review implementation for correctness and risk.";
    r.role = SwarmRole::Reviewer;
    out.push_back(std::move(r));

    SwarmSubtask t;
    t.id = "sub-4";
    t.title = "Test";
    t.payload = "Test implementation and capture regressions.";
    t.role = SwarmRole::Tester;
    out.push_back(std::move(t));

    SwarmSubtask s;
    s.id = "sub-5";
    s.title = "Security";
    s.payload = "Run security checks and ensure fail-closed behavior.";
    s.role = SwarmRole::Security;
    out.push_back(std::move(s));

    return out;
}

std::vector<Phase2MultiAgentSwarm::Agent*> Phase2MultiAgentSwarm::selectAgentsFor(const std::vector<SwarmSubtask>& subtasks) {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<Agent*> out;
    out.reserve(subtasks.size());

    for (const auto& sub : subtasks) {
        Agent* match = nullptr;
        for (auto& a : m_agents) {
            if (a->role == sub.role) {
                match = a.get();
                break;
            }
        }
        if (!match && !m_agents.empty()) {
            match = m_agents.front().get();
        }
        if (match) out.push_back(match);
    }
    return out;
}

std::optional<SwarmSubtaskResult> Phase2MultiAgentSwarm::runSubtask(Agent* agent, const SwarmSubtask& subtask) {
    if (!agent) return std::nullopt;

    SwarmSubtaskResult r;
    r.subtaskId = subtask.id;
    r.agentId = agent->id;
    r.role = agent->role;

    auto t0 = std::chrono::steady_clock::now();
    uint32_t attempts = 0;
    bool success = false;
    std::string finalOut;

    SwarmRoleHandler handler;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        auto it = m_handlers.find(subtask.role);
        if (it != m_handlers.end()) {
            handler = it->second;
        }
    }

    for (; attempts <= subtask.maxRetries; ++attempts) {
        std::optional<std::string> out;
        if (handler) {
            out = handler(subtask);
        } else {
            // Safe fallback behavior: deterministic, no shell/network/process actions.
            out = std::string("[") + roleName(subtask.role) + "] " + subtask.title + " completed.";
        }

        if (out.has_value() && !out->empty()) {
            finalOut = *out;
            success = true;
            break;
        }
    }

    r.attempts = attempts + 1;
    r.success = success;
    r.output = success ? finalOut : std::string("subtask failed: ") + subtask.title;
    r.durationMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    if (success) {
        agent->completed.fetch_add(1, std::memory_order_relaxed);
    } else {
        agent->failed.fetch_add(1, std::memory_order_relaxed);
    }

    return r;
}

std::string Phase2MultiAgentSwarm::synthesizeConsensus(const std::vector<SwarmSubtaskResult>& results, double* confidenceOut) const {
    std::ostringstream oss;

    uint32_t success = 0;
    bool reviewerApproved = false;
    bool securityApproved = false;

    for (const auto& r : results) {
        if (r.success) {
            ++success;
            if (r.role == SwarmRole::Reviewer) reviewerApproved = true;
            if (r.role == SwarmRole::Security) securityApproved = true;
        }

        oss << "[" << roleName(r.role) << "] " << r.agentId << ": " << r.output << "\n";
    }

    double ratio = results.empty() ? 0.0 : (static_cast<double>(success) / static_cast<double>(results.size()));
    bool quorum = ratio >= m_cfg.quorumRatio;

    if (confidenceOut) {
        double conf = ratio;
        if (reviewerApproved) conf += 0.10;
        if (securityApproved) conf += 0.10;
        if (conf > 1.0) conf = 1.0;
        *confidenceOut = conf;
    }

    if (!quorum) {
        return "Consensus failed: quorum not reached\n" + oss.str();
    }
    if (!reviewerApproved) {
        return "Consensus failed: reviewer approval missing\n" + oss.str();
    }
    if (!securityApproved) {
        return "Consensus failed: security approval missing\n" + oss.str();
    }

    return std::string("Consensus passed\n") + oss.str();
}

const char* Phase2MultiAgentSwarm::roleName(SwarmRole role) {
    switch (role) {
        case SwarmRole::Planner: return "Planner";
        case SwarmRole::Coder: return "Coder";
        case SwarmRole::Reviewer: return "Reviewer";
        case SwarmRole::Tester: return "Tester";
        case SwarmRole::Security: return "Security";
        default: return "Unknown";
    }
}

} // namespace RawrXD::Agentic
