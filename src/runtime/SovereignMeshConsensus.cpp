#include "SovereignMeshConsensus.h"
#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>
#include "SovereignSandbox.h"

namespace RawrXD::Runtime {

static std::mutex g_consensusMutex;

SovereignMeshConsensus& SovereignMeshConsensus::instance() {
    static SovereignMeshConsensus instance;
    return instance;
}

SovereignMeshConsensus::SovereignMeshConsensus() {}

SovereignMeshConsensus::~SovereignMeshConsensus() {}

bool SovereignMeshConsensus::proposePatch(void* target, const std::string& code) {
    std::lock_guard<std::mutex> lock(g_consensusMutex);
    
    // Batch 12+ Logic: Quorum requirement increases with mesh size
    uint64_t vid = (uint64_t)target;
    
    MeshProposal prop;
    prop.versionId = vid;
    prop.acceptCount = 1; // Self vote
    prop.quorumThreshold = 3; // Initial mock threshold
    
    // 🛡️ Pre-Trust Local Validation
    if (!SovereignSandbox::instance().validateKernel(target, (void*)code.c_str(), code.length())) {
        std::cerr << "[Consensus] LOCAL_REJECT: Sandbox validation failed for Proposal v" << vid << std::endl;
        return false;
    }

    m_activeProposals[vid] = prop;
    std::cout << "[Consensus] PROPOSED: Kernel v" << vid << " (Passed Sandbox, Waiting for Quorum)" << std::endl;
    
    return true;
}

void SovereignMeshConsensus::onVoteReceived(uint32_t voterId, uint64_t versionId, MeshVote vote) {
    std::lock_guard<std::mutex> lock(g_consensusMutex);
    
    auto it = m_activeProposals.find(versionId);
    if (it == m_activeProposals.end()) return;

    if (vote == MeshVote::ACCEPT) {
        it->second.acceptCount++;
    }

    std::cout << "[Consensus] VOTE: Node " << voterId << " -> v" << versionId << " (" << (vote == MeshVote::ACCEPT ? "YES" : "NO") << ")" << std::endl;
    
    if (checkFinalization(versionId)) {
        m_activeProposals.erase(it);
    }
}

bool SovereignMeshConsensus::checkFinalization(uint64_t versionId) {
    auto& prop = m_activeProposals[versionId];
    
    if (prop.acceptCount >= prop.quorumThreshold) {
        std::cout << "[Consensus] QUORUM REACHED: Committing Version " << versionId << " to Mesh." << std::endl;
        return true;
    }
    return false;
}

} // namespace RawrXD::Runtime
