// ============================================================================
// layer_offload_manager.hpp — RAM ↔ Working-Memory Layer Streaming
// ============================================================================
// Enables inference on models that exceed available VRAM (or working memory)
// by streaming transformer layers between disk/RAM and active compute buffers.
//
// Architecture:
//   - Models are divided into layer groups (zones)
//   - Only N layers are "resident" in working memory at a time
//   - A prefetch thread loads the next layer group while the current runs
//   - Double-buffered: while layer[i] computes, layer[i+1] loads
//   - Integrates with MemoryPressureHandler for adaptive eviction
//   - Integrates with LayerContributionScorer for skip decisions
//   - Q2_K dequantization happens on-load (decompress to FP32 working set)
//
// Supported quantization types for on-load dequant:
//   - Q2_K  (2.625 bpw — 256-element superblocks)
//   - Q4_0  (4.5 bpw — 32-element blocks)
//   - Q8_0  (8 bpw — 32-element blocks)
//   - F16   (16 bpw — direct half→float)
//   - F32   (32 bpw — passthrough)
//
// Design:
//   - PatchResult returns, no exceptions
//   - No std::function (function pointers only)
//   - Thread-safe with std::mutex
//   - Pre-allocated ping-pong buffers for zero-alloc streaming
//
// Usage with Pyre forward():
//   1. OffloadManager::initialize(model_path, config, vram_budget)
//   2. Before forwardTransformerLayer(i):
//        OffloadManager::ensureLayerResident(i)  ← blocks if not loaded yet
//        OffloadManager::prefetchLayer(i+1)      ← async
//   3. After layer completes:
//        OffloadManager::releaseLayer(i)          ← marks evictable
//
// Error model: PatchResult (no exceptions)
// Rule:        NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <string>
#include <unordered_map>

// Forward declarations
struct PatchResult;
struct PyreTensor;
#include "../engine/pyre_compute.h"  // PyreLayerConfig full definition (needed for by-value member)
enum class PyreDataType : uint32_t;

namespace RawrXD {

// ============================================================================
// Quantization format identifiers (matching GGML / Pyre types)
// ============================================================================
enum class QuantFormat : uint32_t {
    F32     = 0,
    F16     = 1,
    Q8_0    = 8,
    Q4_0    = 2,
    Q2_K    = 10,
    Unknown = 0xFF
};

// ============================================================================
// Layer Residency State
// ============================================================================
enum class LayerState : uint8_t {
    OnDisk      = 0,    // Not in RAM — must stream from file
    Loading     = 1,    // Currently being loaded by prefetch thread
    InRAM       = 2,    // Loaded into RAM buffer (compressed/quantized)
    Dequantized = 3,    // Dequantized into FP32 working buffer
    Active      = 4,    // Currently being computed on
    Evictable   = 5     // Computation done — can be evicted
};

const char* layerStateName(LayerState state);

// ============================================================================
// Layer Offload Entry — Metadata for a single transformer layer
// ============================================================================
struct LayerOffloadEntry {
    uint32_t    layerIndex;
    LayerState  state;
    QuantFormat quantType;

    // File offsets for each weight tensor in this layer
    // These are populated during model scan
    struct TensorSlice {
        char        name[128];      // Weight tensor name
        uint64_t    fileOffset;     // Byte offset in GGUF/Pyre file
        uint64_t    byteSize;       // Compressed/quantized size on disk
        uint64_t    dequantSize;    // FP32 decompressed size
        QuantFormat format;         // Per-tensor quant (may differ from layer default)
    };

    std::vector<TensorSlice>    tensors;
    uint64_t                    totalCompressedBytes;   // Sum of all tensor byteSize
    uint64_t                    totalDequantBytes;      // Sum of all FP32 sizes

    // RAM buffer (points into one of the ping-pong buffers)
    void*       ramBuffer;          // Raw loaded data (quantized)
    void*       workBuffer;         // Dequantized FP32 data
    uint64_t    ramBufferSize;
    uint64_t    workBufferSize;

    // Timing
    double      lastLoadTimeMs;
    double      lastDequantTimeMs;
    uint64_t    accessCount;

    // Layer contribution score (from LayerContributionScorer)
    float       contributionScore;  // 0.0 = skip, 1.0 = critical
    bool        skipRecommended;    // If true, skip this layer entirely

    LayerOffloadEntry()
        : layerIndex(0), state(LayerState::OnDisk)
        , quantType(QuantFormat::Unknown)
        , totalCompressedBytes(0), totalDequantBytes(0)
        , ramBuffer(nullptr), workBuffer(nullptr)
        , ramBufferSize(0), workBufferSize(0)
        , lastLoadTimeMs(0.0), lastDequantTimeMs(0.0)
        , accessCount(0), contributionScore(1.0f), skipRecommended(false)
    {}
};

// ============================================================================
// Offload Statistics
// ============================================================================
struct OffloadStats {
    uint64_t    totalLayerLoads;        // How many times a layer was loaded from disk
    uint64_t    totalLayerEvictions;    // How many times a layer was evicted
    uint64_t    prefetchHits;           // Layer was already loaded when needed
    uint64_t    prefetchMisses;         // Had to wait for layer load
    uint64_t    layersSkipped;          // Layers skipped via contribution scoring
    uint64_t    totalBytesStreamed;     // Cumulative bytes read from disk
    uint64_t    totalBytesDecompressed; // Cumulative FP32 bytes produced
    double      avgLoadTimeMs;          // Average per-layer load time
    double      avgDequantTimeMs;       // Average per-layer dequant time
    double      peakThroughputGBs;      // Peak streaming throughput
    uint64_t    currentResidentBytes;   // Currently loaded in RAM
    uint64_t    budgetBytes;            // VRAM/Working memory budget
    uint32_t    residentLayerCount;     // Number of layers currently in RAM
    uint32_t    totalLayerCount;        // Total layers in model
};

// ============================================================================
// Offload Configuration
// ============================================================================
struct OffloadConfig {
    uint64_t    vramBudgetBytes;        // Max working memory for layers (default: 14GB for 16GB card)
    uint32_t    prefetchAhead;          // How many layers to prefetch ahead (default: 2)
    uint32_t    maxResidentLayers;      // Max layers in RAM at once (0 = auto-calculate)
    bool        enableSkipping;         // Allow layer skipping via contribution scores
    float       skipThreshold;          // Skip layers with contribution < this (default: 0.05)
    bool        enableDoubleBuffer;     // Use ping-pong buffers (default: true)
    bool        asyncPrefetch;          // Background thread for prefetch (default: true)
    uint32_t    ioThreadCount;          // Number of I/O threads for loading (default: 2)

    static OffloadConfig defaultConfig() {
        OffloadConfig c{};
        c.vramBudgetBytes = 14ULL * 1024 * 1024 * 1024;  // 14 GB (leave 2GB for KV cache + overhead)
        c.prefetchAhead = 2;
        c.maxResidentLayers = 0;  // auto
        c.enableSkipping = true;
        c.skipThreshold = 0.05f;
        c.enableDoubleBuffer = true;
        c.asyncPrefetch = true;
        c.ioThreadCount = 2;
        return c;
    }

    // Preset for 16GB VRAM (RX 7800 XT)
    static OffloadConfig rx7800xt() {
        OffloadConfig c = defaultConfig();
        c.vramBudgetBytes = 14ULL * 1024 * 1024 * 1024;  // 14 GB
        c.prefetchAhead = 2;
        return c;
    }

    // Preset for RAM-only inference (no VRAM — pure CPU/DDR5)
    static OffloadConfig ramOnly64GB() {
        OffloadConfig c = defaultConfig();
        c.vramBudgetBytes = 48ULL * 1024 * 1024 * 1024;  // 48 GB (leave 16GB for OS + KV cache)
        c.prefetchAhead = 4;
        c.maxResidentLayers = 8;  // Keep up to 8 layers warm
        c.ioThreadCount = 4;
        return c;
    }
};

// ============================================================================
// Q2_K Dequantization Context — Constants and scratch buffers
// ============================================================================
struct Q2KDequantContext {
    static constexpr uint32_t SUPERBLOCK_SIZE = 256;
    static constexpr uint32_t SUB_BLOCK_SIZE = 16;
    static constexpr uint32_t N_SUB_BLOCKS = SUPERBLOCK_SIZE / SUB_BLOCK_SIZE;
    // Superblock layout: f16 scale (2) + f16 min (2) + 16 scale/min bytes (16) + 64 quant bytes (64) = 84
    static constexpr uint32_t SUPERBLOCK_BYTES = 84;

    // Scratch buffers (pre-allocated, reused across dequant calls)
    float*      scratchFP32;        // [SUPERBLOCK_SIZE] temporary
    uint32_t    scratchSize;

    Q2KDequantContext() : scratchFP32(nullptr), scratchSize(0) {}
};

// ============================================================================
// LayerOffloadManager — The Central Manager
// ============================================================================
class LayerOffloadManager {
public:
    LayerOffloadManager();
    ~LayerOffloadManager();

    // Non-copyable
    LayerOffloadManager(const LayerOffloadManager&) = delete;
    LayerOffloadManager& operator=(const LayerOffloadManager&) = delete;

    // ---- Initialization ----
    // Initialize with a GGUF/Pyre model file and configuration
    PatchResult initialize(const char* modelPath, const PyreLayerConfig& config,
                           const OffloadConfig& offloadConfig = OffloadConfig::defaultConfig());

    // Scan model file and build layer→tensor index
    PatchResult scanModelLayers();

    // Pre-allocate all working buffers
    PatchResult allocateBuffers();

    // ---- Layer Lifecycle (called from forward pass) ----

    // Ensure a layer's weights are dequantized and ready for compute.
    // Blocks if the layer is still being loaded.
    PatchResult ensureLayerResident(uint32_t layerIndex);

    // Start asynchronous prefetch of a layer.
    // Returns immediately. Use ensureLayerResident() to wait.
    PatchResult prefetchLayer(uint32_t layerIndex);

    // Mark a layer as done — eligible for eviction.
    PatchResult releaseLayer(uint32_t layerIndex);

    // ---- Bulk Operations ----

    // Prefetch a range of layers [start, start+count)
    PatchResult prefetchRange(uint32_t startLayer, uint32_t count);

    // Evict all layers except the given range
    PatchResult evictExcept(uint32_t keepStart, uint32_t keepCount);

    // Evict all layers (free all RAM)
    PatchResult evictAll();

    // ---- Weight Access (for Pyre forward pass) ----

    // Get a dequantized FP32 pointer to a specific weight tensor in a layer.
    // Returns nullptr if layer is not resident.
    float* getLayerWeight(uint32_t layerIndex, const char* tensorName);

    // Get the dequantized weight tensor size in bytes
    uint64_t getLayerWeightSize(uint32_t layerIndex, const char* tensorName);

    // ---- Q2_K Dequantization ----

    // Dequantize a Q2_K buffer to FP32 in-place
    static PatchResult dequantQ2K(const void* src, float* dst, uint64_t numElements);

    // Dequantize Q4_0 buffer to FP32
    static PatchResult dequantQ4_0(const void* src, float* dst, uint64_t numElements);

    // Dequantize Q8_0 buffer to FP32
    static PatchResult dequantQ8_0(const void* src, float* dst, uint64_t numElements);

    // Dequantize F16 buffer to FP32
    static PatchResult dequantF16(const void* src, float* dst, uint64_t numElements);

    // Generic dequant dispatcher
    static PatchResult dequantize(const void* src, float* dst, uint64_t numElements,
                                  QuantFormat format);

    // ---- Query ----
    LayerState getLayerState(uint32_t layerIndex) const;
    const LayerOffloadEntry* getLayerEntry(uint32_t layerIndex) const;
    OffloadStats getStats() const;
    bool isInitialized() const { return m_initialized; }
    uint32_t getLayerCount() const { return m_layerCount; }
    uint32_t getResidentCount() const;

    // ---- Layer Skipping Integration ----
    void setLayerContributionScore(uint32_t layerIndex, float score);
    bool shouldSkipLayer(uint32_t layerIndex) const;

    // ---- Diagnostics ----
    size_t dumpDiagnostics(char* buffer, size_t bufferSize) const;

    // ---- Singleton ----
    static LayerOffloadManager& instance();

private:
    // ---- Prefetch Thread ----
    void prefetchWorkerThread();
    PatchResult loadLayerFromDisk(uint32_t layerIndex);
    PatchResult dequantLayer(uint32_t layerIndex);
    void evictLeastRecentLayer();

    // ---- Memory-Mapped File Access ----
    HANDLE          m_hFile;
    HANDLE          m_hMapping;
    void*           m_pMappedView;
    uint64_t        m_mappedSize;

    // ---- Layer Database ----
    std::vector<LayerOffloadEntry>  m_layers;
    uint32_t                        m_layerCount;
    bool                            m_initialized;

    // ---- Configuration ----
    OffloadConfig                   m_config;
    PyreLayerConfig                 m_modelConfig;  // Copy of model arch config
    std::string                     m_modelPath;

    // ---- Working Buffers (ping-pong) ----
    // Two large buffers: while one is being computed on, the other loads
    void*           m_bufferA;
    void*           m_bufferB;
    uint64_t        m_bufferSize;        // Size of each buffer
    bool            m_activeBufferIsA;   // Which buffer is currently "active"

    // ---- FP32 Layer Weight Storage ----
    // Maps layerIndex → {tensorName → FP32 pointer}
    struct DequantTensorEntry {
        float*      data;
        uint64_t    sizeBytes;
    };
    std::unordered_map<uint32_t, std::unordered_map<std::string, DequantTensorEntry>> m_dequantWeights;

    // ---- Prefetch Thread State ----
    std::thread                     m_prefetchThread;
    std::mutex                      m_mutex;
    std::condition_variable         m_prefetchCV;
    std::atomic<bool>               m_shutdownRequested{false};
    std::vector<uint32_t>           m_prefetchQueue;

    // ---- LRU Tracking ----
    std::vector<uint64_t>           m_accessTimestamps;  // Per-layer access tick
    uint64_t                        m_globalTick{0};

    // ---- Statistics ----
    mutable std::mutex              m_statsMutex;
    OffloadStats                    m_stats;

    // ---- Q2_K Context ----
    Q2KDequantContext               m_q2kContext;
};

} // namespace RawrXD
