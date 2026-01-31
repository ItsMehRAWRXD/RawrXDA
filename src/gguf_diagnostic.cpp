#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cstdint>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        
        return 1;
    }

    // Read GGUF header
    uint32_t magic, version;
    uint64_t tensor_count, metadata_count;

    file.read(reinterpret_cast<char*>(&magic), 4);
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&tensor_count), 8);
    file.read(reinterpret_cast<char*>(&metadata_count), 8);


    // Read first few metadata entries
    
    uint64_t entries_to_read = (metadata_count < 5) ? metadata_count : 5;
    for (uint64_t i = 0; i < entries_to_read; ++i) {
        uint64_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), 8);

        if (key_len > 1000000) {  // Safety check
            
            break;
        }

        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        uint32_t value_type;
        file.read(reinterpret_cast<char*>(&value_type), 4);


        // Try to read the value based on type
        if (value_type == 1) {  // String
            uint64_t val_len;
            file.read(reinterpret_cast<char*>(&val_len), 8);
            if (val_len > 10000) {
                
                file.seekg(std::min(static_cast<uint64_t>(100), val_len), std::ios::cur);
            } else {
                std::string value(val_len, '\0');
                file.read(&value[0], val_len);
                
            }
        } else if (value_type == 4) {  // uint32
            uint32_t val;
            file.read(reinterpret_cast<char*>(&val), 4);
            
        } else if (value_type == 5) {  // int32
            int32_t val;
            file.read(reinterpret_cast<char*>(&val), 4);
            
        } else {
            
        }
    }

    return 0;
}
