#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct gguf_context;

namespace RawrXD {

class GGUFLoader {
public:
    GGUFLoader();
    ~GGUFLoader();

    // Load GGUF file from path
    bool LoadFromFile(const std::string& file_path);
    
    // Load GGUF from memory buffer
    bool LoadFromMemory(const void* data, size_t size);
    
    // Unload current model
    void Unload();
    
    // Check if a model is loaded
    bool IsLoaded() const;
    
    // Get model information
    std::string GetModelName() const;
    std::string GetModelDescription() const;
    std::vector<std::string> GetTensorNames() const;
    
    // Get tensor data
    const void* GetTensorData(const std::string& tensor_name) const;
    std::vector<size_t> GetTensorShape(const std::string& tensor_name) const;
    
    // Get quantization type
    int GetTensorType(const std::string& tensor_name) const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace RawrXD