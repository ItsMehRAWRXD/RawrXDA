#pragma once

#include <memory>
#include <string>
#include <vector>
#include "vulkan_compute.h"

/**
 * @brief Vulkan Inference Engine - Integrates VulkanCompute with GGML backend
 * 
 * This class manages GPU acceleration for LLM inference using Vulkan.
 * It acts as a bridge between GGML tensor operations and Vulkan compute kernels.
 * 
 * Features:
 * - Automatic GPU detection (AMD, NVIDIA, Intel)
 * - Tensor transfer to/from GPU memory
 * - Compute kernel dispatch for matmul, attention, normalization
 * - Async command buffer pooling for high performance
 * - Fallback to CPU if GPU unavailable
 */
class VulkanInferenceEngine {
public:
    VulkanInferenceEngine();
    ~VulkanInferenceEngine();

    /**
     * @brief Initialize Vulkan GPU acceleration
     * @return true if successful, false if GPU unavailable (CPU fallback will be used)
     */
    bool Initialize();

    /**
     * @brief Check if Vulkan GPU is available and initialized
     */
    bool IsGPUAvailable() const;

    /**
     * @brief Get GPU device information (name, memory, capabilities)
     */
    VulkanDeviceInfo GetGPUInfo() const;

    /**
     * @brief Load a tensor to GPU memory
     * @param tensor_name Unique identifier for the tensor
     * @param data_ptr Host memory pointer
     * @param size_bytes Size of data
     * @return GPU handle or 0 if failed
     */
    uint32_t LoadTensorToGPU(const std::string& tensor_name, const void* data_ptr, size_t size_bytes);

    /**
     * @brief Copy GPU tensor result back to CPU memory
     * @param gpu_handle Handle returned from LoadTensorToGPU or compute operation
     * @param output_ptr Destination host memory
     * @param size_bytes Size to copy
     * @return true if successful
     */
    bool CopyTensorFromGPU(uint32_t gpu_handle, void* output_ptr, size_t size_bytes);

    /**
     * @brief Execute matrix multiplication on GPU
     * Computes: C = A @ B with shapes A[M,K] B[K,N] -> C[M,N]
     */
    uint32_t MatMulGPU(uint32_t a_handle, uint32_t b_handle, uint32_t m, uint32_t k, uint32_t n);

    /**
     * @brief Execute async matrix multiplication
     * Returns immediately; result available later via CopyTensorFromGPU
     */
    uint32_t MatMulGPUAsync(uint32_t a_handle, uint32_t b_handle, uint32_t m, uint32_t k, uint32_t n);

    /**
     * @brief Execute RoPE (Rotary Position Encoding) on GPU
     */
    uint32_t RoPEGPU(uint32_t input_handle, uint32_t seq_pos, uint32_t dim, uint32_t rotation_dim);

    /**
     * @brief Execute RMSNorm on GPU
     */
    uint32_t RMSNormGPU(uint32_t input_handle, uint32_t size, float epsilon);

    /**
     * @brief Execute SiLU activation on GPU
     */
    uint32_t SiLUGPU(uint32_t input_handle, uint32_t size);

    /**
     * @brief Execute Softmax on GPU
     */
    uint32_t SoftmaxGPU(uint32_t input_handle, uint32_t size);

    /**
     * @brief Execute attention on GPU (multi-head attention)
     */
    uint32_t AttentionGPU(uint32_t q_handle, uint32_t k_handle, uint32_t v_handle,
                          uint32_t seq_len, uint32_t head_dim, uint32_t num_heads);

    /**
     * @brief Initialize KV cache for efficient inference
     * @param num_layers Number of transformer layers
     * @param max_seq_len Maximum sequence length
     * @param head_dim Dimension per attention head
     */
    bool InitializeKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim);

    /**
     * @brief Free all GPU resources
     */
    void Shutdown();

    /**
     * @brief Synchronize GPU and wait for pending operations
     */
    void Synchronize();

    /**
     * @brief Get diagnostic info (device name, memory usage, etc.)
     */
    std::string GetDiagnostics() const;

private:
    std::unique_ptr<VulkanCompute> vulkan_compute_;
    bool is_initialized_;
    bool gpu_available_;
    uint32_t next_handle_;

    // Tensor tracking for memory management
    struct TensorInfo {
        std::string name;
        size_t size_bytes;
        uint32_t gpu_buffer_idx;
    };
    std::vector<TensorInfo> loaded_tensors_;

    uint32_t AllocateHandle();
};
