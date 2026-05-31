// ============================================================================
// prometheus_lowmem.cpp — Prometheus 120B Low-Memory Runtime for RawrXD
// Integrates with existing CPUInferenceEngine + RawrXDInference backend.
// ============================================================================
#include "prometheus_lowmem.h"
#include "prometheus_config_lowmem.h"
#include "cpu_inference_engine.h"
#include "rawrxd_inference.h"
#include "agentic_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Prometheus {

// ============================================================================
// PrometheusLowMemory — Thin wrapper over RawrXD::CPUInferenceEngine
// ============================================================================

class PrometheusLowMemoryImpl : public PrometheusLowMemory {
public:
    explicit PrometheusLowMemoryImpl(LowMemoryConfig config)
        : config_(std::move(config))
    {
    }

    ~PrometheusLowMemoryImpl() override = default;

    bool loadModel(const std::string& path) override
    {
        if (!config_.fitsInHardware())
        {
            std::cerr << "[Prometheus] WARNING: Config exceeds hardware limits, proceeding anyway\n";
        }

        engine_ = RawrXD::CPUInferenceEngine::GetSharedInstance();
        if (!engine_)
        {
            lastError_ = "Failed to acquire CPUInferenceEngine instance";
            return false;
        }

        // Apply low-memory configuration to the engine
        engine_->SetContextLimit(config_.maxContext);

        // Load the model (RXA extraction happens inside LoadModel if path ends in .rxa)
        if (!engine_->LoadModel(path))
        {
            lastError_ = engine_->GetLastLoadErrorMessage();
            return false;
        }

        modelLoaded_ = true;
        modelPath_ = path;

        // Log memory estimates
        std::cerr << "[Prometheus] Model loaded: " << path << "\n";
        std::cerr << "[Prometheus] Estimated model RAM: " << (config_.estimateModelMemory() / (1024.0 * 1024.0 * 1024.0)) << " GB\n";
        std::cerr << "[Prometheus] Estimated GPU VRAM:  " << (config_.estimateGPUMemory() / (1024.0 * 1024.0 * 1024.0)) << " GB\n";

        return true;
    }

    GenerationResult generate(
        const std::vector<Message>& messages,
        GenerationConfig config
    ) override
    {
        GenerationResult result;
        if (!engine_ || !modelLoaded_)
        {
            result.text = "[ERROR] Model not loaded";
            return result;
        }

        // Build prompt from messages
        std::string prompt = buildPrompt(messages);

        // Tokenize
        std::vector<int32_t> tokens = engine_->Tokenize(prompt);
        if (tokens.empty())
        {
            result.text = "[ERROR] Tokenization failed";
            return result;
        }

        // Limit context
        if (tokens.size() > config_.maxContext)
        {
            tokens.erase(tokens.begin(), tokens.begin() + (tokens.size() - config_.maxContext));
        }

        // Generate
        uint32_t maxTokens = std::min(config.maxTokens, config_.maxContext - static_cast<uint32_t>(tokens.size()));
        std::vector<int32_t> generated;

        for (uint32_t i = 0; i < maxTokens; ++i)
        {
            // Check memory pressure every 32 tokens
            if (i > 0 && (i % 32) == 0)
            {
                checkMemoryPressure();
            }

            std::vector<int32_t> input = tokens;
            input.insert(input.end(), generated.begin(), generated.end());

            // Limit to sliding window for KV cache management
            if (input.size() > config_.slidingWindow)
            {
                size_t excess = input.size() - config_.slidingWindow;
                input.erase(input.begin(), input.begin() + excess);
            }

            std::vector<int32_t> next = engine_->Generate(input, 1);
            if (next.empty())
                break;

            int32_t token = next.back();
            if (token == eosToken_)
                break;

            generated.push_back(token);

            // KV cache eviction
            if (generated.size() > config_.kvCacheBudget)
            {
                pruneKVCache(config_.kvEvictionRatio);
            }
        }

        // Detokenize
        result.text = engine_->Detokenize(generated);
        result.totalTokens = static_cast<uint32_t>(tokens.size() + generated.size());
        result.promptTokens = static_cast<uint32_t>(tokens.size());
        result.completionTokens = static_cast<uint32_t>(generated.size());

        return result;
    }

    void generateStream(
        const std::vector<Message>& messages,
        StreamCallback onToken,
        GenerationConfig config
    ) override
    {
        if (!engine_ || !modelLoaded_)
        {
            Token errTok{};
            errTok.text = "[ERROR] Model not loaded";
            onToken(errTok);
            return;
        }

        std::string prompt = buildPrompt(messages);
        std::vector<int32_t> tokens = engine_->Tokenize(prompt);
        if (tokens.empty())
        {
            Token errTok{};
            errTok.text = "[ERROR] Tokenization failed";
            onToken(errTok);
            return;
        }

        if (tokens.size() > config_.maxContext)
        {
            tokens.erase(tokens.begin(), tokens.begin() + (tokens.size() - config_.maxContext));
        }

        uint32_t maxTokens = std::min(config.maxTokens, config_.maxContext - static_cast<uint32_t>(tokens.size()));
        std::vector<int32_t> generated;

        for (uint32_t i = 0; i < maxTokens; ++i)
        {
            if (i > 0 && (i % 32) == 0)
            {
                checkMemoryPressure();
            }

            std::vector<int32_t> input = tokens;
            input.insert(input.end(), generated.begin(), generated.end());

            if (input.size() > config_.slidingWindow)
            {
                size_t excess = input.size() - config_.slidingWindow;
                input.erase(input.begin(), input.begin() + excess);
            }

            std::vector<int32_t> next = engine_->Generate(input, 1);
            if (next.empty())
                break;

            int32_t token = next.back();
            if (token == eosToken_)
                break;

            generated.push_back(token);

            Token tok{};
            tok.id = static_cast<uint32_t>(token);
            tok.text = engine_->Detokenize({token});
            onToken(tok);

            if (generated.size() > config_.kvCacheBudget)
            {
                pruneKVCache(config_.kvEvictionRatio);
            }
        }
    }

    MemoryStats getMemoryStats() const override
    {
        MemoryStats stats{};
        stats.modelBytesTotal = config_.estimateModelMemory();
        stats.modelBytesLoaded = modelLoaded_ ? stats.modelBytesTotal : 0;
        stats.kvCacheBytes = generatedCount_ * 80 * 12 * 128 * static_cast<uint64_t>(config_.kvQuant) / 8;

#ifdef _WIN32
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus))
        {
            stats.systemRAMPercent = static_cast<float>(memStatus.dwMemoryLoad) / 100.0f;
        }
#endif

        if (engine_)
        {
            stats.modelBytesLoaded = engine_->GetMemoryUsage();
        }

        return stats;
    }

    void pruneKVCache(float ratio) override
    {
        if (ratio <= 0.0f || ratio >= 1.0f)
            return;

        // For the thin wrapper, pruning is implicit via sliding window
        // during generation. This method is a no-op placeholder for
        // future explicit KV cache management.
        (void)ratio;
    }

    void clearKVCache() override
    {
        if (engine_)
        {
            engine_->ClearCache();
        }
        generatedCount_ = 0;
    }

    void setContextWindow(uint32_t newWindow) override
    {
        config_.maxContext = newWindow;
        config_.slidingWindow = std::min(config_.slidingWindow, newWindow);
        if (engine_)
        {
            engine_->SetContextLimit(newWindow);
        }
    }

    void setGPULayers(uint32_t layers) override
    {
        config_.gpuLayers = layers;
    }

    const std::string& lastError() const { return lastError_; }

private:
    LowMemoryConfig config_;
    std::shared_ptr<RawrXD::CPUInferenceEngine> engine_;
    bool modelLoaded_ = false;
    std::string modelPath_;
    std::string lastError_;
    uint32_t generatedCount_ = 0;
    int32_t eosToken_ = 2;  // Common EOS token ID

    std::string buildPrompt(const std::vector<Message>& messages)
    {
        std::string prompt;
        for (const auto& msg : messages)
        {
            if (msg.role == "system")
            {
                prompt += "System: " + msg.content + "\n";
            }
            else if (msg.role == "user")
            {
                prompt += "User: " + msg.content + "\n";
            }
            else if (msg.role == "assistant")
            {
                prompt += "Assistant: " + msg.content + "\n";
            }
        }
        prompt += "Assistant: ";
        return prompt;
    }

    void checkMemoryPressure()
    {
        auto stats = getMemoryStats();
        if (stats.systemRAMPercent > 0.9f)
        {
            std::cerr << "[Prometheus] WARNING: System RAM pressure detected ("
                      << (stats.systemRAMPercent * 100.0f) << "%)\n";
            pruneKVCache(0.3f);
        }
    }
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<PrometheusLowMemory> PrometheusLowMemory::create(
    const std::string& modelPath,
    LowMemoryConfig config
)
{
    auto instance = std::make_unique<PrometheusLowMemoryImpl>(std::move(config));
    if (!instance->loadModel(modelPath))
    {
        std::cerr << "[Prometheus] Failed to load model: " << instance->lastError() << "\n";
        return nullptr;
    }
    return instance;
}

} // namespace Prometheus
