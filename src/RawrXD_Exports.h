#pragma once

/**
 * @file RawrXD_Exports.h
 * @brief RawrXD 80-Export Production Interface
 * 
 * This header defines the complete public interface for the RawrXD inference engine.
 * All 80 functions are exported from RawrXD_Titan.dll with ordinal references.
 * 
 * Version: 1.2.6-alpha (Singularity lane integration)
 * Date: 2026-04-04
 * Status: Phase 1 - In Implementation
 */

#include <Windows.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
   Type Definitions
   ============================================================================= */

typedef int32_t RAWRXD_STATUS;
typedef uint64_t RAWRXD_HANDLE;
typedef uint64_t RAWRXD_MODEL_HANDLE;
typedef uint64_t RAWRXD_INFERENCE_HANDLE;
typedef uint64_t RAWRXD_APERTURE_HANDLE;

/* Status codes */
#define RAWRXD_SUCCESS                    0x00000000
#define RAWRXD_ERROR_NOT_INITIALIZED      0x80000001
#define RAWRXD_ERROR_INVALID_PARAM        0x80000002
#define RAWRXD_ERROR_NOT_READY            0x80000003
#define RAWRXD_ERROR_NO_MODEL_LOADED      0x80000004
#define RAWRXD_ERROR_OUT_OF_MEMORY        0x80000005
#define RAWRXD_ERROR_INFERENCE_FAILED     0x80000006
#define RAWRXD_ERROR_TIMED_OUT            0x80000007
#define RAWRXD_ERROR_APERTURE_UNAVAIL     0x80000008
#define RAWRXD_ERROR_VA_FRAGMENTED        0x80000009
#define RAWRXD_ERROR_HOTPATCH_FAILED      0x8000000A

/* Capability flags */
#define RAWRXD_CAP_GPU_ACCELERATION       0x00000001
#define RAWRXD_CAP_QUANTIZATION           0x00000002
#define RAWRXD_CAP_APERTURE_MAPPING       0x00000004
#define RAWRXD_CAP_STREAMING              0x00000008
#define RAWRXD_CAP_HOTPATCHING            0x00000010
#define RAWRXD_CAP_MULTI_BATCH            0x00000020
#define RAWRXD_CAP_DIAGNOSTICS            0x00000040

/* Sampling parameter structure */
typedef struct {
    float temperature;              /* 0.0 to 2.0; default 0.7 */
    float top_p;                    /* 0.0 to 1.0; default 0.9 */
    int32_t top_k;                  /* 0 to 100; 0 = no limit; default 40 */
    float repetition_penalty;       /* 0.0 to 2.0; default 1.0 */
    int32_t max_tokens;             /* max tokens to generate; 0 = max model limit */
} RAWRXD_SAMPLING_PARAMS;

/* Model information structure */
typedef struct {
    uint64_t size_bytes;            /* Total model size in bytes */
    uint32_t parameter_count;       /* Parameter count (billions when shifted) */
    uint32_t vocab_size;            /* Vocabulary size */
    uint32_t context_length;        /* Context window size */
    char model_name[256];           /* Human-readable model name */
    char model_path[MAX_PATH];      /* Full path to model file */
    uint32_t precision_bits;        /* 4, 8, 16, 32 for quantization level */
} RAWRXD_MODEL_INFO;

/* Inference result structure */
typedef struct {
    uint64_t output_buffer;         /* Pointer to output token buffer */
    uint32_t output_token_count;    /* Number of tokens generated */
    uint32_t input_token_count;     /* Number of input tokens consumed */
    uint64_t latency_ms;            /* Inference latency in milliseconds */
    RAWRXD_STATUS status;           /* Final inference status */
} RAWRXD_INFERENCE_RESULT;

/* Aperture/VA status structure */
typedef struct {
    uint64_t aperture_base;         /* Base address of 1TB aperture */
    uint64_t aperture_total_bytes;  /* 1TB = 1099511627776 bytes */
    uint64_t mapped_bytes;          /* Currently mapped */
    uint64_t unmapped_bytes;        /* Free/unmapped */
    uint32_t mapped_chunks;         /* Number of chunks currently mapped */
    uint32_t fragment_count;        /* Number of gaps/fragments */
    float fragmentation_ratio;      /* 0.0 to 1.0 */
    uint32_t utilization_pct10000;  /* Utilization as pct * 10000 (0-1000000) */
} RAWRXD_APERTURE_STATUS;

/* Thread priority policy enum (Phase 15.2) */
typedef enum {
    RAWRXD_THREAD_PRIORITY_NORMAL = 0,        /* THREAD_PRIORITY_NORMAL */
    RAWRXD_THREAD_PRIORITY_HIGH = 1,          /* THREAD_PRIORITY_ABOVE_NORMAL + boost */
    RAWRXD_THREAD_PRIORITY_REALTIME_LIGHT = 2, /* THREAD_PRIORITY_TIME_CRITICAL, guarded */
    RAWRXD_THREAD_PRIORITY_AFFINITY_CPU0 = 3  /* Pin to CPU 0 for cache locality */
} RAWRXD_THREAD_PRIORITY_POLICY;

/* Thread policy status structure */
typedef struct {
    RAWRXD_THREAD_PRIORITY_POLICY current_policy;  /* Current policy preset */
    uint32_t base_priority;                         /* Current base priority (1-31) */
    uint32_t boost_enabled;                         /* PriorityBoost state */
    uint32_t affinity_mask;                         /* CPU affinity (0 = no affinity) */
} RAWRXD_THREAD_POLICY_STATUS;

/* Memory statistics structure */
typedef struct {
    uint64_t total_allocated;       /* Total bytes ever allocated */
    uint64_t currently_allocated;   /* Current working set */
    uint64_t peak_allocated;        /* Peak allocation so far */
    uint64_t virtual_address_space; /* Total VA reserved/committed */
    uint32_t heap_blocks;           /* Fragmentation: number of blocks */
    float heap_fragmentation;       /* 0.0 to 1.0 */
} RAWRXD_MEMORY_STATS;

/* Performance counter structure */
typedef struct {
    uint64_t inference_count;       /* Total inferences executed */
    uint64_t total_input_tokens;    /* Cumulative input tokens */
    uint64_t total_output_tokens;   /* Cumulative output tokens */
    double avg_latency_ms;          /* Average latency per inference */
    double peak_latency_ms;         /* Highest observed latency */
    double throughput_tokens_sec;   /* Tokens per second */
} RAWRXD_PERF_COUNTERS;

/* =============================================================================
   SECTION A: Lifecycle Management (8 exports, ordinals 1-8)
   ============================================================================= */

/**
 * @brief Initialize RawrXD subsystem
 * @return RAWRXD_SUCCESS on success, error code otherwise
 * @note Must be called before any other RawrXD function
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_Initialize(void);
/* @ordinal 1 */

/**
 * @brief Gracefully shutdown RawrXD subsystem
 * @return RAWRXD_SUCCESS on success
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_Shutdown(void);
/* @ordinal 2 */

/**
 * @brief Reset RawrXD to initial state
 * @details Unloads models, cancels pending inferences, clears caches
 * @return RAWRXD_SUCCESS on success
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_Reset(void);
/* @ordinal 3 */

/**
 * @brief Emergency abort (no cleanup)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_Abort(void);
/* @ordinal 4 */

/**
 * @brief Query current status
 * @param[out] status_code Pointer to receive status
 * @return RAWRXD_SUCCESS if status_code is populated
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetStatus(RAWRXD_STATUS* status_code);
/* @ordinal 5 */

/**
 * @brief Get version string
 * @param[out] version_string Buffer to receive version (e.g., "1.2.6-alpha")
 * @param[in] buffer_size Maximum bytes to write
 * @return RAWRXD_SUCCESS, or RAWRXD_ERROR_INVALID_PARAM if buffer too small
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetVersion(char* version_string, size_t buffer_size);
/* @ordinal 6 */

/**
 * @brief Get capabilities bitmap
 * @param[out] capabilities Pointer to receive capability flags (RAWRXD_CAP_*)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetCapabilities(uint32_t* capabilities);
/* @ordinal 7 */

/**
 * @brief Set diagnostic log level
 * @param[in] level Log verbosity (0=SILENT, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetLogLevel(int32_t level);
/* @ordinal 8 */

/* =============================================================================
   SECTION B: Model Management (12 exports, ordinals 9-20)
   ============================================================================= */

/**
 * @brief Load a GGUF model into VA aperture
 * @param[in] model_path Full path to .gguf file
 * @param[out] model_handle Handle to loaded model
 * @return RAWRXD_SUCCESS on success
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_LoadModel(const char* model_path, RAWRXD_MODEL_HANDLE* model_handle);
/* @ordinal 9 */

/**
 * @brief Unload model from VA space
 * @param[in] model_handle Model to unload
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_UnloadModel(RAWRXD_MODEL_HANDLE model_handle);
/* @ordinal 10 */

/**
 * @brief Get info about loaded model
 * @param[in] model_handle Model handle (0 for active)
 * @param[out] model_info Pointer to receive model info
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetModelInfo(RAWRXD_MODEL_HANDLE model_handle, RAWRXD_MODEL_INFO* model_info);
/* @ordinal 11 */

/**
 * @brief Enumerate available models
 * @param[out] model_paths Buffer to receive paths (separated by \\0)
 * @param[in] buffer_size Max bytes
 * @param[out] count Actual count
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ListModels(char* model_paths, size_t buffer_size, uint32_t* count);
/* @ordinal 12 */

/**
 * @brief Switch to a different loaded model
 * @param[in] model_handle Model to activate
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SelectModel(RAWRXD_MODEL_HANDLE model_handle);
/* @ordinal 13 */

/**
 * @brief Get currently active model
 * @param[out] model_handle Pointer to receive active model
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetActiveModel(RAWRXD_MODEL_HANDLE* model_handle);
/* @ordinal 14 */

/**
 * @brief Preload chunks into aperture
 * @param[in] model_handle Model
 * @param[in] chunk_start Start index
 * @param[in] chunk_count Count of chunks to preload
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_PreloadChunks(RAWRXD_MODEL_HANDLE model_handle, uint32_t chunk_start, uint32_t chunk_count);
/* @ordinal 15 */

/**
 * @brief Release chunks from aperture
 * @param[in] model_handle Model
 * @param[in] chunk_start Start index
 * @param[in] chunk_count Count to release
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ReleaseChunks(RAWRXD_MODEL_HANDLE model_handle, uint32_t chunk_start, uint32_t chunk_count);
/* @ordinal 16 */

/**
 * @brief Query chunk mapping state
 * @param[in] model_handle Model
 * @param[in] chunk_index Chunk index
 * @param[out] is_mapped 1 if mapped, 0 if not
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetChunkStatus(RAWRXD_MODEL_HANDLE model_handle, uint32_t chunk_index, uint32_t* is_mapped);
/* @ordinal 17 */

/**
 * @brief Verify model integrity via SHA256 checksums
 * @param[in] model_handle Model to validate
 * @param[out] is_valid 1 if valid, 0 if corrupted
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ValidateModelIntegrity(RAWRXD_MODEL_HANDLE model_handle, uint32_t* is_valid);
/* @ordinal 18 */

/**
 * @brief Get absolute path to active model
 * @param[out] model_path Buffer for path
 * @param[in] buffer_size Max bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetModelPath(char* model_path, size_t buffer_size);
/* @ordinal 19 */

/**
 * @brief Prime metadata cache for faster inference
 * @param[in] model_handle Model
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_CacheModelMetadata(RAWRXD_MODEL_HANDLE model_handle);
/* @ordinal 20 */

/* =============================================================================
   SECTION C: Inference Operations (16 exports, ordinals 21-36)
   ============================================================================= */

/**
 * @brief Begin asynchronous inference
 * @param[in] prompt Input prompt/tokens
 * @param[in] prompt_len Length of prompt
 * @param[out] inference_handle Handle for status/result queries
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_InferAsync(const char* prompt, size_t prompt_len, RAWRXD_INFERENCE_HANDLE* inference_handle);
/* @ordinal 21 */

/**
 * @brief Perform synchronous (blocking) inference
 * @param[in] prompt Input prompt
 * @param[in] prompt_len Length
 * @param[out] result Result structure
 * @param[in] timeout_ms Timeout in milliseconds (0 = infinite)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_InferSync(const char* prompt, size_t prompt_len, RAWRXD_INFERENCE_RESULT* result, uint32_t timeout_ms);
/* @ordinal 22 */

/**
 * @brief Cancel pending inference
 * @param[in] inference_handle Handle to cancel
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_CancelInference(RAWRXD_INFERENCE_HANDLE inference_handle);
/* @ordinal 23 */

/**
 * @brief Wait for inference completion with timeout
 * @param[in] inference_handle Handle
 * @param[in] timeout_ms Timeout (0 = infinite)
 * @return RAWRXD_SUCCESS when done, RAWRXD_ERROR_TIMED_OUT on timeout
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_WaitForInference(RAWRXD_INFERENCE_HANDLE inference_handle, uint32_t timeout_ms);
/* @ordinal 24 */

/**
 * @brief Retrieve inference result
 * @param[in] inference_handle Handle
 * @param[out] result Result structure
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetInferenceResult(RAWRXD_INFERENCE_HANDLE inference_handle, RAWRXD_INFERENCE_RESULT* result);
/* @ordinal 25 */

/**
 * @brief Get token count from completed inference
 * @param[in] inference_handle Handle
 * @param[out] token_count Output token count
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetInferenceTokenCount(RAWRXD_INFERENCE_HANDLE inference_handle, uint32_t* token_count);
/* @ordinal 26 */

/**
 * @brief Set sampling parameters
 * @param[in] params Sampling config
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetSamplingParams(const RAWRXD_SAMPLING_PARAMS* params);
/* @ordinal 27 */

/**
 * @brief Get current sampling parameters
 * @param[out] params Pointer to receive config
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetSamplingParams(RAWRXD_SAMPLING_PARAMS* params);
/* @ordinal 28 */

/**
 * @brief Set system prompt context
 * @param[in] system_prompt System message
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetSystemPrompt(const char* system_prompt);
/* @ordinal 29 */

/**
 * @brief Get current system prompt
 * @param[out] buffer Buffer for system prompt
 * @param[in] buffer_size Max bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetSystemPrompt(char* buffer, size_t buffer_size);
/* @ordinal 30 */

/**
 * @brief Tokenize input prompt string
 * @param[in] prompt Input string
 * @param[out] tokens Buffer to receive token IDs
 * @param[in] max_tokens Max tokens to generate
 * @param[out] token_count Actual count produced
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_TokenizeInput(const char* prompt, int32_t* tokens, size_t max_tokens, uint32_t* token_count);
/* @ordinal 31 */

/**
 * @brief Detokenize (decode tokens to text)
 * @param[in] tokens Token IDs
 * @param[in] token_count Count
 * @param[out] output Output text buffer
 * @param[in] output_size Max bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_DetokenizeOutput(const int32_t* tokens, uint32_t token_count, char* output, size_t output_size);
/* @ordinal 32 */

/**
 * @brief Get tokenizer metadata
 * @param[out] vocab_size Vocabulary size
 * @param[out] bos_token BOS token ID
 * @param[out] eos_token EOS token ID
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetTokenizerInfo(uint32_t* vocab_size, int32_t* bos_token, int32_t* eos_token);
/* @ordinal 33 */

/**
 * @brief Estimate token count without running inference
 * @param[in] prompt Input
 * @param[out] token_count Estimated count
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_EstimateTokens(const char* prompt, uint32_t* token_count);
/* @ordinal 34 */

/**
 * @brief Begin streaming inference session
 * @param[out] stream_handle Handle for streaming
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_BeginStreaming(RAWRXD_INFERENCE_HANDLE* stream_handle);
/* @ordinal 35 */

/**
 * @brief End streaming session
 * @param[in] stream_handle Handle to close
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_EndStreaming(RAWRXD_INFERENCE_HANDLE stream_handle);
/* @ordinal 36 */

/**
 * @brief Configure Win32 notification target for stream-ready events
 * @param[in] hwnd_value HWND value cast to uint64_t
 * @param[in] message_id Custom WM_* message to post when new chunk arrives
 * @param[in] wparam_tag User tag passed through WPARAM
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_StreamConfigureWindow(uint64_t hwnd_value, uint32_t message_id, uint32_t wparam_tag);
/* @ordinal 79 */

/**
 * @brief Pop next streaming chunk from internal ring buffer
 * @param[out] buffer UTF-8 payload buffer
 * @param[in] buffer_size Capacity in bytes
 * @param[out] out_len Bytes written excluding terminator
 * @param[out] out_seq Monotonic sequence number
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_StreamPop(char* buffer, size_t buffer_size, uint32_t* out_len, uint64_t* out_seq);
/* @ordinal 80 */

/**
 * @brief Query stream queue counters
 * @param[out] queued_count Number of queued chunks
 * @param[out] dropped_count Number of dropped chunks due to overflow
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_StreamGetStats(uint32_t* queued_count, uint32_t* dropped_count);
/* @ordinal 81 */

/**
 * @brief Clear stream queue and sequence state
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_StreamReset(void);
/* @ordinal 82 */

/* =============================================================================
   SECTION D: Aperture/VA Subdivision (12 exports, ordinals 37-48)
   ============================================================================= */

/**
 * @brief Initialize 1TB aperture placeholder
 * @return RAWRXD_SUCCESS
 * @note Wraps k_swap_aperture_init from v1.2.6-alpha kernel
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ApertureInit(void);
/* @ordinal 37 */

/**
 * @brief Shutdown and release aperture
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ApertureShutdown(void);
/* @ordinal 38 */

/**
 * @brief Map a chunk into aperture slot
 * @param[in] chunk_index Index of chunk (0-N)
 * @param[out] mapped_address Pointer to receive mapped VA
 * @return RAWRXD_SUCCESS
 * @note Wraps k_swap_aperture_map_chunk
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_MapChunk(uint32_t chunk_index, uint64_t* mapped_address);
/* @ordinal 39 */

/**
 * @brief Unmap chunk from aperture
 * @param[in] chunk_index Chunk to release
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_UnmapChunk(uint32_t chunk_index);
/* @ordinal 40 */

/**
 * @brief Get aperture base address
 * @param[out] base_address Pointer to receive base
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetApertureBase(uint64_t* base_address);
/* @ordinal 41 */

/**
 * @brief Get aperture utilization status
 * @param[out] status Aperture status structure
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetApertureUtilization(RAWRXD_APERTURE_STATUS* status);
/* @ordinal 42 */

/**
 * @brief Defragment aperture (if supported)
 * @return RAWRXD_SUCCESS or RAWRXD_ERROR_NOT_READY
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_CompactAperture(void);
/* @ordinal 43 */

/**
 * @brief Reserve chunks upfront
 * @param[in] chunk_count Number of chunks to preallocate
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_PreAllocateChunks(uint32_t chunk_count);
/* @ordinal 44 */

/**
 * @brief Get chunk mapping latency telemetry
 * @param[out] latency_ms Average map time in ms
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetChunkMappingLatency(double* latency_ms);
/* @ordinal 45 */

/**
 * @brief Get chunk unmap latency telemetry
 * @param[out] latency_ms Average unmap time in ms
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetChunkUnmapLatency(double* latency_ms);
/* @ordinal 46 */

/**
 * @brief Set aperture policy (split vs. replace strategy)
 * @param[in] policy_flags Configuration flags
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetAperturePolicy(uint32_t policy_flags);
/* @ordinal 47 */

/**
 * @brief Get current aperture policy
 * @param[out] policy_flags Pointer to receive flags
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetAperturePolicy(uint32_t* policy_flags);
/* @ordinal 48 */

/* =============================================================================
   SECTION E: Memory & Performance (14 exports, ordinals 49-62)
   ============================================================================= */

/**
 * @brief Allocate aligned buffer for intermediate results
 * @param[in] size Bytes to allocate
 * @param[out] buffer Pointer to allocated memory
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_AllocateBuffer(size_t size, void** buffer);
/* @ordinal 49 */

/**
 * @brief Release intermediate buffer
 * @param[in] buffer Pointer to release
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_FreeBuffer(void* buffer);
/* @ordinal 50 */

/**
 * @brief Query working set and VA usage
 * @param[out] stats Memory statistics
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetMemoryStats(RAWRXD_MEMORY_STATS* stats);
/* @ordinal 51 */

/**
 * @brief Query heap fragmentation
 * @param[out] heap_stats Heap statistics
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetHeapStats(RAWRXD_MEMORY_STATS* heap_stats);
/* @ordinal 52 */

/**
 * @brief Trigger garbage collection
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_MemoryGC(void);
/* @ordinal 53 */

/**
 * @brief Set maximum working set limit
 * @param[in] max_bytes Limit in bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetWorkingSetLimit(uint64_t max_bytes);
/* @ordinal 54 */

/**
 * @brief Get maximum working set limit
 * @param[out] limit_bytes Pointer to receive limit
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetWorkingSetLimit(uint64_t* limit_bytes);
/* @ordinal 55 */

/**
 * @brief Query peak memory allocation
 * @param[out] peak_bytes Peak ever allocated
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetPeakMemoryUsage(uint64_t* peak_bytes);
/* @ordinal 56 */

/**
 * @brief Reset memory statistics counters
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ResetMemoryStatistics(void);
/* @ordinal 57 */

/**
 * @brief Query VA fragmentation ratio
 * @param[out] fragmentation Ratio 0.0-1.0
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetVAFragmentation(float* fragmentation);
/* @ordinal 58 */

/**
 * @brief Get cache miss rate telemetry
 * @param[out] miss_rate L1/L2 miss percentage
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetCacheMissRate(float* miss_rate);
/* @ordinal 59 */

/**
 * @brief Get average inference latency
 * @param[out] latency_ms Average in milliseconds
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetInferenceLatency(double* latency_ms);
/* @ordinal 60 */

/**
 * @brief Get throughput in tokens per second
 * @param[out] throughput Tokens/sec
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetThroughputTokensPerSec(double* throughput);
/* @ordinal 61 */

/**
 * @brief Reset performance counters
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ResetPerformanceCounters(void);
/* @ordinal 62 */

/* =============================================================================
   SECTION F: Errors, Diagnostics & Observability (10 exports, ordinals 63-72)
   ============================================================================= */

/**
 * @brief Get most recent error code
 * @return Error code from last operation
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetLastError(void);
/* @ordinal 63 */

/**
 * @brief Convert error code to human-readable message
 * @param[in] error_code Error from RawrXD_GetLastError
 * @param[out] message Buffer for message
 * @param[in] buffer_size Max bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetErrorString(RAWRXD_STATUS error_code, char* message, size_t buffer_size);
/* @ordinal 64 */

/**
 * @brief Clear error state
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ClearError(void);
/* @ordinal 65 */

/**
 * @brief Enable diagnostic logging
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_EnableDiagnostics(void);
/* @ordinal 66 */

/**
 * @brief Disable diagnostic logging
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_DisableDiagnostics(void);
/* @ordinal 67 */

/**
 * @brief Retrieve buffered diagnostic logs
 * @param[out] log_buffer Buffer for logs
 * @param[in] buffer_size Max bytes
 * @param[out] bytes_written Actual bytes written
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetDiagnosticLog(char* log_buffer, size_t buffer_size, size_t* bytes_written);
/* @ordinal 68 */

/**
 * @brief Register external log callback
 * @param[in] callback Function pointer: void (*)(const char* message)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetDiagnosticCallback(void (*callback)(const char*));
/* @ordinal 69 */

/**
 * @brief Generate full audit report (JSON)
 * @param[out] report_buffer Buffer for report
 * @param[in] buffer_size Max bytes
 * @param[out] bytes_written Actual bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GenerateAuditReport(char* report_buffer, size_t buffer_size, size_t* bytes_written);
/* @ordinal 70 */

/**
 * @brief Validate internal state consistency
 * @param[out] is_consistent 1 if OK, 0 if corruption detected
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ValidateInternalConsistency(uint32_t* is_consistent);
/* @ordinal 71 */

/**
 * @brief Debug: dump heap walker output
 * @param[out] output_buffer Buffer for heap dump
 * @param[in] buffer_size Max bytes
 * @param[out] bytes_written Actual bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_DumpHeapWalker(char* output_buffer, size_t buffer_size, size_t* bytes_written);
/* @ordinal 72 */

/* =============================================================================
   SECTION G: Advanced Features (5 exports, ordinals 73-77)
   ============================================================================= */

/**
 * @brief Enable runtime hotpatching support
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_EnableHotpatch(void);
/* @ordinal 73 */

/**
 * @brief Apply a hotpatch to running instance
 * @param[in] patch_data Patch binary
 * @param[in] patch_size Patch size in bytes
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_ApplyHotpatch(const uint8_t* patch_data, size_t patch_size);
/* @ordinal 74 */

/**
 * @brief Query hotpatch status
 * @param[out] status Patch state (0=none, 1=pending, 2=applied)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_QueryHotpatchStatus(uint32_t* status);
/* @ordinal 75 */

/**
 * @brief Activate quantization layer
 * @param[in] bits Quantization bits (4, 8, 16, 32)
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_EnableQuantization(uint32_t bits);
/* @ordinal 76 */

/**
 * @brief Batch multiple inferences for parallel execution
 * @param[in] prompts Array of prompt pointers
 * @param[in] prompt_count Number of prompts
 * @param[out] batch_handle Handle to batch operation
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_BatchInferences(const char** prompts, uint32_t prompt_count, RAWRXD_INFERENCE_HANDLE* batch_handle);
/* @ordinal 77 */

/* ===================================================================
   SECTION Q: Thread Priority Control (3 exports, ordinals 94-96, Phase 15.2)
   =================================================================== */

/**
 * @brief Set inference thread priority policy (Phase 15.2)
 * @details Safe, documented alternatives to raw SetThreadPriority.
 *          Attempts policy; falls back gracefully if unavailable.
 * @param[in] policy Policy preset from RAWRXD_THREAD_PRIORITY_POLICY
 * @return RAWRXD_SUCCESS or error if policy not applicable
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetInferenceThreadPolicy(uint32_t policy);
/* @ordinal 94 */

/**
 * @brief Get current inference thread policy
 * @param[out] status Current policy state
 * @return RAWRXD_SUCCESS
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_GetInferenceThreadPolicy(RAWRXD_THREAD_POLICY_STATUS* status);
/* @ordinal 95 */

/**
 * @brief Set CPU affinity mask for inference worker (Phase 15.2)
 * @details Pins inference thread to specific CPUs for cache locality.
 * @param[in] affinity_mask CPU mask (bit 0 = CPU 0, etc.)
 * @return RAWRXD_SUCCESS or RAWRXD_ERROR_INVALID_PARAM
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
RawrXD_SetInferenceThreadAffinity(uint32_t affinity_mask);
/* @ordinal 96 */

/* ===================================================================
   SECTION R: Extended Metrics Display (3 exports, ordinals 97-99, Phase 15.4)
   =================================================================== */

/**
 * @brief Calculate current inference throughput as integer tok/s (Phase 15.4)
 * @details Derives tok/s from last inference latency and output token count.
 *          Returns 0 if no completed inference is available.
 *          Clamped to [0, 65535] for status bar display.
 * @param[out] out_tokens_per_sec Integer tokens per second
 * @return RAWRXD_SUCCESS or RAWRXD_ERROR_INVALID_PARAM
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
Rawr_CalculateThroughput(uint32_t* out_tokens_per_sec);
/* @ordinal 97 */

/**
 * @brief Calculate current inference latency as integer ms/tok (Phase 15.4)
 * @details Derives ms/tok from last inference latency and output token count.
 *          Returns 0 if no completed inference is available.
 *          Clamped to [0, 999] for status bar display.
 * @param[out] out_ms_per_tok Integer milliseconds per token
 * @return RAWRXD_SUCCESS or RAWRXD_ERROR_INVALID_PARAM
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
Rawr_CalculateLatency(uint32_t* out_ms_per_tok);
/* @ordinal 98 */

/**
 * @brief Calculate expert KV-cache hit ratio as integer 0-100% (Phase 15.4)
 * @details Uses SDMA expert cache hit/miss counters.
 *          Returns 0 if no cache accesses have been recorded.
 * @param[out] out_percent Integer cache hit percentage (0-100)
 * @return RAWRXD_SUCCESS or RAWRXD_ERROR_INVALID_PARAM
 */
__declspec(dllexport) RAWRXD_STATUS __stdcall
Rawr_CalculateCacheHitRatio(uint32_t* out_percent);
/* @ordinal 99 */

#ifdef __cplusplus
}
#endif
