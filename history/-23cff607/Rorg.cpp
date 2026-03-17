#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

// GGUF format structures
struct GGUFHeader {
    char magic[4];      // "GGUF"
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataKVCount;
};

// GGML type enum
enum ggml_type {
    GGML_TYPE_F32  = 0,
    GGML_TYPE_F16  = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    GGML_TYPE_Q2_K = 10,
    GGML_TYPE_Q3_K = 11,
    GGML_TYPE_Q4_K = 12,
    GGML_TYPE_Q5_K = 13,
    GGML_TYPE_Q6_K = 14,
    GGML_TYPE_Q8_K = 15,
};

const char* type_name(int type) {
    switch(type) {
        case GGML_TYPE_F32: return "F32";
        case GGML_TYPE_F16: return "F16";
        case GGML_TYPE_Q4_0: return "Q4_0";
        case GGML_TYPE_Q4_1: return "Q4_1";
        case GGML_TYPE_Q5_0: return "Q5_0";
        case GGML_TYPE_Q5_1: return "Q5_1";
        case GGML_TYPE_Q8_0: return "Q8_0";
        case GGML_TYPE_Q8_1: return "Q8_1";
        case GGML_TYPE_Q2_K: return "Q2_K";
        case GGML_TYPE_Q3_K: return "Q3_K";
        case GGML_TYPE_Q4_K: return "Q4_K";
        case GGML_TYPE_Q5_K: return "Q5_K";
        case GGML_TYPE_Q6_K: return "Q6_K";
        case GGML_TYPE_Q8_K: return "Q8_K";
        default: return "UNKNOWN";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <gguf_file>" << std::endl;
        std::cout << "\nSearching for GGUF files..." << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::ifstream file(filename, std::ios::binary);
    
    if (!file) {
        std::cerr << "Failed to open: " << filename << std::endl;
        return 1;
    }

    std::cout << "=== GGUF Model Analysis ===" << std::endl;
    std::cout << "File: " << filename << std::endl;
    std::cout << std::endl;

    // Read header
    GGUFHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (std::memcmp(header.magic, "GGUF", 4) != 0) {
        std::cerr << "Invalid GGUF magic!" << std::endl;
        return 1;
    }

    std::cout << "Header:" << std::endl;
    std::cout << "  Magic: " << std::string(header.magic, 4) << std::endl;
    std::cout << "  Version: " << header.version << std::endl;
    std::cout << "  Tensor Count: " << header.tensorCount << std::endl;
    std::cout << "  Metadata KV Count: " << header.metadataKVCount << std::endl;
    std::cout << std::endl;

    // Skip metadata (simplified - just count bytes)
    std::cout << "Skipping metadata section..." << std::endl;
    
    // Read through metadata KV pairs (version 3 format)
    for (uint64_t i = 0; i < header.metadataKVCount; i++) {
        // Read key length
        uint64_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), 8);
        
        // Skip key
        file.seekg(key_len, std::ios::cur);
        
        // Read value type
        uint32_t value_type;
        file.read(reinterpret_cast<char*>(&value_type), 4);
        
        // Skip value based on type (simplified)
        if (value_type == 8) { // String
            uint64_t str_len;
            file.read(reinterpret_cast<char*>(&str_len), 8);
            file.seekg(str_len, std::ios::cur);
        } else if (value_type == 4 || value_type == 5) { // uint32/uint64
            file.seekg(value_type == 4 ? 4 : 8, std::ios::cur);
        } else if (value_type == 6 || value_type == 7) { // float/bool
            file.seekg(value_type == 6 ? 4 : 1, std::ios::cur);
        } else if (value_type == 9) { // Array
            uint32_t arr_type;
            uint64_t arr_len;
            file.read(reinterpret_cast<char*>(&arr_type), 4);
            file.read(reinterpret_cast<char*>(&arr_len), 8);
            
            int elem_size = (arr_type == 4) ? 4 : (arr_type == 5) ? 8 : 1;
            file.seekg(arr_len * elem_size, std::ios::cur);
        }
    }

    std::cout << std::endl;
    std::cout << "=== Tensor Information ===" << std::endl;
    std::cout << std::endl;

    // Read tensor info
    struct TensorInfo {
        std::string name;
        uint32_t n_dims;
        std::vector<uint64_t> dims;
        uint32_t type;
        uint64_t offset;
    };

    std::vector<TensorInfo> tensors;
    int q2k_count = 0, q3k_count = 0;
    
    for (uint64_t i = 0; i < header.tensorCount; i++) {
        TensorInfo info;
        
        // Read tensor name
        uint64_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), 8);
        info.name.resize(name_len);
        file.read(&info.name[0], name_len);
        
        // Read dimensions
        file.read(reinterpret_cast<char*>(&info.n_dims), 4);
        info.dims.resize(info.n_dims);
        file.read(reinterpret_cast<char*>(info.dims.data()), info.n_dims * 8);
        
        // Read type and offset
        file.read(reinterpret_cast<char*>(&info.type), 4);
        file.read(reinterpret_cast<char*>(&info.offset), 8);
        
        if (info.type == GGML_TYPE_Q2_K) q2k_count++;
        if (info.type == GGML_TYPE_Q3_K) q3k_count++;
        
        tensors.push_back(info);
    }

    // Display summary
    std::cout << "Quantization Summary:" << std::endl;
    
    std::map<uint32_t, int> type_counts;
    for (const auto& t : tensors) {
        type_counts[t.type]++;
    }
    
    for (const auto& [type, count] : type_counts) {
        std::cout << "  " << type_name(type) << ": " << count << " tensors" << std::endl;
    }
    
    std::cout << std::endl;
    
    if (q2k_count > 0 || q3k_count > 0) {
        std::cout << "✅ FOUND Q2_K/Q3_K TENSORS!" << std::endl;
        std::cout << "   Q2_K tensors: " << q2k_count << std::endl;
        std::cout << "   Q3_K tensors: " << q3k_count << std::endl;
        std::cout << std::endl;
        
        // Show first few Q2_K/Q3_K tensors
        std::cout << "Sample Q2_K/Q3_K tensors:" << std::endl;
        int shown = 0;
        for (const auto& t : tensors) {
            if ((t.type == GGML_TYPE_Q2_K || t.type == GGML_TYPE_Q3_K) && shown < 5) {
                std::cout << "  [" << type_name(t.type) << "] " << t.name;
                std::cout << " (";
                for (size_t i = 0; i < t.dims.size(); i++) {
                    if (i > 0) std::cout << " x ";
                    std::cout << t.dims[i];
                }
                std::cout << ")" << std::endl;
                shown++;
            }
        }
    } else {
        std::cout << "⚠ No Q2_K or Q3_K tensors found in this model." << std::endl;
        std::cout << "   This model uses other quantization types." << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Total tensors analyzed: " << tensors.size() << std::endl;
    
    return (q2k_count > 0 || q3k_count > 0) ? 0 : 1;
}
