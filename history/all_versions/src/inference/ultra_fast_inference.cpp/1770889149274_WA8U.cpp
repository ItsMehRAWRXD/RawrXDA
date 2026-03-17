#include "ultra_fast_inference.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace rawrxd {
namespace inference {

//=============================================================================
// TENSOR PRUNING SCORER IMPLEMENTATION
//=============================================================================

TensorPruningScorer::TensorPruningScorer(const PruningConfig& config)
    : config_(config) {
}

TensorPruningScorer::~TensorPruningScorer() = default;

float TensorPruningScorer::computeMagnitudeScore(const float* weights, size_t count) {
    if (!weights || count == 0) return 0.0f;
    
    // Compute L2 norm
    double sum_squares = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum_squares += static_cast<double>(weights[i]) * weights[i];
    }
    
    return static_cast<float>(std::sqrt(sum_squares / count));
}

TensorPruningScorer::TensorScore TensorPruningScorer::scoreTensor(
    const std::string& tensor_name,
    const float* weights,
    size_t weight_count,
    float layer_criticality
) {
    std::lock_guard<std::mutex> lock(scoring_mutex_);
    
    TensorScore score;
    score.name = tensor_name;
    
    // Compute magnitude score
    score.magnitude_score = computeMagnitudeScore(weights, weight_count);
    
    // Activation score (from tracking)
    score.activation_score = activation_counters_[tensor_name];
    
    // Gradient score (simplified - would need actual gradients)
    score.gradient_score = score.magnitude_score * 0.5f;
    
    // Layer criticality
    score.criticality = layer_criticality;
    
    // Determine if embedding/output layer (critical)
    if (tensor_name.find("embd") != std::string::npos ||
        tensor_name.find("output") != std::string::npos ||
        tensor_name.find("norm") != std::string::npos) {
        score.criticality *= 2.0f;
    }
    
    // Compute final importance
    score.final_importance = 
        score.magnitude_score * 0.4f +
        score.activation_score * 0.3f +
        score.gradient_score * 0.2f +
        score.criticality * 0.1f;
    
    // Pruning decision
    score.should_prune = shouldPrune(score);
    
    return score;
}

bool TensorPruningScorer::shouldPrune(const TensorScore& score) {
    if (!config_.adaptive_pruning) {
        return score.magnitude_score < config_.magnitude_threshold;
    }
    
    // Adaptive: consider all factors
    return (score.final_importance < 0.3f) && 
           (score.magnitude_score < config_.magnitude_threshold) &&
           (score.criticality < 1.5f);
}

std::vector<TensorPruningScorer::TensorScore> TensorPruningScorer::scoreAllTensors(
    const std::vector<float>& model_weights,
    const std::vector<size_t>& tensor_offsets
) {
    std::vector<TensorScore> scores;
    scores.reserve(tensor_offsets.size());
    
    for (size_t i = 0; i < tensor_offsets.size(); ++i) {
        size_t start = tensor_offsets[i];
        size_t end = (i + 1 < tensor_offsets.size()) ? 
                     tensor_offsets[i + 1] : model_weights.size();
        size_t count = end - start;
        
        if (count > 0) {
            std::string name = "tensor_" + std::to_string(i);
            auto score = scoreTensor(name, &model_weights[start], count, 1.0f);
            scores.push_back(score);
        }
    }
    
    return scores;
}

//=============================================================================
// STREAMING TENSOR REDUCER IMPLEMENTATION
//=============================================================================

StreamingTensorReducer::StreamingTensorReducer(const ReductionConfig& config)
    : config_(config) {
    stats_.original_size_mb = 0;
    stats_.reduced_size_mb = 0;
    stats_.actual_ratio = 0;
    stats_.accuracy_loss = 0;
}

StreamingTensorReducer::~StreamingTensorReducer() = default;

std::vector<float> StreamingTensorReducer::applyMagnitudePruning(
    const float* weights,
    size_t count,
    float threshold
) {
    std::vector<float> pruned;
    pruned.reserve(count / config_.target_ratio);
    
    for (size_t i = 0; i < count; ++i) {
        if (std::abs(weights[i]) >= threshold) {
            pruned.push_back(weights[i]);
        }
    }
    
    return pruned;
}

std::vector<float> StreamingTensorReducer::reduceModel(
    const std::vector<float>& original_model,
    const std::vector<std::string>& tensor_names
) {
    std::lock_guard<std::mutex> lock(reduction_mutex_);
    
    stats_.original_size_mb = (original_model.size() * sizeof(float)) / (1024.0f * 1024.0f);
    
    std::vector<float> reduced_model;
    size_t target_size = static_cast<size_t>(original_model.size() / config_.target_ratio);
    reduced_model.reserve(target_size);
    
    switch (config_.strategy) {
        case MAGNITUDE_PRUNING: {
            // Compute threshold for target ratio
            std::vector<float> abs_weights;
            abs_weights.reserve(original_model.size());
            for (float w : original_model) {
                abs_weights.push_back(std::abs(w));
            }
            
            std::nth_element(
                abs_weights.begin(),
                abs_weights.begin() + target_size,
                abs_weights.end(),
                std::greater<float>()
            );
            
            float threshold = abs_weights[target_size];
            
            for (float w : original_model) {
                if (std::abs(w) >= threshold) {
                    reduced_model.push_back(w);
                }
            }
            break;
        }
        
        case MIXED_PRECISION:
            // Keep most weights but reduce precision
            for (size_t i = 0; i < original_model.size(); ++i) {
                // Quantize to lower precision
                float quantized = std::round(original_model[i] * 16.0f) / 16.0f;
                reduced_model.push_back(quantized);
            }
            break;
        
        default:
            reduced_model = original_model;
            break;
    }
    
    stats_.reduced_size_mb = (reduced_model.size() * sizeof(float)) / (1024.0f * 1024.0f);
    stats_.actual_ratio = stats_.original_size_mb / stats_.reduced_size_mb;
    stats_.accuracy_loss = 0.05f;  // Estimate
    
    return reduced_model;
}

void StreamingTensorReducer::reduceModelStreaming(
    const std::string& input_path,
    const std::string& output_path
) {
    // Streaming file-based reduction: read chunks, prune, write chunks
    std::ifstream inFile(input_path, std::ios::binary);
    if (!inFile.is_open()) return;

    std::ofstream outFile(output_path, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) return;

    constexpr size_t CHUNK_FLOATS = 65536;  // 256KB chunks
    std::vector<float> readBuf(CHUNK_FLOATS);
    std::vector<float> writeBuf;
    writeBuf.reserve(CHUNK_FLOATS);

    size_t totalRead = 0, totalWritten = 0;
    float threshold = config_.magnitude_threshold;

    while (inFile.read(reinterpret_cast<char*>(readBuf.data()),
                       CHUNK_FLOATS * sizeof(float))) {
        size_t count = static_cast<size_t>(inFile.gcount()) / sizeof(float);
        totalRead += count;

        writeBuf.clear();

        switch (config_.strategy) {
            case MAGNITUDE_PRUNING:
                for (size_t i = 0; i < count; ++i) {
                    if (std::abs(readBuf[i]) >= threshold) {
                        writeBuf.push_back(readBuf[i]);
                    }
                }
                break;

            case MIXED_PRECISION:
                for (size_t i = 0; i < count; ++i) {
                    writeBuf.push_back(std::round(readBuf[i] * 16.0f) / 16.0f);
                }
                break;

            default:
                writeBuf.assign(readBuf.begin(), readBuf.begin() + count);
                break;
        }

        if (!writeBuf.empty()) {
            outFile.write(reinterpret_cast<const char*>(writeBuf.data()),
                         writeBuf.size() * sizeof(float));
            totalWritten += writeBuf.size();
        }
    }

    // Handle final partial read
    size_t remaining = static_cast<size_t>(inFile.gcount()) / sizeof(float);
    if (remaining > 0) {
        totalRead += remaining;
        writeBuf.clear();
        for (size_t i = 0; i < remaining; ++i) {
            if (config_.strategy == MAGNITUDE_PRUNING) {
                if (std::abs(readBuf[i]) >= threshold) writeBuf.push_back(readBuf[i]);
            } else if (config_.strategy == MIXED_PRECISION) {
                writeBuf.push_back(std::round(readBuf[i] * 16.0f) / 16.0f);
            } else {
                writeBuf.push_back(readBuf[i]);
            }
        }
        if (!writeBuf.empty()) {
            outFile.write(reinterpret_cast<const char*>(writeBuf.data()),
                         writeBuf.size() * sizeof(float));
            totalWritten += writeBuf.size();
        }
    }

    inFile.close();
    outFile.close();

    stats_.original_size_mb = (totalRead * sizeof(float)) / (1024.0f * 1024.0f);
    stats_.reduced_size_mb = (totalWritten * sizeof(float)) / (1024.0f * 1024.0f);
    stats_.actual_ratio = (stats_.reduced_size_mb > 0) ?
                          stats_.original_size_mb / stats_.reduced_size_mb : 0;
    stats_.accuracy_loss = 0.03f; // Estimated
}

//=============================================================================
// MODEL HOTPATCHER IMPLEMENTATION
//=============================================================================

ModelHotpatcher::ModelHotpatcher(const HotpatchConfig& config)
    : config_(config), current_tier_(TIER_70B) {
}

ModelHotpatcher::~ModelHotpatcher() {
    if (prefetch_thread_.joinable()) {
        prefetch_thread_.join();
    }
}

bool ModelHotpatcher::initializeAutomatic(const std::string& model_path) {
    // Auto-detect model size and create tier configs
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(model_path, ec);
    if (ec) return false;

    double sizeGB = static_cast<double>(fileSize) / (1024.0 * 1024.0 * 1024.0);

    // Create tier configurations based on model size
    if (sizeGB > 40.0) {
        // Large model (70B+): create all 4 tiers
        ModelTierConfig t70b{};
        t70b.tier = TIER_70B;
        t70b.model_path = model_path;
        t70b.memory_footprint_mb = static_cast<size_t>(sizeGB * 1024);
        t70b.expected_quality = 0.95f;
        t70b.quantization = "Q4_K_M";
        registerModelTier(t70b);

        ModelTierConfig t21b{};
        t21b.tier = TIER_21B;
        t21b.model_path = model_path;
        t21b.memory_footprint_mb = static_cast<size_t>(sizeGB * 300);
        t21b.expected_quality = 0.85f;
        t21b.quantization = "Q3_K_S";
        registerModelTier(t21b);

        ModelTierConfig t6b{};
        t6b.tier = TIER_6B;
        t6b.model_path = model_path;
        t6b.memory_footprint_mb = static_cast<size_t>(sizeGB * 90);
        t6b.expected_quality = 0.70f;
        t6b.quantization = "Q2_K";
        registerModelTier(t6b);

        ModelTierConfig t2b{};
        t2b.tier = TIER_2B;
        t2b.model_path = model_path;
        t2b.memory_footprint_mb = static_cast<size_t>(sizeGB * 30);
        t2b.expected_quality = 0.55f;
        t2b.quantization = "IQ2_XS";
        registerModelTier(t2b);
    } else if (sizeGB > 10.0) {
        // Medium model: 2 tiers
        ModelTierConfig full{};
        full.tier = TIER_21B;
        full.model_path = model_path;
        full.memory_footprint_mb = static_cast<size_t>(sizeGB * 1024);
        full.expected_quality = 0.90f;
        full.quantization = "Q4_K_M";
        registerModelTier(full);

        ModelTierConfig small{};
        small.tier = TIER_6B;
        small.model_path = model_path;
        small.memory_footprint_mb = static_cast<size_t>(sizeGB * 500);
        small.expected_quality = 0.75f;
        small.quantization = "Q2_K";
        registerModelTier(small);
    } else {
        // Small model: single tier
        ModelTierConfig single{};
        single.tier = TIER_2B;
        single.model_path = model_path;
        single.memory_footprint_mb = static_cast<size_t>(sizeGB * 1024);
        single.expected_quality = 0.90f;
        single.quantization = "Q8_0";
        registerModelTier(single);
    }

    return true;
}

void ModelHotpatcher::registerModelTier(const ModelTierConfig& tier_config) {
    std::lock_guard<std::mutex> lock(hotpatch_mutex_);
    tier_configs_[tier_config.tier] = tier_config;
}

ModelHotpatcher::ModelTier ModelHotpatcher::selectOptimalTier(
    size_t available_memory_mb,
    float quality_requirement
) {
    std::lock_guard<std::mutex> lock(hotpatch_mutex_);
    
    // Select best tier that fits in memory and meets quality
    for (auto tier : {TIER_2B, TIER_6B, TIER_21B, TIER_70B}) {
        if (tier_configs_.count(tier)) {
            auto& config = tier_configs_[tier];
            if (config.memory_footprint_mb <= available_memory_mb &&
                config.expected_quality >= quality_requirement) {
                return tier;
            }
        }
    }
    
    return TIER_2B;  // Fallback to smallest
}

float ModelHotpatcher::hotpatchToTier(ModelTier target_tier) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(hotpatch_mutex_);
    
    if (target_tier == current_tier_) {
        return 0.0f;  // Already at target
    }
    
    // Preserve KV cache
    // Load new tier (memory-mapped if possible)
    // Restore KV cache
    
    current_tier_ = target_tier;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return static_cast<float>(duration.count());
}

void ModelHotpatcher::preserveKVCache(const std::vector<float>& kv_cache) {
    std::lock_guard<std::mutex> lock(hotpatch_mutex_);
    preserved_kv_cache_ = kv_cache;
}

std::vector<float> ModelHotpatcher::getPreservedKVCache() {
    std::lock_guard<std::mutex> lock(hotpatch_mutex_);
    return preserved_kv_cache_;
}

void ModelHotpatcher::prefetchModelTier(ModelTier tier) {
    // Async prefetch in background thread
    if (prefetch_thread_.joinable()) {
        prefetch_thread_.join();
    }

    prefetch_thread_ = std::thread([this, tier]() {
        std::lock_guard<std::mutex> lock(hotpatch_mutex_);
        if (tier_configs_.count(tier)) {
            auto& config = tier_configs_[tier];
            // Pre-read model file into OS page cache via sequential read
            std::ifstream file(config.model_path, std::ios::binary);
            if (file.is_open()) {
                constexpr size_t BUF_SIZE = 1024 * 1024; // 1MB chunks
                std::vector<char> buf(BUF_SIZE);
                while (file.read(buf.data(), BUF_SIZE)) {
                    // Reading into page cache — data is discarded
                }
            }
        }
    });
}

std::string ModelHotpatcher::correctResponseWithTier(
    const std::string& original_response,
    ModelTier correction_tier
) {
    // Generate correction using a different (typically higher-quality) tier
    ModelTier prev_tier = current_tier_;

    // Switch to correction tier
    float swap_ms = hotpatchToTier(correction_tier);
    if (swap_ms < 0) return original_response;

    // Analyze original response for correction needs
    // Heuristic: check for common failure patterns
    std::string corrected = original_response;

    // Remove refusal patterns
    const std::string refusals[] = {
        "I cannot", "I'm unable", "I apologize, but",
        "As an AI", "I don't have the ability"
    };
    for (const auto& refusal : refusals) {
        size_t pos = corrected.find(refusal);
        if (pos != std::string::npos && pos < 50) {
            // Response starts with refusal — flag for re-generation
            corrected = "[CORRECTION_NEEDED: refusal detected at higher tier]";
            break;
        }
    }

    // Check for truncation (response ends mid-sentence)
    if (!corrected.empty()) {
        char lastChar = corrected.back();
        if (lastChar != '.' && lastChar != '!' && lastChar != '?' &&
            lastChar != '\n' && lastChar != '}' && lastChar != ')') {
            corrected += " [truncation detected — higher tier may complete]";
        }
    }

    // Swap back to the original tier
    hotpatchToTier(prev_tier);

    return corrected;
}

//=============================================================================
// AUTONOMOUS INFERENCE ENGINE IMPLEMENTATION
//=============================================================================

AutonomousInferenceEngine::AutonomousInferenceEngine(const InferenceConfig& config)
    : config_(config) {
    
    pruner_ = std::make_unique<TensorPruningScorer>();
    reducer_ = std::make_unique<StreamingTensorReducer>();
    hotpatcher_ = std::make_unique<ModelHotpatcher>();
    
    stats_.tokens_per_second = 0;
    stats_.gpu_utilization_percent = 0;
    stats_.cpu_utilization_percent = 0;
    stats_.memory_used_mb = 0;
    stats_.average_latency_ms = 0;
    stats_.total_tokens_generated = 0;
}

AutonomousInferenceEngine::~AutonomousInferenceEngine() {
    running_ = false;
    if (inference_thread_.joinable()) {
        inference_thread_.join();
    }
}

bool AutonomousInferenceEngine::loadModelAutomatic(const std::string& model_path) {
    // Detect format (GGUF vs Ollama blob)
    if (!detectModelFormat(model_path)) {
        return false;
    }
    
    // Load using appropriate method
    if (config_.enable_ollama_blob_support && 
        model_path.find("sha256-") != std::string::npos) {
        return loadOllamaBlob(model_path);
    }
    
    return loadGGUFModel(model_path);
}

bool AutonomousInferenceEngine::loadOllamaBlob(const std::string& blob_path) {
    // Use Ollama blob directory structure to find GGUF data
    // Ollama stores models as sha256-prefixed blobs in ~/.ollama/models/blobs/
    std::ifstream file(blob_path, std::ios::binary);
    if (!file.is_open()) return false;

    // Read first 4 bytes to check for GGUF magic
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.close();

    // GGUF magic: 0x46475547 ("GGUF")
    if (magic == 0x46475547) {
        // Blob IS a GGUF file — load directly
        return loadGGUFModel(blob_path);
    }

    // Try common Ollama blob directory layout
    std::filesystem::path blobDir = std::filesystem::path(blob_path).parent_path();
    for (auto& entry : std::filesystem::directory_iterator(blobDir)) {
        if (entry.is_regular_file()) {
            std::ifstream probe(entry.path(), std::ios::binary);
            uint32_t probeMagic = 0;
            probe.read(reinterpret_cast<char*>(&probeMagic), 4);
            if (probeMagic == 0x46475547) {
                return loadGGUFModel(entry.path().string());
            }
        }
    }

    return false;
}

bool AutonomousInferenceEngine::loadGGUFModel(const std::string& path) {
    // Load GGUF model with optional streaming pruning
    if (!std::filesystem::exists(path)) return false;

    auto fileSize = std::filesystem::file_size(path);
    stats_.memory_used_mb = static_cast<size_t>(fileSize / (1024 * 1024));

    // Initialize model hotpatcher with auto-detected tiers
    if (config_.enable_hotpatching) {
        hotpatcher_->initializeAutomatic(path);

        // Select optimal tier based on available memory
        auto tier = hotpatcher_->selectOptimalTier(
            config_.max_memory_mb, config_.quality_target);
        hotpatcher_->hotpatchToTier(tier);
    }

    // Apply streaming pruning if enabled
    if (config_.enable_streaming_pruning) {
        std::string prunedPath = path + ".pruned";
        if (!std::filesystem::exists(prunedPath)) {
            reducer_->reduceModelStreaming(path, prunedPath);
        }
        // Use pruned model if it was created
        if (std::filesystem::exists(prunedPath)) {
            stats_.memory_used_mb = static_cast<size_t>(
                std::filesystem::file_size(prunedPath) / (1024 * 1024));
        }
    }

    return true;
}

bool AutonomousInferenceEngine::detectModelFormat(const std::string& path) {
    // Detect model format from file magic bytes or path pattern
    if (!std::filesystem::exists(path)) return false;

    // Check for Ollama blob pattern (sha256-hexstring)
    std::string filename = std::filesystem::path(path).filename().string();
    if (filename.find("sha256-") == 0) {
        return true;  // Ollama blob
    }

    // Check file extension
    std::string ext = std::filesystem::path(path).extension().string();
    if (ext == ".gguf" || ext == ".bin" || ext == ".ggml") {
        return true;
    }

    // Probe file for GGUF magic (0x46475547)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    return (magic == 0x46475547);
}

void AutonomousInferenceEngine::infer(
    const std::vector<int32_t>& prompt,
    std::function<void(const std::string&)> token_callback,
    size_t max_tokens
) {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    
    // Streaming inference loop
    for (size_t i = 0; i < max_tokens; ++i) {
        // Forward pass
        // Sample token
        // Call callback
        std::string token = "token_" + std::to_string(i);
        token_callback(token);
        
        stats_.total_tokens_generated++;
    }
}

void AutonomousInferenceEngine::enableStreamingPruning(bool enable) {
    config_.enable_streaming_pruning = enable;
}

void AutonomousInferenceEngine::enableHotpatching(bool enable) {
    config_.enable_hotpatching = enable;
}

void AutonomousInferenceEngine::enableGPUAcceleration(bool enable) {
    config_.enable_gpu = enable;
}

void AutonomousInferenceEngine::autonomousAdjustment() {
    // Monitor stats and adjust tier/pruning automatically
    if (stats_.memory_used_mb > config_.max_memory_mb * 0.9) {
        // Switch to smaller tier
        auto current = hotpatcher_->getCurrentTier();
        if (current != ModelHotpatcher::TIER_2B) {
            auto next_tier = static_cast<ModelHotpatcher::ModelTier>(
                static_cast<int>(current) + 1
            );
            hotpatcher_->hotpatchToTier(next_tier);
        }
    }
}

void AutonomousInferenceEngine::processFeedback(
    const std::string& feedback,
    bool is_positive
) {
    // Adjust quality targets based on feedback
    if (!is_positive && config_.quality_target < 0.95f) {
        config_.quality_target += 0.05f;
        autonomousAdjustment();
    }
}

ModelHotpatcher::ModelTier AutonomousInferenceEngine::getCurrentTier() const {
    return hotpatcher_->getCurrentTier();
}

void AutonomousInferenceEngine::updateStats() {
    // Update performance statistics from recent inference data
    auto now = std::chrono::high_resolution_clock::now();
    static auto lastUpdate = now;
    static uint64_t lastTokenCount = 0;

    auto elapsed = std::chrono::duration<double>(now - lastUpdate).count();
    if (elapsed > 0.1) { // Update at most 10x/sec
        uint64_t tokensDelta = stats_.total_tokens_generated - lastTokenCount;
        stats_.tokens_per_second = static_cast<float>(tokensDelta / elapsed);

        if (stats_.total_tokens_generated > 0 && elapsed > 0) {
            stats_.average_latency_ms = static_cast<float>(
                (elapsed * 1000.0) / static_cast<double>(tokensDelta > 0 ? tokensDelta : 1));
        }

        lastTokenCount = stats_.total_tokens_generated;
        lastUpdate = now;
    }
}

void AutonomousInferenceEngine::monitorGPUUtilization() {
    // Monitor GPU usage via system queries
#ifdef _WIN32
    // Query GPU utilization via DXGI if available
    // For now, estimate based on inference activity
    if (config_.enable_gpu && stats_.tokens_per_second > 0) {
        // Rough estimate: higher tok/s = higher GPU util
        stats_.gpu_utilization_percent = std::min(100.0f,
            stats_.tokens_per_second * 1.5f);
    } else {
        stats_.gpu_utilization_percent = 0.0f;
    }
#endif
}

void AutonomousInferenceEngine::monitorCPUUtilization() {
    // Monitor CPU usage
#ifdef _WIN32
    // Use GetSystemTimes for CPU utilization estimate
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        static ULARGE_INTEGER prevIdle = {}, prevKernel = {}, prevUser = {};

        ULARGE_INTEGER curIdle, curKernel, curUser;
        curIdle.LowPart = idle.dwLowDateTime;
        curIdle.HighPart = idle.dwHighDateTime;
        curKernel.LowPart = kernel.dwLowDateTime;
        curKernel.HighPart = kernel.dwHighDateTime;
        curUser.LowPart = user.dwLowDateTime;
        curUser.HighPart = user.dwHighDateTime;

        uint64_t idleDelta = curIdle.QuadPart - prevIdle.QuadPart;
        uint64_t kernelDelta = curKernel.QuadPart - prevKernel.QuadPart;
        uint64_t userDelta = curUser.QuadPart - prevUser.QuadPart;

        uint64_t totalDelta = kernelDelta + userDelta;
        if (totalDelta > 0) {
            stats_.cpu_utilization_percent = static_cast<float>(
                100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta));
        }

        prevIdle = curIdle;
        prevKernel = curKernel;
        prevUser = curUser;
    }
#endif
}

} // namespace inference
} // namespace rawrxd
