#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "rawrxd_model_loader.h"

// MASM64 kernels
extern "C" {
    void MatMul_F16F32_AVX512(uint16_t* A, float* B, float* C, 
                              size_t M, size_t N, size_t K);
    void RMSNorm_AVX512(float* x, float* out, size_t N, float eps);
    void SoftMax_AVX512(float* x, size_t N);
    void RoPE_Rotate_AVX512(float* q, float* k, size_t dims, 
                           size_t pos, float theta);
}

class RawrXDTransformer {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    
    // We assume pipelines are created and stored
    
public:
    struct Config {
        uint32_t dim;
        uint32_t hidden_dim;
        uint32_t n_layers;
        uint32_t n_heads;
        uint32_t n_kv_heads;
        uint32_t vocab_size;
        uint32_t seq_len;
        float rope_theta;
        float rms_norm_eps;
    } config;
    
    // KV Cache
    struct KVCache {
        std::vector<VkBuffer> kBuffers;
        std::vector<VkBuffer> vBuffers;
        size_t cacheLen = 0;
        size_t maxSeqLen = 8192;
    } kvCache;

    // Reference to loaded model tensors
    RawrXDModelLoader* loaderRef = nullptr;

    bool Initialize(VkDevice dev, VkPhysicalDevice phys, 
                    const Config& cfg, RawrXDModelLoader& loader);

    std::vector<float> Forward(const std::vector<uint32_t>& tokens, 
                               uint32_t startPos);

private:
    void Attention(uint16_t* x, uint32_t layer, uint32_t startPos);
    void FFN(uint16_t* x, uint32_t layer);
    void RMSNorm(uint16_t* x, VkBuffer weight);
    void RMSNorm(uint16_t* x, uint16_t* scale); // Overload for CPU fallback logic if needed
    
    // CPU fallback pointer helpers
    uint16_t* MatMul(uint16_t* x, VkBuffer weight);
    void SwiGLU_Elementwise(uint16_t* h1, uint16_t* h2, size_t size);
    
    void UpdateKVCache(uint16_t* k, uint16_t* v, uint32_t layer, uint32_t pos, size_t seqLen);
    uint16_t* GetKCache(uint32_t layer, uint32_t head, uint32_t pos);
    float* GetScratchBuffer(size_t size);
    float DotProduct(uint16_t* a, uint16_t* b, size_t n);
    void AccumulateWeightedSum(uint16_t* v, float* scores, uint32_t layer, uint32_t kvHead, size_t count);
    void FinalLogitsToCPU(uint16_t* x, float* logits);
    uint16_t* TokenEmbeddings(const std::vector<uint32_t>& tokens);
    
    VkBuffer GetWeight(const char* fmt, uint32_t layer);
    VkBuffer GetWeight(const char* name);

    void CreateComputePipelines();
    void CreateGPUBuffer(VkBuffer& buf, VkDeviceMemory& mem, size_t size, bool deviceLocal);
};
