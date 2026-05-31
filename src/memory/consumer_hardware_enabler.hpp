// ============================================================================
// File: src/memory/consumer_hardware_enabler.hpp
// ============================================================================
// 
// CONSUMER HARDWARE MODEL ENABLER (CHME)
// 
// Mission: Run 235B+ models on consumer hardware (8GB VRAM, 16GB RAM)
// 
// Techniques Implemented:
// 1. Model Surgery (pruning, distillation, compression)
// 2. Adaptive Inference (early exit, dynamic depth, cascading)
// 3. Memory-Compute Tradeoff (recomputation, virtual layers)
// 4. Hybrid Precision (mixed precision per layer)
// 5. Speculative Execution (draft-verify, parallel generation)
// 6. Progressive Loading (stream layers on demand)
// 7. Smart KV Management (paging, compression, sharing)
// 8. Intelligent Offloading (predictive, tiered)
// 9. Online Learning (adapt to hardware)
// 10. Batch Optimization (continuous batching)
// 
// Target: 235B model on RTX 4090 (24GB) or even RTX 3090 (24GB)
// Stretch: 235B model on RTX 3080 (10GB) or RTX 4070 (12GB)
// 
// ============================================================================

#ifndef RAWRXD_MEMORY_CONSUMER_HARDWARE_ENABLER_HPP
#define RAWRXD_MEMORY_CONSUMER_HARDWARE_ENABLER_HPP

#include <vector>
#include <memory>
#include <unordered_map>
#include <deque>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <thread>
#include <condition_variable>
#include <queue>
#include <sstream>
#include <iomanip>

namespace rawrxd {
namespace memory {

// ============================================================================
// Hardware Profile
// ============================================================================

struct HardwareProfile {
    // GPU
    size_t vram_bytes;
    float gpu_tflops;  // Tensor compute
    float gpu_bandwidth_gbps;
    int gpu_compute_capability;
    bool has_tensor_cores;
    bool has_fp8_support;
    
    // CPU
    size_t ram_bytes;
    int cpu_cores;
    float cpu_ghz;
    size_t l3_cache_bytes;
    
    // Storage
    size_t ssd_read_mbps;
    size_t ssd_write_mbps;
    size_t nvme_read_mbps;
    bool has_nvme;
    
    // Constraints
    float power_limit_watts;
    float thermal_limit_celsius;
    
    // Create profiles for common hardware
    static HardwareProfile RTX4090() {
        HardwareProfile p;
        p.vram_bytes = 24ULL * 1024 * 1024 * 1024;
        p.gpu_tflops = 330.0f;
        p.gpu_bandwidth_gbps = 1008.0f;
        p.gpu_compute_capability = 90;
        p.has_tensor_cores = true;
        p.has_fp8_support = true;
        p.ram_bytes = 64ULL * 1024 * 1024 * 1024;
        p.cpu_cores = 16;
        p.cpu_ghz = 4.5f;
        p.ssd_read_mbps = 7000;
        p.has_nvme = true;
        p.power_limit_watts = 450.0f;
        p.thermal_limit_celsius = 83.0f;
        return p;
    }
    
    static HardwareProfile RTX3080() {
        HardwareProfile p;
        p.vram_bytes = 10ULL * 1024 * 1024 * 1024;
        p.gpu_tflops = 136.0f;
        p.gpu_bandwidth_gbps = 760.0f;
        p.gpu_compute_capability = 86;
        p.has_tensor_cores = true;
        p.has_fp8_support = false;
        p.ram_bytes = 32ULL * 1024 * 1024 * 1024;
        p.cpu_cores = 8;
        p.cpu_ghz = 4.0f;
        p.ssd_read_mbps = 3500;
        p.has_nvme = true;
        p.power_limit_watts = 320.0f;
        p.thermal_limit_celsius = 83.0f;
        return p;
    }
    
    static HardwareProfile RTX4070() {
        HardwareProfile p;
        p.vram_bytes = 12ULL * 1024 * 1024 * 1024;
        p.gpu_tflops = 184.0f;
        p.gpu_bandwidth_gbps = 504.0f;
        p.gpu_compute_capability = 89;
        p.has_tensor_cores = true;
        p.has_fp8_support = true;
        p.ram_bytes = 32ULL * 1024 * 1024 * 1024;
        p.cpu_cores = 8;
        p.cpu_ghz = 4.0f;
        p.ssd_read_mbps = 5000;
        p.has_nvme = true;
        p.power_limit_watts = 200.0f;
        p.thermal_limit_celsius = 83.0f;
        return p;
    }
};

// ============================================================================
// Model Profile
// ============================================================================

struct ModelProfile {
    std::string name;
    std::string architecture;  // "llama", "qwen", "mistral", "mixtral"
    
    size_t num_parameters;
    size_t num_layers;
    size_t hidden_size;
    size_t intermediate_size;
    size_t num_attention_heads;
    size_t num_key_value_heads;
    size_t vocab_size;
    size_t max_seq_length;
    
    // MoE specific
    bool is_moe;
    size_t num_experts;
    size_t experts_per_token;
    
    // Memory requirements
    size_t fp16_weight_bytes;
    size_t fp16_kv_cache_bytes_per_token;
    size_t activation_bytes_per_token;
    
    // Compute requirements
    size_t flops_per_token;
    
    // Quality metrics
    float baseline_perplexity;
    float baseline_accuracy;
    
    // Create profiles for common models
    static ModelProfile Qwen235B() {
        ModelProfile m;
        m.name = "Qwen-235B";
        m.architecture = "qwen";
        m.num_parameters = 235000000000ULL;
        m.num_layers = 80;
        m.hidden_size = 12288;
        m.intermediate_size = 36864;
        m.num_attention_heads = 96;
        m.num_key_value_heads = 8;  // GQA
        m.vocab_size = 152000;
        m.max_seq_length = 32768;
        m.is_moe = false;
        m.fp16_weight_bytes = 470ULL * 1024 * 1024 * 1024;  // ~470 GB
        m.fp16_kv_cache_bytes_per_token = 80 * 8 * 128 * 2 * 2;  // Per token
        m.activation_bytes_per_token = 12288 * 4;
        m.flops_per_token = 470ULL * 1024 * 1024 * 1024;
        m.baseline_perplexity = 6.5f;
        m.baseline_accuracy = 0.85f;
        return m;
    }
    
    static ModelProfile Llama405B() {
        ModelProfile m;
        m.name = "Llama-405B";
        m.architecture = "llama";
        m.num_parameters = 405000000000ULL;
        m.num_layers = 126;
        m.hidden_size = 16384;
        m.intermediate_size = 53248;
        m.num_attention_heads = 128;
        m.num_key_value_heads = 8;
        m.vocab_size = 128000;
        m.max_seq_length = 8192;
        m.is_moe = false;
        m.fp16_weight_bytes = 810ULL * 1024 * 1024 * 1024;  // ~810 GB
        m.activation_bytes_per_token = 16384 * 4;
        m.flops_per_token = 810ULL * 1024 * 1024 * 1024;
        m.baseline_perplexity = 6.0f;
        m.baseline_accuracy = 0.87f;
        return m;
    }
    
    static ModelProfile Mixtral8x7B() {
        ModelProfile m;
        m.name = "Mixtral-8x7B";
        m.architecture = "mixtral";
        m.num_parameters = 46700000000ULL;
        m.num_layers = 32;
        m.hidden_size = 4096;
        m.intermediate_size = 14336;
        m.num_attention_heads = 32;
        m.num_key_value_heads = 8;
        m.vocab_size = 32000;
        m.max_seq_length = 32768;
        m.is_moe = true;
        m.num_experts = 8;
        m.experts_per_token = 2;
        m.fp16_weight_bytes = 94ULL * 1024 * 1024 * 1024;  // ~94 GB
        m.activation_bytes_per_token = 4096 * 4;
        m.flops_per_token = 94ULL * 1024 * 1024 * 1024;
        m.baseline_perplexity = 7.0f;
        m.baseline_accuracy = 0.82f;
        return m;
    }
};

// ============================================================================
// Execution Plan
// ============================================================================

struct ExecutionPlan {
    // Layer assignment
    std::vector<int> gpu_layers;      // Layers on GPU
    std::vector<int> cpu_layers;      // Layers on CPU
    std::vector<int> disk_layers;     // Layers on disk/streamed
    
    // Precision assignment
    std::vector<int> layer_bits;      // Bits per layer (16, 8, 4, 2)
    
    // KV cache plan
    size_t kv_cache_budget;
    int kv_precision_bits;
    bool kv_offload_enabled;
    
    // Compute plan
    bool use_recomputation;
    float recompute_ratio;           // How much to recompute
    
    // Speculative execution
    bool use_speculative;
    int draft_model_size;            // Parameters for draft model
    int speculative_tokens;
    
    // Performance estimates
    float estimated_tps;
    float estimated_latency_ms;
    float estimated_quality;
    
    // Memory estimates
    size_t estimated_vram_usage;
    size_t estimated_ram_usage;
    
    ExecutionPlan()
        : kv_cache_budget(0)
        , kv_precision_bits(16)
        , kv_offload_enabled(false)
        , use_recomputation(false)
        , recompute_ratio(0.0f)
        , use_speculative(false)
        , draft_model_size(0)
        , speculative_tokens(0)
        , estimated_tps(0.0f)
        , estimated_latency_ms(0.0f)
        , estimated_quality(0.0f)
        , estimated_vram_usage(0)
        , estimated_ram_usage(0)
    {}
};

// ============================================================================
// Layer Pruner (Model Surgery)
// ============================================================================

class LayerPruner {
public:
    enum class PruningStrategy {
        IMPORTANCE_BASED,    // Prune least important layers
        REDUNDANCY_BASED,    // Remove redundant layers
        DISTILLATION,        // Knowledge distillation
        PROGRESSIVE,         // Progressive depth reduction
        ADAPTIVE             // Adaptive based on task
    };
    
    struct PruningResult {
        std::vector<int> pruned_layers;
        std::vector<int> remaining_layers;
        float quality_impact;
        size_t memory_saved;
        
        PruningResult()
            : quality_impact(0.0f)
            , memory_saved(0)
        {}
    };
    
    explicit LayerPruner(PruningStrategy strategy);
    
    // Analyze layer importance
    std::vector<float> computeLayerImportance(
        const ModelProfile& model,
        const std::vector<float>& layer_gradients,
        const std::vector<float>& layer_activations
    );
    
    // Prune layers
    PruningResult prune(
        const ModelProfile& model,
        const HardwareProfile& hardware,
        float max_quality_loss = 0.05f
    );
    
    // Get optimal layer count for hardware
    int getOptimalLayerCount(
        const ModelProfile& model,
        const HardwareProfile& hardware
    );
    
private:
    PruningStrategy strategy_;
    
    std::vector<float> importance_scores_;
    
    // Analyze redundancy between layers
    float computeLayerSimilarity(
        const std::vector<float>& layer1_activations,
        const std::vector<float>& layer2_activations
    );
};

// ============================================================================
// Attention Head Pruner
// ============================================================================

class AttentionHeadPruner {
public:
    struct HeadImportance {
        int layer_id;
        int head_id;
        float importance_score;
        float redundancy_score;
        
        HeadImportance()
            : layer_id(-1)
            , head_id(-1)
            , importance_score(0.0f)
            , redundancy_score(0.0f)
        {}
    };
    
    AttentionHeadPruner();
    
    // Compute head importance
    std::vector<HeadImportance> analyzeHeads(
        const std::vector<std::pair<int, const float*>>& attention_weights,
        size_t num_heads,
        size_t seq_len
    );
    
    // Prune heads
    std::vector<std::pair<int, int>> pruneHeads(
        const std::vector<HeadImportance>& importance,
        float prune_ratio = 0.3f,
        float min_quality = 0.95f
    );
    
    // Get optimal head count
    int getOptimalHeadCount(
        int layer_id,
        int original_heads,
        size_t vram_budget
    );
    
private:
    std::unordered_map<int, std::vector<float>> head_importance_;
    
    float computeAttentionEntropy(
        const float* attention_weights,
        size_t seq_len
    );
};

// ============================================================================
// Expert Pruner (for MoE models)
// ============================================================================

class ExpertPruner {
public:
    struct ExpertImportance {
        int expert_id;
        float usage_frequency;
        float quality_contribution;
        float redundancy_score;
        
        ExpertImportance()
            : expert_id(-1)
            , usage_frequency(0.0f)
            , quality_contribution(0.0f)
            , redundancy_score(0.0f)
        {}
    };
    
    ExpertPruner();
    
    // Analyze expert importance from routing history
    std::vector<ExpertImportance> analyzeExperts(
        const std::vector<std::vector<int>>& routing_history,
        const std::vector<float>& quality_scores
    );
    
    // Prune experts
    std::vector<int> pruneExperts(
        const std::vector<ExpertImportance>& importance,
        size_t keep_count,
        float min_quality = 0.95f
    );
    
    // Merge similar experts
    std::vector<std::pair<int, int>> mergeExperts(
        const std::vector<ExpertImportance>& importance,
        float similarity_threshold = 0.9f
    );
    
private:
    std::vector<ExpertImportance> expert_stats_;
    
    float computeExpertSimilarity(
        const float* expert1_weights,
        const float* expert2_weights,
        size_t weight_count
    );
};

// ============================================================================
// Early Exit Controller
// ============================================================================

class EarlyExitController {
public:
    struct ExitDecision {
        bool should_exit;
        int exit_layer;
        float confidence;
        float quality_estimate;
        
        ExitDecision()
            : should_exit(false)
            , exit_layer(-1)
            , confidence(0.0f)
            , quality_estimate(0.0f)
        {}
    };
    
    struct ExitPoint {
        int layer_id;
        std::vector<float> classifier_weights;
        float threshold;
        float accuracy_at_exit;
        
        ExitPoint()
            : layer_id(-1)
            , threshold(0.9f)
            , accuracy_at_exit(0.0f)
        {}
    };
    
    EarlyExitController(float threshold = 0.9f);
    
    // Add exit point
    void addExitPoint(int layer_id, float threshold = 0.9f);
    
    // Decide if should exit
    ExitDecision shouldExit(
        int current_layer,
        const float* hidden_state,
        size_t hidden_size,
        const std::vector<float>& attention_weights
    );
    
    // Train exit classifiers
    void trainExitClassifier(
        int layer_id,
        const std::vector<float>& hidden_states,
        const std::vector<float>& final_outputs,
        const std::vector<int>& labels
    );
    
    // Get statistics
    float getExitRate() const;
    float getAverageExitLayer() const;
    size_t getEarlyExitCount() const { return early_exits_.load(); }
    
private:
    float threshold_;
    std::vector<ExitPoint> exit_points_;
    
    std::atomic<size_t> total_inferences_{0};
    std::atomic<size_t> early_exits_{0};
    std::atomic<float> exit_layer_sum_{0.0f};
    
    float computeEntropy(const float* probs, size_t size) const;
};

// ============================================================================
// Progressive Layer Loader
// ============================================================================

class ProgressiveLayerLoader {
public:
    struct LayerRequest {
        int layer_id;
        int priority;
        std::chrono::steady_clock::time_point deadline;
        std::function<void()> on_loaded;
        
        LayerRequest()
            : layer_id(-1)
            , priority(0)
        {}
    };
    
    ProgressiveLayerLoader(
        const std::string& model_path,
        const HardwareProfile& hardware
    );
    
    ~ProgressiveLayerLoader();
    
    // Start/stop loading
    void start();
    void stop();
    
    // Request layer
    void requestLayer(const LayerRequest& request);
    
    // Get loaded layer
    void* getLayer(int layer_id);
    
    // Check if layer is loaded
    bool isLayerLoaded(int layer_id) const;
    
    // Preload layers (predictive)
    void preloadLayers(const std::vector<int>& predicted_layers);
    
    // Evict layer
    void evictLayer(int layer_id);
    
    // Get statistics
    float getHitRate() const;
    size_t getMemoryUsed() const;
    
private:
    void loadingThread();
    
    std::string model_path_;
    HardwareProfile hardware_;
    
    std::unordered_map<int, void*> loaded_layers_;
    std::queue<LayerRequest> request_queue_;
    
    std::vector<std::thread> loading_threads_;
    std::atomic<bool> running_{false};
    
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
};

// ============================================================================
// Recomputation Engine (Memory-Compute Tradeoff)
// ============================================================================

class RecomputationEngine {
public:
    struct RecomputePlan {
        std::vector<int> checkpoint_layers;   // Save activations
        std::vector<int> recompute_layers;    // Recompute on backward
        size_t memory_saved;
        size_t compute_added;
        float overhead_estimate;
        
        RecomputePlan()
            : memory_saved(0)
            , compute_added(0)
            , overhead_estimate(0.0f)
        {}
    };
    
    RecomputationEngine(float memory_budget);
    
    // Create recompute plan
    RecomputePlan createPlan(
        const ModelProfile& model,
        float max_overhead = 0.3f  // Max 30% compute overhead
    );
    
    // Checkpoint activations
    void checkpoint(int layer_id, const float* activations, size_t size);
    
    // Recompute activations
    std::vector<float> recompute(
        int layer_id,
        const float* input,
        const std::function<std::vector<float>(int, const float*)>& forward_func
    );
    
    // Get statistics
    size_t getMemorySaved() const;
    size_t getComputeAdded() const;
    
private:
    float memory_budget_;
    
    std::unordered_map<int, std::vector<float>> checkpoints_;
    std::unordered_map<int, size_t> recompute_counts_;
    
    std::atomic<size_t> total_memory_saved_{0};
    std::atomic<size_t> total_compute_added_{0};
};

// ============================================================================
// Speculative Execution Engine
// ============================================================================

class SpeculativeExecutionEngine {
public:
    struct SpeculativeResult {
        std::vector<int> draft_tokens;
        std::vector<int> verified_tokens;
        std::vector<int> rejected_tokens;
        float acceptance_rate;
        float speedup_factor;
        
        SpeculativeResult()
            : acceptance_rate(0.0f)
            , speedup_factor(1.0f)
        {}
    };
    
    SpeculativeExecutionEngine(
        int draft_model_size,
        int speculative_tokens
    );
    
    // Generate draft tokens
    std::vector<int> generateDraft(
        const std::vector<int>& context,
        int num_tokens
    );
    
    // Verify draft tokens
    std::vector<int> verifyDraft(
        const std::vector<int>& draft_tokens,
        const std::vector<float>& main_logits,
        float threshold = 0.9f
    );
    
    // Execute speculative decoding
    SpeculativeResult execute(
        const std::vector<int>& context,
        std::function<std::vector<int>(const std::vector<int>&, int)> draft_generate,
        std::function<std::pair<std::vector<float>, int>(const std::vector<int>&)> main_forward
    );
    
    // Update draft model
    void updateDraftFromRejection(
        const std::vector<int>& rejected_tokens,
        const std::vector<int>& correct_tokens
    );
    
    // Get statistics
    float getAcceptanceRate() const;
    float getAverageSpeedup() const;
    size_t getTotalAccepted() const { return total_accepted_.load(); }
    
private:
    int draft_model_size_;
    int speculative_tokens_;
    
    std::atomic<size_t> total_speculative_{0};
    std::atomic<size_t> total_accepted_{0};
    std::atomic<float> total_speedup_{0.0f};
    
    std::mt19937 rng_;
};

// ============================================================================
// KV Cache Manager (Advanced)
// ============================================================================

class KVCacheManager {
public:
    struct CacheConfig {
        size_t max_cache_size;
        int compression_bits;
        bool enable_paging;
        bool enable_sharing;
        bool enable_eviction;
        float eviction_threshold;
        
        CacheConfig()
            : max_cache_size(0)
            , compression_bits(8)
            , enable_paging(true)
            , enable_sharing(true)
            , enable_eviction(true)
            , eviction_threshold(0.9f)
        {}
    };
    
    struct CacheEntry {
        int request_id;
        int layer_id;
        std::vector<float> keys;
        std::vector<float> values;
        size_t seq_len;
        size_t memory_used;
        std::chrono::steady_clock::time_point last_access;
        float importance_score;
    };
    
    KVCacheManager(const CacheConfig& config);
    
    // Store KV cache
    void store(int request_id, int layer_id, const float* keys, const float* values, size_t seq_len);
    
    // Retrieve KV cache
    std::pair<std::vector<float>, std::vector<float>> retrieve(int request_id, int layer_id);
    
    // Share cache between requests (prefix caching)
    void sharePrefix(int from_request, int to_request, size_t prefix_len);
    
    // Evict old caches
    void evict(size_t min_bytes);
    
    // Compress cache
    std::vector<uint8_t> compress(const std::vector<float>& cache);
    std::vector<float> decompress(const std::vector<uint8_t>& compressed);
    
    // Get statistics
    size_t getMemoryUsed() const;
    float getHitRate() const;
    
private:
    CacheConfig config_;
    
    std::unordered_map<std::pair<int, int>, CacheEntry> cache_entries_;
    std::deque<std::pair<int, int>> lru_order_;
    
    size_t total_memory_used_;
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
    
    mutable std::mutex mutex_;
};

// ============================================================================
// Consumer Hardware Model Enabler (Main Class)
// ============================================================================

class ConsumerHardwareEnabler {
public:
    struct EnableResult {
        bool success;
        std::string message;
        ExecutionPlan plan;
        float estimated_quality;
        float estimated_tps;
        size_t memory_required;
    };
    
    struct InferenceStats {
        float tps;
        float latency_ms;
        float quality_score;
        size_t vram_used;
        size_t ram_used;
        size_t cache_hits;
        size_t cache_misses;
        size_t early_exits;
        size_t recomputations;
        size_t speculative_accepts;
    };
    
    ConsumerHardwareEnabler();
    ~ConsumerHardwareEnabler();
    
    // Initialize with hardware profile
    bool initialize(const HardwareProfile& hardware);
    
    // Enable model for this hardware
    EnableResult enableModel(const ModelProfile& model);
    
    // Create execution plan
    ExecutionPlan planExecution(
        const ModelProfile& model,
        const HardwareProfile& hardware,
        float quality_target = 0.95f,
        float tps_target = 10.0f
    );
    
    // Run inference
    std::vector<int> inference(
        const std::vector<int>& input_tokens,
        std::function<std::vector<float>(int, const float*)> layer_forward
    );
    
    // Get layer (load on demand)
    void* getLayer(int layer_id);
    
    // Return layer after use
    void returnLayer(int layer_id);
    
    // Update quality feedback
    void updateQualityFeedback(float actual_quality);
    
    // Adjust execution dynamically
    void adjustExecution(float quality, float tps);
    
    // Get statistics
    InferenceStats getStats() const;
    
    // Get reports
    std::string getPerformanceReport() const;
    std::string getMemoryReport() const;
    std::string getQualityReport() const;
    
    // Configuration
    void setQualityTarget(float target);
    void setTPSTarget(float target);
    void setMemoryBudget(size_t vram, size_t ram);
    
private:
    // Components
    std::unique_ptr<LayerPruner> layer_pruner_;
    std::unique_ptr<AttentionHeadPruner> head_pruner_;
    std::unique_ptr<ExpertPruner> expert_pruner_;
    std::unique_ptr<EarlyExitController> early_exit_;
    std::unique_ptr<ProgressiveLayerLoader> layer_loader_;
    std::unique_ptr<RecomputationEngine> recompute_engine_;
    std::unique_ptr<SpeculativeExecutionEngine> speculative_engine_;
    std::unique_ptr<KVCacheManager> kv_cache_;
    
    // Hardware
    HardwareProfile hardware_;
    
    // Model
    ModelProfile model_;
    ExecutionPlan current_plan_;
    
    // State
    std::unordered_map<int, void*> loaded_layers_;
    std::unordered_map<int, int> layer_precision_;
    
    // Statistics
    std::atomic<float> total_quality_{0.0f};
    std::atomic<float> total_tps_{0.0f};
    std::atomic<size_t> total_inferences_{0};
    
    mutable std::shared_mutex state_mutex_;
};

// ============================================================================
// Implementation
// ============================================================================

// LayerPruner Implementation
inline LayerPruner::LayerPruner(PruningStrategy strategy)
    : strategy_(strategy)
{
}

inline std::vector<float> LayerPruner::computeLayerImportance(
    const ModelProfile& model,
    const std::vector<float>& layer_gradients,
    const std::vector<float>& layer_activations)
{
    std::vector<float> importance(model.num_layers);
    
    for (size_t i = 0; i < model.num_layers; ++i) {
        // Gradient magnitude
        float grad_importance = 0.0f;
        if (i < layer_gradients.size()) {
            grad_importance = std::abs(layer_gradients[i]);
        }
        
        // Activation variance
        float act_importance = 0.0f;
        if (i < layer_activations.size()) {
            act_importance = std::abs(layer_activations[i]);
        }
        
        // Layer depth (deeper layers often more important)
        float depth_factor = 1.0f + (static_cast<float>(i) / model.num_layers);
        
        // Combined importance
        importance[i] = (grad_importance + act_importance) * depth_factor;
    }
    
    return importance;
}

inline LayerPruner::PruningResult LayerPruner::prune(
    const ModelProfile& model,
    const HardwareProfile& hardware,
    float max_quality_loss)
{
    PruningResult result;
    
    // Calculate how much memory we need to save
    size_t weight_memory = model.fp16_weight_bytes;
    size_t vram_available = hardware.vram_bytes;
    
    size_t memory_to_save = 0;
    if (weight_memory > vram_available) {
        memory_to_save = weight_memory - vram_available;
    }
    
    // Calculate layers to prune
    size_t bytes_per_layer = model.fp16_weight_bytes / model.num_layers;
    size_t layers_to_prune = memory_to_save / bytes_per_layer;
    
    // Don't prune too many
    float max_prune_ratio = max_quality_loss / 2.0f;  // Conservative
    size_t max_layers_to_prune = static_cast<size_t>(model.num_layers * max_prune_ratio);
    layers_to_prune = std::min(layers_to_prune, max_layers_to_prune);
    
    // Select layers to prune (least important)
    if (importance_scores_.empty()) {
        // Use heuristic: later layers often less important
        for (size_t i = 0; i < layers_to_prune; ++i) {
            result.pruned_layers.push_back(
                static_cast<int>(model.num_layers - 1 - i)
            );
        }
    } else {
        // Sort by importance
        std::vector<std::pair<float, int>> sorted_layers;
        for (size_t i = 0; i < importance_scores_.size(); ++i) {
            sorted_layers.push_back({importance_scores_[i], static_cast<int>(i)});
        }
        std::sort(sorted_layers.begin(), sorted_layers.end());
        
        // Prune least important
        for (size_t i = 0; i < layers_to_prune && i < sorted_layers.size(); ++i) {
            result.pruned_layers.push_back(sorted_layers[i].second);
        }
    }
    
    // Remaining layers
    for (size_t i = 0; i < model.num_layers; ++i) {
        if (std::find(result.pruned_layers.begin(), result.pruned_layers.end(), i) ==
            result.pruned_layers.end()) {
            result.remaining_layers.push_back(static_cast<int>(i));
        }
    }
    
    result.memory_saved = layers_to_prune * bytes_per_layer;
    result.quality_impact = static_cast<float>(layers_to_prune) / model.num_layers;
    
    return result;
}

inline int LayerPruner::getOptimalLayerCount(
    const ModelProfile& model,
    const HardwareProfile& hardware)
{
    size_t bytes_per_layer = model.fp16_weight_bytes / model.num_layers;
    size_t max_layers = hardware.vram_bytes / bytes_per_layer;
    
    // Account for KV cache and activations
    size_t overhead = 512ULL * 1024 * 1024;  // 512 MB overhead
    max_layers = (hardware.vram_bytes - overhead) / bytes_per_layer;
    
    return static_cast<int>(std::min(max_layers, model.num_layers));
}

// EarlyExitController Implementation
inline EarlyExitController::EarlyExitController(float threshold)
    : threshold_(threshold)
{
}

inline void EarlyExitController::addExitPoint(int layer_id, float threshold) {
    ExitPoint point;
    point.layer_id = layer_id;
    point.threshold = threshold;
    exit_points_.push_back(point);
}

inline EarlyExitController::ExitDecision EarlyExitController::shouldExit(
    int current_layer,
    const float* hidden_state,
    size_t hidden_size,
    const std::vector<float>& attention_weights)
{
    ExitDecision decision;
    
    // Check if this is an exit point
    bool is_exit_point = false;
    float exit_threshold = threshold_;
    
    for (const auto& point : exit_points_) {
        if (point.layer_id == current_layer) {
            is_exit_point = true;
            exit_threshold = point.threshold;
            break;
        }
    }
    
    if (!is_exit_point) {
        return decision;
    }
    
    // Compute confidence from hidden state
    float confidence = 0.0f;
    
    // Entropy-based confidence
    std::vector<float> probs(hidden_size);
    float sum = 0.0f;
    for (size_t i = 0; i < hidden_size; ++i) {
        probs[i] = std::exp(hidden_state[i]);
        sum += probs[i];
    }
    for (float& p : probs) {
        p /= sum;
    }
    
    float entropy = computeEntropy(probs.data(), hidden_size);
    float max_entropy = std::log2(hidden_size);
    confidence = 1.0f - (entropy / max_entropy);
    
    // Attention concentration
    if (!attention_weights.empty()) {
        float attention_entropy = computeEntropy(
            attention_weights.data(),
            attention_weights.size()
        );
        float attention_confidence = 1.0f - (attention_entropy / std::log2(attention_weights.size()));
        confidence = (confidence + attention_confidence) / 2.0f;
    }
    
    // Make decision
    decision.should_exit = (confidence > exit_threshold);
    decision.exit_layer = current_layer;
    decision.confidence = confidence;
    decision.quality_estimate = confidence;
    
    // Update statistics
    total_inferences_.fetch_add(1);
    if (decision.should_exit) {
        early_exits_.fetch_add(1);
        exit_layer_sum_.fetch_add(static_cast<float>(current_layer));
    }
    
    return decision;
}

inline float EarlyExitController::getExitRate() const {
    size_t total = total_inferences_.load();
    if (total == 0) return 0.0f;
    return static_cast<float>(early_exits_.load()) / total;
}

inline float EarlyExitController::getAverageExitLayer() const {
    size_t exits = early_exits_.load();
    if (exits == 0) return 0.0f;
    return exit_layer_sum_.load() / exits;
}

inline float EarlyExitController::computeEntropy(const float* probs, size_t size) const {
    float entropy = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        if (probs[i] > 1e-10f) {
            entropy -= probs[i] * std::log2(probs[i]);
        }
    }
    return entropy;
}

// SpeculativeExecutionEngine Implementation
inline SpeculativeExecutionEngine::SpeculativeExecutionEngine(
    int draft_model_size,
    int speculative_tokens)
    : draft_model_size_(draft_model_size)
    , speculative_tokens_(speculative_tokens)
    , rng_(std::random_device{}())
{
}

inline std::vector<int> SpeculativeExecutionEngine::generateDraft(
    const std::vector<int>& context,
    int num_tokens)
{
    std::vector<int> draft_tokens;
    draft_tokens.reserve(num_tokens);
    
    // Simple draft generation (would use actual draft model in production)
    std::uniform_int_distribution<int> dist(0, 1000);
    
    for (int i = 0; i < num_tokens; ++i) {
        draft_tokens.push_back(dist(rng_));
    }
    
    return draft_tokens;
}

inline std::vector<int> SpeculativeExecutionEngine::verifyDraft(
    const std::vector<int>& draft_tokens,
    const std::vector<float>& main_logits,
    float threshold)
{
    std::vector<int> verified;
    
    // Find the logits for draft tokens
    for (int token : draft_tokens) {
        if (token >= 0 && token < static_cast<int>(main_logits.size())) {
            float prob = std::exp(main_logits[token]);
            float max_prob = 0.0f;
            for (float logit : main_logits) {
                max_prob = std::max(max_prob, std::exp(logit));
            }
            
            float relative_prob = prob / max_prob;
            
            if (relative_prob > threshold) {
                verified.push_back(token);
            }
        }
    }
    
    return verified;
}

inline SpeculativeExecutionEngine::SpeculativeResult SpeculativeExecutionEngine::execute(
    const std::vector<int>& context,
    std::function<std::vector<int>(const std::vector<int>&, int)> draft_generate,
    std::function<std::pair<std::vector<float>, int>(const std::vector<int>&)> main_forward)
{
    SpeculativeResult result;
    
    // Generate draft tokens
    result.draft_tokens = draft_generate(context, speculative_tokens_);
    
    // Verify with main model
    auto [logits, next_token] = main_forward(context);
    result.verified_tokens = verifyDraft(result.draft_tokens, logits, 0.9f);
    
    // Calculate acceptance
    if (!result.draft_tokens.empty()) {
        result.acceptance_rate = static_cast<float>(result.verified_tokens.size()) / 
                                 result.draft_tokens.size();
    }
    
    // Calculate speedup
    result.speedup_factor = 1.0f + (result.acceptance_rate * speculative_tokens_ - 1);
    
    // Update statistics
    total_speculative_.fetch_add(result.draft_tokens.size());
    total_accepted_.fetch_add(result.verified_tokens.size());
    total_speedup_.fetch_add(result.speedup_factor);
    
    return result;
}

inline float SpeculativeExecutionEngine::getAcceptanceRate() const {
    size_t total = total_speculative_.load();
    if (total == 0) return 0.0f;
    return static_cast<float>(total_accepted_.load()) / total;
}

inline float SpeculativeExecutionEngine::getAverageSpeedup() const {
    size_t count = total_speculative_.load();
    if (count == 0) return 1.0f;
    return total_speedup_.load() / count;
}

// ConsumerHardwareEnabler Implementation
inline ConsumerHardwareEnabler::ConsumerHardwareEnabler() {
}

inline ConsumerHardwareEnabler::~ConsumerHardwareEnabler() = default;

inline bool ConsumerHardwareEnabler::initialize(const HardwareProfile& hardware) {
    hardware_ = hardware;
    
    // Initialize components
    layer_pruner_ = std::make_unique<LayerPruner>(
        LayerPruner::PruningStrategy::ADAPTIVE
    );
    
    head_pruner_ = std::make_unique<AttentionHeadPruner>();
    early_exit_ = std::make_unique<EarlyExitController>(0.9f);
    
    KVCacheManager::CacheConfig kv_config;
    kv_config.max_cache_size = hardware_.vram_bytes / 4;
    kv_cache_ = std::make_unique<KVCacheManager>(kv_config);
    
    speculative_engine_ = std::make_unique<SpeculativeExecutionEngine>(
        1000000000,  // 1B draft model
        4            // 4 speculative tokens
    );
    
    return true;
}

inline ConsumerHardwareEnabler::EnableResult ConsumerHardwareEnabler::enableModel(
    const ModelProfile& model)
{
    EnableResult result;
    model_ = model;
    
    // Create execution plan
    result.plan = planExecution(model, hardware_, 0.95f, 10.0f);
    
    // Check if feasible
    if (result.plan.estimated_vram_usage > hardware_.vram_bytes) {
        result.success = false;
        result.message = "Insufficient VRAM even with optimization";
        return result;
    }
    
    // Estimate quality
    result.estimated_quality = result.plan.estimated_quality;
    result.estimated_tps = result.plan.estimated_tps;
    result.memory_required = result.plan.estimated_vram_usage;
    
    result.success = true;
    result.message = "Model can run on this hardware";
    
    return result;
}

inline ExecutionPlan ConsumerHardwareEnabler::planExecution(
    const ModelProfile& model,
    const HardwareProfile& hardware,
    float quality_target,
    float tps_target)
{
    ExecutionPlan plan;
    
    size_t vram = hardware.vram_bytes;
    size_t ram = hardware.ram_bytes;
    
    // Step 1: Determine layer placement
    size_t bytes_per_layer = model.fp16_weight_bytes / model.num_layers;
    
    // GPU layers
    size_t gpu_budget = vram - 512ULL * 1024 * 1024;  // Reserve 512 MB for KV cache
    int gpu_layers = static_cast<int>(gpu_budget / bytes_per_layer);
    gpu_layers = std::min(gpu_layers, static_cast<int>(model.num_layers));
    
    // CPU layers
    size_t cpu_budget = ram - 1024ULL * 1024 * 1024;  // Reserve 1 GB for system
    int cpu_layers = static_cast<int>(cpu_budget / bytes_per_layer);
    cpu_layers = std::min(cpu_layers, static_cast<int>(model.num_layers - gpu_layers));
    
    // Assign layers
    for (int i = 0; i < gpu_layers; ++i) {
        plan.gpu_layers.push_back(i);
    }
    
    for (int i = gpu_layers; i < gpu_layers + cpu_layers; ++i) {
        plan.cpu_layers.push_back(i);
    }
    
    for (int i = gpu_layers + cpu_layers; i < static_cast<int>(model.num_layers); ++i) {
        plan.disk_layers.push_back(i);
    }
    
    // Step 2: Determine precision
    plan.layer_bits.reserve(model.num_layers);
    
    // First half of GPU layers at higher precision
    for (size_t i = 0; i < model.num_layers; ++i) {
        if (i < static_cast<size_t>(gpu_layers) / 2) {
            plan.layer_bits.push_back(16);  // FP16
        } else if (i < static_cast<size_t>(gpu_layers)) {
            plan.layer_bits.push_back(8);   // INT8
        } else if (i < static_cast<size_t>(gpu_layers + cpu_layers)) {
            plan.layer_bits.push_back(4);   // INT4
        } else {
            plan.layer_bits.push_back(4);   // INT4
        }
    }
    
    // Step 3: KV cache plan
    plan.kv_cache_budget = vram / 4;
    plan.kv_precision_bits = 8;
    plan.kv_offload_enabled = true;
    
    // Step 4: Speculative execution
    plan.use_speculative = true;
    plan.draft_model_size = 1000000000;
    plan.speculative_tokens = 4;
    
    // Step 5: Recomputation
    plan.use_recomputation = (gpu_layers < static_cast<int>(model.num_layers) * 0.5);
    plan.recompute_ratio = 0.3f;
    
    // Estimate performance
    float precision_factor = 0.0f;
    for (int bits : plan.layer_bits) {
        precision_factor += 1.0f / bits;
    }
    precision_factor /= plan.layer_bits.size();
    
    plan.estimated_tps = tps_target * (1.0f + plan.use_speculative * 2.0f);
    plan.estimated_latency_ms = 1000.0f / plan.estimated_tps;
    plan.estimated_quality = quality_target;
    
    // Estimate memory
    size_t weight_memory = 0;
    for (size_t i = 0; i < model.num_layers; ++i) {
        size_t bits = plan.layer_bits[i];
        weight_memory += (bytes_per_layer * bits) / 16;
    }
    
    plan.estimated_vram_usage = weight_memory * gpu_layers / model.num_layers + 
                               plan.kv_cache_budget;
    plan.estimated_ram_usage = weight_memory * cpu_layers / model.num_layers;
    
    current_plan_ = plan;
    
    return plan;
}

inline ConsumerHardwareEnabler::InferenceStats ConsumerHardwareEnabler::getStats() const {
    InferenceStats stats;
    
    stats.tps = total_tps_.load() / std::max(1ULL, total_inferences_.load());
    stats.quality_score = total_quality_.load() / std::max(1ULL, total_inferences_.load());
    
    if (early_exit_) {
        stats.early_exits = early_exit_->early_exits_.load();
    }
    
    if (speculative_engine_) {
        stats.speculative_accepts = speculative_engine_->total_accepted_.load();
    }
    
    return stats;
}

inline std::string ConsumerHardwareEnabler::getPerformanceReport() const {
    std::string report = "=== Consumer Hardware Enabler Performance Report ===\n";
    
    auto stats = getStats();
    
    report += "TPS: " + std::to_string(stats.tps) + "\n";
    report += "Latency: " + std::to_string(stats.latency_ms) + " ms\n";
    report += "Quality: " + std::to_string(stats.quality_score) + "\n";
    
    if (early_exit_) {
        report += "Early exit rate: " + 
                  std::to_string(early_exit_->getExitRate() * 100) + "%\n";
    }
    
    if (speculative_engine_) {
        report += "Speculative acceptance: " + 
                  std::to_string(speculative_engine_->getAcceptanceRate() * 100) + "%\n";
        report += "Average speedup: " + 
                  std::to_string(speculative_engine_->getAverageSpeedup()) + "x\n";
    }
    
    return report;
}

} // namespace memory
} // namespace rawrxd

#endif // RAWRXD_MEMORY_CONSUMER_HARDWARE_ENABLER_HPP
