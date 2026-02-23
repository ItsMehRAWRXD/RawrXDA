#include "gguf_loader.h"
#include <iostream>
#include <cassert>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: test_gguf_loader_simple <model.gguf>" << std::endl;
        return 1;
    }
    
    std::string model_path = argv[1];
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Model file not found: " << model_path << std::endl;
        return 1;
    }
    
    try {
        std::cout << "=== GGUF Loader Improvements Test (Simple) ===" << std::endl;
        
        GGUFLoader loader;
        
        // Test 1: Open and parse header
        std::cout << "Test 1: Opening GGUF file..." << std::endl;
        if (!loader.Open(model_path)) {
            std::cerr << "Failed to open GGUF file" << std::endl;
            return 1;
        }
        std::cout << "✓ File opened successfully" << std::endl;
        
        GGUFHeader header = loader.GetHeader();
        std::cout << "  Magic: 0x" << std::hex << header.magic << std::dec << std::endl;
        std::cout << "  Version: " << header.version << std::endl;
        std::cout << "  Tensors: " << header.tensor_count << std::endl;
        std::cout << "  Metadata KV pairs: " << header.metadata_kv_count << std::endl;
        
        // Test 2: Test alignment helper
        std::cout << "\nTest 2: Testing alignment helper..." << std::endl;
        uint64_t test_offsets[] = {0, 1, 31, 32, 33, 63, 64, 100, 1024};
        for (uint64_t offset : test_offsets) {
            uint64_t aligned = loader.AlignTo32Bytes(offset);
            if (aligned % 32 != 0) {
                std::cerr << "  ✗ Alignment failed for offset " << offset 
                          << ": got " << aligned << std::endl;
                return 1;
            }
            if (aligned < offset) {
                std::cerr << "  ✗ Alignment produced smaller value for offset " << offset 
                          << ": got " << aligned << std::endl;
                return 1;
            }
        }
        std::cout << "  ✓ All alignment calculations correct" << std::endl;
        
        loader.Close();
        std::cout << "\n=== SIMPLE TEST PASSED ===" << std::endl;
        std::cout << "GGUF loader alignment improvements are working correctly!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}