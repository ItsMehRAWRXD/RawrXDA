// ============================================================================
// universal_model_hotpatcher.h — Phase B: Universal Model Hotpatcher
// ============================================================================
// 120B-800B parameter support via streaming quantization.
// Decision tree selects Q2_K/Q4_K/Q8_0 per layer based on VRAM pressure.
// Autonomous model surgery: re-quantize individual layers at runtime.
//
// Architecture:
//   1. VRAMPressureMonitor — Tracks real-time VRAM/RAM usage
//   2. LayerQuantDecisionTree — Per-layer quantization selection
//   3. StreamingQuantizer — Applies quantization without full model reload
//   4. ModelSurgeryEngine — Hot-swap layers, merge/split tensors
//
// Integrations:
//   - StreamingEngineRegistry (src/core/streaming_engine_registry.h)
//   - GPUBackendBridge (src/core/gpu_backend_bridge.h)
//   - AMDGPUAccelerator (src/core/amd_gpu_accelerator.h)
//   - UnifiedHotpatchManager (src/core/unified_hotpatch_manager.hpp)
//   - ByteLevelHotpatcher (src/core/byte_level_hotpatcher.hpp)
//
// Supports: Q2_K, Q3_K_S, Q3_K_M, Q3_K_L, Q4_K_S, Q4_K_M, Q5_K_S,
//           Q5_K_M, Q6_K, Q8_0, F16, F32
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded, with streaming pipeline for zero-copy quant.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <chrono>

// Forward declarations
struct PatchResult;

// ============================================================================
// Quantization Types (matches GGML quant types)
// ============================================================================
enum class QuantType : uint32_t {
    F32     = 0,
    F16     = 1,
    Q4_0    = 2,
    Q4_1    = 3,
    Q5_0    = 6,
    Q5_1    = 7,
    Q8_0    = 8,
    Q8_1    = 9,
    Q2_K    = 10,
    Q3_K_S  = 11,
    Q3_K_M  = 12,
    Q3_K_L  = 13,
    Q4_K_S  = 14,
    Q4_K_M  = 15,
    Q5_K_S  = 16,
    Q5_K_M  = 17,
    Q6_K    = 18,
    IQ2_XXS = 19,
    IQ2_XS  = 20,
    IQ3_XXS = 21,
    IQ1_S   = 22,
    IQ4_NL  = 23,
    IQ3_S   = 24,
    IQ2_S   = 25,
    IQ4_XS  = 26,
};

// ============================================================================
// VRAM Pressure Level
// ============================================================================
enum class VRAMPressure : uint8_t {
    Low         = 0,    // < 50% VRAM used — can use F16/Q8_0
    Normal      = 1,    // 50-70% VRAM — use Q5_K_M or Q6_K
    High        = 2,    // 70-85% VRAM — use Q4_K_M or Q4_K_S
    Critical    = 3,    // 85-95% VRAM — use Q3_K_M or Q2_K
    Emergency   = 4,    // > 95% VRAM — force Q2_K, evict non-essential layers
};

// ============================================================================
// Layer Importance Classification
// ============================================================================
enum class LayerImportance : uint8_t {
    Critical    = 0,    // Attention heads, output projection — keep high quant
    High        = 1,    // MLP layers, value projections — moderate quant OK
    Medium      = 2,    // Key projections, layer norms — can quantize more
    Low         = 3,    // Embedding layers, padding — aggressive quant OK
    Expendable  = 4,    // Duplicate/redundant layers — can be evicted
};

// ============================================================================
// Layer Quantization Decision
// ============================================================================
struct LayerQuantDecision {
    uint32_t        layerIndex;
    std::string     layerName;
    QuantType       currentQuant;
    QuantType       targetQuant;
    LayerImportance importance;
    uint64_t        currentSizeBytes;
    uint64_t        targetSizeBytes;
    int64_t         savingsBytes;       // Positive = saves memory
    float           qualityImpact;      // 0.0 = no impact, 1.0 = severe degradation
    bool            approved;           // Whether auto-approved or needs user consent
};

// ============================================================================
// VRAM Budget Snapshot
// ============================================================================
struct VRAMBudget {
    uint64_t    totalVRAM;          // Total GPU VRAM
    uint64_t    usedVRAM;           // Currently used VRAM
    uint64_t    availableVRAM;      // Free VRAM
    uint64_t    totalRAM;           // Total system RAM
    uint64_t    usedRAM;            // Used system RAM
    uint64_t    availableRAM;       // Free system RAM
    uint64_t    modelSizeOnDisk;    // Model file size
    uint64_t    modelSizeInMemory;  // Loaded model memory footprint
    VRAMPressure pressure;          // Current pressure level
    float       utilizationPercent; // 0.0 - 100.0
    bool        gpuAvailable;       // Whether GPU is usable
    bool        gpuAccelEnabled;    // Whether GPU toggle is ON
};

// ============================================================================
// Model Surgery Operation
// ============================================================================
enum class SurgeryOp : uint8_t {
    RequantizeLayer     = 0,    // Change quantization of a single layer
    RequantizeRange     = 1,    // Change quantization of layer range
    RequantizeAll       = 2,    // Full model re-quantization
    EvictLayer          = 3,    // Unload a layer from memory
    ReloadLayer         = 4,    // Reload an evicted layer
    SplitLayer          = 5,    // Split a layer across GPU/CPU
    MergeShards         = 6,    // Merge sharded model fragments
    CompressKVCache     = 7,    // Compress KV cache for long context
};

// ============================================================================
// Surgery Result
// ============================================================================
struct SurgeryResult {
    bool        success;
    SurgeryOp   operation;
    uint32_t    layersAffected;
    int64_t     memorySavedBytes;
    float       qualityImpactEstimate;
    uint32_t    durationMs;
    const char* detail;

    static SurgeryResult ok(SurgeryOp op, uint32_t layers, int64_t saved, const char* msg) {
        SurgeryResult r;
        r.success = true;
        r.operation = op;
        r.layersAffected = layers;
        r.memorySavedBytes = saved;
        r.qualityImpactEstimate = 0.0f;
        r.durationMs = 0;
        r.detail = msg;
        return r;
    }

    static SurgeryResult error(SurgeryOp op, const char* msg) {
        SurgeryResult r;
        r.success = false;
        r.operation = op;
        r.layersAffected = 0;
        r.memorySavedBytes = 0;
        r.qualityImpactEstimate = 0.0f;
        r.durationMs = 0;
        r.detail = msg;
        return r;
    }
};

// ============================================================================
// Model Layer Metadata
// ============================================================================
struct ModelLayerInfo {
    uint32_t    index;
    std::string name;               // e.g., "blk.0.attn_q.weight"
    QuantType   quantType;
    uint64_t    sizeBytes;
    uint64_t    elementCount;
    uint32_t    dimensions[4];      // Tensor shape
    uint32_t    ndims;
    LayerImportance importance;
    bool        loadedInVRAM;
    bool        loadedInRAM;
    bool        evicted;
};

// ============================================================================
// Hotpatcher Statistics
// ============================================================================
struct ModelHotpatcherStats {
    std::atomic<uint64_t> layersAnalyzed{0};
    std::atomic<uint64_t> layersRequantized{0};
    std::atomic<uint64_t> layersEvicted{0};
    std::atomic<uint64_t> layersReloaded{0};
    std::atomic<uint64_t> totalMemorySaved{0};
    std::atomic<uint64_t> totalSurgeries{0};
    std::atomic<uint64_t> autoDecisions{0};
    std::atomic<uint64_t> pressureEvents{0};
    std::atomic<uint64_t> emergencyEvictions{0};
};

// ============================================================================
// Callbacks
// ============================================================================
typedef void (*VRAMPressureCallback)(VRAMPressure level, const VRAMBudget* budget, void* userData);
typedef void (*SurgeryProgressCallback)(SurgeryOp op, uint32_t layerIndex,
                                         uint32_t totalLayers, float progress, void* userData);

// ============================================================================
// UniversalModelHotpatcher — Main Class
// ============================================================================
class UniversalModelHotpatcher {
public:
    static UniversalModelHotpatcher& instance();

    // ---- Lifecycle ----
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_relaxed); }

    // ---- VRAM Pressure Monitoring ----
    
    // Get current VRAM budget snapshot.
    VRAMBudget getVRAMBudget() const;

    // Get current pressure level.
    VRAMPressure getCurrentPressure() const;

    // Set VRAM pressure thresholds (percent).
    void setPressureThresholds(float high, float critical, float emergency);

    // Enable/disable GPU acceleration toggle.
    void setGPUAccelEnabled(bool enabled);
    bool isGPUAccelEnabled() const { return m_gpuAccelEnabled.load(); }

    // ---- Model Analysis ----
    
    // Scan loaded model and classify layer importance.
    bool analyzeModel(const std::string& modelPath);

    // Get metadata for all layers.
    std::vector<ModelLayerInfo> getLayerInfo() const;

    // Get metadata for a specific layer.
    bool getLayerInfo(uint32_t layerIndex, ModelLayerInfo& outInfo) const;

    // Get total parameter count estimate.
    uint64_t estimateParameterCount() const;

    // Get model size tier.
    uint32_t getModelSizeTier() const; // Returns ModelSizeTier enum value

    // ---- Decision Tree: Per-Layer Quantization ----

    // Run the decision tree to determine optimal quantization per layer.
    // This is the core intelligence: it considers VRAM pressure, layer importance,
    // and quality tradeoffs to produce a quantization plan.
    std::vector<LayerQuantDecision> computeQuantPlan();

    // Compute plan with explicit VRAM target (bytes).
    std::vector<LayerQuantDecision> computeQuantPlanForTarget(uint64_t targetVRAMBytes);

    // Get the recommended QuantType for a layer given current pressure.
    QuantType recommendQuantForLayer(uint32_t layerIndex) const;

    // Override: force a specific quant type for a layer range.
    void overrideQuantRange(uint32_t startLayer, uint32_t endLayer, QuantType quant);

    // ---- Streaming Quantization (Hot-Swap) ----

    // Apply a quantization plan without reloading the model.
    // Layers are re-quantized in-place via streaming pipeline.
    SurgeryResult applyQuantPlan(const std::vector<LayerQuantDecision>& plan);

    // Re-quantize a single layer in-place.
    SurgeryResult requantizeLayer(uint32_t layerIndex, QuantType targetQuant);

    // Re-quantize a range of layers.
    SurgeryResult requantizeRange(uint32_t startLayer, uint32_t endLayer, QuantType targetQuant);

    // Full model re-quantization (streaming, not full reload).
    SurgeryResult requantizeAll(QuantType targetQuant);

    // ---- Autonomous Model Surgery ----

    // Evict a layer from memory (keeps metadata for reload).
    SurgeryResult evictLayer(uint32_t layerIndex);

    // Reload a previously evicted layer.
    SurgeryResult reloadLayer(uint32_t layerIndex);

    // Split a layer across GPU (hot) and CPU (cold) memory.
    SurgeryResult splitLayerGPUCPU(uint32_t layerIndex, float gpuFraction);

    // Merge sharded model fragments into coherent memory.
    SurgeryResult mergeShards(const std::vector<std::string>& shardPaths);

    // Compress KV cache for long-context inference.
    SurgeryResult compressKVCache(float targetRatio);

    // ---- Autonomous Pressure Response ----

    // Enable automatic pressure response (runs decision tree on pressure events).
    void enableAutoPressureResponse(bool enable);
    bool isAutoPressureResponseEnabled() const;

    // Manually trigger a pressure response cycle.
    SurgeryResult triggerPressureResponse();

    // ---- Callbacks ----
    void setPressureCallback(VRAMPressureCallback cb, void* userData);
    void setSurgeryProgressCallback(SurgeryProgressCallback cb, void* userData);

    // ---- Statistics ----
    const ModelHotpatcherStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string quantPlanToJson(const std::vector<LayerQuantDecision>& plan) const;
    std::string layersToJson() const;

private:
    UniversalModelHotpatcher();
    ~UniversalModelHotpatcher();
    UniversalModelHotpatcher(const UniversalModelHotpatcher&) = delete;
    UniversalModelHotpatcher& operator=(const UniversalModelHotpatcher&) = delete;

    // Internal: Layer importance classification heuristics
    LayerImportance classifyLayer(const std::string& layerName, uint32_t layerIndex,
                                   uint32_t totalLayers) const;

    // Internal: Quality impact estimation for re-quantization
    float estimateQualityImpact(QuantType from, QuantType to,
                                 LayerImportance importance) const;

    // Internal: Size calculation for different quant types
    uint64_t estimateQuantizedSize(uint64_t elementCount, QuantType quant) const;

    // Internal: Bits-per-weight for each quant type
    float bitsPerWeight(QuantType quant) const;

    // Internal: VRAM pressure monitoring thread
    static DWORD WINAPI pressureMonitorThread(LPVOID param);
    void monitorPressure();

    // Internal: Streaming re-quantization pipeline
    bool streamRequantize(uint32_t layerIndex, QuantType from, QuantType to,
                          void* srcData, void* dstData, uint64_t elementCount);

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                      m_mutex;
    std::atomic<bool>                       m_initialized;
    std::atomic<bool>                       m_gpuAccelEnabled;
    std::atomic<bool>                       m_autoPressureResponse;
    std::atomic<bool>                       m_shutdownRequested;

    // Model layer metadata
    std::vector<ModelLayerInfo>             m_layers;
    std::string                             m_modelPath;
    uint64_t                                m_totalParams;

    // VRAM pressure state
    VRAMBudget                              m_lastBudget;
    float                                   m_thresholdHigh;
    float                                   m_thresholdCritical;
    float                                   m_thresholdEmergency;

    // Quantization overrides
    std::unordered_map<uint32_t, QuantType> m_quantOverrides;

    // Statistics
    ModelHotpatcherStats                    m_stats;

    // Callbacks
    VRAMPressureCallback                    m_pressureCb;
    void*                                   m_pressureUserData;
    SurgeryProgressCallback                 m_surgeryCb;
    void*                                   m_surgeryUserData;

    // Pressure monitor thread
    HANDLE                                  m_hPressureThread;
};
