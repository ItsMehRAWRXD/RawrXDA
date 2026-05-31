// =============================================================================
// swarm_protocol.h — Phase 11: Distributed Swarm Compilation Wire Protocol
// =============================================================================
// Defines the binary wire format for all inter-node communication in the
// RawrXD Distributed Swarm Compilation (DSC) system.
//
// Layout (64-byte fixed header + variable payload, max 64KB):
//
//   Offset  Size  Field
//   ------  ----  -----
//    0       4    magic           (0x52575244 = 'RWRD')
//    4       1    version         (0x01)
//    5       1    opcode          (SwarmOpcode enum)
//    6       2    payloadLen      (uint16_t, max 65535)
//    8       4    sequenceId      (uint32_t, monotonic per-sender)
//   12       8    timestampNs     (RDTSC tick at send time)
//   20       8    taskId          (uint64_t, unique task identifier)
//   28      16    nodeId          (128-bit node identifier / HWID hash)
//   44       4    reserved        (must be zero)
//   48      16    checksum        (Blake2b-128 of payload)
//   64       N    payload         (opcode-specific, 0..65535 bytes)
//
// All multi-byte fields are little-endian (native x64).
//
// MASM kernel (RawrXD_Swarm_Network.asm) and C++ coordinator both use
// this header layout — keep synchronized.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_SWARM_PROTOCOL_H
#define RAWRXD_SWARM_PROTOCOL_H

#include <cstdint>
#include <cstring>

// =============================================================================
//                           PROTOCOL CONSTANTS
// =============================================================================

constexpr uint32_t SWARM_MAGIC           = 0x52575244;  // 'RWRD' little-endian
constexpr uint8_t  SWARM_VERSION         = 0x01;
constexpr uint16_t SWARM_HEADER_SIZE     = 64;
constexpr uint16_t SWARM_MAX_PAYLOAD     = 65535;
constexpr uint16_t SWARM_DEFAULT_PORT    = 11437;       // TCP swarm port
constexpr uint16_t SWARM_DISCOVERY_PORT  = 11436;       // UDP broadcast
constexpr uint32_t SWARM_RING_CAPACITY   = 4096;
constexpr uint32_t SWARM_RING_SLOT_SIZE  = 65600;       // Header + max payload + padding
constexpr uint32_t SWARM_MAX_NODES       = 64;
constexpr uint32_t SWARM_HEARTBEAT_INTERVAL_MS = 100;
constexpr uint32_t SWARM_HEARTBEAT_TIMEOUT_MS  = 3000;  // 3 missed intervals
constexpr uint32_t SWARM_TASK_TIMEOUT_MS       = 30000; // 30s per compile task
constexpr uint32_t SWARM_MAX_RETRIES           = 3;

// =============================================================================
//                           OPCODES
// =============================================================================

enum class SwarmOpcode : uint8_t {
    Heartbeat       = 0x01,     // Periodic keepalive
    TaskPush        = 0x02,     // Leader → Worker: compile this
    TaskPull        = 0x03,     // Worker → Leader: ready for work
    ResultPush      = 0x04,     // Worker → Leader: compilation result
    AttestRequest   = 0x05,     // Leader → New Node: prove identity
    AttestResponse  = 0x06,     // New Node → Leader: HWID + challenge response
    CapsReport      = 0x07,     // Node → Leader: capability report (cores, AVX, RAM)
    DiscoveryPing   = 0x08,     // UDP broadcast: "I exist"
    DiscoveryPong   = 0x09,     // UDP reply: "I see you, connect to me"
    DagSync         = 0x0A,     // Leader → Workers: task dependency graph update
    ShardRequest    = 0x0B,     // Worker → Leader: request missing object shard
    ShardTransfer   = 0x0C,     // Leader → Worker: object shard data
    ConsensusVote   = 0x0D,     // Verification: vote on result hash
    ConsensusCommit = 0x0E,     // Leader: result committed
    LogStream       = 0x0F,     // Worker → Leader: real-time build log
    MetricReport    = 0x10,     // Worker → Leader: performance metrics
    Shutdown        = 0xFF      // Graceful shutdown
};

// =============================================================================
//                        PACKET HEADER (64 bytes)
// =============================================================================

#pragma pack(push, 1)

struct SwarmPacketHeader {
    uint32_t magic;             //  0: 0x52575244
    uint8_t  version;           //  4: 0x01
    uint8_t  opcode;            //  5: SwarmOpcode
    uint16_t payloadLen;        //  6: payload byte count
    uint32_t sequenceId;        //  8: monotonic per-sender
    uint64_t timestampNs;       // 12: RDTSC tick
    uint64_t taskId;            // 20: unique task ID
    uint8_t  nodeId[16];        // 28: 128-bit node identifier
    uint32_t reserved;          // 44: must be zero
    uint8_t  checksum[16];      // 48: Blake2b-128 of payload
};

static_assert(sizeof(SwarmPacketHeader) == 64, "SwarmPacketHeader must be exactly 64 bytes");

// =============================================================================
//                     PAYLOAD STRUCTURES
// =============================================================================

// --- Heartbeat payload (opcode 0x01) ---
struct HeartbeatPayload {
    uint32_t nodeSlotIndex;     // Sender's slot in the node table
    uint32_t activeTasks;       // Number of currently executing tasks
    uint64_t uptimeMs;          // Node uptime in milliseconds
    uint32_t cpuLoadPercent;    // 0-100
    uint32_t memUsedMB;         // Current memory usage
    uint32_t fitnessScore;      // Cached CPUID-based fitness
    uint32_t pad0;
};

static_assert(sizeof(HeartbeatPayload) == 32, "HeartbeatPayload size check");

// --- Task Push payload (opcode 0x02) ---
// Leader sends a compile tasklet to a worker.
struct TaskPushPayload {
    uint64_t taskId;                // Unique task identifier
    uint64_t parentTaskId;          // DAG dependency (0 = root)
    uint32_t taskType;              // SwarmTaskType enum
    uint32_t priority;              // Higher = more urgent
    uint32_t sourceFileLen;         // Length of source file path
    uint32_t compilerArgsLen;       // Length of compiler arguments
    uint32_t objectHashHi;          // Expected content hash (upper 32)
    uint32_t objectHashLo;          // Expected content hash (lower 32)
    uint64_t dagGeneration;         // DAG version for stale-task detection
    // Followed by: sourceFile (null-terminated) + compilerArgs (null-terminated)
    // Total variable data = sourceFileLen + compilerArgsLen
};

static_assert(sizeof(TaskPushPayload) == 48, "TaskPushPayload size check");

// --- Task Pull payload (opcode 0x03) ---
// Worker requests work from the leader.
struct TaskPullPayload {
    uint32_t nodeSlotIndex;
    uint32_t maxConcurrentTasks;    // Worker's capacity
    uint32_t currentLoad;           // Currently executing tasks
    uint32_t preferredTaskType;     // Hint: preferred task type
    uint64_t lastCompletedTaskId;   // For continuity
};

static_assert(sizeof(TaskPullPayload) == 24, "TaskPullPayload size check");

// --- Result Push payload (opcode 0x04) ---
// Worker sends compilation result back to leader.
struct ResultPushPayload {
    uint64_t taskId;                // Which task this is for
    uint32_t exitCode;              // Compiler exit code
    uint32_t objectFileSize;        // Size of compiled .obj
    uint64_t objectHash;            // xxHash64 of the .obj contents
    uint32_t compileTimeMs;         // Wall-clock compile time
    uint32_t warningCount;          // Number of warnings
    uint32_t errorCount;            // Number of errors
    uint32_t logLen;                // Length of attached compiler log
    // Followed by: object file data (objectFileSize bytes)
    // Then: compiler log (logLen bytes, null-terminated)
};

static_assert(sizeof(ResultPushPayload) == 40, "ResultPushPayload size check");

// --- Attestation Request (opcode 0x05) ---
struct AttestRequestPayload {
    uint8_t  challenge[32];         // Random challenge bytes
    uint64_t nonce;                 // Anti-replay nonce
    uint32_t requiredVersion;       // Minimum protocol version
    uint32_t pad0;
};

static_assert(sizeof(AttestRequestPayload) == 48, "AttestRequestPayload size check");

// --- Attestation Response (opcode 0x06) ---
struct AttestResponsePayload {
    uint8_t  hwid[16];             // Machine HWID hash
    uint8_t  challengeResp[32];    // HMAC of challenge + shared secret
    uint32_t protocolVersion;      // Node's protocol version
    uint32_t fitnessScore;         // Self-reported fitness
    uint64_t uptimeMs;
};

static_assert(sizeof(AttestResponsePayload) == 64, "AttestResponsePayload size check");

// --- Capability Report (opcode 0x07) ---
struct CapsReportPayload {
    uint32_t logicalCores;          // Number of logical CPUs
    uint32_t physicalCores;         // Number of physical cores
    uint32_t ramTotalMB;            // Total RAM in MB
    uint32_t ramAvailMB;            // Available RAM in MB
    uint32_t hasAVX2;               // 1 if AVX2 supported
    uint32_t hasAVX512;             // 1 if AVX-512 supported
    uint32_t hasAESNI;              // 1 if AES-NI supported
    uint32_t maxConcurrentTasks;    // Max parallel compiles
    uint32_t fitnessScore;          // CPUID-computed score
    uint32_t osVersion;             // Windows build number
    char     hostname[64];          // Null-terminated hostname
    char     compilerPath[128];     // Path to local compiler
};

static_assert(sizeof(CapsReportPayload) == 232, "CapsReportPayload size check");

// --- Discovery Ping/Pong (opcodes 0x08, 0x09) ---
struct DiscoveryPayload {
    uint16_t swarmPort;             // TCP port for swarm protocol
    uint16_t httpPort;              // HTTP status port (0 = none)
    uint32_t fitnessScore;          // Node fitness
    uint8_t  nodeId[16];           // 128-bit node identifier
    char     hostname[64];          // Null-terminated hostname
    uint32_t isLeader;             // 1 if this node is the swarm leader
    uint32_t nodeCount;            // Known nodes in swarm (leader only)
};

static_assert(sizeof(DiscoveryPayload) == 96, "DiscoveryPayload size check");

// --- DAG Sync (opcode 0x0A) ---
struct DagSyncPayload {
    uint64_t dagGeneration;         // Monotonic DAG version
    uint32_t totalTasks;            // Total tasks in DAG
    uint32_t completedTasks;        // Already completed
    uint32_t pendingTasks;          // Waiting for dependencies
    uint32_t runningTasks;          // Currently executing
    // Followed by: array of DagEdge structs
};

static_assert(sizeof(DagSyncPayload) == 24, "DagSyncPayload size check");

struct DagEdge {
    uint64_t fromTaskId;
    uint64_t toTaskId;
};

static_assert(sizeof(DagEdge) == 16, "DagEdge size check");

// --- Shard Request/Transfer (opcodes 0x0B, 0x0C) ---
struct ShardRequestPayload {
    uint64_t objectHash;            // Hash of needed object
    uint64_t taskId;                // Requesting task context
    uint32_t offset;                // Start offset (for chunked transfer)
    uint32_t maxLen;                // Max bytes to receive
};

static_assert(sizeof(ShardRequestPayload) == 24, "ShardRequestPayload size check");

struct ShardTransferPayload {
    uint64_t objectHash;            // Hash of object being sent
    uint32_t offset;                // Data offset within object
    uint32_t totalSize;             // Total object size
    uint32_t chunkSize;             // Size of this chunk
    uint32_t isLast;                // 1 if this is the final chunk
    // Followed by: chunkSize bytes of object data
};

static_assert(sizeof(ShardTransferPayload) == 24, "ShardTransferPayload size check");

// --- Consensus Vote (opcode 0x0D) ---
struct ConsensusVotePayload {
    uint64_t taskId;
    uint64_t objectHash;            // Voter's computed hash of result
    uint32_t nodeSlotIndex;         // Voter's identity
    uint32_t verdict;               // 1=accept, 0=reject
};

static_assert(sizeof(ConsensusVotePayload) == 24, "ConsensusVotePayload size check");

// --- Consensus Commit (opcode 0x0E) ---
struct ConsensusCommitPayload {
    uint64_t taskId;
    uint64_t finalHash;             // Consensus-agreed hash
    uint32_t voteCount;             // Number of voters
    uint32_t acceptCount;           // Number of accept votes
    uint32_t rejectCount;           // Number of reject votes
    uint32_t pad0;
};

static_assert(sizeof(ConsensusCommitPayload) == 32, "ConsensusCommitPayload size check");

// --- Log Stream (opcode 0x0F) ---
struct LogStreamPayload {
    uint64_t taskId;
    uint32_t nodeSlotIndex;
    uint32_t logLevel;              // 0=debug, 1=info, 2=warn, 3=error
    uint32_t logLen;                // Length of log text
    uint32_t pad0;
    // Followed by: logLen bytes of null-terminated text
};

static_assert(sizeof(LogStreamPayload) == 24, "LogStreamPayload size check");

// --- Metric Report (opcode 0x10) ---
struct MetricReportPayload {
    uint32_t nodeSlotIndex;
    uint32_t cpuPercent;            // 0-100
    uint32_t memUsedMB;
    uint32_t diskIOBytesPerSec;
    uint32_t netTxBytesPerSec;
    uint32_t netRxBytesPerSec;
    uint32_t tasksCompleted;        // Since last report
    uint32_t tasksFailed;
    uint64_t avgCompileTimeUs;      // Average compile time in microseconds
    uint64_t uptimeMs;
};

static_assert(sizeof(MetricReportPayload) == 48, "MetricReportPayload size check");

#pragma pack(pop)

// =============================================================================
//                     EXTERN MASM FUNCTIONS
// =============================================================================
// These are implemented in RawrXD_Swarm_Network.asm

extern "C" {
    // Ring buffer operations
    int      Swarm_RingBuffer_Init(void* ring, void* buffer);
    int      Swarm_RingBuffer_Push(void* ring, const void* data, uint64_t size);
    uint64_t Swarm_RingBuffer_Pop(void* ring, void* dest);
    uint64_t Swarm_RingBuffer_Count(void* ring);

    // Checksums
    int      Swarm_Blake2b_128(const void* data, uint64_t len, void* out16);
    uint64_t Swarm_XXH64(const void* data, uint64_t len, uint64_t seed);

    // Packet operations
    int      Swarm_ValidatePacketHeader(const void* packet);
    int      Swarm_BuildPacketHeader(void* buffer, uint8_t opcode,
                                      uint16_t payloadLen, uint64_t taskId);

    // Heartbeat
    uint64_t Swarm_HeartbeatRecord(uint32_t nodeSlot);
    int      Swarm_HeartbeatCheck(uint32_t nodeSlot);

    // Hardware
    uint32_t Swarm_ComputeNodeFitness(void);

    // IOCP
    void*    Swarm_IOCP_Create(void);
    void*    Swarm_IOCP_Associate(void* socket, void* iocp, uint64_t key);
    int      Swarm_IOCP_GetCompletion(void* iocp, uint32_t timeoutMs,
                                       uint32_t* bytesTransferred, uint64_t* key);

    // Memory
    uint64_t Swarm_MemCopy_NT(void* dest, const void* src, uint64_t count);
}

// =============================================================================
//                     UTILITY HELPERS
// =============================================================================

namespace SwarmProtocol {

    // Build a complete packet (header + payload) into a buffer.
    // Returns total size written (header + payload).
    inline uint32_t buildPacket(void* buffer, SwarmOpcode opcode,
                                 const void* payload, uint16_t payloadLen,
                                 uint64_t taskId, const uint8_t nodeId[16],
                                 uint32_t sequenceId) {
        auto* hdr = reinterpret_cast<SwarmPacketHeader*>(buffer);

        // Copy payload first (MASM checksum reads from buffer+64)
        if (payload && payloadLen > 0) {
            memcpy(reinterpret_cast<uint8_t*>(buffer) + SWARM_HEADER_SIZE,
                   payload, payloadLen);
        }

        // Build header (MASM fills magic, version, checksum, timestamp)
        Swarm_BuildPacketHeader(buffer, static_cast<uint8_t>(opcode),
                                 payloadLen, taskId);

        // Patch fields that MASM leaves for C++ to fill
        hdr->sequenceId = sequenceId;
        if (nodeId) {
            memcpy(hdr->nodeId, nodeId, 16);
        }

        return SWARM_HEADER_SIZE + payloadLen;
    }

    // Validate a received packet and extract the opcode.
    inline bool validateAndExtract(const void* packet,
                                    SwarmOpcode& outOpcode,
                                    uint16_t& outPayloadLen,
                                    uint64_t& outTaskId) {
        int rc = Swarm_ValidatePacketHeader(packet);
        if (rc != 0) return false;

        auto* hdr = reinterpret_cast<const SwarmPacketHeader*>(packet);
        outOpcode     = static_cast<SwarmOpcode>(hdr->opcode);
        outPayloadLen = hdr->payloadLen;
        outTaskId     = hdr->taskId;
        return true;
    }

    // Get pointer to payload data within a packet buffer.
    inline const void* getPayload(const void* packet) {
        return reinterpret_cast<const uint8_t*>(packet) + SWARM_HEADER_SIZE;
    }

    inline void* getPayloadMut(void* packet) {
        return reinterpret_cast<uint8_t*>(packet) + SWARM_HEADER_SIZE;
    }

} // namespace SwarmProtocol

#endif // RAWRXD_SWARM_PROTOCOL_H
