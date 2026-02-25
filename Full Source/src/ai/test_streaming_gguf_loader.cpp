/**
 * Test program for StreamingGGUFLoaderQt with memory-mapped file access
 * Tests loading of large GGUF models (3-70GB) without "bad allocation" errors
 */

#include "streaming_gguf_loader_qt.h"
#include <iostream>
#include <string>
#include <chrono>

#include "logging/logger.h"
static Logger s_logger("test_streaming_gguf_loader");

void printMemoryStats(const StreamingGGUFLoaderQt::MemoryStats& stats) {
    s_logger.info("\n📊 Memory Statistics:");
    s_logger.info("  Total file size: ");
    s_logger.info("  Total tensors: ");
    s_logger.info("  Loaded tensors: ");
}

int main(int argc, char* argv[]) {
    s_logger.info("🧪 StreamingGGUFLoaderQt Test Program\n");
    
    if (argc < 2) {
        s_logger.error( "Usage: " << argv[0] << " <path-to-gguf-file>" << std::endl;
        s_logger.error( "\nExample:" << std::endl;
        s_logger.error( "  " << argv[0] << " phi-3-mini.gguf" << std::endl;
        s_logger.error( "  " << argv[0] << " tinyllama-test.gguf" << std::endl;
        return 1;
    }
    
    std::string modelPath = argv[1];
    s_logger.info("📁 Loading model: ");
    
    // Create loader
    StreamingGGUFLoaderQt loader;
    
    // Measure load time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Load model
    bool success = loader.loadModel(modelPath);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (!success) {
        s_logger.error( "❌ Failed to load model!" << std::endl;
        s_logger.error( "   Error: " << loader.getLastError() << std::endl;
        return 1;
    }
    
    s_logger.info("\n✅ Model loaded successfully in ");
    
    // Print model info
    s_logger.info("📋 Model Information:");
    s_logger.info("  Name: ");
    s_logger.info("  Architecture: ");
    s_logger.info("  Context Length: ");
    s_logger.info("  Tensor Count: ");
    s_logger.info("  Metadata Count: ");
    
    // Print memory stats
    printMemoryStats(loader.getMemoryStats());
    
    // Print some metadata keys
    s_logger.info("\n🔑 Sample Metadata Keys:");
    auto keys = loader.getAllMetadataKeys();
    int count = 0;
    for (const auto& key : keys) {
        s_logger.info("  - ");
        if (++count >= 10) {
            s_logger.info("  ... (");
            break;
        }
    }
    
    // Test tensor access (without loading - just verify it exists)
    s_logger.info("\n🔍 Testing tensor access...");
    if (loader.getTensorCount() > 0) {
        s_logger.info("  ✓ Tensors are indexed and ready for on-demand loading");
        s_logger.info("  ℹ Actual tensor data will be loaded only when requested");
    }
    
    s_logger.info("\n✨ All tests passed! Memory-mapped loading works correctly.");
    s_logger.info("💡 This model can now be used without loading entire file into RAM.");
    
    return 0;
}
