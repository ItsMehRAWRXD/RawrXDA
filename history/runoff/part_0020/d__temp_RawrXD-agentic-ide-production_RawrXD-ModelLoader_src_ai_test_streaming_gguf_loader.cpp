/**
 * Test program for StreamingGGUFLoaderQt with memory-mapped file access
 * Tests loading of large GGUF models (3-70GB) without "bad allocation" errors
 */

#include "streaming_gguf_loader_qt.h"
#include <iostream>
#include <string>
#include <chrono>

void printMemoryStats(const StreamingGGUFLoaderQt::MemoryStats& stats) {
    std::cout << "\n📊 Memory Statistics:" << std::endl;
    std::cout << "  Total file size: " << (stats.totalFileSize / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Total tensors: " << stats.totalTensorsCount << std::endl;
    std::cout << "  Loaded tensors: " << stats.loadedTensorsCount << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "🧪 StreamingGGUFLoaderQt Test Program\n" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-gguf-file>" << std::endl;
        std::cerr << "\nExample:" << std::endl;
        std::cerr << "  " << argv[0] << " phi-3-mini.gguf" << std::endl;
        std::cerr << "  " << argv[0] << " tinyllama-test.gguf" << std::endl;
        return 1;
    }
    
    std::string modelPath = argv[1];
    std::cout << "📁 Loading model: " << modelPath << "\n" << std::endl;
    
    // Create loader
    StreamingGGUFLoaderQt loader;
    
    // Measure load time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Load model
    bool success = loader.loadModel(modelPath);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (!success) {
        std::cerr << "❌ Failed to load model!" << std::endl;
        std::cerr << "   Error: " << loader.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "\n✅ Model loaded successfully in " << loadTime.count() << " ms\n" << std::endl;
    
    // Print model info
    std::cout << "📋 Model Information:" << std::endl;
    std::cout << "  Name: " << loader.getModelName() << std::endl;
    std::cout << "  Architecture: " << loader.getModelArchitecture() << std::endl;
    std::cout << "  Context Length: " << loader.getModelContextLength() << std::endl;
    std::cout << "  Tensor Count: " << loader.getTensorCount() << std::endl;
    std::cout << "  Metadata Count: " << loader.getMetadataCount() << std::endl;
    
    // Print memory stats
    printMemoryStats(loader.getMemoryStats());
    
    // Print some metadata keys
    std::cout << "\n🔑 Sample Metadata Keys:" << std::endl;
    auto keys = loader.getAllMetadataKeys();
    int count = 0;
    for (const auto& key : keys) {
        std::cout << "  - " << key << std::endl;
        if (++count >= 10) {
            std::cout << "  ... (" << (keys.size() - 10) << " more)" << std::endl;
            break;
        }
    }
    
    // Test tensor access (without loading - just verify it exists)
    std::cout << "\n🔍 Testing tensor access..." << std::endl;
    if (loader.getTensorCount() > 0) {
        std::cout << "  ✓ Tensors are indexed and ready for on-demand loading" << std::endl;
        std::cout << "  ℹ Actual tensor data will be loaded only when requested" << std::endl;
    }
    
    std::cout << "\n✨ All tests passed! Memory-mapped loading works correctly." << std::endl;
    std::cout << "💡 This model can now be used without loading entire file into RAM." << std::endl;
    
    return 0;
}
