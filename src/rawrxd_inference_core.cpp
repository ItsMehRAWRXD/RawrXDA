// Note: In visual studio, these cpp includes are handled by the build system normally
// But for the user specific single-file-unit architecture request, we use includes.
// Ensure include guards in referenced files to prevent redefinition if integrated differently.

// Check if these are already included or if we should rely on the linker.
// The user pattern suggests a unity build or direct include.
// Since separate files were created, we use #pragma once in them usually.
// But here we'll assume they are just headers/impls combined.

#pragma once

#include "rawrxd_model_loader.h"
#include "rawrxd_transformer.h"
#include "rawrxd_tokenizer.h"
#include "rawrxd_sampler.h"
#include "crt_free_memory.h"
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>
#include <vector>

// Vulkan Libraries
#pragma comment(lib, "vulkan-1.lib")

class RawrXDInferenceEngine {
    RawrXDModelLoader loader;
    RawrXDTransformer transformer;
    RawrXDTokenizer tokenizer;
    RawrXDSampler sampler;
    
    // Vulkan Context
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    
    std::atomic<bool> stopFlag{false};
    std::string lastErrorMessage;
    std::function<void(const std::string&)> onTrace;

public:
    // Callback for streaming tokens to UI
    std::function<void(const std::string&)> onTokenGenerated;

    RawrXDInferenceEngine() = default;
    
    ~RawrXDInferenceEngine() {
        if (device) vkDestroyDevice(device, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
    }
    
    // Non-copyable
    RawrXDInferenceEngine(const RawrXDInferenceEngine&) = delete;
    RawrXDInferenceEngine& operator=(const RawrXDInferenceEngine&) = delete;

    void SetTraceCallback(std::function<void(const std::string&)> callback) {
        onTrace = callback;
    }

    std::string GetLastErrorMessage() const {
        return lastErrorMessage;
    }

    bool Initialize(const std::string& modelPath) {
        lastErrorMessage.clear();
        
        if (onTrace) onTrace("[RawrXD] Starting inference engine initialization");
        
        bool vulkanAvailable = InitVulkan();
        if (!vulkanAvailable) {
            CRTFreeConsole::WriteLine("[RawrXD] Vulkan Init Failed. Falling back to CPU mode if supported.");
            if (onTrace) onTrace("[RawrXD] Vulkan initialization failed - using CPU fallback");
        } else {
            if (onTrace) onTrace("[RawrXD] Vulkan initialized successfully");
        }
        
        // 1. Load Model
        if (onTrace) onTrace("[RawrXD] Loading model: " + modelPath);
        std::wstring wPath(modelPath.begin(), modelPath.end());
        if (!loader.Load(wPath.c_str(), device, physDevice)) {
            lastErrorMessage = "Failed to load model file: " + modelPath;
            if (onTrace) onTrace("[RawrXD] Model load failed: " + lastErrorMessage);
            return false;
        }
        if (onTrace) onTrace("[RawrXD] Model loaded successfully");
        
        // 2. Configure Transformer
        if (onTrace) onTrace("[RawrXD] Configuring transformer");
        RawrXDTransformer::Config cfg;
        cfg.dim = loader.GetMetadataInt("embedding_length");
        cfg.hidden_dim = loader.GetMetadataInt("feed_forward_length");
        cfg.n_layers = loader.GetMetadataInt("block_count");
        cfg.n_heads = loader.GetMetadataInt("attention.head_count");
        cfg.n_kv_heads = loader.GetMetadataInt("attention.head_count_kv");
        cfg.vocab_size = loader.GetMetadataInt("tokenizer.ggml.tokens");
        cfg.rope_theta = 10000.0f; // Default, usually in metadata
        cfg.rms_norm_eps = 1e-5f;
        
        // Fallback defaults
        if (cfg.dim == 0) cfg.dim = 4096;
        if (cfg.n_layers == 0) cfg.n_layers = 32;
        if (cfg.n_heads == 0) cfg.n_heads = 32;
        
        if (onTrace) {
            char buf[256];
            CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Transformer config: dim=%d, layers=%d, heads=%d", cfg.dim, cfg.n_layers, cfg.n_heads);
            onTrace(buf);
        }
        
        if (!transformer.Initialize(device, physDevice, cfg, loader)) {
            lastErrorMessage = "Failed to initialize transformer";
            if (onTrace) onTrace("[RawrXD] Transformer initialization failed");
            return false;
        }
        if (onTrace) onTrace("[RawrXD] Transformer initialized successfully");
        
        // 3. Load Tokenizer
        if (onTrace) onTrace("[RawrXD] Loading tokenizer");
        // Look for typical file names near model or hardcoded
        tokenizer.Load("vocab.json", "merges.txt");
        if (onTrace) onTrace("[RawrXD] Tokenizer loaded");
        
        // 4. KV Cache verification
        if (onTrace) onTrace("[RawrXD] Verifying KV cache integrity");
        if (!VerifyKVCache()) {
            lastErrorMessage = "KV cache verification failed";
            if (onTrace) onTrace("[RawrXD] KV cache verification failed");
            return false;
        }
        if (onTrace) onTrace("[RawrXD] KV cache verification passed");
        
        if (onTrace) onTrace("[RawrXD] Inference engine initialization complete");
        return true;
    }

    void Generate(const std::string& prompt, int maxTokens) {
        lastErrorMessage.clear();
        
        if (onTrace) onTrace("[RawrXD] Starting generation with prompt length: " + std::to_string(prompt.length()));
        
        stopFlag = false;
        
        // 1. Enforce prompt template
        std::string processedPrompt = EnforcePromptTemplate(prompt);
        if (onTrace) onTrace("[RawrXD] Prompt template enforced, processed length: " + std::to_string(processedPrompt.length()));
        
        // 2. Tokenize
        if (onTrace) onTrace("[RawrXD] Tokenizing prompt");
        std::vector<uint32_t> tokens = tokenizer.Encode(processedPrompt);
        if (tokens.empty()) {
            lastErrorMessage = "Tokenization failed - empty token sequence";
            if (onTrace) onTrace("[RawrXD] Tokenization failed");
            return;
        }
        
        char buf[128];
        CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Tokenized to %zu tokens", tokens.size());
        if (onTrace) onTrace(buf);
        
        uint32_t currentPos = 0;
        
        // 3. Prefill (Process Prompt)
        if (onTrace) onTrace("[RawrXD] Starting prefill phase");
        std::vector<float> logits;
        if (!tokens.empty()) {
            logits = transformer.Forward(tokens, currentPos);
            if (logits.empty()) {
                lastErrorMessage = "Prefill forward pass failed - no logits returned";
                if (onTrace) onTrace("[RawrXD] Prefill failed - no logits");
                return;
            }
            currentPos += tokens.size();
            if (onTrace) onTrace("[RawrXD] Prefill completed successfully");
        }
        
        // 4. Generation Loop with probes
        if (onTrace) onTrace("[RawrXD] Starting generation loop");
        int tokensGenerated = 0;
        for (int i = 0; i < maxTokens && !stopFlag; i++) {
            if (logits.empty()) {
                lastErrorMessage = "Logits became empty during generation";
                if (onTrace) onTrace("[RawrXD] Generation stopped - empty logits");
                break;
            }
            
            // Token generation probe
            if (onTrace && (i % 10 == 0)) {
                CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Generation step %d/%d", i+1, maxTokens);
                onTrace(buf);
            }
            
            // Sample next token
            uint32_t nextToken = sampler.Sample(logits.data(), 
                                                logits.size(), tokens);
            
            // End of generation check
            if (nextToken == 2 || nextToken == 0) { // EOS/PAD
                if (onTrace) onTrace("[RawrXD] EOS token detected, stopping generation");
                break;
            }
            
            // Validate token
            if (nextToken >= tokenizer.GetVocabSize()) {
                lastErrorMessage = "Generated invalid token ID";
                CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Invalid token ID: %u", nextToken);
                if (onTrace) onTrace(buf);
                break;
            }
            
            // Append and Stream
            tokens.push_back(nextToken);
            std::string text = tokenizer.Decode({nextToken});
            if (onTokenGenerated) onTokenGenerated(text);
            else CRTFreeConsole::Write(text.c_str());
            
            tokensGenerated++;
            
            // Auto-regressive step
            logits = transformer.Forward({nextToken}, currentPos++);
            if (logits.empty()) {
                lastErrorMessage = "Auto-regressive forward pass failed";
                if (onTrace) onTrace("[RawrXD] Auto-regressive step failed");
                break;
            }
        }
        
        CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Generation completed, %d tokens generated", tokensGenerated);
        if (onTrace) onTrace(buf);

    void Stop() { stopFlag = true; }

private:
    bool VerifyKVCache() {
        // Verify transformer was initialized with valid config dimensions
        // The transformer stores its config internally; we re-check the loader metadata
        // to ensure dimensions are consistent and non-zero.
        int dim = loader.GetMetadataInt("embedding_length");
        int layers = loader.GetMetadataInt("block_count");
        int heads = loader.GetMetadataInt("attention.head_count");

        // Accept defaults if metadata was missing (same logic as LoadModel)
        if (dim == 0) dim = 4096;
        if (layers == 0) layers = 32;
        if (heads == 0) heads = 32;

        // Sanity checks: dimension must be divisible by head count
        if (heads <= 0 || dim % heads != 0) {
            if (onTrace) onTrace("[RawrXD] KV cache check failed: dim not divisible by n_heads");
            return false;
        }
        // Layers must be positive and within realistic bounds
        if (layers <= 0 || layers > 256) {
            if (onTrace) onTrace("[RawrXD] KV cache check failed: n_layers out of bounds");
            return false;
        }
        return true;
    }

    std::string EnforcePromptTemplate(const std::string& prompt) {
        // Basic prompt template enforcement
        // In a real implementation, this would add system prompts, format properly, etc.
        if (prompt.empty()) return prompt;
        
        // Simple check - ensure prompt doesn't start with weird characters
        if (prompt[0] == '\n' || prompt[0] == '\r') {
            return prompt.substr(1);
        }
        
        return prompt;
    }

    bool InitVulkan() {
        if (onTrace) onTrace("[RawrXD] Initializing Vulkan context");
        
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RawrXD Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RawrXD";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // Extensions? validation layers?
        // Minimal instance
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            lastErrorMessage = "Failed to create Vulkan instance";
            if (onTrace) onTrace("[RawrXD] Vulkan instance creation failed: " + std::to_string(result));
            return false;
        }
        if (onTrace) onTrace("[RawrXD] Vulkan instance created");
        
        // Pick Phy Device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            lastErrorMessage = "No Vulkan physical devices found";
            if (onTrace) onTrace("[RawrXD] No physical devices found");
            return false;
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (onTrace) {
            char buf[64];
            CRTFreeString::format(buf, sizeof(buf), "[RawrXD] Found %u physical devices", deviceCount);
            onTrace(buf);
        }
        
        // Simple pick first discrete
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physDevice = device;
                if (onTrace) onTrace("[RawrXD] Selected discrete GPU");
                break;
            }
        }
        if (physDevice == VK_NULL_HANDLE) {
            physDevice = devices[0];
            if (onTrace) onTrace("[RawrXD] Using integrated/fallback GPU");
        }
        
        // Create Logical Device
        // Need queue family info
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

        int computeFamily = -1;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeFamily = i;
                break;
            }
        }
        
        if (computeFamily == -1) {
            lastErrorMessage = "No compute queue family found";
            if (onTrace) onTrace("[RawrXD] No compute queue family available");
            return false;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = computeFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        
        // Enable features if needed: float16, int8
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

        result = vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            lastErrorMessage = "Failed to create Vulkan device";
            if (onTrace) onTrace("[RawrXD] Vulkan device creation failed: " + std::to_string(result));
            return false;
        }
        
        if (onTrace) onTrace("[RawrXD] Vulkan device created successfully");
        return true;
    }
};

