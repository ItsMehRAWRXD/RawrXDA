/**
 * Minimal standalone test for StreamingGGUFLoaderQt
 * Compiles with: cl /EHsc /std:c++20 /I. test_minimal_streaming.cpp memory_mapped_file.cpp streaming_gguf_loader_qt.cpp /Fe:test_streaming.exe
 */

#include "streaming_gguf_loader_qt.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== Minimal StreamingGGUFLoaderQt Test ===" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <gguf-file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " D:\\temp\\RawrXD-agentic-ide-production\\RawrXD-ModelLoader\\phi-3-mini.gguf" << std::endl;
        return 1;
    }
    
    std::string modelPath = argv[1];
    std::cout << "Loading: " << modelPath << std::endl;
    
    StreamingGGUFLoaderQt loader;
    
    if (!loader.loadModel(modelPath)) {
        std::cerr << "FAILED: " << loader.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "\nSUCCESS!" << std::endl;
    std::cout << "Model: " << loader.getModelName() << std::endl;
    std::cout << "Architecture: " << loader.getModelArchitecture() << std::endl;
    std::cout << "Tensors: " << loader.getTensorCount() << std::endl;
    std::cout << "Metadata: " << loader.getMetadataCount() << std::endl;
    
    auto stats = loader.getMemoryStats();
    std::cout << "File size: " << (stats.totalFileSize / 1024 / 1024) << " MB" << std::endl;
    
    return 0;
}
