// thinking_effort_adjuster.hpp
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rawrxd {

// Thinking effort levels.
//
// The old four-level names are kept as aliases so existing call sites remain
// source-compatible while the public selector exposes the requested six-level
// OFF/LOW/MEDIUM/HIGH/EXTRA/MAX dial.
enum class ThinkingEffort : uint8_t {
    Off = 0,
    Minimal = 1,
    Low = 1,
    Standard = 2,
    Medium = 2,
    Detailed = 3,
    High = 3,
    Extra = 4,
    Maximum = 5,
    Max = 5
};

enum class ThinkingTaskPreset : uint8_t {
    Reasoning = 0,
    Analysis = 1,
    Creative = 2,
    FactChecking = 3,
    CodeReview = 4,
    ProblemSolving = 5
};

struct ThinkingEffortBudget {
    size_t max_iterations = 100;
    size_t max_tokens = 500;
    size_t max_depth = 5;
    size_t max_branching = 10;
    double max_time_ms = 1000.0;
    size_t max_memory_mb = 64;
    double temperature = 0.7;
    double exploration_rate = 0.3;
    bool enable_parallelism = true;
    bool enable_caching = true;

    static ThinkingEffortBudget from_level(ThinkingEffort level);
};

struct ThinkingEffortPlan {
    ThinkingEffort level = ThinkingEffort::Medium;
    ThinkingTaskPreset preset = ThinkingTaskPreset::Reasoning;
    ThinkingEffortBudget budget;
    float estimated_complexity = 0.0f;
    float importance = 0.5f;
    bool reasoning_enabled = true;
    bool chain_enabled = true;
    bool tree_enabled = false;
    bool beam_enabled = false;
    bool refinement_enabled = true;
    int cycle_multiplier = 1;
    int agent_count = 1;
};

// Memory optimization strategies
enum class MemoryStrategy {
    FullPrecision,
    DynamicQuantization,
    KVCacheOptimization,
    LayerOffloading,
    Hybrid
};

struct TokenMetrics {
    float attention_entropy = 0.0f;
    float attention_variance = 0.0f;
    float token_rarity = 0.0f;
    float quality_score = 0.0f;
    float model_uncertainty = 0.0f;

    float compute_importance() const {
        return 0.3f * attention_entropy +
               0.25f * attention_variance +
               0.2f * token_rarity +
               0.15f * quality_score +
               0.1f * model_uncertainty;
    }
};

enum class PrecisionLevel {
    FP16 = 16,
    INT8 = 8,
    INT4 = 4,
    INT2 = 2
};

struct AdaptiveKVCacheEntry {
    std::vector<int8_t> key_data;
    std::vector<int8_t> value_data;
    PrecisionLevel precision = PrecisionLevel::FP16;
    float scale_factor = 1.0f;
    TokenMetrics metrics;
    size_t last_access_timestamp = 0;
    size_t access_count = 0;

    size_t memory_footprint() const {
        return key_data.size() + value_data.size() +
               sizeof(PrecisionLevel) + sizeof(float) +
               sizeof(TokenMetrics) + sizeof(size_t) * 2;
    }
};

struct LayerMemoryBudget {
    size_t original_bytes = 0;
    size_t quantized_bytes = 0;
    size_t evicted_bytes = 0;
    size_t total_budget = 0;
    float oq_ratio = 0.0f;

    float utilization() const {
        if (total_budget == 0) return 0.0f;
        return static_cast<float>(quantized_bytes + original_bytes) /
               static_cast<float>(total_budget);
    }
};

struct MemoryStats {
    size_t model_weights_bytes = 0;
    size_t kv_cache_bytes = 0;
    size_t activations_bytes = 0;
    size_t total_allocated = 0;
    size_t peak_usage = 0;
    size_t available_memory = 0;

    float memory_efficiency() const {
        if (available_memory == 0) return 0.0f;
        return static_cast<float>(total_allocated) /
               static_cast<float>(available_memory);
    }
};

class ThinkingEffortAdjuster {
public:
    explicit ThinkingEffortAdjuster(size_t max_memory_bytes);
    ~ThinkingEffortAdjuster();

    void set_effort_level(ThinkingEffort level);
    ThinkingEffort get_effort_level() const;
    void set_memory_strategy(MemoryStrategy strategy);
    void set_memory_budget(size_t bytes);
    void enable_adaptive_quantization(bool enable);

    bool load_model_optimized(
        const std::string& model_path,
        std::function<void(float)> progress_callback = nullptr);

    void adjust_for_query_complexity(float estimated_complexity);
    void adjust_for_context_length(size_t context_tokens);
    void adjust_for_memory_pressure(float pressure_ratio);

    void initialize_kv_cache(size_t num_layers, size_t num_heads, size_t head_dim);
    PrecisionLevel determine_token_precision(const TokenMetrics& metrics);
    void quantize_kv_entry(AdaptiveKVCacheEntry& entry, PrecisionLevel precision);
    void evict_low_importance_tokens(float eviction_ratio = 0.2f);

    class PrecisionController {
    public:
        PrecisionController();
        ~PrecisionController();

        void train(const std::vector<std::pair<TokenMetrics, PrecisionLevel>>& training_data);
        PrecisionLevel predict(const TokenMetrics& features);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

    class LayerwiseDecomposer {
    public:
        struct ResidualBlock {
            std::vector<int8_t> sign_matrix;
            std::vector<float> singular_values;
            std::vector<float> left_singular_vectors;
            std::vector<float> right_singular_vectors;
            float importance_score = 0.0f;
        };

        void decompose_weights(const float* weights, size_t rows, size_t cols, int num_iterations);
        std::vector<float> reconstruct_weights(size_t num_blocks_to_load);
        void sort_blocks_by_importance();

        size_t get_block_size(int block_index) const;
        float get_cumulative_importance(int num_blocks) const;

    private:
        std::vector<ResidualBlock> residual_blocks_;
        std::vector<size_t> sorted_indices_;
    };

    MemoryStats get_memory_stats() const;
    float get_current_memory_pressure() const;
    size_t estimate_memory_for_context(size_t context_length) const;

    using MemoryPressureCallback = std::function<void(float pressure_ratio)>;
    void set_memory_pressure_callback(MemoryPressureCallback callback);

    static const char* effort_to_string(ThinkingEffort level);
    static ThinkingEffort effort_from_string(const std::string& text);
    static ThinkingEffort recommend_effort(float complexity, float importance);
    static ThinkingEffortBudget apply_task_preset(ThinkingEffortBudget budget,
                                                  ThinkingTaskPreset preset);

    ThinkingEffortBudget get_current_effort_budget() const;
    ThinkingEffortBudget get_effort_budget(ThinkingEffort level) const;
    ThinkingEffortPlan build_execution_plan(const std::string& query,
                                            ThinkingTaskPreset preset = ThinkingTaskPreset::Reasoning,
                                            float importance = 0.5f) const;

    float estimate_query_complexity(const std::string& query) const;
    size_t estimate_tokens_needed(const std::string& query, ThinkingEffort effort) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rawrxd
