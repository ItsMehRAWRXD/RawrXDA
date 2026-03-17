#pragma once
#include "swarmlink_v2_residency.hpp"
#include <stdint.h>

extern "C" {
    // P23-B API
    __declspec(dllexport) void SwarmV23_StartRollingPrefetch(uint32_t active_token_step);
    __declspec(dllexport) void SwarmV23_StopRollingPrefetch(void);
    
    // The 7 Enhancements implemented in P23-B pipeline context
    __declspec(dllexport) BOOL Enh1_DeterministicFallback(uint32_t shard_id);
    __declspec(dllexport) void* Enh2_AllocateVolatileBSS_AST(uint64_t size);
    __declspec(dllexport) BOOL Enh3_RecursiveRetryFetch(uint32_t shard_id, int maxRetries);
    __declspec(dllexport) void Enh4_ExecuteParallelWorkers(int thread_count);
    __declspec(dllexport) void Enh5_BinaryHexPatchPipeline(uint8_t* offset, uint8_t* patch, uint32_t len);
    __declspec(dllexport) BOOL Enh6_EnforceLexicalHandshake(uint32_t model_gen);
    __declspec(dllexport) void Enh7_HushTerminalOutput(BOOL active);
}
