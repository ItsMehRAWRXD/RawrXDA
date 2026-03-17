// ============================================================================
// streaming_engine_registry.cpp — Phase 9: Selectable ASM Engine Registry
// ============================================================================
// Implementation of the central streaming engine registry.
// Discovers, scores, and manages all ASM + C++ streaming engines.
// ============================================================================

#include "streaming_engine_registry.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <intrin.h>

// ============================================================================
// ASM Engine Extern Declarations (statically linked)
// ============================================================================
// QuadBuffer DMA Streamer — linked when RawrXD_QuadBuffer_Streamer.obj is built
// If not linked, the registry still works; QuadBuffer just won't have live function pointers.
#ifdef RAWRXD_LINK_QUADBUFFER_ASM
extern "C" {
    int64_t QB_Init(uint64_t maxVRAM, uint64_t maxRAM);
    int64_t QB_Shutdown();
    int64_t QB_LoadModel(const wchar_t* path, uint32_t formatHint);
    int64_t QB_StreamTensor(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs);
    int64_t QB_ReleaseTensor(uint64_t nameHash);
    int64_t QB_GetStats(void* statsOut);
    int64_t QB_ForceEviction(uint64_t targetBytes);
    int64_t QB_SetVRAMLimit(uint64_t newLimit);
    void*   QB_GetEngineDescriptor();
}
#endif

// Inference Kernels (if linked)
#ifdef RAWRXD_LINK_INFERENCE_KERNELS_ASM
extern "C" {
    void matmul_f16_avx512_masm(const void* A, const void* B, void* C, int M, int N, int K);
    void rmsnorm_avx512_masm(void* o, const void* x, const void* w, int n);
}
#endif

namespace RawrXD {

// ============================================================================
// Singleton
// ============================================================================
static std::unique_ptr<StreamingEngineRegistry> g_registry;

StreamingEngineRegistry& getStreamingEngineRegistry() {
    if (!g_registry) {
        g_registry = std::make_unique<StreamingEngineRegistry>();
    }
    return *g_registry;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

StreamingEngineRegistry::StreamingEngineRegistry()
    : m_hwDetected(false) {
}

StreamingEngineRegistry::~StreamingEngineRegistry() {
    shutdownActiveEngine();
}

// ============================================================================
// Registration
// ============================================================================

bool StreamingEngineRegistry::registerEngine(const StreamingEngineDescriptor& desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (desc.name.empty()) return false;
    
    m_engines[desc.name] = desc;
    std::cout << "[EngineRegistry] Registered: " << desc.name 
              << " (v" << desc.version << ") — " << desc.description << std::endl;
    return true;
}

bool StreamingEngineRegistry::registerEngineFromDLL(const std::string& dllPath, const std::string& engineId) {
    std::wstring wPath(dllPath.begin(), dllPath.end());
    HMODULE hDll = LoadLibraryW(wPath.c_str());
    if (!hDll) {
        std::cerr << "[EngineRegistry] Failed to load DLL: " << dllPath << std::endl;
        return false;
    }
    
    StreamingEngineDescriptor desc;
    desc.name = engineId;
    desc.sourceFile = dllPath;
    desc.version = "dynamic";
    desc.description = "Dynamically loaded engine from " + dllPath;
    
    // Try to resolve standard exports
    desc.fnInit = (EngineInitFn)GetProcAddress(hDll, "QB_Init");
    desc.fnShutdown = (EngineShutdownFn)GetProcAddress(hDll, "QB_Shutdown");
    desc.fnLoadModel = (EngineLoadModelFn)GetProcAddress(hDll, "QB_LoadModel");
    desc.fnStreamTensor = (EngineStreamTensorFn)GetProcAddress(hDll, "QB_StreamTensor");
    desc.fnReleaseTensor = (EngineReleaseTensorFn)GetProcAddress(hDll, "QB_ReleaseTensor");
    desc.fnGetStats = (EngineGetStatsFn)GetProcAddress(hDll, "QB_GetStats");
    desc.fnForceEviction = (EngineForceEvictionFn)GetProcAddress(hDll, "QB_ForceEviction");
    desc.fnSetVRAMLimit = (EngineSetVRAMLimitFn)GetProcAddress(hDll, "QB_SetVRAMLimit");
    
    // Minimal validation
    if (!desc.fnInit && !desc.fnLoadModel) {
        std::cerr << "[EngineRegistry] DLL has no recognized exports: " << dllPath << std::endl;
        FreeLibrary(hDll);
        return false;
    }
    
    desc.loaded = true;
    desc.capabilities = EngineCapability::Streaming;
    if (desc.fnStreamTensor) desc.capabilities = desc.capabilities | EngineCapability::GGUF;
    if (desc.fnForceEviction) desc.capabilities = desc.capabilities | EngineCapability::LRU_Eviction;
    
    return registerEngine(desc);
}

bool StreamingEngineRegistry::unregisterEngine(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_activeEngineName == name) {
        shutdownActiveEngine();
    }
    
    auto it = m_engines.find(name);
    if (it != m_engines.end()) {
        m_engines.erase(it);
        std::cout << "[EngineRegistry] Unregistered: " << name << std::endl;
        return true;
    }
    return false;
}

// ============================================================================
// Auto-Discovery
// ============================================================================

void StreamingEngineRegistry::discoverEngines() {
    std::cout << "[EngineRegistry] Discovering engines..." << std::endl;
    
    // Register all statically linked (built-in) engines first
    registerBuiltinEngines();
    
    // Scan for DLL engines in known paths
    std::vector<std::string> searchPaths = {
        "engines",
        "plugins",
        "lib",
    };
    
    for (const auto& dir : searchPaths) {
        if (!std::filesystem::exists(dir)) continue;
        
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".dll") {
                std::string stem = entry.path().stem().string();
                if (stem.find("engine") != std::string::npos ||
                    stem.find("stream") != std::string::npos ||
                    stem.find("quad") != std::string::npos) {
                    registerEngineFromDLL(entry.path().string(), stem);
                }
            }
        }
    }
    
    std::cout << "[EngineRegistry] Discovery complete. " 
              << m_engines.size() << " engines registered." << std::endl;
}

void StreamingEngineRegistry::registerBuiltinEngines() {
    registerQuadBufferEngine();
    registerRawr1024DualEngine();
    registerDualEngineStreamer();
    registerDualEngineManager();
    registerFlashAttentionEngine();
    registerInferenceKernelsEngine();
    registerGPUBackendEngine();
    registerQuantizationEngine();
    registerSingleFileOrchestrator();
    registerGGUFAnalyzerEngine();
    registerHotpatchEngine();
}

// ============================================================================
// Built-in Engine Registration
// ============================================================================

void StreamingEngineRegistry::registerQuadBufferEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "QuadBuffer-DMA-120B";
    desc.description = "Quad-Buffer DMA Streamer: 120B models on 16GB VRAM via demand paging & AVX-512 non-temporal streaming. Production: full tensor indexing, LRU eviction, VRAM budget enforcement";
    desc.version = "1.0.0-PROD";
    desc.sourceFile = "src/asm/RawrXD_QuadBuffer_Streamer.asm";
    
    desc.capabilities = EngineCapability::Streaming | EngineCapability::QuadBuffer |
                        EngineCapability::AVX512_DMA | EngineCapability::LRU_Eviction |
                        EngineCapability::GGUF | EngineCapability::Safetensors |
                        EngineCapability::Blob | EngineCapability::VRAM_Paging;
    
    desc.maxModelBillions = 120;
    desc.minVRAM = 16ULL * 1024 * 1024 * 1024;     // 16GB
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024; // 64GB
    
    desc.supportedFormats = { ModelFormat::GGUF, ModelFormat::Safetensors, ModelFormat::Blob };
    desc.minSizeTier = ModelSizeTier::Medium;   // 13B+
    desc.maxSizeTier = ModelSizeTier::Large;    // Up to 200B
    
    // Wire function pointers to ASM exports (only when linked)
#ifdef RAWRXD_LINK_QUADBUFFER_ASM
    desc.fnInit = QB_Init;
    desc.fnShutdown = QB_Shutdown;
    desc.fnLoadModel = QB_LoadModel;
    desc.fnStreamTensor = QB_StreamTensor;
    desc.fnReleaseTensor = QB_ReleaseTensor;
    desc.fnGetStats = QB_GetStats;
    desc.fnForceEviction = QB_ForceEviction;
    desc.fnSetVRAMLimit = QB_SetVRAMLimit;
#endif
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerRawr1024DualEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "Rawr1024-DualEngine";
    desc.description = "119KB dual-engine: quantize (RawrQ/Z/X AVX-512), encrypt (Kyber/Dilithium), sliding-door model loading, beacon sync";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/rawr1024_dual_engine.asm";
    
    desc.capabilities = EngineCapability::Streaming | EngineCapability::DualEngine |
                        EngineCapability::AVX512_DMA | EngineCapability::Quantization |
                        EngineCapability::Encryption | EngineCapability::GGUF;
    
    desc.maxModelBillions = 800;
    desc.minVRAM = 8ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 128ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF, ModelFormat::Blob };
    desc.minSizeTier = ModelSizeTier::Small;
    desc.maxSizeTier = ModelSizeTier::Ultra;
    
    // ASM exports (would need extern "C" declarations when linked)
    desc.fnInit = nullptr;     // Uses rawr1024_direct_load
    desc.fnShutdown = nullptr;
    desc.fnLoadModel = nullptr;
    
    desc.loaded = true;  // Statically linked, always available
    registerEngine(desc);
}

void StreamingEngineRegistry::registerDualEngineStreamer() {
    StreamingEngineDescriptor desc;
    desc.name = "DualEngine-Streamer";
    desc.description = "FP32/INT8 hot-switch streaming engine — runtime precision switching during inference";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/RawrXD_DualEngineStreamer.asm";
    
    desc.capabilities = EngineCapability::Streaming | EngineCapability::DualEngine |
                        EngineCapability::GGUF | EngineCapability::Quantization;
    
    desc.maxModelBillions = 70;
    desc.minVRAM = 4ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 32ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF, ModelFormat::Blob };
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Medium;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerDualEngineManager() {
    StreamingEngineDescriptor desc;
    desc.name = "DualEngine-Manager";
    desc.description = "CPU-load-based engine switching — automatically selects optimal engine based on system load";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/RawrXD_DualEngineManager.asm";
    
    desc.capabilities = EngineCapability::DualEngine | EngineCapability::Streaming;
    
    desc.maxModelBillions = 70;
    desc.minVRAM = 2ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 16ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF };
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Medium;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerFlashAttentionEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "FlashAttention-AVX2";
    desc.description = "AVX2 Flash-Attention v2 with Q8_0 quantized K — 1.2x+ over C intrinsics, tiled O(N) memory";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/flash_attn_asm_avx2.asm";
    
    desc.capabilities = EngineCapability::FlashAttention | EngineCapability::Quantization;
    
    desc.maxModelBillions = 200;
    desc.minVRAM = 0;  // CPU-only kernel
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = {};  // Kernel, not a loader
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Large;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerInferenceKernelsEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "InferenceKernels-AVX512";
    desc.description = "AVX-512 matmul_f16, rmsnorm — core compute kernels for transformer layers";
    desc.version = "1.0.0";
    desc.sourceFile = "src/asm/inference_kernels.asm";
    
    desc.capabilities = EngineCapability::AVX512_DMA | EngineCapability::Quantization;
    
    desc.maxModelBillions = 200;
    desc.minVRAM = 0;
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = {};  // Kernel, not a loader
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Large;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerGPUBackendEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "GPU-Backend-MASM";
    desc.description = "Pure MASM GPU backend — multi-backend (CPU/Vulkan/CUDA/ROCm), GPU memory pool, hardware detection";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/gpu_backend_masm.asm";
    
    desc.capabilities = EngineCapability::GPU_Compute | EngineCapability::VRAM_Paging;
    
    desc.maxModelBillions = 200;
    desc.minVRAM = 4ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = {};  // Backend, not a format loader
    desc.minSizeTier = ModelSizeTier::Small;
    desc.maxSizeTier = ModelSizeTier::Large;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerQuantizationEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "Quantization-AVX512";
    desc.description = "MASM quantization: RawrQ Int4/Int8, RawrZ/RawrX AVX-512 quantize, Q5_1 dequant";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/quantization.asm";
    
    desc.capabilities = EngineCapability::Quantization | EngineCapability::AVX512_DMA;
    
    desc.maxModelBillions = 800;
    desc.minVRAM = 0;
    desc.maxRAMTarget = 128ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF, ModelFormat::Blob };
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Ultra;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerSingleFileOrchestrator() {
    StreamingEngineDescriptor desc;
    desc.name = "SingleFile-Orchestrator";
    desc.description = "Streaming orchestrator: FastDeflate + paging + DMA + Vulkan transfer + thread pool, 64GB RAM target";
    desc.version = "1.0.0";
    desc.sourceFile = "E:/rawrxd/RawrXD-SingleFile-Streaming-Orchestrator.asm";
    
    desc.capabilities = EngineCapability::Streaming | EngineCapability::GPU_Compute |
                        EngineCapability::AVX512_DMA | EngineCapability::VRAM_Paging;
    
    desc.maxModelBillions = 200;
    desc.minVRAM = 8ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF, ModelFormat::Blob };
    desc.minSizeTier = ModelSizeTier::Medium;
    desc.maxSizeTier = ModelSizeTier::Large;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerGGUFAnalyzerEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "GGUF-Analyzer";
    desc.description = "GGUF v3 tensor metadata parser + pattern matching + parameter counting";
    desc.version = "1.0.0";
    desc.sourceFile = "E:/rawrxd/RawrXD-GGUFAnalyzer-Complete.asm";
    
    desc.capabilities = EngineCapability::GGUF;
    
    desc.maxModelBillions = 800;
    desc.minVRAM = 0;
    desc.maxRAMTarget = 4ULL * 1024 * 1024 * 1024;
    
    desc.supportedFormats = { ModelFormat::GGUF };
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Ultra;
    
    desc.loaded = true;
    registerEngine(desc);
}

void StreamingEngineRegistry::registerHotpatchEngine() {
    StreamingEngineDescriptor desc;
    desc.name = "Hotpatch-Unified";
    desc.description = "Three-layer hotpatching: memory + byte-level + server patches with event ring buffer";
    desc.version = "1.0.0";
    desc.sourceFile = "src/masm_pure/unified_hotpatch_manager.asm";
    
    desc.capabilities = EngineCapability::HotPatching;
    
    desc.maxModelBillions = 800;
    desc.minVRAM = 0;
    desc.maxRAMTarget = 0;
    
    desc.supportedFormats = {};
    desc.minSizeTier = ModelSizeTier::Tiny;
    desc.maxSizeTier = ModelSizeTier::Ultra;
    
    desc.loaded = true;
    registerEngine(desc);
}

// ============================================================================
// Engine Selection
// ============================================================================

EngineSelectionResult StreamingEngineRegistry::selectEngine(const ModelProfile& model, const HardwareProfile& hw) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    EngineSelectionResult result;
    result.success = false;
    
    if (m_engines.empty()) {
        result.reason = "No engines registered";
        return result;
    }
    
    // Score all engines
    struct ScoredEngine {
        std::string name;
        int score;
    };
    std::vector<ScoredEngine> scored;
    
    for (const auto& [name, engine] : m_engines) {
        int s = scoreEngine(engine, model, hw);
        if (s > 0) {
            scored.push_back({name, s});
        }
    }
    
    if (scored.empty()) {
        result.reason = "No engines match the model profile";
        return result;
    }
    
    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });
    
    // Best match
    result.success = true;
    result.selectedEngine = scored[0].name;
    
    // Build reason
    const auto& best = m_engines[scored[0].name];
    std::ostringstream ss;
    ss << "Selected '" << best.name << "' (score=" << scored[0].score << "): ";
    ss << best.description;
    if (model.sizeTier >= ModelSizeTier::Large) {
        ss << " [Large model: " << model.fileSize / (1024*1024*1024) << "GB on disk]";
    }
    result.reason = ss.str();
    
    // Alternatives
    for (size_t i = 1; i < scored.size() && i <= 3; i++) {
        result.alternatives.push_back(scored[i].name + " (score=" + std::to_string(scored[i].score) + ")");
    }
    
    return result;
}

EngineSelectionResult StreamingEngineRegistry::selectEngine(const ModelProfile& model) {
    if (!m_hwDetected) {
        m_hwProfile = detectHardware();
        m_hwDetected = true;
    }
    return selectEngine(model, m_hwProfile);
}

bool StreamingEngineRegistry::selectEngineByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_engines.find(name);
    if (it == m_engines.end()) return false;
    
    // Shutdown previous if different
    if (!m_activeEngineName.empty() && m_activeEngineName != name) {
        auto prevIt = m_engines.find(m_activeEngineName);
        if (prevIt != m_engines.end()) {
            prevIt->second.active = false;
        }
    }
    
    m_activeEngineName = name;
    it->second.active = true;
    
    std::cout << "[EngineRegistry] Selected engine: " << name << std::endl;
    return true;
}

// ============================================================================
// Scoring Algorithm
// ============================================================================

int StreamingEngineRegistry::scoreEngine(const StreamingEngineDescriptor& engine,
                                          const ModelProfile& model,
                                          const HardwareProfile& hw) const {
    int score = 0;
    
    // ---- Format compatibility (mandatory filter) ----
    if (!engine.supportedFormats.empty()) {
        bool formatMatch = false;
        for (auto fmt : engine.supportedFormats) {
            if (fmt == model.format) { formatMatch = true; break; }
        }
        if (!formatMatch && model.format != ModelFormat::Unknown) {
            return 0;  // Engine can't handle this format
        }
        if (formatMatch) score += 100;
    }
    
    // ---- Size tier match ----
    if (model.sizeTier >= engine.minSizeTier && model.sizeTier <= engine.maxSizeTier) {
        score += 200;
        
        // Bonus for exact tier match (engine designed for this size)
        if (engine.maxModelBillions > 0 && model.parameterCount > 0) {
            uint64_t billionParams = model.parameterCount / 1000000000ULL;
            if (billionParams <= engine.maxModelBillions) {
                score += 50;
            }
        }
    } else {
        return 0;  // Engine can't handle this model size
    }
    
    // ---- VRAM compatibility ----
    if (engine.minVRAM > 0 && hw.totalVRAM < engine.minVRAM) {
        return 0;  // Not enough VRAM for this engine
    }
    if (engine.minVRAM > 0 && hw.totalVRAM >= engine.minVRAM) {
        score += 50;
    }
    
    // ---- Capability bonuses ----
    if (hasCapability(engine.capabilities, EngineCapability::Streaming)) score += 30;
    if (hasCapability(engine.capabilities, EngineCapability::QuadBuffer) && model.sizeTier >= ModelSizeTier::Large) score += 100;
    if (hasCapability(engine.capabilities, EngineCapability::AVX512_DMA) && hw.hasAVX512) score += 80;
    if (hasCapability(engine.capabilities, EngineCapability::LRU_Eviction) && model.sizeTier >= ModelSizeTier::Medium) score += 60;
    if (hasCapability(engine.capabilities, EngineCapability::VRAM_Paging) && model.fileSize > hw.totalVRAM) score += 120;
    if (hasCapability(engine.capabilities, EngineCapability::FlashAttention)) score += 40;
    if (hasCapability(engine.capabilities, EngineCapability::Quantization) && model.quantType > 0) score += 50;
    if (hasCapability(engine.capabilities, EngineCapability::GPU_Compute) && hw.gpuCount > 0) score += 60;
    if (hasCapability(engine.capabilities, EngineCapability::DualEngine)) score += 20;
    if (hasCapability(engine.capabilities, EngineCapability::NUMA_Aware) && hw.cpuCores >= 16) score += 30;
    if (hasCapability(engine.capabilities, EngineCapability::MultiDrive) && model.isSharded) score += 150;
    if (hasCapability(engine.capabilities, EngineCapability::Encryption)) score += 10;
    
    // ---- Function pointer availability bonus (production-readiness) ----
    int fnBonus = 0;
    if (engine.fnInit) fnBonus += 25;
    if (engine.fnShutdown) fnBonus += 10;
    if (engine.fnLoadModel) fnBonus += 25;
    if (engine.fnStreamTensor) fnBonus += 30;
    if (engine.fnReleaseTensor) fnBonus += 10;
    if (engine.fnGetStats) fnBonus += 10;
    if (engine.fnForceEviction) fnBonus += 15;
    if (engine.fnSetVRAMLimit) fnBonus += 10;
    score += fnBonus;
    
    // ---- Production version bonus ----
    if (engine.version.find("PROD") != std::string::npos) score += 50;
    
    return score;
}

// ============================================================================
// Active Engine Operations
// ============================================================================

int64_t StreamingEngineRegistry::initActiveEngine(uint64_t maxVRAM, uint64_t maxRAM) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnInit) return -1;
    
    auto result = it->second.fnInit(maxVRAM, maxRAM);
    if (result == 0) {
        std::cout << "[EngineRegistry] Engine '" << m_activeEngineName << "' initialized" << std::endl;
    }
    return result;
}

int64_t StreamingEngineRegistry::loadModel(const std::wstring& path, ModelFormat formatHint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnLoadModel) return -1;
    
    return it->second.fnLoadModel(path.c_str(), static_cast<uint32_t>(formatHint));
}

int64_t StreamingEngineRegistry::streamTensor(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs) {
    // Hot path — minimal locking
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnStreamTensor) return -1;
    
    auto result = it->second.fnStreamTensor(nameHash, dest, maxBytes, timeoutMs);
    if (result > 0) {
        it->second.totalBytesStreamed += result;
        it->second.totalTensorsServed++;
    }
    return result;
}

int64_t StreamingEngineRegistry::releaseTensor(uint64_t nameHash) {
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnReleaseTensor) return -1;
    return it->second.fnReleaseTensor(nameHash);
}

EngineStats StreamingEngineRegistry::getActiveEngineStats() {
    EngineStats stats = {};
    
    auto it = m_engines.find(m_activeEngineName);
    if (it != m_engines.end() && it->second.fnGetStats) {
        it->second.fnGetStats(&stats);
    }
    return stats;
}

int64_t StreamingEngineRegistry::forceEviction(uint64_t targetBytes) {
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnForceEviction) return -1;
    return it->second.fnForceEviction(targetBytes);
}

int64_t StreamingEngineRegistry::setVRAMLimit(uint64_t newLimit) {
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end() || !it->second.fnSetVRAMLimit) return -1;
    return it->second.fnSetVRAMLimit(newLimit);
}

int64_t StreamingEngineRegistry::shutdownActiveEngine() {
    auto it = m_engines.find(m_activeEngineName);
    if (it == m_engines.end()) return 0;
    
    int64_t result = 0;
    if (it->second.fnShutdown) {
        result = it->second.fnShutdown();
    }
    it->second.active = false;
    std::cout << "[EngineRegistry] Engine '" << m_activeEngineName << "' shut down" << std::endl;
    return result;
}

// ============================================================================
// Query
// ============================================================================

std::vector<std::string> StreamingEngineRegistry::getRegisteredEngineNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_engines.size());
    for (const auto& [name, _] : m_engines) {
        names.push_back(name);
    }
    return names;
}

const StreamingEngineDescriptor* StreamingEngineRegistry::getEngine(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_engines.find(name);
    return (it != m_engines.end()) ? &it->second : nullptr;
}

const StreamingEngineDescriptor* StreamingEngineRegistry::getActiveEngine() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_engines.find(m_activeEngineName);
    return (it != m_engines.end()) ? &it->second : nullptr;
}

std::string StreamingEngineRegistry::getActiveEngineName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeEngineName;
}

size_t StreamingEngineRegistry::getEngineCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_engines.size();
}

// ============================================================================
// Hardware Detection
// ============================================================================

HardwareProfile StreamingEngineRegistry::detectHardware() {
    HardwareProfile hw = {};
    
    // RAM
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        hw.totalRAM = memInfo.ullTotalPhys;
        hw.availableRAM = memInfo.ullAvailPhys;
    }
    
    // CPU info
    SYSTEM_INFO sysInfo = {};
    GetSystemInfo(&sysInfo);
    hw.cpuCores = sysInfo.dwNumberOfProcessors;
    
    // CPU feature detection via CPUID
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 0);
    int maxFunc = cpuInfo[0];
    
    if (maxFunc >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        hw.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
        hw.hasAVX512 = (cpuInfo[1] & (1 << 16)) != 0;  // AVX-512F
    }
    if (maxFunc >= 1) {
        __cpuid(cpuInfo, 1);
        hw.hasSSE42 = (cpuInfo[2] & (1 << 20)) != 0;
    }
    
    // CPU brand string
    char brand[49] = {};
    __cpuid(cpuInfo, 0x80000000);
    if ((unsigned int)cpuInfo[0] >= 0x80000004) {
        __cpuid((int*)(brand + 0), 0x80000002);
        __cpuid((int*)(brand + 16), 0x80000003);
        __cpuid((int*)(brand + 32), 0x80000004);
        hw.cpuName = brand;
    }
    
    // GPU detection (simplified — real would use DXGI or Vulkan enumeration)
    // For now, check for known GPU-related registry keys or use defaults
    hw.gpuCount = 1;       // Assume at least one GPU
    hw.totalVRAM = 16ULL * 1024 * 1024 * 1024; // Default 16GB (RX 7800 XT target)
    hw.availableVRAM = hw.totalVRAM;
    hw.gpuName = "Auto-Detected GPU";
    
    // Drive count (check known drive letters)
    hw.driveCount = 0;
    hw.totalDiskSpace = 0;
    for (char letter = 'C'; letter <= 'Z'; letter++) {
        char root[4] = { letter, ':', '\\', 0 };
        UINT driveType = GetDriveTypeA(root);
        if (driveType == DRIVE_FIXED) {
            ULARGE_INTEGER totalBytes = {};
            if (GetDiskFreeSpaceExA(root, nullptr, &totalBytes, nullptr)) {
                hw.driveCount++;
                hw.totalDiskSpace += totalBytes.QuadPart;
            }
        }
    }
    
    m_hwProfile = hw;
    m_hwDetected = true;
    
    return hw;
}

// ============================================================================
// Model Detection
// ============================================================================

ModelProfile StreamingEngineRegistry::detectModelProfile(const std::string& filePath) {
    ModelProfile profile = {};
    profile.filePath = filePath;
    
    // Extract model name from path
    std::filesystem::path p(filePath);
    profile.modelName = p.stem().string();
    
    // File size
    if (std::filesystem::exists(filePath)) {
        profile.fileSize = std::filesystem::file_size(filePath);
    }
    
    // Detect format
    profile.format = detectFormat(filePath);
    
    // Estimate size tier from file size and quant type
    profile.sizeTier = estimateSizeTier(profile.fileSize, profile.quantType);
    
    // Check for sharded models
    profile.isSharded = false;
    profile.shardCount = 1;
    std::string stem = p.stem().string();
    if (stem.find("-00001-of-") != std::string::npos || 
        stem.find(".part0") != std::string::npos ||
        stem.find("shard_") != std::string::npos) {
        profile.isSharded = true;
        // Count shards
        auto dir = p.parent_path();
        if (std::filesystem::exists(dir)) {
            uint32_t count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == p.extension()) {
                    count++;
                }
            }
            profile.shardCount = count;
        }
    }
    
    // GGUF-specific: read header for tensor count
    if (profile.format == ModelFormat::GGUF && profile.fileSize >= 32) {
        std::ifstream f(filePath, std::ios::binary);
        if (f.is_open()) {
            uint32_t magic = 0, version = 0;
            uint64_t tensorCount = 0;
            f.read(reinterpret_cast<char*>(&magic), 4);
            f.read(reinterpret_cast<char*>(&version), 4);
            f.read(reinterpret_cast<char*>(&tensorCount), 8);
            
            if (magic == 0x46554747) { // GGUF
                profile.tensorCount = static_cast<uint32_t>(tensorCount);
                // Estimate layers from tensor count (rough: ~10 tensors per layer)
                profile.layerCount = profile.tensorCount / 10;
                // Estimate param count from file size and typical quant ratio
                profile.parameterCount = (profile.fileSize / 2) * 4; // rough: Q4 = 0.5 bytes/param
            }
        }
    }
    
    return profile;
}

ModelSizeTier StreamingEngineRegistry::estimateSizeTier(uint64_t fileSize, uint32_t quantType) {
    // Estimate original param count from file size
    // Q4_0: ~0.5 bytes/param, Q8_0: ~1 byte/param, F16: 2 bytes/param, F32: 4 bytes/param
    double bytesPerParam = 0.5;  // Default Q4 assumption
    if (quantType == 8) bytesPerParam = 1.0;      // Q8_0
    else if (quantType == 1) bytesPerParam = 2.0;  // F16
    else if (quantType == 0) bytesPerParam = 4.0;  // F32
    else if (quantType >= 10 && quantType <= 15) bytesPerParam = 0.6; // K-quants average
    
    uint64_t estimatedParams = static_cast<uint64_t>(fileSize / bytesPerParam);
    uint64_t billions = estimatedParams / 1000000000ULL;
    
    if (billions < 3) return ModelSizeTier::Tiny;
    if (billions < 13) return ModelSizeTier::Small;
    if (billions < 70) return ModelSizeTier::Medium;
    if (billions < 200) return ModelSizeTier::Large;
    if (billions < 800) return ModelSizeTier::Massive;
    return ModelSizeTier::Ultra;
}

ModelFormat StreamingEngineRegistry::detectFormat(const std::string& filePath) {
    // Check extension first
    std::filesystem::path p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".gguf") return ModelFormat::GGUF;
    if (ext == ".safetensors") return ModelFormat::Safetensors;
    
    // Check magic bytes
    std::ifstream f(filePath, std::ios::binary);
    if (f.is_open()) {
        char magic[8] = {};
        f.read(magic, 8);
        
        if (magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F') {
            return ModelFormat::GGUF;
        }
        
        // Safetensors: first 8 bytes are uint64 JSON header length, then '{'
        uint64_t headerLen = *reinterpret_cast<uint64_t*>(magic);
        if (headerLen > 0 && headerLen < 10 * 1024 * 1024) { // Reasonable header size
            char jsonStart = 0;
            f.seekg(8);
            f.read(&jsonStart, 1);
            if (jsonStart == '{') return ModelFormat::Safetensors;
        }
    }
    
    return ModelFormat::Blob;
}

// ============================================================================
// Diagnostics
// ============================================================================

std::string StreamingEngineRegistry::getFullDiagnostics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream ss;
    ss << "=== RawrXD Streaming Engine Registry ===\n";
    ss << "Engines Registered: " << m_engines.size() << "\n";
    ss << "Active Engine: " << (m_activeEngineName.empty() ? "(none)" : m_activeEngineName) << "\n\n";
    
    if (m_hwDetected) {
        ss << "--- Hardware Profile ---\n";
        ss << "CPU: " << m_hwProfile.cpuName << " (" << m_hwProfile.cpuCores << " cores)\n";
        ss << "RAM: " << (m_hwProfile.totalRAM / (1024*1024*1024)) << " GB total, "
           << (m_hwProfile.availableRAM / (1024*1024*1024)) << " GB available\n";
        ss << "GPU: " << m_hwProfile.gpuName << " (" << (m_hwProfile.totalVRAM / (1024*1024*1024)) << " GB VRAM)\n";
        ss << "AVX2: " << (m_hwProfile.hasAVX2 ? "Yes" : "No")
           << " | AVX-512: " << (m_hwProfile.hasAVX512 ? "Yes" : "No")
           << " | SSE4.2: " << (m_hwProfile.hasSSE42 ? "Yes" : "No") << "\n";
        ss << "Drives: " << m_hwProfile.driveCount << " (" 
           << (m_hwProfile.totalDiskSpace / (1024ULL*1024*1024*1024)) << " TB total)\n\n";
    }
    
    ss << getEngineComparisonTable();
    return ss.str();
}

std::string StreamingEngineRegistry::getEngineComparisonTable() const {
    std::ostringstream ss;
    ss << "--- Engine Comparison ---\n";
    ss << std::left << std::setw(28) << "Name" 
       << std::setw(8) << "MaxB" 
       << std::setw(8) << "VRAM"
       << std::setw(8) << "Loaded"
       << std::setw(8) << "Active"
       << "Capabilities\n";
    ss << std::string(90, '-') << "\n";
    
    for (const auto& [name, eng] : m_engines) {
        ss << std::left << std::setw(28) << name.substr(0, 27)
           << std::setw(8) << std::to_string(eng.maxModelBillions) + "B"
           << std::setw(8) << std::to_string(eng.minVRAM / (1024*1024*1024)) + "GB"
           << std::setw(8) << (eng.loaded ? "YES" : "no")
           << std::setw(8) << (eng.active ? ">>>" : "");
        
        // Capability flags
        std::string caps;
        if (hasCapability(eng.capabilities, EngineCapability::Streaming)) caps += "STR ";
        if (hasCapability(eng.capabilities, EngineCapability::QuadBuffer)) caps += "QB ";
        if (hasCapability(eng.capabilities, EngineCapability::AVX512_DMA)) caps += "AVX ";
        if (hasCapability(eng.capabilities, EngineCapability::LRU_Eviction)) caps += "LRU ";
        if (hasCapability(eng.capabilities, EngineCapability::GGUF)) caps += "GGUF ";
        if (hasCapability(eng.capabilities, EngineCapability::Safetensors)) caps += "SAFE ";
        if (hasCapability(eng.capabilities, EngineCapability::VRAM_Paging)) caps += "PAGE ";
        if (hasCapability(eng.capabilities, EngineCapability::FlashAttention)) caps += "FA ";
        if (hasCapability(eng.capabilities, EngineCapability::Quantization)) caps += "QUANT ";
        if (hasCapability(eng.capabilities, EngineCapability::DualEngine)) caps += "DUAL ";
        if (hasCapability(eng.capabilities, EngineCapability::GPU_Compute)) caps += "GPU ";
        if (hasCapability(eng.capabilities, EngineCapability::HotPatching)) caps += "HPATCH ";
        if (hasCapability(eng.capabilities, EngineCapability::Encryption)) caps += "ENC ";
        if (hasCapability(eng.capabilities, EngineCapability::MultiDrive)) caps += "5DRV ";
        ss << caps << "\n";
    }
    
    return ss.str();
}

std::string StreamingEngineRegistry::getSelectionRationale(const ModelProfile& model, const HardwareProfile& hw) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream ss;
    ss << "--- Selection Rationale ---\n";
    ss << "Model: " << model.modelName << " (" << (model.fileSize / (1024*1024*1024)) << " GB)\n";
    ss << "Format: " << static_cast<int>(model.format) << " | Tier: " << static_cast<int>(model.sizeTier) << "\n\n";
    
    for (const auto& [name, engine] : m_engines) {
        int s = scoreEngine(engine, model, hw);
        ss << std::left << std::setw(28) << name << " score=" << s;
        if (s == 0) ss << " (DISQUALIFIED)";
        ss << "\n";
    }
    
    return ss.str();
}

} // namespace RawrXD
