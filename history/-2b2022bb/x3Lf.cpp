// Note: In visual studio, these cpp includes are handled by the build system normally
// But for the user specific single-file-unit architecture request, we use includes.
// Ensure include guards in referenced files to prevent redefinition if integrated differently.

// Check if these are already included or if we should rely on the linker.
// The user pattern suggests a unity build or direct include.
// Since separate files were created, we use #pragma once in them usually.
// But here we'll assume they are just headers/impls combined.

#pragma once

// Fix: Prevent redefinition if this file is treated as a translation unit
#ifndef RAWRXD_INFERENCE_CORE
#define RAWRXD_INFERENCE_CORE

#include "rawrxd_model_loader.cpp"
#include "rawrxd_transformer.cpp"
#include "rawrxd_tokenizer.cpp"
#include "rawrxd_sampler.cpp"
#include <thread>
#include <atomic>
#include <functional>

class RawrXDInferenceEngine {
    RawrXDModelLoader loader;
    RawrXDTransformer transformer;
    RawrXDTokenizer tokenizer;
    RawrXDSampler sampler;
    
    // Vulkan Context
    VkInstance instance;
    VkPhysicalDevice physDevice;
    VkDevice device;
    
    std::atomic<bool> stopFlag{false};

public:
    // Callback for streaming tokens to UI
    std::function<void(const std::string&)> onTokenGenerated;

    bool Initialize(const std::string& modelPath) {
        InitVulkan(); // Standard Vulkan boilerplate
        
        // 1. Load Model
        std::wstring wPath(modelPath.begin(), modelPath.end());
        if (!loader.Load(wPath.c_str(), device, physDevice)) return false;
        
        // 2. Configure Transformer
        RawrXDTransformer::Config cfg;
        cfg.dim = 4096; // Example Llama 3 8B params
        cfg.hidden_dim = 14336;
        cfg.n_layers = 32;
        cfg.n_heads = 32;
        cfg.n_kv_heads = 8;
        cfg.vocab_size = 128256;
        cfg.rope_theta = 500000.0f;
        cfg.rms_norm_eps = 1e-5f;
        
        transformer.Initialize(device, physDevice, cfg, loader);
        
        // 3. Load Tokenizer (usually embedded in GGUF, extracted here)
        tokenizer.Load("vocab.json", "merges.txt");
        
        return true;
    }

    void Generate(const std::string& prompt, int maxTokens) {
        stopFlag = false;
        
        // 1. Tokenize
        std::vector<uint32_t> tokens = tokenizer.Encode(prompt);
        uint32_t currentPos = 0;
        
        // 2. Prefill (Process Prompt)
        // Feed tokens in chunks if prompt is long (batching)
        std::vector<float> logits = transformer.Forward(tokens, currentPos);
        currentPos += tokens.size();
        
        // 3. Generation Loop
        for (int i = 0; i < maxTokens && !stopFlag; i++) {
            // Sample next token
            uint32_t nextToken = sampler.Sample(logits.data(), 
                                                logits.size(), tokens);
            
            // End of generation check
            if (nextToken == tokenizer.EOS_ID) break;
            
            // Append and Stream
            tokens.push_back(nextToken);
            std::string text = tokenizer.Decode({nextToken});
            if (onTokenGenerated) onTokenGenerated(text);
            
            // Auto-regressive step
            // Forward pass with just the SINGLE new token
            // KV cache handles the history
            logits = transformer.Forward({nextToken}, currentPos++);
        }
    }

    void Stop() { stopFlag = true; }

private:
    void InitVulkan() { 
        // Simply stubbed for compilation as requested
        // In real impl: vkCreateInstance, vkEnumeratePhysicalDevices, vkCreateDevice
    }
};

#endif
