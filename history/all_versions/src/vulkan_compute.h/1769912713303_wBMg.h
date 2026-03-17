#pragma once

#include <vector>
#include <cstdint>
#include <string>

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

    struct DeviceInfo {
        std::string deviceName = "Unknown Device";
        uint32_t vendorID = 0;
        uint32_t deviceID = 0;
    };
    DeviceInfo GetDeviceInfo() const;

private:
    struct Impl;
    Impl* m_impl; // Pimpl to hide vulkan headers
};
