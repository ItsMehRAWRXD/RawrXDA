#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <cstring>

namespace RawrXD::Tools {

class SafeGGUFParser {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t pos = 0;
    
    // Configuration
    static constexpr size_t MAX_STRING_ALLOC = 50 * 1024 * 1024; // 50MB hard cap
    static constexpr size_t MAX_KEY_LENGTH = 4096;
    static constexpr size_t MAX_ARRAY_ELEMENTS = 100000000;
    
public:
    struct ParseCallbacks {
        std::function<bool(const std::string& key, uint32_t type, uint64_t size)> onMetadata;
        std::function<void(const std::string& name, uint32_t type, const std::vector<uint64_t>& dims, uint64_t offset)> onTensor;
    };

    explicit SafeGGUFParser(const uint8_t* data_ptr, size_t data_size) 
        : data(data_ptr), size(data_size), pos(0) {
        if (!data || size < 32) {
            throw std::runtime_error("File too small for GGUF");
        }
    }

    void Parse(ParseCallbacks& cb, bool skip_large_strings = true) {
        if (size < 32) throw std::runtime_error("File too small for GGUF");
        
        // Validate Magic
        if (memcmp(data, "GGUF", 4) != 0) 
            throw std::runtime_error("Invalid GGUF magic bytes");
        
        pos = 4;
        uint32_t version = ReadU32();
        if (version < 2) throw std::runtime_error("Only GGUF v2+ supported for large models");
        
        uint64_t tensor_count = ReadU64();
        uint64_t metadata_count = ReadU64();

        // Parse Metadata
        for (uint64_t i = 0; i < metadata_count && pos < size; ++i) {
            auto key = ReadString();
            if (key.empty() && pos >= size) break; // Corruption guard
            
            uint32_t val_type = ReadU32();
            
            bool shouldLoad = true;
            uint64_t value_size = 0;
            
            if (val_type == 8) { // String
                uint64_t len = ReadU64();
                if (len > MAX_STRING_ALLOC && skip_large_strings) {
                    // Skip without allocating
                    pos += static_cast<size_t>(len);
                    shouldLoad = false;
                    value_size = len;
                } else if (len > size - pos) {
                    throw std::runtime_error("String length exceeds file bounds (corruption?)");
                } else {
                    // Safe to allocate
                    std::string value(reinterpret_cast<const char*>(data + pos), static_cast<size_t>(len));
                    pos += static_cast<size_t>(len);
                    shouldLoad = cb.onMetadata ? cb.onMetadata(key, val_type, len) : true;
                }
            } 
            else if (val_type == 9) { // Array
                uint32_t arr_type = ReadU32();
                uint64_t arr_len = ReadU64();
                uint64_t elem_sz = GetTypeSize(arr_type);
                
                if (arr_type == 8) { // Array of strings (tokens/merges)
                    // Calculate total size without loading
                    size_t array_start = pos;
                    uint64_t total_str_bytes = 0;
                    
                    for (uint64_t j = 0; j < arr_len && pos < size; ++j) {
                        uint64_t s_len = ReadU64();
                        total_str_bytes += s_len + 8;
                        pos += static_cast<size_t>(s_len);
                    }
                    
                    if (total_str_bytes > MAX_STRING_ALLOC && skip_large_strings) {
                        shouldLoad = false;
                        value_size = total_str_bytes;
                    } else {
                        pos = array_start; // Reset to parse properly
                        shouldLoad = cb.onMetadata ? cb.onMetadata(key, val_type, arr_len) : true;
                        if (!shouldLoad) {
                            // Skip the array
                            for (uint64_t j = 0; j < arr_len; ++j) {
                                uint64_t s_len = ReadU64();
                                pos += static_cast<size_t>(s_len);
                            }
                        }
                    }
                } else {
                    // Fixed-size array - easy skip
                    uint64_t skip_bytes = arr_len * elem_sz;
                    if (skip_bytes > size - pos) 
                        throw std::runtime_error("Array bounds exceed file size");
                    pos += static_cast<size_t>(skip_bytes);
                    shouldLoad = cb.onMetadata ? cb.onMetadata(key, val_type, arr_len) : false;
                }
            } else {
                // Scalar
                uint64_t sz = GetTypeSize(val_type);
                if (sz == 0) throw std::runtime_error("Unknown scalar type");
                pos += static_cast<size_t>(sz);
                shouldLoad = cb.onMetadata ? cb.onMetadata(key, val_type, sz) : true;
            }
            
            if (!shouldLoad) {
                fprintf(stderr, "[GGUF-SAFE] Skipped '%s' (type=%u, size=%llu)\n", 
                        key.c_str(), val_type, (unsigned long long)value_size);
            }
        }

        // Parse Tensor Info (lightweight)
        for (uint64_t i = 0; i < tensor_count && pos < size; ++i) {
            auto name = ReadString();
            uint32_t n_dims = ReadU32();
            std::vector<uint64_t> dims;
            for (uint32_t d = 0; d < n_dims; ++d) dims.push_back(ReadU64());
            
            uint32_t type = ReadU32();
            uint64_t offset = ReadU64();
            
            if (cb.onTensor) cb.onTensor(name, type, dims, offset);
        }
    }

private:
    uint32_t ReadU32() {
        if (pos + 4 > size) throw std::runtime_error("Unexpected EOF reading u32");
        uint32_t v = *reinterpret_cast<const uint32_t*>(data + pos);
        pos += 4;
        return v;
    }
    
    uint64_t ReadU64() {
        if (pos + 8 > size) throw std::runtime_error("Unexpected EOF reading u64");
        uint64_t v = *reinterpret_cast<const uint64_t*>(data + pos);
        pos += 8;
        return v;
    }
    
    uint64_t ReadULEB128() {
        uint64_t result = 0;
        int shift = 0;
        while (pos < size) {
            uint8_t byte = data[pos++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) return result;
            shift += 7;
            if (shift >= 64) throw std::runtime_error("ULEB128 overflow");
        }
        throw std::runtime_error("Unexpected EOF in ULEB128");
    }
    
    std::string ReadString() {
        uint64_t len = ReadULEB128();
        if (len > MAX_KEY_LENGTH) {
            // Sanity check: metadata keys shouldn't be huge
            throw std::runtime_error("Suspicious string length in metadata key");
        }
        if (pos + len > size) throw std::runtime_error("String extends past EOF");
        
        std::string s(reinterpret_cast<const char*>(data + pos), static_cast<size_t>(len));
        pos += static_cast<size_t>(len);
        return s;
    }
    
    static uint64_t GetTypeSize(uint32_t type) {
        constexpr uint64_t sizes[] = {1,1,2,2,4,4,4,1,0,0,8,8,8};
        return (type < 13) ? sizes[type] : 0;
    }
};

} // namespace RawrXD::Tools
