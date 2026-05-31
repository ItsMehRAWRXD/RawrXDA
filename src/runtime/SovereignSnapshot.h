#pragma once
#include <vector>
#include <string>

namespace RawrXD::Runtime {

class SovereignSnapshot {
public:
    static SovereignSnapshot& instance();
    
    // 🧬 Snapshot verification (Cold-Start Layer)
    bool verifyGenesisRoot(uint64_t rootHash);
    
    // Snapshot distribution (Batch 32)
    void getLatestMeshSnapshot(std::vector<uint8_t>& data);

        // Phase 50: Binary slot persistence (slots 0..7)
        bool captureSnapshot(uint32_t slot);
        bool restoreSnapshot(uint32_t slot);
    
private:
    SovereignSnapshot() {}
};

} // namespace RawrXD::Runtime
