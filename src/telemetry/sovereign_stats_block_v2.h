#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// SovereignStatsBlock V2 - Extended for Speculative Decoding & Memory Oracle
// Aligned to 64 bytes (cache line) to prevent false sharing.
typedef struct __declspec(align(64)) SovereignStatsBlockV2 {
    // --- Header (16 bytes) ---
    uint32_t magic;           // 0x00564F53 "SOV\0"
    uint16_t version;         // 2
    uint16_t headerBytes;     // 64
    uint32_t totalBytes;      // Total shmem size
    uint32_t sequenceId;      // Monotonic update counter

    // --- Throughput & Latency (16 bytes) ---
    float tokensPerSec;       // Wall-clock tokens/sec
    float msPerToken;         // Latency (P50)
    uint32_t draftAccepted;   // Cumulative accepted tokens
    uint32_t draftRejected;   // Cumulative rejected tokens

    // --- Memory Oracle State (16 bytes) ---
    float memoryPressure;     // 0.0 - 1.0 (max(vram, ram))
    float weightRetain;       // Current RETAIN strategy weight
    float weightCompress;     // Current COMPRESS strategy weight
    float weightTierDown;     // Current TIERDOWN strategy weight

    // --- Pipeline & Backend (16 bytes) ---
    uint32_t vulkanQueueDepth; // Pending GPU commands
    uint32_t activeThreads;    // Worker thread count
    uint16_t cpuTemp;          // CPU Package Temp (deg C)
    uint16_t gpuTemp;          // GPU Package Temp (deg C)
    uint32_t reserved;         
} SovereignStatsBlockV2;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static_assert(sizeof(SovereignStatsBlockV2) == 64, "SovereignStatsBlockV2 must be exactly 64 bytes");
#endif
