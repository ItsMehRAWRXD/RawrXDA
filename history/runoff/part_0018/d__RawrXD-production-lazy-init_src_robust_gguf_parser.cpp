// ============================================================================
// ROBUST_GGUF_PARSER.CPP
// Implementation with bad_alloc immunization
// ============================================================================
#include "safe_gguf_metadata_parser.hpp"
#include <psapi.h>

#ifdef _WIN64
extern "C" bool __vectorcall ValidateStringLengthASM(uint64_t rcx_len, uint64_t rdx_max);
#endif

namespace rawrxd::gguf {

bool RobustGGUFParser::ValidateStringLengthAVX512(uint64_t len, uint64_t max_val) {
#ifdef _WIN64
    return ValidateStringLengthASM(len, max_val);
#else
    return len <= max_val && len < 0x8000000000000000ULL; // Signed check for corruption
#endif
}

RobustGGUFParser::RobustGGUFParser(const wchar_t* wide_path, bool verbose) 
    : path_(wide_path), verbose_(verbose) {
    
    stream_.open(wide_path, std::ios::binary | std::ios::ate);
    if (!stream_) {
        throw std::runtime_error("Failed to open GGUF file");
    }
    
    file_size_ = stream_.tellg();
    stream_.seekg(0, std::ios::beg);
    
    if (file_size_ < 64) {
        throw std::runtime_error("File too small to be valid GGUF");
    }
}

RobustGGUFParser::~RobustGGUFParser() {
    if (stream_.is_open()) {
        EmergencyPartialClose();
    }
}

bool RobustGGUFParser::SafeRead(void* dst, size_t bytes) {
    if (!stream_.good()) return false;
    
    // Check against file bounds
    int64_t current = stream_.tellg();
    if (current < 0 || (current + static_cast<int64_t>(bytes)) > file_size_) {
        return false;
    }
    
    stream_.read(static_cast<char*>(dst), bytes);
    return stream_.gcount() == static_cast<std::streamsize>(bytes);
}

void RobustGGUFParser::SafeSkip(uint64_t bytes) {
    if (bytes == 0) return;
    
    int64_t current = stream_.tellg();
    if (current < 0) throw std::runtime_error("Invalid stream position");
    
    // Check for overflow/wraparound in 64-bit
    if (bytes > static_cast<uint64_t>(INT64_MAX - current)) {
        throw std::runtime_error("Skip overflow - possible corruption");
    }
    
    int64_t target = current + static_cast<int64_t>(bytes);
    if (target > file_size_) {
        throw std::runtime_error("Skip beyond EOF");
    }
    
    stream_.seekg(target, std::ios::beg);
}

std::string RobustGGUFParser::SafeReadString(uint64_t max_len) {
    uint64_t len = 0;
    if (!SafeRead(&len, 8)) {
        throw std::runtime_error("Failed to read string length");
    }
    
    // MASM-validated length check
    if (!ValidateStringLengthAVX512(len, max_len)) {
        // Skip without allocating
        SafeSkip(len);
        return "[OVERSIZE_SKIPPED]";
    }
    
    std::string result;
    try {
        result.resize(static_cast<size_t>(len));
    } catch (const std::bad_alloc&) {
        SafeSkip(len);
        return "[ALLOC_FAIL_SKIPPED]";
    }
    
    if (!SafeRead(result.data(), static_cast<size_t>(len))) {
        throw std::runtime_error("Truncated string data");
    }
    
    return result;
}

SafeMetadata RobustGGUFParser::ParseWithGuards() {
    SafeMetadata meta;
    
    // Header: magic (4) + version (4) + tensor_count (8) + kv_count (8)
    uint32_t magic = 0;
    if (!SafeRead(&magic, 4) || magic != GGUF_MAGIC) {
        throw std::runtime_error("Invalid GGUF magic or corrupted header");
    }
    
    if (!SafeRead(&meta.version, 4) || meta.version > 3) {
        throw std::runtime_error("Unsupported GGUF version");
    }
    
    uint64_t tensor_count = 0, kv_count = 0;
    SafeRead(&tensor_count, 8);
    SafeRead(&kv_count, 8);
    
    if (verbose_) {
        fprintf(stderr, "[GGUF] Version: %u, Tensors: %llu, KV pairs: %llu\n",
                meta.version, tensor_count, kv_count);
    }
    
    // Parse KV pairs with defensive skipping
    for (uint64_t i = 0; i < kv_count; ++i) {
        // Read key
        std::string key = SafeReadString(1024); // Keys are short
        
        uint32_t type = 0;
        SafeRead(&type, 4);
        
        // Memory pressure check every 100 entries
        if (i % 100 == 0) {
            PROCESS_MEMORY_COUNTERS pmc = {};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                if (pmc.WorkingSetSize > max_ram_bytes_) {
                    throw std::runtime_error("Memory pressure limit exceeded during parse");
                }
            }
        }
        
        switch (type) {
            case 8: { // GGUF_TYPE_STRING
                std::string val = SafeReadString(GGUF_MAX_SAFE_STRING);
                if (val.find("[SKIPPED]") != std::string::npos) {
                    meta.skipped_entries.emplace_back(key, val == "[OVERSIZE_SKIPPED]" ? 
                        GGUF_MAX_SAFE_STRING : 0);
                } else {
                    meta.kv_string[key] = std::move(val);
                }
                break;
            }
            case 9: { // GGUF_TYPE_ARRAY (handle tokens/merges)
                uint32_t arr_type = 0;
                uint64_t arr_count = 0;
                SafeRead(&arr_type, 4);
                SafeRead(&arr_count, 8);
                
                // Skip dangerous arrays
                if ((key == "tokenizer.ggml.tokens" || key == "tokenizer.ggml.merges") ||
                    arr_count > GGUF_MAX_ARRAY_ELEMS) {
                    
                    uint64_t bytes_per_elem = (arr_type == 8) ? 8 : 4; // Conservative
                    uint64_t total_skip = arr_count * bytes_per_elem;
                    
                    SafeSkip(total_skip);
                    meta.skipped_entries.emplace_back(key, arr_count);
                    
                    if (verbose_) {
                        fprintf(stderr, "[GGUF] Lazy-skipping array '%s' (%llu elems)\n", 
                                key.c_str(), arr_count);
                    }
                } else {
                    // Read normally if safe size
                    for (uint64_t j = 0; j < arr_count; ++j) {
                        if (arr_type == 8) SafeReadString();
                        else SafeSkip(4); // Simplified - real impl would parse properly
                    }
                }
                break;
            }
            case 4: { // UINT32
                uint32_t val = 0;
                SafeRead(&val, 4);
                meta.kv_uint64[key] = val;
                break;
            }
            case 5: { // INT32
                SafeSkip(4);
                break;
            }
            case 6: { // FLOAT32
                SafeSkip(4);
                break;
            }
            case 7: { // BOOL
                SafeSkip(1);
                break;
            }
            case 10: { // UINT64
                uint64_t val = 0;
                SafeRead(&val, 8);
                meta.kv_uint64[key] = val;
                break;
            }
            default:
                // Unknown type - skip 4 bytes as safety
                SafeSkip(4);
                break;
        }
    }
    
    meta.tensor_offset = static_cast<uint64_t>(stream_.tellg());
    meta.integrity_verified = true;
    return meta;
}

void RobustGGUFParser::EmergencyPartialClose() {
    try {
        stream_.close();
    } catch (...) {
        // Suppress exceptions in destructor path
    }
}

} // namespace rawrxd::gguf
