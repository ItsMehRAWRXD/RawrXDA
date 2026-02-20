// =============================================================================
// streaming_orchestrator.h — C ABI header for RawrXD-StreamingOrchestrator module
// =============================================================================
// Declares extern "C" symbols for the StreamingOrchestrator MASM module.
// The MASM module provides: Vulkan compute pipeline, multi-threaded DEFLATE,
// layer paging with prefetch/eviction, and timeline semaphore sync.
//
// This is the SUPERSET of VulkanKernel — it includes all Vulkan compute
// functionality PLUS streaming, threading, and memory management.
//
// When RAWR_HAS_MASM=1 and the ASM .obj is linked, these resolve to native ASM.
// Otherwise, the stub file provides C++ fallbacks.
// =============================================================================
#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Operator types (from .exec format)
// ---------------------------------------------------------------------------
constexpr uint32_t SO_OP_NORM   = 1;
constexpr uint32_t SO_OP_ATTN   = 2;
constexpr uint32_t SO_OP_FFN    = 3;
constexpr uint32_t SO_OP_EMBED  = 4;

// Memory pressure levels
constexpr uint32_t SO_PRESSURE_LOW      = 0;
constexpr uint32_t SO_PRESSURE_MEDIUM   = 1;
constexpr uint32_t SO_PRESSURE_HIGH     = 2;
constexpr uint32_t SO_PRESSURE_CRITICAL = 3;

// Layer states
constexpr uint32_t SO_LAYER_NOT_LOADED  = 0;
constexpr uint32_t SO_LAYER_LOADING     = 1;
constexpr uint32_t SO_LAYER_LOADED      = 2;
constexpr uint32_t SO_LAYER_EVICTING    = 3;
constexpr uint32_t SO_LAYER_EVICTED     = 4;

// Configuration
constexpr uint32_t SO_DEFAULT_THREADS   = 8;
constexpr uint32_t SO_PREFETCH_DISTANCE = 2;

// ---------------------------------------------------------------------------
// Opaque context structures (match MASM layouts)
// ---------------------------------------------------------------------------
struct SO_StreamingMetrics {
    uint64_t layers_loaded;
    uint64_t layers_evicted;
    uint64_t bytes_streamed;
    uint64_t bytes_decompressed;
    uint64_t prefetch_hits;
    uint64_t prefetch_misses;
    uint64_t avg_load_time_ms;
    uint64_t avg_eviction_time_ms;
};

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Vulkan Compute Pipeline (shared with VulkanKernel)
// ---------------------------------------------------------------------------

// Load a .exec file (distilled GGUF topology). Returns 1/0.
int       SO_LoadExecFile(const char* filePath);

// Initialize Vulkan instance + device. Returns 1/0.
int       SO_InitializeVulkan(void);

// Allocate a memory arena (VirtualAlloc + Vulkan buffer). Returns ptr or NULL.
void*     SO_CreateMemoryArena(uint64_t sizeBytes);

// Compile SPIR-V shader for an operator type. Returns 1/0.
int       SO_CompileSPIRVShader(void* shaderModule, uint32_t opType, uint32_t opCount);

// Create compute pipelines for all operators. Returns 1/0.
int       SO_CreateComputePipelines(void* operatorTable, uint64_t operatorCount);

// Execute a single layer (norm → attn → ffn).
void      SO_ExecuteLayer(void* layerInfo, void* operatorTable);

// Dispatch a single operator to GPU compute.
void      SO_DispatchOperator(void* operatorPtr);

// Execute full sequential inference (all layers). Returns 1/0.
int       SO_ExecuteInference(void* layerTable, uint64_t layerCount);

// Print inference statistics.
void      SO_PrintStatistics(void);

// ---------------------------------------------------------------------------
// Streaming Engine (unique to StreamingOrchestrator)
// ---------------------------------------------------------------------------

// Initialize the streaming subsystem (contexts, prefetch, timeline). Returns 1/0.
int       SO_InitializeStreaming(void);

// Create thread pool (zero-fill contexts). Returns 1/0.
int       SO_CreateThreadPool(void);

// Spawn DEFLATE worker threads. Returns 1/0.
int       SO_StartDEFLATEThreads(uint32_t threadCount);

// Initialize the prefetch queue. Returns 1/0.
int       SO_InitializePrefetchQueue(void);

// Execute streaming inference with paging + prefetch + eviction. Returns 1/0.
int       SO_ExecuteStreamingInference(void* layerTable, uint64_t layerCount);

// Process a layer asynchronously (assign to idle thread). Returns 1/0.
int       SO_ProcessLayerAsync(uint64_t layerId);

// Evict a layer from memory (-1 = auto-LRU). Returns 1/0.
int       SO_EvictLayer(int64_t layerId);

// Queue a layer for prefetch. Returns 1/0.
int       SO_PrefetchLayer(uint64_t layerId);

// Calculate prefetch priority score (0-100) for a layer.
uint32_t  SO_CalculatePrefetchScore(uint64_t layerId);

// Get current memory pressure level (0=Low, 1=Med, 2=High, 3=Critical).
uint32_t  SO_GetMemoryPressure(void);

// Update streaming metrics.
void      SO_UpdateMetrics(void);

// Print streaming metrics to console.
void      SO_PrintMetrics(void);

// Get streaming metrics snapshot.
void      SO_GetMetrics(SO_StreamingMetrics* out);

// ---------------------------------------------------------------------------
// Timeline Semaphore + Memory-Mapped I/O Utilities
// ---------------------------------------------------------------------------

// Create a Vulkan timeline semaphore. Returns handle.
intptr_t  SO_CreateTimelineSemaphore(void);

// Signal timeline semaphore to a value. Returns 1.
int       SO_SignalTimeline(intptr_t semaphore, uint64_t value);

// Wait for timeline semaphore to reach a value. Returns 1.
int       SO_WaitTimeline(intptr_t semaphore, uint64_t value);

// Seek and memory-map a 64MB file chunk. Returns buffer ptr.
void*     SO_FileSeekAndMap(uint64_t fileOffset);

// Decompress a DEFLATE block. Returns decompressed size.
uint64_t  SO_DecompressBlock(void* src, void* dest, uint64_t compressedSize);

// Execute a single layer's operators.
void      SO_ExecuteLayerOps(void* layerPtr);

// Destroy the streaming system (cleanup all resources).
void      SO_DestroyStreamingSystem(void);

// Open a memory-mapped file. Returns handle.
intptr_t  SO_OpenMemoryMappedFile(const char* path, uint64_t fileSize);

#ifdef __cplusplus
}
#endif
