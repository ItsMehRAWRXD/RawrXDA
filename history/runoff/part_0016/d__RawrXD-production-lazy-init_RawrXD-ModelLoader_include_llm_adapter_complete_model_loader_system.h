#pragma once

#include <string>
#include <vector>
#include <memory>

namespace rawr_xd {

// Complete model loader system for handling all model formats
class CompleteModelLoaderSystem {
public:
    CompleteModelLoaderSystem();
    ~CompleteModelLoaderSystem();
    
    // Initialize the loader system
    bool Initialize();
    
    // Load a model from file
    bool LoadModel(const std::string& filepath);
    
    // Load a model from memory
    bool LoadModelFromMemory(const void* data, size_t size);
    
    // Unload current model
    void UnloadModel();
    
    // Get loaded model info
    std::string GetModelInfo() const;
    
    // Check if model is loaded
    bool IsModelLoaded() const;
    
    // Get model format
    std::string GetModelFormat() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rawr_xd