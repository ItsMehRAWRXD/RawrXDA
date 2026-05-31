#pragma once

#include "SovereignMeshBridge.h"
#include <vector>
#include <string>
#include <mutex>
#include <deque>

namespace RawrXD::Runtime {

struct ReplayFrame {
    uint32_t functionId;
    uint32_t versionId;
    std::vector<uint8_t> input;
    std::vector<uint8_t> output;
    uint64_t timestamp;
};

class SovereignDeterministicReplay {
public:
    static SovereignDeterministicReplay& instance();

    bool recordFrame(uint32_t fid, uint32_t vid, void* in, size_t inSize, void* out, size_t outSize);
    
    // Validates a candidate kernel against recorded historical outputs
    bool validateKernelExecution(void* kernelAddr, uint32_t fid);

private:
    SovereignDeterministicReplay() = default;
    
    std::deque<ReplayFrame> m_history;
    std::mutex m_mutex;
    const uint32_t MAX_REPLAY_FRAMES = 1000;
};

} // namespace RawrXD::Runtime
