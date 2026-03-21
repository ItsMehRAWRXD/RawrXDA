#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <atomic>
#include <unordered_map>
#include <array>
#include <cmath>

/**
 * @file polymorphic_loader.h
 * @brief Format-agnostic, tier-adaptive, fixed-memory model loading architecture
 * 
 * Scales from 70B to 700B+ models while maintaining fixed 2-3 GB active window.
 * Supports GGUF, sharded blobs, mixed-quantization formats through unified descriptor.
 * Features time-travel (jump/rewind), rank-folding, and tier morphing without replanning.
 */

// ============================================================================
// SECTION 1: Universal Tensor Descriptor (UTD) - Format Abstraction
// ============================================================================

enum class TensorRole : uint8_t {
    ATTN_Q = 0,
    ATTN_K = 1,
    ATTN_V = 2,
    ATTN_O = 3,
    MLP_UP = 4,
    MLP_DOWN = 5,
    NORM = 6,
    EMB = 7,
    KV_CACHE = 8,
    MISC = 9
};

enum class QuantizationType : uint8_t {
    F32 = 0,
    F16 = 1,
    Q8_0 = 2,
    Q4_K_M = 3,
    Q2_K = 4,
    Q1_5 = 5,
    SPARSE = 6,
    DROPPED = 7
};

/**
 * @struct TensorDesc
 * @brief Universal descriptor for any tensor in any model format
 * 
 * All formats (GGUF, blobs, sharded, etc.) normalize to this structure.
 * No format-specific logic needed in hot paths.
 */
struct TensorDesc {
    uint64_t file_offset;           // Offset in backing file
    uint32_t byte_length;           // Bytes to stream
    uint16_t layer_id;              // Layer/block index
    TensorRole role;                // What this tensor does
    QuantizationType quant;         // Current quantization
    uint8_t rank_hint;              // Low-rank fold factor (0=dense)
    uint16_t stripe_id;             // For sharded models
    uint32_t shape[4];              // Dimensions (H, W, D, ...)
    float criticality;              // Importance for tier-morphing (0-1)
    uint32_t reuse_count;           // How many times used per token
};

// ============================================================================
// SECTION 2: Format Adapters - Format-Agnostic Interface
// ============================================================================

/**
 * @interface IFormatAdapter
 * @brief Adapter pattern: normalize any model format to TensorDesc
 */
class IFormatAdapter {
public:
    virtual ~IFormatAdapter() = default;
    
    /**
     * Enumerate all tensors in the model file.
     * One-time pass during indexing.
     */
    virtual std::vector<TensorDesc> enumerate(const std::string& path) = 0;
    
    /**
     * Get model metadata (context length, embedding dim, layer count, etc.)
     */
    virtual std::unordered_map<std::string, std::string> getMetadata() = 0;
    
    /**
     * Validate format and check integrity.
     */
    virtual bool validate(const std::string& path) = 0;
};

/**
 * @class GGUFAdapter
 * @brief Standard GGUF format support
 */
class GGUFAdapter : public IFormatAdapter {
public:
    std::vector<TensorDesc> enumerate(const std::string& path) override;
    std::unordered_map<std::string, std::string> getMetadata() override;
    bool validate(const std::string& path) override;

private:
    void parseTensorMetadata(const std::string& path);
};

/**
 * @class ShardedBlobAdapter
 * @brief Multi-file sharded model support
 */
class ShardedBlobAdapter : public IFormatAdapter {
public:
    std::vector<TensorDesc> enumerate(const std::string& path) override;
    std::unordered_map<std::string, std::string> getMetadata() override;
    bool validate(const std::string& path) override;

private:
    std::vector<std::string> detectShards(const std::string& base_path);
};

/**
 * @class MixedTierAdapter
 * @brief Mixed-quantization format (different layers at different quants)
 */
class MixedTierAdapter : public IFormatAdapter {
public:
    std::vector<TensorDesc> enumerate(const std::string& path) override;
    std::unordered_map<std::string, std::string> getMetadata() override;
    bool validate(const std::string& path) override;
};

// ============================================================================
// SECTION 3: Slot Lattice Memory - Fixed-Size, Polymorphic
// ============================================================================

enum class SlotType : uint8_t {
    ATTENTION = 0,
    MLP = 1,
    KV_CACHE = 2,
    AUXILIARY = 3
};

/**
 * @struct Slot
 * @brief Fixed-size memory slot with semantic interpretation
 * 
 * Memory is NOT allocated/freed; slots are overwritten.
 * Same bytes can represent different tensors at different times.
 */
struct Slot {
    void* base;                     // Fixed base address
    uint32_t capacity_bytes;        // Never changes
    SlotType type;                  // Role-based budget
    uint64_t last_written_step;     // For LRU / verification
    uint32_t active_bytes;          // Current usage (< capacity)
};

/**
 * @struct ActiveWindowBudget
 * @brief π-partitioned, compile-time fixed budget
 * 
 * All memory is divided by role. Attempt to exceed triggers:
 * - Tier morphing (Q4 → Q2)
 * - Rank reduction
 * - KV window shrinkage
 */
struct ActiveWindowBudget {
    // Total active working set (configurable, default 2.5 GB)
    static constexpr size_t TOTAL_BYTES = static_cast<size_t>(2500ULL) * 1024ULL * 1024ULL;
    
    // π-based partition ratios (compile-time)
    static constexpr double PI = 3.14159265358979323846;
    static constexpr size_t ATTN_BYTES = static_cast<size_t>(TOTAL_BYTES * PI / 8);     // ~0.98GB
    static constexpr size_t MLP_BYTES = static_cast<size_t>(TOTAL_BYTES * PI / 5);      // ~1.58GB
    static constexpr size_t KV_BYTES = static_cast<size_t>(TOTAL_BYTES * PI / 16);      // ~0.49GB
    static constexpr size_t MISC_BYTES = TOTAL_BYTES - (ATTN_BYTES + MLP_BYTES + KV_BYTES);
    
    // Runtime tracking
    std::atomic<size_t> attn_used{0};
    std::atomic<size_t> mlp_used{0};
    std::atomic<size_t> kv_used{0};
    std::atomic<size_t> misc_used{0};
    
    // Hard cap enforcement
    bool canAllocate(SlotType type, size_t bytes) const;
    void recordUsage(SlotType type, size_t bytes);
};

/**
 * @class SlotLattice
 * @brief Fixed-size memory pool with polymorphic semantics
 */
class SlotLattice {
public:
    explicit SlotLattice(const ActiveWindowBudget& budget, size_t slot_count = 32);
    ~SlotLattice();
    
    /**
     * Acquire a slot for writing (overwrites previous content).
     * Never allocates new memory—only reuses existing slots.
     */
    Slot* acquireSlot(SlotType type, uint32_t bytes_needed, uint64_t step_id);
    
    /**
     * Release a slot back to the pool (semantic only—memory unchanged).
     */
    void releaseSlot(Slot* slot);
    
    /**
     * Get total memory usage across all slots.
     */
    size_t getTotalUsage() const;
    
    /**
     * Get usage for a specific slot type.
     */
    size_t getUsageByType(SlotType type) const;
    
    /**
     * Check if memory budget exceeded.
     */
    bool isBudgetExceeded() const;
    
    /**
     * List all slots for debugging/monitoring.
     */
    std::vector<Slot*> getAllSlots() const;

    /**
     * Get count of active (in-use) slots.
     */
    uint32_t getActiveCount() const;

    /**
     * Find the first slot matching a given role/type.
     */
    Slot* findSlot(SlotType type) const;

private:
    std::vector<Slot> slots_;
    std::vector<Slot*> free_slots_;
    const ActiveWindowBudget& budget_;
    std::atomic<size_t> total_usage_{0};
};

// ============================================================================
// SECTION 4: Polymorphic Math - Projection + Rank Folding + Tier Morphing
// ============================================================================

/**
 * @struct ProjectionOperator
 * @brief Mathematical operation: project model into active window
 * 
 * Memory(t) = Σ wi * Πi(Model, t)
 * where wi = fixed partition weight, Πi = role projection
 */
struct ProjectionOperator {
    TensorRole role;
    float partition_weight;         // wi (fixed, π-derived)
    std::vector<size_t> indices;    // Which logical tensors participate
};

/**
 * @class PolymorphicMathEngine
 * @brief Execute by projection, not storage
 */
class PolymorphicMathEngine {
public:
    /**
     * Apply rank folding: Layer ≈ U · Vᵀ
     * U lives in slots, Vᵀ streams and discards.
     * Multiplies logical width without memory growth.
     */
    static void rankFold(
        void* U_slot,
        const std::string& model_path,
        uint64_t V_offset,
        uint32_t U_rows, uint32_t U_cols, uint32_t V_cols,
        float* output
    );
    
    /**
     * Tier morphing: change quantization without replanning.
     * Q4 → Q2 under memory pressure.
     */
    static void morphTier(
        void* tensor_slot,
        uint32_t tensor_bytes,
        QuantizationType from_quant,
        QuantizationType to_quant
    );
    
    /**
     * Create projection operators for role-based partitioning.
     */
    static std::vector<ProjectionOperator> createProjections(
        const std::vector<TensorDesc>& all_tensors
    );
};

// ============================================================================
// SECTION 5: Stream Plan - Deterministic, Precomputed Execution
// ============================================================================

/**
 * @struct StreamStep
 * @brief Represents one execution step (token or batch)
 * 
 * Specifies exactly which micro-zones to load, where to put them,
 * and which operations to execute. No decisions at runtime.
 */
struct StreamStep {
    uint32_t step_id;                           // Unique identifier
    std::vector<TensorDesc> zones_to_load;      // What to stream
    std::array<Slot*, 128> slot_assignments;    // Where to put them
    uint32_t zone_count;
    uint64_t total_bytes;
    std::vector<uint16_t> layers;               // Logical layers involved
    float expected_flops;                       // For workload estimation
};

/**
 * @class GlobalStreamPlan
 * @brief Precomputed execution schedule for the entire model
 * 
 * Computed once at model index time. Replay deterministically at runtime.
 */
class GlobalStreamPlan {
public:
    /**
     * Build plan from enumerated tensors and fixed π-budget.
     */
    bool buildFromTensors(
        const std::vector<TensorDesc>& all_tensors,
        const ActiveWindowBudget& budget,
        uint32_t max_active_layers = 2
    );
    
    /**
     * Load plan from binary cache (for fast startup).
     */
    bool loadFromDisk(const std::string& cache_path);
    
    /**
     * Save plan to binary cache for reuse.
     */
    bool saveToDisk(const std::string& cache_path) const;
    
    /**
     * Get step by index.
     */
    const StreamStep& getStep(uint32_t step_id) const;
    
    /**
     * Total steps in this plan.
     */
    uint32_t getTotalSteps() const { return static_cast<uint32_t>(plan_.size()); }
    
    /**
     * Verify plan respects budget constraints.
     */
    bool verify() const;

private:
    std::vector<StreamStep> plan_;
};

// ============================================================================
// SECTION 6: Execution Controller - Time-Travel Semantics
// ============================================================================

/**
 * @class ExecutionController
 * @brief Stateful execution with jump/rewind (time-travel)
 * 
 * Works identically for 70B or 700B—only plan length changes.
 */
class ExecutionController {
public:
    explicit ExecutionController(const GlobalStreamPlan& plan, SlotLattice& slots);
    
    /**
     * Get current execution step (what to load/compute now).
     */
    const StreamStep& currentStep() const;
    
    /**
     * Advance to next step in sequence.
     */
    void advance();
    
    /**
     * Jump to arbitrary step (time travel forward).
     * Requires checkpointed state at target or earlier.
     */
    void jumpToStep(uint32_t target_step);
    
    /**
     * Rewind to earlier step (time travel backward).
     */
    void spinBackToStep(uint32_t target_step);
    
    /**
     * Fast-forward from start to step (initialize state).
     */
    void spinUpToStep(uint32_t target_step);
    
    /**
     * Get current step ID.
     */
    uint32_t getCurrentStepId() const { return current_step_; }
    
    /**
     * Check if at end of plan.
     */
    bool isComplete() const;

private:
    const GlobalStreamPlan& plan_;
    SlotLattice& slots_;
    uint32_t current_step_;
    
    // Checkpoints for time-travel (every N steps)
    struct Checkpoint {
        uint32_t step_id;
        std::vector<uint8_t> compressed_kv;
        std::vector<uint8_t> compressed_activations;
        std::vector<uint8_t> compressed_data;
        size_t original_size = 0;
    };
    std::unordered_map<uint32_t, Checkpoint> checkpoints_;
    
    void createCheckpoint(uint32_t step_id);
    void restoreCheckpoint(uint32_t step_id);
};

// ============================================================================
// SECTION 7: Polymorphic Loader - Main Orchestrator
// ============================================================================

/**
 * @class PolymorphicLoader
 * @brief Main controller: format → UTD → slots → execution
 * 
 * Transparently handles GGUF, blobs, sharded models.
 * Maintains fixed 2-3GB active window regardless of model size.
 */
class PolymorphicLoader {
public:
    explicit PolymorphicLoader(size_t active_window_bytes =
                                   static_cast<size_t>(2500ULL) * 1024ULL * 1024ULL);
    ~PolymorphicLoader();
    
    /**
     * Index and prepare model for execution.
     * One-time pass: generates GlobalStreamPlan and cache.
     */
    bool indexModel(const std::string& model_path);
    
    /**
     * Begin execution: load plan, initialize slots.
     */
    bool beginExecution(const std::string& model_path);
    
    /**
     * Execute one step: stream zones, set up compute.
     */
    bool executeStep();
    
    /**
     * Get current step data (for GPU upload, SIMD, etc.).
     */
    const StreamStep& getCurrentStep() const;
    
    /**
     * Advance to next step.
     */
    void advanceStep();
    
    /**
     * Time-travel: jump to any step.
     */
    void jumpToStep(uint32_t step_id);
    
    /**
     * Get performance metrics.
     */
    struct PerformanceMetrics {
        float tokens_per_second;
        float mb_per_second;
        size_t active_memory_bytes;
        uint32_t total_steps;
        uint32_t current_step;
    };
    
    PerformanceMetrics getMetrics() const;
    
    /**
     * Detect model format automatically and load with appropriate adapter.
     */
    static std::unique_ptr<IFormatAdapter> detectAndLoadAdapter(const std::string& path);

private:
    ActiveWindowBudget budget_;
    std::unique_ptr<SlotLattice> slots_;
    std::unique_ptr<GlobalStreamPlan> plan_;
    std::unique_ptr<ExecutionController> controller_;
    std::unique_ptr<IFormatAdapter> adapter_;
    
    std::string current_model_path_;
    PerformanceMetrics metrics_;
    
    // Streaming I/O
    void* model_file_handle_;
    bool startAsyncLoad(const TensorDesc& zone);
};

