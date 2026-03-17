#include "auto_model_loader.h"
#include <iostream>

int main() {
    std::cout << "Testing Automatic Model Loader..." << std::endl;
    
    // Test automatic model loading
    AutoModelLoader::AutoModelLoader& loader = AutoModelLoader::AutoModelLoader::GetInstance();
    
    std::cout << "Auto-load enabled: " << (loader.isAutoLoadEnabled() ? "Yes" : "No") << std::endl;
    
    // Discover available models
    auto models = loader.discoverAvailableModels();
    std::cout << "Found " << models.size() << " models" << std::endl;
    
    for (const auto& model : models) {
        std::cout << "  - " << model << std::endl;
    }
    
    // Try to auto-load a model
    if (loader.autoLoadModel()) {
        std::cout << "Auto-load successful!" << std::endl;
        std::cout << "Loaded model: " << loader.getLoadedModel() << std::endl;
    } else {
        std::cout << "Auto-load failed or no models available" << std::endl;
    }
    
    std::cout << "Status: " << loader.getStatus() << std::endl;
    
    return 0;
}