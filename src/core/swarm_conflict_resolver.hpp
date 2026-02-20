// =============================================================================
// swarm_conflict_resolver.hpp
// RawrXD v14.2.1 Cathedral — Phase D: Multi-Agent Conflict Resolution
//
// Negotiation protocols for multi-agent coordination:
//   - Two agents editing same file → merge strategy
//   - Semantic conflict detection (delete vs. reference)
//   - Consensus voting on architectural decisions
//   - Resource allocation bidding (GPU, CPU cores)
//   - LLM-based merge negotiation
//
// Extends swarm_coordinator.h with conflict resolution capabilities.
// =============================================================================
#pragma once
#ifndef RAWRXD_SWARM_CONFLICT_RESOLVER_HPP
#define RAWRXD_SWARM_CONFLICT_RESOLVER_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>

namespace RawrXD {
namespace Swarm {

// =============================================================================
// Result Types
// =============================================================================

struct ConflictResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static ConflictResult ok(const char* msg) {
        ConflictResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static ConflictResult error(const char* msg, int code = -1) {
        ConflictResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class ConflictType : uint8_t {
    FileEditConflict,       // Two agents editing same file
    SymbolDeleteRef,        // Agent A deletes, Agent B references
    ArchitecturalDisagree,  // Conflicting design decisions
    ResourceContention,     // Multiple agents want same resource
    BuildOrderConflict,     // Conflicting build targets
    TestMutualExclusion,    // Tests that can't run simultaneously
    ConfigConflict,         // Conflicting configuration changes
    MergeConflict           // Git-style textual merge conflict
};

enum class ResolutionStrategy : uint8_t {
    PriorityWins,           // Higher-priority agent wins
    LastWriterWins,         // Most recent write takes precedence
    ThreeWayMerge,          // Git-style merge with common ancestor
    LLMNegotiation,         // Ask LLM to resolve the conflict
    ConsensusVote,          // All agents vote
    HumanEscalation,        // Escalate to human
    AutoMerge,              // Automatic non-conflicting merge
    Retry                   // Abort both, retry with coordination
};

enum class VoteType : uint8_t {
    Accept,
    Reject,
    Abstain,
    Conditional             // Accept if certain conditions met
};

enum class ResourceType : uint8_t {
    CpuCores,
    GpuMemory,
    GpuCompute,
    DiskIO,
    NetworkBandwidth,
    ModelInference,
    BuildSlot,
    TestSlot
};

// =============================================================================
// Core Data Types
// =============================================================================

struct ConflictEvent {
    uint64_t        conflictId;
    ConflictType    type;
    uint64_t        agentA;          // First agent involved
    uint64_t        agentB;          // Second agent involved
    char            targetFile[260]; // File or resource being contested
    char            symbolA[256];    // What Agent A is doing
    char            symbolB[256];    // What Agent B is doing
    char            description[512];
    uint64_t        detectedAt;
    bool            resolved;
    ResolutionStrategy usedStrategy;
    char            resolution[512]; // Description of how it was resolved
};

struct MergeRegion {
    int             startLine;
    int             endLine;
    char            agentId[64];    // Who wrote this region
    char            content[4096];  // The text content
    uint64_t        timestamp;
};

struct AgentVote {
    uint64_t        agentId;
    VoteType        vote;
    char            reason[256];
    float           confidence;      // 0.0 – 1.0
    uint64_t        timestamp;
};

struct ConsensusProposal {
    uint64_t        proposalId;
    uint64_t        proposerAgent;
    char            title[256];
    char            description[1024];
    char            codeChanges[4096];
    std::vector<AgentVote> votes;
    bool            decided;
    bool            accepted;
    float           quorumThreshold;  // Default: 0.51 (simple majority)
    uint64_t        expiresAt;
};

struct ResourceBid {
    uint64_t        bidId;
    uint64_t        agentId;
    ResourceType    resource;
    float           amount;          // Requested amount (cores, MB, etc.)
    float           priority;        // 0.0 – 1.0: urgency
    float           bidPrice;        // Abstract "cost" willing to pay
    uint64_t        durationMs;      // How long the resource is needed
    char            justification[256];
    uint64_t        timestamp;
};

struct ResourceAllocation {
    uint64_t        allocationId;
    uint64_t        bidId;           // Winning bid
    uint64_t        agentId;
    ResourceType    resource;
    float           allocated;       // Amount allocated
    uint64_t        expiresAt;
    bool            active;
};

// =============================================================================
// Callbacks (C function pointers)
// =============================================================================

typedef void (*ConflictDetectedCallback)(const ConflictEvent* event, void* userData);
typedef void (*ConflictResolvedCallback)(const ConflictEvent* event, void* userData);
typedef void (*ConsensusReachedCallback)(const ConsensusProposal* proposal, void* userData);
typedef void (*ResourceAllocatedCallback)(const ResourceAllocation* alloc, void* userData);
typedef std::string (*MergeNegotiatorFn)(const char* codeA, const char* codeB,
                                          const char* ancestor, const char* context, void* userData);

// =============================================================================
// Configuration
// =============================================================================

struct ConflictConfig {
    ResolutionStrategy defaultStrategy;    // Default: ThreeWayMerge
    float              consensusQuorum;    // Default: 0.51
    uint64_t           consensusTimeoutMs; // Default: 30000 (30 sec)
    bool               autoMergeNonOverlap;// Default: true
    bool               escalateOnFailure;  // Default: true
    int                maxMergeRetries;    // Default: 3
    int                maxBidsPerAgent;    // Default: 5

    // Resource pool sizes
    float              totalCpuCores;      // Default: 8
    float              totalGpuMemoryMB;   // Default: 8192
    float              totalBuildSlots;    // Default: 4
    float              totalTestSlots;     // Default: 2

    static ConflictConfig defaults() {
        ConflictConfig c;
        c.defaultStrategy      = ResolutionStrategy::ThreeWayMerge;
        c.consensusQuorum      = 0.51f;
        c.consensusTimeoutMs   = 30000;
        c.autoMergeNonOverlap  = true;
        c.escalateOnFailure    = true;
        c.maxMergeRetries      = 3;
        c.maxBidsPerAgent      = 5;
        c.totalCpuCores        = 8.0f;
        c.totalGpuMemoryMB     = 8192.0f;
        c.totalBuildSlots      = 4.0f;
        c.totalTestSlots       = 2.0f;
        return c;
    }
};

// =============================================================================
// Core Class: SwarmConflictResolver
// =============================================================================

class SwarmConflictResolver {
public:
    static SwarmConflictResolver& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    ConflictResult initialize(const ConflictConfig& config);
    ConflictResult initialize();
    void shutdown();
    bool isActive() const { return m_active.load(); }

    // ── Conflict Detection ─────────────────────────────────────────────────
    // Called by SwarmCoordinator when a potential conflict is detected
    ConflictResult detectConflict(uint64_t agentA, uint64_t agentB,
                                   ConflictType type,
                                   const char* targetFile,
                                   const char* descA, const char* descB);

    // Check if a file is currently being edited by another agent
    bool isFileContested(const char* file) const;
    uint64_t getFileOwner(const char* file) const;

    // Check for semantic conflicts (delete vs. reference)
    ConflictResult checkSemanticConflict(const char* symbolName,
                                          uint64_t deletingAgent,
                                          const char* file);

    // ── Conflict Resolution ────────────────────────────────────────────────
    ConflictResult resolveConflict(uint64_t conflictId);
    ConflictResult resolveConflict(uint64_t conflictId, ResolutionStrategy strategy);

    // Three-way merge
    ConflictResult threeWayMerge(const char* fileA, const char* fileB,
                                  const char* ancestor, const char* outputFile);

    // LLM negotiation
    ConflictResult negotiateResolution(uint64_t conflictId);

    // ── Consensus Voting ───────────────────────────────────────────────────
    uint64_t proposeChange(uint64_t proposerAgent,
                            const char* title,
                            const char* description,
                            const char* codeChanges,
                            float quorum = 0.51f);

    ConflictResult castVote(uint64_t proposalId, uint64_t agentId,
                             VoteType vote, const char* reason = nullptr,
                             float confidence = 1.0f);

    ConflictResult evaluateProposal(uint64_t proposalId);
    ConsensusProposal getProposal(uint64_t proposalId) const;
    std::vector<ConsensusProposal> getActiveProposals() const;

    // ── Resource Allocation (Bidding System) ───────────────────────────────
    uint64_t placeBid(uint64_t agentId, ResourceType resource,
                       float amount, float priority, float price,
                       uint64_t durationMs, const char* justification = nullptr);

    ConflictResult evaluateBids(ResourceType resource);
    ConflictResult releaseResource(uint64_t allocationId);

    std::vector<ResourceAllocation> getActiveAllocations() const;
    float getAvailableResource(ResourceType resource) const;

    // ── File Ownership Tracking ────────────────────────────────────────────
    ConflictResult acquireFile(uint64_t agentId, const char* file);
    ConflictResult releaseFile(uint64_t agentId, const char* file);
    std::vector<std::string> getAgentFiles(uint64_t agentId) const;

    // ── Query ──────────────────────────────────────────────────────────────
    ConflictEvent getConflict(uint64_t conflictId) const;
    std::vector<ConflictEvent> getActiveConflicts() const;
    std::vector<ConflictEvent> getResolvedConflicts(int maxCount = 100) const;
    int getActiveConflictCount() const;

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setConflictDetectedCallback(ConflictDetectedCallback cb, void* ud);
    void setConflictResolvedCallback(ConflictResolvedCallback cb, void* ud);
    void setConsensusReachedCallback(ConsensusReachedCallback cb, void* ud);
    void setResourceAllocatedCallback(ResourceAllocatedCallback cb, void* ud);
    void setMergeNegotiator(MergeNegotiatorFn fn, void* ud);

    // ── Statistics ──────────────────────────────────────────────────────────
    struct Stats {
        uint64_t totalConflicts;
        uint64_t resolvedConflicts;
        uint64_t escalatedConflicts;
        uint64_t mergeSuccesses;
        uint64_t mergeFails;
        uint64_t consensusReached;
        uint64_t consensusFailed;
        uint64_t resourceBids;
        uint64_t resourceAllocations;
        double   avgResolutionTimeMs;
    };

    Stats getStats() const;
    std::string statsToJson() const;
    std::string conflictsToJson() const;

private:
    SwarmConflictResolver();
    ~SwarmConflictResolver();
    SwarmConflictResolver(const SwarmConflictResolver&) = delete;
    SwarmConflictResolver& operator=(const SwarmConflictResolver&) = delete;

    // Internal resolution methods
    ConflictResult resolveByPriority(ConflictEvent& conflict);
    ConflictResult resolveByMerge(ConflictEvent& conflict);
    ConflictResult resolveByConsensus(ConflictEvent& conflict);
    ConflictResult resolveByLLM(ConflictEvent& conflict);
    ConflictResult escalateToHuman(ConflictEvent& conflict);

    // Resource allocation helpers
    float calculateAllocatedAmount(ResourceType resource) const;
    void expireAllocations();

    uint64_t nextConflictId();
    uint64_t nextProposalId();
    uint64_t nextBidId();
    uint64_t nextAllocId();

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    mutable std::mutex m_fileMutex;
    mutable std::mutex m_resourceMutex;
    std::atomic<bool>     m_active{false};
    std::atomic<uint64_t> m_nextConflict{1};
    std::atomic<uint64_t> m_nextProposal{1};
    std::atomic<uint64_t> m_nextBid{1};
    std::atomic<uint64_t> m_nextAlloc{1};

    ConflictConfig m_config;

    // Conflicts
    std::vector<ConflictEvent> m_conflicts;

    // File ownership: file → agentId
    std::unordered_map<std::string, uint64_t> m_fileOwnership;

    // Proposals and votes
    std::unordered_map<uint64_t, ConsensusProposal> m_proposals;

    // Resource bids and allocations
    std::vector<ResourceBid>        m_bids;
    std::vector<ResourceAllocation> m_allocations;

    // Callbacks
    ConflictDetectedCallback  m_conflictDetectedCb;  void* m_conflictDetectedUD;
    ConflictResolvedCallback  m_conflictResolvedCb;  void* m_conflictResolvedUD;
    ConsensusReachedCallback  m_consensusCb;         void* m_consensusUD;
    ResourceAllocatedCallback m_resourceCb;          void* m_resourceUD;
    MergeNegotiatorFn         m_negotiator;          void* m_negotiatorUD;

    alignas(64) Stats m_stats;
};

} // namespace Swarm
} // namespace RawrXD

#endif // RAWRXD_SWARM_CONFLICT_RESOLVER_HPP
