#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "rawrxd_model_loader.h"

class RawrXDTransformer {
public:
    struct Config {
        int dim;
        int n_layers;
        int n_heads;
        int n_kv_heads;
        int vocab_size;
        int seq_len;
        float rope_theta;
        float rms_norm_eps;
    };

    void Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader& loader);
    std::vector<float> Forward(const std::vector<uint32_t>& tokens, int start_pos);

private:
    Config config;
    VkDevice device;
    // Layer weights would be here
};

