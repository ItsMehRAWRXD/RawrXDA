// ============================================================================
// swarm_reconciliation.h — Distributed Swarm Reconciliation Layer
// ============================================================================
//
// PURPOSE:
//   DAG-based state reconciliation for the distributed swarm compilation
//   system. Handles conflict resolution, partition recovery, and state
//   merge when nodes rejoin after network splits.
//
//   Uses existing ConsensusVote (0x0D) and ConsensusCommit (0x0E) opcodes
//   from swarm_protocol.h. Implements Raft-inspired log reconciliation
//   with vector clock conflict detection.
//
// DEPS:     swarm_protocol.h, swarm_types.h
// PATTERN:  PatchResult-compatible, no exceptions, no std::function
// THREADING: Lock-free where possible, std::mutex for state transitions
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Swarm {

// ============================================================================
// Constants
// ============================================================================
static constexpr uint32_t RECONCILE_MAX_ENTRIES  = 4096;
static constexpr uint32_t RECONCILE_MAX_NODES    = 64;
static constexpr uint32_t RECONCILE_HASH_BUCKETS = 256;
static constexpr uint32_t RECONCILE_MAX_CONFLICTS = 128;
static constexpr uint32_t RECONCILE_TIMEOUT_MS   = 10000;
static constexpr uint32_t RECONCILE_QUORUM_PCT   = 51;   // >50% for commit

// ============================================================================
// Vector Clock — per-node logical timestamp
// ============================================================================
struct VectorClock {
    uint64_t ticks[RECONCILE_MAX_NODES];
    uint32_t nodeCount;

    void increment(uint32_t nodeIndex) {
        if (nodeIndex < RECONCILE_MAX_NODES)
            ticks[nodeIndex]++;
    }

    // Returns: -1 = this < other, 0 = concurrent, 1 = this > other
    int compare(const VectorClock& other) const;

    void merge(const VectorClock& other);

    void reset() {
        memset(ticks, 0, sizeof(ticks));
        nodeCount = 0;
    }
};

// ============================================================================
// Reconciliation Entry — single state mutation in the DAG
// ============================================================================
enum class EntryType : uint8_t {
    TaskResult       = 0x01,   // Compilation result
    ConfigChange     = 0x02,   // Cluster configuration change
    NodeJoin         = 0x03,   // New node joined
    NodeLeave        = 0x04,   // Node departed
    PatchApply       = 0x05,   // Hotpatch applied to model
    ModelLoad        = 0x06,   // Model loaded/swapped
    QuantChange      = 0x07,   // Quantization level changed
};

struct ReconcileEntry {
    uint64_t     entryId;           // Monotonic entry ID
    uint64_t     taskId;            // Associated task (0 if N/A)
    uint8_t      nodeId[16];        // Originating node (128-bit HWID)
    VectorClock  vclock;            // Vector clock at creation
    EntryType    type;              // Entry type
    uint8_t      committed;         // 1 = committed by quorum, 0 = pending
    uint8_t      conflicted;        // 1 = conflict detected
    uint8_t      resolved;          // 1 = conflict resolved
    uint64_t     parentEntry;       // Parent entry ID (DAG link)
    uint64_t     timestampMs;       // Wall-clock timestamp
    uint8_t      payloadHash[32];   // SHA-256 of payload
    uint32_t     payloadLen;        // Payload byte count
    uint8_t      payload[1024];     // Inline payload (small entries)
};

// ============================================================================
// Conflict Record
// ============================================================================
enum class ConflictResolution : uint8_t {
    Pending     = 0x00,
    PickLocal   = 0x01,   // Local node's version wins
    PickRemote  = 0x02,   // Remote node's version wins
    Merge       = 0x03,   // Entries merged (both applied)
    Discard     = 0x04,   // Both discarded (e.g., stale config)
};

struct ConflictRecord {
    uint64_t             localEntryId;
    uint64_t             remoteEntryId;
    uint8_t              localNodeId[16];
    uint8_t              remoteNodeId[16];
    EntryType            entryType;
    ConflictResolution   resolution;
    uint64_t             resolvedAtMs;
    char                 detail[256];
};

// ============================================================================
// Reconciliation State Machine
// ============================================================================
enum class ReconcileState : uint32_t {
    Idle             = 0,
    SyncRequested    = 1,   // Request sent to peers
    ReceivingEntries = 2,   // Collecting entries from peers
    DetectConflicts  = 3,   // Analyzing vector clocks for conflicts
    Resolving        = 4,   // Applying conflict resolution strategy
    Voting           = 5,   // ConsensusVote round
    Committing       = 6,   // ConsensusCommit broadcast
    Complete         = 7,
    Failed           = 8,
    PartitionRecovery = 9   // Special mode: node rejoining after partition
};

// ============================================================================
// Reconciliation Result
// ============================================================================
struct ReconcileResult {
    bool            success;
    ReconcileState  finalState;
    const char*     detail;
    int             errorCode;
    uint32_t        entriesReconciled;
    uint32_t        conflictsFound;
    uint32_t        conflictsResolved;
    double          elapsedMs;

    static ReconcileResult ok(uint32_t entries, uint32_t conflicts, double ms) {
        ReconcileResult r{};
        r.success            = true;
        r.finalState         = ReconcileState::Complete;
        r.detail             = "Reconciliation complete";
        r.errorCode          = 0;
        r.entriesReconciled  = entries;
        r.conflictsFound     = conflicts;
        r.conflictsResolved  = conflicts;
        r.elapsedMs          = ms;
        return r;
    }

    static ReconcileResult error(const char* msg, ReconcileState state, int code = -1) {
        ReconcileResult r{};
        r.success            = false;
        r.finalState         = state;
        r.detail             = msg;
        r.errorCode          = code;
        r.entriesReconciled  = 0;
        r.conflictsFound     = 0;
        r.conflictsResolved  = 0;
        r.elapsedMs          = 0.0;
        return r;
    }
};

// ============================================================================
// Vote Tally — tracks ConsensusVote responses for a single entry
// ============================================================================
struct VoteTally {
    uint64_t entryId;
    uint32_t votesFor;
    uint32_t votesAgainst;
    uint32_t totalNodes;
    bool     committed;           // Quorum reached and committed

    bool hasQuorum() const {
        if (totalNodes == 0) return false;
        return (votesFor * 100 / totalNodes) >= RECONCILE_QUORUM_PCT;
    }
};

// ============================================================================
// Reconciliation Statistics
// ============================================================================
struct ReconcileStats {
    uint64_t totalReconciliations;
    uint64_t totalConflicts;
    uint64_t totalConflictsResolved;
    uint64_t totalEntriesProcessed;
    uint64_t partitionRecoveries;
    uint64_t failedReconciliations;
    double   avgReconcileMs;
    double   maxReconcileMs;
    uint64_t lastReconcileTimestamp;
};

// ============================================================================
// Conflict Resolution Callback — function pointer, NOT std::function
// ============================================================================
typedef ConflictResolution (*ConflictResolver)(
    const ReconcileEntry* localEntry,
    const ReconcileEntry* remoteEntry,
    void* userData
);

// ============================================================================
// Packet Send Callback — used to send ConsensusVote/Commit packets
// ============================================================================
typedef bool (*PacketSendCallback)(
    uint8_t opcode,              // SwarmOpcode value
    const uint8_t* nodeId,       // Target node (nullptr = broadcast)
    const void* payload,
    uint32_t payloadLen,
    void* userData
);

// ============================================================================
// SwarmReconciler — Singleton
// ============================================================================
class SwarmReconciler {
public:
    static SwarmReconciler& instance();

    // ======================================================================
    // Lifecycle
    // ======================================================================

    /// Initialize with local node identity
    bool initialize(const uint8_t localNodeId[16], uint32_t localIndex);

    /// Shutdown and flush pending entries
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // ======================================================================
    // Entry Management
    // ======================================================================

    /// Append a new local entry to the DAG
    uint64_t appendEntry(EntryType type, uint64_t taskId,
                          const void* payload, uint32_t payloadLen);

    /// Receive a remote entry from a peer (via SwarmCoordinator)
    bool receiveRemoteEntry(const ReconcileEntry& entry);

    /// Get an entry by ID
    const ReconcileEntry* getEntry(uint64_t entryId) const;

    /// Get the latest committed entry ID
    uint64_t getLastCommittedId() const { return m_lastCommittedId; }

    // ======================================================================
    // Reconciliation Operations
    // ======================================================================

    /// Full reconciliation with all known peers
    ReconcileResult reconcile();

    /// Reconcile with a specific node (e.g., after partition recovery)
    ReconcileResult reconcileWithNode(const uint8_t nodeId[16],
                                       const ReconcileEntry* remoteEntries,
                                       uint32_t remoteCount);

    /// Partition recovery mode: bulk-receive entries from a peer
    /// and reconcile divergent histories
    ReconcileResult partitionRecovery(const uint8_t nodeId[16],
                                       const ReconcileEntry* peerEntries,
                                       uint32_t peerCount);

    // ======================================================================
    // Conflict Resolution
    // ======================================================================

    /// Set the conflict resolution strategy callback
    void setConflictResolver(ConflictResolver resolver, void* userData);

    /// Get the list of unresolved conflicts
    uint32_t getUnresolvedConflicts(ConflictRecord* outConflicts,
                                     uint32_t maxCount) const;

    /// Manually resolve a specific conflict
    bool resolveConflict(uint64_t localEntryId, uint64_t remoteEntryId,
                          ConflictResolution resolution);

    // ======================================================================
    // Consensus (ConsensusVote / ConsensusCommit)
    // ======================================================================

    /// Set the packet send callback (used for vote/commit broadcasts)
    void setPacketSender(PacketSendCallback sender, void* userData);

    /// Initiate a consensus vote for an entry
    bool initiateVote(uint64_t entryId);

    /// Process an incoming ConsensusVote packet
    bool processVote(uint64_t entryId, const uint8_t voterNodeId[16],
                      bool voteFor);

    /// Check if an entry has reached quorum
    bool checkQuorum(uint64_t entryId) const;

    /// Commit an entry that has reached quorum
    bool commitEntry(uint64_t entryId);

    // ======================================================================
    // Statistics & Diagnostics
    // ======================================================================

    ReconcileStats getStats() const;
    void resetStats();

    /// Get the current reconcile state
    ReconcileState getState() const { return m_state.load(std::memory_order_acquire); }

    /// Get the local node's vector clock
    VectorClock getLocalClock() const;

    /// Dump DAG to debug output 
    void dumpDAG() const;

private:
    SwarmReconciler();
    ~SwarmReconciler();
    SwarmReconciler(const SwarmReconciler&) = delete;
    SwarmReconciler& operator=(const SwarmReconciler&) = delete;

    // Internal conflict detection between two entries
    bool detectConflict(const ReconcileEntry& a, const ReconcileEntry& b) const;

    // Default conflict resolver: last-writer-wins by wall-clock
    static ConflictResolution defaultResolver(
        const ReconcileEntry* local, const ReconcileEntry* remote, void* userData);

    // Get or create a vote tally for an entry
    VoteTally* getOrCreateTally(uint64_t entryId);

    bool                    m_initialized;
    uint8_t                 m_localNodeId[16];
    uint32_t                m_localIndex;
    std::atomic<ReconcileState> m_state;
    mutable std::mutex      m_mutex;

    // Entry storage (ring buffer with overflow to array)
    ReconcileEntry          m_entries[RECONCILE_MAX_ENTRIES];
    uint32_t                m_entryCount;
    uint64_t                m_nextEntryId;
    uint64_t                m_lastCommittedId;

    // Conflict tracking
    ConflictRecord          m_conflicts[RECONCILE_MAX_CONFLICTS];
    uint32_t                m_conflictCount;

    // Vote tallies
    VoteTally               m_tallies[RECONCILE_MAX_ENTRIES];
    uint32_t                m_tallyCount;

    // Vector clock for this node
    VectorClock             m_localClock;

    // Callbacks
    ConflictResolver        m_resolver;
    void*                   m_resolverUserData;
    PacketSendCallback      m_packetSender;
    void*                   m_senderUserData;

    // Statistics
    ReconcileStats          m_stats;
    LARGE_INTEGER           m_perfFreq;
};

} // namespace Swarm
} // namespace RawrXD
