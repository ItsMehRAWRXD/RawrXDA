// thinking_effort_adjuster.cpp
#include "thinking_effort_adjuster.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <fstream>
#include <sstream>
#include <limits>

namespace rawrxd {

namespace {

int effort_rank(ThinkingEffort level) {
    return static_cast<int>(level);
}

ThinkingEffort clamp_effort_rank(int rank) {
    if (rank <= 0) return ThinkingEffort::Off;
    if (rank == 1) return ThinkingEffort::Low;
    if (rank == 2) return ThinkingEffort::Medium;
    if (rank == 3) return ThinkingEffort::High;
    if (rank == 4) return ThinkingEffort::Extra;
    return ThinkingEffort::Max;
}

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

ThinkingEffortBudget ThinkingEffortBudget::from_level(ThinkingEffort level) {
    ThinkingEffortBudget budget;

    switch (level) {
        case ThinkingEffort::Off:
            budget.max_iterations = 1;
            budget.max_tokens = 0;
            budget.max_depth = 0;
            budget.max_branching = 1;
            budget.max_time_ms = 0.1;
            budget.max_memory_mb = 1;
            budget.temperature = 0.0;
            budget.exploration_rate = 0.0;
            budget.enable_parallelism = false;
            budget.enable_caching = false;
            break;
        case ThinkingEffort::Low:
            budget.max_iterations = 10;
            budget.max_tokens = 100;
            budget.max_depth = 2;
            budget.max_branching = 3;
            budget.max_time_ms = 100.0;
            budget.max_memory_mb = 16;
            budget.temperature = 0.3;
            budget.exploration_rate = 0.1;
            budget.enable_parallelism = false;
            budget.enable_caching = true;
            break;
        case ThinkingEffort::Medium:
            budget.max_iterations = 100;
            budget.max_tokens = 500;
            budget.max_depth = 5;
            budget.max_branching = 10;
            budget.max_time_ms = 1000.0;
            budget.max_memory_mb = 64;
            budget.temperature = 0.7;
            budget.exploration_rate = 0.3;
            budget.enable_parallelism = true;
            budget.enable_caching = true;
            break;
        case ThinkingEffort::High:
            budget.max_iterations = 1000;
            budget.max_tokens = 2000;
            budget.max_depth = 10;
            budget.max_branching = 30;
            budget.max_time_ms = 5000.0;
            budget.max_memory_mb = 256;
            budget.temperature = 0.9;
            budget.exploration_rate = 0.5;
            budget.enable_parallelism = true;
            budget.enable_caching = true;
            break;
        case ThinkingEffort::Extra:
            budget.max_iterations = 10000;
            budget.max_tokens = 8000;
            budget.max_depth = 20;
            budget.max_branching = 100;
            budget.max_time_ms = 20000.0;
            budget.max_memory_mb = 1024;
            budget.temperature = 1.0;
            budget.exploration_rate = 0.7;
            budget.enable_parallelism = true;
            budget.enable_caching = true;
            break;
        case ThinkingEffort::Max:
            budget.max_iterations = std::numeric_limits<size_t>::max();
            budget.max_tokens = 32000;
            budget.max_depth = 100;
            budget.max_branching = 1000;
            budget.max_time_ms = 300000.0;
            budget.max_memory_mb = 8192;
            budget.temperature = 1.2;
            budget.exploration_rate = 1.0;
            budget.enable_parallelism = true;
            budget.enable_caching = true;
            break;
    }

    return budget;
}

// ============================================================================
// Impl
// ============================================================================

class ThinkingEffortAdjuster::Impl {
public:
    ThinkingEffort current_effort = ThinkingEffort::Standard;
    MemoryStrategy current_strategy = MemoryStrategy::Hybrid;
    size_t memory_budget = 0;
    bool adaptive_quantization_enabled = true;

    std::vector<std::vector<AdaptiveKVCacheEntry>> kv_cache;
    std::vector<LayerMemoryBudget> layer_budgets;
    std::vector<LayerwiseDecomposer> layer_decomposers;

    std::unique_ptr<PrecisionController> precision_controller;
    MemoryPressureCallback pressure_callback;

    MemoryStats memory_stats;

    struct EffortConfig {
        size_t max_context_tokens;
        float quantization_threshold;
        float eviction_ratio;
        int min_precision_bits;
        bool enable_reasoning;
    };

    std::unordered_map<ThinkingEffort, EffortConfig> effort_configs = {
        {ThinkingEffort::Off,    {512,    0.9f, 0.60f, 2,  false}},
        {ThinkingEffort::Low,    {2048,   0.7f, 0.40f, 2,  false}},
        {ThinkingEffort::Medium, {8192,   0.5f, 0.25f, 4,  true}},
        {ThinkingEffort::High,   {32768,  0.3f, 0.15f, 8,  true}},
        {ThinkingEffort::Extra,  {65536,  0.2f, 0.10f, 8,  true}},
        {ThinkingEffort::Max,    {131072, 0.1f, 0.05f, 16, true}},
    };

    struct ModelDimensions {
        size_t num_layers = 32;
        size_t num_heads = 32;
        size_t head_dim = 128;
        size_t hidden_dim = 4096;
        size_t vocab_size = 32000;
        size_t intermediate_dim = 11008;
    } model_dims;

    size_t calculate_weights_memory() const {
        size_t embed = model_dims.vocab_size * model_dims.hidden_dim * 2;
        size_t attn = model_dims.num_layers * 4 *
                      model_dims.hidden_dim * model_dims.hidden_dim * 2;
        size_t mlp = model_dims.num_layers * 3 *
                     model_dims.hidden_dim * model_dims.intermediate_dim * 2;
        size_t norms = model_dims.num_layers * 2 * model_dims.hidden_dim * 2;
        return embed + attn + mlp + norms;
    }

    size_t calculate_kv_cache_memory(size_t context_length) const {
        return 2 * model_dims.num_layers * 2 *
               model_dims.num_heads * model_dims.head_dim * context_length;
    }

    void quantize_to_int8(const float* input, int8_t* output,
                          size_t size, float& scale) {
        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            max_abs = (std::abs(input[i]) > max_abs) ? std::abs(input[i]) : max_abs;
        }
        if (max_abs < 1e-20f) max_abs = 1e-20f;
        scale = max_abs / 127.0f;
        float inv_scale = 127.0f / max_abs;
        for (size_t i = 0; i < size; ++i) {
            output[i] = static_cast<int8_t>(std::round(input[i] * inv_scale));
        }
    }

    void quantize_to_int4(const float* input, int8_t* output,
                          size_t size, float& scale) {
        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            max_abs = (std::abs(input[i]) > max_abs) ? std::abs(input[i]) : max_abs;
        }
        if (max_abs < 1e-20f) max_abs = 1e-20f;
        scale = max_abs / 7.0f;
        float inv_scale = 7.0f / max_abs;
        for (size_t i = 0; i < size; i += 2) {
            int8_t v1 = static_cast<int8_t>(std::round(input[i] * inv_scale));
            int8_t v2 = (i + 1 < size) ?
                        static_cast<int8_t>(std::round(input[i+1] * inv_scale)) : int8_t(0);
            output[i/2] = static_cast<int8_t>((v1 & 0x0F) | ((v2 & 0x0F) << 4));
        }
    }

    void quantize_to_int2(const float* input, int8_t* output,
                          size_t size, float& scale) {
        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            max_abs = (std::abs(input[i]) > max_abs) ? std::abs(input[i]) : max_abs;
        }
        if (max_abs < 1e-20f) max_abs = 1e-20f;
        scale = max_abs / 1.5f;
        float inv_scale = 1.5f / max_abs;
        for (size_t i = 0; i < size; i += 4) {
            int8_t v1 = static_cast<int8_t>(std::round(input[i] * inv_scale));
            int8_t v2 = (i + 1 < size) ? static_cast<int8_t>(std::round(input[i+1] * inv_scale)) : int8_t(0);
            int8_t v3 = (i + 2 < size) ? static_cast<int8_t>(std::round(input[i+2] * inv_scale)) : int8_t(0);
            int8_t v4 = (i + 3 < size) ? static_cast<int8_t>(std::round(input[i+3] * inv_scale)) : int8_t(0);
            output[i/4] = static_cast<int8_t>((v1 & 0x03) | ((v2 & 0x03) << 2) |
                                              ((v3 & 0x03) << 4) | ((v4 & 0x03) << 6));
        }
    }

    float compute_heavy_hitter_score(const TokenMetrics& /*metrics*/,
                                     const std::vector<float>& attention_weights,
                                     float gamma = 263.81f) {
        if (attention_weights.empty()) return 0.0f;
        float mean_attention = std::accumulate(attention_weights.begin(),
                                               attention_weights.end(), 0.0f)
                              / attention_weights.size();
        float variance = 0.0f;
        for (float w : attention_weights) {
            variance += (w - mean_attention) * (w - mean_attention);
        }
        variance /= attention_weights.size();
        return mean_attention + gamma * variance * variance;
    }
};

// ============================================================================
// PrecisionController MLP
// ============================================================================

class ThinkingEffortAdjuster::PrecisionController::Impl {
public:
    static constexpr int INPUT_DIM = 4;
    static constexpr int HIDDEN_DIM = 128;
    static constexpr int OUTPUT_DIM = 4;

    std::vector<std::vector<float>> W1, W2, W3;
    std::vector<float> b1, b2, b3;

    Impl() {
        W1.assign(INPUT_DIM, std::vector<float>(HIDDEN_DIM));
        W2.assign(HIDDEN_DIM, std::vector<float>(HIDDEN_DIM));
        W3.assign(HIDDEN_DIM, std::vector<float>(OUTPUT_DIM));
        b1.assign(HIDDEN_DIM, 0.0f);
        b2.assign(HIDDEN_DIM, 0.0f);
        b3.assign(OUTPUT_DIM, 0.0f);

        auto random_init = [](float& val) {
            val = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 0.1f;
        };
        for (auto& row : W1) for (auto& val : row) random_init(val);
        for (auto& row : W2) for (auto& val : row) random_init(val);
        for (auto& row : W3) for (auto& val : row) random_init(val);
        for (auto& val : b1) random_init(val);
        for (auto& val : b2) random_init(val);
        for (auto& val : b3) random_init(val);
    }

    std::vector<float> forward(const TokenMetrics& features) {
        std::vector<float> input = {
            features.attention_entropy,
            features.attention_variance,
            features.token_rarity,
            features.quality_score
        };

        std::vector<float> h1(HIDDEN_DIM);
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            h1[j] = b1[j];
            for (int i = 0; i < INPUT_DIM; ++i) h1[j] += input[i] * W1[i][j];
            if (h1[j] < 0.0f) h1[j] = 0.0f;
        }

        std::vector<float> h2(HIDDEN_DIM);
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            h2[j] = b2[j];
            for (int i = 0; i < HIDDEN_DIM; ++i) h2[j] += h1[i] * W2[i][j];
            if (h2[j] < 0.0f) h2[j] = 0.0f;
        }

        std::vector<float> output(OUTPUT_DIM);
        for (int j = 0; j < OUTPUT_DIM; ++j) {
            output[j] = b3[j];
            for (int i = 0; i < HIDDEN_DIM; ++i) output[j] += h2[i] * W3[i][j];
        }

        float max_val = *std::max_element(output.begin(), output.end());
        float sum = 0.0f;
        for (auto& val : output) { val = std::exp(val - max_val); sum += val; }
        if (sum > 0.0f) for (auto& val : output) val /= sum;

        return output;
    }
};

ThinkingEffortAdjuster::PrecisionController::PrecisionController()
    : impl_(std::make_unique<Impl>()) {}

ThinkingEffortAdjuster::PrecisionController::~PrecisionController() = default;

void ThinkingEffortAdjuster::PrecisionController::train(
    const std::vector<std::pair<TokenMetrics, PrecisionLevel>>& /*training_data*/) {
    // Placeholder — would implement gradient descent here
}

PrecisionLevel ThinkingEffortAdjuster::PrecisionController::predict(
    const TokenMetrics& features) {
    auto probs = impl_->forward(features);
    auto max_it = std::max_element(probs.begin(), probs.end());
    auto max_idx = std::distance(probs.begin(), max_it);

    static constexpr PrecisionLevel levels[] = {
        PrecisionLevel::FP16, PrecisionLevel::INT8,
        PrecisionLevel::INT4, PrecisionLevel::INT2
    };
    return levels[max_idx];
}

// ============================================================================
// LayerwiseDecomposer
// ============================================================================

void ThinkingEffortAdjuster::LayerwiseDecomposer::decompose_weights(
    const float* weights, size_t rows, size_t cols, int num_iterations) {

    residual_blocks_.clear();
    residual_blocks_.reserve(num_iterations);

    const size_t total = rows * cols;
    std::vector<float> residual(total, 0.0f);
    if (weights != nullptr) {
        for (size_t i = 0; i < total; ++i) residual[i] = weights[i];
    }

    for (int iter = 0; iter < num_iterations; ++iter) {
        ResidualBlock block;
        block.sign_matrix.resize(total);
        std::vector<float> abs_matrix(total);
        for (size_t i = 0; i < total; ++i) {
            block.sign_matrix[i] = (residual[i] >= 0) ? int8_t(1) : int8_t(-1);
            abs_matrix[i] = std::abs(residual[i]);
        }

        size_t rank = 16;
        block.singular_values.resize(rank, 0.0f);
        block.left_singular_vectors.resize(rank * rows, 0.0f);
        block.right_singular_vectors.resize(rank * cols, 0.0f);
        for (size_t i = 0; i < rank && i < total; ++i) {
            block.singular_values[i] = abs_matrix[i];
        }

        float norm = 0.0f;
        for (size_t i = 0; i < total; ++i) norm += residual[i] * residual[i];
        block.importance_score = std::sqrt(norm);

        residual_blocks_.push_back(std::move(block));
        for (auto& val : residual) val *= 0.5f;
    }

    sort_blocks_by_importance();
}

void ThinkingEffortAdjuster::LayerwiseDecomposer::sort_blocks_by_importance() {
    sorted_indices_.resize(residual_blocks_.size());
    std::iota(sorted_indices_.begin(), sorted_indices_.end(), size_t(0));
    std::sort(sorted_indices_.begin(), sorted_indices_.end(),
              [this](size_t a, size_t b) {
                  return residual_blocks_[a].importance_score >
                         residual_blocks_[b].importance_score;
              });
}

std::vector<float> ThinkingEffortAdjuster::LayerwiseDecomposer::reconstruct_weights(
    size_t num_blocks_to_load) {
    if (residual_blocks_.empty()) return {};
    size_t total_size = residual_blocks_[0].sign_matrix.size();
    std::vector<float> reconstructed(total_size, 0.0f);
    size_t blocks_to_use = std::min(num_blocks_to_load, sorted_indices_.size());
    for (size_t i = 0; i < blocks_to_use; ++i) {
        size_t block_idx = sorted_indices_[i];
        const auto& block = residual_blocks_[block_idx];
        if (block.singular_values.empty()) continue;
        float sv0 = block.singular_values[0];
        for (size_t j = 0; j < total_size; ++j) {
            reconstructed[j] += static_cast<float>(block.sign_matrix[j]) * sv0;
        }
    }
    return reconstructed;
}

size_t ThinkingEffortAdjuster::LayerwiseDecomposer::get_block_size(int block_index) const {
    if (block_index < 0 || static_cast<size_t>(block_index) >= residual_blocks_.size()) return 0;
    const auto& block = residual_blocks_[block_index];
    return block.sign_matrix.size() / 8 +
           block.singular_values.size() * 2 +
           block.left_singular_vectors.size() * 2 +
           block.right_singular_vectors.size() * 2;
}

float ThinkingEffortAdjuster::LayerwiseDecomposer::get_cumulative_importance(int num_blocks) const {
    float total = 0.0f;
    size_t blocks_to_use = std::min(static_cast<size_t>(num_blocks < 0 ? 0 : num_blocks),
                                    sorted_indices_.size());
    for (size_t i = 0; i < blocks_to_use; ++i) {
        total += residual_blocks_[sorted_indices_[i]].importance_score;
    }
    float max_total = 0.0f;
    for (const auto& block : residual_blocks_) max_total += block.importance_score;
    return max_total > 0 ? total / max_total : 0.0f;
}

// ============================================================================
// ThinkingEffortAdjuster
// ============================================================================

ThinkingEffortAdjuster::ThinkingEffortAdjuster(size_t max_memory_bytes)
    : impl_(std::make_unique<Impl>()) {
    impl_->memory_budget = max_memory_bytes;
    impl_->precision_controller = std::make_unique<PrecisionController>();
}

ThinkingEffortAdjuster::~ThinkingEffortAdjuster() = default;

void ThinkingEffortAdjuster::set_effort_level(ThinkingEffort level) {
    impl_->current_effort = level;
}

ThinkingEffort ThinkingEffortAdjuster::get_effort_level() const {
    return impl_->current_effort;
}

void ThinkingEffortAdjuster::set_memory_strategy(MemoryStrategy strategy) {
    impl_->current_strategy = strategy;
}

void ThinkingEffortAdjuster::set_memory_budget(size_t bytes) {
    impl_->memory_budget = bytes;
}

void ThinkingEffortAdjuster::enable_adaptive_quantization(bool enable) {
    impl_->adaptive_quantization_enabled = enable;
}

bool ThinkingEffortAdjuster::load_model_optimized(
    const std::string& /*model_path*/,
    std::function<void(float)> progress_callback) {

    size_t weights_memory = impl_->calculate_weights_memory();

    if (weights_memory > impl_->memory_budget &&
        impl_->current_strategy != MemoryStrategy::FullPrecision) {

        impl_->layer_decomposers.resize(impl_->model_dims.num_layers);

        float progress = 0.0f;
        for (size_t layer = 0; layer < impl_->model_dims.num_layers; ++layer) {
            impl_->layer_decomposers[layer].decompose_weights(
                nullptr, impl_->model_dims.hidden_dim,
                impl_->model_dims.hidden_dim, 4);
            progress = static_cast<float>(layer + 1) / impl_->model_dims.num_layers;
            if (progress_callback) progress_callback(progress);
        }

        size_t remaining_budget = impl_->memory_budget;
        size_t blocks_per_layer = 1;
        for (int b = 1; b <= 16; ++b) {
            size_t total_size = 0;
            for (const auto& decomposer : impl_->layer_decomposers) {
                total_size += decomposer.get_block_size(b);
            }
            if (total_size <= remaining_budget) blocks_per_layer = static_cast<size_t>(b);
            else break;
        }

        for (auto& decomposer : impl_->layer_decomposers) {
            auto weights = decomposer.reconstruct_weights(blocks_per_layer);
            (void)weights;
        }
    }

    return true;
}

void ThinkingEffortAdjuster::adjust_for_query_complexity(float estimated_complexity) {
    if (estimated_complexity > 0.92f)      set_effort_level(ThinkingEffort::Max);
    else if (estimated_complexity > 0.78f) set_effort_level(ThinkingEffort::Extra);
    else if (estimated_complexity > 0.58f) set_effort_level(ThinkingEffort::High);
    else if (estimated_complexity > 0.25f) set_effort_level(ThinkingEffort::Medium);
    else if (estimated_complexity > 0.05f) set_effort_level(ThinkingEffort::Low);
    else                                   set_effort_level(ThinkingEffort::Off);
}

void ThinkingEffortAdjuster::adjust_for_context_length(size_t context_tokens) {
    auto& config = impl_->effort_configs[impl_->current_effort];
    if (context_tokens > config.max_context_tokens) {
        if (impl_->current_strategy == MemoryStrategy::FullPrecision) {
            set_memory_strategy(MemoryStrategy::KVCacheOptimization);
        } else if (impl_->current_strategy == MemoryStrategy::KVCacheOptimization) {
            set_memory_strategy(MemoryStrategy::Hybrid);
        }
    }
}

void ThinkingEffortAdjuster::adjust_for_memory_pressure(float pressure_ratio) {
    if (pressure_ratio > 0.9f)       evict_low_importance_tokens(0.5f);
    else if (pressure_ratio > 0.75f) evict_low_importance_tokens(0.3f);
    else if (pressure_ratio > 0.6f)  evict_low_importance_tokens(0.15f);

    if (impl_->pressure_callback) impl_->pressure_callback(pressure_ratio);
}

void ThinkingEffortAdjuster::initialize_kv_cache(
    size_t num_layers, size_t /*num_heads*/, size_t /*head_dim*/) {
    impl_->kv_cache.assign(num_layers, {});
    impl_->layer_budgets.assign(num_layers, LayerMemoryBudget{});
    for (auto& layer_cache : impl_->kv_cache) layer_cache.reserve(1024);
}

PrecisionLevel ThinkingEffortAdjuster::determine_token_precision(
    const TokenMetrics& metrics) {
    if (!impl_->adaptive_quantization_enabled) {
        auto config = impl_->effort_configs[impl_->current_effort];
        if (config.min_precision_bits >= 16) return PrecisionLevel::FP16;
        if (config.min_precision_bits >= 8)  return PrecisionLevel::INT8;
        if (config.min_precision_bits >= 4)  return PrecisionLevel::INT4;
        return PrecisionLevel::INT2;
    }
    return impl_->precision_controller->predict(metrics);
}

void ThinkingEffortAdjuster::quantize_kv_entry(
    AdaptiveKVCacheEntry& entry, PrecisionLevel precision) {
    entry.precision = precision;
    size_t data_size = entry.key_data.size();
    if (data_size == 0) return;
    std::vector<float> temp_float(data_size, 0.0f);

    switch (precision) {
        case PrecisionLevel::INT8:
            impl_->quantize_to_int8(temp_float.data(),
                                   reinterpret_cast<int8_t*>(entry.key_data.data()),
                                   data_size, entry.scale_factor);
            break;
        case PrecisionLevel::INT4:
            if (entry.key_data.size() >= (data_size + 1) / 2) {
                impl_->quantize_to_int4(temp_float.data(),
                                       reinterpret_cast<int8_t*>(entry.key_data.data()),
                                       data_size, entry.scale_factor);
            }
            break;
        case PrecisionLevel::INT2:
            if (entry.key_data.size() >= (data_size + 3) / 4) {
                impl_->quantize_to_int2(temp_float.data(),
                                       reinterpret_cast<int8_t*>(entry.key_data.data()),
                                       data_size, entry.scale_factor);
            }
            break;
        default:
            break;
    }
}

void ThinkingEffortAdjuster::evict_low_importance_tokens(float eviction_ratio) {
    struct EntryRef {
        size_t layer;
        size_t index;
        float importance;
        size_t last_access;
    };
    std::vector<EntryRef> entries;
    for (size_t layer = 0; layer < impl_->kv_cache.size(); ++layer) {
        for (size_t i = 0; i < impl_->kv_cache[layer].size(); ++i) {
            const auto& entry = impl_->kv_cache[layer][i];
            entries.push_back({layer, i, entry.metrics.compute_importance(), entry.last_access_timestamp});
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const EntryRef& a, const EntryRef& b) {
                  if (std::abs(a.importance - b.importance) < 0.01f) {
                      return a.last_access < b.last_access;
                  }
                  return a.importance < b.importance;
              });

    size_t to_evict = static_cast<size_t>(static_cast<float>(entries.size()) * eviction_ratio);
    for (size_t i = 0; i < to_evict && i < entries.size(); ++i) {
        const auto& ref = entries[i];
        auto& slot = impl_->kv_cache[ref.layer][ref.index];
        slot.precision = PrecisionLevel::INT2;
        slot.key_data.clear();
        slot.value_data.clear();
    }
}

MemoryStats ThinkingEffortAdjuster::get_memory_stats() const {
    return impl_->memory_stats;
}

float ThinkingEffortAdjuster::get_current_memory_pressure() const {
    if (impl_->memory_budget == 0) return 0.0f;
    return static_cast<float>(impl_->memory_stats.total_allocated) /
           static_cast<float>(impl_->memory_budget);
}

size_t ThinkingEffortAdjuster::estimate_memory_for_context(size_t context_length) const {
    return impl_->calculate_kv_cache_memory(context_length);
}

void ThinkingEffortAdjuster::set_memory_pressure_callback(
    MemoryPressureCallback callback) {
    impl_->pressure_callback = std::move(callback);
}

const char* ThinkingEffortAdjuster::effort_to_string(ThinkingEffort level) {
    switch (level) {
        case ThinkingEffort::Off: return "OFF";
        case ThinkingEffort::Low: return "LOW";
        case ThinkingEffort::Medium: return "MEDIUM";
        case ThinkingEffort::High: return "HIGH";
        case ThinkingEffort::Extra: return "EXTRA";
        case ThinkingEffort::Max: return "MAX";
    }
    return "UNKNOWN";
}

ThinkingEffort ThinkingEffortAdjuster::effort_from_string(const std::string& text) {
    const std::string lower = lower_ascii(text);
    if (lower == "off" || lower == "0" || lower == "none") return ThinkingEffort::Off;
    if (lower == "low" || lower == "minimal" || lower == "1") return ThinkingEffort::Low;
    if (lower == "medium" || lower == "standard" || lower == "normal" || lower == "2") return ThinkingEffort::Medium;
    if (lower == "high" || lower == "detailed" || lower == "deep" || lower == "3") return ThinkingEffort::High;
    if (lower == "extra" || lower == "exhaustive" || lower == "4") return ThinkingEffort::Extra;
    if (lower == "max" || lower == "maximum" || lower == "critical" || lower == "5") return ThinkingEffort::Max;
    return ThinkingEffort::Medium;
}

ThinkingEffort ThinkingEffortAdjuster::recommend_effort(float complexity, float importance) {
    const float c = std::clamp(complexity, 0.0f, 1.0f);
    const float i = std::clamp(importance, 0.0f, 1.0f);
    if (i >= 0.95f && c >= 0.50f) return ThinkingEffort::Max;
    const float combined = c * 0.6f + i * 0.4f;

    if (combined < 0.10f) return ThinkingEffort::Off;
    if (combined < 0.25f) return ThinkingEffort::Low;
    if (combined < 0.50f) return ThinkingEffort::Medium;
    if (combined < 0.75f) return ThinkingEffort::High;
    if (combined < 0.90f) return ThinkingEffort::Extra;
    return ThinkingEffort::Max;
}

ThinkingEffortBudget ThinkingEffortAdjuster::apply_task_preset(
    ThinkingEffortBudget budget,
    ThinkingTaskPreset preset) {
    switch (preset) {
        case ThinkingTaskPreset::Reasoning:
            budget.exploration_rate = std::max(budget.exploration_rate, 0.5);
            budget.temperature = std::max(budget.temperature, 0.8);
            break;
        case ThinkingTaskPreset::Analysis:
            budget.exploration_rate = std::min(budget.exploration_rate, 0.3);
            budget.temperature = std::min(budget.temperature, 0.5);
            break;
        case ThinkingTaskPreset::Creative:
            budget.exploration_rate = std::max(budget.exploration_rate, 0.9);
            budget.temperature = std::max(budget.temperature, 1.2);
            budget.max_branching = std::min(budget.max_branching * 2, size_t(2000));
            break;
        case ThinkingTaskPreset::FactChecking:
            budget.exploration_rate = std::min(budget.exploration_rate, 0.1);
            budget.temperature = std::min(budget.temperature, 0.3);
            budget.max_branching = std::min(budget.max_branching, size_t(5));
            break;
        case ThinkingTaskPreset::CodeReview:
            budget.exploration_rate = std::min(std::max(budget.exploration_rate, 0.4), 0.6);
            budget.temperature = std::min(std::max(budget.temperature, 0.5), 0.7);
            budget.max_depth = std::min(budget.max_depth, size_t(8));
            break;
        case ThinkingTaskPreset::ProblemSolving:
            budget.exploration_rate = std::max(budget.exploration_rate, 0.7);
            budget.temperature = std::max(budget.temperature, 1.0);
            budget.max_depth = std::max(budget.max_depth, size_t(15));
            break;
    }
    return budget;
}

ThinkingEffortBudget ThinkingEffortAdjuster::get_current_effort_budget() const {
    return get_effort_budget(impl_->current_effort);
}

ThinkingEffortBudget ThinkingEffortAdjuster::get_effort_budget(ThinkingEffort level) const {
    (void)this;
    return ThinkingEffortBudget::from_level(level);
}

ThinkingEffortPlan ThinkingEffortAdjuster::build_execution_plan(
    const std::string& query,
    ThinkingTaskPreset preset,
    float importance) const {
    ThinkingEffortPlan plan;
    plan.estimated_complexity = estimate_query_complexity(query);
    plan.importance = std::clamp(importance, 0.0f, 1.0f);
    plan.level = recommend_effort(plan.estimated_complexity, plan.importance);
    plan.preset = preset;
    plan.budget = apply_task_preset(ThinkingEffortBudget::from_level(plan.level), preset);
    plan.reasoning_enabled = plan.level != ThinkingEffort::Off && plan.budget.max_tokens > 0;
    plan.chain_enabled = plan.reasoning_enabled;
    plan.tree_enabled = effort_rank(plan.level) >= effort_rank(ThinkingEffort::High);
    plan.beam_enabled = effort_rank(plan.level) >= effort_rank(ThinkingEffort::Extra);
    plan.refinement_enabled = effort_rank(plan.level) >= effort_rank(ThinkingEffort::Medium);
    plan.cycle_multiplier = std::max(1, effort_rank(plan.level));
    plan.agent_count = (plan.level == ThinkingEffort::Off || plan.level == ThinkingEffort::Low) ? 1 :
                       (plan.level == ThinkingEffort::Medium ? 1 :
                       (plan.level == ThinkingEffort::High ? 2 :
                       (plan.level == ThinkingEffort::Extra ? 4 : 8)));
    return plan;
}

float ThinkingEffortAdjuster::estimate_query_complexity(const std::string& query) const {
    const std::string lower = lower_ascii(query);
    float complexity = 0.0f;
    complexity += std::min(1.0f, static_cast<float>(query.length()) / 1000.0f) * 0.3f;

    if (lower.find("why") != std::string::npos ||
        lower.find("how") != std::string::npos ||
        lower.find("explain") != std::string::npos) {
        complexity += 0.2f;
    }
    if (lower.find("prove") != std::string::npos ||
        lower.find("analyze") != std::string::npos ||
        lower.find("compare") != std::string::npos ||
        lower.find("audit") != std::string::npos ||
        lower.find("security") != std::string::npos) {
        complexity += 0.3f;
    }
    if (lower.find("function") != std::string::npos ||
        lower.find("algorithm") != std::string::npos ||
        lower.find("architecture") != std::string::npos ||
        lower.find("performance") != std::string::npos ||
        lower.find("optimize") != std::string::npos ||
        lower.find("calculate") != std::string::npos) {
        complexity += 0.2f;
    }
    return std::min(1.0f, complexity);
}

size_t ThinkingEffortAdjuster::estimate_tokens_needed(
    const std::string& query, ThinkingEffort effort) const {
    if (effort == ThinkingEffort::Off) return 0;

    float complexity = estimate_query_complexity(query);
    size_t base_tokens = std::max<size_t>(1, query.length() / 4);
    float multiplier = 1.0f;
    switch (effort) {
        case ThinkingEffort::Off:    multiplier = 0.0f; break;
        case ThinkingEffort::Low:    multiplier = 1.5f; break;
        case ThinkingEffort::Medium: multiplier = 2.5f; break;
        case ThinkingEffort::High:   multiplier = 4.0f; break;
        case ThinkingEffort::Extra:  multiplier = 6.0f; break;
        case ThinkingEffort::Max:    multiplier = 8.0f; break;
    }
    const size_t estimate = static_cast<size_t>(static_cast<float>(base_tokens) * multiplier * (1.0f + complexity));
    return std::min(estimate, ThinkingEffortBudget::from_level(effort).max_tokens);
}

} // namespace rawrxd
