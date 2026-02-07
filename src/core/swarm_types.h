// =============================================================================
// swarm_types.h — Phase 11: Distributed Swarm Compilation Shared Types
// =============================================================================
// Core types used by SwarmCoordinator, SwarmWorker, SwarmDiscovery, and the
// IDE integration layer. All types are plain C++ structs with no virtual
// functions. Threading uses std::mutex / std::atomic only.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_SWARM_TYPES_H
#define RAWRXD_SWARM_TYPES_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

#include "swarm_protocol.h"

// =============================================================================
//                           ENUMERATIONS
// =============================================================================

// State of a swarm node from the coordinator's perspective.
enum class SwarmNodeState : uint8_t {
    Unknown     = 0,
    Discovered  = 1,    // Found via UDP broadcast, not yet attested
    Attesting   = 2,    // Challenge-response in progress
    Online      = 3,    // Authenticated and ready for tasks
    Busy        = 4,    // Currently executing tasks at capacity
    Draining    = 5,    // No new tasks, finishing current work
    Offline     = 6,    // Missed heartbeats, considered dead
    Blacklisted = 7     // Failed attestation or returned bad results
};

// Type of compile task in the DAG.
enum class SwarmTaskType : uint8_t {
    CompileCpp      = 0,    // .cpp → .obj
    CompileC        = 1,    // .c → .obj
    AssembleMASM    = 2,    // .asm → .obj (ml64)
    AssembleNASM    = 3,    // .asm → .obj (nasm)
    LinkPartial     = 4,    // Partial link (multiple .obj → .lib)
    LinkFinal       = 5,    // Final link (.obj + .lib → .exe/.dll)
    GenerateHeader  = 6,    // Code generation task
    CustomCommand   = 7     // Arbitrary shell command
};

// State of an individual task.
enum class SwarmTaskState : uint8_t {
    Pending     = 0,    // Waiting for dependencies
    Ready       = 1,    // All dependencies met, can be scheduled
    Assigned    = 2,    // Sent to a worker
    Running     = 3,    // Worker acknowledged execution start
    Completed   = 4,    // Successfully compiled
    Failed      = 5,    // Compilation error
    Cancelled   = 6,    // Cancelled by user or dependency failure
    Verifying   = 7,    // Result received, awaiting consensus
    Retrying    = 8     // Failed, being re-sent to another node
};

// Leader election state (lightweight Raft-inspired).
enum class SwarmLeaderState : uint8_t {
    Follower    = 0,
    Candidate   = 1,
    Leader      = 2
};

// Swarm operational mode.
enum class SwarmMode : uint8_t {
    Disabled    = 0,    // Swarm not active
    Leader      = 1,    // This node is the coordinator
    Worker      = 2,    // This node is a compute worker
    Hybrid      = 3     // Both leader and worker (small clusters)
};

// =============================================================================
//                          NODE INFORMATION
// =============================================================================

// Represents a single node in the swarm cluster.
struct SwarmNodeInfo {
    uint8_t         nodeId[16];             // 128-bit unique identifier (HWID hash)
    char            hostname[64];           // Human-readable name
    char            ipAddress[46];          // IPv4 or IPv6 string
    uint16_t        swarmPort;              // TCP port for swarm protocol
    uint16_t        httpPort;               // HTTP status port (0 = none)
    SwarmNodeState  state;                  // Current state
    uint8_t         pad0[3];
    uint32_t        slotIndex;              // Index in coordinator's node table
    uint32_t        fitnessScore;           // CPUID-based fitness
    uint32_t        logicalCores;           // Logical CPUs
    uint32_t        physicalCores;          // Physical cores
    uint32_t        ramTotalMB;
    uint32_t        ramAvailMB;
    uint32_t        hasAVX2;                // Boolean
    uint32_t        hasAVX512;              // Boolean
    uint32_t        hasAESNI;               // Boolean
    uint32_t        maxConcurrentTasks;     // Max parallel compiles
    uint32_t        activeTasks;            // Currently running tasks
    uint32_t        completedTasks;         // Lifetime completed
    uint32_t        failedTasks;            // Lifetime failed
    uint32_t        cpuLoadPercent;         // Last reported CPU load
    uint32_t        memUsedMB;              // Last reported memory usage
    uint64_t        lastHeartbeatMs;        // Epoch ms of last heartbeat
    uint64_t        joinedAtMs;             // Epoch ms when node joined
    uint64_t        uptimeMs;               // Node-reported uptime
    uint64_t        avgCompileTimeUs;       // Average compile time
    uint32_t        sequenceId;             // Last seen sequence ID
    uint32_t        missedHeartbeats;       // Consecutive missed heartbeats
    uint64_t        totalBytesSent;         // Network stats
    uint64_t        totalBytesRecv;
    SOCKET          socket;                 // TCP connection to this node (or INVALID_SOCKET)
};

// =============================================================================
//                         COMPILE TASKLET
// =============================================================================

// A single unit of work in the distributed build DAG.
struct SwarmTasklet {
    uint64_t        taskId;                 // Unique identifier
    uint64_t        parentTaskId;           // Parent in DAG (0 = root)
    SwarmTaskType   taskType;
    SwarmTaskState  taskState;
    uint8_t         retryCount;
    uint8_t         pad0;
    uint32_t        priority;               // Higher = more urgent
    uint32_t        assignedNode;           // Node slot index (UINT32_MAX = unassigned)
    uint64_t        dagGeneration;          // DAG version when created
    std::string     sourceFile;             // Source file path
    std::string     compilerArgs;           // Compiler arguments
    std::string     outputFile;             // Expected output path
    uint64_t        contentHash;            // xxHash64 of source content
    uint64_t        resultHash;             // xxHash64 of compiled output (0 until done)
    uint32_t        objectFileSize;         // Size of compiled output
    uint32_t        exitCode;               // Compiler exit code
    uint32_t        warningCount;
    uint32_t        errorCount;
    uint32_t        compileTimeMs;          // Wall-clock compile time on worker
    uint32_t        pad1;
    uint64_t        createdAtMs;            // When task was created
    uint64_t        assignedAtMs;           // When task was assigned
    uint64_t        completedAtMs;          // When result was received
    std::string     compilerLog;            // Captured stdout+stderr
    std::vector<uint64_t> dependsOn;        // Task IDs that must complete first
    std::vector<uint64_t> dependedBy;       // Tasks waiting on this one

    SwarmTasklet()
        : taskId(0), parentTaskId(0)
        , taskType(SwarmTaskType::CompileCpp)
        , taskState(SwarmTaskState::Pending)
        , retryCount(0), pad0(0)
        , priority(0), assignedNode(0xFFFFFFFF)
        , dagGeneration(0)
        , contentHash(0), resultHash(0)
        , objectFileSize(0), exitCode(0)
        , warningCount(0), errorCount(0)
        , compileTimeMs(0), pad1(0)
        , createdAtMs(0), assignedAtMs(0), completedAtMs(0)
    {}
};

// =============================================================================
//                        TASK GRAPH (DAG)
// =============================================================================

// The build dependency graph managed by the coordinator.
struct SwarmTaskGraph {
    uint64_t                    generation;         // Monotonic version counter
    std::vector<SwarmTasklet>   tasks;              // All tasks in the graph
    uint32_t                    totalTasks;
    uint32_t                    completedTasks;
    uint32_t                    failedTasks;
    uint32_t                    runningTasks;
    uint32_t                    pendingTasks;
    uint32_t                    cancelledTasks;
    uint64_t                    startTimeMs;        // When build started
    uint64_t                    endTimeMs;          // When build finished (0 = in progress)
    mutable std::mutex           graphMutex;

    SwarmTaskGraph()
        : generation(0), totalTasks(0), completedTasks(0)
        , failedTasks(0), runningTasks(0), pendingTasks(0)
        , cancelledTasks(0), startTimeMs(0), endTimeMs(0)
    {}

    // Non-copyable due to mutex
    SwarmTaskGraph(const SwarmTaskGraph&) = delete;
    SwarmTaskGraph& operator=(const SwarmTaskGraph&) = delete;

    // Move constructor
    SwarmTaskGraph(SwarmTaskGraph&& other) noexcept
        : generation(other.generation)
        , tasks(std::move(other.tasks))
        , totalTasks(other.totalTasks)
        , completedTasks(other.completedTasks)
        , failedTasks(other.failedTasks)
        , runningTasks(other.runningTasks)
        , pendingTasks(other.pendingTasks)
        , cancelledTasks(other.cancelledTasks)
        , startTimeMs(other.startTimeMs)
        , endTimeMs(other.endTimeMs)
    {}
};

// =============================================================================
//                        CONSENSUS STATE
// =============================================================================

// Tracks verification votes for a single task result.
struct ConsensusEntry {
    uint64_t    taskId;
    uint64_t    expectedHash;       // Hash reported by the first responder
    uint32_t    totalVotes;
    uint32_t    acceptVotes;
    uint32_t    rejectVotes;
    uint32_t    requiredVotes;      // Quorum threshold
    bool        committed;
    bool        rejected;
    uint8_t     pad0[6];
    std::vector<uint32_t> voterNodes;   // Node slot indices that voted

    ConsensusEntry()
        : taskId(0), expectedHash(0)
        , totalVotes(0), acceptVotes(0), rejectVotes(0)
        , requiredVotes(1), committed(false), rejected(false)
        , pad0{}
    {}
};

// =============================================================================
//                        SWARM STATISTICS
// =============================================================================

struct SwarmStats {
    // Cluster stats
    uint32_t    totalNodes;
    uint32_t    onlineNodes;
    uint32_t    busyNodes;
    uint32_t    offlineNodes;

    // Task stats
    uint32_t    totalTasks;
    uint32_t    completedTasks;
    uint32_t    failedTasks;
    uint32_t    runningTasks;
    uint32_t    pendingTasks;
    uint32_t    retriedTasks;

    // Performance
    uint64_t    totalCompileTimeMs;     // Sum of all compile times
    uint64_t    avgCompileTimeMs;       // Average compile time
    uint64_t    maxCompileTimeMs;       // Worst-case compile time
    uint64_t    totalBuildTimeMs;       // Wall-clock total build time
    double      parallelSpeedup;        // Speedup vs serial build
    uint32_t    objectCacheHits;        // Content-addressable cache hits
    uint32_t    objectCacheMisses;

    // Network stats
    uint64_t    totalBytesSent;
    uint64_t    totalBytesRecv;
    uint64_t    totalPacketsSent;
    uint64_t    totalPacketsRecv;
    uint32_t    ringOverflows;
    uint32_t    checksumFailures;
    uint32_t    heartbeatTimeouts;
    uint32_t    attestationFailures;

    // Consensus
    uint32_t    consensusCommits;
    uint32_t    consensusRejects;

    SwarmStats() { memset(this, 0, sizeof(SwarmStats)); }
};

// =============================================================================
//                     SWARM CONFIGURATION
// =============================================================================
// Named DscConfig (Distributed Swarm Compilation) to avoid collision with
// the HexMag SwarmConfig in subagent_core.h from Phase 7.

struct DscConfig {
    // Network
    uint16_t    swarmPort;              // TCP port (default 11437)
    uint16_t    discoveryPort;          // UDP broadcast port (default 11436)
    uint16_t    httpPort;               // HTTP status port (0 = same as IDE)
    uint16_t    pad0;

    // Behavior
    SwarmMode   mode;                   // Leader / Worker / Hybrid
    uint8_t     pad1[3];
    uint32_t    maxNodes;               // Max nodes in cluster
    uint32_t    maxConcurrentTasks;     // Max parallel tasks per worker
    uint32_t    heartbeatIntervalMs;    // Heartbeat send interval
    uint32_t    heartbeatTimeoutMs;     // Missed heartbeat threshold
    uint32_t    taskTimeoutMs;          // Per-task timeout
    uint32_t    maxRetries;             // Max task retries
    uint32_t    consensusQuorum;        // Votes needed (0 = auto = ceil(nodes/2))

    // Security
    bool        requireAttestation;     // Require HWID attestation
    bool        encryptTraffic;         // Future: TLS for swarm protocol
    uint8_t     pad2[2];
    char        sharedSecret[64];       // Pre-shared key for attestation HMAC

    // Paths
    char        compilerPath[256];      // Local compiler executable
    char        objectCachePath[256];   // Content-addressable object cache

    // Discovery
    char        broadcastAddress[46];   // Broadcast/multicast address
    uint8_t     pad3[2];
    uint32_t    discoveryIntervalMs;    // How often to send discovery pings

    DscConfig() {
        memset(this, 0, sizeof(DscConfig));
        swarmPort           = SWARM_DEFAULT_PORT;
        discoveryPort       = SWARM_DISCOVERY_PORT;
        httpPort            = 0;
        mode                = SwarmMode::Disabled;
        maxNodes            = SWARM_MAX_NODES;
        maxConcurrentTasks  = 4;
        heartbeatIntervalMs = SWARM_HEARTBEAT_INTERVAL_MS;
        heartbeatTimeoutMs  = SWARM_HEARTBEAT_TIMEOUT_MS;
        taskTimeoutMs       = SWARM_TASK_TIMEOUT_MS;
        maxRetries          = SWARM_MAX_RETRIES;
        consensusQuorum     = 0;
        requireAttestation  = true;
        encryptTraffic      = false;
        discoveryIntervalMs = 5000;
        strncpy(broadcastAddress, "255.255.255.255", sizeof(broadcastAddress) - 1);
    }
};

// =============================================================================
//                     SWARM EVENT TYPES
// =============================================================================

// Events emitted by the swarm system for the IDE to display.
enum class SwarmEventType : uint8_t {
    NodeJoined          = 0,
    NodeLeft            = 1,
    NodeBlacklisted     = 2,
    TaskAssigned        = 3,
    TaskCompleted       = 4,
    TaskFailed          = 5,
    TaskRetried         = 6,
    BuildStarted        = 7,
    BuildCompleted      = 8,
    BuildFailed         = 9,
    ConsensusReached    = 10,
    ConsensusRejected   = 11,
    HeartbeatTimeout    = 12,
    AttestationSuccess  = 13,
    AttestationFailure  = 14,
    ConfigChanged       = 15,
    LeaderElected       = 16,
    LeaderStepDown      = 17,
    SwarmStarted        = 18,
    SwarmStopped        = 19
};

struct SwarmEvent {
    SwarmEventType  type;
    uint8_t         pad0[3];
    uint32_t        nodeSlotIndex;      // Related node (UINT32_MAX = n/a)
    uint64_t        taskId;             // Related task (0 = n/a)
    uint64_t        timestampMs;        // Epoch ms
    char            message[256];       // Human-readable description

    SwarmEvent()
        : type(SwarmEventType::SwarmStarted)
        , pad0{}, nodeSlotIndex(0xFFFFFFFF)
        , taskId(0), timestampMs(0)
    {
        message[0] = '\0';
    }
};

// =============================================================================
//                 CONTENT-ADDRESSABLE OBJECT CACHE
// =============================================================================

// Entry in the local object cache (xxHash64 → file path).
struct ObjectCacheEntry {
    uint64_t    contentHash;        // xxHash64 of source + compiler args
    uint64_t    resultHash;         // xxHash64 of compiled .obj
    uint32_t    objectSize;         // File size in bytes
    uint32_t    hitCount;           // Cache hit counter
    uint64_t    createdAtMs;        // When cached
    uint64_t    lastAccessMs;       // Last cache hit
    std::string objectPath;         // Path to cached .obj file
    std::string sourceFile;         // Original source path (for display)

    ObjectCacheEntry()
        : contentHash(0), resultHash(0), objectSize(0), hitCount(0)
        , createdAtMs(0), lastAccessMs(0)
    {}
};

// =============================================================================
//                     UTILITY: TIME HELPERS
// =============================================================================

namespace SwarmTime {
    inline uint64_t nowMs() {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count());
    }

    inline uint64_t nowUs() {
        auto now = std::chrono::high_resolution_clock::now();
        auto epoch = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
    }
}

#endif // RAWRXD_SWARM_TYPES_H
