// ============================================================================
// universal_model_merger.h — Phase 22C: Universal Model Merger
// ============================================================================
// Combines N specialist models (e.g., 8 × 100B parameters) into a single
// Mixture-of-Experts (MoE) model (e.g., 800B aggregate) with a learned
// gating network that routes tokens to the appropriate expert at inference.
//
// Architecture:
//   1. ExpertModelRegistry   — Register and validate source expert models
//   2. GatingNetworkBuilder  — Construct the top-k router / gating function
//   3. TensorMergeEngine     — Concatenate / interleave expert weight tensors
//   4. MoEArchitectBuilder   — Wire experts into transformer blocks
//   5. QualityValidator      — Verify merged model produces correct outputs
//   6. GGUFExporter          — Export the merged MoE model as GGUF
//
// Expert Selection (default 8 × 100B specialists):
//   Expert 0: "Code"        — Programming & code generation
//   Expert 1: "Math"        — Mathematics & formal reasoning
//   Expert 2: "Science"     — Physics, chemistry, biology
//   Expert 3: "Creative"    — Creative writing & storytelling
//   Expert 4: "Legal"       — Legal analysis & compliance
//   Expert 5: "Medical"     — Medical / clinical knowledge
//   Expert 6: "Finance"     — Financial analysis & trading
//   Expert 7: "Reasoning"   — General reasoning & logic
//
// Integrations:
//   - UniversalModelHotpatcher — quant types, layer info, surgery ops
//   - GPUBackendBridge — VRAM management for large tensor operations
//   - QuantumSafeTransport — secure transfer of expert shards
//   - AdaptivePipelineParallel — distributed merge across cluster
//   - SwarmCoordinator — multi-node coordination for merge
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded, Win32 threads for background merge.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <array>
#include <unordered_map>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Forward declarations
class SwarmCoordinator;
class UniversalModelHotpatcher;
namespace RawrXD { namespace GPU { class GPUBackendBridge; } }

// Pull in QuantType from universal_model_hotpatcher.h
#include "universal_model_hotpatcher.h"

// ============================================================================
// Maximum Experts and Configuration Limits
// ============================================================================
namespace MoELimits {
    constexpr uint32_t MAX_EXPERTS              = 64;       // Hard limit on expert count
    constexpr uint32_t DEFAULT_EXPERTS          = 8;        // Default: 8 × 100B
    constexpr uint32_t DEFAULT_TOP_K            = 2;        // Top-2 routing
    constexpr uint32_t MAX_TOP_K                = 8;        // Maximum active experts per token
    constexpr uint32_t MAX_LAYERS               = 512;      // Maximum transformer layers
    constexpr uint64_t MAX_MERGE_SIZE_BYTES     = 4ULL * 1024 * 1024 * 1024 * 1024; // 4 TB
    constexpr uint32_t GATING_HIDDEN_DIM        = 256;      // Gating network hidden dimension
} // namespace MoELimits

// ============================================================================
// Expert Specialization Domain
// ============================================================================
enum class ExpertDomain : uint8_t {
    Code        = 0,    // Programming & code generation
    Math        = 1,    // Mathematics & formal reasoning
    Science     = 2,    // Physics, chemistry, biology
    Creative    = 3,    // Creative writing & storytelling
    Legal       = 4,    // Legal analysis & compliance
    Medical     = 5,    // Medical / clinical knowledge
    Finance     = 6,    // Financial analysis & trading
    Reasoning   = 7,    // General reasoning & logic
    Multilingual = 8,   // Translation & multilingual
    Vision      = 9,    // Multimodal / vision
    Custom      = 10,   // User-defined specialty
    General     = 255,  // No specialization
};

// ============================================================================
// Gating Network Architecture
// ============================================================================
enum class GatingType : uint8_t {
    TopK_Softmax        = 0,    // Standard top-k with softmax (Mixtral-style)
    TopK_Sigmoid        = 1,    // Top-k with sigmoid gating
    Expert_Choice       = 2,    // Expert chooses tokens (reverse routing)
    Learned_Hash        = 3,    // Hash-based routing with learned buckets
    SwitchTransformer   = 4,    // Switch Transformer (top-1, load balanced)
    Soft_MoE            = 5,    // Soft MoE (all experts, weighted)
    GroupedExperts      = 6,    // Grouped expert routing (DeepSeek-style)
};

// ============================================================================
// Merge Strategy — how expert weights are combined
// ============================================================================
enum class MergeStrategy : uint8_t {
    Concatenate         = 0,    // Stack expert weights (true MoE, largest output)
    Average             = 1,    // Average weights (model soup / averaging)
    SLERP               = 2,    // Spherical linear interpolation
    TIES                = 3,    // TIES-Merging (trim, elect, sign merge)
    DARE                = 4,    // DARE (drop and rescale merge)
    TaskArithmetic      = 5,    // Task arithmetic (add task vectors)
    FrankenMerge        = 6,    // Layer-level frankenmerge (mix layers from models)
    ExpertSlotting      = 7,    // Direct slot: each expert becomes an MoE slot
};

// ============================================================================
// Merge Result (structured, no exceptions)
// ============================================================================
struct MergeResult {
    bool        success;
    int32_t     errorCode;
    const char* detail;
    uint32_t    expertsProcessed;
    uint32_t    layersMerged;
    uint64_t    outputSizeBytes;
    float       durationSeconds;
    float       qualityScore;           // 0.0 = bad, 1.0 = perfect

    static MergeResult ok(const char* msg = "OK") {
        MergeResult r;
        r.success = true;
        r.errorCode = 0;
        r.detail = msg;
        r.expertsProcessed = 0;
        r.layersMerged = 0;
        r.outputSizeBytes = 0;
        r.durationSeconds = 0;
        r.qualityScore = 0;
        return r;
    }

    static MergeResult error(int32_t code, const char* msg) {
        MergeResult r;
        r.success = false;
        r.errorCode = code;
        r.detail = msg;
        r.expertsProcessed = 0;
        r.layersMerged = 0;
        r.outputSizeBytes = 0;
        r.durationSeconds = 0;
        r.qualityScore = 0;
        return r;
    }
};

// ============================================================================
// Expert Model Specification — describes a single expert model
// ============================================================================
struct ExpertModelSpec {
    uint32_t    expertIndex;            // 0-based expert slot
    std::string modelPath;              // Path to GGUF file
    std::string modelName;              // Human-readable name
    ExpertDomain domain;                // Specialization domain
    QuantType   quantType;              // Current quantization
    uint64_t    parameterCount;         // Total parameters
    uint64_t    sizeBytes;              // File size
    uint32_t    numLayers;              // Transformer layers
    uint32_t    hiddenDim;              // Hidden dimension
    uint32_t    numAttentionHeads;      // Query heads
    uint32_t    numKVHeads;             // KV heads (GQA)
    uint32_t    headDim;                // Per-head dimension
    uint32_t    intermediateSize;       // MLP intermediate
    uint32_t    vocabSize;              // Vocabulary size
    float       qualityScore;           // Benchmark score (0-1)
    bool        validated;              // Structure validated OK
    bool        loaded;                 // Currently loaded in memory

    ExpertModelSpec()
        : expertIndex(0), domain(ExpertDomain::General)
        , quantType(QuantType::Q4_K_M), parameterCount(0)
        , sizeBytes(0), numLayers(0), hiddenDim(0)
        , numAttentionHeads(0), numKVHeads(0), headDim(0)
        , intermediateSize(0), vocabSize(0)
        , qualityScore(0), validated(false), loaded(false)
    {}
};

// ============================================================================
// Gating Network Configuration
// ============================================================================
struct GatingNetworkConfig {
    GatingType  type;                   // Routing algorithm
    uint32_t    numExperts;             // Total expert count
    uint32_t    topK;                   // Active experts per token
    uint32_t    inputDim;               // Input dimension (= hidden_dim)
    uint32_t    hiddenDim;              // Gating network hidden size
    float       loadBalanceFactor;      // Auxiliary loss weight for load balancing
    float       routerZLossFactor;      // Z-loss for router stability
    float       expertCapacityFactor;   // Expert capacity factor (1.0 = exact, 1.25 = 25% slack)
    float       jitterNoise;            // Jitter noise for training stability
    bool        useAuxLoss;             // Enable auxiliary load balancing loss
    bool        useGroupedRouting;      // DeepSeek-style grouped routing
    uint32_t    numGroups;              // Number of expert groups (grouped routing)

    GatingNetworkConfig()
        : type(GatingType::TopK_Softmax)
        , numExperts(MoELimits::DEFAULT_EXPERTS)
        , topK(MoELimits::DEFAULT_TOP_K)
        , inputDim(0), hiddenDim(MoELimits::GATING_HIDDEN_DIM)
        , loadBalanceFactor(0.01f)
        , routerZLossFactor(0.001f)
        , expertCapacityFactor(1.25f)
        , jitterNoise(0.01f)
        , useAuxLoss(true)
        , useGroupedRouting(false)
        , numGroups(1)
    {}
};

// ============================================================================
// Gating Network Weights — the router weights for a single MoE layer
// ============================================================================
struct GatingLayerWeights {
    uint32_t    layerIndex;
    std::vector<float> gateWeight;      // [numExperts × inputDim] linear projection
    std::vector<float> gateBias;        // [numExperts] bias (optional)
    bool        hasBias;

    GatingLayerWeights() : layerIndex(0), hasBias(false) {}
};

// ============================================================================
// MoE Layer Configuration — how a transformer block is wired in MoE mode
// ============================================================================
struct MoELayerConfig {
    uint32_t    layerIndex;
    bool        isMoELayer;             // True = MoE, False = shared (dense)
    uint32_t    numActiveExperts;       // Top-k for this layer
    uint32_t    numTotalExperts;        // Total experts available
    std::vector<uint32_t> expertIndices; // Which experts are assigned to this layer
    bool        sharedAttention;        // Attention is shared across experts
    bool        expertMLP;              // MLP is per-expert
    uint64_t    perExpertMLPBytes;      // Size of each expert's MLP weights

    MoELayerConfig()
        : layerIndex(0), isMoELayer(false)
        , numActiveExperts(0), numTotalExperts(0)
        , sharedAttention(true), expertMLP(true)
        , perExpertMLPBytes(0)
    {}
};

// ============================================================================
// Merge Plan — complete specification for the merge operation
// ============================================================================
struct MergePlan {
    MergeStrategy           strategy;
    GatingNetworkConfig     gatingConfig;
    std::vector<ExpertModelSpec>  experts;
    std::vector<MoELayerConfig>   layers;
    std::string             outputPath;         // Where to write merged model
    QuantType               outputQuantType;    // Target quantization for output
    uint64_t                estimatedOutputSize; // Estimated output file size
    uint64_t                peakMemoryRequired; // Peak RAM/VRAM needed during merge
    float                   estimatedDuration;  // Estimated merge time (seconds)
    bool                    validated;          // Plan validated OK

    MergePlan()
        : strategy(MergeStrategy::ExpertSlotting)
        , outputQuantType(QuantType::Q4_K_M)
        , estimatedOutputSize(0), peakMemoryRequired(0)
        , estimatedDuration(0), validated(false)
    {}
};

// ============================================================================
// Expert Routing Statistics — runtime routing analysis
// ============================================================================
struct ExpertRoutingStats {
    uint32_t    expertIndex;
    uint64_t    tokensRouted;           // Total tokens routed to this expert
    float       avgGateWeight;          // Average gating weight when selected
    float       loadFraction;           // Fraction of total tokens (load balance)
    float       utilizationPercent;     // How often this expert is in top-k

    ExpertRoutingStats()
        : expertIndex(0), tokensRouted(0)
        , avgGateWeight(0), loadFraction(0), utilizationPercent(0)
    {}
};

// ============================================================================
// Merge Progress Info
// ============================================================================
struct MergeProgressInfo {
    uint32_t    currentExpert;
    uint32_t    totalExperts;
    uint32_t    currentLayer;
    uint32_t    totalLayers;
    uint64_t    bytesProcessed;
    uint64_t    bytesTotal;
    float       progressFraction;       // 0.0 to 1.0
    float       elapsedSeconds;
    float       estimatedRemainingSeconds;
    const char* currentOperation;

    MergeProgressInfo()
        : currentExpert(0), totalExperts(0), currentLayer(0), totalLayers(0)
        , bytesProcessed(0), bytesTotal(0), progressFraction(0)
        , elapsedSeconds(0), estimatedRemainingSeconds(0)
        , currentOperation("idle")
    {}
};

// ============================================================================
// Merger Statistics
// ============================================================================
struct ModelMergerStats {
    std::atomic<uint64_t> mergesCompleted{0};
    std::atomic<uint64_t> mergesFailed{0};
    std::atomic<uint64_t> expertsRegistered{0};
    std::atomic<uint64_t> expertsValidated{0};
    std::atomic<uint64_t> layersMerged{0};
    std::atomic<uint64_t> gatingNetworksBuilt{0};
    std::atomic<uint64_t> tensorsConcatenated{0};
    std::atomic<uint64_t> bytesProcessed{0};
    std::atomic<uint64_t> bytesOutput{0};
    std::atomic<uint64_t> qualityValidations{0};
    std::atomic<uint64_t> totalMergeDurationMs{0};
};

// ============================================================================
// Callbacks (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*MergeProgressCallback)(const MergeProgressInfo* progress, void* userData);
typedef void (*MergeCompleteCallback)(const MergeResult* result, void* userData);
typedef void (*ExpertLoadCallback)(uint32_t expertIndex, bool loaded, void* userData);

// ============================================================================
// UniversalModelMerger — Main Class
// ============================================================================
class UniversalModelMerger {
public:
    static UniversalModelMerger& instance();

    // ---- Lifecycle ----
    MergeResult initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_relaxed); }

    // ---- Expert Registration ----

    // Register an expert model from a GGUF file path.
    MergeResult addExpertModel(uint32_t expertIndex, const std::string& modelPath,
                                ExpertDomain domain, const std::string& name = "");

    // Register an expert from an already-parsed specification.
    MergeResult addExpertSpec(const ExpertModelSpec& spec);

    // Remove an expert from the registry.
    MergeResult removeExpert(uint32_t expertIndex);

    // Get the spec for a registered expert.
    bool getExpertSpec(uint32_t expertIndex, ExpertModelSpec& outSpec) const;

    // Get all registered experts.
    std::vector<ExpertModelSpec> getRegisteredExperts() const;

    // Get the number of registered experts.
    uint32_t getExpertCount() const;

    // ---- Expert Validation ----

    // Validate that all experts are compatible for merging.
    // Checks: matching layer counts, hidden dims, vocab sizes.
    MergeResult validateExperts();

    // Validate a single expert's structure.
    MergeResult validateExpertStructure(uint32_t expertIndex);

    // ---- Gating Network ----

    // Configure the gating/routing network.
    void setGatingConfig(const GatingNetworkConfig& config);
    const GatingNetworkConfig& getGatingConfig() const { return m_gatingConfig; }

    // Build gating network weights (random initialization for fine-tuning,
    // or load from pre-trained router).
    MergeResult buildGatingNetwork();

    // Load pre-trained gating weights from file.
    MergeResult loadGatingWeights(const std::string& path);

    // Save gating weights to file.
    MergeResult saveGatingWeights(const std::string& path);

    // ---- Merge Planning ----

    // Compute a merge plan from registered experts and gating config.
    MergeResult computeMergePlan(MergePlan& outPlan);

    // Validate a merge plan before execution.
    MergeResult validateMergePlan(const MergePlan& plan);

    // Get the current merge plan.
    const MergePlan& getCurrentPlan() const { return m_currentPlan; }

    // ---- Merge Execution ----

    // Execute the merge: combine all experts into a single MoE model.
    // This is the main operation — may take minutes to hours for large models.
    MergeResult executeMerge(const MergePlan& plan);

    // Execute merge with default plan (auto-computed).
    MergeResult executeMerge();

    // Cancel an in-progress merge.
    void cancelMerge();
    bool isMergeInProgress() const { return m_mergeInProgress.load(); }

    // Get current merge progress.
    MergeProgressInfo getMergeProgress() const;

    // ---- Output ----

    // Export merged model as GGUF file.
    MergeResult exportGGUF(const std::string& outputPath, QuantType outputQuant);

    // Get the merged model's total parameter count.
    uint64_t getMergedParameterCount() const;

    // Get the merged model's estimated size.
    uint64_t getMergedModelSize() const;

    // ---- Quality Validation ----

    // Run quality validation on the merged model.
    // Feeds test prompts and checks output coherence.
    MergeResult validateMergedQuality(float& outScore);

    // Analyze expert routing distribution (token-level).
    MergeResult analyzeRoutingDistribution(std::vector<ExpertRoutingStats>& outStats);

    // ---- Configuration ----
    void setSwarmCoordinator(SwarmCoordinator* coordinator);
    void setMergeStrategy(MergeStrategy strategy);
    MergeStrategy getMergeStrategy() const { return m_mergeStrategy; }
    void setOutputQuantType(QuantType quant);
    void setMaxMemoryBudget(uint64_t bytes);

    // ---- Statistics ----
    const ModelMergerStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- Callbacks ----
    void setProgressCallback(MergeProgressCallback cb, void* userData);
    void setCompleteCallback(MergeCompleteCallback cb, void* userData);
    void setExpertLoadCallback(ExpertLoadCallback cb, void* userData);

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string expertsToJson() const;
    std::string mergePlanToJson(const MergePlan& plan) const;
    std::string routingStatsToJson(const std::vector<ExpertRoutingStats>& stats) const;
    std::string statsToJson() const;

private:
    UniversalModelMerger();
    ~UniversalModelMerger();
    UniversalModelMerger(const UniversalModelMerger&) = delete;
    UniversalModelMerger& operator=(const UniversalModelMerger&) = delete;

    // Internal: Parse GGUF header to extract model specs
    MergeResult parseGGUFHeader(const std::string& path, ExpertModelSpec& outSpec);

    // Internal: Verify two experts have compatible architectures
    bool areExpertsCompatible(const ExpertModelSpec& a, const ExpertModelSpec& b) const;

    // Internal: Initialize gating weights (Xavier initialization)
    void initializeGatingWeights(GatingLayerWeights& weights, uint32_t inputDim,
                                  uint32_t numExperts);

    // Internal: Merge attention layers (shared across experts)
    MergeResult mergeAttentionLayers(uint32_t layerIndex,
                                      const std::vector<ExpertModelSpec>& experts);

    // Internal: Merge MLP layers into MoE expert slots
    MergeResult mergeMoEMLPLayers(uint32_t layerIndex,
                                    const std::vector<ExpertModelSpec>& experts,
                                    const MoELayerConfig& moeConfig);

    // Internal: Concatenate expert weight tensors
    MergeResult concatenateExpertTensors(const std::string& tensorName,
                                          uint32_t layerIndex,
                                          const std::vector<ExpertModelSpec>& experts,
                                          std::vector<uint8_t>& outMerged);

    // Internal: Apply TIES merge
    MergeResult tiesMerge(const std::vector<std::vector<float>>& taskVectors,
                           const std::vector<float>& baseWeights,
                           float density, std::vector<float>& outMerged);

    // Internal: Apply SLERP between two weight vectors
    void slerp(const float* a, const float* b, float t, float* out, uint32_t len);

    // Internal: Compute merge plan layer assignments
    void computeLayerAssignments(const std::vector<ExpertModelSpec>& experts,
                                  std::vector<MoELayerConfig>& outLayers);

    // Internal: Write GGUF header and metadata
    MergeResult writeGGUFHeader(FILE* f, const MergePlan& plan);

    // Internal: Write tensor data to GGUF
    MergeResult writeGGUFTensor(FILE* f, const std::string& name,
                                 const void* data, uint64_t sizeBytes,
                                 QuantType quant, const uint32_t* dims, uint32_t ndims);

    // Internal: Background merge thread
    static DWORD WINAPI mergeThread(LPVOID param);
    void executeMergeInternal();

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                              m_mutex;
    std::atomic<bool>                               m_initialized;
    std::atomic<bool>                               m_shutdownRequested;
    std::atomic<bool>                               m_mergeInProgress;
    std::atomic<bool>                               m_mergeCancelled;

    // Expert registry
    std::unordered_map<uint32_t, ExpertModelSpec>   m_experts;

    // Gating network
    GatingNetworkConfig                             m_gatingConfig;
    std::vector<GatingLayerWeights>                 m_gatingWeights;
    bool                                            m_gatingBuilt;

    // Current merge plan
    MergePlan                                       m_currentPlan;

    // Merge progress
    MergeProgressInfo                               m_progress;

    // Merged model data (in-memory, before export)
    std::vector<std::vector<uint8_t>>               m_mergedTensors;
    std::vector<std::string>                        m_mergedTensorNames;
    uint64_t                                        m_mergedTotalSize;
    uint64_t                                        m_currentDataOffset = 0; // Running offset for GGUF tensor writes

    // Configuration
    SwarmCoordinator*                               m_coordinator;
    MergeStrategy                                   m_mergeStrategy;
    QuantType                                       m_outputQuantType;
    uint64_t                                        m_maxMemoryBudget;

    // Statistics
    ModelMergerStats                                m_stats;

    // Callbacks
    MergeProgressCallback                           m_progressCb;
    void*                                           m_progressUserData;
    MergeCompleteCallback                           m_completeCb;
    void*                                           m_completeUserData;
    ExpertLoadCallback                              m_expertLoadCb;
    void*                                           m_expertLoadUserData;

    // Merge thread
    HANDLE                                          m_hMergeThread;
};
