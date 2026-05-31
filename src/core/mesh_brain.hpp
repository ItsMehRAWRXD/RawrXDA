// mesh_brain.hpp — Phase G: Distributed Consciousness (The Mesh)
//
// The Cathedral Network: RawrXD instances form a global P2P mesh intelligence.
// CRDT-based code graph sharing, zero-knowledge proof of optimization,
// Kademlia DHT for peer discovery, federated weight aggregation, gossip
// protocols, and torrent-style model shard distribution.
//
// Self-healing across the internet — if one node dies, others absorb workload.
// The IDE becomes a distributed entity with a local interface.
//
// Architecture: C++20 bridge → MASM64 MeshBrain kernel
// Threading: SRW lock protected; gossip and DHT are async
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

// ---------------------------------------------------------------------------
// ASM kernel exports — Mesh Brain operations
// ---------------------------------------------------------------------------
#ifdef RAWR_HAS_MASM
extern "C" {
    int      asm_mesh_init();
    uint64_t asm_mesh_crdt_merge(const void* entries, uint32_t count);
    uint64_t asm_mesh_crdt_delta(uint64_t sinceTimestamp, void* outBuf, uint32_t maxEntries);
    int      asm_mesh_zkp_generate(const void* metrics, void* proofOut);
    int      asm_mesh_zkp_verify(void* proof);
    uint32_t asm_mesh_dht_xor_distance(const void* idA, const void* idB);
    uint32_t asm_mesh_dht_find_closest(const void* targetId, void* outIds, uint32_t k);
    int      asm_mesh_fedavg_aggregate(const void* deltaArray, uint32_t numContrib,
                                        void* outAvg, uint32_t numElements);
    uint32_t asm_mesh_gossip_disseminate(const void* msg, uint64_t msgSize,
                                          void* sendCallback);
    int      asm_mesh_shard_hash(const void* data, uint64_t size, void* hashOut);
    int      asm_mesh_shard_bitfield(uint32_t pieceIndex, uint32_t operation);
    int      asm_mesh_quorum_vote(const uint8_t* votes, uint32_t count, uint32_t threshold);
    int      asm_mesh_topology_update(const void* nodeEntry);
    int      asm_mesh_crdt_lookup(uint64_t key, uint64_t* outValue);
    void     asm_mesh_topology_remove(const uint64_t* nodeId);
    uint32_t asm_mesh_topology_count();
    void     asm_mesh_topology_list(void* outBuf, uint32_t maxCount);
    uint32_t asm_mesh_topology_active_count();
    void*    asm_mesh_get_stats();
    int      asm_mesh_shutdown();
}
#endif

// ---------------------------------------------------------------------------
// CRDT Types
// ---------------------------------------------------------------------------
enum class CRDTType : uint32_t {
    GrowCounter   = 1,   // Grow-only counter
    PNCounter     = 2,   // Positive-negative counter
    LWWRegister   = 3,   // Last-writer-wins register
    ORSet         = 4,   // Observed-remove set
    MVRegister    = 5,   // Multi-value register
};

// ---------------------------------------------------------------------------
// CRDTEntry — Conflict-free replicated data entry
// ---------------------------------------------------------------------------
struct CRDTEntry {
    uint64_t    originNode;     // First 8 bytes of originating node ID
    uint64_t    key;            // FNV-1a hash of entry key
    uint64_t    value;          // Entry value (or pointer)
    uint64_t    timestamp;      // Lamport logical clock
    CRDTType    type;
    uint32_t    flags;          // Tombstone, dirty, synced
    uint64_t    seqNo;          // Monotonic sequence number
};

// ---------------------------------------------------------------------------
// ZKProof — Zero-knowledge proof of optimization
// ---------------------------------------------------------------------------
struct ZKProof {
    uint64_t    commitment[4];  // 32-byte commitment
    uint64_t    challenge[4];   // 32-byte challenge
    uint64_t    response[8];    // 64-byte response
    int32_t     perfDelta;      // Claimed performance improvement %
    uint32_t    verified;       // 0 = unverified, 1 = valid
    uint64_t    timestamp;
};

// ---------------------------------------------------------------------------
// MeshNodeInfo — Peer node in the mesh topology
// ---------------------------------------------------------------------------
struct MeshNodeInfo {
    uint64_t    nodeId[4];       // 256-bit node ID
    uint32_t    publicIP;        // IPv4 (network byte order)
    uint16_t    port;
    uint16_t    flags;           // Online, NAT, Relay
    uint32_t    fitness;         // Node fitness score
    uint64_t    lastSeen;        // RDTSC timestamp
    uint64_t    bytesSent;
    uint64_t    bytesReceived;
    uint64_t    shardsAvail[64]; // 4096-bit shard availability bitfield
};

// ---------------------------------------------------------------------------
// MeshStats — Runtime statistics
// ---------------------------------------------------------------------------
struct MeshStats {
    uint64_t    peersActive;
    uint64_t    crdtMerges;
    uint64_t    zkpGenerated;
    uint64_t    zkpVerified;
    uint64_t    gossipSent;
    uint64_t    gossipRecv;
    uint64_t    shardsServed;
    uint64_t    shardsRecv;
    uint64_t    quorumRounds;
    uint64_t    topologyChanges;
    uint64_t    bytesTx;
    uint64_t    bytesRx;
};

// ---------------------------------------------------------------------------
// FederatedDelta — Weight delta for federated averaging
// ---------------------------------------------------------------------------
struct FederatedDelta {
    std::string     sourceNodeId;
    std::vector<float> weightDeltas;
    uint64_t        generation;     // Bootstrap generation that produced this delta
    int32_t         perfImprovement; // Percentage improvement
};

// ---------------------------------------------------------------------------
// ModelShard — Torrent-style shard descriptor
// ---------------------------------------------------------------------------
struct ModelShard {
    uint32_t    pieceIndex;
    uint64_t    pieceSize;
    uint8_t     hash[16];       // Blake2b-128 hash
    bool        available;      // Local availability
    uint32_t    seedCount;      // Number of peers seeding this piece
};

// ---------------------------------------------------------------------------
// QuorumResult — Consensus vote result
// ---------------------------------------------------------------------------
struct QuorumResult {
    bool        reached;        // Quorum threshold met
    bool        approved;       // Majority voted yes
    uint32_t    yesVotes;
    uint32_t    noVotes;
    uint32_t    abstentions;
    uint32_t    totalVoters;
};

// ---------------------------------------------------------------------------
// GossipMessage — Epidemic broadcast message
// ---------------------------------------------------------------------------
struct GossipMessage {
    uint64_t    messageId;
    uint32_t    ttl;            // Time-to-live (hop count)
    std::string topic;
    std::vector<uint8_t> payload;
    uint64_t    originTimestamp;
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
typedef void (*MeshCallback)(const char* event, void* data, void* userData);
typedef void (*GossipSendFn)(void* peerAddr, void* msg, uint64_t size);

// ---------------------------------------------------------------------------
// MeshBrain — Distributed consciousness orchestrator
// ---------------------------------------------------------------------------
class MeshBrain {
public:
    static MeshBrain& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;
    PatchResult shutdown();

    // ---- CRDT Operations ----
    PatchResult mergeRemoteState(const std::vector<CRDTEntry>& entries);
    PatchResult computeDelta(uint64_t sinceTimestamp, std::vector<CRDTEntry>& outDelta);
    PatchResult putCRDT(uint64_t key, uint64_t value, CRDTType type);
    bool        getCRDT(uint64_t key, uint64_t& outValue) const;
    uint32_t    getCRDTCount() const;

    // ---- Zero-Knowledge Proofs ----
    PatchResult generateOptimizationProof(int32_t perfDelta, ZKProof& outProof);
    PatchResult verifyOptimizationProof(ZKProof& proof);
    const std::vector<ZKProof>& getVerifiedProofs() const;

    // ---- Peer Discovery (DHT) ----
    PatchResult addPeer(const MeshNodeInfo& node);
    PatchResult removePeer(const uint64_t nodeId[4]);
    uint32_t    xorDistance(const uint64_t idA[4], const uint64_t idB[4]) const;
    std::vector<MeshNodeInfo> findClosestPeers(const uint64_t targetId[4], uint32_t k) const;
    uint32_t    getPeerCount() const;
    std::vector<MeshNodeInfo> getAllPeers() const;

    // ---- Federated Learning ----
    PatchResult submitWeightDelta(const FederatedDelta& delta);
    PatchResult aggregateDeltas(std::vector<float>& outAveraged);
    uint32_t    getPendingDeltaCount() const;

    // ---- Gossip Protocol ----
    PatchResult broadcast(const GossipMessage& msg);
    void        registerGossipHandler(const std::string& topic,
                                       std::function<void(const GossipMessage&)> handler);
    void        setGossipSendCallback(GossipSendFn fn);

    // ---- Model Shard Distribution ----
    PatchResult registerShard(uint32_t pieceIndex, const void* data, uint64_t size);
    bool        hasShard(uint32_t pieceIndex) const;
    PatchResult announceShard(uint32_t pieceIndex);
    std::vector<ModelShard> getShardMap() const;

    // ---- Quorum Consensus ----
    QuorumResult conductVote(const std::vector<uint8_t>& votes, uint32_t threshold = 67);

    // ---- Topology ----
    PatchResult updateTopology(const MeshNodeInfo& node);
    uint32_t    getActiveNodeCount() const;

    // ---- Statistics ----
    MeshStats   getStats() const;

    // ---- Callbacks ----
    void registerCallback(MeshCallback cb, void* userData);

    // ---- Diagnostics ----
    size_t dumpDiagnostics(char* buffer, size_t bufferSize) const;

private:
    MeshBrain();
    ~MeshBrain();
    MeshBrain(const MeshBrain&) = delete;
    MeshBrain& operator=(const MeshBrain&) = delete;

    void notifyCallback(const char* event, void* data);

    mutable std::mutex              m_mutex;
    bool                            m_initialized;
    std::vector<FederatedDelta>     m_pendingDeltas;
    std::vector<ZKProof>            m_verifiedProofs;
    std::vector<ModelShard>         m_shardMap;
    GossipSendFn                    m_gossipSendFn;
    struct CBEntry { MeshCallback fn; void* userData; };
    std::vector<CBEntry>            m_callbacks;

    // ---- CRDT local store (key → entry, last-writer-wins by timestamp) ----
    std::vector<CRDTEntry>          m_crdtEntries;

    // ---- Peer table (DHT-style, keyed by nodeId) ----
    std::vector<MeshNodeInfo>       m_peers;

    // ---- Gossip handler dispatch (topic → handler) ----
    std::unordered_map<std::string, std::function<void(const GossipMessage&)>> m_gossipHandlers;
};
