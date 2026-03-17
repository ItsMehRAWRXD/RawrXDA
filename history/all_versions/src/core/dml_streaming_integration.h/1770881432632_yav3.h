// ============================================================================
// dml_streaming_integration.h — DirectML → StreamingEngineRegistry Integration
// ============================================================================
// Registers the DirectML inference engine as a selectable streaming engine
// inside the StreamingEngineRegistry (Phase 9). This enables dual-model
// simultaneous inference via the MultiEngineSystem pipeline:
//
//   StreamingEngineRegistry
//     └──> "DirectML-DualInference" engine
//            ├──> Session 0: Primary model (8GB VRAM)
//            └──> Session 1: Secondary model (8GB VRAM)
//
// Engine Function Pointers:
//   Init          → Initialize DirectMLCompute + GGUFDMLBridge
//   Shutdown      → Release all DML resources
//   LoadModel     → Parse GGUF, upload tensors to DML session
//   StreamTensor  → Read tensor from DML session for downstream use
//   ReleaseTensor → Free tensor from VRAM
//   GetStats      → Return EngineStats from DML counters
//   ForceEviction → Evict LRU layers
//   SetVRAMLimit  → Adjust per-session VRAM budget
//
// Additional Features:
//   - Dual-model coordination (auto-split VRAM 50/50)
//   - Layer-streaming mode for models exceeding VRAM
//   - Integrated progress reporting
//   - Shared D3D12 device with GPUBackendBridge
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>

namespace RawrXD {

// Forward declarations
class StreamingEngineRegistry;
struct StreamingEngineDescriptor;
struct EngineStats;

namespace DML {

class DirectMLCompute;
class GGUFDMLBridge;

// ============================================================================
// DML Engine State (tracks the dual-model streaming engine)
// ============================================================================
struct DMLEngineState {
    DirectMLCompute*    dmlCompute      = nullptr;
    GGUFDMLBridge*      ggufBridge      = nullptr;

    // Model slots
    struct ModelSlot {
        uint32_t    sessionId           = 0;
        bool        active              = false;
        std::string modelPath;
        uint64_t    vramUsed            = 0;
        uint64_t    vramBudget          = 0;
        uint32_t    numLayers           = 0;
        uint32_t    layersInVRAM        = 0;
        bool        fullyLoaded         = false;
        bool        streamingMode       = false;    // Layer-by-layer
    };
    ModelSlot           slots[2];                   // Dual model support
    uint32_t            activeSlotCount  = 0;

    // Hardware
    uint64_t            totalVRAM        = 0;       // Detected VRAM bytes
    uint64_t            reservedVRAM     = 1024ULL * 1024 * 1024; // 1GB reserved for system

    // Runtime state
    std::atomic<bool>   initialized{false};
    std::atomic<uint64_t> totalBytesStreamed{0};
    std::atomic<uint64_t> totalTensorsServed{0};
    std::atomic<int64_t>  lastError{0};
    std::mutex          stateMutex;

    // Layer streaming
    uint32_t            layerCacheCount  = 4;       // Layers to keep hot in VRAM
    bool                autoEvict        = true;    // Auto-evict LRU layers
};

// ============================================================================
// Registration & Lifecycle Functions
// ============================================================================

// Register the DirectML engine with the StreamingEngineRegistry.
// Call this once during initialization (after registry is created).
// Creates and populates a StreamingEngineDescriptor with all function pointers.
void registerDMLStreamingEngine(StreamingEngineRegistry& registry);

// Get the global DML engine state (lazily initialized)
DMLEngineState& getDMLEngineState();

// ============================================================================
// C-compatible engine function pointers (match EngineInitFn, etc.)
// ============================================================================
extern "C" {
    // Initialize the DirectML engine with VRAM/RAM limits
    int64_t DML_Engine_Init(uint64_t maxVRAM, uint64_t maxRAM);

    // Shutdown and release all resources
    int64_t DML_Engine_Shutdown();

    // Load a GGUF model (primary or secondary)
    // path: wchar_t path to GGUF file
    // formatHint: 1=GGUF, 0=auto-detect
    int64_t DML_Engine_LoadModel(const wchar_t* path, uint32_t formatHint);

    // Stream a tensor by name hash
    // Returns bytes copied, or negative on error
    int64_t DML_Engine_StreamTensor(uint64_t nameHash, void* dest,
                                     uint64_t maxBytes, uint32_t timeoutMs);

    // Release a tensor from VRAM
    int64_t DML_Engine_ReleaseTensor(uint64_t nameHash);

    // Get engine stats into EngineStats struct
    int64_t DML_Engine_GetStats(void* statsOut);

    // Force eviction to free targetBytes of VRAM
    int64_t DML_Engine_ForceEviction(uint64_t targetBytes);

    // Update VRAM limit for the active session
    int64_t DML_Engine_SetVRAMLimit(uint64_t newLimit);
}

// ============================================================================
// High-Level Dual-Model API
// ============================================================================

// Load primary model into slot 0
DMLResult loadPrimaryModel(const char* ggufPath);

// Load secondary model into slot 1 (splits VRAM budget)
DMLResult loadSecondaryModel(const char* ggufPath);

// Unload a model from a slot
DMLResult unloadModelSlot(uint32_t slotIndex);

// Run inference on a specific model slot
DMLResult runInferenceOnSlot(uint32_t slotIndex,
                              const int32_t* inputTokens, uint32_t numTokens,
                              float* outputLogits, uint32_t vocabSize);

// Get diagnostics string for all slots
std::string getDualModelDiagnostics();

} // namespace DML
} // namespace RawrXD
