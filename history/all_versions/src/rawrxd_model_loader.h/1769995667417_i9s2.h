#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class RawrXDModelLoader {
    void* mapped_data = nullptr;
    size_t file_size = 0;
    
public:
    struct TensorInfo {
        std::string name;
        std::vector<int64_t> shape;
        size_t offset;
    };
    
    bool Load(const std::wstring& path, VkDevice device, VkPhysicalDevice physDevice);
    int GetMetadataInt(const std::string& key);
    float* GetTensor(const std::string& name);
    
    ~RawrXDModelLoader();
};
