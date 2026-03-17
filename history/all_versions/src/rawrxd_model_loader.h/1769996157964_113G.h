#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class RawrXDModelLoader {
public:
    struct TensorInfo {
        std::string name;
        std::vector<int64_t> shape;
        size_t offset;
    };
    
    RawrXDModelLoader(); // Added constructor
    ~RawrXDModelLoader();
    
    bool Load(const std::wstring& path, VkDevice device, VkPhysicalDevice physDevice);
    int GetMetadataInt(const std::string& key);
    float* GetTensor(const std::string& name);

private:
    class Impl;
    Impl* m_impl;
};
