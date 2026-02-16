/**
 * Minimal standalone test for StreamingGGUFLoaderQt
 * Compiles with: cl /EHsc /std:c++20 /I. test_minimal_streaming.cpp memory_mapped_file.cpp streaming_gguf_loader_qt.cpp /Fe:test_streaming.exe
 */

#include "streaming_gguf_loader_qt.h"
#include "logging/logger.h"
#include <string>

int main(int argc, char* argv[]) {
    Logger logger("TestMinimalStreaming");
    logger.info("=== Minimal StreamingGGUFLoaderQt Test ===");
    
    if (argc < 2) {
        logger.error("Usage: {} <gguf-file>", argv[0]);
        return 1;
    }
    
    std::string modelPath = argv[1];
    logger.info("Loading: {}", modelPath);
    
    StreamingGGUFLoaderQt loader;
    
    if (!loader.loadModel(modelPath)) {
        logger.error("FAILED: {}", loader.getLastError());
        return 1;
    }
    
    logger.info("SUCCESS!");
    logger.info("Model: {}", loader.getModelName());
    logger.info("Architecture: {}", loader.getModelArchitecture());
    logger.info("Tensors: {}", loader.getTensorCount());
    logger.info("Metadata: {}", loader.getMetadataCount());
    
    auto stats = loader.getMemoryStats();
    logger.info("File size: {} MB", stats.totalFileSize / 1024 / 1024);
    
    return 0;
}
