// ============================================================================
// agent_explainability.cpp — Decision Attribution & Causal Trace (Phase 8A)
// ============================================================================
// Pure read-only implementation.  Builds causal graphs from:
//   - AgentHistoryRecorder  (Phase 5 events)
//   - PolicyEngine          (Phase 7 policies, heuristics)
//
// No mutations, no retries, no inference.  Attribution only.
// ============================================================================

#include "agent_explainability.h"
#include "agent_history.h"
#include "agent_policy.h"

#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>
#include <ctime>
#include <set>

// ============================================================================
// Utility
// ============================================================================

std::string ExplainabilityEngine::generateUUID() const {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFF);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(4) << dist(gen) << std::setw(4) << dist(gen) << "-"
       << std::setw(4) << dist(gen) << "-"
       << std::setw(4) << (0x4000 | (dist(gen) & 0x0FFF)) << "-"
       << std::setw(4) << (0x8000 | (dist(gen) & 0x3FFF)) << "-"
       << std::setw(4) << dist(gen) << std::setw(4) << dist(gen)
       << std::setw(4) << dist(gen);
    return ss.str();
}

int64_t ExplainabilityEngine::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string ExplainabilityEngine::escapeJson(const std::string& s) const {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string ExplainabilityEngine::nowISO8601() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ExplainabilityEngine::ExplainabilityEngine() {}
ExplainabilityEngine::~ExplainabilityEngine() {}

// ============================================================================
// DecisionNode::toJSON
// ============================================================================

std::string DecisionNode::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"eventId\":" << eventId;
    ss << ",\"eventType\":\"" << esc(eventType) << "\"";
    ss << ",\"agentId\":\"" << esc(agentId) << "\"";
    ss << ",\"description\":\"" << esc(description) << "\"";
    ss << ",\"timestampMs\":" << timestampMs;
    ss << ",\"durationMs\":" << durationMs;
    ss << ",\"success\":" << (success ? "true" : "false");
    if (!errorMessage.empty())
        ss << ",\"errorMessage\":\"" << esc(errorMessage) << "\"";
    if (parentEventId > 0)
        ss << ",\"parentEventId\":" << parentEventId;
    if (!trigger.empty())
        ss << ",\"trigger\":\"" << esc(trigger) << "\"";
    if (!policyId.empty()) {
        ss << ",\"policyId\":\"" << esc(policyId) << "\"";
        ss << ",\"policyName\":\"" << esc(policyName) << "\"";
        ss << ",\"policyEffect\":\"" << esc(policyEffect) << "\"";
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// DecisionTrace::toJSON
// ============================================================================

std::string DecisionTrace::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"traceId\":\"" << esc(traceId) << "\"";
    ss << ",\"rootAgentId\":\"" << esc(rootAgentId) << "\"";
    ss << ",\"rootEventType\":\"" << esc(rootEventType) << "\"";
    ss << ",\"sessionId\":\"" << esc(sessionId) << "\"";
    ss << ",\"overallSuccess\":" << (overallSuccess ? "true" : "false");
    ss << ",\"totalDurationMs\":" << totalDurationMs;
    ss << ",\"nodeCount\":" << nodeCount;
    ss << ",\"failureCount\":" << failureCount;
    ss << ",\"policyFireCount\":" << policyFireCount;
    ss << ",\"summary\":\"" << esc(summary) << "\"";
    ss << ",\"nodes\":[";
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) ss << ",";
        ss << nodes[i].toJSON();
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// FailureAttribution::toJSON
// ============================================================================

std::string FailureAttribution::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"failureEventId\":" << failureEventId;
    ss << ",\"agentId\":\"" << esc(agentId) << "\"";
    ss << ",\"failureType\":\"" << esc(failureType) << "\"";
    ss << ",\"errorMessage\":\"" << esc(errorMessage) << "\"";
    ss << ",\"timestampMs\":" << timestampMs;
    ss << ",\"wasRetried\":" << (wasRetried ? "true" : "false");
    if (wasRetried) {
        ss << ",\"retryEventId\":" << retryEventId;
        ss << ",\"retrySucceeded\":" << (retrySucceeded ? "true" : "false");
    }
    ss << ",\"correctionStrategy\":\"" << esc(correctionStrategy) << "\"";
    if (!policyId.empty()) {
        ss << ",\"policyId\":\"" << esc(policyId) << "\"";
        ss << ",\"policyName\":\"" << esc(policyName) << "\"";
        ss << ",\"policyRationale\":\"" << esc(policyRationale) << "\"";
    }
    if (historicalSuccessRate >= 0.0f) {
        ss << ",\"historicalSuccessRate\":" << historicalSuccessRate;
        ss << ",\"historicalOccurrences\":" << historicalOccurrences;
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// PolicyAttribution::toJSON
// ============================================================================

std::string PolicyAttribution::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"policyId\":\"" << esc(policyId) << "\"";
    ss << ",\"policyName\":\"" << esc(policyName) << "\"";
    ss << ",\"policyDescription\":\"" << esc(policyDescription) << "\"";
    ss << ",\"policyPriority\":" << policyPriority;
    ss << ",\"triggerEventType\":\"" << esc(triggerEventType) << "\"";
    ss << ",\"triggerPattern\":\"" << esc(triggerPattern) << "\"";
    ss << ",\"triggerFailureRate\":" << triggerFailureRate;
    ss << ",\"effectDescription\":\"" << esc(effectDescription) << "\"";
    ss << ",\"redirectedSwarmToChain\":" << (redirectedSwarmToChain ? "true" : "false");
    if (timeoutOverrideMs > 0)
        ss << ",\"timeoutOverrideMs\":" << timeoutOverrideMs;
    if (maxRetriesOverride >= 0)
        ss << ",\"maxRetriesOverride\":" << maxRetriesOverride;
    ss << ",\"addedValidation\":" << (addedValidation ? "true" : "false");
    ss << ",\"estimatedImprovement\":" << estimatedImprovement;
    ss << ",\"policyAppliedCount\":" << policyAppliedCount;
    ss << "}";
    return ss.str();
}

// ============================================================================
// SessionExplanation::toJSON
// ============================================================================

std::string SessionExplanation::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"sessionId\":\"" << esc(sessionId) << "\"";
    ss << ",\"startTimestampMs\":" << startTimestampMs;
    ss << ",\"endTimestampMs\":" << endTimestampMs;
    ss << ",\"totalEvents\":" << totalEvents;
    ss << ",\"agentSpawns\":" << agentSpawns;
    ss << ",\"chainExecutions\":" << chainExecutions;
    ss << ",\"swarmExecutions\":" << swarmExecutions;
    ss << ",\"failures\":" << failures;
    ss << ",\"retries\":" << retries;
    ss << ",\"policyFirings\":" << policyFirings;

    ss << ",\"traces\":[";
    for (size_t i = 0; i < traces.size(); ++i) {
        if (i > 0) ss << ",";
        ss << traces[i].toJSON();
    }
    ss << "]";

    ss << ",\"failureAttributions\":[";
    for (size_t i = 0; i < failureAttributions.size(); ++i) {
        if (i > 0) ss << ",";
        ss << failureAttributions[i].toJSON();
    }
    ss << "]";

    ss << ",\"policyAttributions\":[";
    for (size_t i = 0; i < policyAttributions.size(); ++i) {
        if (i > 0) ss << ",";
        ss << policyAttributions[i].toJSON();
    }
    ss << "]";

    ss << ",\"narrative\":\"" << esc(narrative) << "\"";
    ss << "}";
    return ss.str();
}

// ============================================================================
// ExplainabilitySnapshot::toJSON
// ============================================================================

std::string ExplainabilitySnapshot::toJSON() const {
    std::ostringstream ss;
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };
    ss << "{";
    ss << "\"version\":\"" << esc(version) << "\"";
    ss << ",\"generatedAt\":\"" << esc(generatedAt) << "\"";
    ss << ",\"engineVersion\":\"" << esc(engineVersion) << "\"";
    ss << ",\"session\":" << session.toJSON();
    // Raw data is already JSON, embed directly
    ss << ",\"rawEvents\":" << (rawEventsJSON.empty() ? "[]" : rawEventsJSON);
    ss << ",\"rawPolicies\":" << (rawPoliciesJSON.empty() ? "[]" : rawPoliciesJSON);
    ss << ",\"rawHeuristics\":" << (rawHeuristicsJSON.empty() ? "{}" : rawHeuristicsJSON);
    ss << "}";
    return ss.str();
}

// ============================================================================
// Internal: buildNode — convert an AgentEvent into a DecisionNode
// ============================================================================

DecisionNode ExplainabilityEngine::buildNode(const AgentEvent& event) const {
    DecisionNode node;
    node.eventId      = event.id;
    node.eventType    = event.eventType;
    node.agentId      = event.agentId;
    node.description  = event.description;
    node.timestampMs  = event.timestampMs;
    node.durationMs   = event.durationMs;
    node.success      = event.success;
    node.errorMessage = event.errorMessage;

    // Determine parent event ID from metadata if available
    // Metadata may contain "parentEventId" as a JSON field
    if (!event.parentId.empty()) {
        // The parentId field in AgentEvent is the agent/session parent
        // We'll link via parentId → agent events
        node.trigger = "Spawned by " + event.parentId;
    }

    // Build trigger description based on event type
    if (event.eventType == "agent_spawn") {
        node.trigger = "Agent spawned";
        if (!event.parentId.empty())
            node.trigger += " (parent: " + event.parentId + ")";
    } else if (event.eventType == "agent_fail") {
        node.trigger = "Agent failed: " + event.errorMessage;
    } else if (event.eventType == "agent_complete") {
        node.trigger = "Agent completed";
    } else if (event.eventType == "chain_start") {
        node.trigger = "Chain initiated";
    } else if (event.eventType == "chain_step") {
        node.trigger = "Chain step from previous output";
    } else if (event.eventType == "chain_complete") {
        node.trigger = "Chain completed all steps";
    } else if (event.eventType == "chain_fail") {
        node.trigger = "Chain failed at step: " + event.errorMessage;
    } else if (event.eventType == "swarm_start") {
        node.trigger = "Swarm fan-out initiated";
    } else if (event.eventType == "swarm_task") {
        node.trigger = "Swarm parallel task";
    } else if (event.eventType == "swarm_merge" || event.eventType == "swarm_complete") {
        node.trigger = "Swarm merge completed";
    } else if (event.eventType == "tool_invoke") {
        node.trigger = "Tool invoked";
    } else if (event.eventType == "tool_result") {
        node.trigger = "Tool returned result";
    } else if (event.eventType == "chat_request") {
        node.trigger = "User chat request";
    } else if (event.eventType == "chat_response") {
        node.trigger = "Model generated response";
    } else {
        node.trigger = event.eventType;
    }

    return node;
}

// ============================================================================
// Internal: attachPolicyAttribution — check if any policy matched this event
// ============================================================================

void ExplainabilityEngine::attachPolicyAttribution(DecisionNode& node) const {
    if (!m_policyEngine) return;

    // Evaluate what policies *would* match this event type
    // Note: we use a const_cast-free approach — PolicyEngine::evaluate is non-const
    // because it increments atomic counters. We accept this for attribution.
    PolicyEngine* pe = m_policyEngine;  // non-const access for evaluate()

    PolicyEvalResult eval = pe->evaluate(
        node.eventType,
        node.description,
        "",   // toolName — not available at node level
        node.errorMessage
    );

    if (eval.hasMatch && !eval.matchedPolicies.empty()) {
        const AgentPolicy* top = eval.matchedPolicies[0];
        node.policyId     = top->id;
        node.policyName   = top->name;

        // Build effect description
        std::string effect;
        if (eval.mergedAction.preferChainOverSwarm)
            effect += "Prefer chain over swarm. ";
        if (eval.mergedAction.maxRetries >= 0)
            effect += "Max retries: " + std::to_string(eval.mergedAction.maxRetries) + ". ";
        if (eval.mergedAction.timeoutOverrideMs > 0)
            effect += "Timeout: " + std::to_string(eval.mergedAction.timeoutOverrideMs) + "ms. ";
        if (eval.mergedAction.reduceParallelism > 0)
            effect += "Reduce parallelism by " + std::to_string(eval.mergedAction.reduceParallelism) + ". ";
        if (eval.mergedAction.addValidationStep)
            effect += "Add validation step. ";

        if (effect.empty()) effect = "Policy matched (no action override).";
        node.policyEffect = effect;
    }
}

// ============================================================================
// Internal: buildNarrative — generate human-readable session narrative
// ============================================================================

std::string ExplainabilityEngine::buildNarrative(const SessionExplanation& session) const {
    std::ostringstream ss;

    ss << "Session " << session.sessionId << " processed "
       << session.totalEvents << " events";

    if (session.agentSpawns > 0)
        ss << ", spawned " << session.agentSpawns << " agent(s)";
    if (session.chainExecutions > 0)
        ss << ", executed " << session.chainExecutions << " chain(s)";
    if (session.swarmExecutions > 0)
        ss << ", ran " << session.swarmExecutions << " swarm(s)";

    ss << ".";

    if (session.failures > 0) {
        ss << " " << session.failures << " failure(s) occurred";
        if (session.retries > 0)
            ss << ", " << session.retries << " were retried";
        ss << ".";
    }

    if (session.policyFirings > 0) {
        ss << " " << session.policyFirings << " policy rule(s) fired";
        // Count swarm→chain redirections
        int redirects = 0;
        for (const auto& pa : session.policyAttributions) {
            if (pa.redirectedSwarmToChain) redirects++;
        }
        if (redirects > 0)
            ss << " (" << redirects << " swarm→chain redirect(s))";
        ss << ".";
    }

    if (session.failures == 0 && session.policyFirings == 0) {
        ss << " No failures or policy interventions.";
    }

    return ss.str();
}

// ============================================================================
// Internal: buildFailureAttribution — explain one failure event
// ============================================================================

FailureAttribution ExplainabilityEngine::buildFailureAttribution(const AgentEvent& failEvent) const {
    FailureAttribution fa;
    fa.failureEventId = failEvent.id;
    fa.agentId        = failEvent.agentId;
    fa.failureType    = failEvent.eventType;
    fa.errorMessage   = failEvent.errorMessage;
    fa.timestampMs    = failEvent.timestampMs;

    // Look for a retry: an agent_spawn for the same parentId after this failure
    if (m_historyRecorder) {
        auto timeline = m_historyRecorder->getAgentTimeline(failEvent.agentId);

        // Search for an agent_spawn after the failure timestamp with same parent
        for (const auto& e : timeline) {
            if (e.timestampMs > failEvent.timestampMs &&
                e.eventType == "agent_spawn" &&
                e.parentId == failEvent.parentId) {
                fa.wasRetried = true;
                fa.retryEventId = e.id;
                // Check if the retry succeeded
                for (const auto& e2 : timeline) {
                    if (e2.agentId == e.agentId && e2.eventType == "agent_complete") {
                        fa.retrySucceeded = true;
                        break;
                    }
                }
                break;
            }
        }

        // Also check: was there a subsequent agent_complete for the same parent?
        if (!fa.wasRetried) {
            // Broader search — get parent timeline
            if (!failEvent.parentId.empty()) {
                auto parentEvents = m_historyRecorder->getAgentTimeline(failEvent.parentId);
                for (const auto& e : parentEvents) {
                    if (e.timestampMs > failEvent.timestampMs &&
                        e.eventType == "agent_spawn") {
                        fa.wasRetried = true;
                        fa.retryEventId = e.id;
                        for (const auto& e2 : parentEvents) {
                            if (e2.id > e.id && e2.eventType == "agent_complete") {
                                fa.retrySucceeded = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    fa.correctionStrategy = fa.wasRetried ? "retry" : "none";

    // Check if a policy influenced this (based on matching error message / event type)
    if (m_policyEngine) {
        PolicyEngine* pe = m_policyEngine;
        PolicyEvalResult eval = pe->evaluate(
            failEvent.eventType, failEvent.description,
            "", failEvent.errorMessage);

        if (eval.hasMatch && !eval.matchedPolicies.empty()) {
            const AgentPolicy* top = eval.matchedPolicies[0];
            fa.policyId = top->id;
            fa.policyName = top->name;
            fa.policyRationale = top->description;
            if (eval.mergedAction.preferChainOverSwarm)
                fa.correctionStrategy = "policy_redirect";
            else if (eval.mergedAction.maxRetries >= 0)
                fa.correctionStrategy = "policy_retry";
        }

        // Attach heuristic context
        const PolicyHeuristic* heur = pe->getHeuristic(failEvent.eventType);
        if (heur) {
            fa.historicalSuccessRate = heur->successRate;
            fa.historicalOccurrences = heur->totalEvents;
        }
    }

    return fa;
}

// ============================================================================
// Internal: buildPolicyAttribution — explain one policy's impact
// ============================================================================

PolicyAttribution ExplainabilityEngine::buildPolicyAttribution(const AgentPolicy& policy) const {
    PolicyAttribution pa;
    pa.policyId          = policy.id;
    pa.policyName        = policy.name;
    pa.policyDescription = policy.description;
    pa.policyPriority    = policy.priority;
    pa.policyAppliedCount = policy.appliedCount;

    // Trigger details
    pa.triggerEventType  = policy.trigger.eventType;
    pa.triggerPattern    = policy.trigger.taskPattern;
    pa.triggerFailureRate = policy.trigger.failureRateAbove;

    // Effect description
    std::string effect;
    if (policy.action.preferChainOverSwarm) {
        effect += "Redirects swarm to chain execution. ";
        pa.redirectedSwarmToChain = true;
    }
    if (policy.action.timeoutOverrideMs > 0) {
        effect += "Overrides timeout to " + std::to_string(policy.action.timeoutOverrideMs) + "ms. ";
        pa.timeoutOverrideMs = policy.action.timeoutOverrideMs;
    }
    if (policy.action.maxRetries >= 0) {
        effect += "Sets max retries to " + std::to_string(policy.action.maxRetries) + ". ";
        pa.maxRetriesOverride = policy.action.maxRetries;
    }
    if (policy.action.reduceParallelism > 0) {
        effect += "Reduces parallelism by " + std::to_string(policy.action.reduceParallelism) + ". ";
    }
    if (policy.action.addValidationStep) {
        effect += "Adds validation sub-agent step. ";
        pa.addedValidation = true;
    }
    if (!policy.action.customAction.empty()) {
        effect += "Custom action: " + policy.action.customAction + ". ";
    }
    if (effect.empty()) effect = "Policy matched (monitoring only).";
    pa.effectDescription = effect;

    return pa;
}

// ============================================================================
// traceAgent — build causal trace for a specific agent
// ============================================================================

DecisionTrace ExplainabilityEngine::traceAgent(const std::string& agentId) const {
    DecisionTrace trace;
    trace.traceId      = generateUUID();
    trace.rootAgentId  = agentId;
    trace.rootEventType = "agent_spawn";

    if (!m_historyRecorder) {
        logError("No history recorder — cannot build trace");
        trace.summary = "No history data available.";
        return trace;
    }

    trace.sessionId = m_historyRecorder->sessionId();

    // Get all events for this agent
    auto events = m_historyRecorder->getAgentTimeline(agentId);
    if (events.empty()) {
        // Try as a parentId (for chains/swarms)
        events = m_historyRecorder->getChainEvents(agentId);
        if (!events.empty()) {
            trace.rootEventType = "chain_start";
        } else {
            events = m_historyRecorder->getSwarmEvents(agentId);
            if (!events.empty()) {
                trace.rootEventType = "swarm_start";
            }
        }
    }

    if (events.empty()) {
        trace.summary = "No events found for agent " + agentId + ".";
        return trace;
    }

    // Sort by timestamp
    std::sort(events.begin(), events.end(),
        [](const AgentEvent& a, const AgentEvent& b) {
            return a.timestampMs < b.timestampMs;
        });

    // Build nodes
    int64_t prevId = 0;
    for (const auto& e : events) {
        DecisionNode node = buildNode(e);
        node.parentEventId = prevId;
        attachPolicyAttribution(node);

        if (!node.success) trace.failureCount++;
        if (!node.policyId.empty()) trace.policyFireCount++;

        trace.nodes.push_back(node);
        prevId = e.id;
    }

    // Compute aggregates
    trace.nodeCount = (int)trace.nodes.size();
    trace.overallSuccess = (trace.failureCount == 0);
    if (!trace.nodes.empty()) {
        trace.totalDurationMs = (int)(
            trace.nodes.back().timestampMs - trace.nodes.front().timestampMs +
            trace.nodes.back().durationMs);
    }

    // Generate summary
    std::ostringstream ss;
    ss << "Agent " << agentId << ": " << trace.nodeCount << " events";
    if (trace.failureCount > 0) ss << ", " << trace.failureCount << " failure(s)";
    if (trace.policyFireCount > 0) ss << ", " << trace.policyFireCount << " policy intervention(s)";
    ss << ". " << (trace.overallSuccess ? "Completed successfully." : "Ended with failure.");
    trace.summary = ss.str();

    return trace;
}

// ============================================================================
// traceChain — build causal trace for a chain execution
// ============================================================================

DecisionTrace ExplainabilityEngine::traceChain(const std::string& parentId) const {
    DecisionTrace trace;
    trace.traceId       = generateUUID();
    trace.rootAgentId   = parentId;
    trace.rootEventType = "chain_start";

    if (!m_historyRecorder) {
        trace.summary = "No history data available.";
        return trace;
    }
    trace.sessionId = m_historyRecorder->sessionId();

    auto events = m_historyRecorder->getChainEvents(parentId);
    if (events.empty()) {
        trace.summary = "No chain events found for " + parentId + ".";
        return trace;
    }

    std::sort(events.begin(), events.end(),
        [](const AgentEvent& a, const AgentEvent& b) { return a.timestampMs < b.timestampMs; });

    int64_t prevId = 0;
    for (const auto& e : events) {
        DecisionNode node = buildNode(e);
        node.parentEventId = prevId;
        node.trigger = (e.eventType == "chain_start") ? "Chain initiated"
                     : (e.eventType == "chain_step")  ? "Previous step output → next step input"
                     : (e.eventType == "chain_complete") ? "All chain steps completed"
                     : "Chain step failed: " + e.errorMessage;
        attachPolicyAttribution(node);

        if (!node.success) trace.failureCount++;
        if (!node.policyId.empty()) trace.policyFireCount++;

        trace.nodes.push_back(node);
        prevId = e.id;
    }

    trace.nodeCount = (int)trace.nodes.size();
    trace.overallSuccess = (trace.failureCount == 0);
    if (!trace.nodes.empty()) {
        trace.totalDurationMs = (int)(
            trace.nodes.back().timestampMs - trace.nodes.front().timestampMs +
            trace.nodes.back().durationMs);
    }

    std::ostringstream ss;
    ss << "Chain " << parentId << ": " << trace.nodeCount << " steps";
    if (trace.failureCount > 0) ss << ", " << trace.failureCount << " failure(s)";
    if (trace.policyFireCount > 0) ss << ", " << trace.policyFireCount << " policy intervention(s)";
    ss << ". " << (trace.overallSuccess ? "Completed." : "Failed.");
    trace.summary = ss.str();

    return trace;
}

// ============================================================================
// traceSwarm — build causal trace for a swarm execution
// ============================================================================

DecisionTrace ExplainabilityEngine::traceSwarm(const std::string& parentId) const {
    DecisionTrace trace;
    trace.traceId       = generateUUID();
    trace.rootAgentId   = parentId;
    trace.rootEventType = "swarm_start";

    if (!m_historyRecorder) {
        trace.summary = "No history data available.";
        return trace;
    }
    trace.sessionId = m_historyRecorder->sessionId();

    auto events = m_historyRecorder->getSwarmEvents(parentId);
    if (events.empty()) {
        trace.summary = "No swarm events found for " + parentId + ".";
        return trace;
    }

    std::sort(events.begin(), events.end(),
        [](const AgentEvent& a, const AgentEvent& b) { return a.timestampMs < b.timestampMs; });

    int64_t prevId = 0;
    for (const auto& e : events) {
        DecisionNode node = buildNode(e);
        node.parentEventId = prevId;
        node.trigger = (e.eventType == "swarm_start") ? "Swarm fan-out initiated"
                     : (e.eventType == "swarm_task")  ? "Parallel task execution"
                     : (e.eventType == "swarm_merge" || e.eventType == "swarm_complete")
                       ? "Merge: " + e.description
                     : e.eventType;
        attachPolicyAttribution(node);

        if (!node.success) trace.failureCount++;
        if (!node.policyId.empty()) trace.policyFireCount++;

        trace.nodes.push_back(node);
        prevId = e.id;
    }

    trace.nodeCount = (int)trace.nodes.size();
    trace.overallSuccess = (trace.failureCount == 0);
    if (!trace.nodes.empty()) {
        trace.totalDurationMs = (int)(
            trace.nodes.back().timestampMs - trace.nodes.front().timestampMs +
            trace.nodes.back().durationMs);
    }

    std::ostringstream ss;
    ss << "Swarm " << parentId << ": " << trace.nodeCount << " events";
    if (trace.failureCount > 0) ss << ", " << trace.failureCount << " failure(s)";
    if (trace.policyFireCount > 0) ss << ", " << trace.policyFireCount << " policy intervention(s)";
    ss << ". " << (trace.overallSuccess ? "Merged successfully." : "Failed.");
    trace.summary = ss.str();

    return trace;
}

// ============================================================================
// traceAuto — auto-detect type and trace
// ============================================================================

DecisionTrace ExplainabilityEngine::traceAuto(const std::string& id) const {
    if (!m_historyRecorder) {
        DecisionTrace t;
        t.summary = "No history data available.";
        return t;
    }

    // Try agent first
    auto agentEvents = m_historyRecorder->getAgentTimeline(id);
    if (!agentEvents.empty()) return traceAgent(id);

    // Try chain
    auto chainEvents = m_historyRecorder->getChainEvents(id);
    if (!chainEvents.empty()) return traceChain(id);

    // Try swarm
    auto swarmEvents = m_historyRecorder->getSwarmEvents(id);
    if (!swarmEvents.empty()) return traceSwarm(id);

    // Fallback — search all events for this ID
    return traceAgent(id);
}

// ============================================================================
// explainFailures — all failures in current session
// ============================================================================

std::vector<FailureAttribution> ExplainabilityEngine::explainFailures() const {
    std::vector<FailureAttribution> result;
    if (!m_historyRecorder) return result;

    auto allEvents = m_historyRecorder->allEvents();
    for (const auto& e : allEvents) {
        if (!e.success && !e.errorMessage.empty()) {
            // Only attribute failures for actionable event types
            if (e.eventType == "agent_fail" ||
                e.eventType == "chain_fail" ||
                e.eventType == "tool_result") {
                result.push_back(buildFailureAttribution(e));
            }
        }
    }

    return result;
}

// ============================================================================
// explainFailure — single failure event
// ============================================================================

FailureAttribution ExplainabilityEngine::explainFailure(int64_t eventId) const {
    FailureAttribution fa;
    if (!m_historyRecorder) {
        fa.errorMessage = "No history data available.";
        return fa;
    }

    auto allEvents = m_historyRecorder->allEvents();
    for (const auto& e : allEvents) {
        if (e.id == eventId) {
            return buildFailureAttribution(e);
        }
    }

    fa.errorMessage = "Event not found: " + std::to_string(eventId);
    return fa;
}

// ============================================================================
// explainPolicies — all policy firings in session
// ============================================================================

std::vector<PolicyAttribution> ExplainabilityEngine::explainPolicies() const {
    std::vector<PolicyAttribution> result;
    if (!m_policyEngine) return result;

    auto policies = m_policyEngine->getAllPolicies(true);  // enabled only
    for (const auto& p : policies) {
        if (p.appliedCount > 0) {
            result.push_back(buildPolicyAttribution(p));
        }
    }

    return result;
}

// ============================================================================
// explainPolicy — single policy impact
// ============================================================================

PolicyAttribution ExplainabilityEngine::explainPolicy(const std::string& policyId) const {
    PolicyAttribution pa;
    if (!m_policyEngine) {
        pa.effectDescription = "No policy engine available.";
        return pa;
    }

    const AgentPolicy* pol = m_policyEngine->getPolicy(policyId);
    if (!pol) {
        pa.effectDescription = "Policy not found: " + policyId;
        return pa;
    }

    return buildPolicyAttribution(*pol);
}

// ============================================================================
// explainSession — full session explanation (no arguments = current)
// ============================================================================

SessionExplanation ExplainabilityEngine::explainSession() const {
    if (!m_historyRecorder) {
        SessionExplanation s;
        s.narrative = "No history data available.";
        return s;
    }
    return explainSession(m_historyRecorder->sessionId());
}

SessionExplanation ExplainabilityEngine::explainSession(const std::string& sessionId) const {
    SessionExplanation session;
    session.sessionId = sessionId;

    if (!m_historyRecorder) {
        session.narrative = "No history data available.";
        return session;
    }

    auto events = m_historyRecorder->getSessionTimeline(sessionId);
    if (events.empty()) {
        // Fall back to all events if session filter returns nothing
        events = m_historyRecorder->allEvents();
    }

    session.totalEvents = (int)events.size();
    if (!events.empty()) {
        session.startTimestampMs = events.front().timestampMs;
        session.endTimestampMs   = events.back().timestampMs;
    }

    // Collect unique root agent IDs for trace building
    std::set<std::string> tracedAgents;
    std::set<std::string> tracedChains;
    std::set<std::string> tracedSwarms;

    for (const auto& e : events) {
        if (e.eventType == "agent_spawn" && tracedAgents.find(e.agentId) == tracedAgents.end()) {
            session.agentSpawns++;
            tracedAgents.insert(e.agentId);
        }
        else if (e.eventType == "chain_start") {
            session.chainExecutions++;
            tracedChains.insert(e.parentId.empty() ? e.agentId : e.parentId);
        }
        else if (e.eventType == "swarm_start") {
            session.swarmExecutions++;
            tracedSwarms.insert(e.parentId.empty() ? e.agentId : e.parentId);
        }

        if (!e.success) session.failures++;
    }

    // Build traces for roots (limit to first 50 to avoid explosion)
    int traceLimit = 50;
    for (const auto& agentId : tracedAgents) {
        if ((int)session.traces.size() >= traceLimit) break;
        session.traces.push_back(traceAgent(agentId));
    }
    for (const auto& chainId : tracedChains) {
        if ((int)session.traces.size() >= traceLimit) break;
        session.traces.push_back(traceChain(chainId));
    }
    for (const auto& swarmId : tracedSwarms) {
        if ((int)session.traces.size() >= traceLimit) break;
        session.traces.push_back(traceSwarm(swarmId));
    }

    // Count retries across all traces
    for (const auto& t : session.traces) {
        session.policyFirings += t.policyFireCount;
    }

    // Failure attributions
    session.failureAttributions = explainFailures();
    session.retries = 0;
    for (const auto& fa : session.failureAttributions) {
        if (fa.wasRetried) session.retries++;
    }

    // Policy attributions
    session.policyAttributions = explainPolicies();

    // Build narrative
    session.narrative = buildNarrative(session);

    return session;
}

// ============================================================================
// generateSnapshot — portable JSON export
// ============================================================================

ExplainabilitySnapshot ExplainabilityEngine::generateSnapshot() const {
    ExplainabilitySnapshot snap;
    snap.version       = "1.0";
    snap.generatedAt   = nowISO8601();
    snap.engineVersion = "7.3.0";

    snap.session = explainSession();

    // Raw data for reproducibility
    if (m_historyRecorder) {
        auto allEvents = m_historyRecorder->allEvents();
        snap.rawEventsJSON = m_historyRecorder->toJSON(allEvents);
    }
    if (m_policyEngine) {
        snap.rawPoliciesJSON  = m_policyEngine->exportPolicies();
        snap.rawHeuristicsJSON = m_policyEngine->heuristicsSummaryJSON();
    }

    return snap;
}

// ============================================================================
// exportSnapshot — write snapshot to file
// ============================================================================

bool ExplainabilityEngine::exportSnapshot(const std::string& filePath) const {
    ExplainabilitySnapshot snap = generateSnapshot();
    std::ofstream file(filePath);
    if (!file.is_open()) {
        logError("Cannot open file for snapshot: " + filePath);
        return false;
    }
    file << snap.toJSON();
    file.close();
    logInfo("Snapshot exported to " + filePath);
    return true;
}

// ============================================================================
// summarizeAgent — one-line summary for CLI
// ============================================================================

std::string ExplainabilityEngine::summarizeAgent(const std::string& agentId) const {
    DecisionTrace trace = traceAuto(agentId);
    return trace.summary;
}

// ============================================================================
// summarizeFailures — quick failure summary
// ============================================================================

std::string ExplainabilityEngine::summarizeFailures() const {
    auto failures = explainFailures();
    if (failures.empty()) return "No failures in current session.";

    std::ostringstream ss;
    ss << failures.size() << " failure(s):\n";
    int retried = 0, policyAssisted = 0;
    for (const auto& f : failures) {
        if (f.wasRetried) retried++;
        if (!f.policyId.empty()) policyAssisted++;
    }
    ss << "  Retried: " << retried << "\n";
    ss << "  Policy-assisted: " << policyAssisted << "\n";

    for (const auto& f : failures) {
        ss << "  [" << f.failureType << "] " << f.agentId
           << ": " << f.errorMessage;
        if (f.wasRetried) ss << " → retried (" << (f.retrySucceeded ? "success" : "failed") << ")";
        if (!f.policyId.empty()) ss << " [policy: " << f.policyName << "]";
        ss << "\n";
    }
    return ss.str();
}

// ============================================================================
// summarizePolicies — quick policy impact summary
// ============================================================================

std::string ExplainabilityEngine::summarizePolicies() const {
    auto policyAttrs = explainPolicies();
    if (policyAttrs.empty()) return "No policies have fired in this session.";

    std::ostringstream ss;
    ss << policyAttrs.size() << " active policy(ies):\n";
    for (const auto& pa : policyAttrs) {
        ss << "  [" << pa.policyName << "] applied " << pa.policyAppliedCount << " time(s)\n";
        ss << "    Effect: " << pa.effectDescription << "\n";
        if (pa.redirectedSwarmToChain)
            ss << "    ⚡ Redirected swarm → chain\n";
    }
    return ss.str();
}
