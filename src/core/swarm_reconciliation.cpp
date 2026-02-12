// ============================================================================
// swarm_reconciliation.cpp — Distributed Swarm Reconciliation Layer
// ============================================================================
//
// DAG-based state reconciliation for distributed swarm compilation.
// Implements vector clock conflict detection, quorum-based consensus
// (ConsensusVote/ConsensusCommit), partition recovery, and state merge.
//
// DEPS:     swarm_reconciliation.h, swarm_protocol.h
// PATTERN:  PatchResult-compatible, no exceptions
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/swarm_reconciliation.h"
#include "../core/swarm_protocol.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace RawrXD {
namespace Swarm {

// ============================================================================
// VectorClock Implementation
// ============================================================================
int VectorClock::compare(const VectorClock& other) const {
    bool thisGreater = false;
    bool otherGreater = false;
    uint32_t maxNodes = (nodeCount > other.nodeCount) ? nodeCount : other.nodeCount;
    if (maxNodes > RECONCILE_MAX_NODES) maxNodes = RECONCILE_MAX_NODES;

    for (uint32_t i = 0; i < maxNodes; i++) {
        if (ticks[i] > other.ticks[i]) thisGreater = true;
        if (ticks[i] < other.ticks[i]) otherGreater = true;
    }

    if (thisGreater && !otherGreater) return 1;   // this > other
    if (otherGreater && !thisGreater) return -1;  // this < other
    return 0; // concurrent (or equal)
}

void VectorClock::merge(const VectorClock& other) {
    uint32_t maxNodes = (nodeCount > other.nodeCount) ? nodeCount : other.nodeCount;
    if (maxNodes > RECONCILE_MAX_NODES) maxNodes = RECONCILE_MAX_NODES;

    for (uint32_t i = 0; i < maxNodes; i++) {
        if (other.ticks[i] > ticks[i]) {
            ticks[i] = other.ticks[i];
        }
    }
    if (other.nodeCount > nodeCount) {
        nodeCount = other.nodeCount;
    }
}

// ============================================================================
// Singleton
// ============================================================================
SwarmReconciler& SwarmReconciler::instance() {
    static SwarmReconciler s_instance;
    return s_instance;
}

SwarmReconciler::SwarmReconciler()
    : m_initialized(false)
    , m_localIndex(0)
    , m_state(ReconcileState::Idle)
    , m_entryCount(0)
    , m_nextEntryId(1)
    , m_lastCommittedId(0)
    , m_conflictCount(0)
    , m_tallyCount(0)
    , m_resolver(defaultResolver)
    , m_resolverUserData(nullptr)
    , m_packetSender(nullptr)
    , m_senderUserData(nullptr)
{
    memset(m_localNodeId, 0, sizeof(m_localNodeId));
    memset(m_entries, 0, sizeof(m_entries));
    memset(m_conflicts, 0, sizeof(m_conflicts));
    memset(m_tallies, 0, sizeof(m_tallies));
    memset(&m_stats, 0, sizeof(m_stats));
    m_localClock.reset();

    QueryPerformanceFrequency(&m_perfFreq);
}

SwarmReconciler::~SwarmReconciler() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
bool SwarmReconciler::initialize(const uint8_t localNodeId[16], uint32_t localIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) return true; // Already initialized

    memcpy(m_localNodeId, localNodeId, 16);
    m_localIndex = localIndex;
    m_entryCount = 0;
    m_nextEntryId = 1;
    m_lastCommittedId = 0;
    m_conflictCount = 0;
    m_tallyCount = 0;
    m_localClock.reset();
    m_localClock.nodeCount = localIndex + 1;
    memset(&m_stats, 0, sizeof(m_stats));

    m_state.store(ReconcileState::Idle, std::memory_order_release);
    m_initialized = true;

    OutputDebugStringA("[SwarmReconciler] Initialized\n");
    return true;
}

void SwarmReconciler::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return;

    m_initialized = false;
    m_state.store(ReconcileState::Idle, std::memory_order_release);

    OutputDebugStringA("[SwarmReconciler] Shutdown\n");
}

// ============================================================================
// Entry Management
// ============================================================================
uint64_t SwarmReconciler::appendEntry(EntryType type, uint64_t taskId,
                                        const void* payload, uint32_t payloadLen)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return 0;
    if (m_entryCount >= RECONCILE_MAX_ENTRIES) {
        OutputDebugStringA("[SwarmReconciler] Entry buffer full, cannot append\n");
        return 0;
    }

    // Increment our vector clock
    m_localClock.increment(m_localIndex);

    ReconcileEntry& entry = m_entries[m_entryCount];
    memset(&entry, 0, sizeof(entry));

    entry.entryId = m_nextEntryId++;
    entry.taskId = taskId;
    memcpy(entry.nodeId, m_localNodeId, 16);
    entry.vclock = m_localClock;
    entry.type = type;
    entry.committed = 0;
    entry.conflicted = 0;
    entry.resolved = 0;
    entry.parentEntry = (m_entryCount > 0) ? m_entries[m_entryCount - 1].entryId : 0;
    entry.timestampMs = GetTickCount64();

    // Copy payload (bounded)
    if (payload && payloadLen > 0) {
        uint32_t copyLen = (payloadLen > sizeof(entry.payload)) ?
                            (uint32_t)sizeof(entry.payload) : payloadLen;
        memcpy(entry.payload, payload, copyLen);
        entry.payloadLen = copyLen;
    }

    // Compute payload hash (simple FNV-1a for speed, real deployment uses SHA-256)
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint32_t i = 0; i < entry.payloadLen; i++) {
        hash ^= entry.payload[i];
        hash *= 0x100000001b3ULL;
    }
    memcpy(entry.payloadHash, &hash, sizeof(hash));

    m_entryCount++;
    m_stats.totalEntriesProcessed++;

    char dbg[128];
    snprintf(dbg, sizeof(dbg), "[SwarmReconciler] Appended entry %llu (type %u)\n",
             (unsigned long long)entry.entryId, (unsigned)entry.type);
    OutputDebugStringA(dbg);

    return entry.entryId;
}

bool SwarmReconciler::receiveRemoteEntry(const ReconcileEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;
    if (m_entryCount >= RECONCILE_MAX_ENTRIES) return false;

    // Check for duplicate
    for (uint32_t i = 0; i < m_entryCount; i++) {
        if (m_entries[i].entryId == entry.entryId &&
            memcmp(m_entries[i].nodeId, entry.nodeId, 16) == 0) {
            return true; // Already have this entry
        }
    }

    // Store remote entry
    m_entries[m_entryCount] = entry;
    m_entryCount++;

    // Merge vector clock
    m_localClock.merge(entry.vclock);
    m_localClock.increment(m_localIndex);

    m_stats.totalEntriesProcessed++;
    return true;
}

const ReconcileEntry* SwarmReconciler::getEntry(uint64_t entryId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_entryCount; i++) {
        if (m_entries[i].entryId == entryId) {
            return &m_entries[i];
        }
    }
    return nullptr;
}

// ============================================================================
// Conflict Detection
// ============================================================================
bool SwarmReconciler::detectConflict(const ReconcileEntry& a,
                                       const ReconcileEntry& b) const
{
    // Same type operating on same task → potential conflict
    if (a.type != b.type) return false;
    if (a.taskId != b.taskId && a.taskId != 0 && b.taskId != 0) return false;

    // Same node cannot conflict with itself
    if (memcmp(a.nodeId, b.nodeId, 16) == 0) return false;

    // Vector clock comparison: concurrent = conflict
    int cmp = a.vclock.compare(b.vclock);
    return (cmp == 0); // Concurrent events = conflict
}

// ============================================================================
// Default Conflict Resolver — Last-Writer-Wins by wall-clock
// ============================================================================
ConflictResolution SwarmReconciler::defaultResolver(
    const ReconcileEntry* local, const ReconcileEntry* remote, void* /*userData*/)
{
    if (!local || !remote) return ConflictResolution::Discard;

    // Last-writer-wins based on wall-clock timestamp
    if (local->timestampMs >= remote->timestampMs) {
        return ConflictResolution::PickLocal;
    }
    return ConflictResolution::PickRemote;
}

// ============================================================================
// Full Reconciliation
// ============================================================================
ReconcileResult SwarmReconciler::reconcile() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return ReconcileResult::error("Not initialized", ReconcileState::Idle);
    }

    LARGE_INTEGER startTime;
    QueryPerformanceCounter(&startTime);

    m_state.store(ReconcileState::DetectConflicts, std::memory_order_release);

    // Phase 1: Detect conflicts among all entries
    uint32_t conflictsFound = 0;
    m_conflictCount = 0;

    for (uint32_t i = 0; i < m_entryCount && conflictsFound < RECONCILE_MAX_CONFLICTS; i++) {
        if (m_entries[i].committed || m_entries[i].resolved) continue;

        for (uint32_t j = i + 1; j < m_entryCount && conflictsFound < RECONCILE_MAX_CONFLICTS; j++) {
            if (m_entries[j].committed || m_entries[j].resolved) continue;

            if (detectConflict(m_entries[i], m_entries[j])) {
                m_entries[i].conflicted = 1;
                m_entries[j].conflicted = 1;

                ConflictRecord& cr = m_conflicts[m_conflictCount];
                memset(&cr, 0, sizeof(cr));
                cr.localEntryId  = m_entries[i].entryId;
                cr.remoteEntryId = m_entries[j].entryId;
                memcpy(cr.localNodeId, m_entries[i].nodeId, 16);
                memcpy(cr.remoteNodeId, m_entries[j].nodeId, 16);
                cr.entryType = m_entries[i].type;
                cr.resolution = ConflictResolution::Pending;

                m_conflictCount++;
                conflictsFound++;
            }
        }
    }

    // Phase 2: Resolve conflicts
    m_state.store(ReconcileState::Resolving, std::memory_order_release);
    uint32_t resolved = 0;

    for (uint32_t c = 0; c < m_conflictCount; c++) {
        ConflictRecord& cr = m_conflicts[c];
        if (cr.resolution != ConflictResolution::Pending) continue;

        const ReconcileEntry* localEntry = nullptr;
        const ReconcileEntry* remoteEntry = nullptr;

        for (uint32_t i = 0; i < m_entryCount; i++) {
            if (m_entries[i].entryId == cr.localEntryId)  localEntry = &m_entries[i];
            if (m_entries[i].entryId == cr.remoteEntryId) remoteEntry = &m_entries[i];
        }

        if (localEntry && remoteEntry) {
            cr.resolution = m_resolver(localEntry, remoteEntry, m_resolverUserData);
            cr.resolvedAtMs = GetTickCount64();
            resolved++;

            // Mark resolved entries
            for (uint32_t i = 0; i < m_entryCount; i++) {
                if (m_entries[i].entryId == cr.localEntryId ||
                    m_entries[i].entryId == cr.remoteEntryId) {
                    m_entries[i].resolved = 1;
                }
            }

            snprintf(cr.detail, sizeof(cr.detail),
                     "Resolved via %s",
                     cr.resolution == ConflictResolution::PickLocal  ? "PickLocal" :
                     cr.resolution == ConflictResolution::PickRemote ? "PickRemote" :
                     cr.resolution == ConflictResolution::Merge      ? "Merge" :
                     "Discard");
        }
    }

    // Phase 3: Vote on uncommitted entries
    m_state.store(ReconcileState::Voting, std::memory_order_release);

    uint32_t entriesReconciled = 0;
    for (uint32_t i = 0; i < m_entryCount; i++) {
        if (!m_entries[i].committed && !m_entries[i].conflicted) {
            // Non-conflicting, uncommitted → auto-commit
            m_entries[i].committed = 1;
            if (m_entries[i].entryId > m_lastCommittedId) {
                m_lastCommittedId = m_entries[i].entryId;
            }
            entriesReconciled++;
        }
    }

    // Phase 4: Mark complete
    m_state.store(ReconcileState::Complete, std::memory_order_release);

    LARGE_INTEGER endTime;
    QueryPerformanceCounter(&endTime);
    double elapsedMs = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / m_perfFreq.QuadPart;

    // Update stats
    m_stats.totalReconciliations++;
    m_stats.totalConflicts += conflictsFound;
    m_stats.totalConflictsResolved += resolved;
    m_stats.totalEntriesProcessed += entriesReconciled;
    m_stats.lastReconcileTimestamp = GetTickCount64();

    if (elapsedMs > m_stats.maxReconcileMs) m_stats.maxReconcileMs = elapsedMs;
    m_stats.avgReconcileMs =
        (m_stats.avgReconcileMs * (m_stats.totalReconciliations - 1) + elapsedMs)
        / m_stats.totalReconciliations;

    m_state.store(ReconcileState::Idle, std::memory_order_release);

    char dbg[256];
    snprintf(dbg, sizeof(dbg),
             "[SwarmReconciler] Reconciliation complete: %u entries, %u conflicts (%u resolved), %.2fms\n",
             entriesReconciled, conflictsFound, resolved, elapsedMs);
    OutputDebugStringA(dbg);

    return ReconcileResult::ok(entriesReconciled, conflictsFound, elapsedMs);
}

// ============================================================================
// Reconcile With Specific Node
// ============================================================================
ReconcileResult SwarmReconciler::reconcileWithNode(
    const uint8_t nodeId[16],
    const ReconcileEntry* remoteEntries,
    uint32_t remoteCount)
{
    if (!m_initialized) {
        return ReconcileResult::error("Not initialized", ReconcileState::Idle);
    }

    if (!remoteEntries || remoteCount == 0) {
        return ReconcileResult::ok(0, 0, 0.0);
    }

    // Receive all remote entries first
    for (uint32_t i = 0; i < remoteCount; i++) {
        receiveRemoteEntry(remoteEntries[i]);
    }

    // Then do full reconciliation
    return reconcile();
}

// ============================================================================
// Partition Recovery
// ============================================================================
ReconcileResult SwarmReconciler::partitionRecovery(
    const uint8_t nodeId[16],
    const ReconcileEntry* peerEntries,
    uint32_t peerCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return ReconcileResult::error("Not initialized", ReconcileState::Idle);
    }

    m_state.store(ReconcileState::PartitionRecovery, std::memory_order_release);
    m_stats.partitionRecoveries++;

    char dbg[128];
    snprintf(dbg, sizeof(dbg),
             "[SwarmReconciler] Partition recovery: receiving %u entries from peer\n",
             peerCount);
    OutputDebugStringA(dbg);

    // In partition recovery, we receive all peer entries and mark them for
    // special conflict resolution (partition-aware merge)
    // Temporarily unlock to receive entries (receiveRemoteEntry takes the lock)
    // We already hold the lock, so we inline the logic here

    for (uint32_t i = 0; i < peerCount && m_entryCount < RECONCILE_MAX_ENTRIES; i++) {
        // Check for duplicate
        bool duplicate = false;
        for (uint32_t j = 0; j < m_entryCount; j++) {
            if (m_entries[j].entryId == peerEntries[i].entryId &&
                memcmp(m_entries[j].nodeId, peerEntries[i].nodeId, 16) == 0) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            m_entries[m_entryCount] = peerEntries[i];
            m_entryCount++;
            m_localClock.merge(peerEntries[i].vclock);
        }
    }

    m_localClock.increment(m_localIndex);

    m_state.store(ReconcileState::Idle, std::memory_order_release);

    // Now reconcile with the merged state (unlock first since reconcile takes lock)
    // Since we already hold the lock, directly do conflict detection here

    uint32_t conflicts = 0;
    for (uint32_t i = 0; i < m_entryCount; i++) {
        if (m_entries[i].committed) continue;
        for (uint32_t j = i + 1; j < m_entryCount; j++) {
            if (m_entries[j].committed) continue;
            if (detectConflict(m_entries[i], m_entries[j])) {
                conflicts++;
            }
        }
    }

    return ReconcileResult::ok(peerCount, conflicts, 0.0);
}

// ============================================================================
// Conflict Resolution Management
// ============================================================================
void SwarmReconciler::setConflictResolver(ConflictResolver resolver, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resolver = resolver ? resolver : defaultResolver;
    m_resolverUserData = userData;
}

uint32_t SwarmReconciler::getUnresolvedConflicts(ConflictRecord* outConflicts,
                                                   uint32_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;

    for (uint32_t i = 0; i < m_conflictCount && count < maxCount; i++) {
        if (m_conflicts[i].resolution == ConflictResolution::Pending) {
            outConflicts[count++] = m_conflicts[i];
        }
    }
    return count;
}

bool SwarmReconciler::resolveConflict(uint64_t localEntryId, uint64_t remoteEntryId,
                                        ConflictResolution resolution)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_conflictCount; i++) {
        if (m_conflicts[i].localEntryId == localEntryId &&
            m_conflicts[i].remoteEntryId == remoteEntryId) {
            m_conflicts[i].resolution = resolution;
            m_conflicts[i].resolvedAtMs = GetTickCount64();
            m_stats.totalConflictsResolved++;

            // Mark entries resolved
            for (uint32_t j = 0; j < m_entryCount; j++) {
                if (m_entries[j].entryId == localEntryId ||
                    m_entries[j].entryId == remoteEntryId) {
                    m_entries[j].resolved = 1;
                }
            }
            return true;
        }
    }
    return false;
}

// ============================================================================
// Consensus (ConsensusVote / ConsensusCommit)
// ============================================================================
void SwarmReconciler::setPacketSender(PacketSendCallback sender, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetSender  = sender;
    m_senderUserData = userData;
}

VoteTally* SwarmReconciler::getOrCreateTally(uint64_t entryId) {
    // Find existing
    for (uint32_t i = 0; i < m_tallyCount; i++) {
        if (m_tallies[i].entryId == entryId) return &m_tallies[i];
    }

    // Create new
    if (m_tallyCount >= RECONCILE_MAX_ENTRIES) return nullptr;

    VoteTally& t = m_tallies[m_tallyCount++];
    memset(&t, 0, sizeof(t));
    t.entryId = entryId;
    return &t;
}

bool SwarmReconciler::initiateVote(uint64_t entryId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_packetSender) return false;

    VoteTally* tally = getOrCreateTally(entryId);
    if (!tally) return false;

    // Broadcast ConsensusVote packet
    struct VotePayload {
        uint64_t entryId;
        uint8_t  voterNodeId[16];
        uint8_t  voteFor; // 1 = yes
    };

    VotePayload payload;
    payload.entryId = entryId;
    memcpy(payload.voterNodeId, m_localNodeId, 16);
    payload.voteFor = 1; // Self-vote = yes

    tally->votesFor = 1; // Count our own vote

    bool sent = m_packetSender(
        static_cast<uint8_t>(SwarmOpcode::ConsensusVote),
        nullptr, // broadcast
        &payload, sizeof(payload),
        m_senderUserData);

    return sent;
}

bool SwarmReconciler::processVote(uint64_t entryId, const uint8_t voterNodeId[16],
                                    bool voteFor)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    VoteTally* tally = getOrCreateTally(entryId);
    if (!tally) return false;

    if (voteFor) tally->votesFor++;
    else         tally->votesAgainst++;

    tally->totalNodes++;

    // Check if quorum reached
    if (tally->hasQuorum() && !tally->committed) {
        commitEntry(entryId);
    }

    return true;
}

bool SwarmReconciler::checkQuorum(uint64_t entryId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_tallyCount; i++) {
        if (m_tallies[i].entryId == entryId) {
            return m_tallies[i].hasQuorum();
        }
    }
    return false;
}

bool SwarmReconciler::commitEntry(uint64_t entryId) {
    // Note: caller must hold m_mutex

    // Find and mark the entry committed
    for (uint32_t i = 0; i < m_entryCount; i++) {
        if (m_entries[i].entryId == entryId) {
            m_entries[i].committed = 1;
            if (entryId > m_lastCommittedId) {
                m_lastCommittedId = entryId;
            }

            // Mark tally as committed
            for (uint32_t t = 0; t < m_tallyCount; t++) {
                if (m_tallies[t].entryId == entryId) {
                    m_tallies[t].committed = true;
                    break;
                }
            }

            // Broadcast ConsensusCommit if we have a sender
            if (m_packetSender) {
                struct CommitPayload {
                    uint64_t entryId;
                    uint8_t  leaderNodeId[16];
                };

                CommitPayload cp;
                cp.entryId = entryId;
                memcpy(cp.leaderNodeId, m_localNodeId, 16);

                m_packetSender(
                    static_cast<uint8_t>(SwarmOpcode::ConsensusCommit),
                    nullptr, // broadcast
                    &cp, sizeof(cp),
                    m_senderUserData);
            }

            char dbg[128];
            snprintf(dbg, sizeof(dbg),
                     "[SwarmReconciler] Entry %llu committed\n",
                     (unsigned long long)entryId);
            OutputDebugStringA(dbg);

            return true;
        }
    }
    return false;
}

// ============================================================================
// Statistics & Diagnostics
// ============================================================================
ReconcileStats SwarmReconciler::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void SwarmReconciler::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

VectorClock SwarmReconciler::getLocalClock() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_localClock;
}

void SwarmReconciler::dumpDAG() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[SwarmReconciler] DAG dump: %u entries, %u conflicts, lastCommitted=%llu\n",
             m_entryCount, m_conflictCount,
             (unsigned long long)m_lastCommittedId);
    OutputDebugStringA(buf);

    for (uint32_t i = 0; i < m_entryCount; i++) {
        const ReconcileEntry& e = m_entries[i];
        snprintf(buf, sizeof(buf),
                 "  Entry #%llu: type=%u task=%llu parent=%llu committed=%d conflicted=%d resolved=%d\n",
                 (unsigned long long)e.entryId,
                 (unsigned)e.type,
                 (unsigned long long)e.taskId,
                 (unsigned long long)e.parentEntry,
                 e.committed, e.conflicted, e.resolved);
        OutputDebugStringA(buf);
    }
}

} // namespace Swarm
} // namespace RawrXD
