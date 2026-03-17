#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Minimal GPU abstraction facade replacing Vulkan/ROCm. No external GPU headers required.

#if defined(NO_VULKAN)
#include "../src/gpu/gpu_backend.h"
namespace rawrxd { namespace gpu { using Device = ::RawrXD::GPU::Backend; using DeviceInfo = ::RawrXD::GPU::Backend; inline ::RawrXD::GPU::Backend* createDevice() { return ::RawrXD::GPU::get_global_backend(); } } }
#else
namespace rawrxd { namespace gpu { class Device; struct DeviceInfo; Device* createDevice(); } }
#endif

struct VulkanDeviceInfo {
    std::string device_name;
    uint32_t vendor_id{0};
    uint32_t device_id{0};
    int compute_queue_family{0};
    bool supports_compute{false};
    int compute_units{0};
    size_t vram_bytes{0};
};

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    bool Initialize();
    void Cleanup();

    VulkanDeviceInfo GetDeviceInfo() const;

    // CPU-backed implementations (agentic GPU abstraction handles future acceleration)
    bool LoadShader(const std::string& name, const std::string& spirv_path);
    bool CreateComputePipeline(const std::string& shader_name);

    bool ExecuteMatMul(const float* input_a, const float* input_b, float* output,
                       uint32_t m, uint32_t k, uint32_t n);
    bool ExecuteAttention(const float* queries, const float* keys, const float* values,
                          float* output, uint32_t seq_len, uint32_t head_dim);
    bool ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim);
    bool ExecuteRMSNorm(float* data, uint32_t size, float epsilon);
    bool ExecuteSiLU(float* data, uint32_t size);
    bool ExecuteSoftmax(float* data, uint32_t size);
    bool ExecuteDequantize(const uint8_t* quantized, float* output,
                           uint32_t elements, const std::string& quant_type);

#if defined(NO_VULKAN)
    // Simple buffer-based GPU simulation APIs used by inference engine when Vulkan is disabled
    bool IsAMDDevice() const;
    bool IsNvidiaDevice() const;
    bool AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size);
    bool CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size);
    bool CopyBufferToHost(uint32_t buffer_idx, void* dst, size_t size);
    bool DispatchMatMul(uint32_t input_a_idx, uint32_t input_b_idx, uint32_t out_idx, uint32_t M, uint32_t K, uint32_t N);
    bool DispatchMatMulAsync(uint32_t input_a_idx, uint32_t input_b_idx, uint32_t out_idx, uint32_t M, uint32_t K, uint32_t N);
    bool DispatchRoPE(uint32_t input_idx, uint32_t out_idx, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim);
#endif

private:
    std::unique_ptr<rawrxd::gpu::Device> device_;
    VulkanDeviceInfo device_info_;
#if defined(NO_VULKAN)
    // Simple host-side buffer storage for NO_VULKAN mode
    std::unordered_map<uint32_t, std::pair<void*, size_t>> allocated_buffers_;
    uint32_t next_buffer_idx_ = 1;
#endif
};
