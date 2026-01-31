/**
 * Test program for StreamingGGUFLoaderQt with memory-mapped file access
 * Tests loading of large GGUF models (3-70GB) without "bad allocation" errors
 */

#include "streaming_gguf_loader_qt.h"
#include <iostream>
#include <string>
#include <chrono>

void printMemoryStats(const StreamingGGUFLoader//MemoryStats& stats) {


}

int main(int argc, char* argv[]) {


    if (argc < 2) {


        return 1;
    }
    
    std::string modelPath = argv[1];


    // Create loader
    StreamingGGUFLoaderQt loader;
    
    // Measure load time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Load model
    bool success = loader.loadModel(modelPath);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (!success) {


        return 1;
    }


    // Print model info


    // Print memory stats
    printMemoryStats(loader.getMemoryStats());
    
    // Print some metadata keys
    
    auto keys = loader.getAllMetadataKeys();
    int count = 0;
    for (const auto& key : keys) {
        
        if (++count >= 10) {
            
            break;
        }
    }
    
    // Test tensor access (without loading - just verify it exists)
    
    if (loader.getTensorCount() > 0) {


    }


    return 0;
}
