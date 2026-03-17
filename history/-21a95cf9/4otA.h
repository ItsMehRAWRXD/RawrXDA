// ============================================================================
// adaptive_pipeline_parallel.h — Phase 22B: Adaptive Pipeline Parallelism
// ============================================================================
// Dynamically splits inference workloads between batch parallelism, tensor
// parallelism, and pipeline parallelism based on real-time GPU utilization,
// model topology, batch dimensions, and network latency across swarm nodes.
//
// Architecture:
//   1. ParallelismAnalyzer  — Profile model dims + hardware to pick strategy
//   2. BatchSplitter        — Partition batches across nodes for data parallelism
//   3. TensorSplitter       — Shard weight tensors across GPUs (row/col split)
//   4. PipelineSplitter     — Assign layer ranges to pipeline stages
//   5. HybridScheduler      — Combine all three in a single unified schedule
//   6. DynamicRebalancer    — Monitor latency skew and rebalance at runtime
//
// Integrations:
//   - SwarmCoordinator (src/core/swarm_coordinator.h)     — multi-node routing
//   - SwarmDecisionBridge (src/core/swarm_decision_bridge.h)
//   - GPUBackendBridge (src/core/gpu_backend_bridge.h)    — VRAM capacity
//   - FlashAttention (src/core/flash_attention.h)         — attention dispatch
//   - UniversalModelHotpatcher (src/core/universal_model_hotpatcher.h)
//   - QuantumSafeTransport (src/core/quantum_safe_transport.h) — secure xfer
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded, Win32 threads for background rebalancer.
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
namespace RawrXD { namespace GPU { class GPUBackendBridge; } }

// ============================================================================
// Parallelism Strategy Enum
// ============================================================================
enum class ParallelismStrategy : uint8_t {
    None                = 0,    // Single device, no parallelism
    BatchParallel       = 1,    // Data parallelism: split batch across nodes
    TensorParallel      = 2,    // Model parallelism: shard tensors across GPUs
    PipelineParallel    = 3,    // Layer-range pipeline across nodes
    Hybrid_BatchTensor  = 4,    // Batch + tensor parallel combination
    Hybrid_BatchPipeline = 5,   // Batch + pipeline parallel combination
    Hybrid_TensorPipeline = 6,  // Tensor + pipeline parallel combination
    Hybrid_All          = 7,    // All three combined (large clusters)
};

// ============================================================================
// Pipeline Stage Assignment
// ============================================================================
enum class StageType : uint8_t {
    Embedding       = 0,    // Token embedding lookup
    AttentionBlock  = 1,    // Self-attention (QKV + output projection)
    MLPBlock        = 2,    // Feed-forward MLP (2x linear + activation)
    LayerNorm       = 3,    // RMSNorm / LayerNorm
    OutputHead      = 4,    // LM head (final projection + softmax)
    KVCache         = 5,    // KV cache management
    Mixed           = 6,    // Multiple operation types combined
};

// ============================================================================
// Tensor Sharding Dimension
// ============================================================================
enum class ShardDim : uint8_t {
    None        = 0,    // No sharding
    Row         = 1,    // Shard along rows (column-parallel MLP)
    Column      = 2,    // Shard along columns (row-parallel attention)
    Both        = 3,    // 2D sharding (row + column)
};

// ============================================================================
// Pipeline Parallel Result (structured, no exceptions)
// ============================================================================
struct PipelineResult {
    bool        success;
    int32_t     errorCode;
    const char* detail;

    static PipelineResult ok(const char* msg = "OK") {
        return { true, 0, msg };
    }
    static PipelineResult error(int32_t code, const char* msg) {
        return { false, code, msg };
    }
};

// ============================================================================
// Node Hardware Profile (per-node capability snapshot)
// ============================================================================
struct NodeHardwareProfile {
    uint32_t    nodeSlot;               // Swarm node slot
    uint64_t    vramBytes;              // GPU VRAM capacity
    uint64_t    vramAvailable;          // Free VRAM
    uint64_t    ramBytes;               // System RAM
    uint64_t    ramAvailable;           // Free RAM
    uint32_t    computeUnits;           // GPU SMs / CUs
    float       teraflopsF16;           // FP16 peak compute (TFLOPS)
    float       teraflopsF32;           // FP32 peak compute (TFLOPS)
    float       memBandwidthGBs;        // GPU memory bandwidth (GB/s)
    float       networkBandwidthGBs;    // Inter-node bandwidth (GB/s)
    float       networkLatencyMs;       // Average round-trip latency (ms)
    bool        hasAVX512;
    bool        hasTensorCores;         // NVIDIA Tensor Cores / AMD Matrix Cores
    bool        gpuAvailable;

    NodeHardwareProfile()
        : nodeSlot(0), vramBytes(0), vramAvailable(0), ramBytes(0), ramAvailable(0)
        , computeUnits(0), teraflopsF16(0), teraflopsF32(0), memBandwidthGBs(0)
        , networkBandwidthGBs(0), networkLatencyMs(0)
        , hasAVX512(false), hasTensorCores(false), gpuAvailable(false)
    {}
};

// ============================================================================
// Model Topology — describes the transformer architecture for scheduling
// ============================================================================
struct ModelTopology {
    std::string modelName;
    uint32_t    numLayers;              // Total transformer blocks
    uint32_t    hiddenDim;              // Hidden dimension (d_model)
    uint32_t    numAttentionHeads;      // Query heads
    uint32_t    numKVHeads;             // KV heads (GQA)
    uint32_t    headDim;                // Per-head dimension
    uint32_t    intermediateSize;       // MLP intermediate (typically 4 * hiddenDim)
    uint32_t    vocabSize;              // Token vocabulary size
    uint32_t    maxSeqLen;              // Maximum sequence length
    uint64_t    totalParams;            // Total parameter count
    uint64_t    modelSizeBytes;         // Size in memory at current quant
    uint32_t    kvCacheBytesPerToken;   // KV cache bytes per token per layer
    float       avgLayerSizeMB;         // Average layer memory footprint
    bool        isGQA;                  // Grouped Query Attention
    bool        hasMoE;                 // Mixture-of-Experts layers
    uint32_t    numExperts;             // MoE expert count (0 if no MoE)
    uint32_t    numActiveExperts;       // MoE top-k active experts

    ModelTopology()
        : numLayers(0), hiddenDim(0), numAttentionHeads(0), numKVHeads(0)
        , headDim(0), intermediateSize(0), vocabSize(0), maxSeqLen(0)
        , totalParams(0), modelSizeBytes(0), kvCacheBytesPerToken(0)
        , avgLayerSizeMB(0), isGQA(false), hasMoE(false)
        , numExperts(0), numActiveExperts(0)
    {}
};

// ============================================================================
// Pipeline Stage Configuration — a single stage in the pipeline
// ============================================================================
struct PipelineStageConfig {
    uint32_t    stageId;                // Stage index (0 = first)
    uint32_t    nodeSlot;               // Assigned swarm node
    uint32_t    startLayer;             // First transformer block (inclusive)
    uint32_t    endLayer;               // Last transformer block (exclusive)
    StageType   primaryType;            // Dominant operation type
    ShardDim    tensorShardDim;         // How tensors are sharded within stage
    uint32_t    tensorShardCount;       // Number of tensor shards
    uint64_t    estimatedVRAMBytes;     // VRAM required for this stage
    uint64_t    estimatedComputeFlops;  // Compute required per forward pass
    float       estimatedLatencyMs;     // Expected forward-pass latency (ms)
    float       measuredLatencyMs;      // Actual measured latency (updated at runtime)
    float       loadBalance;            // 0.0 = underutilized, 1.0 = perfectly balanced
    bool        includesEmbedding;      // Stage handles token embedding
    bool        includesLMHead;         // Stage handles output head

    PipelineStageConfig()
        : stageId(0), nodeSlot(0), startLayer(0), endLayer(0)
        , primaryType(StageType::Mixed), tensorShardDim(ShardDim::None)
        , tensorShardCount(1), estimatedVRAMBytes(0), estimatedComputeFlops(0)
        , estimatedLatencyMs(0), measuredLatencyMs(0), loadBalance(1.0f)
        , includesEmbedding(false), includesLMHead(false)
    {}
};

// ============================================================================
// Batch Split Descriptor — how a batch is divided across nodes
// ============================================================================
struct BatchSplitDescriptor {
    uint32_t    totalBatchSize;
    uint32_t    numSplits;
    struct Split {
        uint32_t    nodeSlot;
        uint32_t    batchOffset;        // Start index in global batch
        uint32_t    batchCount;         // Number of sequences in this split
        float       nodeCapacityFrac;   // Fraction of total compute capacity
    };
    std::vector<Split> splits;

    BatchSplitDescriptor() : totalBatchSize(0), numSplits(0) {}
};

// ============================================================================
// Tensor Shard Descriptor — how a tensor is partitioned across devices
// ============================================================================
struct TensorShardDescriptor {
    std::string tensorName;             // e.g., "blk.5.attn_q.weight"
    uint32_t    layerIndex;
    ShardDim    shardDim;
    uint32_t    numShards;
    struct Shard {
        uint32_t    nodeSlot;
        uint32_t    shardIndex;
        uint64_t    offsetBytes;        // Offset in original tensor
        uint64_t    sizeBytes;          // Shard size
        uint32_t    rows;               // Shard row count
        uint32_t    cols;               // Shard column count
    };
    std::vector<Shard> shards;

    TensorShardDescriptor() : layerIndex(0), shardDim(ShardDim::None), numShards(0) {}
};

// ============================================================================
// Full Execution Schedule — the complete parallelism plan
// ============================================================================
struct ExecutionSchedule {
    ParallelismStrategy strategy;
    ModelTopology       topology;

    // Pipeline stages (ordered)
    std::vector<PipelineStageConfig>    stages;

    // Batch split (if using batch parallelism)
    BatchSplitDescriptor                batchSplit;

    // Tensor shards (if using tensor parallelism)
    std::vector<TensorShardDescriptor>  tensorShards;

    // Per-stage microbatch configuration
    uint32_t    numMicrobatches;        // Pipeline microbatch count
    uint32_t    microbatchSize;         // Tokens per microbatch

    // Communication volume estimates
    uint64_t    totalActivationTransferBytes;   // Activation data between stages
    uint64_t    totalGradientTransferBytes;     // Gradient sync (training only)
    uint64_t    totalAllReduceBytes;            // AllReduce for tensor parallel

    // Timing estimates
    float       estimatedTotalLatencyMs;        // End-to-end latency
    float       estimatedThroughputTokPerSec;   // Tokens per second
    float       pipelineBubbleFraction;         // Pipeline bubble overhead (0-1)
    float       communicationOverheadFraction;  // Communication vs compute ratio

    // Quality flags
    bool        isOptimal;
    bool        needsRebalance;

    ExecutionSchedule()
        : strategy(ParallelismStrategy::None)
        , numMicrobatches(1), microbatchSize(1)
        , totalActivationTransferBytes(0), totalGradientTransferBytes(0)
        , totalAllReduceBytes(0)
        , estimatedTotalLatencyMs(0), estimatedThroughputTokPerSec(0)
        , pipelineBubbleFraction(0), communicationOverheadFraction(0)
        , isOptimal(false), needsRebalance(false)
    {}
};

// ============================================================================
// Latency Profile — per-stage runtime measurements for rebalancing
// ============================================================================
struct LatencyProfile {
    uint32_t    stageId;
    float       lastLatencyMs;
    float       avgLatencyMs;
    float       p99LatencyMs;
    float       minLatencyMs;
    float       maxLatencyMs;
    uint64_t    sampleCount;
    float       jitterMs;               // Stddev of latency
    float       stallFractionPercent;    // Time spent stalled on communication

    LatencyProfile()
        : stageId(0), lastLatencyMs(0), avgLatencyMs(0), p99LatencyMs(0)
        , minLatencyMs(FLT_MAX), maxLatencyMs(0), sampleCount(0)
        , jitterMs(0), stallFractionPercent(0)
    {}

    void recordSample(float latencyMs) {
        sampleCount++;
        lastLatencyMs = latencyMs;
        if (latencyMs < minLatencyMs) minLatencyMs = latencyMs;
        if (latencyMs > maxLatencyMs) maxLatencyMs = latencyMs;
        // Exponential moving average (α = 0.1)
        if (sampleCount == 1) {
            avgLatencyMs = latencyMs;
        } else {
            avgLatencyMs = avgLatencyMs * 0.9f + latencyMs * 0.1f;
        }
        // Update P99 estimate using exponential quantile approximation
        if (latencyMs > p99LatencyMs) {
            p99LatencyMs += 0.01f * (latencyMs - p99LatencyMs);
        } else {
            p99LatencyMs -= 0.99f * (p99LatencyMs - latencyMs) * 0.01f;
        }
    }
};

// ============================================================================
// Rebalance Event — describes a pipeline rebalancing action
// ============================================================================
struct RebalanceEvent {
    uint64_t    timestampMs;
    ParallelismStrategy prevStrategy;
    ParallelismStrategy newStrategy;
    uint32_t    stagesModified;
    float       latencyImprovement;     // Estimated improvement (ms)
    float       throughputImprovement;   // Estimated improvement (tok/s)
    const char* reason;

    RebalanceEvent()
        : timestampMs(0)
        , prevStrategy(ParallelismStrategy::None)
        , newStrategy(ParallelismStrategy::None)
        , stagesModified(0)
        , latencyImprovement(0), throughputImprovement(0)
        , reason("none")
    {}
};

// ============================================================================
// Pipeline Parallelism Statistics
// ============================================================================
struct PipelineParallelStats {
    std::atomic<uint64_t> schedulesComputed{0};
    std::atomic<uint64_t> rebalanceEvents{0};
    std::atomic<uint64_t> batchSplitsPerformed{0};
    std::atomic<uint64_t> tensorShardsCreated{0};
    std::atomic<uint64_t> pipelineStagesExecuted{0};
    std::atomic<uint64_t> activationTransfersBytes{0};
    std::atomic<uint64_t> microbatchesProcessed{0};
    std::atomic<uint64_t> totalTokensProcessed{0};
    std::atomic<uint64_t> avgLatencyUs{0};
    std::atomic<uint64_t> pipelineStalls{0};
    std::atomic<uint64_t> communicationStalls{0};
};

// ============================================================================
// Callbacks (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*PipelineStageCallback)(uint32_t stageId, float latencyMs, void* userData);
typedef void (*RebalanceCallback)(const RebalanceEvent* event, void* userData);
typedef void (*PipelineProgressCallback)(uint32_t microbatch, uint32_t totalMicrobatches,
                                          float progressFrac, void* userData);

// ============================================================================
// AdaptivePipelineParallel — Main Class
// ============================================================================
class AdaptivePipelineParallel {
public:
    static AdaptivePipelineParallel& instance();

    // ---- Lifecycle ----
    PipelineResult initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_relaxed); }

    // ---- Hardware Profiling ----

    // Profile the local node's hardware capabilities.
    PipelineResult profileLocalHardware(NodeHardwareProfile& outProfile);

    // Profile all online swarm nodes via coordinator.
    PipelineResult profileSwarmCluster(std::vector<NodeHardwareProfile>& outProfiles);

    // Manually register a node's hardware profile.
    void registerNodeProfile(const NodeHardwareProfile& profile);

    // Get registered profiles.
    std::vector<NodeHardwareProfile> getNodeProfiles() const;

    // ---- Model Topology Analysis ----

    // Analyze a loaded model's topology for scheduling.
    PipelineResult analyzeModelTopology(const std::string& modelPath, ModelTopology& outTopo);

    // Manually set topology (e.g., from config or GGUF header).
    void setModelTopology(const ModelTopology& topo);

    const ModelTopology& getModelTopology() const { return m_topology; }

    // ---- Strategy Selection ----

    // The core scheduling algorithm: given model topology + cluster hardware,
    // select the optimal parallelism strategy and produce a full schedule.
    PipelineResult computeOptimalSchedule(ExecutionSchedule& outSchedule);

    // Compute schedule with explicit strategy override.
    PipelineResult computeScheduleWithStrategy(ParallelismStrategy strategy,
                                                ExecutionSchedule& outSchedule);

    // Select strategy based on analysis (returns recommendation).
    ParallelismStrategy selectStrategy() const;

    // ---- Batch Parallelism ----

    // Split a batch across available nodes by compute capacity.
    PipelineResult splitBatchParallel(uint32_t batchSize, BatchSplitDescriptor& outSplit);

    // Execute batch-parallel inference: distribute and gather results.
    PipelineResult executeBatchParallel(const BatchSplitDescriptor& split,
                                         const void* inputTokens, uint32_t seqLen,
                                         void* outputLogits);

    // ---- Tensor Parallelism ----

    // Shard a weight tensor across nodes (row or column split).
    PipelineResult shardTensor(const std::string& tensorName, uint32_t layerIndex,
                                ShardDim dim, uint32_t numShards,
                                TensorShardDescriptor& outDesc);

    // Execute tensor-parallel matmul with all-reduce.
    PipelineResult executeTensorParallel(uint32_t layerIndex,
                                          const void* activations, uint32_t activationLen,
                                          void* output, uint32_t outputLen);

    // ---- Pipeline Parallelism ----

    // Assign layer ranges to pipeline stages on different nodes.
    PipelineResult assignPipelineStages(const ModelTopology& topo,
                                         const std::vector<NodeHardwareProfile>& nodes,
                                         std::vector<PipelineStageConfig>& outStages);

    // Execute pipeline-parallel inference with microbatching.
    PipelineResult executePipelineParallel(const std::vector<PipelineStageConfig>& stages,
                                            uint32_t numMicrobatches,
                                            const void* inputTokens, uint32_t seqLen,
                                            void* outputLogits);

    // ---- Dynamic Rebalancing ----

    // Record a latency measurement for a pipeline stage.
    void recordStageLatency(uint32_t stageId, float latencyMs);

    // Get current latency profiles for all stages.
    std::vector<LatencyProfile> getLatencyProfiles() const;

    // Check if rebalancing is needed (latency skew exceeds threshold).
    bool needsRebalance() const;

    // Trigger dynamic rebalancing: recompute stage assignments.
    PipelineResult rebalancePipeline();

    // Set rebalance threshold (max latency skew ratio before triggering).
    void setRebalanceThreshold(float skewRatio);
    float getRebalanceThreshold() const { return m_rebalanceThreshold; }

    // Enable automatic rebalancing on the monitor thread.
    void enableAutoRebalance(bool enable);
    bool isAutoRebalanceEnabled() const { return m_autoRebalance.load(); }

    // ---- Configuration ----
    void setSwarmCoordinator(SwarmCoordinator* coordinator);
    void setMaxPipelineStages(uint32_t maxStages);
    uint32_t getMaxPipelineStages() const { return m_maxPipelineStages; }
    void setTargetMicrobatchSize(uint32_t tokens);

    // ---- Active Schedule ----
    const ExecutionSchedule& getActiveSchedule() const { return m_activeSchedule; }
    bool hasActiveSchedule() const { return m_hasActiveSchedule.load(); }

    // ---- Statistics ----
    const PipelineParallelStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- Callbacks ----
    void setStageCallback(PipelineStageCallback cb, void* userData);
    void setRebalanceCallback(RebalanceCallback cb, void* userData);
    void setProgressCallback(PipelineProgressCallback cb, void* userData);

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string scheduleToJson(const ExecutionSchedule& schedule) const;
    std::string latencyProfilesToJson() const;
    std::string statsToJson() const;

private:
    AdaptivePipelineParallel();
    ~AdaptivePipelineParallel();
    AdaptivePipelineParallel(const AdaptivePipelineParallel&) = delete;
    AdaptivePipelineParallel& operator=(const AdaptivePipelineParallel&) = delete;

    // Internal: Compute per-node capacity scores
    float computeNodeCapacity(const NodeHardwareProfile& node) const;

    // Internal: Estimate pipeline bubble overhead
    float estimateBubbleFraction(uint32_t numStages, uint32_t numMicrobatches) const;

    // Internal: Estimate communication overhead for a schedule
    float estimateCommunicationOverhead(const ExecutionSchedule& schedule) const;

    // Internal: Cost model for comparing parallelism strategies
    float evaluateStrategyCost(ParallelismStrategy strategy,
                                const ModelTopology& topo,
                                const std::vector<NodeHardwareProfile>& nodes) const;

    // Internal: Greedy layer-to-stage assignment for balanced pipeline
    void greedyLayerAssignment(const ModelTopology& topo,
                                const std::vector<NodeHardwareProfile>& nodes,
                                std::vector<PipelineStageConfig>& stages) const;

    // Internal: Compute optimal microbatch count for pipeline
    uint32_t computeOptimalMicrobatchCount(uint32_t numStages,
                                            uint32_t batchSize) const;

    // Internal: Rebalancing monitor thread
    static DWORD WINAPI rebalanceThread(LPVOID param);
    void monitorRebalance();

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                              m_mutex;
    std::atomic<bool>                               m_initialized;
    std::atomic<bool>                               m_shutdownRequested;
    std::atomic<bool>                               m_autoRebalance;
    std::atomic<bool>                               m_hasActiveSchedule;

    // Model topology
    ModelTopology                                   m_topology;

    // Cluster hardware profiles (indexed by node slot)
    std::unordered_map<uint32_t, NodeHardwareProfile> m_nodeProfiles;

    // Active execution schedule
    ExecutionSchedule                               m_activeSchedule;

    // Per-stage latency tracking
    std::unordered_map<uint32_t, LatencyProfile>    m_latencyProfiles;

    // Rebalance history
    std::vector<RebalanceEvent>                     m_rebalanceHistory;

    // Configuration
    SwarmCoordinator*                               m_coordinator;
    float                                           m_rebalanceThreshold; // default 1.5 (50% skew)
    uint32_t                                        m_maxPipelineStages;
    uint32_t                                        m_targetMicrobatchTokens;

    // Statistics
    PipelineParallelStats                           m_stats;

    // Callbacks
    PipelineStageCallback                           m_stageCb;
    void*                                           m_stageUserData;
    RebalanceCallback                               m_rebalanceCb;
    void*                                           m_rebalanceUserData;
    PipelineProgressCallback                        m_progressCb;
    void*                                           m_progressUserData;

    // Rebalance monitor thread
    HANDLE                                          m_hRebalanceThread;
};
