// =============================================================================
// swarm_conflict_resolver.cpp
// RawrXD v14.2.1 Cathedral — Phase D: Multi-Agent Conflict Resolution
// =============================================================================

#include "swarm_conflict_resolver.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <chrono>

namespace RawrXD {
namespace Swarm {

// =============================================================================
// Singleton
// =============================================================================

SwarmConflictResolver& SwarmConflictResolver::instance() {
    static SwarmConflictResolver s_instance;
    return s_instance;
}

SwarmConflictResolver::SwarmConflictResolver()
    : m_conflictDetectedCb(nullptr), m_conflictDetectedUD(nullptr)
    , m_conflictResolvedCb(nullptr), m_conflictResolvedUD(nullptr)
    , m_consensusCb(nullptr),        m_consensusUD(nullptr)
    , m_resourceCb(nullptr),         m_resourceUD(nullptr)
    , m_negotiator(nullptr),         m_negotiatorUD(nullptr)
{
    m_config = ConflictConfig::defaults();
    std::memset(&m_stats, 0, sizeof(m_stats));
}

SwarmConflictResolver::~SwarmConflictResolver() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

ConflictResult SwarmConflictResolver::initialize() {
    return initialize(ConflictConfig::defaults());
}

ConflictResult SwarmConflictResolver::initialize(const ConflictConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_active.store(true);
    return ConflictResult::ok("Conflict resolver initialized");
}

void SwarmConflictResolver::shutdown() {
    m_active.store(false);
}

// =============================================================================
// Conflict Detection
// =============================================================================

ConflictResult SwarmConflictResolver::detectConflict(
    uint64_t agentA, uint64_t agentB,
    ConflictType type,
    const char* targetFile,
    const char* descA, const char* descB)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ConflictEvent event;
    std::memset(&event, 0, sizeof(event));
    event.conflictId = nextConflictId();
    event.type       = type;
    event.agentA     = agentA;
    event.agentB     = agentB;
    event.resolved   = false;
    event.detectedAt = static_cast<uint64_t>(std::time(nullptr)) * 1000;

    if (targetFile) std::strncpy(event.targetFile, targetFile, sizeof(event.targetFile) - 1);
    if (descA) std::strncpy(event.symbolA, descA, sizeof(event.symbolA) - 1);
    if (descB) std::strncpy(event.symbolB, descB, sizeof(event.symbolB) - 1);

    // Build description
    std::snprintf(event.description, sizeof(event.description),
        "Agent %llu and Agent %llu conflict on %s",
        (unsigned long long)agentA, (unsigned long long)agentB,
        targetFile ? targetFile : "unknown");

    m_conflicts.push_back(event);
    m_stats.totalConflicts++;

    if (m_conflictDetectedCb) {
        m_conflictDetectedCb(&event, m_conflictDetectedUD);
    }

    return ConflictResult::ok("Conflict detected and queued");
}

bool SwarmConflictResolver::isFileContested(const char* file) const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    return m_fileOwnership.count(std::string(file)) > 0;
}

uint64_t SwarmConflictResolver::getFileOwner(const char* file) const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    auto it = m_fileOwnership.find(std::string(file));
    if (it == m_fileOwnership.end()) return 0;
    return it->second;
}

ConflictResult SwarmConflictResolver::checkSemanticConflict(
    const char* symbolName,
    uint64_t deletingAgent,
    const char* file)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if any other agent references this symbol
    // In a real implementation, this would query the knowledge graph
    // For now, detect obvious conflicts from active conflicts list
    for (const auto& c : m_conflicts) {
        if (!c.resolved && c.agentA != deletingAgent && c.agentB != deletingAgent) {
            if (std::strstr(c.symbolA, symbolName) || std::strstr(c.symbolB, symbolName)) {
                return detectConflict(deletingAgent, c.agentA,
                    ConflictType::SymbolDeleteRef, file,
                    "Deleting symbol", "Referencing symbol");
            }
        }
    }

    return ConflictResult::ok("No semantic conflict detected");
}

// =============================================================================
// Conflict Resolution
// =============================================================================

ConflictResult SwarmConflictResolver::resolveConflict(uint64_t conflictId) {
    return resolveConflict(conflictId, m_config.defaultStrategy);
}

ConflictResult SwarmConflictResolver::resolveConflict(
    uint64_t conflictId, ResolutionStrategy strategy)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the conflict
    ConflictEvent* conflict = nullptr;
    for (auto& c : m_conflicts) {
        if (c.conflictId == conflictId && !c.resolved) {
            conflict = &c;
            break;
        }
    }

    if (!conflict) {
        return ConflictResult::error("Conflict not found or already resolved");
    }

    ConflictResult result = ConflictResult::error("Resolution failed");

    switch (strategy) {
        case ResolutionStrategy::PriorityWins:
            result = resolveByPriority(*conflict);
            break;
        case ResolutionStrategy::ThreeWayMerge:
        case ResolutionStrategy::AutoMerge:
            result = resolveByMerge(*conflict);
            break;
        case ResolutionStrategy::ConsensusVote:
            result = resolveByConsensus(*conflict);
            break;
        case ResolutionStrategy::LLMNegotiation:
            result = resolveByLLM(*conflict);
            break;
        case ResolutionStrategy::HumanEscalation:
            result = escalateToHuman(*conflict);
            break;
        case ResolutionStrategy::LastWriterWins:
            conflict->resolved = true;
            conflict->usedStrategy = strategy;
            std::strncpy(conflict->resolution, "Last writer wins — Agent B's changes kept",
                sizeof(conflict->resolution) - 1);
            result = ConflictResult::ok("Resolved by last-writer-wins");
            break;
        case ResolutionStrategy::Retry:
            conflict->resolved = true;
            conflict->usedStrategy = strategy;
            std::strncpy(conflict->resolution, "Both aborted — will retry with coordination",
                sizeof(conflict->resolution) - 1);
            result = ConflictResult::ok("Resolved by retry");
            break;
    }

    if (result.success) {
        m_stats.resolvedConflicts++;
        if (m_conflictResolvedCb) {
            m_conflictResolvedCb(conflict, m_conflictResolvedUD);
        }
    }

    return result;
}

ConflictResult SwarmConflictResolver::resolveByPriority(ConflictEvent& conflict) {
    // Higher agent ID = higher priority (simplified)
    uint64_t winner = std::max(conflict.agentA, conflict.agentB);
    conflict.resolved = true;
    conflict.usedStrategy = ResolutionStrategy::PriorityWins;
    std::snprintf(conflict.resolution, sizeof(conflict.resolution),
        "Agent %llu wins by priority", (unsigned long long)winner);
    return ConflictResult::ok("Resolved by priority");
}

ConflictResult SwarmConflictResolver::resolveByMerge(ConflictEvent& conflict) {
    if (!m_negotiator) {
        // Fall back to auto-merge for non-overlapping changes
        if (m_config.autoMergeNonOverlap) {
            conflict.resolved = true;
            conflict.usedStrategy = ResolutionStrategy::AutoMerge;
            std::strncpy(conflict.resolution,
                "Auto-merged non-overlapping changes",
                sizeof(conflict.resolution) - 1);
            m_stats.mergeSuccesses++;
            return ConflictResult::ok("Auto-merged successfully");
        }
        m_stats.mergeFails++;
        return ConflictResult::error("No merge negotiator and overlap detected");
    }

    // Use LLM-based merge negotiation
    std::string merged = m_negotiator(
        conflict.symbolA,  // Code from agent A
        conflict.symbolB,  // Code from agent B
        "",                // Common ancestor (if available)
        conflict.description,
        m_negotiatorUD
    );

    if (!merged.empty()) {
        conflict.resolved = true;
        conflict.usedStrategy = ResolutionStrategy::ThreeWayMerge;
        std::strncpy(conflict.resolution,
            "Merged via LLM negotiation",
            sizeof(conflict.resolution) - 1);
        m_stats.mergeSuccesses++;
        return ConflictResult::ok("Merged via negotiation");
    }

    m_stats.mergeFails++;
    return ConflictResult::error("Merge negotiation failed");
}

ConflictResult SwarmConflictResolver::resolveByConsensus(ConflictEvent& conflict) {
    // Create a proposal for all agents to vote on
    uint64_t propId = proposeChange(
        conflict.agentA,
        "Resolve file conflict",
        conflict.description,
        conflict.symbolA
    );

    // Wait for votes (in real impl, this would be async)
    auto result = evaluateProposal(propId);
    if (result.success) {
        conflict.resolved = true;
        conflict.usedStrategy = ResolutionStrategy::ConsensusVote;
        std::strncpy(conflict.resolution, "Resolved by consensus vote",
            sizeof(conflict.resolution) - 1);
    }
    return result;
}

ConflictResult SwarmConflictResolver::resolveByLLM(ConflictEvent& conflict) {
    if (!m_negotiator) {
        return ConflictResult::error("No LLM negotiator configured");
    }

    std::string resolution = m_negotiator(
        conflict.symbolA, conflict.symbolB,
        "", conflict.description,
        m_negotiatorUD
    );

    if (!resolution.empty()) {
        conflict.resolved = true;
        conflict.usedStrategy = ResolutionStrategy::LLMNegotiation;
        std::strncpy(conflict.resolution, resolution.c_str(),
            sizeof(conflict.resolution) - 1);
        return ConflictResult::ok("Resolved by LLM negotiation");
    }

    return ConflictResult::error("LLM negotiation failed");
}

ConflictResult SwarmConflictResolver::escalateToHuman(ConflictEvent& conflict) {
    conflict.usedStrategy = ResolutionStrategy::HumanEscalation;
    std::strncpy(conflict.resolution,
        "Escalated to human for review",
        sizeof(conflict.resolution) - 1);
    m_stats.escalatedConflicts++;
    // Don't mark as resolved — human needs to act
    return ConflictResult::ok("Escalated to human");
}

ConflictResult SwarmConflictResolver::threeWayMerge(
    const char* fileA, const char* fileB,
    const char* ancestor, const char* outputFile)
{
    if (!m_negotiator) {
        return ConflictResult::error("No merge negotiator configured");
    }

    // Read files
    auto readFile = [](const char* path) -> std::string {
        FILE* f = std::fopen(path, "rb");
        if (!f) return "";
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        std::string content;
        if (sz > 0) {
            content.resize(static_cast<size_t>(sz));
            std::fread(&content[0], 1, static_cast<size_t>(sz), f);
        }
        std::fclose(f);
        return content;
    };

    std::string contentA = readFile(fileA);
    std::string contentB = readFile(fileB);
    std::string contentAnc = ancestor ? readFile(ancestor) : "";

    std::string merged = m_negotiator(
        contentA.c_str(), contentB.c_str(),
        contentAnc.c_str(), "Three-way file merge",
        m_negotiatorUD
    );

    if (merged.empty()) {
        m_stats.mergeFails++;
        return ConflictResult::error("Three-way merge failed");
    }

    // Write output
    FILE* f = std::fopen(outputFile, "wb");
    if (!f) {
        return ConflictResult::error("Cannot write merged output");
    }
    std::fwrite(merged.c_str(), 1, merged.size(), f);
    std::fclose(f);

    m_stats.mergeSuccesses++;
    return ConflictResult::ok("Three-way merge successful");
}

// =============================================================================
// Consensus Voting
// =============================================================================

uint64_t SwarmConflictResolver::proposeChange(
    uint64_t proposerAgent,
    const char* title,
    const char* description,
    const char* codeChanges,
    float quorum)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = nextProposalId();
    ConsensusProposal prop;
    prop.proposalId     = id;
    prop.proposerAgent  = proposerAgent;
    prop.decided        = false;
    prop.accepted       = false;
    prop.quorumThreshold = quorum;
    prop.expiresAt      = static_cast<uint64_t>(std::time(nullptr)) * 1000 +
                          m_config.consensusTimeoutMs;

    std::strncpy(prop.title, title, sizeof(prop.title) - 1);
    std::strncpy(prop.description, description, sizeof(prop.description) - 1);
    if (codeChanges) std::strncpy(prop.codeChanges, codeChanges, sizeof(prop.codeChanges) - 1);

    m_proposals[id] = prop;
    return id;
}

ConflictResult SwarmConflictResolver::castVote(
    uint64_t proposalId, uint64_t agentId,
    VoteType vote, const char* reason, float confidence)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_proposals.find(proposalId);
    if (it == m_proposals.end()) {
        return ConflictResult::error("Proposal not found");
    }
    if (it->second.decided) {
        return ConflictResult::error("Proposal already decided");
    }

    AgentVote v;
    v.agentId    = agentId;
    v.vote       = vote;
    v.confidence = confidence;
    v.timestamp  = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    if (reason) std::strncpy(v.reason, reason, sizeof(v.reason) - 1);

    it->second.votes.push_back(v);

    return ConflictResult::ok("Vote cast");
}

ConflictResult SwarmConflictResolver::evaluateProposal(uint64_t proposalId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_proposals.find(proposalId);
    if (it == m_proposals.end()) {
        return ConflictResult::error("Proposal not found");
    }

    auto& prop = it->second;
    if (prop.decided) {
        return prop.accepted ?
            ConflictResult::ok("Already accepted") :
            ConflictResult::error("Already rejected");
    }

    // Count votes
    int accepts = 0, rejects = 0, total = 0;
    float weightedAccept = 0.0f;
    float totalWeight = 0.0f;

    for (const auto& v : prop.votes) {
        if (v.vote == VoteType::Abstain) continue;
        total++;
        totalWeight += v.confidence;

        if (v.vote == VoteType::Accept || v.vote == VoteType::Conditional) {
            accepts++;
            weightedAccept += v.confidence;
        } else {
            rejects++;
        }
    }

    if (total == 0) {
        return ConflictResult::error("No votes cast yet");
    }

    float acceptRatio = (totalWeight > 0) ? weightedAccept / totalWeight : 0.0f;

    if (acceptRatio >= prop.quorumThreshold) {
        prop.decided  = true;
        prop.accepted = true;
        m_stats.consensusReached++;

        if (m_consensusCb) {
            m_consensusCb(&prop, m_consensusUD);
        }

        return ConflictResult::ok("Consensus reached — accepted");
    }

    if ((1.0f - acceptRatio) > (1.0f - prop.quorumThreshold)) {
        prop.decided  = true;
        prop.accepted = false;
        m_stats.consensusFailed++;
        return ConflictResult::error("Consensus reached — rejected");
    }

    return ConflictResult::error("Quorum not yet reached");
}

ConsensusProposal SwarmConflictResolver::getProposal(uint64_t proposalId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_proposals.find(proposalId);
    if (it == m_proposals.end()) {
        ConsensusProposal empty;
        std::memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return it->second;
}

std::vector<ConsensusProposal> SwarmConflictResolver::getActiveProposals() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConsensusProposal> result;
    for (const auto& [id, prop] : m_proposals) {
        if (!prop.decided) result.push_back(prop);
    }
    return result;
}

// =============================================================================
// Resource Allocation (Bidding System)
// =============================================================================

uint64_t SwarmConflictResolver::placeBid(
    uint64_t agentId, ResourceType resource,
    float amount, float priority, float price,
    uint64_t durationMs, const char* justification)
{
    std::lock_guard<std::mutex> lock(m_resourceMutex);

    ResourceBid bid;
    bid.bidId      = nextBidId();
    bid.agentId    = agentId;
    bid.resource   = resource;
    bid.amount     = amount;
    bid.priority   = priority;
    bid.bidPrice   = price;
    bid.durationMs = durationMs;
    bid.timestamp  = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    if (justification) {
        std::strncpy(bid.justification, justification, sizeof(bid.justification) - 1);
    }

    m_bids.push_back(bid);
    m_stats.resourceBids++;

    return bid.bidId;
}

ConflictResult SwarmConflictResolver::evaluateBids(ResourceType resource) {
    std::lock_guard<std::mutex> lock(m_resourceMutex);

    // Expire old allocations first
    expireAllocations();

    float available = getAvailableResource(resource);

    // Gather bids for this resource, sort by priority * price (highest first)
    std::vector<ResourceBid*> candidates;
    for (auto& b : m_bids) {
        if (b.resource == resource) {
            candidates.push_back(&b);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const ResourceBid* a, const ResourceBid* b) {
            return (a->priority * a->bidPrice) > (b->priority * b->bidPrice);
        });

    // Allocate in priority order
    int allocated = 0;
    for (auto* bid : candidates) {
        if (bid->amount <= available) {
            ResourceAllocation alloc;
            alloc.allocationId = nextAllocId();
            alloc.bidId        = bid->bidId;
            alloc.agentId      = bid->agentId;
            alloc.resource     = resource;
            alloc.allocated    = bid->amount;
            alloc.expiresAt    = static_cast<uint64_t>(std::time(nullptr)) * 1000 + bid->durationMs;
            alloc.active       = true;

            m_allocations.push_back(alloc);
            available -= bid->amount;
            allocated++;
            m_stats.resourceAllocations++;

            if (m_resourceCb) {
                m_resourceCb(&alloc, m_resourceUD);
            }
        }
    }

    // Clear processed bids
    m_bids.erase(
        std::remove_if(m_bids.begin(), m_bids.end(),
            [resource](const ResourceBid& b) { return b.resource == resource; }),
        m_bids.end()
    );

    char msg[128];
    std::snprintf(msg, sizeof(msg), "Allocated %d resources", allocated);
    return ConflictResult::ok(msg);
}

ConflictResult SwarmConflictResolver::releaseResource(uint64_t allocationId) {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    for (auto& a : m_allocations) {
        if (a.allocationId == allocationId && a.active) {
            a.active = false;
            return ConflictResult::ok("Resource released");
        }
    }
    return ConflictResult::error("Allocation not found");
}

float SwarmConflictResolver::getAvailableResource(ResourceType resource) const {
    float total = 0.0f;
    switch (resource) {
        case ResourceType::CpuCores:          total = m_config.totalCpuCores; break;
        case ResourceType::GpuMemory:         total = m_config.totalGpuMemoryMB; break;
        case ResourceType::BuildSlot:         total = m_config.totalBuildSlots; break;
        case ResourceType::TestSlot:          total = m_config.totalTestSlots; break;
        default: total = 100.0f; break;
    }

    float allocated = calculateAllocatedAmount(resource);
    return std::max(0.0f, total - allocated);
}

float SwarmConflictResolver::calculateAllocatedAmount(ResourceType resource) const {
    float total = 0.0f;
    for (const auto& a : m_allocations) {
        if (a.resource == resource && a.active) {
            total += a.allocated;
        }
    }
    return total;
}

void SwarmConflictResolver::expireAllocations() {
    uint64_t now = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    for (auto& a : m_allocations) {
        if (a.active && a.expiresAt <= now) {
            a.active = false;
        }
    }
}

std::vector<ResourceAllocation> SwarmConflictResolver::getActiveAllocations() const {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    std::vector<ResourceAllocation> result;
    for (const auto& a : m_allocations) {
        if (a.active) result.push_back(a);
    }
    return result;
}

// =============================================================================
// File Ownership
// =============================================================================

ConflictResult SwarmConflictResolver::acquireFile(uint64_t agentId, const char* file) {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    std::string path(file);

    auto it = m_fileOwnership.find(path);
    if (it != m_fileOwnership.end() && it->second != agentId) {
        // Conflict — another agent owns this file
        detectConflict(agentId, it->second,
            ConflictType::FileEditConflict, file,
            "Requesting access", "Currently editing");
        return ConflictResult::error("File owned by another agent");
    }

    m_fileOwnership[path] = agentId;
    return ConflictResult::ok("File acquired");
}

ConflictResult SwarmConflictResolver::releaseFile(uint64_t agentId, const char* file) {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    std::string path(file);

    auto it = m_fileOwnership.find(path);
    if (it == m_fileOwnership.end()) {
        return ConflictResult::ok("File was not owned");
    }
    if (it->second != agentId) {
        return ConflictResult::error("File owned by different agent");
    }

    m_fileOwnership.erase(it);
    return ConflictResult::ok("File released");
}

std::vector<std::string> SwarmConflictResolver::getAgentFiles(uint64_t agentId) const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    std::vector<std::string> result;
    for (const auto& [file, owner] : m_fileOwnership) {
        if (owner == agentId) result.push_back(file);
    }
    return result;
}

// =============================================================================
// Query
// =============================================================================

ConflictEvent SwarmConflictResolver::getConflict(uint64_t conflictId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& c : m_conflicts) {
        if (c.conflictId == conflictId) return c;
    }
    ConflictEvent empty;
    std::memset(&empty, 0, sizeof(empty));
    return empty;
}

std::vector<ConflictEvent> SwarmConflictResolver::getActiveConflicts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConflictEvent> result;
    for (const auto& c : m_conflicts) {
        if (!c.resolved) result.push_back(c);
    }
    return result;
}

std::vector<ConflictEvent> SwarmConflictResolver::getResolvedConflicts(int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConflictEvent> result;
    for (auto it = m_conflicts.rbegin();
         it != m_conflicts.rend() && static_cast<int>(result.size()) < maxCount; ++it) {
        if (it->resolved) result.push_back(*it);
    }
    return result;
}

int SwarmConflictResolver::getActiveConflictCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& c : m_conflicts) {
        if (!c.resolved) count++;
    }
    return count;
}

// =============================================================================
// Setters & Callbacks
// =============================================================================

void SwarmConflictResolver::setConflictDetectedCallback(ConflictDetectedCallback cb, void* ud) { m_conflictDetectedCb = cb; m_conflictDetectedUD = ud; }
void SwarmConflictResolver::setConflictResolvedCallback(ConflictResolvedCallback cb, void* ud) { m_conflictResolvedCb = cb; m_conflictResolvedUD = ud; }
void SwarmConflictResolver::setConsensusReachedCallback(ConsensusReachedCallback cb, void* ud) { m_consensusCb = cb; m_consensusUD = ud; }
void SwarmConflictResolver::setResourceAllocatedCallback(ResourceAllocatedCallback cb, void* ud) { m_resourceCb = cb; m_resourceUD = ud; }
void SwarmConflictResolver::setMergeNegotiator(MergeNegotiatorFn fn, void* ud) { m_negotiator = fn; m_negotiatorUD = ud; }

uint64_t SwarmConflictResolver::nextConflictId() { return m_nextConflict.fetch_add(1); }
uint64_t SwarmConflictResolver::nextProposalId() { return m_nextProposal.fetch_add(1); }
uint64_t SwarmConflictResolver::nextBidId()      { return m_nextBid.fetch_add(1); }
uint64_t SwarmConflictResolver::nextAllocId()    { return m_nextAlloc.fetch_add(1); }

// =============================================================================
// Statistics
// =============================================================================

SwarmConflictResolver::Stats SwarmConflictResolver::getStats() const {
    return m_stats;
}

std::string SwarmConflictResolver::statsToJson() const {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        R"({"totalConflicts":%llu,"resolvedConflicts":%llu,"escalated":%llu,)"
        R"("mergeSuccesses":%llu,"mergeFails":%llu,)"
        R"("consensusReached":%llu,"consensusFailed":%llu,)"
        R"("resourceBids":%llu,"resourceAllocations":%llu})",
        (unsigned long long)m_stats.totalConflicts,
        (unsigned long long)m_stats.resolvedConflicts,
        (unsigned long long)m_stats.escalatedConflicts,
        (unsigned long long)m_stats.mergeSuccesses,
        (unsigned long long)m_stats.mergeFails,
        (unsigned long long)m_stats.consensusReached,
        (unsigned long long)m_stats.consensusFailed,
        (unsigned long long)m_stats.resourceBids,
        (unsigned long long)m_stats.resourceAllocations
    );
    return std::string(buf);
}

std::string SwarmConflictResolver::conflictsToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string json = "[";
    for (size_t i = 0; i < m_conflicts.size(); i++) {
        if (i > 0) json += ",";
        const auto& c = m_conflicts[i];
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            R"({"id":%llu,"type":%d,"agentA":%llu,"agentB":%llu,)"
            R"("file":"%s","resolved":%s,"strategy":%d})",
            (unsigned long long)c.conflictId,
            static_cast<int>(c.type),
            (unsigned long long)c.agentA,
            (unsigned long long)c.agentB,
            c.targetFile,
            c.resolved ? "true" : "false",
            static_cast<int>(c.usedStrategy)
        );
        json += buf;
    }
    json += "]";
    return json;
}

} // namespace Swarm
} // namespace RawrXD
