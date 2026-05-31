// ============================================================================
// swarm_decision_bridge.h — Phase 11 Revisit: Decision Tree → Swarm Bridge
// ============================================================================
// Wires the AgenticDecisionTree (Phase 19) to Swarm_NodeBroadcast (Phase 11).
// Agentic tasks distribute across LAN nodes. Headless CLI becomes swarm
// orchestrator for autonomous inference correction across the cluster.
//
// Architecture:
//   AgenticDecisionTree::evaluate()
//     → SwarmDecisionBridge::distributeTask()
//       → SwarmCoordinator::addTask() [custom agentic task type]
//         → Wire protocol TaskPush to remote workers
//           → Remote workers run local decision tree sub-evaluation
//             → Results aggregated via ConsensusVote
//
// Integrations:
//   - AgenticDecisionTree (src/cli/agentic_decision_tree.h)
//   - SwarmCoordinator (src/core/swarm_coordinator.h)
//   - UnifiedHotpatchManager (src/core/unified_hotpatch_manager.hpp)
//   - AMDGPUAccelerator (src/core/amd_gpu_accelerator.h)
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded. IOCP for async result collection.
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
#include <unordered_map>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>

// Forward declarations
class AgenticDecisionTree;
class SwarmCoordinator;
struct TreeContext;
struct DecisionOutcome;
struct SwarmNodeInfo;

// ============================================================================
// Swarm Agentic Task Types (extensions to SwarmTaskType)
// ============================================================================
enum class SwarmAgenticTaskType : uint8_t {
    InferenceCorrection     = 0x80,     // Distribute inference failure correction
    SSALiftRemote           = 0x81,     // Remote SSA lifting on worker node
    MemoryPatchDistributed  = 0x82,     // Coordinate memory patches across nodes
    BytePatchDistributed    = 0x83,     // Coordinate byte-level GGUF patches
    ModelSurgeryTask        = 0x84,     // Distributed model surgery (layer quantization)
    ConsensusCorrection     = 0x85,     // Multi-node correction consensus
    GPUOffloadTask          = 0x86,     // Offload GPU compute to capable node
    HealthCheck             = 0x87,     // Distributed health monitoring
};

// ============================================================================
// Swarm Agentic Task Status
// ============================================================================
enum class SwarmAgenticStatus : uint8_t {
    Pending         = 0,
    Distributed     = 1,
    RemoteRunning   = 2,
    ResultReceived  = 3,
    ConsensusPhase  = 4,
    Completed       = 5,
    Failed          = 6,
    TimedOut        = 7,
    Cancelled       = 8,
};

// ============================================================================
// Distributed Decision Task
// ============================================================================
struct DistributedDecisionTask {
    uint64_t                taskId;
    SwarmAgenticTaskType    taskType;
    SwarmAgenticStatus      status;
    uint32_t                assignedNodeSlot;       // UINT32_MAX = unassigned
    uint32_t                priority;
    
    // Decision context (serialized)
    std::string             serializedContext;       // JSON-serialized TreeContext
    std::string             targetOutput;            // Model output to correct
    std::string             targetPrompt;            // Original prompt
    
    // Results
    std::string             correctionResult;        // Correction from remote node
    float                   correctionConfidence;    // Remote node's confidence
    bool                    correctionSuccess;
    
    // GPU offload info
    bool                    requiresGPU;
    uint64_t                requiredVRAM;
    
    // Timing
    uint64_t                createdAtMs;
    uint64_t                distributedAtMs;
    uint64_t                completedAtMs;
    uint32_t                timeoutMs;
    
    // Consensus (multi-node verification)
    uint32_t                consensusQuorum;
    uint32_t                votesReceived;
    uint32_t                votesAgreeing;
    
    DistributedDecisionTask()
        : taskId(0), taskType(SwarmAgenticTaskType::InferenceCorrection)
        , status(SwarmAgenticStatus::Pending)
        , assignedNodeSlot(0xFFFFFFFF), priority(0)
        , correctionConfidence(0.0f), correctionSuccess(false)
        , requiresGPU(false), requiredVRAM(0)
        , createdAtMs(0), distributedAtMs(0), completedAtMs(0)
        , timeoutMs(30000), consensusQuorum(1)
        , votesReceived(0), votesAgreeing(0)
    {}
};

// ============================================================================
// Swarm Orchestrator Mode (for headless CLI)
// ============================================================================
enum class OrchestratorMode : uint8_t {
    Disabled        = 0,    // Not orchestrating
    Leader          = 1,    // This node distributes tasks
    Worker          = 2,    // This node executes distributed tasks
    Hybrid          = 3,    // Both leader and worker
};

// ============================================================================
// Bridge Statistics
// ============================================================================
struct SwarmDecisionBridgeStats {
    std::atomic<uint64_t> tasksDistributed{0};
    std::atomic<uint64_t> tasksCompleted{0};
    std::atomic<uint64_t> tasksFailed{0};
    std::atomic<uint64_t> tasksTimedOut{0};
    std::atomic<uint64_t> consensusReached{0};
    std::atomic<uint64_t> consensusFailed{0};
    std::atomic<uint64_t> gpuOffloads{0};
    std::atomic<uint64_t> totalLatencyMs{0};
    std::atomic<uint64_t> nodesUtilized{0};
};

// ============================================================================
// Callbacks (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*SwarmTaskCompletionCallback)(const DistributedDecisionTask* task, void* userData);
typedef void (*SwarmBroadcastCallback)(uint32_t nodeSlot, const char* message, void* userData);

// ============================================================================
// SwarmDecisionBridge — Decision Tree → Swarm Coordinator Bridge
// ============================================================================
class SwarmDecisionBridge {
public:
    static SwarmDecisionBridge& instance();

    // ---- Lifecycle ----
    bool initialize(OrchestratorMode mode = OrchestratorMode::Hybrid);
    void shutdown();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // ---- Engine Wiring ----
    void setDecisionTree(AgenticDecisionTree* tree);
    void setSwarmCoordinator(SwarmCoordinator* coordinator);

    // ---- Orchestrator Mode ----
    void setMode(OrchestratorMode mode);
    OrchestratorMode getMode() const { return m_mode; }

    // ---- Task Distribution ----
    
    // Distribute a decision tree evaluation across the swarm.
    // Serializes the TreeContext and broadcasts to available workers.
    uint64_t distributeTask(const TreeContext& ctx,
                            SwarmAgenticTaskType type = SwarmAgenticTaskType::InferenceCorrection,
                            uint32_t priority = 0);

    // Distribute with GPU requirement (routes to GPU-capable nodes).
    uint64_t distributeGPUTask(const TreeContext& ctx,
                               uint64_t requiredVRAM,
                               SwarmAgenticTaskType type = SwarmAgenticTaskType::GPUOffloadTask);

    // Broadcast a task to ALL online nodes for consensus-based correction.
    uint64_t broadcastForConsensus(const TreeContext& ctx,
                                    uint32_t quorum = 3);

    // Cancel a distributed task.
    bool cancelTask(uint64_t taskId);

    // ---- Result Collection ----
    
    // Poll for completed tasks (non-blocking).
    bool pollCompletedTask(DistributedDecisionTask& outTask);

    // Wait for a specific task to complete (blocking with timeout).
    bool waitForTask(uint64_t taskId, DistributedDecisionTask& outTask, uint32_t timeoutMs = 30000);

    // Get task status.
    SwarmAgenticStatus getTaskStatus(uint64_t taskId) const;

    // ---- Node Broadcast (Swarm_NodeBroadcast wire) ----
    
    // Broadcast a correction action to all swarm nodes.
    bool broadcastCorrection(const std::string& correctionData,
                             const std::string& targetModel);

    // Broadcast a hotpatch to all swarm nodes simultaneously.
    bool broadcastHotpatch(const void* patchData, size_t patchSize,
                           const char* layerName);

    // ---- Headless CLI Orchestrator ----
    
    // Enter orchestrator REPL mode (for headless CLI usage).
    void runOrchestratorLoop();

    // Process a single orchestrator command.
    std::string processOrchestratorCommand(const std::string& cmd);

    // ---- Node Selection (GPU-aware) ----
    
    // Select best node for a task based on capabilities.
    uint32_t selectBestNode(const DistributedDecisionTask& task) const;

    // Select nodes with GPU capability.
    std::vector<uint32_t> selectGPUCapableNodes(uint64_t minVRAM = 0) const;

    // ---- Callbacks ----
    void setTaskCompletionCallback(SwarmTaskCompletionCallback cb, void* userData);
    void setBroadcastCallback(SwarmBroadcastCallback cb, void* userData);

    // ---- Statistics ----
    const SwarmDecisionBridgeStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string activeTasksToJson() const;
    std::string statsToJson() const;

private:
    SwarmDecisionBridge();
    ~SwarmDecisionBridge();
    SwarmDecisionBridge(const SwarmDecisionBridge&) = delete;
    SwarmDecisionBridge& operator=(const SwarmDecisionBridge&) = delete;

    // Internal: Serialize TreeContext to JSON for transmission
    std::string serializeContext(const TreeContext& ctx) const;
    bool deserializeContext(const std::string& json, TreeContext& ctx) const;

    // Internal: Send task to specific node via swarm protocol
    bool sendTaskToNode(uint32_t nodeSlot, const DistributedDecisionTask& task);

    // Internal: Handle incoming result from worker
    void handleWorkerResult(uint32_t nodeSlot, uint64_t taskId,
                            const std::string& result, float confidence, bool success);

    // Internal: Consensus resolution
    void resolveConsensus(uint64_t taskId);

    // Internal: Timeout monitoring
    static DWORD WINAPI timeoutMonitorThread(LPVOID param);
    void checkTaskTimeouts();

    // Internal: Task ID generation
    uint64_t generateTaskId();

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                          m_mutex;
    std::atomic<bool>                           m_running;
    OrchestratorMode                            m_mode;

    // Engine pointers (non-owning)
    AgenticDecisionTree*                        m_decisionTree;
    SwarmCoordinator*                           m_coordinator;

    // Active tasks
    std::unordered_map<uint64_t, DistributedDecisionTask>  m_activeTasks;
    std::vector<DistributedDecisionTask>        m_completedQueue;
    std::atomic<uint64_t>                       m_nextTaskId;

    // Statistics
    SwarmDecisionBridgeStats                    m_stats;

    // Callbacks
    SwarmTaskCompletionCallback                 m_completionCb;
    void*                                       m_completionUserData;
    SwarmBroadcastCallback                      m_broadcastCb;
    void*                                       m_broadcastUserData;

    // Timeout monitor thread
    HANDLE                                      m_hTimeoutThread;
    std::atomic<bool>                           m_shutdownRequested;
};
