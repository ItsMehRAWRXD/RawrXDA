#include "session_state.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_set>

namespace {

using rawrxd::ExecutionPlan;
using rawrxd::ExecutionPlanIR;
using rawrxd::PlanGraph;
using rawrxd::PlanGraphEdge;
using rawrxd::PlanGraphNode;
using rawrxd::PlanStep;
using rawrxd::StepCondition;
using rawrxd::StepFileRead;
using rawrxd::StepLLMSynthesize;
using rawrxd::StepTerminalExecute;

constexpr size_t kMaxHistoryRecords = 64;
constexpr size_t kMaxInteractions = 128;
constexpr size_t kMaxPlanGraphNodes = 128;
constexpr size_t kMaxContinuityPreviewChars = 220;

#define SAFE_LOOKUP(m, k) \
    ([&]() -> decltype(auto) { \
        auto it = (m).find(k); \
        if (it == (m).end()) { \
            __debugbreak(); \
        } \
        return it->second; \
    })()

std::string NormalizeInlineWhitespace(const std::string& text);
std::string NormalizeField(const std::string& text);
void TrimPlanGraph(PlanGraph* graph);

std::string NowTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm timeParts{};
#ifdef _WIN32
    localtime_s(&timeParts, &now);
#else
    localtime_r(&now, &timeParts);
#endif
    std::ostringstream stream;
    stream << std::put_time(&timeParts, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string EffectiveSessionId(std::string sessionId) {
    if (sessionId.empty()) {
        return "default-session";
    }
    return sessionId;
}

std::string MakePlanNodeId(const std::string& planId, int sequence) {
    return planId + "#" + std::to_string(sequence);
}

std::string NormalizeSourceKind(std::string sourceKind) {
    sourceKind = NormalizeInlineWhitespace(sourceKind);
    std::transform(sourceKind.begin(), sourceKind.end(), sourceKind.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return sourceKind;
}

std::string StepCanonicalKey(const PlanStep& step) {
    return std::visit([](const auto& concrete) -> std::string {
        using T = std::decay_t<decltype(concrete)>;
        if constexpr (std::is_same_v<T, StepFileRead>) {
            return "file:" + NormalizeField(concrete.path) + "->" + NormalizeField(concrete.alias);
        } else if constexpr (std::is_same_v<T, StepTerminalExecute>) {
            return "terminal:" + NormalizeField(concrete.command) + "->" + NormalizeField(concrete.alias) +
                ":" + std::to_string(concrete.timeoutMs) + ":" + (concrete.captureOutput ? "1" : "0");
        } else if constexpr (std::is_same_v<T, StepLLMSynthesize>) {
            return "llm:" + NormalizeField(concrete.promptTemplate) + "->" + NormalizeField(concrete.alias) +
                ":" + std::to_string(concrete.maxTokens) + ":" + (concrete.optional ? "1" : "0");
        } else if constexpr (std::is_same_v<T, StepCondition>) {
            return "cond:" + NormalizeField(concrete.lhs) + ":" + NormalizeField(concrete.op) + ":" +
                NormalizeField(concrete.rhs) + ":" + std::to_string(concrete.trueBranchIndex) + ":" +
                std::to_string(concrete.falseBranchIndex);
        }
        return std::string{};
    }, step);
}

std::string BuildPlanCanonicalKey(const std::string& sourceKind,
                                  const ExecutionPlanIR& plan,
                                  const std::string& summary) {
    std::ostringstream stream;
    stream << NormalizeSourceKind(sourceKind) << "|" << NormalizeField(plan.planId) << "|" << NormalizeField(summary);
    for (const auto& step : plan.steps) {
        stream << "|" << StepCanonicalKey(step);
    }
    return stream.str();
}

void NormalizePlanGraph(PlanGraph* graph) {
    if (graph == nullptr) {
        return;
    }

    std::map<std::string, PlanGraphNode> deduped;
    std::vector<std::string> order;
    order.reserve(graph->nodes.size());

    for (auto node : graph->nodes) {
        if (node.sessionId.empty()) {
            node.sessionId = graph->sessionId;
        }
        node.sourceKind = NormalizeSourceKind(node.sourceKind);
        node.summary = NormalizeField(node.summary);
        node.outputPreview = NormalizeField(node.outputPreview);
        node.verification.summary = NormalizeField(node.verification.summary);
        if (node.planId.empty()) {
            node.planId = node.nodeId.empty() ? "plan" : node.nodeId;
        }
        node.canonicalKey = BuildPlanCanonicalKey(node.sourceKind, node.plan, node.summary);

        const std::string dedupeKey = !node.planId.empty() ? node.planId : node.canonicalKey;
        auto it = deduped.find(dedupeKey);
        if (it == deduped.end()) {
            order.push_back(dedupeKey);
            deduped.emplace(dedupeKey, std::move(node));
            continue;
        }

        PlanGraphNode& existing = it->second;
        existing.summary = existing.summary.size() >= node.summary.size() ? existing.summary : node.summary;
        existing.outputPreview = node.outputPreview.empty() ? existing.outputPreview : node.outputPreview;
        existing.executable = existing.executable || node.executable;
        existing.completed = existing.completed || node.completed;
        existing.success = node.completed ? node.success : existing.success;
        existing.timestamp = node.timestamp.empty() ? existing.timestamp : node.timestamp;
        existing.version = std::max(existing.version, node.version) + 1;
        if (!node.parentNodeId.empty()) {
            existing.parentNodeId = node.parentNodeId;
        }
        if (existing.plan.steps.empty() && !node.plan.steps.empty()) {
            existing.plan = node.plan;
        }
        if (node.verification.attempted) {
            existing.verification = node.verification;
        }
        existing.canonicalKey = BuildPlanCanonicalKey(existing.sourceKind, existing.plan, existing.summary);
    }

    std::vector<PlanGraphNode> normalizedNodes;
    normalizedNodes.reserve(order.size());
    std::unordered_set<std::string> validNodeIds;
    for (const auto& key : order) {
        auto it = deduped.find(key);
        if (it == deduped.end()) {
            continue;
        }
        PlanGraphNode node = std::move(it->second);
        if (!node.plan.Validate()) {
            node.executable = false;
        }
        normalizedNodes.push_back(std::move(node));
    }

    for (size_t i = 0; i < normalizedNodes.size(); ++i) {
        PlanGraphNode& node = normalizedNodes[i];
        node.sequence = static_cast<int>(i);
        node.nodeId = MakePlanNodeId(node.planId, node.sequence);
        if (i == 0) {
            node.parentNodeId.clear();
        } else {
            bool parentLooksLikeNodeId = node.parentNodeId.find('#') != std::string::npos;
            if (node.parentNodeId.empty() || !parentLooksLikeNodeId) {
                node.parentNodeId = normalizedNodes[i - 1].nodeId;
            }
        }
        validNodeIds.insert(node.nodeId);
    }

    for (size_t i = 1; i < normalizedNodes.size(); ++i) {
        if (normalizedNodes[i].parentNodeId.empty() || validNodeIds.count(normalizedNodes[i].parentNodeId) == 0U) {
            normalizedNodes[i].parentNodeId = normalizedNodes[i - 1].nodeId;
        }
    }

    std::vector<PlanGraphEdge> normalizedEdges;
    std::unordered_set<std::string> seenEdges;
    for (size_t i = 0; i < normalizedNodes.size(); ++i) {
        const PlanGraphNode& node = normalizedNodes[i];
        if (!node.parentNodeId.empty()) {
            const std::string edgeKey = node.parentNodeId + "->" + node.nodeId + ":depends_on";
            if (seenEdges.insert(edgeKey).second) {
                normalizedEdges.push_back({node.parentNodeId, node.nodeId, "depends_on"});
            }
        }
        if (i > 0) {
            const std::string edgeKey = normalizedNodes[i - 1].nodeId + "->" + node.nodeId + ":sequence";
            if (seenEdges.insert(edgeKey).second) {
                normalizedEdges.push_back({normalizedNodes[i - 1].nodeId, node.nodeId, "sequence"});
            }
        }
    }

    graph->nodes = std::move(normalizedNodes);
    graph->edges = std::move(normalizedEdges);
    TrimPlanGraph(graph);
}

void UpdatePlanOutcomeLocked(PlanGraph* graph,
                             const std::string& planId,
                             bool success,
                             const std::string& outputPreview) {
    if (graph == nullptr || planId.empty()) {
        return;
    }

    for (auto it = graph->nodes.rbegin(); it != graph->nodes.rend(); ++it) {
        if (it->planId == planId) {
            it->completed = true;
            it->success = success;
            it->outputPreview = NormalizeField(outputPreview);
            if (it->timestamp.empty()) {
                it->timestamp = NowTimestamp();
            }
            ++it->version;
            NormalizePlanGraph(graph);
            return;
        }
    }
}

void TrimPlanGraph(PlanGraph* graph) {
    if (graph == nullptr) {
        return;
    }
    while (graph->nodes.size() > kMaxPlanGraphNodes) {
        const std::string removedNodeId = graph->nodes.front().nodeId;
        graph->nodes.erase(graph->nodes.begin());
        graph->edges.erase(
            std::remove_if(graph->edges.begin(),
                           graph->edges.end(),
                           [&](const PlanGraphEdge& edge) {
                               return edge.fromNodeId == removedNodeId || edge.toNodeId == removedNodeId;
                           }),
            graph->edges.end());
    }
}

std::string NormalizeInlineWhitespace(const std::string& text) {
    std::string normalized;
    normalized.reserve(text.size());
    bool previousWasSpace = false;
    for (const char ch : text) {
        const bool isSpace = ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
        if (isSpace) {
            if (!previousWasSpace) {
                normalized.push_back(' ');
                previousWasSpace = true;
            }
            continue;
        }
        normalized.push_back(ch);
        previousWasSpace = false;
    }
    return normalized;
}

std::string NormalizeField(const std::string& text) {
    return NormalizeInlineWhitespace(text);
}

std::string TruncateForPrompt(const std::string& text, size_t maxChars) {
    const std::string normalized = NormalizeInlineWhitespace(text);
    if (normalized.size() <= maxChars) {
        return normalized;
    }
    return normalized.substr(0, maxChars) + "...";
}

}  // namespace

namespace rawrxd {

bool PlanGraph::Validate() const {
    std::unordered_set<std::string> nodeIds;
    std::unordered_set<std::string> planIds;
    for (const auto& node : nodes) {
        if (node.nodeId.empty() || node.planId.empty() || node.sessionId.empty()) {
            return false;
        }
        if (!nodeIds.insert(node.nodeId).second) {
            return false;
        }
        if (!planIds.insert(node.planId).second) {
            return false;
        }
        if (node.sequence < 0) {
            return false;
        }
        if (!node.plan.Validate()) {
            return false;
        }
        if (!node.parentNodeId.empty() && node.sequence != 0 && nodeIds.count(node.parentNodeId) == 0U) {
            return false;
        }
    }

    for (const auto& edge : edges) {
        if (edge.fromNodeId.empty() || edge.toNodeId.empty() || edge.kind.empty()) {
            return false;
        }
        if (nodeIds.count(edge.fromNodeId) == 0U || nodeIds.count(edge.toNodeId) == 0U) {
            return false;
        }
    }

    return true;
}

void SessionState::BindWorkspaceSnapshot(const WorkspaceSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = snapshot;
}

void SessionState::RecordInteraction(const UserInteraction& interaction) {
    std::lock_guard<std::mutex> lock(mutex_);
    UserInteraction copy = interaction;
    if (copy.timestamp.empty()) {
        copy.timestamp = NowTimestamp();
    }
    interactions_.push_back(std::move(copy));
    if (interactions_.size() > kMaxInteractions) {
        interactions_.erase(interactions_.begin());
    }
}

void SessionState::AppendExecution(const ExecutionRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    ExecutionRecord copy = record;
    if (copy.timestamp.empty()) {
        copy.timestamp = NowTimestamp();
    }
    history_.push_back(std::move(copy));
    if (history_.size() > kMaxHistoryRecords) {
        history_.erase(history_.begin());
    }

    if (!record.planId.empty()) {
        const std::string effectiveSessionId = EffectiveSessionId(record.sessionId);
        auto graphIt = plan_graphs_.find(effectiveSessionId);
        if (graphIt != plan_graphs_.end()) {
            UpdatePlanOutcomeLocked(&graphIt->second,
                                    record.planId,
                                    record.result.success,
                                    record.result.success ? record.result.output : record.result.error);
        }
    }
}

void SessionState::AppendPlanIR(const std::string& sessionId,
                                const ExecutionPlanIR& plan,
                                const std::string& sourceKind,
                                const std::string& summary,
                                const std::string& rootGoal) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string effectiveSessionId = EffectiveSessionId(sessionId);
    PlanGraph& graph = plan_graphs_[effectiveSessionId];
    graph.sessionId = effectiveSessionId;
    if (graph.rootGoal.empty() && !rootGoal.empty()) {
        graph.rootGoal = rootGoal;
    }

    PlanGraphNode node;
    node.sequence = static_cast<int>(graph.nodes.size());
    node.nodeId = MakePlanNodeId(plan.planId.empty() ? "plan" : plan.planId, node.sequence);
    node.planId = plan.planId.empty() ? node.nodeId : plan.planId;
    node.canonicalKey = BuildPlanCanonicalKey(sourceKind, plan, summary);
    node.parentNodeId = graph.nodes.empty() ? "" : graph.nodes.back().nodeId;
    node.sessionId = effectiveSessionId;
    node.sourceKind = NormalizeSourceKind(sourceKind);
    node.summary = NormalizeField(summary);
    node.timestamp = NowTimestamp();
    node.executable = plan.Validate();
    node.plan = plan;

    bool updatedExisting = false;
    for (auto& existing : graph.nodes) {
        if ((!node.planId.empty() && existing.planId == node.planId) ||
            (!node.canonicalKey.empty() && existing.canonicalKey == node.canonicalKey)) {
            existing.sourceKind = node.sourceKind;
            existing.summary = node.summary;
            existing.timestamp = node.timestamp;
            existing.executable = node.executable;
            existing.plan = node.plan;
            existing.canonicalKey = node.canonicalKey;
            ++existing.version;
            updatedExisting = true;
            break;
        }
    }

    if (!updatedExisting) {
        graph.nodes.push_back(std::move(node));
    }

    NormalizePlanGraph(&graph);
}

void SessionState::UpdatePlanOutcome(const std::string& sessionId,
                                     const std::string& planId,
                                     bool success,
                                     const std::string& outputPreview) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string effectiveSessionId = EffectiveSessionId(sessionId);
    auto graphIt = plan_graphs_.find(effectiveSessionId);
    if (graphIt == plan_graphs_.end()) {
        return;
    }

    UpdatePlanOutcomeLocked(&graphIt->second, planId, success, outputPreview);
}

void SessionState::AppendVerificationOutcome(const std::string& sessionId,
                                             const std::string& planId,
                                             const std::string& verifierKind,
                                             bool passed,
                                             const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string effectiveSessionId = EffectiveSessionId(sessionId);
    auto graphIt = plan_graphs_.find(effectiveSessionId);
    if (graphIt == plan_graphs_.end()) {
        return;
    }

    auto& nodes = graphIt->second.nodes;
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (it->planId == planId) {
            const std::string normalizedKind = NormalizeSourceKind(verifierKind);
            const std::string normalizedSummary = NormalizeField(summary);
            if (!it->verification.attempted) {
                it->verification.attempted = true;
                it->verification.passed = passed;
                it->verification.verifierKind = normalizedKind;
                it->verification.summary = normalizedSummary;
            } else {
                it->verification.passed = it->verification.passed && passed;
                if (!normalizedKind.empty() && it->verification.verifierKind.find(normalizedKind) == std::string::npos) {
                    if (!it->verification.verifierKind.empty()) {
                        it->verification.verifierKind += ",";
                    }
                    it->verification.verifierKind += normalizedKind;
                }
                if (!normalizedSummary.empty()) {
                    if (!it->verification.summary.empty()) {
                        it->verification.summary += " | ";
                    }
                    it->verification.summary += normalizedSummary;
                }
            }
            it->verification.timestamp = NowTimestamp();
            ++it->version;
            NormalizePlanGraph(&graphIt->second);
            break;
        }
    }
}

ReviewablePlan SessionState::BuildReviewablePlan(const ExecutionPlan& plan,
                                                 const std::string& preview,
                                                 bool requiresApproval) const {
    return ReviewablePlan{plan, preview, requiresApproval};
}

PlanGraph SessionState::planGraph(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string effectiveSessionId = EffectiveSessionId(sessionId);
    const auto it = plan_graphs_.find(effectiveSessionId);
    if (it == plan_graphs_.end()) {
        return PlanGraph{effectiveSessionId, "", {}, {}};
    }
    PlanGraph copy = it->second;
    NormalizePlanGraph(&copy);
    return copy;
}

std::string SessionState::BuildPlanContinuityContext(const std::string& sessionId,
                                                     size_t maxRecords) const {
    if (maxRecords == 0) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string effectiveSessionId = EffectiveSessionId(sessionId);
    const auto it = plan_graphs_.find(effectiveSessionId);
    if (it == plan_graphs_.end() || it->second.nodes.empty()) {
        return {};
    }

    PlanGraph normalized = it->second;
    NormalizePlanGraph(&normalized);
    if (normalized.nodes.empty()) {
        return {};
    }

    std::vector<const PlanGraphNode*> nodes;
    nodes.reserve(maxRecords);
    for (auto nodeIt = normalized.nodes.rbegin(); nodeIt != normalized.nodes.rend() && nodes.size() < maxRecords; ++nodeIt) {
        if (nodeIt->planId.empty()) {
            continue;
        }
        nodes.push_back(&(*nodeIt));
    }
    if (nodes.empty()) {
        return {};
    }

    std::ostringstream stream;
    stream << "Recent execution continuity:\n";
    for (auto nodeIt = nodes.rbegin(); nodeIt != nodes.rend(); ++nodeIt) {
        const PlanGraphNode& node = *(*nodeIt);
        stream << "- PlanIR " << node.planId
               << " version=" << node.version
               << " source=" << node.sourceKind
               << " executable=" << (node.executable ? "yes" : "no")
               << " completed=" << (node.completed ? "yes" : "no")
               << " success=" << (node.success ? "yes" : "no")
               << " timestamp=" << node.timestamp << "\n";
        if (node.verification.attempted) {
            stream << "  Verification: kind=" << node.verification.verifierKind
                   << " passed=" << (node.verification.passed ? "yes" : "no") << "\n";
            if (!node.verification.summary.empty()) {
                stream << "  Verification Summary: "
                       << TruncateForPrompt(node.verification.summary, kMaxContinuityPreviewChars)
                       << "\n";
            }
        }
        if (!node.summary.empty()) {
            stream << "  Preview: " << TruncateForPrompt(node.summary, kMaxContinuityPreviewChars) << "\n";
        }
        if (!node.outputPreview.empty()) {
            stream << "  Output: " << TruncateForPrompt(node.outputPreview, kMaxContinuityPreviewChars) << "\n";
        }
    }

    return stream.str();
}

WorkspaceSnapshot SessionState::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

std::vector<ExecutionRecord> SessionState::history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

std::vector<UserInteraction> SessionState::interactions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interactions_;
}

}  // namespace rawrxd
