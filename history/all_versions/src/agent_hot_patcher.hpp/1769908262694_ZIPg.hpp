#pragma once
#include <string>
#include <vector>
#include <memory>
#include "language_server_integration.hpp" // For Range?

struct HotPatchCandidate {
    std::string uri;
    Range range;
    std::string replacement;
    float confidence;
    bool autoApply;
};

class AgentHotPatcher {
public:
    static AgentHotPatcher* instance() {
        static AgentHotPatcher inst;
        return &inst;
    }

    void stagePatch(const HotPatchCandidate& candidate) {
        // Explicit logic: Store patch
        m_stagedPatches.push_back(candidate);
    }
    
    std::vector<HotPatchCandidate> getStagedPatches() const {
        return m_stagedPatches;
    }
    
private:
    std::vector<HotPatchCandidate> m_stagedPatches;
};
