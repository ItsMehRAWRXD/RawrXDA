// ============================================================================
// agentic_decision_tree.h — Headless Agentic Decision Tree for CLI
// ============================================================================
//
// Phase 19: Autonomous decision-making for the RawrXD CLI (headless).
// The tree enables the AI to autonomously:
//   1. Run the SSA Lifter on suspicious functions.
//   2. Identify potential failures in inference output.
//   3. Apply Memory Hotpatches to the inference engine to fix logic.
// All without a GUI — pure terminal-driven autonomy.
//
// Architecture:
//   DecisionNode    — A single decision point in the tree
//   DecisionOutcome — Structured result of tree evaluation
//   TreeContext     — Mutable state carried through traversal
//   AgenticDecisionTree — Root orchestrator: evaluator + action executor
//
// Integrations:
//   - RawrCodex SSA Lifter (src/reverse_engineering/RawrCodex.hpp)
//   - AgenticHotpatchOrchestrator (src/agent/agentic_hotpatch_orchestrator.hpp)
//   - UnifiedHotpatchManager (src/core/unified_hotpatch_manager.hpp)
//   - AgentSafetyContract (src/core/agent_safety_contract.h)
//   - AgenticEngine (src/agentic_engine.h)
//   - SubAgentManager (src/subagent_core.h)
//
// Pattern: Structured results (PatchResult-style), no exceptions.
// Threading: All methods are thread-safe (mutex-guarded).
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <unordered_map>

// Forward declarations — no circular includes
class AgenticEngine;
class SubAgentManager;
struct PatchResult;

// ============================================================================
// ENUMS
// ============================================================================

// What kind of decision node is this?
enum class DecisionNodeType : uint8_t {
    Root            = 0,    // Entry point of the tree
    Observation     = 1,    // Gather data / analyze output
    Classification  = 2,    // Classify failure type
    SSALift         = 3,    // Run SSA lifter on target
    FailureDetect   = 4,    // Detect inference failure
    CorrectionPlan  = 5,    // Plan a correction strategy
    MemoryHotpatch  = 6,    // Apply memory-layer hotpatch
    ByteHotpatch    = 7,    // Apply byte-layer GGUF patch
    ServerPatch     = 8,    // Inject server-layer transform
    Verification    = 9,    // Verify correction was effective
    Escalation      = 10,   // Escalate to user (cannot auto-fix)
    Terminal        = 11,   // End of decision path (leaf)
};

// Outcome of evaluating a single decision node
enum class NodeVerdict : uint8_t {
    Continue        = 0,    // Proceed to child nodes
    Success         = 1,    // Action succeeded, can stop or verify
    Failure         = 2,    // Action failed, try fallback
    Escalate        = 3,    // Cannot auto-resolve, need user
    Skip            = 4,    // Condition not met, skip this branch
    Retry           = 5,    // Transient error, retry this node
    Abort           = 6,    // Critical failure, stop entire tree
};

// What triggered autonomous intervention?
enum class TriggerReason : uint8_t {
    InferenceFailure    = 0,    // Model output was bad
    SSAAnomaly          = 1,    // SSA analysis found suspicious pattern
    PerformanceDegraded = 2,    // Token/sec dropped below threshold
    ConfidenceLow       = 3,    // Model confidence below threshold
    UserRequested       = 4,    // User invoked !auto_patch or similar
    ScheduledScan       = 5,    // Periodic health check
    HotpatchDrift       = 6,    // Previous patch may have drifted
    OutputLoop          = 7,    // Repeating/looping output detected
};

// Risk level of the autonomous action
enum class AutonomyRisk : uint8_t {
    ReadOnly    = 0,    // Observation only, no side effects
    Low         = 1,    // Reversible, e.g. token bias
    Medium      = 2,    // Memory patch (can revert)
    High        = 3,    // Byte-level GGUF modification
    Critical    = 4,    // Irreversible or multi-layer patch
};

// ============================================================================
// STRUCTS
// ============================================================================

// A single node in the decision tree
struct DecisionNode {
    uint32_t            nodeId;
    DecisionNodeType    type;
    AutonomyRisk        risk;
    const char*         name;           // Human-readable label
    const char*         description;    // What this node does
    uint32_t            parentId;       // 0 for root
    std::vector<uint32_t> childIds;     // IDs of child nodes
    int                 maxRetries;     // Max retries for this node
    float               confidenceThreshold; // Min confidence to proceed
    bool                requiresApproval;    // Pause for user confirmation
};

// Context carried through the decision tree traversal
struct TreeContext {
    // Input
    std::string         inferenceOutput;        // The model's raw output
    std::string         inferencePrompt;        // The original prompt
    std::string         targetBinaryPath;       // Binary under analysis (if any)
    uint64_t            targetFunctionAddr;     // Function address for SSA lift
    std::string         targetFunctionName;     // Function name for SSA lift

    // Mutable state (accumulated during traversal)
    std::string         ssaLiftResult;          // SSA lifter output
    std::string         failureDescription;     // Detected failure description
    float               failureConfidence;      // 0.0 – 1.0
    uint8_t             failureType;            // Maps to InferenceFailureType
    std::string         correctionPlan;         // What correction was planned
    std::string         patchDetail;            // Details of applied patch
    bool                patchApplied;           // Whether a patch was applied
    bool                verificationPassed;     // Post-patch verification result
    int                 totalRetries;           // Retries consumed so far
    std::string         escalationReason;       // Why escalation was needed

    // Trace / audit
    std::vector<std::string> traceLog;          // Breadcrumb trail of decisions
    std::chrono::steady_clock::time_point startTime;
    uint64_t            sequenceId;

    TreeContext() : targetFunctionAddr(0), failureConfidence(0.0f),
                    failureType(0), patchApplied(false),
                    verificationPassed(false), totalRetries(0),
                    sequenceId(0) {
        startTime = std::chrono::steady_clock::now();
    }

    void addTrace(const std::string& msg) {
        traceLog.push_back(msg);
    }

    int elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime).count();
    }
};

// Outcome of the full decision tree evaluation
struct DecisionOutcome {
    bool                success;
    NodeVerdict         finalVerdict;
    TriggerReason       trigger;
    AutonomyRisk        riskTaken;
    const char*         summary;
    int                 nodesEvaluated;
    int                 patchesApplied;
    int                 retriesUsed;
    int                 durationMs;
    std::vector<std::string> traceLog;

    static DecisionOutcome ok(const char* msg, int nodes = 0, int patches = 0) {
        DecisionOutcome r;
        r.success       = true;
        r.finalVerdict  = NodeVerdict::Success;
        r.trigger       = TriggerReason::InferenceFailure;
        r.riskTaken     = AutonomyRisk::ReadOnly;
        r.summary       = msg;
        r.nodesEvaluated = nodes;
        r.patchesApplied = patches;
        r.retriesUsed   = 0;
        r.durationMs    = 0;
        return r;
    }

    static DecisionOutcome error(NodeVerdict v, const char* msg) {
        DecisionOutcome r;
        r.success       = false;
        r.finalVerdict  = v;
        r.trigger       = TriggerReason::InferenceFailure;
        r.riskTaken     = AutonomyRisk::ReadOnly;
        r.summary       = msg;
        r.nodesEvaluated = 0;
        r.patchesApplied = 0;
        r.retriesUsed   = 0;
        r.durationMs    = 0;
        return r;
    }

    static DecisionOutcome escalated(const char* msg) {
        DecisionOutcome r;
        r.success       = false;
        r.finalVerdict  = NodeVerdict::Escalate;
        r.trigger       = TriggerReason::InferenceFailure;
        r.riskTaken     = AutonomyRisk::ReadOnly;
        r.summary       = msg;
        r.nodesEvaluated = 0;
        r.patchesApplied = 0;
        r.retriesUsed   = 0;
        r.durationMs    = 0;
        return r;
    }
};

// Statistics for the decision tree
struct DecisionTreeStats {
    std::atomic<uint64_t> treesEvaluated{0};
    std::atomic<uint64_t> nodesVisited{0};
    std::atomic<uint64_t> ssaLiftsPerformed{0};
    std::atomic<uint64_t> failuresDetected{0};
    std::atomic<uint64_t> patchesApplied{0};
    std::atomic<uint64_t> patchesReverted{0};
    std::atomic<uint64_t> escalations{0};
    std::atomic<uint64_t> successfulCorrections{0};
    std::atomic<uint64_t> failedCorrections{0};
    std::atomic<uint64_t> totalRetries{0};
    std::atomic<uint64_t> aborts{0};
};

// Callback types (function pointers, no std::function in hot path)
typedef void (*DecisionTraceCallback)(const char* nodeDesc, NodeVerdict verdict, void* userData);
typedef bool (*UserApprovalCallback)(const char* actionDesc, AutonomyRisk risk, void* userData);
typedef void (*DecisionCompleteCallback)(const DecisionOutcome* outcome, void* userData);

// ============================================================================
// AgenticDecisionTree — Main class
// ============================================================================

class AgenticDecisionTree {
public:
    static AgenticDecisionTree& instance();

    // ---- Engine Wiring ----
    void setAgenticEngine(AgenticEngine* engine);
    void setSubAgentManager(SubAgentManager* mgr);

    // ---- Tree Construction ----
    // Build the default decision tree (standard autonomy pipeline).
    void buildDefaultTree();

    // Add a custom decision node.
    uint32_t addNode(const DecisionNode& node);

    // Link parent → child.
    void linkNode(uint32_t parentId, uint32_t childId);

    // Get a node by ID.
    const DecisionNode* getNode(uint32_t nodeId) const;

    // ---- Tree Evaluation ----

    // Full pipeline: observe → classify → decide → act → verify.
    // This is the main entry point for autonomous correction.
    DecisionOutcome evaluate(TreeContext& ctx);

    // Evaluate a specific sub-tree starting from nodeId.
    DecisionOutcome evaluateFrom(uint32_t nodeId, TreeContext& ctx);

    // One-shot convenience: analyze model output, auto-correct if needed.
    DecisionOutcome analyzeAndFix(const std::string& output,
                                   const std::string& prompt);

    // ---- SSA Integration ----

    // Run SSA lifter on a function address in a loaded binary.
    // Returns the SSA IR as human-readable text (stored in ctx.ssaLiftResult).
    bool runSSALift(TreeContext& ctx);

    // Run SSA lifter and auto-detect anomalies (dead code, type mismatches).
    bool runSSALiftWithAnomalyDetection(TreeContext& ctx);

    // ---- Failure Detection ----

    // Analyze output text and classify failure (stores in ctx.failureType/confidence).
    bool detectFailure(TreeContext& ctx);

    // ---- Hotpatch Application ----

    // Apply a memory-layer hotpatch to the inference engine.
    bool applyMemoryHotpatch(TreeContext& ctx);

    // Apply a byte-layer GGUF hotpatch.
    bool applyByteHotpatch(TreeContext& ctx);

    // Apply a server-layer patch.
    bool applyServerPatch(TreeContext& ctx);

    // Revert the most recent patch.
    bool revertLastPatch(TreeContext& ctx);

    // ---- Verification ----

    // Re-run inference with same prompt and check if the failure is fixed.
    bool verifyCorrection(TreeContext& ctx);

    // ---- Callbacks ----
    void registerTraceCallback(DecisionTraceCallback cb, void* userData);
    void registerApprovalCallback(UserApprovalCallback cb, void* userData);
    void registerCompleteCallback(DecisionCompleteCallback cb, void* userData);
    void unregisterTraceCallback(DecisionTraceCallback cb);
    void unregisterCompleteCallback(DecisionCompleteCallback cb);

    // ---- Statistics ----
    const DecisionTreeStats& getStats() const;
    void resetStats();

    // ---- Configuration ----
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setMaxTreeDepth(int depth);
    void setMaxTotalRetries(int retries);
    void setGlobalConfidenceThreshold(float threshold);
    void setAutoEscalateOnCritical(bool enabled);

    // ---- Serialization ----
    // Dump the tree structure as JSON for inspection / debugging.
    std::string dumpTreeJSON() const;

    // Dump the decision trace from the last evaluation.
    std::string dumpLastTrace() const;

private:
    AgenticDecisionTree();
    ~AgenticDecisionTree();
    AgenticDecisionTree(const AgenticDecisionTree&) = delete;
    AgenticDecisionTree& operator=(const AgenticDecisionTree&) = delete;

    // ---- Internal Node Evaluators ----
    NodeVerdict evalRoot(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalObservation(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalClassification(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalSSALift(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalFailureDetect(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalCorrectionPlan(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalMemoryHotpatch(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalByteHotpatch(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalServerPatch(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalVerification(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalEscalation(const DecisionNode& node, TreeContext& ctx);
    NodeVerdict evalTerminal(const DecisionNode& node, TreeContext& ctx);

    // Recursive traversal
    NodeVerdict traverseNode(uint32_t nodeId, TreeContext& ctx, int depth);

    // Fire callbacks
    void notifyTrace(const char* desc, NodeVerdict verdict);
    void notifyComplete(const DecisionOutcome& outcome);
    bool requestApproval(const char* desc, AutonomyRisk risk);

    // ---- State ----
    mutable std::mutex                              m_mutex;
    std::unordered_map<uint32_t, DecisionNode>      m_nodes;
    uint32_t                                        m_rootNodeId;
    uint32_t                                        m_nextNodeId;
    DecisionTreeStats                               m_stats;
    std::vector<std::string>                        m_lastTrace;

    // Engine pointers (non-owning)
    AgenticEngine*                                  m_engine;
    SubAgentManager*                                m_subAgentMgr;

    // Configuration
    bool                                            m_enabled;
    int                                             m_maxTreeDepth;
    int                                             m_maxTotalRetries;
    float                                           m_globalConfidenceThreshold;
    bool                                            m_autoEscalateOnCritical;

    // Callback storage
    struct TraceCB   { DecisionTraceCallback fn; void* userData; };
    struct ApproveCB { UserApprovalCallback fn; void* userData; };
    struct CompleteCB { DecisionCompleteCallback fn; void* userData; };
    std::vector<TraceCB>                            m_traceCallbacks;
    std::vector<CompleteCB>                         m_completeCallbacks;
    UserApprovalCallback                            m_approvalFn;
    void*                                           m_approvalUserData;
};

#endif // Header guard via #pragma once

