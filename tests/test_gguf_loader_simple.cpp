#include "gguf_loader.h"
#include <iostream>
#include <cassert>
#include <filesystem>

#include "logging/logger.h"
static Logger s_logger("test_gguf_loader_simple");

int main(int argc, char* argv[]) {
    if (argc < 2) {
        s_logger.error( "Usage: test_gguf_loader_simple <model.gguf>" << std::endl;
        return 1;
    }
    
    std::string model_path = argv[1];
    if (!std::filesystem::exists(model_path)) {
        s_logger.error( "Model file not found: " << model_path << std::endl;
        return 1;
    }
    
    try {
        s_logger.info("=== GGUF Loader Improvements Test (Simple) ===");
        
        GGUFLoader loader;
        
        // Test 1: Open and parse header
        s_logger.info("Test 1: Opening GGUF file...");
        if (!loader.Open(model_path)) {
            s_logger.error( "Failed to open GGUF file" << std::endl;
            return 1;
        }
        s_logger.info("✓ File opened successfully");
        
        GGUFHeader header = loader.GetHeader();
        s_logger.info("  Magic: 0x");
        s_logger.info("  Version: ");
        s_logger.info("  Tensors: ");
        s_logger.info("  Metadata KV pairs: ");
        
        // Test 2: Test alignment helper
        s_logger.info("\nTest 2: Testing alignment helper...");
        uint64_t test_offsets[] = {0, 1, 31, 32, 33, 63, 64, 100, 1024};
        for (uint64_t offset : test_offsets) {
            uint64_t aligned = loader.AlignTo32Bytes(offset);
            if (aligned % 32 != 0) {
                s_logger.error( "  ✗ Alignment failed for offset " << offset 
                          << ": got " << aligned << std::endl;
                return 1;
            }
            if (aligned < offset) {
                s_logger.error( "  ✗ Alignment produced smaller value for offset " << offset 
                          << ": got " << aligned << std::endl;
                return 1;
            }
        }
        s_logger.info("  ✓ All alignment calculations correct");
        
        loader.Close();
        s_logger.info("\n=== SIMPLE TEST PASSED ===");
        s_logger.info("GGUF loader alignment improvements are working correctly!");
        
    } catch (const std::exception& e) {
        s_logger.error( "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}