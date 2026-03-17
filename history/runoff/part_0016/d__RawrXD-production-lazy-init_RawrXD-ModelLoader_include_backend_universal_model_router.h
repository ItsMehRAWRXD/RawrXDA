#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Universal model router for handling different model formats
class UniversalModelRouter {
public:
    UniversalModelRouter();
    ~UniversalModelRouter();
    
    // Initialize the router
    bool Initialize();
    
    // Load a model
    bool LoadModel(const std::string& path, const std::string& format);
    
    // Unload current model
    void UnloadModel();
    
    // Get loaded model info
    std::string GetModelInfo() const;
    
    // Check if model is loaded
    bool IsModelLoaded() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD