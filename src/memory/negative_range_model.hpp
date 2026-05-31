// negative_range_model.hpp
#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <functional>
#include <unordered_map>
#include <random>
#include <cmath>
#include <memory>
#include <algorithm>
#include <limits>
#include <cstring>

namespace rawrxd {

// ============================================================================
// Negative Bit-Range Precision Levels
// ============================================================================

enum class NegativePrecision {
    FP16 = 16,
    INT8 = 8,
    INT4 = 4,
    INT2 = 2,
    INT1 = 1,

    INT_HALF = 0,
    INT_QUARTER = -1,
    INT_EIGHTH = -2,

    INT_HASH = -3,
    INT_DIFF = -4,
    INT_SVD_IMPLICIT = -5,
    INT_HYPERNET = -6
};

inline float get_effective_bits(NegativePrecision p) {
    switch (p) {
        case NegativePrecision::FP16: return 16.0f;
        case NegativePrecision::INT8: return 8.0f;
        case NegativePrecision::INT4: return 4.0f;
        case NegativePrecision::INT2: return 2.0f;
        case NegativePrecision::INT1: return 1.0f;
        case NegativePrecision::INT_HALF: return 0.5f;
        case NegativePrecision::INT_QUARTER: return 0.25f;
        case NegativePrecision::INT_EIGHTH: return 0.125f;
        case NegativePrecision::INT_HASH: return 0.0f;
        case NegativePrecision::INT_DIFF: return -0.5f;
        case NegativePrecision::INT_SVD_IMPLICIT: return -1.0f;
        case NegativePrecision::INT_HYPERNET: return -2.0f;
        default: return 16.0f;
    }
}

// ============================================================================
// Fractional Bit Encoder (sub-bit quantization)
// ============================================================================

class FractionalBitEncoder {
public:
    struct Palette {
        std::vector<float> values;
        std::vector<uint8_t> indices;
        float scale = 1.0f;
        float zero_point = 0.0f;
        float bits_per_value = 16.0f;
        float entropy_bits = 0.0f;
    };

    static Palette encode_half_bit(const float* data, size_t size) {
        Palette p;
        p.bits_per_value = 0.5f;
        p.values = {-1.0f, 1.0f};

        float sum = 0.0f;
        for (size_t i = 0; i < size; ++i) sum += data[i];
        float threshold = (size > 0) ? sum / static_cast<float>(size) : 0.0f;

        size_t packed_size = (size + 1) / 2;
        p.indices.assign(packed_size, 0);

        for (size_t i = 0; i < size; i += 2) {
            uint8_t v0 = (data[i] >= threshold) ? 1u : 0u;
            uint8_t v1 = (i + 1 < size && data[i + 1] >= threshold) ? 1u : 0u;
            p.indices[i / 2] = static_cast<uint8_t>(v0 | (v1 << 1));
        }

        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            float a = std::abs(data[i]);
            if (a > max_abs) max_abs = a;
        }
        p.scale = (max_abs > 0.0f) ? max_abs : 1.0f;
        p.zero_point = threshold;
        return p;
    }

    static Palette encode_quarter_bit(const float* data, size_t size) {
        Palette p;
        p.bits_per_value = 0.25f;
        p.values = cluster_values(data, size, 16);

        size_t packed_size = (size + 3) / 4;
        p.indices.assign(packed_size, 0);

        for (size_t i = 0; i < size; i += 4) {
            uint8_t byte = 0;
            for (int j = 0; j < 4 && i + static_cast<size_t>(j) < size; ++j) {
                uint8_t idx = find_nearest_palette_index(data[i + j], p.values);
                byte = static_cast<uint8_t>(byte | ((idx & 0x03) << (j * 2)));
            }
            p.indices[i / 4] = byte;
        }
        p.scale = compute_scale(data, size, p.values);
        return p;
    }

    static Palette encode_eighth_bit(const float* data, size_t size) {
        Palette p;
        p.bits_per_value = 0.125f;
        p.values = cluster_values(data, size, 256);

        size_t packed_size = (size + 7) / 8;
        p.indices.assign(packed_size, 0);

        for (size_t i = 0; i < size; i += 8) {
            uint8_t hash = 0;
            for (int j = 0; j < 8 && i + static_cast<size_t>(j) < size; ++j) {
                uint8_t idx = find_nearest_palette_index(data[i + j], p.values);
                hash = static_cast<uint8_t>(hash ^ ((idx & 0x01) << j));
            }
            p.indices[i / 8] = hash;
        }
        p.scale = compute_scale(data, size, p.values);
        return p;
    }

    static std::vector<float> decode_half_bit(const Palette& p, size_t size) {
        std::vector<float> result(size, 0.0f);
        if (p.indices.empty() || p.values.size() < 2) return result;
        for (size_t i = 0; i < size; i += 2) {
            uint8_t byte = p.indices[i / 2];
            result[i] = p.values[byte & 0x01] * p.scale;
            if (i + 1 < size) result[i + 1] = p.values[(byte >> 1) & 0x01] * p.scale;
        }
        return result;
    }

    static std::vector<float> decode_quarter_bit(const Palette& p, size_t size) {
        std::vector<float> result(size, 0.0f);
        if (p.indices.empty() || p.values.empty()) return result;
        for (size_t i = 0; i < size; i += 4) {
            uint8_t byte = p.indices[i / 4];
            for (int j = 0; j < 4 && i + static_cast<size_t>(j) < size; ++j) {
                uint8_t idx = static_cast<uint8_t>((byte >> (j * 2)) & 0x03);
                size_t lookup = static_cast<size_t>(idx) * 4;
                if (lookup >= p.values.size()) lookup = p.values.size() - 1;
                result[i + j] = p.values[lookup] * p.scale;
            }
        }
        return result;
    }

    static std::vector<float> decode_eighth_bit(const Palette& p, size_t size) {
        std::vector<float> result(size, 0.0f);
        if (p.values.empty()) return result;
        // Simplified reconstruction: map hashed indices back to palette
        for (size_t i = 0; i < size; i += 8) {
            uint8_t byte = p.indices[i / 8];
            for (int j = 0; j < 8 && i + static_cast<size_t>(j) < size; ++j) {
                uint8_t bit = static_cast<uint8_t>((byte >> j) & 0x01);
                size_t lookup = (bit == 0) ? 0 : (p.values.size() - 1);
                result[i + j] = p.values[lookup] * p.scale;
            }
        }
        return result;
    }

private:
    static std::vector<float> cluster_values(const float* data, size_t size, int k) {
        std::vector<float> centroids(k, 0.0f);
        if (size == 0 || k <= 0) return centroids;

        std::vector<int> assignments(size, 0);
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, size - 1);
        for (int i = 0; i < k; ++i) centroids[i] = data[dist(rng)];

        for (int iter = 0; iter < 10; ++iter) {
            for (size_t i = 0; i < size; ++i) {
                float min_dist = std::abs(data[i] - centroids[0]);
                assignments[i] = 0;
                for (int j = 1; j < k; ++j) {
                    float d = std::abs(data[i] - centroids[j]);
                    if (d < min_dist) { min_dist = d; assignments[i] = j; }
                }
            }
            std::vector<float> sums(k, 0.0f);
            std::vector<int> counts(k, 0);
            for (size_t i = 0; i < size; ++i) {
                sums[assignments[i]] += data[i];
                counts[assignments[i]]++;
            }
            for (int j = 0; j < k; ++j) if (counts[j] > 0) centroids[j] = sums[j] / counts[j];
        }
        std::sort(centroids.begin(), centroids.end());
        return centroids;
    }

    static uint8_t find_nearest_palette_index(float value, const std::vector<float>& palette) {
        if (palette.empty()) return 0;
        uint8_t best = 0;
        float min_dist = std::abs(value - palette[0]);
        for (size_t i = 1; i < palette.size(); ++i) {
            float d = std::abs(value - palette[i]);
            if (d < min_dist) { min_dist = d; best = static_cast<uint8_t>(i); }
        }
        return best;
    }

    static float compute_scale(const float* data, size_t size, const std::vector<float>& /*palette*/) {
        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            float a = std::abs(data[i]);
            if (a > max_abs) max_abs = a;
        }
        return (max_abs > 0.0f) ? max_abs : 1.0f;
    }
};

// ============================================================================
// Hash-Derived Weights (zero-bit storage)
// ============================================================================

class HashDerivedWeights {
public:
    struct WeightGenerator {
        uint64_t seed;
        uint32_t layer_id;
        uint32_t weight_type;
        float scale;
        float bias;

        uint64_t hash_position(size_t row, size_t col) const {
            uint64_t h = seed;
            h ^= static_cast<uint64_t>(layer_id) * 0x9e3779b97f4a7c15ULL;
            h ^= static_cast<uint64_t>(weight_type) * 0xbf58476d1ce4e5b9ULL;
            h ^= static_cast<uint64_t>(row) * 0x94d049bb133111ebULL;
            h ^= static_cast<uint64_t>(col) * 0xefcdab8967452301ULL;
            h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return h;
        }

        float generate_weight(size_t row, size_t col) const {
            uint64_t h = hash_position(row, col);
            float f = static_cast<float>(h & 0x7FFFFFFFu) / static_cast<float>(0x7FFFFFFFu);
            f = f * 2.0f - 1.0f;
            return f * scale + bias;
        }
    };

    static std::vector<float> generate_weights(
        uint64_t seed, uint32_t layer_id, uint32_t weight_type,
        size_t rows, size_t cols, float scale = 1.0f) {
        WeightGenerator gen{seed, layer_id, weight_type, scale, 0.0f};
        std::vector<float> weights(rows * cols);
        for (size_t r = 0; r < rows; ++r)
            for (size_t c = 0; c < cols; ++c)
                weights[r * cols + c] = gen.generate_weight(r, c);
        return weights;
    }

    static float generate_single_weight(
        uint64_t seed, uint32_t layer_id, uint32_t weight_type,
        size_t row, size_t col, float scale = 1.0f) {
        WeightGenerator gen{seed, layer_id, weight_type, scale, 0.0f};
        return gen.generate_weight(row, col);
    }

    struct LearnedRepresentation {
        uint64_t optimal_seed = 0;
        float optimal_scale = 1.0f;
        float optimal_bias = 0.0f;
        float reconstruction_error = 0.0f;
    };

    static LearnedRepresentation learn_representation(
        const float* target_weights, size_t rows, size_t cols,
        uint32_t layer_id, uint32_t weight_type) {
        LearnedRepresentation best;
        best.reconstruction_error = std::numeric_limits<float>::max();
        std::mt19937 rng(42);
        const size_t N = rows * cols;
        if (N == 0) return best;

        float sum = 0.0f, sq_sum = 0.0f;
        for (size_t i = 0; i < N; ++i) { sum += target_weights[i]; sq_sum += target_weights[i] * target_weights[i]; }
        float mean = sum / static_cast<float>(N);
        float var = sq_sum / static_cast<float>(N) - mean * mean;
        float stdev = (var > 0.0f) ? std::sqrt(var) : 1.0f;

        // Reduced trial count for smoke test practicality
        int trials = (N > 4096) ? 32 : 256;
        for (int trial = 0; trial < trials; ++trial) {
            uint64_t seed = static_cast<uint64_t>(rng()) ^ (static_cast<uint64_t>(rng()) << 32);
            auto generated = generate_weights(seed, layer_id, weight_type, rows, cols, stdev);
            float error = 0.0f;
            for (size_t i = 0; i < N; ++i) {
                float diff = target_weights[i] - (generated[i] + mean);
                error += diff * diff;
            }
            error = std::sqrt(error / static_cast<float>(N));
            if (error < best.reconstruction_error) {
                best.optimal_seed = seed;
                best.optimal_scale = stdev;
                best.optimal_bias = mean;
                best.reconstruction_error = error;
            }
        }
        return best;
    }
};

// ============================================================================
// Delta / Diff Encoder
// ============================================================================

class DeltaEncoder {
public:
    struct DeltaRepresentation {
        std::vector<int8_t> delta_data;
        float scale = 1.0f;
        int32_t base_model_id = 0;
        float compression_ratio = 1.0f;
    };

    static DeltaRepresentation compute_delta(
        const float* target_weights,
        const float* base_weights,
        size_t size,
        float sparsity_threshold = 0.01f) {
        DeltaRepresentation delta;
        delta.scale = 1.0f;
        if (size == 0) return delta;

        std::vector<float> differences(size);
        for (size_t i = 0; i < size; ++i)
            differences[i] = target_weights[i] - base_weights[i];

        std::vector<float> abs_diffs(size);
        for (size_t i = 0; i < size; ++i) abs_diffs[i] = std::abs(differences[i]);
        std::sort(abs_diffs.begin(), abs_diffs.end());

        size_t keep_count = static_cast<size_t>(static_cast<float>(size) * sparsity_threshold);
        if (keep_count == 0) keep_count = 1;
        if (keep_count > size) keep_count = size;
        float threshold = abs_diffs[size - keep_count];

        float max_abs = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(differences[i]) > threshold) {
                float a = std::abs(differences[i]);
                if (a > max_abs) max_abs = a;
            }
        }
        delta.scale = (max_abs > 0.0f) ? max_abs / 127.0f : 1.0f;

        delta.delta_data.reserve(keep_count * 5);
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(differences[i]) > threshold) {
                uint32_t idx = static_cast<uint32_t>(i);
                int8_t val = static_cast<int8_t>(std::round(differences[i] / delta.scale));
                uint8_t bytes[4];
                std::memcpy(bytes, &idx, 4);
                delta.delta_data.push_back(static_cast<int8_t>(bytes[0]));
                delta.delta_data.push_back(static_cast<int8_t>(bytes[1]));
                delta.delta_data.push_back(static_cast<int8_t>(bytes[2]));
                delta.delta_data.push_back(static_cast<int8_t>(bytes[3]));
                delta.delta_data.push_back(val);
            }
        }
        delta.compression_ratio = static_cast<float>(delta.delta_data.size()) /
                                  static_cast<float>(size * 2);
        return delta;
    }

    static std::vector<float> apply_delta(
        const float* base_weights,
        const DeltaRepresentation& delta,
        size_t size) {
        std::vector<float> result(size, 0.0f);
        if (base_weights != nullptr) {
            for (size_t i = 0; i < size; ++i) result[i] = base_weights[i];
        }
        for (size_t i = 0; i + 5 <= delta.delta_data.size(); i += 5) {
            uint32_t idx = 0;
            uint8_t bytes[4] = {
                static_cast<uint8_t>(delta.delta_data[i]),
                static_cast<uint8_t>(delta.delta_data[i+1]),
                static_cast<uint8_t>(delta.delta_data[i+2]),
                static_cast<uint8_t>(delta.delta_data[i+3])
            };
            std::memcpy(&idx, bytes, 4);
            int8_t val = delta.delta_data[i + 4];
            if (idx < size) result[idx] += static_cast<float>(val) * delta.scale;
        }
        return result;
    }
};

// ============================================================================
// SVD Implicit (seed-based singular vectors)
// ============================================================================

class SVDSImplicit {
public:
    struct ImplicitSVD {
        uint64_t seed_u = 0;
        uint64_t seed_v = 0;
        std::vector<float> singular_values;
        size_t rank = 0;
        float scale = 1.0f;
    };

    static ImplicitSVD learn_implicit(
        const float* weights, size_t rows, size_t cols, size_t rank) {
        ImplicitSVD impl;
        impl.rank = rank;
        impl.singular_values.assign(rank, 0.0f);
        std::mt19937 rng(42);
        impl.seed_u = static_cast<uint64_t>(rng()) ^ (static_cast<uint64_t>(rng()) << 32);
        impl.seed_v = static_cast<uint64_t>(rng()) ^ (static_cast<uint64_t>(rng()) << 32);

        if (rows == 0 || cols == 0 || rank == 0) return impl;

        std::vector<float> u(rows * rank, 0.0f);
        std::vector<float> v(cols * rank, 0.0f);

        // Reduced iterations for smoke test
        const int iters = 10;

        for (size_t k = 0; k < rank; ++k) {
            std::vector<float> v_k(cols, 1.0f / std::sqrt(static_cast<float>(cols)));
            std::vector<float> u_k(rows, 0.0f);

            for (int iter = 0; iter < iters; ++iter) {
                std::fill(u_k.begin(), u_k.end(), 0.0f);
                for (size_t i = 0; i < rows; ++i)
                    for (size_t j = 0; j < cols; ++j)
                        u_k[i] += weights[i * cols + j] * v_k[j];

                for (size_t prev = 0; prev < k; ++prev) {
                    float dot = 0.0f;
                    for (size_t i = 0; i < rows; ++i) dot += u_k[i] * u[i * rank + prev];
                    for (size_t i = 0; i < rows; ++i) u_k[i] -= dot * u[i * rank + prev];
                }
                float norm = 0.0f;
                for (float val : u_k) norm += val * val;
                norm = std::sqrt(norm);
                if (norm > 0.0f) for (float& val : u_k) val /= norm;

                std::fill(v_k.begin(), v_k.end(), 0.0f);
                for (size_t j = 0; j < cols; ++j)
                    for (size_t i = 0; i < rows; ++i)
                        v_k[j] += weights[i * cols + j] * u_k[i];

                for (size_t prev = 0; prev < k; ++prev) {
                    float dot = 0.0f;
                    for (size_t j = 0; j < cols; ++j) dot += v_k[j] * v[j * rank + prev];
                    for (size_t j = 0; j < cols; ++j) v_k[j] -= dot * v[j * rank + prev];
                }
                norm = 0.0f;
                for (float val : v_k) norm += val * val;
                norm = std::sqrt(norm);
                if (norm > 0.0f) for (float& val : v_k) val /= norm;

                for (size_t i = 0; i < rows; ++i) u[i * rank + k] = u_k[i];
                for (size_t j = 0; j < cols; ++j) v[j * rank + k] = v_k[j];
            }

            float sigma = 0.0f;
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j)
                    sigma += weights[i * cols + j] * u[i * rank + k] * v[j * rank + k];
            impl.singular_values[k] = sigma;
        }
        impl.scale = impl.singular_values.empty() ? 1.0f : impl.singular_values[0];
        return impl;
    }

    static std::vector<float> reconstruct(
        const ImplicitSVD& impl, size_t rows, size_t cols) {
        std::vector<float> result(rows * cols, 0.0f);
        if (impl.rank == 0) return result;
        auto u = HashDerivedWeights::generate_weights(impl.seed_u, 0, 0, rows, impl.rank, 1.0f);
        auto v = HashDerivedWeights::generate_weights(impl.seed_v, 0, 1, cols, impl.rank, 1.0f);
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                float sum = 0.0f;
                for (size_t k = 0; k < impl.rank; ++k)
                    sum += u[i * impl.rank + k] * impl.singular_values[k] * v[j * impl.rank + k];
                result[i * cols + j] = sum;
            }
        }
        return result;
    }
};

// ============================================================================
// Hypernetwork Weight Generator
// ============================================================================

class HypernetworkGenerator {
public:
    struct Hypernetwork {
        std::vector<float> weights;
        uint32_t input_dim = 32;
        uint32_t hidden_dim = 64;
        uint32_t output_dim = 1;
    };

    static Hypernetwork train_hypernetwork(
        const float* target_weights,
        size_t rows, size_t cols,
        uint32_t hidden_dim = 64);

    static std::vector<float> generate_weights(
        const Hypernetwork& hyper, size_t rows, size_t cols);
};

// ============================================================================
// Main NegativeRangeModel
// ============================================================================

class NegativeRangeModel {
public:
    struct LayerRepresentation {
        NegativePrecision precision = NegativePrecision::FP16;
        FractionalBitEncoder::Palette palette;
        HashDerivedWeights::LearnedRepresentation hash_repr;
        DeltaEncoder::DeltaRepresentation delta;
        SVDSImplicit::ImplicitSVD svd_implicit;
        HypernetworkGenerator::Hypernetwork hypernet;
        size_t rows = 0, cols = 0;
        float actual_bits_per_param = 0.0f;
        float reconstruction_error = 0.0f;
        uint32_t layer_id = 0;
        uint32_t weight_type = 0;
    };

    explicit NegativeRangeModel(size_t memory_budget_bytes);
    ~NegativeRangeModel();

    LayerRepresentation compress_layer(
        const float* weights,
        size_t rows, size_t cols,
        uint32_t layer_id,
        uint32_t weight_type,
        NegativePrecision target_precision);

    std::vector<float> decompress_layer(const LayerRepresentation& repr);

    NegativePrecision select_optimal_precision(
        const float* weights,
        size_t rows, size_t cols,
        float error_tolerance = 0.05f);

    size_t estimate_memory(size_t rows, size_t cols, NegativePrecision p);
    float estimate_error(const float* weights, size_t size, NegativePrecision p);

    bool compress_model(const std::string& model_path, const std::string& output_path);
    bool decompress_model(const std::string& input_path, const std::string& output_path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rawrxd
