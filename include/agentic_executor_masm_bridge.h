#pragma once

// D:\copilot-instructions.md specifies MASM x64 assembler path:
// C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe

// AgenticExecutor MASM Bridge
// These function signatures are exposed to MASM kernels for:
// 1. Vector operations (AVX-512 dot products, top-k)
// 2. Execution state capture (stack, registers, RIP)
// 3. Async process monitoring and callbacks

#include <cstdint>
#include <cstddef>
#include <string>

/**
 * @brief Execution state structure that MASM kernels can populate
 * 
 * MASM fills this in with:
 * - rsp, rip, rax-r15 register state
 * - Stack frame pointers
 * - Current instruction context
 */
struct MasmExecutionState
{
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rip;
    uint64_t flags;
    
    // Stack frame chain (for unwinding)
    uint64_t frame_base;
    uint64_t frame_size;
    
    // Thread/process context
    uint64_t thread_id;
    uint64_t process_id;
    
    // Timestamp when captured
    uint64_t timestamp_ms;
};

/**
 * @brief Results from MASM semantic search kernel
 * 
 * Filled by RawrXD_Q8_Dot256 and related kernels
 */
struct MasmSearchResult
{
    float score;              // Similarity score
    uint32_t candidate_index; // Which candidate in batch
    uint32_t file_id;         // Which file in index
    uint32_t line_number;     // Line in file
    uint64_t match_offset;    // Byte offset in mmap'd data
};

/**
 * @name MASM Kernel Exports (called FROM MASM, implemented in C++)
 * These allow MASM to feed data back to the executor.
 * @{
 */

extern "C" {
    void AgenticExecutor_SetBridgeInstance(class AgenticExecutor* executor);

    /**
     * @brief MASM kernel reports execution snapshot back to executor
     * 
     * Called by MASM kernels when desired to inject runtime context.
     * Executor stores this for LLM planning.
     * 
     * @param state Filled-in register + stack state from MASM
     * @return 0 on success, non-zero on error
     */
    uint32_t AgenticExecutor_InjectExecutionState(const MasmExecutionState* state);
    
    /**
     * @brief MASM kernel reports search results
     * 
     * Called by semantic search kernels (RawrXD_Q8_Dot256, etc.)
     * after computing similarity scores.
     * 
     * @param results Array of results
     * @param count Number of results
     * @return 0 on success
     */
    uint32_t AgenticExecutor_ReportSearchResults(const MasmSearchResult* results, uint32_t count);
    
    /**
     * @brief MASM kernel requests process execution
     * 
     * High-performance alternative to C++ executeStep for time-critical operations.
     * MASM can request compilation, tool execution, etc. directly.
     * 
     * @param command Executable path or command string
     * @param args Newline-separated argument list
     * @param output_buffer Caller-allocated buffer for stdout
     * @param output_size Size of buffer
     * @return Exit code, or 0xFFFFFFFF on error
     */
    uint32_t AgenticExecutor_ExecuteProcess(
        const char* command,
        const char* args,
        char* output_buffer,
        uint32_t output_size
    );
    
    /**
     * @brief MASM kernel queries memory value
     * 
     * Allows MASM to read cached values (model weights, quantized vectors, etc.)
     * from executor memory without copies.
     * 
     * @param key Null-terminated key string
     * @param out_value Pointer to store value pointer (not a copy)
     * @param out_size Size of value
     * @return 0 on found, 1 on not found
     */
    uint32_t AgenticExecutor_QueryMemory(const char* key, const void** out_value, uint64_t* out_size);
    
    /**
     * @brief MASM kernel stores execution metric
     * 
     * For performance telemetry (latency, throughput, cache misses, etc.)
     * 
     * @param metric_name Stats key (e.g., "avx512_dot_latency_us")
     * @param value Metric value
     * @return 0 on success
     */
    uint32_t AgenticExecutor_RecordMetric(const char* metric_name, double value);
}

/**
 * @name MASM Kernel Imports (implemented in ASM, called FROM C++)
 * These are the high-performance MASM functions that replace C++ code.
 * @{
 */

extern "C" {
    
    // ========== SEMANTIC SEARCH (VECTOR 3) ==========
    
    /**
     * @brief AVX-512 quantized dot product: 256 dimensions
     * 
     * Computes dot(vec, query) for 8-bit quantized embeddings.
     * Uses AVX-512 with non-temporal stores for cache efficiency.
     * 
     * @param vec 64-byte-aligned int8[256]
     * @param query 64-byte-aligned int8[256]
     * @param scale Quantization scale factor
     * @return Similarity score as float
     * 
     * Assemble with: ml64 /c /Zi /FoRawrXD_AVX512_Semantic.obj RawrXD_AVX512_Semantic.asm
     */
    float RawrXD_Q8_Dot256(const int8_t* vec, const int8_t* query, float scale);
    
    /**
     * @brief Batch top-K selection using partial sort
     * 
     * Given N candidates with scores, extract top K without full sort.
     * Uses min-heap for k <= 256.
     * 
     * @param scores Input score array (N elements)
     * @param indices Input index array (N elements)
     * @param count Number of candidates (N)
     * @param k Desired top-K (k <= count)
     * @param out_scores Output buffer (k elements)
     * @param out_indices Output buffer (k elements)
     * 
     * Assemble with: ml64 /c /Zi /FoRawrXD_AVX512_Semantic.obj RawrXD_AVX512_Semantic.asm
     */
    void RawrXD_TopK_Select(
        float* scores, uint32_t* indices, 
        size_t count, uint32_t k,
        float* out_scores, uint32_t* out_indices
    );
    
    /**
     * @brief Stream batch scoring with non-temporal writes
     * 
     * Score N vectors against a query using movntss (bypass cache).
     * Useful for large batches where latency < throughput.
     * 
     * @param vec_base Base pointer to vector data
     * @param query Quantized query vector (int8[256])
     * @param vec_stride Bytes between consecutive vectors
     * @param count Number of vectors to score
     * @param out_scores Output scores (count elements)
     * @param scale Quantization scale
     */
    void RawrXD_Stream_Score_Batch(
        const int8_t* vec_base,
        const int8_t* query,
        size_t vec_stride,
        size_t count,
        float* out_scores,
        float scale
    );
    
    // ========== EXECUTION STATE CAPTURE ==========
    
    /**
     * @brief Capture current register state into MasmExecutionState
     * 
     * Fills in rax-r15, rsp, rip, rflags, etc.
     * Called by C++ captureExecutionState() to inject runtime context.
     * 
     * @param out_state Pointer to MasmExecutionState structure to fill
     * @return 0 on success
     */
    uint32_t RawrXD_CaptureRegisterState(MasmExecutionState* out_state);
    
    /**
     * @brief Unwind stack frames and record in execution state
     * 
     * Walks the frame chain (rbp-based or CFI) and populates frame info.
     * 
     * @param out_state Execution state to update
     * @param max_depth Maximum frames to walk
     * @return Actual frames found
     */
    uint32_t RawrXD_UnwindStackFrames(MasmExecutionState* out_state, uint32_t max_depth);
    
    // ========== PROCESS EXECUTION (HAD-HOC) ==========
    
    /**
     * @brief Fast path process launch and wait
     * 
     * CreateProcessW + WaitForSingleObject in MASM for latency sensitivity.
     * 
     * @param exe Executable path (UTF-8)
     * @param args Command-line arguments (UTF-8)
     * @param timeout_ms Timeout in milliseconds (0 = infinite)
     * @param out_exit_code Pointer to store exit code
     * @return 0 on success, non-zero on error
     */
    uint32_t RawrXD_LaunchAndWait(
        const char* exe,
        const char* args,
        uint32_t timeout_ms,
        uint32_t* out_exit_code
    );
}

/** @} */
