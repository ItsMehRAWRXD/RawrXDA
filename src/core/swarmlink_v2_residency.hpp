#pragma once
#include <stdint.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0

// Core States
enum RX_SHARD_STATE {
    SHARD_UNMAPPED, SHARD_FETCHING, SHARD_STAGED, 
    SHARD_VALIDATING, SHARD_READY, SHARD_PINNED, 
    SHARD_EVICTING, SHARD_STALE, SHARD_FAILED
};

// Shard Manifest Structure
typedef struct RX_V23_SHARD_DESC {
    uint32_t shard_id;
    uint32_t layer_lo;
    uint32_t layer_hi;
    uint32_t tier_pref;
    uint32_t quant_mode;
    uint64_t nvme_offset;
    uint64_t byte_size;
    uint64_t crc64;
    uint32_t dep_group;
    uint32_t flags;
    
    // Runtime Metadata
    RX_SHARD_STATE state;
    uint32_t generation;
} RX_V23_SHARD_DESC;

// Plan Generation Context
typedef struct RX_V23_PLAN {
    uint32_t plan_generation;
    uint32_t model_generation;
    uint32_t shard_count;
    uint32_t active_devices;
    uint32_t flags;
} RX_V23_PLAN;

// Indirection Table for Zero-Patch Live Linking
typedef struct RX_TENSOR_PTR_ENTRY {
    void*    ptr;
    uint32_t shard_id;
    uint32_t generation;
    uint32_t flags;
} RX_TENSOR_PTR_ENTRY;

// P23-A Deterministic L3 API 
extern "C" {
    __declspec(dllexport) BOOL SwarmV23_LoadShardManifest(const char* path);
    __declspec(dllexport) BOOL SwarmV23_InitRingBuffer(uint32_t queueDepth, uint32_t slabMB);
    __declspec(dllexport) BOOL SwarmV23_BuildPlan(uint32_t model_generation);
    __declspec(dllexport) BOOL SwarmV23_PrefetchWindow(uint32_t token_step);
    __declspec(dllexport) BOOL SwarmV23_ValidateShard(uint32_t shard_id);
    __declspec(dllexport) BOOL SwarmV23_PublishReadySet(void);
    __declspec(dllexport) BOOL SwarmV23_EvictColdSet(uint32_t token_step);
}
