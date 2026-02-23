#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cstdint>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: gguf_diagnostic <gguf_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << argv[1] << std::endl;
        return 1;
    }

    // Read GGUF header
    uint32_t magic, version;
    uint64_t tensor_count, metadata_count;

    file.read(reinterpret_cast<char*>(&magic), 4);
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&tensor_count), 8);
    file.read(reinterpret_cast<char*>(&metadata_count), 8);

    std::cout << "=== GGUF HEADER ===" << std::endl;
    std::cout << "Magic: 0x" << std::hex << magic << std::dec << std::endl;
    std::cout << "Version: " << version << std::endl;
    std::cout << "Tensor Count: " << tensor_count << std::endl;
    std::cout << "Metadata Count: " << metadata_count << std::endl;
    std::cout << "Metadata Offset: " << file.tellg() << std::endl;

    // Read first few metadata entries
    std::cout << "\n=== FIRST 5 METADATA ENTRIES ===" << std::endl;
    uint64_t entries_to_read = (metadata_count < 5) ? metadata_count : 5;
    for (uint64_t i = 0; i < entries_to_read; ++i) {
        uint64_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), 8);

        if (key_len > 1000000) {  // Safety check
            std::cout << "Entry " << i << ": INVALID KEY LENGTH: " << key_len << " at offset " 
                      << static_cast<uint64_t>(file.tellg()) - 8 << std::endl;
            break;
        }

        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        uint32_t value_type;
        file.read(reinterpret_cast<char*>(&value_type), 4);

        std::cout << "Entry " << i << ": key='" << key << "' type=" << value_type << std::endl;

        // Try to read the value based on type
        if (value_type == 1) {  // String
            uint64_t val_len;
            file.read(reinterpret_cast<char*>(&val_len), 8);
            if (val_len > 10000) {
                std::cout << "  Value: STRING (length=" << val_len << ", truncated)" << std::endl;
                file.seekg(std::min(static_cast<uint64_t>(100), val_len), std::ios::cur);
            } else {
                std::string value(val_len, '\0');
                file.read(&value[0], val_len);
                std::cout << "  Value: " << value << std::endl;
            }
        } else if (value_type == 4) {  // uint32
            uint32_t val;
            file.read(reinterpret_cast<char*>(&val), 4);
            std::cout << "  Value: " << val << std::endl;
        } else if (value_type == 5) {  // int32
            int32_t val;
            file.read(reinterpret_cast<char*>(&val), 4);
            std::cout << "  Value: " << val << std::endl;
        } else {
            std::cout << "  Value: (type " << value_type << " - skipping)" << std::endl;
        }
    }

    return 0;
}
