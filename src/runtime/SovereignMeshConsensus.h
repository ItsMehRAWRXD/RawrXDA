#pragma once

#include "SovereignMeshBridge.h"
#include "SovereignStabilityLayer.h"
#include <map>
#include <vector>
#include <mutex>

namespace RawrXD::Runtime {

enum class MeshVote {
    ACCEPT = 1,
    REJECT = 0,
    PENDING = 2
};

struct PendingProposal {
    KernelVersion version;
    std::string candidateCode;
    std::map<uint32_t, MeshVote> votes;
    uint32_t acceptCount = 0;
    uint32_t rejectCount = 0;
    uint32_t quorumThreshold; // Typically (MeshNodeCount / 2) + 1
};

class SovereignMeshConsensus {
public:
    static SovereignMeshConsensus& instance();

    bool proposePatch(void* target, const std::string& code);
    
    // Inbound Pulse Handling (NeuralMeshSync)
    void onProposalReceived(uint32_t originId, const KernelVersion& version);
    void onVoteReceived(uint32_t voterId, uint64_t versionId, MeshVote vote);

    // Consensus Check: Triggers Hotpatch_ApplyAtomic if quorum reached
    bool checkFinalization(uint64_t versionId);

private:
    SovereignMeshConsensus() = default;
    
    std::map<uint64_t, PendingProposal> m_proposals;
    std::mutex m_mutex;
};

} // namespace RawrXD::Runtime
