#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <functional>
#include <unordered_map>

/**
 * @file RawrXD_UNIFIED_ENGINE.h
 * @brief THE SUPER SINGLE SOURCE: Universal Model Loader & Agentic Engine
 *
 * COHERENT ARCHITECTURE:
 * 1. Polymorphic Format Handling (GGUF, Blob, Sharded)
 * 2. Fixed Pi-Partitioned Memory (2.5GB Budget)
 * 3. Streaming Rank Folding & Tier Morphing
 * 4. Deterministic Stream Planning
 * 5. Integrated Win32 Agentic Tools
 */

namespace rawrxd {

// ============================================================================
// CORE ABSTRACTIONS
// ============================================================================

enum class TensorRole : uint8_t {
    ATTN_Q, ATTN_K, ATTN_V, ATTN_O,
    MLP_UP, MLP_DOWN, NORM, EMB, KV_CACHE, MISC
};

enum class QuantizationType : uint8_t {
    F32, F16, Q8_0, Q4_K_M, Q2_K, SPARSE, DROPPED
};

struct TensorDesc {
    uint64_t file_offset;
    uint32_t byte_length;
    uint16_t layer_id;
    TensorRole role;
    QuantizationType quant;
    float criticality;
};

// ============================================================================
// MEMORY LATTICE (The 2.5GB Constraint)
// ============================================================================

struct MemoryBudget {
    static constexpr size_t TOTAL_BYTES = 2500ULL * 1024 * 1024;
    static constexpr double PI = 3.1415926535;
    
    static constexpr size_t ATTN = (size_t)(TOTAL_BYTES * PI / 8); 
    static constexpr size_t MLP = (size_t)(TOTAL_BYTES * PI / 5);
    static constexpr size_t KV = (size_t)(TOTAL_BYTES * PI / 16);
    static constexpr size_t MISC = TOTAL_BYTES - (ATTN + MLP + KV);
};

enum class SlotType { ATTN, MLP, KV, MISC };

struct Slot {
    void* base;
    uint32_t capacity;
    SlotType type;
    uint32_t active;
};

class SlotLattice {
public:
    SlotLattice();
    ~SlotLattice();
    Slot* acquire(SlotType type, uint32_t bytes);
    void release(Slot* slot);
private:
    std::vector<Slot> slots_;
};

// ============================================================================
// EXECUTION & STREAMING
// ============================================================================

struct StreamStep {
    uint32_t step_id;
    std::vector<TensorDesc> zones;
    uint64_t total_bytes;
};

class UnifiedLoader {
public:
     UnifiedLoader();
     ~UnifiedLoader();

     bool loadModel(const std::string& path);
     bool executeNext();
     
     // Agentic Interface
     void setAIGuidance(const std::string& insight);
     void runWin32Task(const std::string& task);

private:
    std::unique_ptr<SlotLattice> lattice_;
    std::vector<StreamStep> plan_;
    uint32_t current_step_ = 0;
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    std::string model_path_;

    bool buildPlan();
    void asyncLoad(const TensorDesc& zone);
};

// ============================================================================
// AGENTIC WIN32 TOOLS (Integrated)
// ============================================================================

class AgenticTools {
public:
    static void InjectPayload(DWORD pid, const std::string& dll);
    static std::vector<std::string> ListProcesses();
    static void OverwriteMemory(DWORD pid, uint64_t addr, const std::vector<uint8_t>& data);
};

} // namespace rawrxd
