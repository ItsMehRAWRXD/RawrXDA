// ============================================================================
// execution_scheduler.h — Phase 9.2: Layer Execution Scheduler
// ============================================================================
// Deterministic layer-by-layer execution spine for transformer inference.
// Manages tensor lifecycle (pin → dequant → execute → release) and schedules
// async prefetch of layer[i+1] while layer[i] executes.
//
// Architecture:
//   GenerateStreaming() → ExecutionScheduler::RunForward()
//     → for each layer:
//         1. Prefetch(layer+1) [async, overlapped]
//         2. Pin(layer)        [ensure in RAM]
//         3. Dequant(layer)    [K-quant → float32]
//         4. Execute(layer)    [TransformerLayer]
//         5. Release(layer)    [unpin, allow eviction]
//
// Integration:
//   - StreamingEngineRegistry: tensor streaming / pinning / eviction
//   - CPUInferenceEngine: dequant kernels + TransformerLayer
//   - QuadBuffer DMA: COLD→WARM→HOT state transitions
//
// Threading: Single execution thread + one prefetch thread.
// No exceptions. PatchResult-style error returns.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>

namespace RawrXD {

// ============================================================================
// Forward declarations
// ============================================================================
class CPUInferenceEngine;
class StreamingEngineRegistry;

// ============================================================================
// Tensor Residency State (matches QuadBuffer block states)
// ============================================================================
enum class TensorState : uint8_t {
    Cold        = 0,    // On disk only
    Warm        = 1,    // Compressed in system RAM
    Hot         = 2,    // Decompressed in RAM, ready for compute
    Pinned      = 3,    // Hot + pinned (will not be evicted)
    Released    = 4,    // Unpinned, eligible for eviction
};

// ============================================================================
// Tensor Slot — tracks one tensor through its lifecycle
// ============================================================================
struct TensorSlot {
    std::string         name;           // e.g., "blk.0.attn_q.weight"
    uint64_t            nameHash;       // FNV1a of name (for QuadBuffer lookup)
    TensorState         state;
    int                 layerIndex;     // -1 for global tensors (embeddings, norms)
    uint64_t            sizeBytes;      // Raw (quantized) size on disk
    uint64_t            dequantBytes;   // Dequantized F32 size
    uint32_t            quantType;      // ggml_type
    int32_t             refCount;       // Pin reference count
    uint64_t            lastAccessTick; // For LRU eviction
    
    // Timing telemetry
    double              lastPrefetchMs;
    double              lastDequantMs;
    double              lastExecMs;
};

// ============================================================================
// Layer Manifest — all tensors needed by one transformer layer
// ============================================================================
struct LayerManifest {
    int                 layerIndex;
    
    // Attention tensors
    std::string         attn_q;         // "blk.N.attn_q.weight"
    std::string         attn_k;         // "blk.N.attn_k.weight"
    std::string         attn_v;         // "blk.N.attn_v.weight"
    std::string         attn_output;    // "blk.N.attn_output.weight"
    std::string         attn_norm;      // "blk.N.attn_norm.weight"
    
    // FFN tensors
    std::string         ffn_gate;       // "blk.N.ffn_gate.weight"
    std::string         ffn_up;         // "blk.N.ffn_up.weight"
    std::string         ffn_down;       // "blk.N.ffn_down.weight"
    std::string         ffn_norm;       // "blk.N.ffn_norm.weight"
    
    // Computed stats
    uint64_t            totalRawBytes;      // Sum of all tensor sizes (quantized)
    uint64_t            totalDequantBytes;  // Sum of all F32 outputs
    double              estimatedExecMs;    // From historical telemetry
    bool                prefetched;         // True if async prefetch completed
};

// ============================================================================
// Execution Stats — per-run telemetry
// ============================================================================
struct ExecutionStats {
    // Timing
    double              totalForwardMs;
    double              totalPrefetchMs;
    double              totalDequantMs;
    double              totalComputeMs;
    double              totalEvictionMs;
    
    // Throughput
    uint64_t            tokensGenerated;
    double              tokensPerSecond;
    
    // Memory
    uint64_t            peakRAMUsage;
    uint64_t            peakPinnedBytes;
    uint64_t            totalBytesStreamed;
    uint64_t            totalBytesEvicted;
    
    // Cache performance
    uint64_t            prefetchHits;       // layer[i+1] was ready when needed
    uint64_t            prefetchMisses;     // Had to wait for prefetch
    uint64_t            evictionCount;
    
    // Layer-level breakdown
    std::vector<double> layerExecMs;        // Per-layer execution time
    std::vector<double> layerPrefetchMs;    // Per-layer prefetch time
    std::vector<double> layerDequantMs;     // Per-layer dequant time
};

// ============================================================================
// Prefetch Request — queued for the prefetch thread
// ============================================================================
struct PrefetchRequest {
    int         layerIndex;
    uint64_t    priority;       // Lower = higher priority
    bool        completed;
    bool        cancelled;
};

// ============================================================================
// Scheduler Configuration
// ============================================================================
struct SchedulerConfig {
    // Prefetch
    int         prefetchAhead;          // How many layers to prefetch (default: 1)
    bool        enableAsyncPrefetch;    // Use background thread (default: true)
    uint64_t    prefetchTimeoutMs;      // Max wait for prefetch (default: 5000)
    
    // Memory budget
    uint64_t    maxPinnedBytes;         // Max bytes pinned simultaneously
    uint64_t    evictionThreshold;      // Trigger eviction when pinned > this
    
    // Telemetry
    bool        enableTelemetry;        // Collect per-layer timing (default: true)
    bool        enablePrefetchHinting;  // Log prefetch hit/miss (default: true)
    
    // Threading
    int         computeThreads;         // Threads for MatMul/attention (default: hw cores)
    
    SchedulerConfig()
        : prefetchAhead(1)
        , enableAsyncPrefetch(true)
        , prefetchTimeoutMs(5000)
        , maxPinnedBytes(4ULL * 1024 * 1024 * 1024)  // 4GB default
        , evictionThreshold(3ULL * 1024 * 1024 * 1024) // 3GB trigger
        , enableTelemetry(true)
        , enablePrefetchHinting(true)
        , computeThreads(0)             // 0 = auto-detect
    {}
};

// ============================================================================
// ExecutionScheduler — The Layer Execution Spine
// ============================================================================
class ExecutionScheduler {
public:
    ExecutionScheduler();
    ~ExecutionScheduler();

    // ---- Configuration ----
    void configure(const SchedulerConfig& config);
    SchedulerConfig getConfig() const;
    
    // ---- Initialization ----
    // Bind to the inference engine and streaming registry.
    // Must be called before RunForward.
    bool bind(CPUInferenceEngine* engine, StreamingEngineRegistry* registry);
    
    // Build layer manifests from model metadata.
    // Called once after model load.
    bool buildManifests(int numLayers, int embeddingDim,
                        const std::unordered_map<std::string, TensorSlot>& tensorMap);
    
    // ---- Execution ----
    // Run full forward pass for one token position.
    // state: [embeddingDim] float array, modified in-place.
    // Returns true on success.
    bool runForwardPass(float* state, float* scratch, int seqPos);
    
    // Run a single transformer layer with scheduling.
    // Handles: prefetch(layer+1) → pin(layer) → dequant → execute → release
    bool runScheduledLayer(float* state, float* scratch, int layerIdx, int seqPos);
    
    // ---- Prefetch Control ----
    // Request async prefetch of a layer's tensors
    void requestPrefetch(int layerIdx);
    
    // Wait for a prefetch to complete (blocking)
    bool awaitPrefetch(int layerIdx, uint64_t timeoutMs);
    
    // Cancel pending prefetch
    void cancelPrefetch(int layerIdx);
    
    // ---- Tensor Lifecycle ----
    // Pin a tensor (increment refcount, ensure Hot state)
    bool pinTensor(const std::string& name);
    
    // Release a tensor (decrement refcount, allow eviction)
    void releaseTensor(const std::string& name);
    
    // Pin all tensors for a layer
    bool pinLayer(int layerIdx);
    
    // Release all tensors for a layer
    void releaseLayer(int layerIdx);
    
    // ---- Eviction ----
    // Evict tensors until pinnedBytes < target
    uint64_t evictToTarget(uint64_t targetBytes);
    
    // Force evict all unpinned tensors
    uint64_t evictAll();
    
    // ---- Telemetry ----
    ExecutionStats getStats() const;
    void resetStats();
    
    // Per-layer timing (for HUD display)
    double getLayerExecMs(int layerIdx) const;
    double getLayerPrefetchMs(int layerIdx) const;
    
    // ---- Diagnostics ----
    std::string getDiagnostics() const;
    std::string getMemoryMap() const;
    std::string getPrefetchStatus() const;
    
    // ---- Shutdown ----
    void shutdown();
    
private:
    // ---- Internal: Prefetch Thread ----
    void prefetchThreadFunc();
    void startPrefetchThread();
    void stopPrefetchThread();
    
    // ---- Internal: Tensor Loading ----
    // Load tensor data from streaming engine into slot
    bool loadTensorToHot(TensorSlot& slot);
    
    // Dequantize a tensor in-place (quantized bytes → float32)
    bool dequantTensorToFloat(const TensorSlot& slot, 
                              const uint8_t* src, float* dst, size_t numElements);
    
    // ---- Internal: Layer Manifest Helpers ----
    void buildLayerManifest(int layerIdx, const std::string& prefix);
    std::vector<std::string> getLayerTensorNames(int layerIdx) const;
    
    // ---- Internal: Memory Accounting ----
    void accountPin(uint64_t bytes);
    void accountRelease(uint64_t bytes);
    bool isOverBudget() const;
    
    // ---- Internal: Timing ----
    double nowMs() const;
    
    // ---- State ----
    CPUInferenceEngine*         m_engine;
    StreamingEngineRegistry*    m_registry;
    SchedulerConfig             m_config;
    
    // Layer manifests (indexed by layer)
    std::vector<LayerManifest>  m_manifests;
    int                         m_numLayers;
    int                         m_embeddingDim;
    
    // Tensor tracking
    std::unordered_map<std::string, TensorSlot> m_tensorSlots;
    mutable std::mutex          m_slotMutex;
    
    // Memory accounting
    std::atomic<uint64_t>       m_pinnedBytes;
    std::atomic<uint64_t>       m_totalStreamed;
    
    // Prefetch thread
    std::thread                 m_prefetchThread;
    std::mutex                  m_prefetchMutex;
    std::condition_variable     m_prefetchCV;
    std::vector<PrefetchRequest> m_prefetchQueue;
    std::atomic<bool>           m_prefetchRunning;
    std::atomic<bool>           m_shutdownRequested;
    
    // Per-layer prefetch completion flags
    std::vector<std::atomic<bool>> m_prefetchComplete;
    std::mutex                  m_prefetchCompleteMutex;
    std::condition_variable     m_prefetchCompleteCV;
    
    // Telemetry
    ExecutionStats              m_stats;
    mutable std::mutex          m_statsMutex;
    
    // Monotonic tick counter for LRU
    std::atomic<uint64_t>       m_tickCounter;
    
    // Bind state
    bool                        m_bound;
    bool                        m_manifestsBuilt;
};

// ============================================================================
// Global accessor
// ============================================================================
ExecutionScheduler& getExecutionScheduler();

} // namespace RawrXD
