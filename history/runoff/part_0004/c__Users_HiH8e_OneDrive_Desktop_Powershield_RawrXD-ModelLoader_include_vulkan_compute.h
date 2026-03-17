#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

// SCALAR-ONLY: All GPU/Vulkan code removed - pure CPU scalar operations

struct VulkanDeviceInfo {
    std::string device_name;
    uint32_t vendor_id;
    uint32_t device_id;
    bool supports_compute;
    uint32_t compute_queue_family;
};

struct ComputeShader {
    std::string name;
};

struct VulkanTensor {
    std::string name;
    size_t size_bytes{0};
    std::vector<float> host_data;  // Scalar data stored in CPU memory
};

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    bool Initialize();
    bool LoadShader(const std::string& name, const std::string& spirv_path);
    bool CreateComputePipeline(const std::string& shader_name);
    VulkanTensor TransferGGUFTensor(const std::string& tensor_name,
                                    const void* data_ptr,
                                    size_t size_bytes,
                                    uint32_t usage = 0);
    void ReleaseTensors();
    bool EnsureMatMulPipeline(const std::string& spirv_path);
    bool DispatchMatMul(uint32_t input_a_idx,
                        uint32_t input_b_idx,
                        uint32_t output_idx,
                        uint32_t M,
                        uint32_t K,
                        uint32_t N);
    
    VulkanDeviceInfo GetDeviceInfo() const { return device_info_; }
    bool IsAMDDevice() const { return device_info_.vendor_id == 0x1002; }
    bool IsNvidiaDevice() const { return device_info_.vendor_id == 0x10DE; }
    
    bool AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size);
    bool CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size);
    bool CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size);
    
    // Scalar CPU implementations (no GPU)
    bool ExecuteMatMul(const float* input_a, const float* input_b, 
                       float* output, uint32_t m, uint32_t k, uint32_t n);
    bool ExecuteAttention(const float* queries, const float* keys, const float* values,
                         float* output, uint32_t seq_len, uint32_t head_dim);
    bool ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim);
    bool ExecuteRMSNorm(float* data, uint32_t size, float epsilon = 1e-5f);
    bool ExecuteSiLU(float* data, uint32_t size);
    bool ExecuteSoftmax(float* data, uint32_t size);
    bool ExecuteDequantize(const uint8_t* quantized, float* output,
                           uint32_t elements, const std::string& quant_type);
    
    void Cleanup();

private:
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateCommandPool();
    bool LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code);
    uint32_t FindMemoryType(uint32_t type_filter, uint32_t properties);

    VulkanDeviceInfo device_info_;
    std::unordered_map<std::string, ComputeShader> shaders_;
    std::vector<VulkanTensor> uploaded_tensors_;
};
