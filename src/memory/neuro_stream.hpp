#ifndef RAWRXD_MEMORY_NEURO_STREAM_HPP
#define RAWRXD_MEMORY_NEURO_STREAM_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rawrxd {
namespace memory {

struct CompressedLayerPattern {
    uint32_t archetype_hash = 0;

    std::vector<float> U;
    std::vector<float> S;
    std::vector<float> V;
    uint32_t rank = 0;

    std::vector<float> codebook;
    std::vector<uint16_t> indices;

    std::vector<float> outlier_values;
    std::vector<uint32_t> outlier_positions;

    float compression_ratio = 1.0f;

    size_t original_bytes = 0;
    size_t compressed_bytes = 0;

    uint32_t input_dim = 0;
    uint32_t output_dim = 0;
};

class NeuralWeightReconstructor {
public:
    struct Decoder {
        std::vector<float> W1;
        std::vector<float> W2;
        std::vector<float> W3;
        std::vector<float> b1;
        std::vector<float> b2;
        std::vector<float> b3;

        static constexpr int latent_dim = 64;
        static constexpr int hidden_dim = 256;
    };

    std::vector<float> reconstruct(const CompressedLayerPattern& pattern,
                                   int /*layer_id*/,
                                   int target_rows,
                                   int target_cols) const {
        if (target_rows <= 0 || target_cols <= 0) {
            return {};
        }

        std::vector<float> weights(static_cast<size_t>(target_rows) * static_cast<size_t>(target_cols), 0.0f);
        const uint32_t rank = std::min<uint32_t>(pattern.rank, static_cast<uint32_t>(pattern.S.size()));

        for (int i = 0; i < target_rows; ++i) {
            for (int j = 0; j < target_cols; ++j) {
                float sum = 0.0f;
                for (uint32_t k = 0; k < rank; ++k) {
                    const size_t u_idx = static_cast<size_t>(i) * pattern.rank + k;
                    const size_t v_idx = static_cast<size_t>(k) * static_cast<size_t>(target_cols) + static_cast<size_t>(j);
                    if (u_idx < pattern.U.size() && v_idx < pattern.V.size()) {
                        sum += pattern.U[u_idx] * pattern.S[k] * pattern.V[v_idx];
                    }
                }
                const size_t out = static_cast<size_t>(i) * static_cast<size_t>(target_cols) + static_cast<size_t>(j);
                weights[out] = sum;
            }
        }

        const size_t limit = std::min(weights.size(), pattern.indices.size());
        for (size_t idx = 0; idx < limit; ++idx) {
            const uint16_t cb = pattern.indices[idx];
            if (cb < pattern.codebook.size()) {
                weights[idx] += pattern.codebook[cb];
            }
        }

        const size_t outlier_n = std::min(pattern.outlier_values.size(), pattern.outlier_positions.size());
        for (size_t i = 0; i < outlier_n; ++i) {
            const uint32_t pos = pattern.outlier_positions[i];
            if (pos < weights.size()) {
                weights[pos] = pattern.outlier_values[i];
            }
        }

        return weights;
    }

    float computeDotProduct(const float* input,
                            const CompressedLayerPattern& pattern,
                            int row_idx,
                            int cols) const {
        if (!input || row_idx < 0 || cols <= 0 || pattern.rank == 0) {
            return 0.0f;
        }

        float output = 0.0f;
        const uint32_t rank = std::min<uint32_t>(pattern.rank, static_cast<uint32_t>(pattern.S.size()));

        for (uint32_t k = 0; k < rank; ++k) {
            const size_t u_idx = static_cast<size_t>(row_idx) * pattern.rank + k;
            if (u_idx >= pattern.U.size()) {
                continue;
            }

            float right = 0.0f;
            for (int j = 0; j < cols; ++j) {
                const size_t v_idx = static_cast<size_t>(k) * static_cast<size_t>(cols) + static_cast<size_t>(j);
                if (v_idx < pattern.V.size()) {
                    right += input[j] * pattern.V[v_idx];
                }
            }
            output += pattern.U[u_idx] * pattern.S[k] * right;
        }

        const size_t base = static_cast<size_t>(row_idx) * static_cast<size_t>(cols);
        for (int j = 0; j < cols; ++j) {
            const size_t idx = base + static_cast<size_t>(j);
            if (idx < pattern.indices.size()) {
                const uint16_t cb = pattern.indices[idx];
                if (cb < pattern.codebook.size()) {
                    output += input[j] * pattern.codebook[cb];
                }
            }
        }

        return output;
    }
};

class ComputeInStorageEngine {
public:
    struct ComputeRequest {
        int layer_id = 0;
        const float* input = nullptr;
        float* output = nullptr;
        size_t input_size = 0;
        size_t output_size = 0;

        std::function<void()> on_complete;
    };

    explicit ComputeInStorageEngine(size_t vram_budget)
        : workspace_size_(std::max<size_t>(1, vram_budget / 4)),
          vram_budget_(vram_budget) {
        workspace_ = allocateGPUMemory(workspace_size_);
    }

    ~ComputeInStorageEngine() {
        freeGPU(workspace_);
        workspace_ = nullptr;
    }

    void registerPattern(int layer_id, const CompressedLayerPattern& pattern) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        pattern_cache_[layer_id] = pattern;
    }

    bool hasPattern(int layer_id) const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return pattern_cache_.find(layer_id) != pattern_cache_.end();
    }

    void compute(const ComputeRequest& req) {
        if (!req.input || !req.output || req.input_size == 0 || req.output_size == 0) {
            return;
        }

        const CompressedLayerPattern* pattern = nullptr;
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = pattern_cache_.find(req.layer_id);
            if (it != pattern_cache_.end()) {
                pattern = &it->second;
            }
        }

        if (!pattern) {
            return;
        }

        if (pattern->compressed_bytes < vram_budget_ / 2) {
            auto d_pattern = copyToGPU(*pattern);
            auto d_weights = reconstructOnGPU(*d_pattern);
            gpuGemm(req.input, req.input_size, d_weights->weights, d_weights->rows, d_weights->cols,
                    req.output, req.output_size);
            freeGPU(d_weights);
            freeGPU(d_pattern);
        } else {
            computePatternByPattern(req, *pattern);
        }

        if (req.on_complete) {
            req.on_complete();
        }
    }

private:
    struct GPUPattern {
        CompressedLayerPattern pattern;
    };

    struct GPUWeights {
        std::vector<float> weights;
        size_t rows = 0;
        size_t cols = 0;
    };

    struct PatternChunk {
        size_t start_row = 0;
        size_t rows = 0;
        std::vector<float> row_buffer;
    };

    void* allocateGPUMemory(size_t bytes) {
        if (bytes == 0) {
            return nullptr;
        }
        return static_cast<void*>(new std::vector<uint8_t>(bytes, 0));
    }

    void freeGPU(void* ptr) {
        if (!ptr) {
            return;
        }
        delete static_cast<std::vector<uint8_t>*>(ptr);
    }

    void freeGPU(GPUPattern* ptr) {
        delete ptr;
    }

    void freeGPU(GPUWeights* ptr) {
        delete ptr;
    }

    GPUPattern* copyToGPU(const CompressedLayerPattern& pattern) {
        auto* gpu = new GPUPattern();
        gpu->pattern = pattern;
        return gpu;
    }

    GPUWeights* reconstructOnGPU(const GPUPattern& pattern) {
        auto* out = new GPUWeights();
        out->rows = pattern.pattern.output_dim;
        out->cols = pattern.pattern.input_dim;
        out->weights = reconstructor_.reconstruct(pattern.pattern, 0, static_cast<int>(out->rows),
                                                  static_cast<int>(out->cols));
        return out;
    }

    void gpuGemm(const float* input,
                 size_t input_size,
                 const std::vector<float>& weights,
                 size_t rows,
                 size_t cols,
                 float* output,
                 size_t output_size) {
        if (!input || !output || cols == 0 || rows == 0) {
            return;
        }

        const size_t used_cols = std::min(input_size, cols);
        const size_t used_rows = std::min(output_size, rows);

        for (size_t i = 0; i < used_rows; ++i) {
            float acc = 0.0f;
            const size_t row_off = i * cols;
            for (size_t j = 0; j < used_cols; ++j) {
                const size_t w = row_off + j;
                if (w < weights.size()) {
                    acc += input[j] * weights[w];
                }
            }
            output[i] = acc;
        }
    }

    PatternChunk loadPatternChunk(const CompressedLayerPattern& pattern, size_t start_row, size_t rows) {
        PatternChunk chunk;
        chunk.start_row = start_row;
        chunk.rows = rows;

        const size_t cols = static_cast<size_t>(pattern.input_dim);
        if (cols == 0 || rows == 0) {
            return chunk;
        }

        const size_t total = rows * cols;
        chunk.row_buffer.resize(total, 0.0f);

        for (size_t r = 0; r < rows; ++r) {
            const int row = static_cast<int>(start_row + r);
            if (row < 0 || row >= static_cast<int>(pattern.output_dim)) {
                continue;
            }
            for (size_t c = 0; c < cols; ++c) {
                const size_t idx = r * cols + c;
                chunk.row_buffer[idx] = reconstructor_.computeDotProduct(nullptr, pattern, row, 0);
            }
        }

        return chunk;
    }

    std::vector<float> reconstructChunk(const PatternChunk& chunk, const CompressedLayerPattern& pattern) {
        const size_t cols = static_cast<size_t>(pattern.input_dim);
        std::vector<float> weights(chunk.rows * cols, 0.0f);

        for (size_t r = 0; r < chunk.rows; ++r) {
            const int row = static_cast<int>(chunk.start_row + r);
            if (row < 0 || row >= static_cast<int>(pattern.output_dim)) {
                continue;
            }
            const auto full_row = reconstructor_.reconstruct(pattern, 0, static_cast<int>(pattern.output_dim),
                                                             static_cast<int>(pattern.input_dim));
            const size_t src_off = static_cast<size_t>(row) * cols;
            const size_t dst_off = r * cols;
            if (src_off + cols <= full_row.size() && dst_off + cols <= weights.size()) {
                std::copy(full_row.begin() + static_cast<std::ptrdiff_t>(src_off),
                          full_row.begin() + static_cast<std::ptrdiff_t>(src_off + cols),
                          weights.begin() + static_cast<std::ptrdiff_t>(dst_off));
            }
        }

        return weights;
    }

    float dotProduct(const float* input, const std::vector<float>& weights, size_t row, size_t cols) {
        if (!input || cols == 0) {
            return 0.0f;
        }

        const size_t row_off = row * cols;
        float sum = 0.0f;
        for (size_t j = 0; j < cols; ++j) {
            const size_t idx = row_off + j;
            if (idx < weights.size()) {
                sum += input[j] * weights[idx];
            }
        }
        return sum;
    }

    void freeChunk(PatternChunk& chunk) {
        chunk.row_buffer.clear();
        chunk.row_buffer.shrink_to_fit();
    }

    void computePatternByPattern(const ComputeRequest& req, const CompressedLayerPattern& pattern) {
        const size_t cols = static_cast<size_t>(pattern.input_dim);
        if (cols == 0) {
            return;
        }

        const size_t bytes_per_row = std::max<size_t>(1, cols * sizeof(float));
        const size_t chunk_rows = std::max<size_t>(1, workspace_size_ / bytes_per_row);

        for (size_t row_start = 0; row_start < req.output_size; row_start += chunk_rows) {
            const size_t rows = std::min(chunk_rows, req.output_size - row_start);
            auto chunk = loadPatternChunk(pattern, row_start, rows);
            auto weights = reconstructChunk(chunk, pattern);

            for (size_t r = 0; r < rows; ++r) {
                req.output[row_start + r] = dotProduct(req.input, weights, r, cols);
            }

            freeChunk(chunk);
        }
    }

    mutable std::mutex cache_mutex_;
    std::unordered_map<int, CompressedLayerPattern> pattern_cache_;
    void* workspace_ = nullptr;
    size_t workspace_size_ = 0;
    size_t vram_budget_ = 0;
    NeuralWeightReconstructor reconstructor_;
};

class ProgressiveResolutionInference {
public:
    struct ResolutionLevel {
        int bits = 8;
        float confidence = 0.0f;
        float cost_ms = 0.0f;
    };

    std::vector<int> generateToken(const std::vector<int>& context, float quality_target) {
        auto draft = generateLowPrecision(context, 2);
        float confidence = estimateConfidence(draft);
        if (confidence >= quality_target) {
            return draft;
        }

        auto medium = generateLowPrecision(context, 4);
        confidence = estimateConfidence(medium);
        if (confidence >= quality_target) {
            return medium;
        }

        return generateLowPrecision(context, 8);
    }

private:
    std::vector<int> generateLowPrecision(const std::vector<int>& context, int bits) {
        std::vector<int> out;
        out.reserve(context.size());

        const int step = std::max(1, bits / 2);
        for (size_t i = 0; i < context.size(); ++i) {
            const int v = context[i];
            const int q = (v / step) * step;
            out.push_back(q);
        }

        if (out.empty()) {
            out.push_back(0);
        }

        return out;
    }

    float estimateConfidence(const std::vector<int>& tokens) {
        if (tokens.empty()) {
            return 0.0f;
        }

        const float mean = static_cast<float>(std::accumulate(tokens.begin(), tokens.end(), 0.0)) /
                           static_cast<float>(tokens.size());
        float variance = 0.0f;
        for (int v : tokens) {
            const float d = static_cast<float>(v) - mean;
            variance += d * d;
        }
        variance /= static_cast<float>(tokens.size());

        return 1.0f / (1.0f + std::sqrt(variance));
    }
};

class HybridNeuroStream {
public:
    struct Config {
        size_t vram_budget = 0;
        size_t pattern_cache_mb = 0;
        bool use_compute_in_storage = true;
        bool use_progressive_resolution = false;
        float quality_target = 0.8f;
    };

    explicit HybridNeuroStream(const Config& config)
        : config_(config),
          compute_engine_(std::make_unique<ComputeInStorageEngine>(config.vram_budget)) {
        pattern_cache_.reserve(config.pattern_cache_mb * 16);
    }

    bool loadPatterns(const std::string& pattern_file) {
        std::ifstream file(pattern_file, std::ios::binary);
        if (!file) {
            return false;
        }

        PatternFileHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || header.magic != kPatternMagic || header.version != kPatternVersion) {
            return false;
        }

        std::unordered_map<int, CompressedLayerPattern> loaded;
        loaded.reserve(header.num_layers);

        for (uint32_t i = 0; i < header.num_layers; ++i) {
            CompressedLayerPattern pattern;

            if (!readPOD(file, pattern.archetype_hash) ||
                !readPOD(file, pattern.rank) ||
                !readPOD(file, pattern.input_dim) ||
                !readPOD(file, pattern.output_dim) ||
                !readPOD(file, pattern.compression_ratio) ||
                !readPOD(file, pattern.original_bytes) ||
                !readPOD(file, pattern.compressed_bytes)) {
                return false;
            }

            if (!readVector(file, pattern.U) ||
                !readVector(file, pattern.S) ||
                !readVector(file, pattern.V) ||
                !readVector(file, pattern.codebook) ||
                !readVector(file, pattern.indices) ||
                !readVector(file, pattern.outlier_values) ||
                !readVector(file, pattern.outlier_positions)) {
                return false;
            }

            loaded.emplace(static_cast<int>(i), std::move(pattern));
        }

        {
            std::lock_guard<std::mutex> lock(pattern_mutex_);
            pattern_cache_ = std::move(loaded);
            for (const auto& kv : pattern_cache_) {
                compute_engine_->registerPattern(kv.first, kv.second);
            }
        }

        return true;
    }

    std::vector<int> inference(const std::vector<int>& input, int max_tokens) {
        std::vector<int> output;
        if (max_tokens <= 0) {
            return output;
        }

        std::vector<float> hidden = embed(input);
        output.reserve(static_cast<size_t>(max_tokens));

        for (int t = 0; t < max_tokens; ++t) {
            std::vector<int> order;
            {
                std::lock_guard<std::mutex> lock(pattern_mutex_);
                order.reserve(pattern_cache_.size());
                for (const auto& kv : pattern_cache_) {
                    order.push_back(kv.first);
                }
            }
            std::sort(order.begin(), order.end());

            for (int layer_id : order) {
                CompressedLayerPattern pattern;
                {
                    std::lock_guard<std::mutex> lock(pattern_mutex_);
                    auto it = pattern_cache_.find(layer_id);
                    if (it == pattern_cache_.end()) {
                        continue;
                    }
                    pattern = it->second;
                }

                hidden = compute_layer(hidden, layer_id, pattern);
            }

            const int token = sample(hidden);
            output.push_back(token);

            hidden.push_back(static_cast<float>(token));
            if (hidden.size() > max_hidden_size_) {
                hidden.erase(hidden.begin());
            }
        }

        if (config_.use_progressive_resolution) {
            return progressive_.generateToken(output, config_.quality_target);
        }

        return output;
    }

private:
    static constexpr uint32_t kPatternMagic = 0x4E535452; // "NSTR"
    static constexpr uint32_t kPatternVersion = 1;

    struct PatternFileHeader {
        uint32_t magic = kPatternMagic;
        uint32_t version = kPatternVersion;
        uint32_t num_layers = 0;
    };

    template <typename T>
    static bool readPOD(std::ifstream& in, T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "readPOD requires POD");
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
        return static_cast<bool>(in);
    }

    template <typename T>
    static bool readVector(std::ifstream& in, std::vector<T>& vec) {
        uint32_t n = 0;
        if (!readPOD(in, n)) {
            return false;
        }
        vec.resize(n);
        if (n == 0) {
            return true;
        }
        in.read(reinterpret_cast<char*>(vec.data()), sizeof(T) * n);
        return static_cast<bool>(in);
    }

    std::vector<float> embed(const std::vector<int>& input) {
        std::vector<float> v;
        v.reserve(input.size());
        for (int token : input) {
            v.push_back(static_cast<float>(token) / 1024.0f);
        }
        if (v.empty()) {
            v.push_back(0.0f);
        }
        return v;
    }

    std::vector<float> compute_layer(const std::vector<float>& input,
                                     int layer_id,
                                     const CompressedLayerPattern& pattern) {
        const size_t out_dim = pattern.output_dim == 0 ? input.size() : static_cast<size_t>(pattern.output_dim);
        const size_t in_dim = pattern.input_dim == 0 ? input.size() : static_cast<size_t>(pattern.input_dim);

        std::vector<float> padded_input(in_dim, 0.0f);
        const size_t cp = std::min(in_dim, input.size());
        std::copy(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(cp), padded_input.begin());

        std::vector<float> out(out_dim, 0.0f);

        if (config_.use_compute_in_storage && compute_engine_->hasPattern(layer_id)) {
            ComputeInStorageEngine::ComputeRequest req;
            req.layer_id = layer_id;
            req.input = padded_input.data();
            req.output = out.data();
            req.input_size = padded_input.size();
            req.output_size = out.size();
            compute_engine_->compute(req);
            return out;
        }

        NeuralWeightReconstructor recon;
        const auto w = recon.reconstruct(pattern, layer_id, static_cast<int>(out_dim), static_cast<int>(in_dim));
        for (size_t r = 0; r < out_dim; ++r) {
            float sum = 0.0f;
            const size_t row_off = r * in_dim;
            for (size_t c = 0; c < in_dim; ++c) {
                const size_t idx = row_off + c;
                if (idx < w.size()) {
                    sum += padded_input[c] * w[idx];
                }
            }
            out[r] = std::tanh(sum);
        }

        return out;
    }

    int sample(const std::vector<float>& logits) {
        if (logits.empty()) {
            return 0;
        }

        size_t best = 0;
        float best_val = -std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < logits.size(); ++i) {
            if (logits[i] > best_val) {
                best_val = logits[i];
                best = i;
            }
        }
        return static_cast<int>(best);
    }

    Config config_;
    std::unordered_map<int, CompressedLayerPattern> pattern_cache_;
    std::unique_ptr<ComputeInStorageEngine> compute_engine_;
    ProgressiveResolutionInference progressive_;

    std::mutex pattern_mutex_;
    static constexpr size_t max_hidden_size_ = 4096;
};

} // namespace memory
} // namespace rawrxd

#endif // RAWRXD_MEMORY_NEURO_STREAM_HPP
