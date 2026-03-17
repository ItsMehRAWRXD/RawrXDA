#pragma once

#include <vector>
#include <cstdint>

class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    bool Initialize();
    void Cleanup();
    
    // Compute operations
    bool LoadShader(const std::vector<uint32_t>& spirv);
    bool ExecuteCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    bool Wait();

private:
    struct Impl;
    Impl* m_impl; // Pimpl to hide vulkan headers
};
