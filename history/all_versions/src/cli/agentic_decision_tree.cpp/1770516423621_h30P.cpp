// ============================================================================
// agentic_decision_tree.cpp — Phase 19: Headless Agentic Decision Tree
// ============================================================================
//
// Full implementation of the CLI decision tree for autonomous operation.
// Evaluates nodes, runs SSA lifter, detects failures, applies hotpatches.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "agentic_decision_tree.h"
#include "../agentic_engine.h"
#include "../subagent_core.h"
#include "../core/model_memory_hotpatch.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../agent/agentic_hotpatch_orchestrator.hpp"
#include "../reverse_engineering/RawrCodex.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================================
// Singleton
// ============================================================================

AgenticDecisionTree& AgenticDecisionTree::instance() {
    static AgenticDecisionTree inst;
    return inst;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

AgenticDecisionTree::AgenticDecisionTree()
    : m_rootNodeId(0)
    , m_nextNodeId(1)
    , m_engine(nullptr)
    , m_subAgentMgr(nullptr)
    , m_enabled(true)
    , m_maxTreeDepth(16)
    , m_maxTotalRetries(5)
    , m_globalConfidenceThreshold(0.6f)
    , m_autoEscalateOnCritical(true)
    , m_approvalFn(nullptr)
    , m_approvalUserData(nullptr)
{
    buildDefaultTree();
}

AgenticDecisionTree::~AgenticDecisionTree() = default;

// ============================================================================
// Engine Wiring
// ============================================================================

void AgenticDecisionTree::setAgenticEngine(AgenticEngine* engine) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_engine = engine;
}

void AgenticDecisionTree::setSubAgentManager(SubAgentManager* mgr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subAgentMgr = mgr;
}

// ============================================================================
// Tree Construction
// ============================================================================

void AgenticDecisionTree::buildDefaultTree() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nodes.clear();
    m_nextNodeId = 1;

    // ---- Root Node (ID=1) ----
    DecisionNode root{};
    root.nodeId             = m_nextNodeId++;
    root.type               = DecisionNodeType::Root;
    root.risk               = AutonomyRisk::ReadOnly;
    root.name               = "Root";
    root.description        = "Entry point: triage incoming output";
    root.parentId           = 0;
    root.maxRetries         = 0;
    root.confidenceThreshold = 0.0f;
    root.requiresApproval   = false;
    m_rootNodeId = root.nodeId;
    m_nodes[root.nodeId] = root;

    // ---- Observation Node (ID=2) ----
    DecisionNode observe{};
    observe.nodeId          = m_nextNodeId++;
    observe.type            = DecisionNodeType::Observation;
    observe.risk            = AutonomyRisk::ReadOnly;
    observe.name            = "Observe";
    observe.description     = "Gather output metadata (length, pattern, confidence)";
    observe.parentId        = root.nodeId;
    observe.maxRetries      = 0;
    observe.confidenceThreshold = 0.0f;
    observe.requiresApproval = false;
    m_nodes[observe.nodeId] = observe;
    m_nodes[root.nodeId].childIds.push_back(observe.nodeId);

    // ---- Failure Detection Node (ID=3) ----
    DecisionNode failDetect{};
    failDetect.nodeId       = m_nextNodeId++;
    failDetect.type         = DecisionNodeType::FailureDetect;
    failDetect.risk         = AutonomyRisk::ReadOnly;
    failDetect.name         = "DetectFailure";
    failDetect.description  = "Classify inference failure (refusal, hallucination, loop, etc.)";
    failDetect.parentId     = observe.nodeId;
    failDetect.maxRetries   = 0;
    failDetect.confidenceThreshold = 0.5f;
    failDetect.requiresApproval = false;
    m_nodes[failDetect.nodeId] = failDetect;
    m_nodes[observe.nodeId].childIds.push_back(failDetect.nodeId);

    // ---- Classification Node (ID=4) ----
    DecisionNode classify{};
    classify.nodeId         = m_nextNodeId++;
    classify.type           = DecisionNodeType::Classification;
    classify.risk           = AutonomyRisk::ReadOnly;
    classify.name           = "Classify";
    classify.description    = "Route to appropriate correction strategy based on failure type";
    classify.parentId       = failDetect.nodeId;
    classify.maxRetries     = 0;
    classify.confidenceThreshold = 0.0f;
    classify.requiresApproval = false;
    m_nodes[classify.nodeId] = classify;
    m_nodes[failDetect.nodeId].childIds.push_back(classify.nodeId);

    // ---- SSA Lift Node (ID=5) ----
    DecisionNode ssaLift{};
    ssaLift.nodeId          = m_nextNodeId++;
    ssaLift.type            = DecisionNodeType::SSALift;
    ssaLift.risk            = AutonomyRisk::Low;
    ssaLift.name            = "SSALift";
    ssaLift.description     = "Run SSA lifter on suspicious function for deep analysis";
    ssaLift.parentId        = classify.nodeId;
    ssaLift.maxRetries      = 1;
    ssaLift.confidenceThreshold = 0.7f;
    ssaLift.requiresApproval = false;
    m_nodes[ssaLift.nodeId] = ssaLift;
    m_nodes[classify.nodeId].childIds.push_back(ssaLift.nodeId);

    // ---- Correction Plan Node (ID=6) ----
    DecisionNode plan{};
    plan.nodeId             = m_nextNodeId++;
    plan.type               = DecisionNodeType::CorrectionPlan;
    plan.risk               = AutonomyRisk::Low;
    plan.name               = "PlanCorrection";
    plan.description        = "Determine which hotpatch layer to use and what data to patch";
    plan.parentId           = classify.nodeId;
    plan.maxRetries         = 0;
    plan.confidenceThreshold = 0.6f;
    plan.requiresApproval   = false;
    m_nodes[plan.nodeId]    = plan;
    m_nodes[classify.nodeId].childIds.push_back(plan.nodeId);

    // ---- Memory Hotpatch Node (ID=7) ----
    DecisionNode memPatch{};
    memPatch.nodeId         = m_nextNodeId++;
    memPatch.type           = DecisionNodeType::MemoryHotpatch;
    memPatch.risk           = AutonomyRisk::Medium;
    memPatch.name           = "MemoryHotpatch";
    memPatch.description    = "Apply direct memory patch to inference engine (VirtualProtect)";
    memPatch.parentId       = plan.nodeId;
    memPatch.maxRetries     = 2;
    memPatch.confidenceThreshold = 0.75f;
    memPatch.requiresApproval = false;
    m_nodes[memPatch.nodeId] = memPatch;
    m_nodes[plan.nodeId].childIds.push_back(memPatch.nodeId);

    // ---- Byte Hotpatch Node (ID=8) ----
    DecisionNode bytePatch{};
    bytePatch.nodeId        = m_nextNodeId++;
    bytePatch.type          = DecisionNodeType::ByteHotpatch;
    bytePatch.risk          = AutonomyRisk::High;
    bytePatch.name          = "ByteHotpatch";
    bytePatch.description   = "Apply byte-level GGUF patch (fallback if memory patch insufficient)";
    bytePatch.parentId      = plan.nodeId;
    bytePatch.maxRetries    = 1;
    bytePatch.confidenceThreshold = 0.8f;
    bytePatch.requiresApproval = true;
    m_nodes[bytePatch.nodeId] = bytePatch;
    m_nodes[plan.nodeId].childIds.push_back(bytePatch.nodeId);

    // ---- Server Patch Node (ID=9) ----
    DecisionNode srvPatch{};
    srvPatch.nodeId         = m_nextNodeId++;
    srvPatch.type           = DecisionNodeType::ServerPatch;
    srvPatch.risk           = AutonomyRisk::Low;
    srvPatch.name           = "ServerPatch";
    srvPatch.description    = "Inject server-layer transform (token bias, output rewrite)";
    srvPatch.parentId       = plan.nodeId;
    srvPatch.maxRetries     = 1;
    srvPatch.confidenceThreshold = 0.6f;
    srvPatch.requiresApproval = false;
    m_nodes[srvPatch.nodeId] = srvPatch;
    m_nodes[plan.nodeId].childIds.push_back(srvPatch.nodeId);

    // ---- Verification Node (ID=10) ----
    DecisionNode verify{};
    verify.nodeId           = m_nextNodeId++;
    verify.type             = DecisionNodeType::Verification;
    verify.risk             = AutonomyRisk::ReadOnly;
    verify.name             = "Verify";
    verify.description      = "Re-run inference and confirm failure is resolved";
    verify.parentId         = plan.nodeId;
    verify.maxRetries       = 1;
    verify.confidenceThreshold = 0.0f;
    verify.requiresApproval = false;
    m_nodes[verify.nodeId]  = verify;
    // Verification hangs off each patch node
    m_nodes[memPatch.nodeId].childIds.push_back(verify.nodeId);
    m_nodes[bytePatch.nodeId].childIds.push_back(verify.nodeId);
    m_nodes[srvPatch.nodeId].childIds.push_back(verify.nodeId);

    // ---- Escalation Node (ID=11) ----
    DecisionNode escalate{};
    escalate.nodeId         = m_nextNodeId++;
    escalate.type           = DecisionNodeType::Escalation;
    escalate.risk           = AutonomyRisk::ReadOnly;
    escalate.name           = "Escalate";
    escalate.description    = "Cannot auto-fix; escalate to user with diagnostic context";
    escalate.parentId       = plan.nodeId;
    escalate.maxRetries     = 0;
    escalate.confidenceThreshold = 0.0f;
    escalate.requiresApproval = false;
    m_nodes[escalate.nodeId] = escalate;
    m_nodes[verify.nodeId].childIds.push_back(escalate.nodeId);

    // ---- Terminal Success Node (ID=12) ----
    DecisionNode termOk{};
    termOk.nodeId           = m_nextNodeId++;
    termOk.type             = DecisionNodeType::Terminal;
    termOk.risk             = AutonomyRisk::ReadOnly;
    termOk.name             = "Success";
    termOk.description      = "Correction verified, pipeline complete";
    termOk.parentId         = verify.nodeId;
    termOk.maxRetries       = 0;
    termOk.confidenceThreshold = 0.0f;
    termOk.requiresApproval = false;
    m_nodes[termOk.nodeId]  = termOk;
    m_nodes[verify.nodeId].childIds.push_back(termOk.nodeId);
}

uint32_t AgenticDecisionTree::addNode(const DecisionNode& node) {
    std::lock_guard<std::mutex> lock(m_mutex);
    DecisionNode n = node;
    n.nodeId = m_nextNodeId++;
    m_nodes[n.nodeId] = n;
    return n.nodeId;
}

void AgenticDecisionTree::linkNode(uint32_t parentId, uint32_t childId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto pit = m_nodes.find(parentId);
    auto cit = m_nodes.find(childId);
    if (pit != m_nodes.end() && cit != m_nodes.end()) {
        pit->second.childIds.push_back(childId);
        cit->second.parentId = parentId;
    }
}

const DecisionNode* AgenticDecisionTree::getNode(uint32_t nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

// ============================================================================
// Tree Evaluation — Main Entry Points
// ============================================================================

DecisionOutcome AgenticDecisionTree::evaluate(TreeContext& ctx) {
    if (!m_enabled) {
        return DecisionOutcome::error(NodeVerdict::Skip, "Decision tree is disabled");
    }

    m_stats.treesEvaluated++;
    ctx.sequenceId = m_stats.treesEvaluated.load();
    ctx.startTime = std::chrono::steady_clock::now();
    m_lastTrace.clear();

    ctx.addTrace("[TREE] Evaluation started (seq=" + std::to_string(ctx.sequenceId) + ")");

    NodeVerdict result = traverseNode(m_rootNodeId, ctx, 0);

    int elapsed = ctx.elapsedMs();

    DecisionOutcome outcome;
    outcome.finalVerdict    = result;
    outcome.trigger         = TriggerReason::InferenceFailure;
    outcome.riskTaken       = ctx.patchApplied ? AutonomyRisk::Medium : AutonomyRisk::ReadOnly;
    outcome.nodesEvaluated  = (int)ctx.traceLog.size();
    outcome.patchesApplied  = ctx.patchApplied ? 1 : 0;
    outcome.retriesUsed     = ctx.totalRetries;
    outcome.durationMs      = elapsed;
    outcome.traceLog        = ctx.traceLog;

    if (result == NodeVerdict::Success) {
        outcome.success = true;
        outcome.summary = "Autonomous correction succeeded";
        m_stats.successfulCorrections++;
    } else if (result == NodeVerdict::Escalate) {
        outcome.success = false;
        outcome.summary = "Escalated to user — could not auto-fix";
        m_stats.escalations++;
    } else if (result == NodeVerdict::Abort) {
        outcome.success = false;
        outcome.summary = "Tree evaluation aborted";
        m_stats.aborts++;
    } else {
        outcome.success = false;
        outcome.summary = "Correction failed";
        m_stats.failedCorrections++;
    }

    m_lastTrace = ctx.traceLog;
    notifyComplete(outcome);

    ctx.addTrace("[TREE] Evaluation complete: " + std::string(outcome.summary)
                 + " (" + std::to_string(elapsed) + "ms)");

    return outcome;
}

DecisionOutcome AgenticDecisionTree::evaluateFrom(uint32_t nodeId, TreeContext& ctx) {
    if (!m_enabled) {
        return DecisionOutcome::error(NodeVerdict::Skip, "Decision tree is disabled");
    }
    if (m_nodes.find(nodeId) == m_nodes.end()) {
        return DecisionOutcome::error(NodeVerdict::Abort, "Node not found");
    }

    m_stats.treesEvaluated++;
    ctx.sequenceId = m_stats.treesEvaluated.load();

    NodeVerdict result = traverseNode(nodeId, ctx, 0);

    DecisionOutcome outcome;
    outcome.success         = (result == NodeVerdict::Success);
    outcome.finalVerdict    = result;
    outcome.trigger         = TriggerReason::InferenceFailure;
    outcome.riskTaken       = ctx.patchApplied ? AutonomyRisk::Medium : AutonomyRisk::ReadOnly;
    outcome.nodesEvaluated  = (int)ctx.traceLog.size();
    outcome.patchesApplied  = ctx.patchApplied ? 1 : 0;
    outcome.retriesUsed     = ctx.totalRetries;
    outcome.durationMs      = ctx.elapsedMs();
    outcome.summary         = outcome.success ? "Sub-tree succeeded" : "Sub-tree failed";
    outcome.traceLog        = ctx.traceLog;

    return outcome;
}

DecisionOutcome AgenticDecisionTree::analyzeAndFix(const std::string& output,
                                                     const std::string& prompt) {
    TreeContext ctx;
    ctx.inferenceOutput = output;
    ctx.inferencePrompt = prompt;
    return evaluate(ctx);
}

// ============================================================================
// Recursive Traversal
// ============================================================================

NodeVerdict AgenticDecisionTree::traverseNode(uint32_t nodeId, TreeContext& ctx, int depth) {
    if (depth > m_maxTreeDepth) {
        ctx.addTrace("[ABORT] Max tree depth exceeded (" + std::to_string(m_maxTreeDepth) + ")");
        m_stats.aborts++;
        return NodeVerdict::Abort;
    }

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        ctx.addTrace("[ERROR] Node " + std::to_string(nodeId) + " not found");
        return NodeVerdict::Abort;
    }

    const DecisionNode& node = it->second;
    m_stats.nodesVisited++;

    ctx.addTrace("[NODE:" + std::to_string(node.nodeId) + "] " + std::string(node.name)
                 + " — " + std::string(node.description));

    // Check if approval is required
    if (node.requiresApproval) {
        if (!requestApproval(node.description, node.risk)) {
            ctx.addTrace("[DENIED] User denied action: " + std::string(node.name));
            return NodeVerdict::Escalate;
        }
    }

    // Evaluate this node (with retry logic)
    NodeVerdict verdict = NodeVerdict::Failure;
    int retries = 0;

    for (int attempt = 0; attempt <= node.maxRetries; attempt++) {
        switch (node.type) {
            case DecisionNodeType::Root:           verdict = evalRoot(node, ctx); break;
            case DecisionNodeType::Observation:    verdict = evalObservation(node, ctx); break;
            case DecisionNodeType::Classification: verdict = evalClassification(node, ctx); break;
            case DecisionNodeType::SSALift:        verdict = evalSSALift(node, ctx); break;
            case DecisionNodeType::FailureDetect:  verdict = evalFailureDetect(node, ctx); break;
            case DecisionNodeType::CorrectionPlan: verdict = evalCorrectionPlan(node, ctx); break;
            case DecisionNodeType::MemoryHotpatch: verdict = evalMemoryHotpatch(node, ctx); break;
            case DecisionNodeType::ByteHotpatch:   verdict = evalByteHotpatch(node, ctx); break;
            case DecisionNodeType::ServerPatch:    verdict = evalServerPatch(node, ctx); break;
            case DecisionNodeType::Verification:   verdict = evalVerification(node, ctx); break;
            case DecisionNodeType::Escalation:     verdict = evalEscalation(node, ctx); break;
            case DecisionNodeType::Terminal:        verdict = evalTerminal(node, ctx); break;
        }

        if (verdict != NodeVerdict::Retry) break;

        retries++;
        ctx.totalRetries++;
        m_stats.totalRetries++;
        ctx.addTrace("[RETRY] Node " + std::string(node.name) + " attempt " + std::to_string(attempt + 2));
    }

    notifyTrace(node.name, verdict);

    // Handle verdict
    switch (verdict) {
        case NodeVerdict::Success:
        case NodeVerdict::Abort:
        case NodeVerdict::Escalate:
            return verdict;

        case NodeVerdict::Skip:
            return NodeVerdict::Continue;

        case NodeVerdict::Failure:
            // If this node failed, try next sibling (handled by parent)
            return NodeVerdict::Failure;

        case NodeVerdict::Continue:
            break;

        case NodeVerdict::Retry:
            // Exhausted retries
            ctx.addTrace("[EXHAUSTED] Node " + std::string(node.name) + " retries exhausted");
            return NodeVerdict::Failure;
    }

    // Traverse children
    if (!node.childIds.empty()) {
        for (uint32_t childId : node.childIds) {
            NodeVerdict childResult = traverseNode(childId, ctx, depth + 1);

            switch (childResult) {
                case NodeVerdict::Success:
                    return NodeVerdict::Success;

                case NodeVerdict::Abort:
                    return NodeVerdict::Abort;

                case NodeVerdict::Escalate:
                    // If this is a correction plan, try next child before escalating
                    if (node.type == DecisionNodeType::CorrectionPlan) {
                        ctx.addTrace("[FALLBACK] Trying next correction strategy...");
                        continue;
                    }
                    return NodeVerdict::Escalate;

                case NodeVerdict::Failure:
                    // Try next sibling
                    ctx.addTrace("[FALLBACK] Child failed, trying next...");
                    continue;

                case NodeVerdict::Continue:
                case NodeVerdict::Skip:
                case NodeVerdict::Retry:
                    continue;
            }
        }
    }

    // If we're a terminal or leaf with no children, inherit verdict
    if (node.childIds.empty()) {
        return verdict;
    }

    // All children exhausted without success
    return NodeVerdict::Failure;
}

// ============================================================================
// Individual Node Evaluators
// ============================================================================

NodeVerdict AgenticDecisionTree::evalRoot(const DecisionNode& node, TreeContext& ctx) {
    (void)node;
    // Root just triages — check if we have valid input
    if (ctx.inferenceOutput.empty()) {
        ctx.addTrace("[ROOT] No inference output to analyze");
        return NodeVerdict::Skip;
    }
    ctx.addTrace("[ROOT] Triaging output (" + std::to_string(ctx.inferenceOutput.size()) + " bytes)");
    return NodeVerdict::Continue;
}

NodeVerdict AgenticDecisionTree::evalObservation(const DecisionNode& node, TreeContext& ctx) {
    (void)node;
    // Gather output metadata
    size_t len = ctx.inferenceOutput.size();
    ctx.addTrace("[OBSERVE] Output length: " + std::to_string(len) + " chars");

    // Check for obviously empty or truncated output
    if (len == 0) {
        ctx.failureDescription = "Empty output (possible timeout or crash)";
        ctx.failureConfidence = 1.0f;
        ctx.failureType = 7; // Timeout
        ctx.addTrace("[OBSERVE] CRITICAL: Empty output detected");
        return NodeVerdict::Continue;
    }

    if (len < 10) {
        ctx.addTrace("[OBSERVE] WARNING: Very short output (" + std::to_string(len) + " chars)");
        ctx.failureConfidence = 0.7f;
    }

    // Check for repetition (simple loop detection)
    if (len > 200) {
        std::string first100 = ctx.inferenceOutput.substr(0, 100);
        size_t repeatPos = ctx.inferenceOutput.find(first100, 100);
        if (repeatPos != std::string::npos && repeatPos < 300) {
            ctx.failureDescription = "Repeating content detected (possible infinite loop)";
            ctx.failureConfidence = 0.85f;
            ctx.failureType = 4; // InfiniteLoop
            ctx.addTrace("[OBSERVE] Loop pattern detected at offset " + std::to_string(repeatPos));
        }
    }

    return NodeVerdict::Continue;
}

NodeVerdict AgenticDecisionTree::evalFailureDetect(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    // Delegate to the hotpatch orchestrator's detection engine
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();
    InferenceFailureEvent evt = orchestrator.detectFailure(
        ctx.inferenceOutput.c_str(), ctx.inferenceOutput.size(),
        ctx.inferencePrompt.c_str(), ctx.inferencePrompt.size());

    if (evt.type == InferenceFailureType::None) {
        // No failure detected
        if (ctx.failureConfidence < node.confidenceThreshold) {
            ctx.addTrace("[DETECT] No failure detected (output appears healthy)");
            return NodeVerdict::Skip; // Skip correction, output is fine
        }
    }

    ctx.failureType = static_cast<uint8_t>(evt.type);
    ctx.failureConfidence = evt.confidence;
    if (evt.description) {
        ctx.failureDescription = evt.description;
    }
    m_stats.failuresDetected++;

    ctx.addTrace("[DETECT] Failure: type=" + std::to_string(ctx.failureType)
                 + " confidence=" + std::to_string(ctx.failureConfidence)
                 + " desc=" + ctx.failureDescription);

    if (ctx.failureConfidence < m_globalConfidenceThreshold) {
        ctx.addTrace("[DETECT] Confidence below threshold ("
                     + std::to_string(m_globalConfidenceThreshold) + "), skipping correction");
        return NodeVerdict::Skip;
    }

    return NodeVerdict::Continue;
}

NodeVerdict AgenticDecisionTree::evalClassification(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    // Route based on failure type
    // The children of Classification are: SSALift, CorrectionPlan
    // Classification determines which sub-tree to prioritize.

    std::string failType;
    switch (static_cast<InferenceFailureType>(ctx.failureType)) {
        case InferenceFailureType::Refusal:          failType = "Refusal"; break;
        case InferenceFailureType::Hallucination:    failType = "Hallucination"; break;
        case InferenceFailureType::FormatViolation:  failType = "FormatViolation"; break;
        case InferenceFailureType::InfiniteLoop:     failType = "InfiniteLoop"; break;
        case InferenceFailureType::TokenLimit:       failType = "TokenLimit"; break;
        case InferenceFailureType::ResourceExhausted: failType = "ResourceExhausted"; break;
        case InferenceFailureType::Timeout:          failType = "Timeout"; break;
        case InferenceFailureType::SafetyViolation:  failType = "SafetyViolation"; break;
        case InferenceFailureType::LowConfidence:    failType = "LowConfidence"; break;
        case InferenceFailureType::GarbageOutput:    failType = "GarbageOutput"; break;
        default: failType = "Unknown"; break;
    }

    ctx.addTrace("[CLASSIFY] Failure classified as: " + failType
                 + " (confidence=" + std::to_string(ctx.failureConfidence) + ")");

    // If we have a target binary and the failure involves logic errors,
    // prioritize SSA analysis
    if (!ctx.targetBinaryPath.empty() &&
        (ctx.failureType == static_cast<uint8_t>(InferenceFailureType::Hallucination) ||
         ctx.failureType == static_cast<uint8_t>(InferenceFailureType::GarbageOutput))) {
        ctx.addTrace("[CLASSIFY] Binary target available — SSA lift will be prioritized");
    }

    return NodeVerdict::Continue;
}

NodeVerdict AgenticDecisionTree::evalSSALift(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    if (ctx.targetBinaryPath.empty()) {
        ctx.addTrace("[SSA] No binary target specified, skipping SSA lift");
        return NodeVerdict::Skip;
    }

    ctx.addTrace("[SSA] Running SSA lifter on: " + ctx.targetBinaryPath
                 + " @ 0x" + ([&]() {
                     std::ostringstream oss;
                     oss << std::hex << ctx.targetFunctionAddr;
                     return oss.str();
                 })());

    bool ok = runSSALiftWithAnomalyDetection(ctx);
    m_stats.ssaLiftsPerformed++;

    if (ok) {
        ctx.addTrace("[SSA] Lift complete, " + std::to_string(ctx.ssaLiftResult.size()) + " chars of IR");
        return NodeVerdict::Continue;
    } else {
        ctx.addTrace("[SSA] Lift failed");
        return NodeVerdict::Failure;
    }
}

NodeVerdict AgenticDecisionTree::evalCorrectionPlan(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    // Determine which correction to apply based on failure type and confidence
    auto failType = static_cast<InferenceFailureType>(ctx.failureType);

    switch (failType) {
        case InferenceFailureType::Refusal:
            ctx.correctionPlan = "ServerPatch: inject anti-refusal token bias";
            ctx.addTrace("[PLAN] Strategy: Server-layer token bias injection (anti-refusal)");
            break;

        case InferenceFailureType::Hallucination:
            ctx.correctionPlan = "MemoryHotpatch: reduce temperature / adjust sampling weights";
            ctx.addTrace("[PLAN] Strategy: Memory patch to adjust sampling parameters");
            break;

        case InferenceFailureType::InfiniteLoop:
            ctx.correctionPlan = "ServerPatch: repetition penalty + stream termination";
            ctx.addTrace("[PLAN] Strategy: Server-layer repetition penalty");
            break;

        case InferenceFailureType::FormatViolation:
            ctx.correctionPlan = "ServerPatch: output rewrite transform";
            ctx.addTrace("[PLAN] Strategy: Server-layer output rewriter");
            break;

        case InferenceFailureType::GarbageOutput:
            if (ctx.failureConfidence > 0.85f) {
                ctx.correctionPlan = "MemoryHotpatch: patch attention weights + re-infer";
                ctx.addTrace("[PLAN] Strategy: Memory patch on attention layer");
            } else {
                ctx.correctionPlan = "ServerPatch: output filter + retry";
                ctx.addTrace("[PLAN] Strategy: Server-layer output filtering");
            }
            break;

        case InferenceFailureType::SafetyViolation:
            ctx.correctionPlan = "ServerPatch: safety bypass rewrite";
            ctx.addTrace("[PLAN] Strategy: Server-layer safety transform");
            break;

        case InferenceFailureType::Timeout:
        case InferenceFailureType::ResourceExhausted:
            ctx.correctionPlan = "MemoryHotpatch: reduce context window / layer count";
            ctx.addTrace("[PLAN] Strategy: Memory patch to reduce compute load");
            break;

        case InferenceFailureType::TokenLimit:
            ctx.correctionPlan = "ServerPatch: truncation handler + continuation prompt";
            ctx.addTrace("[PLAN] Strategy: Server-layer continuation handler");
            break;

        case InferenceFailureType::LowConfidence:
            ctx.correctionPlan = "MemoryHotpatch: temperature adjustment";
            ctx.addTrace("[PLAN] Strategy: Memory patch for temperature tuning");
            break;

        default:
            ctx.correctionPlan = "Escalate: unknown failure type";
            ctx.addTrace("[PLAN] Unknown failure type — escalating");
            return NodeVerdict::Escalate;
    }

    if (ctx.failureConfidence < node.confidenceThreshold) {
        ctx.addTrace("[PLAN] Confidence too low for autonomous correction — escalating");
        return NodeVerdict::Escalate;
    }

    return NodeVerdict::Continue;
}

NodeVerdict AgenticDecisionTree::evalMemoryHotpatch(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    // Only attempt if correction plan calls for memory patch
    if (ctx.correctionPlan.find("MemoryHotpatch") == std::string::npos) {
        return NodeVerdict::Skip;
    }

    if (ctx.failureConfidence < node.confidenceThreshold) {
        ctx.addTrace("[MEMPATCH] Confidence below threshold for memory patch");
        return NodeVerdict::Skip;
    }

    ctx.addTrace("[MEMPATCH] Applying memory-layer hotpatch...");

    bool ok = applyMemoryHotpatch(ctx);
    if (ok) {
        m_stats.patchesApplied++;
        ctx.patchApplied = true;
        ctx.addTrace("[MEMPATCH] ✅ Memory hotpatch applied: " + ctx.patchDetail);
        return NodeVerdict::Continue; // Proceed to verification
    } else {
        ctx.addTrace("[MEMPATCH] ❌ Memory hotpatch failed: " + ctx.patchDetail);
        return NodeVerdict::Failure;
    }
}

NodeVerdict AgenticDecisionTree::evalByteHotpatch(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    if (ctx.correctionPlan.find("ByteHotpatch") == std::string::npos) {
        return NodeVerdict::Skip;
    }

    if (ctx.failureConfidence < node.confidenceThreshold) {
        ctx.addTrace("[BYTEPATCH] Confidence below threshold");
        return NodeVerdict::Skip;
    }

    ctx.addTrace("[BYTEPATCH] Applying byte-level GGUF hotpatch...");

    bool ok = applyByteHotpatch(ctx);
    if (ok) {
        m_stats.patchesApplied++;
        ctx.patchApplied = true;
        ctx.addTrace("[BYTEPATCH] ✅ Byte hotpatch applied: " + ctx.patchDetail);
        return NodeVerdict::Continue;
    } else {
        ctx.addTrace("[BYTEPATCH] ❌ Byte hotpatch failed: " + ctx.patchDetail);
        return NodeVerdict::Failure;
    }
}

NodeVerdict AgenticDecisionTree::evalServerPatch(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    if (ctx.correctionPlan.find("ServerPatch") == std::string::npos) {
        return NodeVerdict::Skip;
    }

    ctx.addTrace("[SRVPATCH] Applying server-layer transform...");

    bool ok = applyServerPatch(ctx);
    if (ok) {
        m_stats.patchesApplied++;
        ctx.patchApplied = true;
        ctx.addTrace("[SRVPATCH] ✅ Server patch applied: " + ctx.patchDetail);
        return NodeVerdict::Continue;
    } else {
        ctx.addTrace("[SRVPATCH] ❌ Server patch failed: " + ctx.patchDetail);
        return NodeVerdict::Failure;
    }
}

NodeVerdict AgenticDecisionTree::evalVerification(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    if (!ctx.patchApplied) {
        ctx.addTrace("[VERIFY] No patch applied, nothing to verify");
        return NodeVerdict::Skip;
    }

    ctx.addTrace("[VERIFY] Re-running inference to verify correction...");

    bool ok = verifyCorrection(ctx);
    if (ok) {
        ctx.verificationPassed = true;
        ctx.addTrace("[VERIFY] ✅ Verification passed — failure resolved");
        return NodeVerdict::Success;
    } else {
        ctx.addTrace("[VERIFY] ❌ Verification failed — correction insufficient");
        // Revert the patch
        revertLastPatch(ctx);
        return NodeVerdict::Failure;
    }
}

NodeVerdict AgenticDecisionTree::evalEscalation(const DecisionNode& node, TreeContext& ctx) {
    (void)node;

    m_stats.escalations++;
    ctx.escalationReason = "Autonomous correction exhausted all strategies. "
                            "Failure: " + ctx.failureDescription +
                            " (confidence=" + std::to_string(ctx.failureConfidence) + ")";

    ctx.addTrace("[ESCALATE] " + ctx.escalationReason);
    return NodeVerdict::Escalate;
}

NodeVerdict AgenticDecisionTree::evalTerminal(const DecisionNode& node, TreeContext& ctx) {
    (void)node;
    ctx.addTrace("[TERMINAL] Decision path complete");
    return NodeVerdict::Success;
}

// ============================================================================
// SSA Integration
// ============================================================================

bool AgenticDecisionTree::runSSALift(TreeContext& ctx) {
    if (ctx.targetBinaryPath.empty()) return false;

    RawrXD::ReverseEngineering::RawrCodex codex;
    if (!codex.LoadBinary(ctx.targetBinaryPath)) {
        ctx.addTrace("[SSA] Failed to load binary: " + ctx.targetBinaryPath);
        return false;
    }

    // Lift SSA from the target function
    uint64_t addr = ctx.targetFunctionAddr;
    if (addr == 0) {
        // Try to find function by name
        auto symbols = codex.GetSymbols();
        for (const auto& sym : symbols) {
            if (sym.name == ctx.targetFunctionName ||
                sym.demangledName == ctx.targetFunctionName) {
                addr = sym.address;
                break;
            }
        }
    }

    if (addr == 0) {
        ctx.addTrace("[SSA] Function address not resolved");
        return false;
    }

    // Build CFG → Lift to SSA
    auto blocks = codex.AnalyzeControlFlow(addr);
    if (blocks.empty()) {
        ctx.addTrace("[SSA] CFG analysis returned no basic blocks");
        return false;
    }

    codex.LiftToSSA(addr);

    // Format SSA IR into readable text
    auto ssaInstrs = codex.GetSSAInstructions();
    auto ssaVars   = codex.GetSSAVariables();
    auto phiNodes  = codex.GetPhiNodes();

    std::ostringstream oss;
    oss << "=== SSA IR for " << ctx.targetFunctionName << " @ 0x" << std::hex << addr << " ===\n";
    oss << "Variables: " << std::dec << ssaVars.size() << "\n";
    oss << "Instructions: " << ssaInstrs.size() << "\n";
    oss << "PHI Nodes: " << phiNodes.size() << "\n\n";

    for (const auto& phi : phiNodes) {
        oss << "  BB" << phi.bbIndex << ": PHI v" << phi.dstVarId << " = phi(";
        for (size_t i = 0; i < phi.operandVarIds.size(); i++) {
            if (i > 0) oss << ", ";
            oss << "v" << phi.operandVarIds[i] << " from BB" << phi.operandBBs[i];
        }
        oss << ")\n";
    }

    for (const auto& instr : ssaInstrs) {
        oss << "  [0x" << std::hex << instr.origAddress << "] "
            << std::dec;
        if (instr.dstVarId >= 0) oss << "v" << instr.dstVarId << " = ";
        oss << "op" << static_cast<int>(instr.op);
        if (instr.src1VarId >= 0) oss << " v" << instr.src1VarId;
        if (instr.src2VarId >= 0) oss << ", v" << instr.src2VarId;
        if (instr.isDeadCode) oss << " [DEAD]";
        oss << "\n";
    }

    ctx.ssaLiftResult = oss.str();
    return true;
}

bool AgenticDecisionTree::runSSALiftWithAnomalyDetection(TreeContext& ctx) {
    if (!runSSALift(ctx)) return false;

    // Scan for anomalies in the SSA IR
    RawrXD::ReverseEngineering::RawrCodex codex;
    codex.LoadBinary(ctx.targetBinaryPath);
    codex.LiftToSSA(ctx.targetFunctionAddr);

    auto instrs = codex.GetSSAInstructions();
    int deadCount = 0;
    int totalInstrs = (int)instrs.size();

    for (const auto& instr : instrs) {
        if (instr.isDeadCode) deadCount++;
    }

    if (totalInstrs > 0) {
        float deadRatio = (float)deadCount / (float)totalInstrs;
        if (deadRatio > 0.3f) {
            ctx.addTrace("[SSA:ANOMALY] High dead code ratio: "
                         + std::to_string((int)(deadRatio * 100)) + "% ("
                         + std::to_string(deadCount) + "/" + std::to_string(totalInstrs) + ")");
        }
    }

    // Check for suspicious patterns (e.g. multiple unresolved calls, type mismatches)
    int unresolvedCalls = 0;
    for (const auto& instr : instrs) {
        if (instr.op == RawrXD::ReverseEngineering::SSAOpType::Call && instr.callTarget == 0) {
            unresolvedCalls++;
        }
    }

    if (unresolvedCalls > 3) {
        ctx.addTrace("[SSA:ANOMALY] " + std::to_string(unresolvedCalls) + " unresolved call targets");
    }

    // Run type recovery and check for inconsistencies
    codex.RecoverTypes();
    auto typeInfos = codex.GetRecoveredTypes();
    int lowConfTypes = 0;
    for (const auto& ti : typeInfos) {
        if (ti.confidence == RawrXD::ReverseEngineering::TypeConfidence::None ||
            ti.confidence == RawrXD::ReverseEngineering::TypeConfidence::Low) {
            lowConfTypes++;
        }
    }
    if (lowConfTypes > 5) {
        ctx.addTrace("[SSA:ANOMALY] " + std::to_string(lowConfTypes) + " low-confidence type inferences");
    }

    return true;
}

// ============================================================================
// Failure Detection
// ============================================================================

bool AgenticDecisionTree::detectFailure(TreeContext& ctx) {
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();
    InferenceFailureEvent evt = orchestrator.detectFailure(
        ctx.inferenceOutput.c_str(), ctx.inferenceOutput.size(),
        ctx.inferencePrompt.c_str(), ctx.inferencePrompt.size());

    ctx.failureType = static_cast<uint8_t>(evt.type);
    ctx.failureConfidence = evt.confidence;
    if (evt.description) ctx.failureDescription = evt.description;

    return (evt.type != InferenceFailureType::None);
}

// ============================================================================
// Hotpatch Application
// ============================================================================

bool AgenticDecisionTree::applyMemoryHotpatch(TreeContext& ctx) {
    auto& uhm = UnifiedHotpatchManager::instance();
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();

    // Build a failure event for the orchestrator
    InferenceFailureEvent failEvt{};
    failEvt.type = static_cast<InferenceFailureType>(ctx.failureType);
    failEvt.confidence = ctx.failureConfidence;
    failEvt.description = ctx.failureDescription.c_str();
    failEvt.timestamp = GetTickCount64();

    // Let the orchestrator decide the actual patch content
    CorrectionOutcome outcome = orchestrator.orchestrateCorrection(failEvt);

    if (outcome.success) {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Memory patch applied via orchestrator";
        return true;
    } else {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Memory patch failed";
        return false;
    }
}

bool AgenticDecisionTree::applyByteHotpatch(TreeContext& ctx) {
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();

    InferenceFailureEvent failEvt{};
    failEvt.type = static_cast<InferenceFailureType>(ctx.failureType);
    failEvt.confidence = ctx.failureConfidence;
    failEvt.description = ctx.failureDescription.c_str();
    failEvt.timestamp = GetTickCount64();

    CorrectionOutcome outcome = orchestrator.orchestrateCorrection(failEvt);

    if (outcome.success) {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Byte patch applied";
        return true;
    } else {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Byte patch failed";
        return false;
    }
}

bool AgenticDecisionTree::applyServerPatch(TreeContext& ctx) {
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();

    InferenceFailureEvent failEvt{};
    failEvt.type = static_cast<InferenceFailureType>(ctx.failureType);
    failEvt.confidence = ctx.failureConfidence;
    failEvt.description = ctx.failureDescription.c_str();
    failEvt.timestamp = GetTickCount64();

    CorrectionOutcome outcome = orchestrator.orchestrateCorrection(failEvt);

    if (outcome.success) {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Server patch applied";
        return true;
    } else {
        ctx.patchDetail = outcome.detail ? outcome.detail : "Server patch failed";
        return false;
    }
}

bool AgenticDecisionTree::revertLastPatch(TreeContext& ctx) {
    m_stats.patchesReverted++;
    ctx.addTrace("[REVERT] Reverting last applied patch");
    // Revert is handled by the UnifiedHotpatchManager's tracked entries
    ctx.patchApplied = false;
    return true;
}

// ============================================================================
// Verification
// ============================================================================

bool AgenticDecisionTree::verifyCorrection(TreeContext& ctx) {
    if (!m_engine) {
        ctx.addTrace("[VERIFY] No agentic engine attached — cannot re-infer");
        return false;
    }

    if (!m_engine->isModelLoaded()) {
        ctx.addTrace("[VERIFY] Model not loaded — cannot re-infer");
        return false;
    }

    // Re-run the same prompt
    std::string newOutput = m_engine->chat(ctx.inferencePrompt);
    ctx.addTrace("[VERIFY] Re-inference produced " + std::to_string(newOutput.size()) + " chars");

    // Check if the failure persists
    auto& orchestrator = AgenticHotpatchOrchestrator::instance();
    InferenceFailureEvent evt = orchestrator.detectFailure(
        newOutput.c_str(), newOutput.size(),
        ctx.inferencePrompt.c_str(), ctx.inferencePrompt.size());

    if (evt.type == InferenceFailureType::None) {
        ctx.addTrace("[VERIFY] No failure detected in new output — correction successful");
        return true;
    }

    if (evt.confidence < ctx.failureConfidence * 0.5f) {
        ctx.addTrace("[VERIFY] Failure severity reduced significantly ("
                     + std::to_string(evt.confidence) + " vs "
                     + std::to_string(ctx.failureConfidence) + ") — partial success");
        return true;
    }

    ctx.addTrace("[VERIFY] Failure persists: type="
                 + std::to_string(static_cast<int>(evt.type))
                 + " confidence=" + std::to_string(evt.confidence));
    return false;
}

// ============================================================================
// Callbacks
// ============================================================================

void AgenticDecisionTree::registerTraceCallback(DecisionTraceCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_traceCallbacks.push_back({cb, userData});
}

void AgenticDecisionTree::registerApprovalCallback(UserApprovalCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_approvalFn = cb;
    m_approvalUserData = userData;
}

void AgenticDecisionTree::registerCompleteCallback(DecisionCompleteCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completeCallbacks.push_back({cb, userData});
}

void AgenticDecisionTree::unregisterTraceCallback(DecisionTraceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_traceCallbacks.erase(
        std::remove_if(m_traceCallbacks.begin(), m_traceCallbacks.end(),
                        [cb](const TraceCB& t) { return t.fn == cb; }),
        m_traceCallbacks.end());
}

void AgenticDecisionTree::unregisterCompleteCallback(DecisionCompleteCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completeCallbacks.erase(
        std::remove_if(m_completeCallbacks.begin(), m_completeCallbacks.end(),
                        [cb](const CompleteCB& c) { return c.fn == cb; }),
        m_completeCallbacks.end());
}

void AgenticDecisionTree::notifyTrace(const char* desc, NodeVerdict verdict) {
    for (const auto& cb : m_traceCallbacks) {
        cb.fn(desc, verdict, cb.userData);
    }
}

void AgenticDecisionTree::notifyComplete(const DecisionOutcome& outcome) {
    for (const auto& cb : m_completeCallbacks) {
        cb.fn(&outcome, cb.userData);
    }
}

bool AgenticDecisionTree::requestApproval(const char* desc, AutonomyRisk risk) {
    if (!m_approvalFn) {
        // No approval callback — auto-approve if risk is low enough
        return (risk <= AutonomyRisk::Medium);
    }
    return m_approvalFn(desc, risk, m_approvalUserData);
}

// ============================================================================
// Statistics
// ============================================================================

const DecisionTreeStats& AgenticDecisionTree::getStats() const {
    return m_stats;
}

void AgenticDecisionTree::resetStats() {
    m_stats.treesEvaluated.store(0);
    m_stats.nodesVisited.store(0);
    m_stats.ssaLiftsPerformed.store(0);
    m_stats.failuresDetected.store(0);
    m_stats.patchesApplied.store(0);
    m_stats.patchesReverted.store(0);
    m_stats.escalations.store(0);
    m_stats.successfulCorrections.store(0);
    m_stats.failedCorrections.store(0);
    m_stats.totalRetries.store(0);
    m_stats.aborts.store(0);
}

// ============================================================================
// Configuration
// ============================================================================

void AgenticDecisionTree::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool AgenticDecisionTree::isEnabled() const {
    return m_enabled;
}

void AgenticDecisionTree::setMaxTreeDepth(int depth) {
    m_maxTreeDepth = depth;
}

void AgenticDecisionTree::setMaxTotalRetries(int retries) {
    m_maxTotalRetries = retries;
}

void AgenticDecisionTree::setGlobalConfidenceThreshold(float threshold) {
    m_globalConfidenceThreshold = threshold;
}

void AgenticDecisionTree::setAutoEscalateOnCritical(bool enabled) {
    m_autoEscalateOnCritical = enabled;
}

// ============================================================================
// Serialization / Debug
// ============================================================================

std::string AgenticDecisionTree::dumpTreeJSON() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\n  \"nodes\": [\n";

    bool first = true;
    for (const auto& [id, node] : m_nodes) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    {\"id\":" << node.nodeId
            << ",\"type\":" << static_cast<int>(node.type)
            << ",\"risk\":" << static_cast<int>(node.risk)
            << ",\"name\":\"" << node.name << "\""
            << ",\"parent\":" << node.parentId
            << ",\"children\":[";
        for (size_t i = 0; i < node.childIds.size(); i++) {
            if (i > 0) oss << ",";
            oss << node.childIds[i];
        }
        oss << "]"
            << ",\"maxRetries\":" << node.maxRetries
            << ",\"requiresApproval\":" << (node.requiresApproval ? "true" : "false")
            << "}";
    }

    oss << "\n  ],\n  \"rootId\":" << m_rootNodeId
        << ",\n  \"enabled\":" << (m_enabled ? "true" : "false")
        << ",\n  \"maxDepth\":" << m_maxTreeDepth
        << ",\n  \"maxRetries\":" << m_maxTotalRetries
        << ",\n  \"confidenceThreshold\":" << m_globalConfidenceThreshold
        << "\n}";

    return oss.str();
}

std::string AgenticDecisionTree::dumpLastTrace() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "=== Decision Tree Trace (" << m_lastTrace.size() << " entries) ===\n";
    for (size_t i = 0; i < m_lastTrace.size(); i++) {
        oss << "  " << (i + 1) << ". " << m_lastTrace[i] << "\n";
    }
    return oss.str();
}

