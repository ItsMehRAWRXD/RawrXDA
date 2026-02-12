// ============================================================================
// gguf_dml_bridge.h — Bridge: GGUF Tensor Loader → DirectML Inference Sessions
// ============================================================================
// Wires the existing GGUF loader chain (IGGUFLoader → StreamingGGUFLoader →
// EnhancedStreamingGGUFLoader) to the DirectML compute engine's session system.
//
// Supports:
//   - Full GGUF model load: iterate all tensors, dequantize, upload to VRAM
//   - Streaming layer load: load/unload individual transformer layers
//   - Dual-model management: load two models into separate DML sessions
//   - Model metadata extraction: read architecture config from GGUF header
//   - VRAM budget enforcement with automatic layer eviction
//
// Dependencies:
//   - directml_compute.h (DML session + tensor management)
//   - gguf_loader.h or streaming_gguf_loader.h (GGUF parsing)
//   - engine/common_types.h (block_q4_0, block_q8_0 structs)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_GGUF_DML_BRIDGE_H
#define RAWRXD_GGUF_DML_BRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <mutex>

// Forward declarations — no circular includes
struct ID3D12Resource;

namespace RawrXD {
namespace DML {

class DirectMLCompute;
struct DMLTensorBuffer;
struct DMLResult;
enum class TensorDataType : uint32_t;
enum class GGUFQuantType : uint32_t;

// ============================================================================
// GGUF Tensor Metadata (extracted from GGUF file for DML upload)
// ============================================================================
struct GGUFTensorMeta {
    const char* name;               // e.g. "blk.0.attn_q.weight"
    uint64_t nameHash;              // FNV-1a hash of name
    uint64_t fileOffset;            // Byte offset in GGUF file
    uint64_t rawSizeBytes;          // Size of raw (quantized) data
    uint64_t dequantSizeBytes;      // Size after dequantization to compute dtype
    uint32_t shape[8];              // Tensor dimensions
    uint32_t shapeDims;             // Number of dimensions
    uint32_t ggmlType;              // Original GGML quantization type
    int32_t layerIndex;             // -1 for non-layer tensors (embedding, output)
    bool isWeight;                  // true for .weight, false for .bias
    bool uploaded;                  // true if already in VRAM
};

// ============================================================================
// Model Architecture Config (parsed from GGUF header metadata)
// ============================================================================
struct GGUFModelConfig {
    char architecture[64];          // e.g. "qwen2", "llama"
    uint32_t numLayers;             // blk count
    uint32_t hiddenSize;            // embedding_length
    uint32_t intermediateSize;      // feed_forward_length
    uint32_t numHeads;              // attention.head_count
    uint32_t numKVHeads;            // attention.head_count_kv
    uint32_t headDim;               // hiddenSize / numHeads
    uint32_t vocabSize;             // vocab_size
    uint32_t maxSeqLen;             // context_length
    float rmsNormEps;               // attention.layer_norm_rms_epsilon
    float ropeTheta;                // rope.freq_base
    uint32_t ropeScalingType;       // rope.scaling.type (0=none, 1=linear, 2=yarn)
    float ropeScalingFactor;        // rope.scaling.factor
    uint64_t totalParamBytes;       // Total raw weight bytes
    uint64_t estimatedVRAM;         // Estimated VRAM needed (dequantized)
};

// ============================================================================
// Layer Management
// ============================================================================
struct LayerLoadState {
    int32_t layerIndex;
    bool loaded;
    uint64_t vramUsed;              // Bytes consumed in VRAM for this layer
    uint32_t tensorCount;           // Number of tensors in layer
    uint64_t lastAccessTick;        // For LRU eviction
};

// ============================================================================
// VRAM-to-System-Cache Spillover
// ============================================================================
// When a model exceeds GPU VRAM, tensors that don't fit are dequantized
// and cached in system RAM.  During forward pass the bridge will:
//   1. Upload spilled tensors on-demand before each layer's DML dispatch
//   2. Evict them from VRAM immediately after the layer completes
//   3. Prefetch the *next* layer's spilled tensors asynchronously
//
// This allows models up to 2× VRAM to run (at reduced throughput).
// ============================================================================

enum class TensorResidence : uint8_t {
    None         = 0,   // Not loaded anywhere
    VRAM         = 1,   // Resident in GPU VRAM (DML tensor buffer)
    SystemCache  = 2,   // Dequantized copy in pinned system RAM
    Both         = 3,   // Temporarily in both (during upload / prefetch)
};

struct SpilloverEntry {
    uint64_t nameHash;              // Matches GGUFTensorMeta::nameHash
    size_t   tensorIndex;           // Index into tensorMetas
    int32_t  layerIndex;            // Layer this tensor belongs to (-1 = fixed)
    uint64_t sizeBytes;             // Size of the dequantized data in system RAM
    uint8_t* hostBuffer;            // Pinned (VirtualAlloc) host memory
    TensorResidence residence;      // Current location
    uint64_t lastAccessTick;        // LRU tracking for host-cache eviction
};

struct SpilloverStats {
    uint64_t totalHostCacheBytes;   // Current system RAM used by spillover
    uint64_t peakHostCacheBytes;    // High-water mark
    uint64_t hostCacheBudget;       // Soft limit (0 = unlimited)
    uint64_t uploadCount;           // Number of host→GPU transfers
    uint64_t evictCount;            // Number of GPU→host evictions (spill events)
    uint64_t prefetchHits;          // Prefetch predicted correctly
    uint64_t prefetchMisses;        // Prefetch prediction wrong
    uint32_t layersSpilled;         // Layers with ≥1 tensor in system cache
    uint32_t tensorsInHostCache;    // Total tensors currently in system RAM
};

// ============================================================================
// Load Progress Callback
// ============================================================================
using LoadProgressCallback = void(*)(
    uint32_t currentTensor,
    uint32_t totalTensors,
    uint64_t bytesUploaded,
    uint64_t totalBytes,
    const char* tensorName,
    void* userData
);

// ============================================================================
// GGUFDMLBridge — Main Bridge Class
// ============================================================================
class GGUFDMLBridge {
    friend class DirectMLCompute;
public:
    GGUFDMLBridge();
    ~GGUFDMLBridge();

    // ---- Configuration ----
    void setDirectMLCompute(DirectMLCompute* dml);
    void setProgressCallback(LoadProgressCallback cb, void* userData = nullptr);
    void setComputeDataType(TensorDataType dtype);    // FP16 default
    void setMaxVRAMBudget(uint64_t bytes);            // Per-session VRAM limit
    void setDequantOnCPU(bool enable);                // CPU dequant before GPU upload
    void setStreamingEnabled(bool enable);             // Layer-by-layer loading
    void setLayerCacheCount(uint32_t count);           // Active layers in VRAM
    void setSpilloverEnabled(bool enable);              // VRAM→System-Cache spillover
    void setHostCacheBudget(uint64_t bytes);            // Max system RAM for spilled tensors

    // ---- Full Model Load ----

    // Open GGUF file, parse header, extract metadata
    DMLResult openModel(const char* ggufPath, uint32_t sessionId);

    // Read all tensors from GGUF, dequantize, upload to DML session
    // This is the "load everything" path for models that fit in VRAM.
    DMLResult loadAllTensors(uint32_t sessionId);

    // ---- Streaming Layer Load ----

    // Load a single transformer layer's tensors into VRAM
    DMLResult loadLayer(uint32_t sessionId, int32_t layerIndex);

    // Unload a layer's tensors from VRAM
    DMLResult unloadLayer(uint32_t sessionId, int32_t layerIndex);

    // Load embedding and output head tensors (always needed)
    DMLResult loadFixedTensors(uint32_t sessionId);

    // Auto-manage: load requested layer, evict LRU if over budget
    DMLResult ensureLayerLoaded(uint32_t sessionId, int32_t layerIndex);

    // ---- Dual-Model Management ----

    // Load a second model into a separate DML session
    DMLResult loadSecondModel(const char* ggufPath, uint32_t session2Id);

    // Get which models are loaded and their VRAM usage
    struct ModelSlot {
        uint32_t sessionId;
        char modelPath[512];
        char architecture[64];
        uint32_t numLayers;
        uint64_t vramUsed;
        uint64_t vramBudget;
        bool fullLoaded;
        uint32_t layersInVRAM;
    };
    bool getModelSlots(ModelSlot* slots, uint32_t maxSlots, uint32_t& outCount) const;

    // ---- Query ----
    const GGUFModelConfig& getModelConfig(uint32_t sessionId) const;
    uint32_t getTensorCount(uint32_t sessionId) const;
    uint64_t getVRAMUsed(uint32_t sessionId) const;
    uint64_t getVRAMBudget(uint32_t sessionId) const;
    int32_t getLoadedLayerCount(uint32_t sessionId) const;
    bool isLayerLoaded(uint32_t sessionId, int32_t layerIndex) const;
    std::string getDiagnostics() const;

    // ---- Spillover Query ----
    SpilloverStats getSpilloverStats(uint32_t sessionId) const;
    uint64_t getHostCacheUsed(uint32_t sessionId) const;
    bool isTensorSpilled(uint32_t sessionId, uint64_t nameHash) const;
    TensorResidence getTensorResidence(uint32_t sessionId, uint64_t nameHash) const;

    // ---- Spillover Operations ----

    // Ensure an entire layer is available for compute — uploads spilled tensors
    // from system cache to VRAM, evicting other layers' VRAM if necessary.
    // After the layer's compute is done, call releaseLayerVRAM() to spill back.
    DMLResult materializeLayerForCompute(uint32_t sessionId, int32_t layerIndex);

    // Release VRAM for a layer's tensors that have host-side copies (spill back).
    DMLResult releaseLayerVRAM(uint32_t sessionId, int32_t layerIndex);

    // Prefetch the next N layers' spilled tensors to VRAM asynchronously
    DMLResult prefetchLayers(uint32_t sessionId, int32_t startLayer, uint32_t count);

    // ---- Cleanup ----
    DMLResult closeModel(uint32_t sessionId);
    DMLResult closeAllModels();

private:
    // ---- Internal State per Session ----
    struct SessionState {
        uint32_t sessionId;
        std::string modelPath;
        GGUFModelConfig config;
        std::vector<GGUFTensorMeta> tensorMetas;
        std::unordered_map<uint64_t, size_t> tensorHashToIndex;    // nameHash → index in tensorMetas
        std::unordered_map<int32_t, LayerLoadState> layerStates;   // layerIdx → load state
        uint64_t vramUsed;
        uint64_t vramBudget;
        uint64_t accessTick;                // Monotonic counter for LRU
        bool fixedTensorsLoaded;            // embedding + output head

        // GGUF file handle
        void* fileHandle;                   // HANDLE (CreateFile)
        void* fileMapping;                  // HANDLE (CreateFileMapping)
        void* mappedBase;                   // void* (MapViewOfFile)
        uint64_t fileSize;

        // Dequant staging buffer (host-side)
        std::vector<uint8_t> stagingBuffer;

        // ---- VRAM-to-System-Cache Spillover State ----
        std::unordered_map<uint64_t, SpilloverEntry> spilloverCache; // nameHash → entry
        SpilloverStats spillStats;
        int32_t prefetchLayerHint;        // Next layer expected to be requested
    };

    // ---- Methods ----

    // Parse GGUF header and populate tensorMetas + config
    DMLResult parseGGUFHeader(SessionState& state);

    // Read metadata key-value pairs from GGUF
    DMLResult parseModelConfig(SessionState& state);

    // Upload a single tensor: read from mmap → dequant → upload to DML
    DMLResult uploadSingleTensor(SessionState& state, size_t tensorIndex);

    // CPU-side dequantization
    DMLResult dequantizeTensor(const void* srcData, void* dstData,
                               uint32_t ggmlType, uint64_t elementCount);

    // Compute the dequantized size for a tensor
    uint64_t computeDequantSize(uint32_t ggmlType, const uint32_t* shape,
                                 uint32_t shapeDims) const;

    // Evict least-recently-used layers until `freedBytes` worth of VRAM is free
    DMLResult evictLRULayers(SessionState& state, uint64_t freedBytes);

    // ---- Spillover internals ----

    // Dequantize tensor to a VirtualAlloc-pinned host buffer in spillover cache
    DMLResult spillTensorToHostCache(SessionState& state, size_t tensorIndex);

    // Upload a spilled tensor from host cache to VRAM (temporary, no mmap re-read)
    DMLResult uploadFromSpillover(SessionState& state, uint64_t nameHash);

    // Free host-side buffer for a spilled tensor
    void freeSpilloverEntry(SpilloverEntry& entry);

    // Evict least-recently-used host-cache entries until `freedBytes` is reclaimed
    DMLResult evictHostCacheLRU(SessionState& state, uint64_t freedBytes);

    // Try to load tensor: VRAM first, if budget exceeded spill to host cache
    DMLResult loadTensorWithSpillover(SessionState& state, size_t tensorIndex);

    // Map GGML type to DML TensorDataType
    TensorDataType ggmlTypeToTensorDataType(uint32_t ggmlType) const;

    // Map GGML type to GGUFQuantType
    GGUFQuantType ggmlTypeToQuantType(uint32_t ggmlType) const;

    // Compute FNV-1a hash of a tensor name
    uint64_t hashTensorName(const char* name) const;

    // Extract layer index from tensor name (e.g. "blk.5.attn_q.weight" → 5)
    int32_t extractLayerIndex(const char* name) const;

    // ---- State ----
    DirectMLCompute* m_dml = nullptr;
    std::unordered_map<uint32_t, SessionState> m_sessions;
    mutable std::mutex m_mutex;

    // Config
    TensorDataType m_computeType;           // Default: Float16
    uint64_t m_maxVRAMBudget;               // Per model
    bool m_dequantOnCPU;                    // CPU-side dequant
    bool m_streamingEnabled;                // Layer-by-layer
    uint32_t m_layerCacheCount;             // How many layers to keep in VRAM
    bool m_spilloverEnabled;                // VRAM→System-Cache spillover
    uint64_t m_hostCacheBudget;             // Soft limit on system RAM for spillover (0=unlimited)

    // Callbacks
    LoadProgressCallback m_progressCb = nullptr;
    void* m_progressUserData = nullptr;

    // Static default config
    static const GGUFModelConfig s_emptyConfig;
};

// ============================================================================
// External ASM function declarations
// ============================================================================
extern "C" {
    int64_t asm_dml_dequant_q4_0_to_fp32(float* dest, const uint8_t* src, uint64_t blockCount);
    int64_t asm_dml_dequant_q8_0_to_fp32(float* dest, const uint8_t* src, uint64_t blockCount);
    int64_t asm_dml_fast_memcpy_h2d(void* dest, const void* src, uint64_t byteCount);
    int64_t asm_dml_fast_memcpy_d2h(void* dest, const void* src, uint64_t byteCount);
    int64_t asm_dml_rope_rotate_fp32(float* qk, const float* cosTable,
                                      const float* sinTable, uint32_t halfDim,
                                      uint32_t seqLen);
    uint64_t asm_dml_compute_op_hash(const char* name, const uint32_t* params, uint32_t paramCount);
    int64_t asm_dml_prefetch_tensor_block(const void* address, uint64_t byteCount);
}

// ============================================================================
// Global accessor
// ============================================================================
GGUFDMLBridge& getGGUFDMLBridge();

} // namespace DML
} // namespace RawrXD

#endif // RAWRXD_GGUF_DML_BRIDGE_H
