// ================================================================
// RawrXD-ExecAI Runtime Header
// C interface for streaming inference engine
// ================================================================
#ifndef EXECAI_RUNTIME_COMPLETE_H
#define EXECAI_RUNTIME_COMPLETE_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================
// Core Runtime Functions
// ================================================================

/**
 * Initialize ExecAI runtime with executable model
 * @param model_path Path to .exec file containing structural definition
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeExecAI(const char* model_path);

/**
 * Shutdown ExecAI runtime and release resources
 */
void ShutdownExecAI(void);

/**
 * Run streaming inference on token file
 * @param token_path Path to binary token file
 * @return TRUE on success, FALSE on failure
 */
BOOL RunStreamingInference(const char* token_path);

/**
 * Evaluate single token synchronously
 * @param token Input token ID
 * @return Output value (first state element)
 */
float EvaluateSingleToken(uint32_t token);

/**
 * Export current inference state
 * @param output Buffer to receive state (must be state_dim * sizeof(float))
 * @param buffer_size Size of output buffer in bytes
 * @return TRUE on success, FALSE on failure
 */
BOOL GetInferenceState(float* output, uint32_t buffer_size);

// ================================================================
// Runtime Statistics
// ================================================================
typedef struct {
    uint64_t total_tokens_processed;
    uint32_t buffer_occupancy;
    uint32_t operator_count;
    uint32_t state_dim;
} RuntimeStats;

/**
 * Get runtime performance statistics
 * @param stats Pointer to RuntimeStats structure to fill
 * @return TRUE on success, FALSE on failure
 */
BOOL GetRuntimeStatistics(RuntimeStats* stats);

// ================================================================
// Global State (for testing and debugging)
// ================================================================
extern float g_StateVector[4096];
extern uint32_t g_TokenBuffer[];
extern volatile LONG g_BufferHead;
extern volatile LONG g_BufferTail;

#ifdef __cplusplus
}
#endif

#endif // EXECAI_RUNTIME_COMPLETE_H
