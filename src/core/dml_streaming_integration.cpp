// ============================================================================
// dml_streaming_integration.cpp — DirectML Streaming Engine Registration
// ============================================================================
// Wires the DirectML inference engine + GGUF bridge into the
// StreamingEngineRegistry as the "DirectML-DualInference" engine.
// Implements all streaming engine function pointers and provides
// dual-model lifecycle management.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "dml_streaming_integration.h"
#include "directml_compute.h"
#include "gguf_dml_bridge.h"
#include "streaming_engine_registry.h"

#include "logging/logger.h"
static Logger s_logger("dml_streaming_integration");

#include <iostream>
#include <sstream>
#include <cstring>

namespace RawrXD {
namespace DML {

// ============================================================================
// Global DML Engine State (singleton)
// ============================================================================
static DMLEngineState s_dmlEngineState;

DMLEngineState& getDMLEngineState() {
    return s_dmlEngineState;
}

// ============================================================================
// Helper: wide-string to narrow-string
// ============================================================================
static std::string wideToNarrow(const wchar_t* wstr) {
    if (!wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

// ============================================================================
// Engine Function Implementations
// ============================================================================

extern "C" int64_t DML_Engine_Init(uint64_t maxVRAM, uint64_t maxRAM) {
    auto& state = getDMLEngineState();
    std::lock_guard<std::mutex> lock(state.stateMutex);

    if (state.initialized.load()) return 0; // Already initialized

    // Get or create DirectMLCompute singleton
    auto& dmlCompute = getDirectMLCompute();
    state.dmlCompute = &dmlCompute;

    // Initialize DML if not already done
    if (!dmlCompute.isInitialized()) {
        auto r = dmlCompute.initialize();
        if (!r.success) {
            s_logger.error( "[DML-Stream] DML init failed: " << r.detail << std::endl;
            state.lastError.store(r.errorCode);
            return -1;
        }
    }

    // Get or create GGUFDMLBridge singleton
    auto& bridge = getGGUFDMLBridge();
    bridge.setDirectMLCompute(state.dmlCompute);
    state.ggufBridge = &bridge;

    // Configure VRAM budget
    if (maxVRAM == 0) {
        // Auto-detect: 16GB total, 1GB reserved for system
        state.totalVRAM = 16ULL * 1024 * 1024 * 1024;
    } else {
        state.totalVRAM = maxVRAM;
    }

    uint64_t usableVRAM = state.totalVRAM - state.reservedVRAM;
    bridge.setMaxVRAMBudget(usableVRAM);
    bridge.setDequantOnCPU(true);
    bridge.setStreamingEnabled(true);
    bridge.setLayerCacheCount(state.layerCacheCount);

    // Progress callback
    bridge.setProgressCallback([](uint32_t current, uint32_t total,
                                   uint64_t bytesUp, uint64_t totalBytes,
                                   const char* tensorName, void* ud) {
        if (total > 0 && current % 50 == 0) {
            float pct = (float)current / total * 100.0f;
            s_logger.info("[DML-Stream] Loading: ");
        }
    });

    state.initialized.store(true);
    s_logger.info("[DML-Stream] Engine initialized, usable VRAM=");

    return 0;
}

extern "C" int64_t DML_Engine_Shutdown() {
    auto& state = getDMLEngineState();
    std::lock_guard<std::mutex> lock(state.stateMutex);

    if (!state.initialized.load()) return 0;

    // Close all models through bridge
    if (state.ggufBridge) {
        state.ggufBridge->closeAllModels();
    }

    // Shutdown DML compute
    if (state.dmlCompute && state.dmlCompute->isInitialized()) {
        state.dmlCompute->shutdown();
    }

    // Reset state
    for (auto& slot : state.slots) {
        slot.active = false;
        slot.modelPath.clear();
        slot.vramUsed = 0;
        slot.fullyLoaded = false;
    }
    state.activeSlotCount = 0;
    state.initialized.store(false);

    s_logger.info("[DML-Stream] Engine shutdown complete");
    return 0;
}

extern "C" int64_t DML_Engine_LoadModel(const wchar_t* path, uint32_t formatHint) {
    auto& state = getDMLEngineState();
    std::lock_guard<std::mutex> lock(state.stateMutex);

    if (!state.initialized.load()) {
        state.lastError.store(-100);
        return -100;
    }

    std::string narrowPath = wideToNarrow(path);
    if (narrowPath.empty()) return -1;

    // Find first available slot
    uint32_t slotIdx = UINT32_MAX;
    for (uint32_t i = 0; i < 2; ++i) {
        if (!state.slots[i].active) {
            slotIdx = i;
            break;
        }
    }

    if (slotIdx == UINT32_MAX) {
        s_logger.error( "[DML-Stream] No free model slots (max 2 models)" << std::endl;
        return -2;
    }

    uint32_t sessionId = 100 + slotIdx;  // Session IDs 100, 101

    // If second model, use dual-model API for VRAM splitting
    DMLResult r;
    if (state.activeSlotCount > 0) {
        r = state.ggufBridge->loadSecondModel(narrowPath.c_str(), sessionId);
    } else {
        r = state.ggufBridge->openModel(narrowPath.c_str(), sessionId);
    }

    if (!r.success) {
        s_logger.error( "[DML-Stream] Model load failed: " << r.detail << std::endl;
        state.lastError.store(r.errorCode);
        return r.errorCode;
    }

    // Populate slot info
    auto& slot = state.slots[slotIdx];
    slot.sessionId = sessionId;
    slot.active = true;
    slot.modelPath = narrowPath;

    auto& config = state.ggufBridge->getModelConfig(sessionId);
    slot.numLayers = config.numLayers;
    slot.vramBudget = state.ggufBridge->getVRAMBudget(sessionId);

    // Determine load strategy: full or streaming
    if (config.estimatedVRAM <= slot.vramBudget) {
        // Model fits entirely in VRAM — full load
        slot.streamingMode = false;
        auto r2 = state.ggufBridge->loadAllTensors(sessionId);
        if (r2.success) {
            slot.fullyLoaded = true;
        } else {
            // Fall back to streaming mode
            slot.streamingMode = true;
            state.ggufBridge->loadFixedTensors(sessionId);
        }
    } else {
        // Model exceeds VRAM — streaming layer mode
        slot.streamingMode = true;
        state.ggufBridge->loadFixedTensors(sessionId);
    }

    slot.vramUsed = state.ggufBridge->getVRAMUsed(sessionId);
    slot.layersInVRAM = state.ggufBridge->getLoadedLayerCount(sessionId);
    state.activeSlotCount++;

    // Set as active session in DML compute
    state.dmlCompute->setActiveSession(sessionId);

    s_logger.info("[DML-Stream] Model loaded to slot ");

    return 0;
}

extern "C" int64_t DML_Engine_StreamTensor(uint64_t nameHash, void* dest,
                                            uint64_t maxBytes, uint32_t timeoutMs) {
    auto& state = getDMLEngineState();
    if (!state.initialized.load()) return -100;

    // Find which slot has this tensor
    for (uint32_t i = 0; i < 2; ++i) {
        if (!state.slots[i].active) continue;

        uint32_t sessionId = state.slots[i].sessionId;
        auto* session = state.dmlCompute->getSession(sessionId);
        if (!session) continue;

        auto tIt = session->tensors.find(nameHash);
        if (tIt == session->tensors.end()) continue;

        // Found the tensor — download it
        uint64_t bytesToCopy = std::min(maxBytes, tIt->second.sizeBytes);
        auto r = state.dmlCompute->downloadTensor(dest, tIt->second, bytesToCopy);
        if (r.success) {
            state.totalBytesStreamed.fetch_add(bytesToCopy);
            state.totalTensorsServed.fetch_add(1);
            return static_cast<int64_t>(bytesToCopy);
        } else {
            return r.errorCode;
        }
    }

    return -3; // Tensor not found in any slot
}

extern "C" int64_t DML_Engine_ReleaseTensor(uint64_t nameHash) {
    auto& state = getDMLEngineState();
    if (!state.initialized.load()) return -100;

    for (uint32_t i = 0; i < 2; ++i) {
        if (!state.slots[i].active) continue;

        auto* session = state.dmlCompute->getSession(state.slots[i].sessionId);
        if (!session) continue;

        auto tIt = session->tensors.find(nameHash);
        if (tIt == session->tensors.end()) continue;

        state.dmlCompute->freeTensor(tIt->second);
        session->tensors.erase(tIt);
        return 0;
    }

    return -3; // Not found
}

extern "C" int64_t DML_Engine_GetStats(void* statsOut) {
    if (!statsOut) return -1;

    auto& state = getDMLEngineState();
    auto* stats = static_cast<EngineStats*>(statsOut);

    stats->totalBytesStreamed = state.totalBytesStreamed.load();
    stats->tensorCount = state.totalTensorsServed.load();

    if (state.dmlCompute) {
        auto& dmlStats = state.dmlCompute->getStats();
        stats->usedVRAM = dmlStats.vramAllocated.load() - dmlStats.vramFreed.load();
        stats->cacheHits = dmlStats.cacheHits.load();
        stats->cacheMisses = dmlStats.cacheMisses.load();
    }

    // Compute used RAM from staging buffers (bridge side)
    uint64_t totalRAM = 0;
    uint64_t evictions = 0;
    uint64_t blocks = 0;
    for (uint32_t i = 0; i < 2; ++i) {
        if (!state.slots[i].active) continue;
        auto* session = state.dmlCompute ? state.dmlCompute->getSession(state.slots[i].sessionId) : nullptr;
        if (session) {
            blocks += session->tensors.size();
            for (const auto& [hash, tensor] : session->tensors) {
                totalRAM += tensor.sizeBytes;
            }
        }
    }
    stats->usedRAM = totalRAM;
    stats->evictionCount = evictions;
    stats->blockCount = blocks;

    return 0;
}

extern "C" int64_t DML_Engine_ForceEviction(uint64_t targetBytes) {
    auto& state = getDMLEngineState();
    if (!state.initialized.load()) return -100;

    // Evict from each active slot proportionally
    for (uint32_t i = 0; i < 2; ++i) {
        if (!state.slots[i].active || !state.slots[i].streamingMode) continue;

        uint64_t perSlot = targetBytes / state.activeSlotCount;
        // The bridge's ensureLayerLoaded triggers eviction internally
        // For explicit eviction, unload oldest layers directly

        auto* session = state.dmlCompute->getSession(state.slots[i].sessionId);
        if (!session) continue;

        // Unload layers from the back until we've freed enough
        uint64_t freed = 0;
        for (int32_t layer = session->numLayers - 1; layer >= 0 && freed < perSlot; --layer) {
            if (state.ggufBridge->isLayerLoaded(state.slots[i].sessionId, layer)) {
                uint64_t before = state.ggufBridge->getVRAMUsed(state.slots[i].sessionId);
                state.ggufBridge->unloadLayer(state.slots[i].sessionId, layer);
                uint64_t after = state.ggufBridge->getVRAMUsed(state.slots[i].sessionId);
                freed += (before - after);
                state.slots[i].layersInVRAM--;
            }
        }

        state.slots[i].vramUsed = state.ggufBridge->getVRAMUsed(state.slots[i].sessionId);
    }

    return 0;
}

extern "C" int64_t DML_Engine_SetVRAMLimit(uint64_t newLimit) {
    auto& state = getDMLEngineState();
    if (!state.ggufBridge) return -100;

    state.ggufBridge->setMaxVRAMBudget(newLimit);

    for (uint32_t i = 0; i < 2; ++i) {
        if (state.slots[i].active) {
            state.slots[i].vramBudget = (state.activeSlotCount > 1)
                ? newLimit / 2 : newLimit;
        }
    }

    return 0;
}

// ============================================================================
// High-Level Dual-Model API
// ============================================================================

DMLResult loadPrimaryModel(const char* ggufPath) {
    auto& state = getDMLEngineState();

    if (!state.initialized.load()) {
        DML_Engine_Init(0, 0);
    }

    // Convert to wide string for the engine function
    int wlen = MultiByteToWideChar(CP_UTF8, 0, ggufPath, -1, nullptr, 0);
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, ggufPath, -1, &wpath[0], wlen);

    int64_t result = DML_Engine_LoadModel(wpath.c_str(), 1);
    return (result == 0)
        ? DMLResult::ok("Primary model loaded")
        : DMLResult::error("Primary model load failed", static_cast<int32_t>(result));
}

DMLResult loadSecondaryModel(const char* ggufPath) {
    auto& state = getDMLEngineState();

    if (!state.initialized.load()) {
        return DMLResult::error("Engine not initialized", -100);
    }

    if (state.activeSlotCount >= 2) {
        return DMLResult::error("Both model slots occupied", -2);
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, ggufPath, -1, nullptr, 0);
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, ggufPath, -1, &wpath[0], wlen);

    int64_t result = DML_Engine_LoadModel(wpath.c_str(), 1);
    return (result == 0)
        ? DMLResult::ok("Secondary model loaded")
        : DMLResult::error("Secondary model load failed", static_cast<int32_t>(result));
}

DMLResult unloadModelSlot(uint32_t slotIndex) {
    auto& state = getDMLEngineState();
    std::lock_guard<std::mutex> lock(state.stateMutex);

    if (slotIndex >= 2 || !state.slots[slotIndex].active) {
        return DMLResult::error("Invalid/inactive slot", -1);
    }

    state.ggufBridge->closeModel(state.slots[slotIndex].sessionId);
    state.slots[slotIndex].active = false;
    state.slots[slotIndex].modelPath.clear();
    state.slots[slotIndex].fullyLoaded = false;
    state.slots[slotIndex].vramUsed = 0;
    state.activeSlotCount--;

    return DMLResult::ok("Model slot unloaded");
}

DMLResult runInferenceOnSlot(uint32_t slotIndex,
                              const int32_t* inputTokens, uint32_t numTokens,
                              float* outputLogits, uint32_t vocabSize) {
    auto& state = getDMLEngineState();

    if (slotIndex >= 2 || !state.slots[slotIndex].active) {
        return DMLResult::error("Invalid/inactive slot", -1);
    }

    auto& slot = state.slots[slotIndex];

    // If streaming mode, ensure layers are loaded progressively
    if (slot.streamingMode) {
        auto& config = state.ggufBridge->getModelConfig(slot.sessionId);
        for (uint32_t layer = 0; layer < config.numLayers; ++layer) {
            auto r = state.ggufBridge->ensureLayerLoaded(slot.sessionId, layer);
            if (!r.success) {
                return DMLResult::error("Layer load failed during inference", r.errorCode);
            }
        }
    }

    // Set active session and run forward pass
    state.dmlCompute->setActiveSession(slot.sessionId);
    return state.dmlCompute->runModelForward(slot.sessionId, inputTokens, numTokens,
                                              outputLogits, vocabSize);
}

std::string getDualModelDiagnostics() {
    auto& state = getDMLEngineState();
    std::ostringstream ss;

    ss << "=== DirectML Dual-Model Streaming Engine ===\n";
    ss << "Initialized: " << (state.initialized.load() ? "Yes" : "No") << "\n";
    ss << "Total VRAM: " << (state.totalVRAM / (1024 * 1024)) << " MB\n";
    ss << "Reserved VRAM: " << (state.reservedVRAM / (1024 * 1024)) << " MB\n";
    ss << "Active Slots: " << state.activeSlotCount << "/2\n";
    ss << "Tensors Served: " << state.totalTensorsServed.load() << "\n";
    ss << "Bytes Streamed: " << (state.totalBytesStreamed.load() / (1024 * 1024)) << " MB\n\n";

    for (uint32_t i = 0; i < 2; ++i) {
        auto& slot = state.slots[i];
        ss << "--- Slot " << i << " ---\n";
        if (!slot.active) {
            ss << "  Status: Empty\n\n";
            continue;
        }
        ss << "  Model: " << slot.modelPath << "\n";
        ss << "  Session: " << slot.sessionId << "\n";
        ss << "  Layers: " << slot.numLayers << "\n";
        ss << "  Layers in VRAM: " << slot.layersInVRAM << "\n";
        ss << "  Streaming: " << (slot.streamingMode ? "Yes" : "No") << "\n";
        ss << "  Fully Loaded: " << (slot.fullyLoaded ? "Yes" : "No") << "\n";
        ss << "  VRAM: " << (slot.vramUsed / (1024 * 1024))
           << "/" << (slot.vramBudget / (1024 * 1024)) << " MB\n\n";
    }

    // Append DML engine diagnostics
    if (state.dmlCompute && state.dmlCompute->isInitialized()) {
        ss << state.dmlCompute->getDiagnosticsString();
    }

    // Append GGUF bridge diagnostics
    if (state.ggufBridge) {
        ss << state.ggufBridge->getDiagnostics();
    }

    return ss.str();
}

// ============================================================================
// StreamingEngineRegistry Registration
// ============================================================================

void registerDMLStreamingEngine(StreamingEngineRegistry& registry) {
    StreamingEngineDescriptor desc;

    // Identity
    desc.name           = "DirectML-DualInference";
    desc.description    = "DirectML GPU inference engine with dual-model support, "
                          "GGUF tensor streaming, and LRU layer eviction. "
                          "Uses native DML operators for transformer forward pass "
                          "on AMD RX 7800 XT (16GB VRAM, RDNA3).";
    desc.version        = "1.0.0";
    desc.sourceFile     = "directml_compute.cpp + gguf_dml_bridge.cpp + DirectML_Bridge.asm";

    // Capabilities
    desc.capabilities   = EngineCapability::Streaming
                        | EngineCapability::GGUF
                        | EngineCapability::VRAM_Paging
                        | EngineCapability::GPU_Compute
                        | EngineCapability::DualEngine
                        | EngineCapability::LRU_Eviction
                        | EngineCapability::Quantization;

    desc.maxModelBillions = 120;    // Up to 120B with streaming layers
    desc.minVRAM        = 4ULL * 1024 * 1024 * 1024;   // 4GB minimum
    desc.maxRAMTarget   = 64ULL * 1024 * 1024 * 1024;  // 64GB system RAM

    // Supported formats
    desc.supportedFormats = { ModelFormat::GGUF };

    // Size tiers
    desc.minSizeTier    = ModelSizeTier::Tiny;      // Works for any size
    desc.maxSizeTier    = ModelSizeTier::Large;     // Up to 200B with streaming

    // Function pointers
    desc.fnInit          = DML_Engine_Init;
    desc.fnShutdown      = DML_Engine_Shutdown;
    desc.fnLoadModel     = DML_Engine_LoadModel;
    desc.fnStreamTensor  = DML_Engine_StreamTensor;
    desc.fnReleaseTensor = DML_Engine_ReleaseTensor;
    desc.fnGetStats      = DML_Engine_GetStats;
    desc.fnForceEviction = DML_Engine_ForceEviction;
    desc.fnSetVRAMLimit  = DML_Engine_SetVRAMLimit;

    // Initial state
    desc.loaded          = true;    // Statically linked
    desc.active          = false;
    desc.totalBytesStreamed = 0;
    desc.totalTensorsServed = 0;
    desc.lastErrorCode   = 0;

    registry.registerEngine(desc);

    s_logger.info("[DML-Stream] Registered 'DirectML-DualInference' engine");
}

} // namespace DML
} // namespace RawrXD
