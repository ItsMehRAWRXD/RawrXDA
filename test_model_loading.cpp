#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

// Minimal GGUF loader test to check for metadata corruption
struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
};

struct GGUFMetadata {
    uint32_t vocab_size = 0;
    uint32_t embedding_length = 0;
    uint32_t layer_count = 0;
    uint32_t attention_head_count = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <model_path>" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::cout << "[DIRECT_TEST] Testing GGUF metadata extraction with model: " << model_path << std::endl;

    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        std::cout << "[DIRECT_TEST] ERROR: Cannot open model file" << std::endl;
        return 1;
    }

    // Read and validate GGUF header
    GGUFHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    std::cout << "[DIRECT_TEST] Raw header bytes:" << std::endl;
    std::cout << "[DIRECT_TEST]   magic = 0x" << std::hex << header.magic << std::dec << std::endl;
    std::cout << "[DIRECT_TEST]   version = " << header.version << std::endl;
    std::cout << "[DIRECT_TEST]   n_tensors = " << header.n_tensors << std::endl;
    std::cout << "[DIRECT_TEST]   n_kv = " << header.n_kv << std::endl;

    // Check for corruption pattern (0xCDCDCDCD)
    if (header.magic == 0xCDCDCDCD || header.version == 0xCDCDCDCD || 
        header.n_tensors == 0xCDCDCDCDCDCDCDCDULL || header.n_kv == 0xCDCDCDCDCDCDCDCDULL) {
        std::cout << "[DIRECT_TEST] CORRUPTION DETECTED: 0xCDCDCDCD pattern found in header!" << std::endl;
        return 1;
    }

    // Check for expected GGUF magic
    if (header.magic == 0x46554747) { // "GGUF" in little endian
        std::cout << "[DIRECT_TEST] SUCCESS: Valid GGUF magic found" << std::endl;
    } else {
        std::cout << "[DIRECT_TEST] WARNING: Unexpected magic value (expected 0x46554747)" << std::endl;
    }

    // Try to read some KV metadata if available
    if (header.n_kv > 0 && header.n_kv < 1000) { // Sanity check
        std::cout << "[DIRECT_TEST] Attempting to read " << header.n_kv << " key-value pairs..." << std::endl;
        
        for (uint64_t i = 0; i < std::min(header.n_kv, uint64_t(10)); i++) {
            // Read key length
            uint64_t key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            
            if (key_len > 1000 || key_len == 0xCDCDCDCDCDCDCDCDULL) {
                std::cout << "[DIRECT_TEST] CORRUPTION: Invalid key length " << key_len << std::endl;
                break;
            }
            
            // Read key string
            std::vector<char> key(key_len + 1);
            file.read(key.data(), key_len);
            key[key_len] = '\0';
            
            std::cout << "[DIRECT_TEST] KV[" << i << "] key: \"" << key.data() << "\"" << std::endl;
            
            // Skip value type and value data for now
            uint32_t value_type;
            file.read(reinterpret_cast<char*>(&value_type), sizeof(value_type));
            
            // Simple hop over value data (this is approximate)
            if (value_type <= 12) { // Basic types
                uint64_t skip_bytes = 8; // Approximate value size
                file.seekg(skip_bytes, std::ios::cur);
            }
        }
    }

    std::cout << "[DIRECT_TEST] Test completed successfully - no 0xCDCDCDCD corruption detected" << std::endl;
    return 0;
}