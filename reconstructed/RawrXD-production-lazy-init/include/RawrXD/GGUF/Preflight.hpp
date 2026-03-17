#pragma once
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace RawrXD::Tools {

struct GGUFMemoryProjection {
    uint64_t total_file_size = 0;
    uint64_t metadata_heap_risk = 0;      // Strings/arrays that could cause bad_alloc
    uint64_t tensor_data_size = 0;        // Raw tensor blob size
    uint64_t header_overhead = 0;         // GGUF header + tensor info
    uint32_t tensor_count = 0;
    uint32_t metadata_kv_count = 0;
    bool contains_800b_model = false;
    std::vector<std::string> high_risk_keys; // Keys requiring skip logic
};

class GGUFInspector {
public:
    static GGUFMemoryProjection Analyze(const char* filepath, bool verbose = false) {
        GGUFMemoryProjection proj;
        FILE* f = nullptr;
        
#ifdef _WIN32
        fopen_s(&f, filepath, "rb");
#else
        f = fopen(filepath, "rb");
#endif
        if (!f) throw std::runtime_error("Failed to open GGUF for inspection");

        // Get file size
        _fseeki64(f, 0, SEEK_END);
        proj.total_file_size = _ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);

        // Read header
        char magic[4];
        if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "GGUF", 4) != 0) {
            fclose(f);
            throw std::runtime_error("Invalid GGUF magic");
        }

        uint32_t version;
        fread(&version, 4, 1, f);
        
        // Reverse-engineered: GGUF v2/v3 use 64-bit counts
        uint64_t tensor_count = 0, metadata_kv_count = 0;
        if (version >= 2) {
            fread(&tensor_count, 8, 1, f);
            fread(&metadata_kv_count, 8, 1, f);
        } else {
            uint32_t tc, mkv;
            fread(&tc, 4, 1, f);
            fread(&mkv, 4, 1, f);
            tensor_count = tc;
            metadata_kv_count = mkv;
        }

        proj.tensor_count = static_cast<uint32_t>(tensor_count);
        proj.metadata_kv_count = static_cast<uint32_t>(metadata_kv_count);

        // Scan metadata keys without loading values
        for (uint64_t i = 0; i < metadata_kv_count; ++i) {
            ScanMetadataEntry(f, proj, verbose);
        }

        // Sum tensor sizes
        for (uint64_t i = 0; i < tensor_count; ++i) {
            SkipTensorInfo(f, proj);
        }

        // Calculate data offset vs file size to detect corruption
        int64_t current_pos = _ftelli64(f);
        proj.tensor_data_size = proj.total_file_size - current_pos;
        proj.header_overhead = current_pos;

        // Heuristic: >100GB tensor data suggests 800B+ class model
        if (proj.tensor_data_size > (100ULL * 1024 * 1024 * 1024)) {
            proj.contains_800b_model = true;
        }

        if (verbose) {
            fprintf(stderr, "[GGUF-INSPECT] File: %s\n", filepath);
            fprintf(stderr, "  Version: %u | Tensors: %llu | Metadata: %llu\n", 
                    version, (unsigned long long)tensor_count, (unsigned long long)metadata_kv_count);
            fprintf(stderr, "  High-risk allocations: %llu bytes\n", (unsigned long long)proj.metadata_heap_risk);
            fprintf(stderr, "  Tensor payload: %.2f GB\n", proj.tensor_data_size / (1024.0 * 1024 * 1024));
            fprintf(stderr, "  800B Class: %s\n", proj.contains_800b_model ? "YES" : "NO");
        }

        fclose(f);
        return proj;
    }

private:
    static uint64_t ReadULEB128(FILE* f) {
        uint64_t result = 0;
        int shift = 0;
        uint8_t byte;
        do {
            if (fread(&byte, 1, 1, f) != 1) break;
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            shift += 7;
        } while (byte & 0x80);
        return result;
    }

    static void ScanMetadataEntry(FILE* f, GGUFMemoryProjection& proj, bool verbose) {
        // Skip key string (read length, seek past)
        uint64_t key_len = ReadULEB128(f);
        _fseeki64(f, static_cast<int64_t>(key_len), SEEK_CUR);
        
        // Read value type
        uint32_t val_type;
        fread(&val_type, 4, 1, f);

        // Calculate heap risk based on type
        uint64_t risk_size = 0;
        
        if (val_type == 8) { // GGUF_TYPE_STRING
            uint64_t str_len;
            fread(&str_len, 8, 1, f);
            risk_size = str_len;
            
            // Check if this would cause bad_alloc (>50MB string is suspicious)
            if (str_len > (50 * 1024 * 1024)) {
                char key_name[256] = {0};
                proj.high_risk_keys.push_back("[Oversized string: " + std::to_string(str_len / (1024*1024)) + "MB]");
                if (verbose) fprintf(stderr, "[RISK] Oversized string: %llu MB\n", (unsigned long long)(str_len / (1024*1024)));
            }
            
            _fseeki64(f, static_cast<int64_t>(str_len), SEEK_CUR); // Skip data
        }
        else if (val_type == 9) { // GGUF_TYPE_ARRAY
            uint32_t arr_type;
            uint64_t arr_len;
            fread(&arr_type, 4, 1, f);
            fread(&arr_len, 8, 1, f);
            
            // Calculate element size
            uint64_t elem_size = GetGGUFTypeSize(arr_type);
            risk_size = elem_size * arr_len;
            
            if (arr_type == 8) { // Array of strings (merges/tokens)
                // Each element has its own length prefix
                for (uint64_t i = 0; i < std::min(arr_len, 1000ULL); ++i) {
                    uint64_t s_len;
                    fread(&s_len, 8, 1, f);
                    risk_size += s_len;
                    _fseeki64(f, static_cast<int64_t>(s_len), SEEK_CUR);
                    
                    if (risk_size > (100 * 1024 * 1024)) {
                        proj.high_risk_keys.push_back("[Array overflow: 100MB+]");
                        break;
                    }
                }
                // Skip rest if we bailed early
                if (arr_len > 1000) {
                    _fseeki64(f, static_cast<int64_t>((arr_len - 1000) * elem_size), SEEK_CUR);
                }
            } else {
                _fseeki64(f, static_cast<int64_t>(risk_size), SEEK_CUR);
            }
        } else {
            // Scalar - skip fixed size
            _fseeki64(f, static_cast<int64_t>(GetGGUFTypeSize(val_type)), SEEK_CUR);
        }

        proj.metadata_heap_risk += risk_size;
    }

    static void SkipTensorInfo(FILE* f, GGUFMemoryProjection& proj) {
        uint64_t name_len = ReadULEB128(f);
        _fseeki64(f, static_cast<int64_t>(name_len), SEEK_CUR); // Skip name
        
        uint32_t n_dims;
        fread(&n_dims, 4, 1, f);
        _fseeki64(f, static_cast<int64_t>(n_dims * 8), SEEK_CUR); // Skip dimensions
        
        uint32_t type;
        fread(&type, 4, 1, f);
        
        uint64_t offset;
        fread(&offset, 8, 1, f);
    }

    static uint64_t GetGGUFTypeSize(uint32_t type) {
        switch(type) {
            case 0: return 1;  // UINT8
            case 1: return 1;  // INT8
            case 2: return 2;  // UINT16
            case 3: return 2;  // INT16
            case 4: return 4;  // UINT32
            case 5: return 4;  // INT32
            case 6: return 4;  // FLOAT32
            case 7: return 1;  // BOOL
            case 8: return 0;  // STRING (variable)
            case 9: return 0;  // ARRAY (variable)
            case 10: return 8; // UINT64
            case 11: return 8; // INT64
            case 12: return 8; // FLOAT64
            default: return 1;
        }
    }
};

} // namespace RawrXD::Tools
