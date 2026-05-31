// ============================================================================
// HardwareScout.h — Hardware Interrogation for Dynamic Tiering
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Core {

enum class ExecutionTier {
    VRAM_ACCELERATED, // Discrete GPU + High VRAM (120+ TPS)
    CPU_AVX512,       // Modern CPU with AVX-512 (20-40 TPS)
    CPU_AVX2,         // Standard Modern CPU (10-20 TPS)
    CPU_GENERIC       // Legacy/Compatibility (5-10 TPS)
};

struct HardwareProfile {
    ExecutionTier tier;
    std::string gpuName;
    uint64_t vramBytes;
    bool hasAVX512;
    bool hasAVX2;
    bool hasVulkan;
};

class HardwareScout {
public:
    static HardwareProfile GetCurrentProfile();
    static const char* TierToString(ExecutionTier tier);
};

} // namespace Core
} // namespace RawrXD
