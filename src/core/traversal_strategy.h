// ============================================================================
// traversal_strategy.h — Adaptive Tensor Traversal Strategy System
// ============================================================================
// Defines strategies for how the iterative inference engine traverses tensor
// space. Strategies adapt in real-time based on hardware feedback (TPS,
// latency, memory pressure, GPU utilization).
//
// This is the "how" of the traversal: which layers to visit, in what order,
// with what parameters, and when to skip/clamp/redirect.
//
// Key design:
//   - Strategies are value types, not polymorphic classes
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers for callbacks
//   - Hardware-aware: same model, same code, different behavior
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef TRAVERSAL_STRATEGY_H
#define TRAVERSAL_STRATEGY_H

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>

// ============================================================================
// Forward declarations
// ============================================================================
struct GPUHardwareProfile;

// ============================================================================
// Enums
// ============================================================================

// TraversalMode — The high-level approach to tensor space exploration
enum class TraversalMode : uint8_t {
    Full           = 0,   // Process all layers (traditional inference)
    SkipUniform    = 1,   // Skip every Nth layer uniformly
    SkipAdaptive   = 2,   // Skip layers based on contribution scores
    DepthFirst     = 3,   // Deep traversal of early layers, shallow late
    BreadthFirst   = 4,   // Broad coverage, minimal depth
    BunnyHop       = 5,   // Skip-keep pattern with configurable stride
    IterDeepening  = 6,   // Iterative deepening: expand coverage each pass
    Custom         = 7    // User-defined via function pointer
};

// LayerPriority — Per-layer importance classification
enum class LayerPriority : uint8_t {
    Critical   = 0,   // Must always execute (embeddings, output head)
    High       = 1,   // Strongly impacts output quality
    Medium     = 2,   // Moderate contribution
    Low        = 3,   // Can be skipped with minimal quality loss
    Negligible = 4,   // Skip-safe for most queries
    Unknown    = 5    // Not yet scored
};

// ClampTarget — What parameter is being clamped
enum class ClampTarget : uint8_t {
    Temperature    = 0,
    TopP           = 1,
    TopK           = 2,
    RepeatPenalty  = 3,
    ContextLength  = 4,
    BatchSize      = 5,
    TokenBudget    = 6,
    EntropyFloor   = 7,
    EntropyChain   = 8
};

// AdaptationReason — Why a strategy was changed
enum class AdaptationReason : uint8_t {
    None                = 0,
    HighLatency         = 1,
    LowTPS              = 2,
    MemoryPressure      = 3,
    GPUThrottling       = 4,
    DecodeFailed        = 5,
    QualityDrop         = 6,
    ConvergenceStall    = 7,
    UserInterrupt       = 8,
    TimeoutApproaching  = 9
};

// ============================================================================
// LayerSkipRule — Determines which layers to skip/include
// ============================================================================
struct LayerSkipRule {
    uint32_t       layerIndex;       // Specific layer (or UINT32_MAX for "all")
    LayerPriority  minPriority;      // Skip if layer priority >= this
    float          skipProbability;  // 0.0 = never skip, 1.0 = always skip
    bool           forceInclude;     // Override: always include this layer
    bool           forceExclude;     // Override: always exclude this layer
};

// ============================================================================
// ParameterClamp — Constrains a generation parameter
// ============================================================================
struct ParameterClamp {
    ClampTarget target;
    float       minValue;
    float       maxValue;
    float       currentValue;
    bool        enabled;

    static ParameterClamp make(ClampTarget t, float min, float max, float cur) {
        ParameterClamp c;
        c.target       = t;
        c.minValue     = min;
        c.maxValue     = max;
        c.currentValue = cur;
        c.enabled      = true;
        return c;
    }
};

// ============================================================================
// HardwareFeedback — Real-time metrics from the running system
// ============================================================================
struct HardwareFeedback {
    double   tokensPerSecond;        // Current TPS
    double   latencyMs;              // Per-token latency in ms
    double   gpuUtilization;         // 0.0 - 1.0
    double   memoryUsedBytes;        // Current system memory usage
    double   vramUsedBytes;          // Current VRAM usage
    double   vramTotalBytes;         // Total VRAM
    double   cpuTemperature;         // Celsius (0 if unavailable)
    double   gpuTemperature;         // Celsius (0 if unavailable)
    uint32_t decodeFailCount;        // Failures in current session
    uint32_t successCount;           // Successful decodes
    uint64_t timestampMs;            // When this sample was taken

    double memoryPressure() const {
        if (vramTotalBytes < 1.0) return 0.0;
        return vramUsedBytes / vramTotalBytes;
    }

    double successRate() const {
        uint32_t total = decodeFailCount + successCount;
        if (total == 0) return 1.0;
        return static_cast<double>(successCount) / total;
    }
};

// ============================================================================
// TraversalPlan — The concrete output of a strategy: what to do this pass
// ============================================================================
struct TraversalPlan {
    // Layer execution mask: true = execute, false = skip
    // Index corresponds to model layer index
    std::vector<bool>            layerMask;

    // Parameter clamps for this pass
    std::vector<ParameterClamp>  clamps;

    // Context window for this pass
    uint32_t                     contextLength;

    // Token budget for this pass (max tokens to generate)
    uint32_t                     tokenBudget;

    // Batch size for this pass
    uint32_t                     batchSize;

    // The traversal mode that generated this plan
    TraversalMode                mode;

    // Pass number (monotonically increasing)
    uint32_t                     passNumber;

    // Estimated resource cost (0.0 = minimal, 1.0 = full model)
    float                        estimatedCostRatio;

    // How many layers are active in this plan
    uint32_t activeLayerCount() const {
        uint32_t count = 0;
        for (bool b : layerMask) { if (b) ++count; }
        return count;
    }

    // What fraction of total layers are active
    float activeLayerRatio() const {
        if (layerMask.empty()) return 0.0f;
        return static_cast<float>(activeLayerCount()) / layerMask.size();
    }
};

// ============================================================================
// StrategyAdaptation — Record of a strategy change
// ============================================================================
struct StrategyAdaptation {
    AdaptationReason reason;
    TraversalMode    oldMode;
    TraversalMode    newMode;
    float            oldSkipRatio;
    float            newSkipRatio;
    uint32_t         passNumber;
    uint64_t         timestampMs;
    const char*      description;
};

// ============================================================================
// TraversalStrategyConfig — Static configuration for the strategy engine
// ============================================================================
struct TraversalStrategyConfig {
    // Initial traversal mode
    TraversalMode     initialMode       = TraversalMode::BunnyHop;

    // Total layer count of the model
    uint32_t          totalLayers       = 80;

    // Initial skip ratio (0.0 = skip nothing, 1.0 = skip everything)
    float             initialSkipRatio  = 0.3f;

    // BunnyHop pattern: keep first N and last M layers always
    uint32_t          keepFirstLayers   = 4;
    uint32_t          keepLastLayers    = 2;

    // BunnyHop stride: skip every Nth layer in the middle
    uint32_t          hopStride         = 3;

    // Maximum context length the hardware can handle
    uint32_t          maxContextLength  = 4096;

    // Minimum context length (below this is useless)
    uint32_t          minContextLength  = 128;

    // TPS threshold: below this, adapt aggressively
    double            minAcceptableTPS  = 1.0;

    // Latency threshold: above this, reduce load
    double            maxAcceptableLatencyMs = 5000.0;

    // Memory pressure threshold (0.0-1.0): above this, reduce
    double            memoryPressureThreshold = 0.85;

    // Maximum adaptations per session before locking
    uint32_t          maxAdaptations    = 100;

    // Whether to allow the strategy to self-modify
    bool              adaptiveEnabled   = true;
};

// ============================================================================
// Custom strategy callback (function pointer, NOT std::function)
// ============================================================================
typedef void (*CustomStrategyFn)(
    const HardwareFeedback* feedback,
    const TraversalPlan*    currentPlan,
    TraversalPlan*          outPlan,
    void*                   userData
);

// ============================================================================
// TraversalStrategy — Main class
// ============================================================================
class TraversalStrategy {
public:
    static TraversalStrategy& instance();

    // ----- Lifecycle -----
    PatchResult initialize(const TraversalStrategyConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Plan Generation -----
    // Generate the next traversal plan based on current state + feedback
    PatchResult generatePlan(const HardwareFeedback& feedback, TraversalPlan* outPlan);

    // Generate an initial (first-pass) plan with no feedback
    PatchResult generateInitialPlan(TraversalPlan* outPlan);

    // ----- Adaptation -----
    // Adapt the strategy based on hardware feedback
    PatchResult adapt(const HardwareFeedback& feedback);

    // Force a specific traversal mode
    PatchResult setMode(TraversalMode mode);

    // Set skip ratio directly (0.0-1.0)
    PatchResult setSkipRatio(float ratio);

    // ----- Layer Priority -----
    // Set priority for a specific layer
    PatchResult setLayerPriority(uint32_t layerIndex, LayerPriority priority);

    // Batch-set priorities from an array of scores (0.0-1.0, higher = more important)
    PatchResult setLayerPrioritiesFromScores(const float* scores, uint32_t count);

    // Get current priority for a layer
    LayerPriority getLayerPriority(uint32_t layerIndex) const;

    // ----- Custom Strategy -----
    PatchResult setCustomStrategy(CustomStrategyFn fn, void* userData);
    PatchResult clearCustomStrategy();

    // ----- Parameter Clamps -----
    PatchResult setClamp(const ParameterClamp& clamp);
    PatchResult removeClamp(ClampTarget target);
    PatchResult clearClamps();

    // ----- Queries -----
    TraversalMode         currentMode() const;
    float                 currentSkipRatio() const;
    uint32_t              adaptationCount() const;
    const TraversalStrategyConfig& getConfig() const { return m_config; }

    // ----- History -----
    uint32_t getAdaptationHistory(StrategyAdaptation* outBuf, uint32_t maxCount) const;

    // ----- Statistics -----
    struct Stats {
        std::atomic<uint64_t> plansGenerated{0};
        std::atomic<uint64_t> adaptations{0};
        std::atomic<uint64_t> layerSkips{0};
        std::atomic<uint64_t> clampActivations{0};
        std::atomic<uint64_t> modeChanges{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    TraversalStrategy();
    ~TraversalStrategy();
    TraversalStrategy(const TraversalStrategy&) = delete;
    TraversalStrategy& operator=(const TraversalStrategy&) = delete;

    // ----- Internal plan generators -----
    void generateFullPlan(TraversalPlan* out);
    void generateSkipUniformPlan(TraversalPlan* out);
    void generateSkipAdaptivePlan(TraversalPlan* out);
    void generateDepthFirstPlan(TraversalPlan* out);
    void generateBreadthFirstPlan(TraversalPlan* out);
    void generateBunnyHopPlan(TraversalPlan* out);
    void generateIterDeepeningPlan(TraversalPlan* out);
    void applyClampsToPlan(TraversalPlan* out);

    // ----- Adaptation logic -----
    void adaptForHighLatency(const HardwareFeedback& fb);
    void adaptForLowTPS(const HardwareFeedback& fb);
    void adaptForMemoryPressure(const HardwareFeedback& fb);
    void adaptForDecodeFail(const HardwareFeedback& fb);
    void recordAdaptation(AdaptationReason reason, TraversalMode oldMode,
                           TraversalMode newMode, float oldSkip, float newSkip,
                           const char* desc);

    // ----- Members -----
    std::atomic<bool>              m_initialized{false};
    mutable std::mutex             m_mutex;

    TraversalStrategyConfig        m_config;
    TraversalMode                  m_currentMode;
    float                          m_currentSkipRatio;
    uint32_t                       m_passCounter;

    // Per-layer priorities
    std::vector<LayerPriority>     m_layerPriorities;

    // Active clamps
    std::vector<ParameterClamp>    m_activeClamps;

    // Custom strategy
    CustomStrategyFn               m_customFn;
    void*                          m_customUserData;

    // Adaptation history (ring buffer)
    static constexpr size_t HISTORY_SIZE = 256;
    static constexpr size_t HISTORY_MASK = HISTORY_SIZE - 1;
    StrategyAdaptation             m_history[HISTORY_SIZE];
    std::atomic<uint32_t>          m_historyHead{0};

    Stats                          m_stats;
};

#endif // TRAVERSAL_STRATEGY_H
