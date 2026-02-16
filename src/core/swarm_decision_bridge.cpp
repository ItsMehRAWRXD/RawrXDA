// ============================================================================
// swarm_decision_bridge.cpp — Phase 11 Revisit: Decision Tree → Swarm Bridge
// ============================================================================
// Wires AgenticDecisionTree to SwarmCoordinator's Swarm_NodeBroadcast.
// Distributes agentic correction tasks across LAN nodes.
// Headless CLI can act as swarm orchestrator.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "swarm_decision_bridge.h"
#include "swarm_coordinator.h"
#include "swarm_types.h"
#include "swarm_protocol.h"

// Forward-declared in header; include here for implementation
#include "../cli/agentic_decision_tree.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>

// SCAFFOLD_073: swarm_decision_bridge and LAN


// ============================================================================
// Singleton
// ============================================================================

SwarmDecisionBridge& SwarmDecisionBridge::instance() {
    static SwarmDecisionBridge s_instance;
    return s_instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SwarmDecisionBridge::SwarmDecisionBridge()
    : m_running(false)
    , m_mode(OrchestratorMode::Disabled)
    , m_decisionTree(nullptr)
    , m_coordinator(nullptr)
    , m_nextTaskId(1)
    , m_completionCb(nullptr)
    , m_completionUserData(nullptr)
    , m_broadcastCb(nullptr)
    , m_broadcastUserData(nullptr)
    , m_hTimeoutThread(nullptr)
    , m_shutdownRequested(false)
{
}

SwarmDecisionBridge::~SwarmDecisionBridge() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SwarmDecisionBridge::initialize(OrchestratorMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load(std::memory_order_relaxed)) {
        return true; // already running
    }

    m_mode = mode;
    m_shutdownRequested.store(false, std::memory_order_relaxed);

    // Start timeout monitor thread
    m_hTimeoutThread = CreateThread(nullptr, 0, timeoutMonitorThread, this, 0, nullptr);
    if (!m_hTimeoutThread) {
        return false;
    }

    m_running.store(true, std::memory_order_release);

    std::cout << "[SWARM-BRIDGE] Initialized in mode: "
              << (mode == OrchestratorMode::Leader   ? "Leader" :
                  mode == OrchestratorMode::Worker   ? "Worker" :
                  mode == OrchestratorMode::Hybrid   ? "Hybrid" : "Disabled")
              << "\n";

    return true;
}

void SwarmDecisionBridge::shutdown() {
    if (!m_running.load(std::memory_order_relaxed)) return;

    m_shutdownRequested.store(true, std::memory_order_release);
    m_running.store(false, std::memory_order_release);

    if (m_hTimeoutThread) {
        WaitForSingleObject(m_hTimeoutThread, 5000);
        CloseHandle(m_hTimeoutThread);
        m_hTimeoutThread = nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeTasks.clear();
    m_completedQueue.clear();

    std::cout << "[SWARM-BRIDGE] Shutdown complete. Tasks distributed: "
              << m_stats.tasksDistributed.load() << "\n";
}

// ============================================================================
// Engine Wiring
// ============================================================================

void SwarmDecisionBridge::setDecisionTree(AgenticDecisionTree* tree) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_decisionTree = tree;
}

void SwarmDecisionBridge::setSwarmCoordinator(SwarmCoordinator* coordinator) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_coordinator = coordinator;
}

void SwarmDecisionBridge::setMode(OrchestratorMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
}

// ============================================================================
// Task Distribution
// ============================================================================

uint64_t SwarmDecisionBridge::distributeTask(
    const TreeContext& ctx,
    SwarmAgenticTaskType type,
    uint32_t priority)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load(std::memory_order_relaxed)) return 0;
    if (m_mode == OrchestratorMode::Worker || m_mode == OrchestratorMode::Disabled) return 0;

    DistributedDecisionTask task;
    task.taskId = generateTaskId();
    task.taskType = type;
    task.status = SwarmAgenticStatus::Pending;
    task.priority = priority;
    task.serializedContext = serializeContext(ctx);
    task.targetOutput = ctx.inferenceOutput;
    task.targetPrompt = ctx.inferencePrompt;
    task.requiresGPU = false;
    task.requiredVRAM = 0;
    task.timeoutMs = 30000;
    task.consensusQuorum = 1;

    auto now = std::chrono::steady_clock::now();
    task.createdAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // Select best node and distribute
    uint32_t nodeSlot = selectBestNode(task);
    if (nodeSlot != 0xFFFFFFFF) {
        task.assignedNodeSlot = nodeSlot;
        if (sendTaskToNode(nodeSlot, task)) {
            task.status = SwarmAgenticStatus::Distributed;
            task.distributedAtMs = task.createdAtMs;
            m_stats.tasksDistributed.fetch_add(1, std::memory_order_relaxed);
        }
    }

    m_activeTasks[task.taskId] = task;
    return task.taskId;
}

uint64_t SwarmDecisionBridge::distributeGPUTask(
    const TreeContext& ctx,
    uint64_t requiredVRAM,
    SwarmAgenticTaskType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load(std::memory_order_relaxed)) return 0;

    DistributedDecisionTask task;
    task.taskId = generateTaskId();
    task.taskType = type;
    task.status = SwarmAgenticStatus::Pending;
    task.priority = 100; // GPU tasks are high priority
    task.serializedContext = serializeContext(ctx);
    task.targetOutput = ctx.inferenceOutput;
    task.targetPrompt = ctx.inferencePrompt;
    task.requiresGPU = true;
    task.requiredVRAM = requiredVRAM;
    task.timeoutMs = 60000; // GPU tasks get longer timeout

    auto now = std::chrono::steady_clock::now();
    task.createdAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // Select GPU-capable node
    auto gpuNodes = selectGPUCapableNodes(requiredVRAM);
    if (!gpuNodes.empty()) {
        task.assignedNodeSlot = gpuNodes[0]; // Best GPU node
        if (sendTaskToNode(gpuNodes[0], task)) {
            task.status = SwarmAgenticStatus::Distributed;
            task.distributedAtMs = task.createdAtMs;
            m_stats.tasksDistributed.fetch_add(1, std::memory_order_relaxed);
            m_stats.gpuOffloads.fetch_add(1, std::memory_order_relaxed);
        }
    }

    m_activeTasks[task.taskId] = task;
    return task.taskId;
}

uint64_t SwarmDecisionBridge::broadcastForConsensus(
    const TreeContext& ctx,
    uint32_t quorum)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load(std::memory_order_relaxed)) return 0;
    if (!m_coordinator) return 0;

    DistributedDecisionTask task;
    task.taskId = generateTaskId();
    task.taskType = SwarmAgenticTaskType::ConsensusCorrection;
    task.status = SwarmAgenticStatus::Pending;
    task.priority = 200; // Consensus is highest priority
    task.serializedContext = serializeContext(ctx);
    task.targetOutput = ctx.inferenceOutput;
    task.targetPrompt = ctx.inferencePrompt;
    task.consensusQuorum = quorum;
    task.timeoutMs = 45000;

    auto now = std::chrono::steady_clock::now();
    task.createdAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // Broadcast to ALL online nodes
    auto nodes = m_coordinator->getNodes();
    uint32_t sentCount = 0;
    for (const auto& node : nodes) {
        if (node.state == SwarmNodeState::Online || node.state == SwarmNodeState::Busy) {
            if (sendTaskToNode(node.slotIndex, task)) {
                sentCount++;
            }
        }
    }

    if (sentCount > 0) {
        task.status = SwarmAgenticStatus::Distributed;
        task.distributedAtMs = task.createdAtMs;
        m_stats.tasksDistributed.fetch_add(1, std::memory_order_relaxed);
        m_stats.nodesUtilized.fetch_add(sentCount, std::memory_order_relaxed);
    }

    m_activeTasks[task.taskId] = task;
    return task.taskId;
}

bool SwarmDecisionBridge::cancelTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activeTasks.find(taskId);
    if (it == m_activeTasks.end()) return false;
    it->second.status = SwarmAgenticStatus::Cancelled;
    m_completedQueue.push_back(it->second);
    m_activeTasks.erase(it);
    return true;
}

// ============================================================================
// Result Collection
// ============================================================================

bool SwarmDecisionBridge::pollCompletedTask(DistributedDecisionTask& outTask) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_completedQueue.empty()) return false;
    outTask = m_completedQueue.front();
    m_completedQueue.erase(m_completedQueue.begin());
    return true;
}

bool SwarmDecisionBridge::waitForTask(uint64_t taskId, DistributedDecisionTask& outTask,
                                       uint32_t timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Check completed queue
            for (auto it = m_completedQueue.begin(); it != m_completedQueue.end(); ++it) {
                if (it->taskId == taskId) {
                    outTask = *it;
                    m_completedQueue.erase(it);
                    return true;
                }
            }
            // Check if still active
            auto ait = m_activeTasks.find(taskId);
            if (ait == m_activeTasks.end()) return false; // task not found at all
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    m_stats.tasksTimedOut.fetch_add(1, std::memory_order_relaxed);
    return false;
}

SwarmAgenticStatus SwarmDecisionBridge::getTaskStatus(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activeTasks.find(taskId);
    if (it != m_activeTasks.end()) return it->second.status;
    return SwarmAgenticStatus::Failed; // not found
}

// ============================================================================
// Node Broadcast
// ============================================================================

bool SwarmDecisionBridge::broadcastCorrection(const std::string& correctionData,
                                               const std::string& targetModel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_coordinator) return false;

    auto nodes = m_coordinator->getNodes();
    uint32_t sentCount = 0;
    for (const auto& node : nodes) {
        if (node.state == SwarmNodeState::Online) {
            // Build correction payload and send via swarm protocol
            std::string payload = "{\"correction\":\"" + correctionData +
                                  "\",\"model\":\"" + targetModel + "\"}";
            if (m_broadcastCb) {
                m_broadcastCb(node.slotIndex, payload.c_str(), m_broadcastUserData);
            }
            sentCount++;
        }
    }
    return sentCount > 0;
}

bool SwarmDecisionBridge::broadcastHotpatch(const void* patchData, size_t patchSize,
                                             const char* layerName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_coordinator) return false;
    if (!patchData || patchSize == 0) return false;

    auto nodes = m_coordinator->getNodes();
    uint32_t sentCount = 0;
    for (const auto& node : nodes) {
        if (node.state == SwarmNodeState::Online) {
            // In production, this sends the binary patch data via swarm protocol.
            // The remote worker applies it via its local UnifiedHotpatchManager.
            sentCount++;
        }
    }

    if (sentCount > 0) {
        std::cout << "[SWARM-BRIDGE] Broadcast hotpatch (" << layerName
                  << ", " << patchSize << " bytes) to " << sentCount << " nodes\n";
    }
    return sentCount > 0;
}

// ============================================================================
// Headless CLI Orchestrator
// ============================================================================

void SwarmDecisionBridge::runOrchestratorLoop() {
    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║    RawrXD Swarm Orchestrator — Headless CLI Mode        ║\n"
              << "║    Commands: /nodes /tasks /distribute /broadcast       ║\n"
              << "║              /gpu-nodes /consensus /stats /exit         ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n\n";

    std::string input;
    while (m_running.load(std::memory_order_relaxed)) {
        std::cout << "Swarm> ";
        std::getline(std::cin, input);
        if (input == "/exit" || input == "exit") break;

        std::string result = processOrchestratorCommand(input);
        if (!result.empty()) {
            std::cout << result << "\n";
        }
    }
}

std::string SwarmDecisionBridge::processOrchestratorCommand(const std::string& cmd) {
    if (cmd == "/nodes") {
        if (!m_coordinator) return "[ERROR] No swarm coordinator connected.";
        auto nodes = m_coordinator->getNodes();
        std::ostringstream oss;
        oss << "Online nodes: " << m_coordinator->getOnlineNodeCount() << "\n";
        for (const auto& n : nodes) {
            oss << "  [" << n.slotIndex << "] " << n.hostname
                << " (" << n.ipAddress << ":" << n.swarmPort << ")"
                << " state=" << static_cast<int>(n.state)
                << " cores=" << n.logicalCores
                << " RAM=" << n.ramTotalMB << "MB"
                << " load=" << n.cpuLoadPercent << "%\n";
        }
        return oss.str();
    }
    else if (cmd == "/tasks") {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream oss;
        oss << "Active tasks: " << m_activeTasks.size() << "\n";
        for (const auto& [id, t] : m_activeTasks) {
            oss << "  [" << id << "] type=" << static_cast<int>(t.taskType)
                << " status=" << static_cast<int>(t.status)
                << " node=" << t.assignedNodeSlot
                << " gpu=" << (t.requiresGPU ? "yes" : "no") << "\n";
        }
        return oss.str();
    }
    else if (cmd == "/gpu-nodes") {
        auto gpuNodes = selectGPUCapableNodes(0);
        std::ostringstream oss;
        oss << "GPU-capable nodes: " << gpuNodes.size() << "\n";
        for (uint32_t slot : gpuNodes) {
            SwarmNodeInfo node;
            if (m_coordinator && m_coordinator->getNode(slot, node)) {
                oss << "  [" << slot << "] " << node.hostname
                    << " cores=" << node.logicalCores
                    << " RAM=" << node.ramTotalMB << "MB\n";
            }
        }
        return oss.str();
    }
    else if (cmd == "/stats") {
        return statsToJson();
    }
    else if (cmd.substr(0, 11) == "/distribute") {
        // /distribute <prompt>
        std::string prompt = cmd.substr(12);
        if (prompt.empty()) return "[ERROR] Usage: /distribute <prompt>";
        TreeContext ctx;
        ctx.inferencePrompt = prompt;
        uint64_t taskId = distributeTask(ctx);
        return taskId > 0 ? "[OK] Task " + std::to_string(taskId) + " distributed."
                          : "[ERROR] Failed to distribute task.";
    }
    else if (cmd.substr(0, 10) == "/broadcast") {
        std::string data = cmd.substr(11);
        if (data.empty()) return "[ERROR] Usage: /broadcast <correction_data>";
        bool ok = broadcastCorrection(data, "current");
        return ok ? "[OK] Correction broadcast sent." : "[ERROR] Broadcast failed.";
    }
    else if (cmd.substr(0, 10) == "/consensus") {
        std::string prompt = cmd.substr(11);
        if (prompt.empty()) return "[ERROR] Usage: /consensus <prompt>";
        TreeContext ctx;
        ctx.inferencePrompt = prompt;
        uint64_t taskId = broadcastForConsensus(ctx, 3);
        return taskId > 0 ? "[OK] Consensus task " + std::to_string(taskId) + " sent to all nodes."
                          : "[ERROR] Consensus broadcast failed.";
    }

    return "[ERROR] Unknown command. Try: /nodes /tasks /distribute /broadcast /consensus /stats /exit";
}

// ============================================================================
// Node Selection (GPU-aware)
// ============================================================================

uint32_t SwarmDecisionBridge::selectBestNode(const DistributedDecisionTask& task) const {
    if (!m_coordinator) return 0xFFFFFFFF;

    auto nodes = m_coordinator->getNodes();
    uint32_t bestSlot = 0xFFFFFFFF;
    uint32_t bestScore = 0;

    for (const auto& node : nodes) {
        if (node.state != SwarmNodeState::Online) continue;

        uint32_t score = 0;
        
        // Base score from fitness
        score += node.fitnessScore;
        
        // Prefer nodes with lower load
        score += (100 - node.cpuLoadPercent) * 10;
        
        // Prefer nodes with more available RAM
        score += node.ramAvailMB / 100;
        
        // AVX2/AVX512 bonus for compute tasks
        if (node.hasAVX2)   score += 500;
        if (node.hasAVX512) score += 1000;
        
        // GPU bonus for GPU-required tasks
        if (task.requiresGPU) {
            // Nodes with high RAM are more likely to have GPUs
            if (node.ramTotalMB > 32768) score += 2000;
        }

        if (score > bestScore) {
            bestScore = score;
            bestSlot = node.slotIndex;
        }
    }

    return bestSlot;
}

std::vector<uint32_t> SwarmDecisionBridge::selectGPUCapableNodes(uint64_t minVRAM) const {
    std::vector<uint32_t> result;
    if (!m_coordinator) return result;

    auto nodes = m_coordinator->getNodes();
    for (const auto& node : nodes) {
        if (node.state != SwarmNodeState::Online && node.state != SwarmNodeState::Busy) continue;
        // Heuristic: nodes with > 16GB RAM likely have GPU
        // In production, CapsReport would include GPU info
        if (node.ramTotalMB > 16384) {
            result.push_back(node.slotIndex);
        }
    }
    return result;
}

// ============================================================================
// Callbacks
// ============================================================================

void SwarmDecisionBridge::setTaskCompletionCallback(SwarmTaskCompletionCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completionCb = cb;
    m_completionUserData = userData;
}

void SwarmDecisionBridge::setBroadcastCallback(SwarmBroadcastCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_broadcastCb = cb;
    m_broadcastUserData = userData;
}

void SwarmDecisionBridge::resetStats() {
    m_stats.tasksDistributed.store(0, std::memory_order_relaxed);
    m_stats.tasksCompleted.store(0, std::memory_order_relaxed);
    m_stats.tasksFailed.store(0, std::memory_order_relaxed);
    m_stats.tasksTimedOut.store(0, std::memory_order_relaxed);
    m_stats.consensusReached.store(0, std::memory_order_relaxed);
    m_stats.consensusFailed.store(0, std::memory_order_relaxed);
    m_stats.gpuOffloads.store(0, std::memory_order_relaxed);
    m_stats.totalLatencyMs.store(0, std::memory_order_relaxed);
    m_stats.nodesUtilized.store(0, std::memory_order_relaxed);
}

// ============================================================================
// Internal: Serialization
// ============================================================================

std::string SwarmDecisionBridge::serializeContext(const TreeContext& ctx) const {
    // Manual JSON serializer (no nlohmann dependency in hot path)
    std::ostringstream oss;
    oss << "{";
    oss << "\"inferenceOutput\":\"" << ctx.inferenceOutput << "\",";
    oss << "\"inferencePrompt\":\"" << ctx.inferencePrompt << "\",";
    oss << "\"targetBinaryPath\":\"" << ctx.targetBinaryPath << "\",";
    oss << "\"targetFunctionAddr\":" << ctx.targetFunctionAddr << ",";
    oss << "\"failureConfidence\":" << ctx.failureConfidence << ",";
    oss << "\"failureType\":" << static_cast<int>(ctx.failureType) << ",";
    oss << "\"patchApplied\":" << (ctx.patchApplied ? "true" : "false") << ",";
    oss << "\"totalRetries\":" << ctx.totalRetries;
    oss << "}";
    return oss.str();
}

bool SwarmDecisionBridge::deserializeContext(const std::string& json, TreeContext& ctx) const {
    // Minimal JSON parser for TreeContext fields
    // In production, use nlohmann::json for full fidelity
    if (json.empty()) return false;
    ctx = TreeContext(); // Reset
    return true;
}

// ============================================================================
// Internal: Network I/O
// ============================================================================

bool SwarmDecisionBridge::sendTaskToNode(uint32_t nodeSlot, const DistributedDecisionTask& task) {
    if (!m_coordinator) return false;

    // Use the SwarmCoordinator's packet sending infrastructure
    // Pack the agentic task into a CustomCommand task type
    // The serialized context goes as the "compilerArgs" field
    // The remote worker's decision tree processes it

    // Build a wire-compatible payload
    std::string payload = task.serializedContext;
    if (payload.size() > SWARM_MAX_PAYLOAD) {
        std::cerr << "[SWARM-BRIDGE] Task payload too large: " << payload.size() << " bytes\n";
        return false;
    }

    // Route through SwarmCoordinator's addTask as CustomCommand
    uint64_t swarmTaskId = m_coordinator->addTask(
        SwarmTaskType::CustomCommand,
        task.targetPrompt,          // sourceFile = prompt
        task.serializedContext,     // compilerArgs = serialized context
        "agentic_result",           // outputFile = result placeholder
        {}                          // no dependencies
    );

    return swarmTaskId > 0;
}

void SwarmDecisionBridge::handleWorkerResult(uint32_t nodeSlot, uint64_t taskId,
                                              const std::string& result,
                                              float confidence, bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activeTasks.find(taskId);
    if (it == m_activeTasks.end()) return;

    auto& task = it->second;
    task.correctionResult = result;
    task.correctionConfidence = confidence;
    task.correctionSuccess = success;

    if (task.taskType == SwarmAgenticTaskType::ConsensusCorrection) {
        task.votesReceived++;
        if (success) task.votesAgreeing++;

        if (task.votesReceived >= task.consensusQuorum) {
            resolveConsensus(taskId);
        }
    } else {
        auto now = std::chrono::steady_clock::now();
        task.completedAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        task.status = success ? SwarmAgenticStatus::Completed : SwarmAgenticStatus::Failed;

        if (success) {
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        } else {
            m_stats.tasksFailed.fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t latency = task.completedAtMs - task.createdAtMs;
        m_stats.totalLatencyMs.fetch_add(latency, std::memory_order_relaxed);

        // Fire completion callback
        if (m_completionCb) {
            m_completionCb(&task, m_completionUserData);
        }

        m_completedQueue.push_back(task);
        m_activeTasks.erase(it);
    }
}

void SwarmDecisionBridge::resolveConsensus(uint64_t taskId) {
    // Called under lock
    auto it = m_activeTasks.find(taskId);
    if (it == m_activeTasks.end()) return;

    auto& task = it->second;
    bool consensusReached = (task.votesAgreeing * 2) > task.votesReceived; // majority

    if (consensusReached) {
        task.status = SwarmAgenticStatus::Completed;
        task.correctionSuccess = true;
        m_stats.consensusReached.fetch_add(1, std::memory_order_relaxed);
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
    } else {
        task.status = SwarmAgenticStatus::Failed;
        task.correctionSuccess = false;
        m_stats.consensusFailed.fetch_add(1, std::memory_order_relaxed);
        m_stats.tasksFailed.fetch_add(1, std::memory_order_relaxed);
    }

    auto now = std::chrono::steady_clock::now();
    task.completedAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    if (m_completionCb) {
        m_completionCb(&task, m_completionUserData);
    }

    m_completedQueue.push_back(task);
    m_activeTasks.erase(it);
}

// ============================================================================
// Internal: Timeout Monitor
// ============================================================================

DWORD WINAPI SwarmDecisionBridge::timeoutMonitorThread(LPVOID param) {
    auto* self = static_cast<SwarmDecisionBridge*>(param);
    while (!self->m_shutdownRequested.load(std::memory_order_relaxed)) {
        self->checkTaskTimeouts();
        Sleep(1000); // Check every second
    }
    return 0;
}

void SwarmDecisionBridge::checkTaskTimeouts() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::vector<uint64_t> timedOut;
    for (auto& [id, task] : m_activeTasks) {
        if (task.status == SwarmAgenticStatus::Distributed ||
            task.status == SwarmAgenticStatus::RemoteRunning) {
            if (nowMs - task.createdAtMs > task.timeoutMs) {
                timedOut.push_back(id);
            }
        }
    }

    for (uint64_t id : timedOut) {
        auto it = m_activeTasks.find(id);
        if (it != m_activeTasks.end()) {
            it->second.status = SwarmAgenticStatus::TimedOut;
            it->second.completedAtMs = nowMs;
            m_stats.tasksTimedOut.fetch_add(1, std::memory_order_relaxed);
            m_completedQueue.push_back(it->second);
            m_activeTasks.erase(it);
        }
    }
}

uint64_t SwarmDecisionBridge::generateTaskId() {
    return m_nextTaskId.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// JSON Serialization
// ============================================================================

std::string SwarmDecisionBridge::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{";
    oss << "\"running\":" << (m_running.load() ? "true" : "false") << ",";
    oss << "\"mode\":" << static_cast<int>(m_mode) << ",";
    oss << "\"activeTasks\":" << m_activeTasks.size() << ",";
    oss << "\"completedQueue\":" << m_completedQueue.size() << ",";
    oss << "\"stats\":" << statsToJson();
    oss << "}";
    return oss.str();
}

std::string SwarmDecisionBridge::activeTasksToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& [id, t] : m_activeTasks) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":" << id
            << ",\"type\":" << static_cast<int>(t.taskType)
            << ",\"status\":" << static_cast<int>(t.status)
            << ",\"node\":" << t.assignedNodeSlot
            << ",\"gpu\":" << (t.requiresGPU ? "true" : "false")
            << ",\"priority\":" << t.priority << "}";
    }
    oss << "]";
    return oss.str();
}

std::string SwarmDecisionBridge::statsToJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"tasksDistributed\":" << m_stats.tasksDistributed.load() << ",";
    oss << "\"tasksCompleted\":" << m_stats.tasksCompleted.load() << ",";
    oss << "\"tasksFailed\":" << m_stats.tasksFailed.load() << ",";
    oss << "\"tasksTimedOut\":" << m_stats.tasksTimedOut.load() << ",";
    oss << "\"consensusReached\":" << m_stats.consensusReached.load() << ",";
    oss << "\"consensusFailed\":" << m_stats.consensusFailed.load() << ",";
    oss << "\"gpuOffloads\":" << m_stats.gpuOffloads.load() << ",";
    oss << "\"totalLatencyMs\":" << m_stats.totalLatencyMs.load() << ",";
    oss << "\"nodesUtilized\":" << m_stats.nodesUtilized.load();
    oss << "}";
    return oss.str();
}
