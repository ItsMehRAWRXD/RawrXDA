// =============================================================================
// QuadBuffer_DMA.h
// Public API Header for RawrXD Quad-Buffer DMA Orchestrator
// Interfaces with Phase 2 (Model Loading), Phase 3 (Inference Kernel),
// Phase 4 (Swarm Transport), and Phase 5 (Orchestration)
// =============================================================================

#ifndef QUADBUFFER_DMA_H
#define QUADBUFFER_DMA_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// CONSTANTS
// =============================================================================

#define QUAD_BUFFER_COUNT        4
#define PAGE_SIZE                0x40000000          // 1GB
#define PAGE_SHIFT               30
#define YTFN_SENTINEL            0x7FFFFFFFFFFFFFFF  // Trap sentinel
#define DMA_ALIGNMENT            4096                // Page alignment

// Buffer states
#define BUF_STATE_EMPTY          0
#define BUF_STATE_LOADING        1
#define BUF_STATE_READY          2
#define BUF_STATE_COMPUTING      3

// =============================================================================
// OPAQUE TYPES
// =============================================================================

typedef void* QuadBufferHandle;

// =============================================================================
// METRICS STRUCTURE
// =============================================================================

typedef struct {
    uint64_t hdd_read_bytes;        // Total bytes read from disk
    uint64_t dma_write_bytes;       // Total bytes written via DMA
    uint64_t stall_cycles;          // CPU cycles spent stalling for data
    uint32_t trap_count;            // Number of YTFN_SENTINEL traps
    uint32_t trap_resolved_count;   // Number of traps resolved
    uint64_t uptime_microseconds;   // System uptime
    double   hdd_throughput_mbps;   // HDD throughput in MB/s
    double   dma_throughput_mbps;   // DMA throughput in MB/s
} QuadBufferMetrics;

// =============================================================================
// BUFFER STATUS STRUCTURE
// =============================================================================

typedef struct {
    uint32_t slots[QUAD_BUFFER_COUNT];      // State of each slot (BUF_STATE_*)
    int32_t layers[QUAD_BUFFER_COUNT];      // Layer index in each slot
    double efficiency_percent;               // Percentage of slots in use
} QuadBufferStatus;

// =============================================================================
// INITIALIZATION & LIFECYCLE
// =============================================================================

/**
 * Create a new quad-buffer orchestrator instance
 * Returns opaque handle for subsequent operations
 */
QuadBufferHandle QuadBuffer_Create(void);

/**
 * Initialize the quad-buffer system with model file and phase contexts
 *
 * Parameters:
 *   handle        - Orchestrator handle from QuadBuffer_Create()
 *   model_file    - Path to 800B parameter model file (wchar_t*)
 *   layer_size    - Size of each layer in bytes (typically 1GB for 800B models)
 *   num_layers    - Total number of layers in model (800)
 *   vram_base     - Base VRAM address for GPU buffer allocation
 *   phase2_ctx    - Phase 2 model loader context (optional, can be NULL)
 *   phase3_ctx    - Phase 3 inference kernel context (optional)
 *   phase4_ctx    - Phase 4 swarm transport context (optional)
 *   phase5_ctx    - Phase 5 orchestration context (optional)
 *
 * Returns:
 *   true if initialization successful, false otherwise
 *
 * Notes:
 *   - Model file must be accessible with GENERIC_READ permission
 *   - File is opened with FILE_FLAG_NO_BUFFERING to bypass OS cache
 *   - VRAM must be pre-allocated and aligned to 1GB boundaries
 *   - Phase contexts are stored for integration callbacks
 */
bool QuadBuffer_Initialize(
    QuadBufferHandle handle,
    const wchar_t* model_file,
    uint64_t layer_size,
    uint32_t num_layers,
    void* vram_base,
    void* phase2_ctx,
    void* phase3_ctx,
    void* phase4_ctx,
    void* phase5_ctx
);

/**
 * Shutdown and cleanup the quad-buffer system
 * Closes file handles, I/O ports, frees all allocated memory
 */
void QuadBuffer_Destroy(QuadBufferHandle handle);

// =============================================================================
// CORE BUFFER ACCESS - THE "BACKWARDS FORMULA"
// =============================================================================

/**
 * Get physical VRAM pointer for a layer (blocking variant)
 *
 * Implements the "backwards formula" for 800B model streaming:
 *   slot = layer_idx % 4
 *   if slots[slot].layer == layer_idx && slots[slot].ready
 *       return slots[slot].vram_ptr
 *   else
 *       return YTFN_SENTINEL (triggers stall + trap handler)
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   layer_idx   - Requested layer index (0 to 799 for 800B model)
 *
 * Returns:
 *   Physical VRAM pointer ready for GPU compute, or YTFN_SENTINEL if not ready
 *   
 * Behavior:
 *   - If layer data is not yet in VRAM, this function BLOCKS
 *   - Blocks by calling INFINITY_HandleYTfnTrap() which yields until ready
 *   - Guarantees physical pointer is valid when function returns
 *   - NEVER returns YTFN_SENTINEL on success (unless already loaded)
 *
 * GPU Integration:
 *   GPU code: mov rax, [QuadBuffer_GetLayerPtr(layer_idx)]
 *   If not ready: GPU hits YTFN_SENTINEL trap handler
 *   Trap handler: Calls INFINITY_HandleYTfnTrap() -> stalls until ready
 *   Result: GPU automatically waits for data without explicit synchronization
 */
uint64_t QuadBuffer_GetLayerPtr(
    QuadBufferHandle handle,
    uint64_t layer_idx
);

/**
 * Get physical VRAM pointer for a layer (non-blocking variant)
 *
 * Same as QuadBuffer_GetLayerPtr() but returns immediately without blocking
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   layer_idx   - Requested layer index
 *
 * Returns:
 *   Physical VRAM pointer if data is ready
 *   YTFN_SENTINEL if data is not yet available
 *
 * Usage:
 *   Useful for checking if layer is ready without stalling
 *   Example: If polling is needed before compute kernel launch
 */
uint64_t QuadBuffer_GetLayerPtrNonBlocking(
    QuadBufferHandle handle,
    uint64_t layer_idx
);

/**
 * Notify orchestrator that GPU has finished processing a layer
 *
 * This triggers the buffer rotation mechanism:
 *   1. Free slot (current - 2): GPU is done, safe to reuse
 *   2. Rotate: Move window forward by 1 layer
 *   3. Prefetch: Start loading (current + 2) from HDD
 *   4. DMA: Start copying (current + 1) to VRAM
 *
 * Parameters:
 *   handle              - Orchestrator handle
 *   completed_layer_idx - Layer index that GPU just finished
 *
 * Timing:
 *   Should be called immediately after GPU completes compute on layer
 *   Typically called from Phase 3 (inference kernel)
 *
 * Notes:
 *   - Can be called from GPU command queue (not necessarily CPU)
 *   - Is thread-safe (uses SRWLock internally)
 *   - Cascades: frees old slots, starts new HDD reads
 */
void QuadBuffer_NotifyLayerComplete(
    QuadBufferHandle handle,
    uint64_t completed_layer_idx
);

// =============================================================================
// SLOT MANAGEMENT - Low-Level Access
// =============================================================================

/**
 * Get current state of a specific buffer slot
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   slot_index  - Slot number (0-3)
 *
 * Returns:
 *   BUF_STATE_EMPTY (0) - Available for loading
 *   BUF_STATE_LOADING (1) - DMA from HDD in progress
 *   BUF_STATE_READY (2) - Data in RAM, ready for GPU DMA
 *   BUF_STATE_COMPUTING (3) - Data in VRAM, GPU actively using
 */
uint32_t QuadBuffer_GetSlotState(
    QuadBufferHandle handle,
    uint32_t slot_index
);

/**
 * Get VRAM pointer for a slot
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   slot_index  - Slot number (0-3)
 *
 * Returns:
 *   Physical VRAM address (always valid, 1GB aligned)
 *   Each slot has fixed VRAM location
 */
uint64_t QuadBuffer_GetSlotVramPtr(
    QuadBufferHandle handle,
    uint32_t slot_index
);

/**
 * Get pinned RAM pointer for a slot
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   slot_index  - Slot number (0-3)
 *
 * Returns:
 *   Physical pinned RAM address (always valid, page aligned)
 *   This is the intermediate buffer for HDD->RAM->VRAM pipeline
 */
uint64_t QuadBuffer_GetSlotRamPtr(
    QuadBufferHandle handle,
    uint32_t slot_index
);

// =============================================================================
// STATUS & DIAGNOSTICS
// =============================================================================

/**
 * Get comprehensive buffer status snapshot
 *
 * Parameters:
 *   handle - Orchestrator handle
 *
 * Returns:
 *   QuadBufferStatus structure with:
 *     - slots[]: Current state of each quad slot
 *     - layers[]: Layer index loaded in each slot
 *     - efficiency_percent: Percentage of slots actively in use
 *
 * Usage:
 *   For monitoring, diagnostics, and autotuning decisions
 */
QuadBufferStatus QuadBuffer_GetStatus(QuadBufferHandle handle);

/**
 * Get performance metrics
 *
 * Parameters:
 *   handle - Orchestrator handle
 *
 * Returns:
 *   QuadBufferMetrics structure with throughput, latency, trap statistics
 *
 * Metrics Include:
 *   - hdd_read_bytes: Total data read from disk
 *   - dma_write_bytes: Total data transferred to VRAM
 *   - stall_cycles: CPU cycles spent waiting for data
 *   - trap_count: Number of times GPU hit unready data
 *   - trap_resolved_count: Number of traps successfully resolved
 *   - hdd_throughput_mbps: Measured HDD sequential read speed
 *   - dma_throughput_mbps: Measured GPU DMA speed
 *
 * Used By:
 *   Phase 5: Autotuning and performance monitoring
 *   Performance analysis and optimization
 */
QuadBufferMetrics QuadBuffer_GetMetrics(QuadBufferHandle handle);

/**
 * Reset all performance metrics to zero
 *
 * Parameters:
 *   handle - Orchestrator handle
 *
 * Usage:
 *   After benchmarking warmup, to start clean measurements
 */
void QuadBuffer_ResetMetrics(QuadBufferHandle handle);

/**
 * Print human-readable status to stdout
 *
 * Displays:
 *   - Buffer slot states
 *   - Efficiency percentage
 *   - Throughput metrics
 *   - Trap statistics
 *
 * Useful for:
 *   Debugging, monitoring, development
 */
void QuadBuffer_PrintStatus(QuadBufferHandle handle);

// =============================================================================
// PHASE INTEGRATION CALLBACKS
// =============================================================================

/**
 * Phase 2 Integration: Get next layer pointer
 *
 * Called by Phase 2 (model loader) to fetch layers during streaming
 * Coordinates with Phase 1 to ensure buffers are ready
 *
 * Parameters:
 *   handle    - Orchestrator handle
 *   layer_idx - Layer to load
 *
 * Returns:
 *   Physical pointer to layer in VRAM (blocking until ready)
 */
uint64_t QuadBuffer_Phase2_GetNextLayerPtr(
    QuadBufferHandle handle,
    uint32_t layer_idx
);

/**
 * Phase 3 Integration: Notify layer computation complete
 *
 * Called by Phase 3 (inference kernel) after tensor operations on a layer
 * Signals that GPU is done with the layer, triggers buffer rotation
 *
 * Parameters:
 *   handle    - Orchestrator handle
 *   layer_idx - Layer that was just computed
 */
void QuadBuffer_Phase3_NotifyLayerComplete(
    QuadBufferHandle handle,
    uint32_t layer_idx
);

/**
 * Phase 4 Integration: Initiate RAM->VRAM DMA transfer
 *
 * Called by Phase 4 (swarm transport) to copy layer from pinned RAM to VRAM
 * Coordinates GPU DMA operations with the quad-buffer state machine
 *
 * Parameters:
 *   handle      - Orchestrator handle
 *   slot_index  - Slot containing data in RAM
 *   dest_vram   - Target VRAM address
 *
 * Returns:
 *   true if DMA was initiated successfully
 *   false if slot not ready (wait and retry)
 */
bool QuadBuffer_Phase4_InitiateDMA(
    QuadBufferHandle handle,
    uint32_t slot_index,
    uint64_t dest_vram
);

/**
 * Phase 5 Integration: Report metrics to orchestrator
 *
 * Called by Phase 5 (orchestration) to collect performance data
 * Used for autotuning, load balancing, health checks
 *
 * Parameters:
 *   handle - Orchestrator handle
 */
void QuadBuffer_Phase5_ReportMetrics(QuadBufferHandle handle);

// =============================================================================
// TRAP HANDLER - Internal use, but documented for completeness
// =============================================================================

/**
 * Handle YTFN_SENTINEL trap from GPU
 *
 * Automatically called when GPU hits YTFN_SENTINEL during memory access
 * Stalls until the requested data is ready in VRAM
 *
 * INTERNAL FUNCTION - Not typically called directly by application code
 *
 * Parameters:
 *   handle           - Orchestrator handle
 *   trapped_address  - YTFN_SENTINEL value (encodes layer index)
 *
 * Returns:
 *   Physical VRAM pointer after stall resolves
 *
 * How It Works:
 *   1. GPU loads from YTFN_SENTINEL address
 *   2. Memory controller signals trap exception
 *   3. Trap handler decodes layer index from address
 *   4. Handler calls INFINITY_HandleYTfnTrap()
 *   5. Stalls with SwitchToThread() until data ready
 *   6. Returns valid VRAM pointer
 *   7. GPU retries load with valid address
 *
 * This creates "magical" seamless data availability without explicit GPU waits
 */
uint64_t QuadBuffer_HandleTrap(
    QuadBufferHandle handle,
    uint64_t trapped_address
);

// =============================================================================
// ARCHITECTURE OVERVIEW
// =============================================================================

/*
    ┌─────────────────────────────────────────────────────────────────┐
    │ 800B Parameter Model on Disk (800GB total)                      │
    └─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (Direct I/O, no OS cache)
    ┌─────────────────────────────────────────────────────────────────┐
    │ Layer N-3 │ Layer N-2 │ Layer N-1 │ Layer N   │ ...             │
    │ HDD Slot  │ (Prefetch │ (Loading) │ (Ready)   │                 │
    │ (Empty)   │ Next)     │ (Async    │ (In RAM)  │                 │
    │           │           │ from HDD) │           │                 │
    └─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (Pinned RAM buffers, 4GB total)
    ┌───────────┬───────────┬───────────┬───────────┐
    │ Slot 0    │ Slot 1    │ Slot 2    │ Slot 3    │ QUAD-BUFFER
    │ (1GB)     │ (1GB)     │ (1GB)     │ (1GB)     │
    │ EMPTY     │ LOADING   │ READY     │ COMPUTING│ State Machine
    └───────────┴───────────┴───────────┴───────────┘
                              │
                              ▼ (GPU DMA, 1GB at a time)
    ┌─────────────────────────────────────────────────────────────────┐
    │ GPU VRAM: 4GB sliding window of current + next 3 layers         │
    │ Layer N in Slot 0, Layer N+1 in Slot 1, etc.                   │
    └─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (Compute on GPU)
    ┌─────────────────────────────────────────────────────────────────┐
    │ GPU Compute Kernels (Phase 3)                                   │
    │ Process current layer, then notify completion                   │
    └─────────────────────────────────────────────────────────────────┘

    PROCESS FLOW:
    1. GPU: QuadBuffer_GetLayerPtr(N)
       Returns VRAM pointer or YTFN_SENTINEL trap
       
    2. GPU: Compute on layer N
    
    3. GPU: QuadBuffer_NotifyLayerComplete(N)
       Triggers buffer rotation:
       - Free slot (N-2), reuse for prefetch
       - Prefetch layer (N+2) from HDD
       - DMA layer (N+1) to VRAM
       - Mark layer N as COMPUTING
    
    4. Repeat for N+1, N+2, etc.

    KEY INSIGHT:
    GPU sees only 4GB of VRAM (4 * 1GB slots)
    But transparently processes 800GB model through buffering
    Never needs to know about disk I/O or buffer management
    All coordination is automatic via YTFN trap and notifications
*/

// =============================================================================
// USAGE EXAMPLE - Conceptual pseudocode
// =============================================================================

/*
    // Initialize
    QuadBufferHandle qb = QuadBuffer_Create();
    QuadBuffer_Initialize(qb, L"model.gguf", 1GB, 800, vram_base,
                         phase2_ctx, phase3_ctx, phase4_ctx, phase5_ctx);
    
    // Inference loop
    for (uint32_t layer = 0; layer < 800; layer++) {
        // Get pointer (will stall until ready)
        uint64_t layer_ptr = QuadBuffer_GetLayerPtr(qb, layer);
        
        // Compute on GPU
        GPU_InferenceKernel(layer_ptr, attention_cache, output);
        
        // Notify completion (triggers next prefetch/DMA)
        QuadBuffer_NotifyLayerComplete(qb, layer);
    }
    
    // Cleanup
    QuadBuffer_Destroy(qb);
*/

#ifdef __cplusplus
}
#endif

#endif // QUADBUFFER_DMA_H
