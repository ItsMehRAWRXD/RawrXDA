#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#define SOVEREIGN_ALIGN64 __declspec(align(64))
#elif defined(__GNUC__)
#define SOVEREIGN_ALIGN64 __attribute__((aligned(64)))
#else
#define SOVEREIGN_ALIGN64
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum : uint32_t {
    SOVEREIGN_STATS_MAGIC = 0x00564F53u // "SOV\0" little-endian
};

typedef struct SOVEREIGN_ALIGN64 SovereignStatsBlock {
    uint32_t magic;        // SOVEREIGN_STATS_MAGIC
    uint16_t version;      // Layout version
    uint16_t headerBytes;  // Size of this header
    uint32_t totalBytes;   // Total shared memory size

    float tokensPerSec;    // Live tokens/sec
    uint32_t skipRate;     // TurboSkip percentage
    uint32_t gpuSplit;     // GPU/CPU split ratio
    uint32_t driveTemps[4];// Drive temperatures (C)

    float latencyMs;       // Average token latency (ms)
    uint32_t swapCount;    // Page/zone swap count
    uint64_t energyUsed;   // Energy used (uJ or vendor-defined)

    uint8_t reserved[8];   // Future expansion (keeps size 64B-aligned)
} SovereignStatsBlock;

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(SovereignStatsBlock) % 64 == 0, "SovereignStatsBlock must be 64-byte aligned");
#endif
