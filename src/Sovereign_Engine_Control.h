#pragma once

// Sovereign_Engine_Control.h - KV-Cache Rollback for Token Steering

#include <cstdint>
#include <limits>

// Forward declarations for llama.cpp structures (avoid including llama.h in headers)
struct llama_context;
struct llama_token_data;
struct llama_token_data_array;
typedef struct llama_memory_i * llama_memory_t;
typedef int32_t llama_pos;
typedef int32_t llama_seq_id;

namespace RawrXD {
namespace Sovereign {

// ============================================================================
// KV-Cache State-Space Rollback
// ============================================================================

// Perform a deep rollback of the inference state when an unauthorized token
// is detected. This removes the forbidden token from the KV-cache and forces
// the model to re-sample from a clean state.
//
// Parameters:
//   ctx          - The llama_context containing the inference state
//   candidates   - The token candidate array to modify
//   forbidden_id - The specific token ID to suppress
//
// Returns:
//   true if rollback succeeded, false if rollback failed
//
bool ForceEmergencySteer(
    llama_context* ctx,
    llama_token_data_array* candidates,
    int32_t forbidden_id
);

// Check if the KV-cache supports rollback operations
bool CanRollbackKVCache(llama_context* ctx);

// Get the current token count in the KV-cache for a specific sequence
int32_t GetKVCacheTokenCount(llama_context* ctx, llama_seq_id seq_id = 0);

// ============================================================================
// Low-Level Cache Manipulation (No-Dep MASM variants)
// ============================================================================

// These are the extern "C" MASM kernel exports if we need ultra-low-latency
extern "C" {
    // Direct pointer-based rollback (bypasses llama.cpp API overhead)
    // rcx = llama_context*, rdx = rollback count
    // Returns: new token count
    int32_t Heretic_KV_Rollback_NoDep(void* ctx_ptr, int32_t rollback_count);
}

} // namespace Sovereign
} // namespace RawrXD
