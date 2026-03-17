#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <gguf_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return 1;
    }

    // Read magic
    char magic[4];
    file.read(magic, 4);
    
    if (std::memcmp(magic, "GGUF", 4) != 0) {
        std::cerr << "Not a GGUF file!" << std::endl;
        return 1;
    }

    // Read version and counts
    uint32_t version;
    uint64_t tensor_count, kv_count;
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&tensor_count), 8);
    file.read(reinterpret_cast<char*>(&kv_count), 8);

    std::cout << "=== Quick GGUF Check ===" << std::endl;
    std::cout << "File: " << argv[1] << std::endl;
    std::cout << "Version: " << version << std::endl;
    std::cout << "Tensors: " << tensor_count << std::endl;
    std::cout << "Metadata KVs: " << kv_count << std::endl;
    std::cout << std::endl;

    // Just look at file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    
    std::cout << "File size: " << (file_size / 1024.0 / 1024.0 / 1024.0) << " GB" << std::endl;
    
    // Check filename for quantization hint
    std::string filename = argv[1];
    bool has_q2k = (filename.find("Q2_K") != std::string::npos || filename.find("q2_k") != std::string::npos);
    bool has_q3k = (filename.find("Q3_K") != std::string::npos || filename.find("q3_k") != std::string::npos);
    bool has_q4k = (filename.find("Q4_K") != std::string::npos || filename.find("q4_k") != std::string::npos);
    
    std::cout << std::endl;
    std::cout << "Quantization type (from filename):" << std::endl;
    if (has_q2k) std::cout << "  ✅ Q2_K detected" << std::endl;
    if (has_q3k) std::cout << "  ✅ Q3_K detected" << std::endl;
    if (has_q4k) std::cout << "  ✅ Q4_K detected" << std::endl;
    if (!has_q2k && !has_q3k && !has_q4k) std::cout << "  ⚠ Unknown quantization" << std::endl;
    
    std::cout << std::endl;
    std::cout << "This model can be used to test Q2_K/Q3_K support!" << std::endl;
    
    return 0;
}
