#pragma once
// =============================================================================
// rawrxd_asm_orchestration.h
// C/C++ interface to the three pure-MASM sovereign agentic modules.
//
// Only included when RAWRXD_MASM_CORE_NATIVE_BRIDGE is defined (MSVC + ml64
// build).  All symbols are exported from the corresponding .asm translation
// units with C linkage (no name mangling).
// =============================================================================

#if defined(RAWRXD_MASM_CORE_NATIVE_BRIDGE) && defined(_MSC_VER) && defined(_M_X64)

#include <cstdint>

extern "C" {

// ----------------------------------------------------------------------------
// RawrXD_TerminalCapture.asm
//
// PipeCapture_Run – spawn a child process with redirected stdout+stderr,
//   read all output into pOutBuf, then wait for the process to exit.
//
//   pCmdLine     – ANSI command string (CreateProcessA may modify it in place;
//                  caller must not pass a string literal / const char*)
//   pOutBuf      – caller-supplied buffer that receives captured output
//   dwBufSize    – byte capacity of pOutBuf
//   pdwBytesRead – receives total bytes written to pOutBuf (may be NULL)
//   Returns 0 on success, Win32 error code on failure.
// ----------------------------------------------------------------------------
DWORD PipeCapture_Run(char* pCmdLine, unsigned char* pOutBuf,
                      DWORD dwBufSize, DWORD* pdwBytesRead);

// ----------------------------------------------------------------------------
// RawrXD_ContextBuffer.asm
//
// Zero-heap 8 KB ring buffer.  Accumulates "Step: / Result:" pairs so that
// the next Think prompt carries full execution history without hitting the heap.
// ----------------------------------------------------------------------------

/// Clear the buffer (length = 0, buf[0] = '\0').
void ContextBuf_Reset();

/// Append "Step: <pToolName>\r\n" to the buffer.
void ContextBuf_AppendStep(const char* pToolName);

/// Append "Result: <pResult>\r\n\r\n" to the buffer.
void ContextBuf_AppendResult(const char* pResult);

/// Return pointer to the null-terminated context string.
const char* ContextBuf_Get();

/// Return the current written length (bytes, excluding null terminator).
unsigned int ContextBuf_GetLen();

/// Assemble a full LLM prompt into pOutBuf:
///   <pSystemPrompt>
///   Goal: <pGoal>
///   \r\n\r\nPrevious execution context:\r\n
///   <accumulated ctx>
/// pSystemPrompt may be NULL.
void ContextBuf_BuildPrompt(const char* pGoal, const char* pSystemPrompt,
                            char* pOutBuf, DWORD dwOutSize);

// ----------------------------------------------------------------------------
// RawrXD_JsonPlanParser.asm
//
// PlanStep memory layout (SIZEOF_PLAN_STEP = 832 bytes):
//   +  0 ..  63  tool[64]    – null-terminated tool name
//   + 64 .. 319  param[256]  – null-terminated parameter string
//   +320 .. 831  result[512] – filled by orchestrator after execution
// ----------------------------------------------------------------------------

#pragma pack(push, 1)
struct RawrXD_PlanStep {
    char  tool  [64];
    char  param [256];
    char  result[512];
};
#pragma pack(pop)

static_assert(sizeof(RawrXD_PlanStep) == 832,
              "RawrXD_PlanStep size mismatch with MASM SIZEOF_PLAN_STEP");

/// Extract a single JSON string value: finds "key":"<value>" in pJson.
/// Returns 1 (TRUE) if found, 0 if not found or buffer too small.
int JsonPlan_ExtractValue(const char* pJson, const char* pKey,
                          char* pOutVal, DWORD dwValSize);

/// Parse a JSON array of plan objects into pPlanSteps.
/// Returns 1 if at least one step was parsed, 0 otherwise.
int JsonPlan_Parse(const char* pJson, RawrXD_PlanStep* pPlanSteps,
                   DWORD dwMaxSteps, DWORD* pdwStepCount);

// ----------------------------------------------------------------------------
// inference_core.asm – GEMM / GEMV Inference Kernels (AVX2/FMA3 + AVX-512)
// ----------------------------------------------------------------------------

struct RawrXD_GemmParams {
    float* A;           // +0: row-major, M×K
    float* B;           // +8: row-major, K×N
    float* C;           // +16: row-major, M×N
    int32_t M;          // +24
    int32_t N;          // +28
    int32_t K;          // +32
    float alpha;        // +36
    float beta;         // +40
    int32_t lda;        // +44
    int32_t ldb;        // +48
    int32_t ldc;        // +52
};

struct RawrXD_GemvParams {
    float* A;           // +0: row-major, M×N
    float* x;           // +8: vector, length N
    float* y;           // +16: vector, length M, output
    int32_t M;          // +24
    int32_t N;          // +28
    float alpha;        // +32
    float beta;         // +36
    int32_t lda;        // +40
};

/**
 * Detect CPU features and set internal dispatch pointers.
 * @return capability bitmask (Bit 0: AVX2, Bit 1: FMA3, Bit 2: AVX-512F)
 */
uint32_t InferenceCore_Init();

/**
 * Returns cached capability bitmask.
 */
uint32_t InferenceCore_GetCapabilities();

/**
 * Dispatched SGEMM (C = alpha*A*B + beta*C).
 * Uses best available path (AVX-512 > AVX2/FMA3).
 */
int32_t InferenceCore_SGEMM(const RawrXD_GemmParams* params);

/**
 * Dispatched SGEMV (y = alpha*A*x + beta*y).
 */
int32_t InferenceCore_SGEMV(const RawrXD_GemvParams* params);

/**
 * Reads performance counters from MASM globals.
 * @param stats Array of 3 uint64_t [GEMM_Calls, GEMV_Calls, FLOPs]
 */
int32_t InferenceCore_GetStats(uint64_t* stats);

// ----------------------------------------------------------------------------
// RawrXD_AVX512_Dequant_BPE.asm – Vectorized BPE Tokenizer
// ----------------------------------------------------------------------------

/**
 * Vectorized BPE Tokenizer Entry Point.
 * @param text UTF-8 input string
 * @param vocab_ptr Pointer to vocabulary hash table (internal format)
 * @param output_tokens Pointer to pre-allocated buffer for token IDs
 * @param max_tokens Size of output buffer
 * @return Number of tokens generated
 */
int32_t RawrXD_MASM_BPETokenize(const char* text, void* vocab_ptr, int32_t* output_tokens, int32_t max_tokens);

// ----------------------------------------------------------------------------
// RAWRXD_SAMPLER_AVX512.asm – SoftMax / Top-K / Top-P Sampling Kernels
// ----------------------------------------------------------------------------

/**
 * Multiplies all logits by 1.0/temperature using 512-bit vectors.
 */
void Sampler_ApplyTemperature_AVX512(float* pLogits, int n, float invTemp);

/**
 * Finds the maximum logit value for SoftMax normalization.
 */
float Sampler_FindMax_AVX512(const float* pLogits, int n);

/**
 * Computes sum(exp(logits - maxVal)) using AVX-512 polynomial expansion.
 */
float Sampler_ExpSum_AVX512(const float* pLogits, int n, float maxVal);

/**
 * Computes fused SoftMax and Top-K extraction using a bitonic merge network.
 * @param pLogits    Input/Output: Logits to sort/normalize (modified if in-place)
 * @param pIndices   Output: Top-K token indices
 * @param n          Vocabulary size
 * @param K          Number of elements to extract
 */
void Sampler_SoftMax_TopK_Fused(float* pLogits, uint32_t* pIndices, int n, int K);

} // extern "C"

#endif // RAWRXD_MASM_CORE_NATIVE_BRIDGE && _MSC_VER && _M_X64
