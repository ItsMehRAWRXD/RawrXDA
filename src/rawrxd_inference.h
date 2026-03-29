#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <cmath>
#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Vulkan stubs for CPU mode
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;
typedef struct { int dummy; } VkApplicationInfo;
typedef struct { int dummy; } VkInstanceCreateInfo;
typedef struct { int dummy; } VkDeviceQueueCreateInfo;
typedef struct { int dummy; } VkDeviceCreateInfo;
typedef struct { int dummy; } VkPhysicalDeviceProperties;
typedef struct { uint32_t queueFlags; } VkQueueFamilyProperties;
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 0
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 0
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 0
#define VK_MAKE_VERSION(a,b,c) 0
#define VK_API_VERSION_1_2 0
#define VK_SUCCESS 0
#define VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 0
#define VK_QUEUE_COMPUTE_BIT 0
#endif
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
    uint32_t m_contextLimit = 0;
    std::vector<float> m_lastLogits;
    std::string m_lastLoadErrorMessage;
    
    // Helpers
    VkInstance CreateVulkanInstance() {
#ifndef RAWR_ENABLE_VULKAN
        return VK_NULL_HANDLE;
#else
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
#endif
    }
    
    VkPhysicalDevice SelectPhysicalDevice(VkInstance instance) {
#ifndef RAWR_ENABLE_VULKAN
        (void)instance;
        return VK_NULL_HANDLE;
#else
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
#endif
    }
    
    VkDevice CreateLogicalDevice(VkPhysicalDevice physDevice) {
#ifndef RAWR_ENABLE_VULKAN
        (void)physDevice;
        return VK_NULL_HANDLE;
#else
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
#endif
    }

public:
    const std::string& GetLastLoadErrorMessage() const { return m_lastLoadErrorMessage; }

    bool Initialize(const wchar_t* modelPath, 
                   const char* vocabPath,
                   const char* mergesPath) {
        m_lastLoadErrorMessage.clear();
        loader.SetLoadErrorCallback(
            [this](const std::string& stage, const std::string& message) {
                m_lastLoadErrorMessage = stage + ": " + message;
            });
#ifdef RAWR_ENABLE_VULKAN
        VkInstance instance = CreateVulkanInstance();
        if(!instance) return false;
        
        VkPhysicalDevice physDevice = SelectPhysicalDevice(instance);
        if(!physDevice) return false;
        
        VkDevice device = CreateLogicalDevice(physDevice);
        if(!device) return false;
#else
        // CPU-only mode — no GPU required
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physDevice = VK_NULL_HANDLE;
        printf("[RawrXD] CPU-only mode (Vulkan disabled)\n");
#endif
        
        if (!loader.Load(modelPath, device, physDevice)) {
            if (m_lastLoadErrorMessage.empty()) {
                m_lastLoadErrorMessage = loader.GetLastLoadErrorMessage();
            }
            printf("[RawrXD] Failed to load model\n");
            return false;
        }
        
        RawrXDTransformer::Config cfg{}; // Zero-init all fields
        cfg.dim = loader.getDim();   
        cfg.n_layers = loader.getLayers();
        cfg.n_heads = loader.getHeads();
        cfg.n_kv_heads = loader.getKVHeads();
        cfg.vocab_size = loader.getVocabSize();
        
        if (cfg.vocab_size == 0) cfg.vocab_size = 32000;
        if (cfg.dim == 0) cfg.dim = 4096;
        if (cfg.n_layers == 0) cfg.n_layers = 32;
        if (cfg.n_heads == 0) cfg.n_heads = 32;
        if (cfg.n_kv_heads == 0) cfg.n_kv_heads = cfg.n_heads;
        
        cfg.hidden_dim = (loader.getFFNDim() > 0) ? loader.getFFNDim() : cfg.dim * 4;
        cfg.n_ctx = 2048;  // Conservative context for CPU-only mode
        cfg.seq_len = 2048;
        cfg.rope_theta = 10000.0f;
        cfg.rms_norm_eps = 1e-5f;

        printf("[RawrXD] Config: dim=%d layers=%d heads=%d kv_heads=%d vocab=%d hidden=%d ctx=%d\n",
               cfg.dim, cfg.n_layers, cfg.n_heads, cfg.n_kv_heads, cfg.vocab_size, cfg.hidden_dim, cfg.n_ctx);
        transformer.Initialize(device, physDevice, cfg, &loader);
        tokenizer.Load(vocabPath);
        m_contextLimit = static_cast<uint32_t>(cfg.n_ctx);
        m_lastLogits.clear();
        
        printf("[RawrXD] Inference engine READY\n");
        m_initialized = true;
        return true;
    }
    
    bool IsInitialized() const { return m_initialized; }
    
    // Expose loader metadata to facade
    int getVocabSize() const { return loader.getVocabSize(); }
    int getDim() const { return loader.getDim(); }
    int getLayers() const { return loader.getLayers(); }
    int getHeads() const { return loader.getHeads(); }
    int getKVHeads() const { return loader.getKVHeads(); }
    uint32_t getContextLimit() const { return m_contextLimit; }

    std::vector<uint32_t> Tokenize(const std::string& text) {
        if (!m_initialized) return {};
        return tokenizer.Encode(text);
    }

    std::string Detokenize(const std::vector<uint32_t>& tokens) {
        if (!m_initialized) return {};
        return tokenizer.Decode(tokens);
    }

    std::vector<float> ForwardTokens(const std::vector<uint32_t>& tokens, uint32_t startPos = 0) {
        if (!m_initialized || tokens.empty()) {
            return {};
        }
        m_lastLogits = transformer.Forward(tokens, startPos);
        return m_lastLogits;
    }

    const std::vector<float>& LastLogits() const { return m_lastLogits; }

    std::vector<uint32_t> GenerateFromTokens(
        const std::vector<uint32_t>& promptTokens,
        uint32_t maxTokens = 512,
        std::function<void(uint32_t, const std::string&)> callback = nullptr)
    {
        std::vector<uint32_t> generated;
        if (!m_initialized || maxTokens == 0 || promptTokens.empty()) {
            return generated;
        }
        maxTokens = std::min<uint32_t>(maxTokens, 8192);

        std::vector<uint32_t> tokens = promptTokens;
        const uint32_t vocabSize = std::max(1, loader.getVocabSize());

        // Keep right-most context when prompt exceeds model context.
        if (m_contextLimit > 0 && tokens.size() > m_contextLimit) {
            tokens.erase(tokens.begin(), tokens.end() - m_contextLimit);
        }
        for (auto& t : tokens) {
            if (t >= vocabSize) t %= vocabSize;
        }

        auto logits = transformer.Forward(tokens, 0);
        m_lastLogits = logits;
        uint32_t absolutePos = static_cast<uint32_t>(tokens.size());

        for (uint32_t i = 0; i < maxTokens; i++) {
            if (logits.empty()) break;

            bool hasFinite = false;
            for (float v : logits) {
                if (std::isfinite(v)) {
                    hasFinite = true;
                    break;
                }
            }
            if (!hasFinite) break;

            uint32_t nextToken = sampler.Sample(logits.data(), logits.size(), tokens);
            if (nextToken >= vocabSize) {
                nextToken %= vocabSize;
            }
            tokens.push_back(nextToken);
            if (m_contextLimit > 0 && tokens.size() > m_contextLimit) {
                tokens.erase(tokens.begin(), tokens.end() - m_contextLimit);
            }
            generated.push_back(nextToken);

            if (nextToken == 2) break;

            std::vector<uint32_t> nextTokVec = {nextToken};
            logits = transformer.Forward(nextTokVec, absolutePos);
            absolutePos++;
            m_lastLogits = logits;

            if (callback) {
                const std::string piece = tokenizer.Decode({nextToken});
                try {
                    callback(nextToken, piece);
                } catch (...) {
                    // Callback failures should not crash inference.
                }
            }
        }

        return generated;
    }

    std::string Generate(const std::string& prompt, uint32_t maxTokens = 512, 
                        std::function<void(const std::string&)> callback = nullptr) {
        if (!m_initialized || maxTokens == 0) {
            return {};
        }
        maxTokens = std::min<uint32_t>(maxTokens, 8192);

        const auto tokens = tokenizer.Encode(prompt);
        if (tokens.empty()) {
            return {};
        }

        std::string fullResponse;
        auto generated = GenerateFromTokens(tokens, maxTokens,
            [&](uint32_t, const std::string& piece) {
                if (callback) callback(piece);
                fullResponse += piece;
            });
        (void)generated;
        return fullResponse;
    }
};
