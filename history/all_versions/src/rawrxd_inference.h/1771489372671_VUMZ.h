#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>

// Include model loader first — it brings in comprehensive Vulkan stubs if SDK absent
#include "rawrxd_model_loader.h"
#include "rawrxd_transformer.h"
#include "rawrxd_tokenizer.h"
#include "rawrxd_sampler.h"

// Additional Vulkan stubs needed only by this file's orchestrator
#if !__has_include(<vulkan/vulkan.h>)
#ifndef RAWRXD_VK_INFERENCE_STUBS
#define RAWRXD_VK_INFERENCE_STUBS
#ifndef VK_MAKE_VERSION
#define VK_MAKE_VERSION(major, minor, patch) ((major << 22) | (minor << 12) | patch)
#endif
#ifndef VK_API_VERSION_1_2
#define VK_API_VERSION_1_2 VK_MAKE_VERSION(1, 2, 0)
#endif
#ifndef VK_STRUCTURE_TYPE_APPLICATION_INFO
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#endif
#ifndef VK_QUEUE_COMPUTE_BIT
#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 2
#endif
#ifndef RAWRXD_VK_INSTANCE_TYPEDEF
#define RAWRXD_VK_INSTANCE_TYPEDEF
typedef void* VkInstance;
typedef uint32_t VkStructureType;
#endif
struct VkApplicationInfo { VkStructureType sType; const void* pNext{}; const char* pApplicationName; uint32_t applicationVersion; const char* pEngineName; uint32_t engineVersion; uint32_t apiVersion; };
struct VkInstanceCreateInfo { VkStructureType sType; const void* pNext{}; uint32_t flags{}; const VkApplicationInfo* pApplicationInfo; uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames{}; uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames{}; };
struct VkPhysicalDeviceProperties { uint32_t apiVersion; uint32_t driverVersion; uint32_t vendorID; uint32_t deviceID; uint32_t deviceType; char deviceName[256]; uint8_t pipelineCacheUUID[16]; };
struct VkQueueFamilyProperties { uint32_t queueFlags; uint32_t queueCount; uint32_t timestampValidBits; uint32_t minImageTransferGranularity[3]; };
struct VkDeviceQueueCreateInfo { VkStructureType sType; const void* pNext{}; uint32_t flags{}; uint32_t queueFamilyIndex; uint32_t queueCount; const float* pQueuePriorities; };
struct VkPhysicalDeviceFeatures { uint32_t features[55]{}; };
struct VkDeviceCreateInfo { VkStructureType sType; const void* pNext{}; uint32_t flags{}; uint32_t queueCreateInfoCount; const VkDeviceQueueCreateInfo* pQueueCreateInfos; uint32_t enabledLayerCount{}; const char* const* ppEnabledLayerNames{}; uint32_t enabledExtensionCount{}; const char* const* ppEnabledExtensionNames{}; const VkPhysicalDeviceFeatures* pEnabledFeatures; };
inline VkResult vkCreateInstance(const VkInstanceCreateInfo*, const void*, VkInstance* i) { *i = VK_NULL_HANDLE; return 1; }
inline void vkEnumeratePhysicalDevices(VkInstance, uint32_t* c, VkPhysicalDevice*) { *c = 0; }
inline void vkGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties*) {}
inline void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t* c, VkQueueFamilyProperties*) { *c = 0; }
inline VkResult vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice* d) { *d = VK_NULL_HANDLE; return 1; }
#endif
#endif

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
        
        RawrXDTransformer::Config cfg;
        cfg.dim = loader.getDim();   
        cfg.n_layers = loader.getLayers();
        cfg.n_heads = loader.getHeads();
        cfg.n_kv_heads = loader.getKVHeads();
        cfg.vocab_size = loader.getVocabSize();
        
        if (cfg.vocab_size == 0) cfg.vocab_size = 32000;
        if (cfg.dim == 0) cfg.dim = 4096;
        if (cfg.n_layers == 0) cfg.n_layers = 32;
        
        cfg.seq_len = 4096;
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

    std::string Generate(const std::string& prompt, uint32_t maxTokens = 512, 
                        std::function<void(const std::string&)> callback = nullptr) {
        auto tokens = tokenizer.Encode(prompt);
        // printf("[RawrXD] Prompt: %zu tokens\n", tokens.size());
        
        auto logits = transformer.Forward(tokens, 0);
        
        std::string fullResponse;

        for (uint32_t i = 0; i < maxTokens; i++) {
            uint32_t nextToken = sampler.Sample(logits.data(), 
                                               logits.size(), tokens);
            tokens.push_back(nextToken);
            
            if (nextToken == 2) break; 
            
            std::vector<uint32_t> nextTokVec = {nextToken};
            logits = transformer.Forward(nextTokVec, tokens.size() - 1);
            
            std::string piece = tokenizer.Decode({nextToken});
            if (callback) {
                callback(piece);
            } else {
                // printf("%s", piece.c_str());
                // fflush(stdout);
            }
            fullResponse += piece;
        }
        
        // printf("\n");
        return fullResponse;
    }
};
