#pragma once

#include "memory/negative_range_model.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rawrxd {

// Unified precision spectrum from dense storage to implicit generation.
enum class UnifiedPrecision : int8_t {
    FP16 = 16,
    BF16 = 15,
    FP8_E4M3 = 14,
    FP8_E5M2 = 13,
    INT8 = 8,
    INT4 = 4,
    INT4_GPTQ = 3,
    INT4_AWQ = 2,
    INT2 = 1,
    INT1 = 0,

    INT_0_75 = -1,
    INT_0_5 = -2,
    INT_0_375 = -3,
    INT_0_25 = -4,
    INT_0_125 = -5,
    INT_0_0625 = -6,

    HASH_SEED = -10,
    HASH_MIXED = -11,
    PROJECTION = -12,

    DELTA_SPARSE = -20,
    DELTA_QUANT = -21,
    LORA = -22,
    ADAPTER = -23,

    SVD_IMPLICIT = -30,
    HYPERNET = -31,
    NERF_STYLE = -32,
    WAVELET = -33,
    FOURIER = -34,

    COMPRESSED_SENSING = -40,
    SPARSE_BASIS = -41,
    DICTIONARY = -42,
};

inline float unified_precision_bits(UnifiedPrecision p) {
    switch (p) {
        case UnifiedPrecision::FP16: return 16.0f;
        case UnifiedPrecision::BF16: return 15.0f;
        case UnifiedPrecision::FP8_E4M3:
        case UnifiedPrecision::FP8_E5M2:
        case UnifiedPrecision::INT8: return 8.0f;
        case UnifiedPrecision::INT4: return 4.0f;
        case UnifiedPrecision::INT4_GPTQ:
        case UnifiedPrecision::INT4_AWQ: return 3.5f;
        case UnifiedPrecision::INT2: return 2.0f;
        case UnifiedPrecision::INT1: return 1.0f;
        case UnifiedPrecision::INT_0_75: return 0.75f;
        case UnifiedPrecision::INT_0_5: return 0.5f;
        case UnifiedPrecision::INT_0_375: return 0.375f;
        case UnifiedPrecision::INT_0_25: return 0.25f;
        case UnifiedPrecision::INT_0_125: return 0.125f;
        case UnifiedPrecision::INT_0_0625: return 0.0625f;
        case UnifiedPrecision::HASH_SEED: return 0.00001f;
        case UnifiedPrecision::HASH_MIXED: return 0.001f;
        case UnifiedPrecision::PROJECTION: return 0.0001f;
        case UnifiedPrecision::DELTA_SPARSE: return -0.5f;
        case UnifiedPrecision::DELTA_QUANT: return -0.25f;
        case UnifiedPrecision::LORA: return -2.0f;
        case UnifiedPrecision::ADAPTER: return -5.0f;
        case UnifiedPrecision::SVD_IMPLICIT: return 0.0001f;
        case UnifiedPrecision::HYPERNET: return 0.001f;
        case UnifiedPrecision::NERF_STYLE: return 0.0005f;
        case UnifiedPrecision::WAVELET: return 0.0002f;
        case UnifiedPrecision::FOURIER: return 0.0003f;
        case UnifiedPrecision::COMPRESSED_SENSING: return 0.00005f;
        case UnifiedPrecision::SPARSE_BASIS: return 0.0001f;
        case UnifiedPrecision::DICTIONARY: return 0.0002f;
        default: return 16.0f;
    }
}

inline float expected_reconstruction_error(UnifiedPrecision p) {
    const float bits = unified_precision_bits(p);

    if (bits <= 0.0f) {
        switch (p) {
            case UnifiedPrecision::LORA: return 0.02f;
            case UnifiedPrecision::SVD_IMPLICIT: return 0.12f;
            case UnifiedPrecision::HYPERNET: return 0.18f;
            case UnifiedPrecision::HASH_SEED: return 0.35f;
            default: return 0.25f;
        }
    }

    // For normalized weights in [-1, 1], quantization error is roughly 2 / 2^bits.
    return 2.0f / std::pow(2.0f, bits);
}

inline NegativePrecision to_negative_precision(UnifiedPrecision p) {
    switch (p) {
        case UnifiedPrecision::FP16: return NegativePrecision::FP16;
        case UnifiedPrecision::INT8: return NegativePrecision::INT8;
        case UnifiedPrecision::INT4:
        case UnifiedPrecision::INT4_GPTQ:
        case UnifiedPrecision::INT4_AWQ: return NegativePrecision::INT4;
        case UnifiedPrecision::INT2: return NegativePrecision::INT2;
        case UnifiedPrecision::INT1: return NegativePrecision::INT1;
        case UnifiedPrecision::INT_0_5: return NegativePrecision::INT_HALF;
        case UnifiedPrecision::INT_0_25: return NegativePrecision::INT_QUARTER;
        case UnifiedPrecision::INT_0_125: return NegativePrecision::INT_EIGHTH;
        case UnifiedPrecision::HASH_SEED:
        case UnifiedPrecision::HASH_MIXED: return NegativePrecision::INT_HASH;
        case UnifiedPrecision::DELTA_SPARSE:
        case UnifiedPrecision::DELTA_QUANT: return NegativePrecision::INT_DIFF;
        case UnifiedPrecision::SVD_IMPLICIT: return NegativePrecision::INT_SVD_IMPLICIT;
        case UnifiedPrecision::HYPERNET:
        case UnifiedPrecision::NERF_STYLE: return NegativePrecision::INT_HYPERNET;
        default: return NegativePrecision::FP16;
    }
}

struct MemoryTier {
    UnifiedPrecision precision = UnifiedPrecision::FP16;
    std::size_t memory_bytes = 0;
    float quality_score = 1.0f;
    float latency_factor = 1.0f;
    bool supports_streaming = false;

    float efficiency_score() const {
        return quality_score / static_cast<float>(memory_bytes == 0 ? 1 : memory_bytes);
    }
};

enum class LayerImportance {
    CRITICAL,
    IMPORTANT,
    STANDARD,
    DISPENSABLE,
    ADAPTATION,
};

struct LayerClassification {
    uint32_t layer_id = 0;
    uint32_t weight_type = 0; // 0=q,1=k,2=v,3=o,4=gate,5=up,6=down
    LayerImportance importance = LayerImportance::STANDARD;
    float sensitivity = 0.5f;
    float redundancy = 0.5f;
    bool can_be_implicit = true;
};

class ExtremeMemoryOptimizer {
public:
    struct Config {
        std::size_t memory_budget_bytes = 0;
        float quality_target = 0.92f;
        float latency_target = 1.5f;
        bool enable_implicit_generation = true;
        bool enable_streaming_weights = false;
        bool enable_adaptive_precision = true;
        bool enable_kv_cache_optimization = true;
        bool enable_layer_offloading = false;

        static Config aggressive(std::size_t budget) {
            Config c;
            c.memory_budget_bytes = budget;
            c.quality_target = 0.85f;
            c.latency_target = 2.0f;
            c.enable_implicit_generation = true;
            c.enable_streaming_weights = true;
            c.enable_adaptive_precision = true;
            c.enable_kv_cache_optimization = true;
            c.enable_layer_offloading = true;
            return c;
        }

        static Config balanced(std::size_t budget) {
            Config c;
            c.memory_budget_bytes = budget;
            c.quality_target = 0.92f;
            c.latency_target = 1.5f;
            c.enable_implicit_generation = true;
            c.enable_streaming_weights = false;
            c.enable_adaptive_precision = true;
            c.enable_kv_cache_optimization = true;
            c.enable_layer_offloading = false;
            return c;
        }

        static Config conservative(std::size_t budget) {
            Config c;
            c.memory_budget_bytes = budget;
            c.quality_target = 0.97f;
            c.latency_target = 1.2f;
            c.enable_implicit_generation = false;
            c.enable_streaming_weights = false;
            c.enable_adaptive_precision = false;
            c.enable_kv_cache_optimization = true;
            c.enable_layer_offloading = false;
            return c;
        }
    };

    struct ModelInfo {
        std::size_t num_layers = 0;
        std::size_t num_heads = 0;
        std::size_t head_dim = 0;
        std::size_t hidden_dim = 0;
        std::size_t intermediate_dim = 0;
        std::size_t vocab_size = 0;
        std::size_t max_context_length = 0;

        std::size_t total_params() const {
            const std::size_t embed = vocab_size * hidden_dim;
            const std::size_t attn = num_layers * 4 * hidden_dim * hidden_dim;
            const std::size_t mlp = num_layers * 3 * hidden_dim * intermediate_dim;
            return embed + attn + mlp;
        }

        std::size_t fp16_memory() const {
            return total_params() * 2;
        }

        std::size_t kv_cache_memory(std::size_t context_length) const {
            return 2ULL * num_layers * 2ULL * num_heads * head_dim * context_length;
        }
    };

    struct CompressionPlan {
        std::vector<LayerClassification> layer_classifications;
        std::unordered_map<uint64_t, UnifiedPrecision> precision_assignments;
        std::size_t total_memory = 0;
        float estimated_quality = 0.0f;
        float estimated_latency = 0.0f;
        float compression_ratio = 1.0f;

        std::string summary() const {
            std::ostringstream ss;
            ss << "=== Compression Plan Summary ===\n";
            ss << "Total Memory: " << (static_cast<double>(total_memory) / 1e9) << " GB\n";
            ss << "Compression Ratio: " << compression_ratio << "x\n";
            ss << "Estimated Quality: " << (estimated_quality * 100.0f) << "%\n";
            ss << "Estimated Latency: " << estimated_latency << "x\n";
            return ss.str();
        }
    };

    struct MemoryStats {
        std::size_t weights_memory = 0;
        std::size_t kv_cache_memory = 0;
        std::size_t activations_memory = 0;
        std::size_t total_used = 0;
        std::size_t available = 0;
        float pressure_ratio = 0.0f;
    };

    class StreamingWeightGenerator {
    public:
        float generate_weight(UnifiedPrecision precision,
                              uint64_t seed,
                              uint32_t layer_id,
                              uint32_t weight_type,
                              std::size_t row,
                              std::size_t col) {
            const uint64_t key = seed ^ (static_cast<uint64_t>(layer_id) << 32) ^
                                 static_cast<uint64_t>(weight_type) ^
                                 (static_cast<uint64_t>(row) * 0x9e3779b97f4a7c15ULL) ^
                                 (static_cast<uint64_t>(col) * 0xbf58476d1ce4e5b9ULL);

            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                auto it = cache_.find(key);
                if (it != cache_.end()) {
                    return it->second;
                }
            }

            const float scale = (precision == UnifiedPrecision::HASH_MIXED) ? 0.85f : 1.0f;
            const float value = hash_generate(seed, layer_id, weight_type, row, col) * scale;

            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                if (cache_.size() < max_cache_entries_) {
                    cache_[key] = value;
                }
            }
            return value;
        }

        void prefetch_weights(uint32_t layer_id, std::size_t start_row, std::size_t end_row) {
            std::thread([this, layer_id, start_row, end_row]() {
                for (std::size_t row = start_row; row < end_row; ++row) {
                    (void)generate_weight(UnifiedPrecision::HASH_SEED, 0xC0FFEEULL, layer_id, 0, row, row);
                }
            }).detach();
        }

        void clear_cache() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            cache_.clear();
        }

    private:
        static float hash_generate(uint64_t seed,
                                   uint32_t layer,
                                   uint32_t type,
                                   std::size_t row,
                                   std::size_t col) {
            uint64_t h = seed;
            h ^= static_cast<uint64_t>(layer) * 0x9e3779b97f4a7c15ULL;
            h ^= static_cast<uint64_t>(type) * 0xbf58476d1ce4e5b9ULL;
            h ^= static_cast<uint64_t>(row) * 0x94d049bb133111ebULL;
            h ^= static_cast<uint64_t>(col) * 0xefcdab8967452301ULL;

            h ^= h >> 33;
            h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;

            const float f = static_cast<float>(h & 0x7FFFFFFFu) / static_cast<float>(0x7FFFFFFFu);
            return f * 2.0f - 1.0f;
        }

        std::unordered_map<uint64_t, float> cache_;
        std::mutex cache_mutex_;
        std::size_t max_cache_entries_ = 1000000;
    };

    explicit ExtremeMemoryOptimizer(const Config& config)
        : config_(config) {
        memory_stats_.available = config.memory_budget_bytes;
    }

    CompressionPlan create_compression_plan(const ModelInfo& model_info,
                                            const std::vector<LayerClassification>& layer_info = {}) {
        model_info_ = model_info;
        CompressionPlan plan;
        plan.layer_classifications = layer_info.empty() ? classify_layers(model_info) : layer_info;
        allocate_budget(plan, config_.memory_budget_bytes);
        current_plan_ = plan;
        refresh_memory_stats();
        return plan;
    }

    std::vector<LayerClassification> classify_layers(const ModelInfo& model_info) const {
        std::vector<LayerClassification> classes;
        classes.reserve(model_info.num_layers * 7);

        for (uint32_t layer = 0; layer < model_info.num_layers; ++layer) {
            classes.push_back({layer, 0, LayerImportance::CRITICAL, 0.95f, 0.10f, false});
            classes.push_back({layer, 1, LayerImportance::CRITICAL, 0.92f, 0.10f, false});
            classes.push_back({layer, 2, LayerImportance::IMPORTANT, 0.85f, 0.20f, false});
            classes.push_back({layer, 3, LayerImportance::IMPORTANT, 0.80f, 0.25f, false});
            classes.push_back({layer, 4, LayerImportance::STANDARD, 0.70f, 0.35f, true});
            classes.push_back({layer, 5, LayerImportance::STANDARD, 0.65f, 0.40f, true});
            classes.push_back({layer, 6, LayerImportance::DISPENSABLE, 0.55f, 0.50f, true});
        }

        if (classes.size() >= 7) {
            for (std::size_t i = 0; i < 7; ++i) {
                classes[i].importance = LayerImportance::CRITICAL;
                classes[i].sensitivity = 0.98f;
            }

            const std::size_t base = classes.size() - 7;
            for (std::size_t i = 0; i < 7; ++i) {
                classes[base + i].importance = LayerImportance::IMPORTANT;
                classes[base + i].sensitivity = std::min(1.0f, classes[base + i].sensitivity + 0.1f);
            }
        }

        return classes;
    }

    void adjust_for_context_length(std::size_t context_length) {
        current_context_length_.store(context_length);
        const std::size_t kv = model_info_.kv_cache_memory(context_length);
        memory_stats_.kv_cache_memory = kv;
        if (config_.memory_budget_bytes > 0) {
            const double ratio = static_cast<double>(kv) / static_cast<double>(config_.memory_budget_bytes);
            current_memory_pressure_.store(static_cast<float>(std::min(2.0, ratio)));
        }
        refresh_memory_stats();
    }

    void adjust_for_memory_pressure(float pressure) {
        current_memory_pressure_.store(std::max(0.0f, pressure));

        if (pressure > 0.9f) {
            for (auto& entry : current_plan_.precision_assignments) {
                if (unified_precision_bits(entry.second) > 0.5f) {
                    entry.second = UnifiedPrecision::INT_0_5;
                }
            }
        }

        refresh_memory_stats();
    }

    void adjust_for_query_complexity(float complexity) {
        if (complexity <= 0.8f) {
            return;
        }

        for (auto& entry : current_plan_.precision_assignments) {
            const uint32_t type = static_cast<uint32_t>(entry.first & 0xFFFFFFFFULL);
            if (type <= 2) {
                entry.second = next_higher_precision(entry.second);
            }
        }

        refresh_memory_stats();
    }

    MemoryStats get_memory_stats() const {
        return memory_stats_;
    }

private:
    static uint64_t key_for(uint32_t layer, uint32_t type) {
        return (static_cast<uint64_t>(layer) << 32) | static_cast<uint64_t>(type);
    }

    std::size_t layer_size(const LayerClassification& layer) const {
        const std::size_t hidden = model_info_.hidden_dim;
        const std::size_t inter = model_info_.intermediate_dim;
        switch (layer.weight_type) {
            case 0:
            case 1:
            case 2:
            case 3: return hidden * hidden;
            case 4:
            case 5: return hidden * inter;
            case 6: return inter * hidden;
            default: return hidden * hidden;
        }
    }

    UnifiedPrecision solve_precision(const LayerClassification& layer,
                                     std::size_t remaining_budget) const {
        (void)remaining_budget;
        switch (layer.importance) {
            case LayerImportance::CRITICAL:
                return config_.quality_target >= 0.95f ? UnifiedPrecision::INT8 : UnifiedPrecision::INT4_AWQ;
            case LayerImportance::IMPORTANT:
                return config_.quality_target >= 0.9f ? UnifiedPrecision::INT4 : UnifiedPrecision::INT2;
            case LayerImportance::STANDARD:
                return config_.enable_implicit_generation ? UnifiedPrecision::SVD_IMPLICIT : UnifiedPrecision::INT1;
            case LayerImportance::DISPENSABLE:
                return config_.enable_implicit_generation ? UnifiedPrecision::HASH_SEED : UnifiedPrecision::INT_0_5;
            case LayerImportance::ADAPTATION:
                return UnifiedPrecision::LORA;
            default:
                return UnifiedPrecision::INT4;
        }
    }

    static UnifiedPrecision next_higher_precision(UnifiedPrecision p) {
        static const std::vector<UnifiedPrecision> order = {
            UnifiedPrecision::INT_0_5,
            UnifiedPrecision::INT1,
            UnifiedPrecision::INT2,
            UnifiedPrecision::INT4,
            UnifiedPrecision::INT8,
            UnifiedPrecision::FP16,
        };

        auto it = std::find(order.begin(), order.end(), p);
        if (it == order.end() || std::next(it) == order.end()) {
            return UnifiedPrecision::FP16;
        }
        return *std::next(it);
    }

    float estimate_quality(const CompressionPlan& plan) const {
        if (plan.precision_assignments.empty()) {
            return 0.0f;
        }

        float weighted_quality = 0.0f;
        float weight_sum = 0.0f;

        for (const auto& pair : plan.precision_assignments) {
            const uint32_t type = static_cast<uint32_t>(pair.first & 0xFFFFFFFFULL);
            const float weight = (type <= 2) ? 2.0f : 1.0f;
            const float quality = 1.0f - expected_reconstruction_error(pair.second);
            weighted_quality += quality * weight;
            weight_sum += weight;
        }

        return (weight_sum > 0.0f) ? (weighted_quality / weight_sum) : 0.0f;
    }

    static float estimate_latency(const CompressionPlan& plan) {
        if (plan.precision_assignments.empty()) {
            return 1.0f;
        }

        float total = 0.0f;
        for (const auto& pair : plan.precision_assignments) {
            const float bits = unified_precision_bits(pair.second);
            if (bits <= 0.0f) {
                total += 2.0f;
            } else if (bits < 4.0f) {
                total += 1.5f;
            } else {
                total += 1.1f;
            }
        }
        return total / static_cast<float>(plan.precision_assignments.size());
    }

    void allocate_budget(CompressionPlan& plan, std::size_t budget) {
        std::vector<std::size_t> idx(plan.layer_classifications.size());
        std::iota(idx.begin(), idx.end(), 0);

        std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
            return plan.layer_classifications[a].sensitivity > plan.layer_classifications[b].sensitivity;
        });

        std::size_t remaining = budget;
        for (const std::size_t i : idx) {
            const LayerClassification& layer = plan.layer_classifications[i];
            const UnifiedPrecision p = solve_precision(layer, remaining);
            const uint64_t key = key_for(layer.layer_id, layer.weight_type);
            plan.precision_assignments[key] = p;

            const float bits = std::max(unified_precision_bits(p), 0.00001f);
            const std::size_t bytes = static_cast<std::size_t>((static_cast<double>(layer_size(layer)) * bits) / 8.0);
            remaining = (bytes >= remaining) ? 0 : (remaining - bytes);
        }

        plan.total_memory = budget - remaining;
        plan.compression_ratio = plan.total_memory == 0
            ? std::numeric_limits<float>::infinity()
            : static_cast<float>(model_info_.fp16_memory()) / static_cast<float>(plan.total_memory);
        plan.estimated_quality = estimate_quality(plan);
        plan.estimated_latency = estimate_latency(plan);
    }

    void refresh_memory_stats() {
        std::size_t weights = current_plan_.total_memory;
        std::size_t kv = memory_stats_.kv_cache_memory;

        memory_stats_.weights_memory = weights;
        memory_stats_.total_used = weights + kv + memory_stats_.activations_memory;
        memory_stats_.available = config_.memory_budget_bytes;

        if (config_.memory_budget_bytes > 0) {
            const double ratio = static_cast<double>(memory_stats_.total_used) /
                                 static_cast<double>(config_.memory_budget_bytes);
            memory_stats_.pressure_ratio = static_cast<float>(ratio);
        } else {
            memory_stats_.pressure_ratio = 0.0f;
        }
    }

    Config config_;
    ModelInfo model_info_;
    CompressionPlan current_plan_;
    MemoryStats memory_stats_;

    std::atomic<float> current_memory_pressure_{0.0f};
    std::atomic<std::size_t> current_context_length_{0};
};

} // namespace rawrxd
