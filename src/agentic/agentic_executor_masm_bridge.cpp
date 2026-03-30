// agentic_executor_masm_bridge.cpp
// Bridge implementation: C++ entry points for MASM kernel calls

#include "agentic_executor_masm_bridge.h"
#include "agentic_executor.h"
#include "agentic_executor_state.h"

#include <cstring>
#include <memory>
#include <map>
#include <mutex>

#if defined(__GNUC__) || defined(__clang__)
#define RAWRXD_WEAK __attribute__((weak))
#else
#define RAWRXD_WEAK
#endif

// Global executor instance for MASM callbacks
// Set by AgenticExecutor::initialize() or explicitly via setMasmBridgeExecutor()
static thread_local AgenticExecutor* g_bridgeExecutor = nullptr;
static std::mutex g_bridgeMutex;

namespace {
    // Cache for metrics (MASM reports statistics)
    struct MetricsStore {
        std::map<std::string, double> values;
        std::mutex mutex;
    };
    
    static MetricsStore g_metrics;
}

/**
 * @brief Set the executor instance for MASM callbacks
 * 
 * Must be called before executing MASM kernels that report back.
 * Typically called from AgenticExecutor::initialize().
 */
extern "C" void AgenticExecutor_SetBridgeInstance(AgenticExecutor* executor)
{
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    g_bridgeExecutor = executor;
}

/**
 * @brief MASM reports execution state snapshot
 * 
 * Implementation: Store state snapshot in executor's context for LLM planning.
 */
extern "C" uint32_t AgenticExecutor_InjectExecutionState(const MasmExecutionState* state)
{
    if (!state) {
        return 1; // Error: null pointer
    }
    
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (!g_bridgeExecutor) {
        return 2; // Error: no executor set
    }
    
    // TODO: Capture this state into the executor's ExecutionStateSnapshot
    // This would involve:
    // 1. Converting MasmExecutionState to ExecutionStateSnapshot
    // 2. Storing in m_stateCapture or m_executionContext
    // 3. Making it available to planNextActionTyped()
    
    // Stub for now - just log it
    std::string msg = "MASM State Injected: rip=0x" + std::to_string(state->rip) + 
                      " rsp=0x" + std::to_string(state->rsp);
    
    // Would call: g_bridgeExecutor->logMessage(msg);
    // But logMessage is private - need accessor
    
    return 0; // Success
}

/**
 * @brief MASM reports semantic search results
 * 
 * Implementation: Process top-K matches and add to executor memory.
 */
extern "C" uint32_t AgenticExecutor_ReportSearchResults(const MasmSearchResult* results, uint32_t count)
{
    if (!results || count == 0) {
        return 1;
    }
    
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (!g_bridgeExecutor) {
        return 2;
    }
    
    // TODO: Convert results to JSON and add to executor memory
    // For now, just record metrics
    
    for (uint32_t i = 0; i < count && i < 10; ++i) {
        AgenticExecutor_RecordMetric(("search_result_" + std::to_string(i) + "_score").c_str(),
                                     results[i].score);
    }
    
    return 0; // Success
}

/**
 * @brief MASM requests process execution
 * 
 * Implementation: Wrap executor's runExecutable() for MASM access.
 */
extern "C" uint32_t AgenticExecutor_ExecuteProcess(
    const char* command,
    const char* args,
    char* output_buffer,
    uint32_t output_size)
{
    if (!command || !output_buffer || output_size == 0) {
        return 0xFFFFFFFF; // Error
    }
    
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (!g_bridgeExecutor) {
        return 0xFFFFFFFF;
    }
    
    // TODO: Parse args (newline-separated) and call executor's runExecutable()
    // For now, return error code
    
    return 1; // Placeholder error
}

/**
 * @brief MASM queries cached memory value
 * 
 * Implementation: Return pointer to executor's memory without copy.
 */
extern "C" uint32_t AgenticExecutor_QueryMemory(const char* key, const void** out_value, uint64_t* out_size)
{
    if (!key || !out_value || !out_size) {
        return 1; // Error
    }
    
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (!g_bridgeExecutor) {
        return 1;
    }
    
    // TODO: Look up key in executor's memory map
    // TODO: Return pointer to value and size
    // This is zero-copy - MASM reads directly
    
    return 1; // Not found (placeholder)
}

/**
 * @brief MASM reports performance metric
 * 
 * Implementation: Store in global metrics store for telemetry.
 */
extern "C" uint32_t AgenticExecutor_RecordMetric(const char* metric_name, double value)
{
    if (!metric_name) {
        return 1;
    }
    
    std::lock_guard<std::mutex> lock(g_metrics.mutex);
    g_metrics.values[metric_name] = value;
    
    return 0; // Success
}

/**
 * @brief Retrieve recorded metrics (for telemetry/profiling)
 * 
 * Can be called from C++ to get performance data from MASM kernels.
 */
namespace AgenticExecutor_Metrics {
    double getMetric(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(g_metrics.mutex);
        auto it = g_metrics.values.find(name);
        return (it != g_metrics.values.end()) ? it->second : 0.0;
    }
    
    std::map<std::string, double> getAllMetrics()
    {
        std::lock_guard<std::mutex> lock(g_metrics.mutex);
        return g_metrics.values;
    }
}

// ========== STUB IMPLEMENTATIONS (will be replaced by real MASM) ==========

/**
 * @brief Stub: AVX-512 quantized dot product
 * 
 * This is a C++ fallback. Real implementation is in MASM.
 * If no .obj is linked, this stub runs.
 */
extern "C" RAWRXD_WEAK
float RawrXD_Q8_Dot256(const int8_t* vec, const int8_t* query, float scale)
{
    // Stub: Simple C++ implementation for testing
    if (!vec || !query) return 0.0f;
    
    int32_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += (int32_t)vec[i] * query[i];
    }
    
    return (float)sum * scale;
}

/**
 * @brief Stub: Batch top-K selection
 * 
 * Stub implementation using partial sort.
 */
extern "C" RAWRXD_WEAK
void RawrXD_TopK_Select(
    float* scores, uint32_t* indices,
    size_t count, uint32_t k,
    float* out_scores, uint32_t* out_indices)
{
    if (!scores || !indices || !out_scores || !out_indices || count == 0 || k == 0) {
        return;
    }
    
    // Stub: Copy first k elements
    // Real implementation would use heap or partial_sort
    for (uint32_t i = 0; i < k && i < count; ++i) {
        out_scores[i] = scores[i];
        out_indices[i] = indices[i];
    }
}

/**
 * @brief Stub: Stream batch scoring
 */
extern "C" RAWRXD_WEAK
void RawrXD_Stream_Score_Batch(
    const int8_t* vec_base,
    const int8_t* query,
    size_t vec_stride,
    size_t count,
    float* out_scores,
    float scale)
{
    if (!vec_base || !query || !out_scores || count == 0) {
        return;
    }
    
    // Stub: Score each vector
    for (size_t i = 0; i < count; ++i) {
        const int8_t* vec = vec_base + (i * vec_stride);
        out_scores[i] = RawrXD_Q8_Dot256(vec, query, scale);
    }
}

/**
 * @brief Stub: Capture register state
 */
extern "C" RAWRXD_WEAK
uint32_t RawrXD_CaptureRegisterState(MasmExecutionState* out_state)
{
    if (!out_state) {
        return 1;
    }
    
    // Stub: Zero out (on real MASM, would capture actual registers)
    out_state->rax = 0;
    out_state->rbx = 0;
    out_state->rcx = 0;
    out_state->rdx = 0;
    out_state->rsi = 0;
    out_state->rdi = 0;
    out_state->rbp = 0;
    out_state->rsp = (uint64_t)&out_state; // Approximate
    out_state->rip = (uint64_t)RawrXD_CaptureRegisterState;
    
    return 0;
}

/**
 * @brief Stub: Unwind stack frames
 */
extern "C" RAWRXD_WEAK
uint32_t RawrXD_UnwindStackFrames(MasmExecutionState* out_state, uint32_t max_depth)
{
    if (!out_state) {
        return 0;
    }
    
    // Stub: Return 0 frames
    return 0;
}

/**
 * @brief Stub: Fast process launch
 */
extern "C" RAWRXD_WEAK
uint32_t RawrXD_LaunchAndWait(
    const char* exe,
    const char* args,
    uint32_t timeout_ms,
    uint32_t* out_exit_code)
{
    if (!exe || !out_exit_code) {
        return 1;
    }
    
    // Stub: Not implemented
    *out_exit_code = 1;
    return 1;
}

// ========== NEW AVX-512 KERNEL WEAK STUBS (Layer 3: RawrXD_AVX512_Semantic.asm) ==========

#if !defined(RAWRXD_HAS_AVX512_KERNELS) || (RAWRXD_HAS_AVX512_KERNELS == 0)

/**
 * @brief Weak stub: RawrXD_Q8_CosineBatch
 * C++ fallback for batch cosine similarity (Q8 quantized vectors)
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_Q8_CosineBatch)
 */
extern "C" RAWRXD_WEAK
void RawrXD_Q8_CosineBatch(
    const int8_t* vectors,
    const int8_t* query,
    size_t vecCount,
    size_t vecDim,
    float* outScores,
    float scale)
{
    if (!vectors || !query || !outScores || vecCount == 0 || vecDim == 0) {
        return;
    }
    
    // C++ fallback: scalar dot product per vector
    for (size_t i = 0; i < vecCount; ++i) {
        int32_t sum = 0;
        for (size_t j = 0; j < vecDim; ++j) {
            sum += (int32_t)vectors[i * vecDim + j] * (int32_t)query[j];
        }
        outScores[i] = (float)sum * scale;
    }
}

/**
 * @brief Weak stub: RawrXD_TopK_Heapify
 * C++ fallback for top-K selection via min-heap
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_TopK_Heapify)
 */
extern "C" RAWRXD_WEAK
void RawrXD_TopK_Heapify(
    float* scores,
    uint32_t* indices,
    size_t count,
    uint32_t k,
    float* outScores,
    uint32_t* outIndices)
{
    if (!scores || !indices || !outScores || !outIndices || count == 0 || k == 0) {
        return;
    }
    
    // C++ fallback: simple linear scan + partial sort
    uint32_t actualK = (k < count) ? k : count;
    
    for (uint32_t i = 0; i < actualK; ++i) {
        outScores[i] = scores[i];
        outIndices[i] = indices[i];
    }
    
    // Sort if needed (simplified: just copy)
    // Real heap implementation would maintain heap invariant during insertions
}

/**
 * @brief Weak stub: RawrXD_CaptureExecutionState
 * C++ fallback for register/execution state capture
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_CaptureExecutionState)
 */
extern "C" RAWRXD_WEAK
void RawrXD_CaptureExecutionState(void* snapPtr)
{
    // Stub: Do nothing (snapshot remains as initialized by caller)
    // Real MASM would capture all GPRs, stack pointer, and return address
}

/**
 * @brief Weak stub: RawrXD_StreamCompareNT
 * C++ fallback for non-temporal memory compare (verification)
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_StreamCompareNT)
 */
extern "C" RAWRXD_WEAK
bool RawrXD_StreamCompareNT(
    const void* src1,
    const void* src2,
    size_t length)
{
    if (length == 0) return true;
    if (!src1 || !src2) return false;
    
    // C++ fallback: byte-by-byte comparison
    return std::memcmp(src1, src2, length) == 0;
}

/**
 * @brief Weak stub: RawrXD_PrefetchIndexPages
 * C++ fallback for software prefetch (no-op in fallback)
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_PrefetchIndexPages)
 */
extern "C" RAWRXD_WEAK
void RawrXD_PrefetchIndexPages(
    const void* base,
    size_t pageCount,
    size_t stride)
{
    // Stub: No-op in C++ fallback
    // Real MASM issues prefetchnta instructions for cache optimization
}

/**
 * @brief Weak stub: RawrXD_MemcpyNT
 * C++ fallback for non-temporal copy 
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_MemcpyNT)
 */
extern "C" RAWRXD_WEAK
void RawrXD_MemcpyNT(
    void* dst,
    const void* src,
    size_t size)
{
    if (size == 0 || !dst || !src) return;
    
    // C++ fallback: standard memcpy
    std::memcpy(dst, src, size);
}

/**
 * @brief Weak stub: RawrXD_F32_DotBatch
 * C++ fallback for F32·F32 FMA dot product batch
 * Real implementation: src/kernels/RawrXD_AVX512_Semantic.asm (RawrXD_F32_DotBatch) [future]
 */
extern "C" RAWRXD_WEAK
void RawrXD_F32_DotBatch(
    const float* vectors,
    const float* query,
    size_t vecCount,
    size_t vecDim,
    float* outScores,
    float scale)
{
    if (!vectors || !query || !outScores || vecCount == 0 || vecDim == 0) {
        return;
    }
    
    // C++ fallback: scalar dot product with FMA simulation
    for (size_t i = 0; i < vecCount; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < vecDim; ++j) {
            sum += vectors[i * vecDim + j] * query[j];  // FMA in hardware
        }
        outScores[i] = sum * scale;
    }
}

#endif // !RAWRXD_HAS_AVX512_KERNELS

/**
 * @brief Callback: AgenticExecutor_CaptureRegisters
 * Called by MASM RawrXD_CaptureExecutionState to finalize snapshot
 * Implemented by executor to add timestamp, symbol resolution, etc.
 */
extern "C" RAWRXD_WEAK
void AgenticExecutor_CaptureRegisters(void* snapPtr)
{
    // Stub: Executor can override to add metadata
    // (Weak so real executor can provide full implementation)
}

