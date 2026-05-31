#include "SovereignHotpatchBridge.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace RawrXD::Runtime {

enum PatchTrust {
    LOCAL_TEST = 0,      // Unverified local mutation
    VERIFIED = 1,        // Passed local sandbox tests
    MESH_PROPAGATED = 2  // Approved by mesh consensus quorum
};

struct KernelVersion {
    uint64_t version_id;
    uint32_t checksum;
    uint64_t timestamp;
    PatchTrust trust;
};

class SovereignStabilityLayer {
public:
    static SovereignStabilityLayer& instance();

    bool registerKernelVersion(void* addr, uint64_t version, uint32_t crc);
    bool validatePatchTrust(void* addr, PatchTrust required);
    
    // Prevents oscillation/thrashing
    bool checkCooldown(void* addr);
    void updateMutationTimestamp(void* addr);

private:
    SovereignStabilityLayer() = default;
    
    struct FunctionMetadata {
        KernelVersion version;
        uint64_t lastMutationTime;
        uint32_t mutationCount;
    };

    std::map<void*, FunctionMetadata> m_registry;
    const uint64_t COOLDOWN_MS = 30000; // 30s stability window
};

} // namespace RawrXD::Runtime
