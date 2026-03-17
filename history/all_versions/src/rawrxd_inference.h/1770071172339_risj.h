#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
// #include <vulkan/vulkan.h>
#include "rawrxd_model_loader.h"
#include "rawrxd_transformer.h"
#include "rawrxd_tokenizer.h"
#include "rawrxd_sampler.h"

// RawrXD Real Inference Orchestrator
// One-shot: Load model -> Tokenize -> Forward -> Sample -> Detokenize

class RawrXDInference {
    RawrXDModelLoader loader;
    RawrXDTransformer transformer;
    RawrXDTokenizer tokenizer;
    RawrXDSampler sampler;
    bool m_initialized = false;
    
    // Helpers
    VkInstance CreateVulkanInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RawrXD Inference";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RawrXD Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2; 

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;
        createInfo.enabledExtensionCount = 0;
        
        VkInstance instance = VK_NULL_HANDLE;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            printf("Failed to create Vulkan instance\n");
            return VK_NULL_HANDLE;
        }
        return instance;
    }
    
    VkPhysicalDevice SelectPhysicalDevice(VkInstance instance) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) return VK_NULL_HANDLE;
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                printf("Selected GPU: %s\n", deviceProperties.deviceName);
                return device;
            }
        }
        return devices[0];
    }
    
    VkDevice CreateLogicalDevice(VkPhysicalDevice physDevice) {
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
        
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = computeFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        VkPhysicalDeviceFeatures deviceFeatures{}; 
        createInfo.pEnabledFeatures = &deviceFeatures;

        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            printf("failed to create logical device!\n");
            return VK_NULL_HANDLE;
        }
        return device;
    }

public:
    bool Initialize(const wchar_t* modelPath, 
                   const char* vocabPath,
                   const char* mergesPath) {
        VkInstance instance = CreateVulkanInstance();
        if(!instance) return false;
        
        VkPhysicalDevice physDevice = SelectPhysicalDevice(instance);
        if(!physDevice) return false;
        
        VkDevice device = CreateLogicalDevice(physDevice);
        if(!device) return false;
        
        if (!loader.Load(modelPath, device, physDevice)) {
            printf("[RawrXD] Failed to load model\n");
            return false;
        }
        
        RawrXDTransformer::Config cfg{}; // Zero initialize
        cfg.dim = loader.getDim();   
        cfg.n_layers = loader.getLayers();
        cfg.n_heads = loader.getHeads();
        cfg.n_kv_heads = loader.getKVHeads();
        cfg.vocab_size = loader.getVocabSize();
        
        if (cfg.vocab_size == 0) cfg.vocab_size = 32000;
        if (cfg.dim == 0) cfg.dim = 4096;
        if (cfg.n_layers == 0) cfg.n_layers = 32;
        
        cfg.seq_len = 4096;
        cfg.n_ctx = 4096; // Explicitly set context length
        cfg.rope_theta = 10000.0f;
        cfg.rms_norm_eps = 1e-5f;

        transformer.Initialize(device, physDevice, cfg, &loader);
        tokenizer.Load(vocabPath);
        
        printf("[RawrXD] Inference engine READY\n");
        m_initialized = true;
        return true;
    }
    
    bool IsInitialized() const { return m_initialized; }

    std::vector<uint32_t> Tokenize(const std::string& text) {
        return tokenizer.Encode(text);
    }

    std::string Detokenize(const std::vector<uint32_t>& tokens) {
        return tokenizer.Decode(tokens);
    }

    struct GenStats {
        std::string text;
        int token_count;
    };

    GenStats Generate(const std::string& prompt, uint32_t maxTokens = 512, 
                        std::function<void(const std::string&)> callback = nullptr) {
        auto tokens = tokenizer.Encode(prompt);
        // printf("[RawrXD] Prompt: %zu tokens\n", tokens.size());
        
        auto logits = transformer.Forward(tokens, 0);
        
        std::string fullResponse;
        int count = 0;

        for ( uint32_t i = 0; i < maxTokens; i++) {
            uint32_t nextToken = sampler.Sample(logits.data(), 
                                               logits.size(), tokens);
            
            if (nextToken == 2) break; 
            
            tokens.push_back(nextToken); // History update
            count++;
            
            std::vector<uint32_t> nextTokVec = {nextToken};
            logits = transformer.Forward(nextTokVec, tokens.size() - 1);
            
            std::string piece = tokenizer.Decode({nextToken});
            if (callback) {
                callback(piece);
            }
            fullResponse += piece;
        }
        
        return {fullResponse, count};
    }
};
