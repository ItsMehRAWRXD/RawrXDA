// ============================================================================
// streaming_engine_registry.h — Phase 9: Selectable ASM Engine Registry
// ============================================================================
// Central registry for all MASM streaming engines in RawrXD.
// Each ASM engine exports a descriptor; this registry discovers them,
// validates capabilities, and selects the optimal engine based on:
//   - Model size (120B, 800B, 7B, etc.)
//   - Model format (GGUF, Safetensors, Blob)
//   - Available hardware (VRAM, RAM, CPU features)
//   - User preference (explicit override)
//
// Engines registered:
//   - QuadBuffer-DMA-120B       (RawrXD_QuadBuffer_Streamer.asm)
//   - Rawr1024-DualEngine       (rawr1024_dual_engine.asm)
//   - DualEngineStreamer         (RawrXD_DualEngineStreamer.asm)
//   - DualEngineManager          (RawrXD_DualEngineManager.asm)
//   - FlashAttention-AVX2       (flash_attn_asm_avx2.asm)
//   - InferenceKernels-AVX512   (inference_kernels.asm)
//   - GPU-Backend-MASM          (gpu_backend_masm.asm)
//   - Quantization-Engine       (quantization.asm)
//   - SingleFile-Orchestrator   (RawrXD-SingleFile-Streaming-Orchestrator.asm)
//   - GGUF-Analyzer             (RawrXD-GGUFAnalyzer-Complete.asm)
//   - Hotpatch-Unified          (unified_hotpatch_manager.asm)
//   + Any dynamically loaded engine DLLs
//
// Architecture:
//   1. Each engine provides a StreamingEngineDescriptor via exported symbol
//   2. Registry auto-selects based on ModelProfile + HardwareProfile
//   3. BackendSwitcher (Phase 8B) delegates LocalGGUF to this registry
//   4. User can override via !engine select <name>
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>
#include <mutex>
#include <atomic>
#include "flash_attention.h"

namespace RawrXD {

// ============================================================================
// Capability Flags (match ASM ENGINE_CAP_* constants)
// ============================================================================
enum class EngineCapability : uint32_t {
    Streaming       = 0x0001,
    QuadBuffer      = 0x0002,
    AVX512_DMA      = 0x0004,
    LRU_Eviction    = 0x0008,
    GGUF            = 0x0010,
    Safetensors     = 0x0020,
    Blob            = 0x0040,
    VRAM_Paging     = 0x0080,
    NUMA_Aware      = 0x0100,
    FlashAttention  = 0x0200,
    Quantization    = 0x0400,
    DualEngine      = 0x0800,
    HotPatching     = 0x1000,
    GPU_Compute     = 0x2000,
    Encryption      = 0x4000,
    MultiDrive      = 0x8000,
};

inline EngineCapability operator|(EngineCapability a, EngineCapability b) {
    return static_cast<EngineCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline EngineCapability operator&(EngineCapability a, EngineCapability b) {
    return static_cast<EngineCapability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasCapability(EngineCapability flags, EngineCapability cap) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(cap)) != 0;
}

// ============================================================================
// Model Format
// ============================================================================
enum class ModelFormat : uint32_t {
    Unknown     = 0,
    GGUF        = 1,
    Safetensors = 2,
    Blob        = 3,
    Sharded     = 4,    // Multi-file (800B across drives)
};

// ============================================================================
// Model Size Tier (for engine selection heuristics)
// ============================================================================
enum class ModelSizeTier : uint32_t {
    Tiny        = 0,    // < 3B params
    Small       = 1,    // 3B - 13B
    Medium      = 2,    // 13B - 70B
    Large       = 3,    // 70B - 200B (needs quad-buffer)
    Massive     = 4,    // 200B - 800B (needs multi-drive)
    Ultra       = 5,    // 800B+ (needs distributed engines)
};

// ============================================================================
// Hardware Profile (auto-detected at init)
// ============================================================================
struct HardwareProfile {
    uint64_t    totalRAM;           // Total system RAM in bytes
    uint64_t    availableRAM;       // Available RAM at query time
    uint64_t    totalVRAM;          // Total GPU VRAM in bytes
    uint64_t    availableVRAM;      // Available VRAM at query time
    uint32_t    cpuCores;           // Logical core count
    bool        hasAVX2;
    bool        hasAVX512;
    bool        hasSSE42;
    uint32_t    gpuCount;           // Number of GPUs
    std::string gpuName;            // Primary GPU name
    std::string cpuName;            // CPU model
    uint32_t    driveCount;         // Available fast storage drives
    uint64_t    totalDiskSpace;     // Combined fast storage
};

// ============================================================================
// Model Profile (detected from file or user-specified)
// ============================================================================
struct ModelProfile {
    std::string     filePath;
    std::string     modelName;
    ModelFormat     format;
    ModelSizeTier   sizeTier;
    uint64_t        fileSize;           // Total bytes on disk
    uint64_t        parameterCount;     // Estimated parameters
    uint32_t        tensorCount;        // Number of tensors
    uint32_t        layerCount;         // Transformer layers
    uint32_t        quantType;          // Primary quantization (ggml_type)
    bool            isSharded;          // Multi-file model
    uint32_t        shardCount;         // Number of shard files
};

// ============================================================================
// Engine Function Pointers (C-compatible, match ASM exports)
// ============================================================================
// These match the function signatures exported by each ASM engine
typedef int64_t (*EngineInitFn)(uint64_t maxVRAM, uint64_t maxRAM);
typedef int64_t (*EngineShutdownFn)();
typedef int64_t (*EngineLoadModelFn)(const wchar_t* path, uint32_t formatHint);
typedef int64_t (*EngineStreamTensorFn)(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs);
typedef int64_t (*EngineReleaseTensorFn)(uint64_t nameHash);
typedef int64_t (*EngineGetStatsFn)(void* statsOut);
typedef int64_t (*EngineForceEvictionFn)(uint64_t targetBytes);
typedef int64_t (*EngineSetVRAMLimitFn)(uint64_t newLimit);

// ============================================================================
// Streaming Engine Descriptor
// ============================================================================
// This struct is populated from ASM engine exports or C++ engine registrations.
// Each engine provides one of these.
struct StreamingEngineDescriptor {
    // Identity
    std::string         name;           // e.g., "QuadBuffer-DMA-120B"
    std::string         description;    // Human-readable description
    std::string         version;        // e.g., "1.0.0"
    std::string         sourceFile;     // ASM/DLL source path
    
    // Capabilities
    EngineCapability    capabilities;   // Bitfield of ENGINE_CAP_*
    uint32_t            maxModelBillions; // Max supported model size in billions
    uint64_t            minVRAM;        // Minimum VRAM required
    uint64_t            maxRAMTarget;   // Optimal RAM target
    
    // Model format support
    std::vector<ModelFormat> supportedFormats;
    
    // Size tier range
    ModelSizeTier       minSizeTier;    // Smallest model this engine handles well
    ModelSizeTier       maxSizeTier;    // Largest model this engine can handle
    
    // Function pointers (null if not available/loaded)
    EngineInitFn            fnInit;
    EngineShutdownFn        fnShutdown;
    EngineLoadModelFn       fnLoadModel;
    EngineStreamTensorFn    fnStreamTensor;
    EngineReleaseTensorFn   fnReleaseTensor;
    EngineGetStatsFn        fnGetStats;
    EngineForceEvictionFn   fnForceEviction;
    EngineSetVRAMLimitFn    fnSetVRAMLimit;
    
    // Runtime state
    bool                loaded;
    bool                active;
    uint64_t            totalBytesStreamed;
    uint64_t            totalTensorsServed;
    int64_t             lastErrorCode;
    
    StreamingEngineDescriptor()
        : capabilities(static_cast<EngineCapability>(0))
        , maxModelBillions(0), minVRAM(0), maxRAMTarget(0)
        , minSizeTier(ModelSizeTier::Tiny)
        , maxSizeTier(ModelSizeTier::Ultra)
        , fnInit(nullptr), fnShutdown(nullptr)
        , fnLoadModel(nullptr), fnStreamTensor(nullptr)
        , fnReleaseTensor(nullptr), fnGetStats(nullptr)
        , fnForceEviction(nullptr), fnSetVRAMLimit(nullptr)
        , loaded(false), active(false)
        , totalBytesStreamed(0), totalTensorsServed(0)
        , lastErrorCode(0)
    {}
};

// ============================================================================
// Engine Stats (filled by GetStats)
// ============================================================================
struct EngineStats {
    uint64_t    usedVRAM;
    uint64_t    usedRAM;
    uint64_t    cacheHits;
    uint64_t    cacheMisses;
    uint64_t    evictionCount;
    uint64_t    totalBytesStreamed;
    uint64_t    tensorCount;
    uint64_t    blockCount;
};

// ============================================================================
// Selection Result
// ============================================================================
struct EngineSelectionResult {
    bool        success;
    std::string selectedEngine;
    std::string reason;             // Why this engine was chosen
    std::vector<std::string> alternatives; // Other viable engines
};

// ============================================================================
// StreamingEngineRegistry — The Central Registry
// ============================================================================
class StreamingEngineRegistry {
public:
    StreamingEngineRegistry();
    ~StreamingEngineRegistry();
    
    // ---- Registration ----
    // Register a fully-described engine (from C++ or parsed ASM descriptor)
    bool registerEngine(const StreamingEngineDescriptor& desc);
    
    // Register engine from DLL (loads exports dynamically)
    bool registerEngineFromDLL(const std::string& dllPath, const std::string& engineId);
    
    // Unregister engine
    bool unregisterEngine(const std::string& name);
    
    // ---- Auto-Discovery ----
    // Scan known ASM directories and register all found engines
    void discoverEngines();
    
    // Register all built-in (statically linked) engines
    void registerBuiltinEngines();
    
    // ---- Selection ----
    // Auto-select best engine for a model profile + hardware profile
    EngineSelectionResult selectEngine(const ModelProfile& model, const HardwareProfile& hw);
    
    // Auto-select with auto-detected hardware
    EngineSelectionResult selectEngine(const ModelProfile& model);
    
    // Select by explicit name
    bool selectEngineByName(const std::string& name);
    
    // ---- Active Engine Operations ----
    // Initialize the active engine
    int64_t initActiveEngine(uint64_t maxVRAM = 0, uint64_t maxRAM = 0);
    
    // Load model through active engine
    int64_t loadModel(const std::wstring& path, ModelFormat formatHint = ModelFormat::Unknown);
    
    // Stream tensor through active engine
    int64_t streamTensor(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs = 5000);
    
    // Release tensor through active engine
    int64_t releaseTensor(uint64_t nameHash);
    
    // Get stats from active engine
    EngineStats getActiveEngineStats();
    
    // Force eviction on active engine
    int64_t forceEviction(uint64_t targetBytes);
    
    // Set VRAM limit on active engine
    int64_t setVRAMLimit(uint64_t newLimit);
    
    // Shutdown active engine
    int64_t shutdownActiveEngine();
    
    // ---- Query ----
    std::vector<std::string> getRegisteredEngineNames() const;
    const StreamingEngineDescriptor* getEngine(const std::string& name) const;
    const StreamingEngineDescriptor* getActiveEngine() const;
    std::string getActiveEngineName() const;
    size_t getEngineCount() const;
    
    // ---- Hardware Detection ----
    HardwareProfile detectHardware();
    
    // ---- Model Detection ----
    ModelProfile detectModelProfile(const std::string& filePath);
    ModelSizeTier estimateSizeTier(uint64_t fileSize, uint32_t quantType);
    ModelFormat detectFormat(const std::string& filePath);
    
    // ---- Diagnostics ----
    std::string getFullDiagnostics() const;
    std::string getEngineComparisonTable() const;
    std::string getSelectionRationale(const ModelProfile& model, const HardwareProfile& hw) const;
    
private:
    // Engine storage
    std::unordered_map<std::string, StreamingEngineDescriptor> m_engines;
    std::string m_activeEngineName;
    mutable std::mutex m_mutex;
    
    // Hardware cache
    HardwareProfile m_hwProfile;
    bool m_hwDetected;
    
    // Flash Attention AVX-512 engine instance
    FlashAttentionEngine m_flashAttention;
    
    // Selection scoring
    int scoreEngine(const StreamingEngineDescriptor& engine, 
                    const ModelProfile& model,
                    const HardwareProfile& hw) const;
    
    // Built-in engine registration helpers
    void registerQuadBufferEngine();
    void registerRawr1024DualEngine();
    void registerDualEngineStreamer();
    void registerDualEngineManager();
    void registerFlashAttentionEngine();
    void registerInferenceKernelsEngine();
    void registerGPUBackendEngine();
    void registerQuantizationEngine();
    void registerSingleFileOrchestrator();
    void registerGGUFAnalyzerEngine();
    void registerHotpatchEngine();
};

// ============================================================================
// Global singleton accessor
// ============================================================================
StreamingEngineRegistry& getStreamingEngineRegistry();

} // namespace RawrXD
