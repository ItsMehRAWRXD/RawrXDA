#pragma once

#include "../agentic/execution_contracts.h"
#include "../agentic/execution_plan_ir.h"
#include "../context/workspace_context.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd {

struct UserInteraction {
    std::string kind;
    std::string primaryText;
    std::string secondaryText;
    std::string filePath;
    std::string sessionId;
    std::string timestamp;
};

struct ExecutionResult {
    bool success = false;
    std::string output;
    std::string error;
};

struct ExecutionRecord {
    ExecutionPlan plan;
    ExecutionResult result;
    TrustScore trust = TrustScore::ModelSuspect;
    int iterationCount = 0;
    std::string sessionId;
    std::string timestamp;
    std::string planId;
};

struct ReviewablePlan {
    ExecutionPlan plan;
    std::string preview;
    bool requiresApproval = true;
};

struct PlanVerification {
    bool attempted = false;
    bool passed = false;
    std::string verifierKind;
    std::string summary;
    std::string timestamp;
};

struct PlanGraphNode {
    std::string nodeId;
    std::string planId;
    std::string canonicalKey;
    std::string parentNodeId;
    std::string sessionId;
    std::string sourceKind;
    std::string summary;
    std::string outputPreview;
    std::string timestamp;
    int sequence = 0;
    int version = 1;
    bool executable = false;
    bool completed = false;
    bool success = false;
    PlanVerification verification;
    ExecutionPlanIR plan;
};

struct PlanGraphEdge {
    std::string fromNodeId;
    std::string toNodeId;
    std::string kind;
};

struct PlanGraph {
    std::string sessionId;
    std::string rootGoal;
    std::vector<PlanGraphNode> nodes;
    std::vector<PlanGraphEdge> edges;

    bool Validate() const;
};

class SessionState {
  public:
    void BindWorkspaceSnapshot(const WorkspaceSnapshot& snapshot);
    void RecordInteraction(const UserInteraction& interaction);
    void AppendExecution(const ExecutionRecord& record);
    void AppendPlanIR(const std::string& sessionId,
                      const ExecutionPlanIR& plan,
                      const std::string& sourceKind,
                      const std::string& summary,
                      const std::string& rootGoal = "");
    void UpdatePlanOutcome(const std::string& sessionId,
                           const std::string& planId,
                           bool success,
                           const std::string& outputPreview);
    void AppendVerificationOutcome(const std::string& sessionId,
                                   const std::string& planId,
                                   const std::string& verifierKind,
                                   bool passed,
                                   const std::string& summary);
    ReviewablePlan BuildReviewablePlan(const ExecutionPlan& plan,
                                       const std::string& preview,
                                       bool requiresApproval) const;
    PlanGraph planGraph(const std::string& sessionId) const;
    std::string BuildPlanContinuityContext(const std::string& sessionId,
                                           size_t maxRecords = 3) const;
    WorkspaceSnapshot snapshot() const;
    std::vector<ExecutionRecord> history() const;
    std::vector<UserInteraction> interactions() const;

  private:
    WorkspaceSnapshot snapshot_;
    std::vector<ExecutionRecord> history_;
    std::vector<UserInteraction> interactions_;
    std::unordered_map<std::string, PlanGraph> plan_graphs_;
    mutable std::mutex mutex_;
};

}  // namespace rawrxd
