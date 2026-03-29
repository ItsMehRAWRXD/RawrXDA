#pragma once
#include <vector>
#include <cstdint>
#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Standard Win32/CPU build - Vulkan handles not needed
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif
#endif
#include "rawrxd_model_loader.h"

class RawrXDTransformer {
public:
    struct Config {
        int dim;
        int hidden_dim;
        int n_layers;
        int n_heads;
        int n_kv_heads;
        int vocab_size;
        int n_ctx;      // Context length
        int seq_len;
        float rope_theta;
        float rms_norm_eps;
    };

    void Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader* loader);
    std::vector<float> Forward(const std::vector<uint32_t>& tokens, int start_pos);

private:
    Config config;
    VkDevice device;
    RawrXDModelLoader* loader;
    
    // KV Cache
    std::vector<float> kv_cache_k;
    std::vector<float> kv_cache_v;
    std::vector<int64_t> kv_cache_pos;
};

