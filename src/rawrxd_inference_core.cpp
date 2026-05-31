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

    bool Initialize(const std::string& modelPath) {
        if (!InitVulkan()) {
            std::cerr << "[RawrXD] Vulkan Init Failed. Falling back to CPU mode if supported." << std::endl;
            // continue, letting loader/transformer handle VK_NULL_HANDLE
        }
        
        // 1. Load Model
        std::wstring wPath(modelPath.begin(), modelPath.end());
        if (!loader.Load(wPath.c_str(), device, physDevice)) return false;
        
        // 2. Configure Transformer
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
        
        transformer.Initialize(device, physDevice, cfg, loader);
        
        // 3. Load Tokenizer
        // Look for typical file names near model or hardcoded
        tokenizer.Load("vocab.json", "merges.txt");
        
        return true;
    }

    void Generate(const std::string& prompt, int maxTokens) {
        stopFlag = false;
        
        // 1. Tokenize
        std::vector<uint32_t> tokens = tokenizer.Encode(prompt);
        // printf("Prompt tokens: %zu\n", tokens.size());
        
        uint32_t currentPos = 0;
        
        // 2. Prefill (Process Prompt)
        // Feed tokens in chunks if prompt is long (batching) to avoid OOM
        // Here we feed all at once for simplicity, assuming fits in context
        
        std::vector<float> logits;
        if (!tokens.empty()) {
             logits = transformer.Forward(tokens, currentPos);
             currentPos += tokens.size();
        }
        
        // 3. Generation Loop
        for (int i = 0; i < maxTokens && !stopFlag; i++) {
            if (logits.empty()) break;
            
            // Sample next token
            uint32_t nextToken = sampler.Sample(logits.data(), 
                                                logits.size(), tokens);
            
            // End of generation check
            if (nextToken == 2 || nextToken == 0) break; // EOS/PAD
            
            // Append and Stream
            tokens.push_back(nextToken);
            std::string text = tokenizer.Decode({nextToken});
            if (onTokenGenerated) onTokenGenerated(text);
            else std::cout << text << std::flush;
            
            // Auto-regressive step
            logits = transformer.Forward({nextToken}, currentPos++);
        }
    }

    void Stop() { stopFlag = true; }

private:
    bool InitVulkan() {
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
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            return false;
        }
        
        // Pick Phy Device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) return false;
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        // Simple pick first discrete
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physDevice = device;
                break;
            }
        }
        if (physDevice == VK_NULL_HANDLE) physDevice = devices[0];
        
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
        
        if (computeFamily == -1) return false;

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

        if (vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            return false;
        }

        return true;
    }
};

