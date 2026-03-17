// =============================================================================
// RawrXD Hardened Streaming GGUF Loader v2.0
// Production-grade ParseMetadata with corruption detection
// Eliminates bad_alloc vectors from tokenizer metadata bombs
// =============================================================================
#pragma once

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace rawrxd::gguf {

// GGUF Type Enumeration
enum class GGUFType : uint32_t {
    UINT8 = 0, INT8, UINT16, INT16, UINT32, INT32, FLOAT32,
    BOOL, STRING, ARRAY, UINT64, INT64, FLOAT64
};

// =============================================================================
// Memory Pressure Guard - Prevents loading if swap would be triggered
// =============================================================================
class MemoryGuard {
public:
    static bool CanAllocate(size_t requested_bytes) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(memInfo);
        GlobalMemoryStatusEx(&memInfo);

        // Require 1.5x requested as headroom (for fragmentation/tensor overhead)
        size_t required = requested_bytes + (requested_bytes / 2);
        
        if (memInfo.ullAvailPhys < required) {
            fprintf(stderr, 
                "[MemoryGuard] Blocked allocation: Need %zu MB, Have %zu MB\n",
                required / (1024*1024), 
                static_cast<size_t>(memInfo.ullAvailPhys / (1024*1024)));
            return false;
        }
        return true;
    }

    static void ReserveEmergencyPool() {
        // Hold 512MB hostage for emergency tensor operations
        if (!emergency_pool_) {
            emergency_pool_ = VirtualAlloc(nullptr, 512ULL * 1024 * 1024, 
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }
    }

    static void ReleaseEmergencyPool() {
        if (emergency_pool_) {
            VirtualFree(emergency_pool_, 0, MEM_RELEASE);
            emergency_pool_ = nullptr;
        }
    }

    static size_t GetAvailableMemoryMB() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(memInfo);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<size_t>(memInfo.ullAvailPhys / (1024 * 1024));
    }

private:
    inline static void* emergency_pool_ = nullptr;
};

// RAII wrapper for GGUF loading sessions
class GGUFMemorySession {
public:
    GGUFMemorySession() { MemoryGuard::ReserveEmergencyPool(); }
    ~GGUFMemorySession() { MemoryGuard::ReleaseEmergencyPool(); }
    
    bool CheckTensorLoad(size_t tensor_size) const {
        return MemoryGuard::CanAllocate(tensor_size);
    }
    
    // Non-copyable
    GGUFMemorySession(const GGUFMemorySession&) = delete;
    GGUFMemorySession& operator=(const GGUFMemorySession&) = delete;
};

// =============================================================================
// Hardened Streaming GGUF Loader
// =============================================================================
class HardenedGGUFLoader {
public:
    struct Header {
        char magic[4];
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_kv_count;
    };

    struct LoadStats {
        uint64_t pairs_parsed = 0;
        uint64_t pairs_skipped = 0;
        uint64_t bytes_skipped = 0;
        uint64_t corrupted_detected = 0;
        bool memory_pressure_triggered = false;
    };

private:
    std::ifstream file_;
    std::string filepath_;
    Header header_{};
    std::unordered_map<std::string, std::string> metadata_;
    LoadStats stats_{};
    bool verbose_ = false;

    // Skip list: memory bombs in current model generation
    static inline const std::unordered_set<std::string> kSkipKeys = {
        "tokenizer.ggml.tokens",      // 50MB+ vocab arrays
        "tokenizer.ggml.merges",      // BPE merge tables (30MB+)
        "tokenizer.ggml.scores",      // Float arrays (unused by inference)
        "tokenizer.ggml.token_type",  // Int arrays (unused)
        "tokenizer.chat_template"     // Corrupted/Oversized templates (>10MB)
    };

    // Safety limits
    static constexpr uint64_t MAX_STRING = 100 * 1024 * 1024;     // 100MB hard cap
    static constexpr uint64_t MAX_SAFE_STRING = 10 * 1024 * 1024; // 10MB auto-skip
    static constexpr uint64_t MAX_KEY_LENGTH = 65536;             // 64KB max key
    static constexpr uint64_t MAX_ARRAY_ELEMENTS = 100000000ULL;  // 100M elements
    static constexpr uint64_t VERIFY_THRESHOLD = 1099511627776ULL; // 1TB sanity

public:
    explicit HardenedGGUFLoader(bool verbose = false) : verbose_(verbose) {}
    
    ~HardenedGGUFLoader() {
        if (file_.is_open()) file_.close();
    }

    bool Open(const std::string& filepath) {
        filepath_ = filepath;
        file_.open(filepath, std::ios::binary);
        if (!file_.is_open()) {
            fprintf(stderr, "[GGUF] Cannot open: %s\n", filepath.c_str());
            return false;
        }
        return true;
    }

    bool ParseHeader() {
        if (!file_.is_open()) return false;

        // Read header
        file_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        
        // Validate magic
        if (memcmp(header_.magic, "GGUF", 4) != 0) {
            fprintf(stderr, "[GGUF] Invalid magic: expected 'GGUF'\n");
            return false;
        }

        // Validate version (2, 3, 4 supported)
        if (header_.version < 2 || header_.version > 4) {
            fprintf(stderr, "[GGUF] Unsupported version: %u\n", header_.version);
            return false;
        }

        // Sanity check counts
        if (header_.tensor_count > 1000000) {
            fprintf(stderr, "[GGUF] Suspicious tensor count: %llu\n", 
                (unsigned long long)header_.tensor_count);
            return false;
        }

        if (header_.metadata_kv_count > 10000000) {
            fprintf(stderr, "[GGUF] Suspicious metadata count: %llu\n",
                (unsigned long long)header_.metadata_kv_count);
            return false;
        }

        return true;
    }

    // =========================================================================
    // HARDENED ParseMetadata - THE CORE FIX FOR BAD_ALLOC
    // =========================================================================
    bool ParseMetadata() {
        if (!file_.is_open()) return false;

        // Memory pressure guard: require 2GB headroom for tensor mapping
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(memInfo);
        GlobalMemoryStatusEx(&memInfo);
        const DWORDLONG availableMB = memInfo.ullAvailPhys / (1024 * 1024);
        
        if (availableMB < 2048) {
            fprintf(stderr, "[GGUF] FATAL: Insufficient physical memory (%llu MB < 2048 MB)\n", 
                availableMB);
            stats_.memory_pressure_triggered = true;
            return false;
        }

        try {
            for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
                // =============================================================
                // Key reading with bounds check
                // =============================================================
                uint64_t key_len;
                if (!ReadValue(key_len) || key_len > MAX_KEY_LENGTH) {
                    fprintf(stderr, "[GGUF] Corruption detected: key length %llu at entry %llu\n", 
                        (unsigned long long)key_len, (unsigned long long)i);
                    stats_.corrupted_detected++;
                    return false;
                }

                std::string key;
                if (key_len > 0) {
                    key.resize(key_len);
                    if (!file_.read(key.data(), key_len)) {
                        fprintf(stderr, "[GGUF] EOF during key read at entry %llu\n", 
                            (unsigned long long)i);
                        return false;
                    }
                }

                // =============================================================
                // Type validation
                // =============================================================
                uint32_t type_val;
                if (!ReadValue(type_val) || type_val > 15) {
                    fprintf(stderr, "[GGUF] Invalid type %u for key '%s'\n", 
                        type_val, key.c_str());
                    stats_.corrupted_detected++;
                    return false;
                }
                auto type = static_cast<GGUFType>(type_val);

                // =============================================================
                // Skip decision: dangerous keys get bypassed
                // =============================================================
                const bool should_skip = kSkipKeys.find(key) != kSkipKeys.end();

                if (should_skip && verbose_) {
                    fprintf(stderr, "[GGUF] Lazy-bypassing known memory-heavy key: %s\n", 
                        key.c_str());
                }

                // =============================================================
                // Type-specific handling
                // =============================================================
                switch (type) {
                    case GGUFType::UINT8:   HandleScalar<uint8_t>(key, should_skip); break;
                    case GGUFType::INT8:    HandleScalar<int8_t>(key, should_skip); break;
                    case GGUFType::UINT16:  HandleScalar<uint16_t>(key, should_skip); break;
                    case GGUFType::INT16:   HandleScalar<int16_t>(key, should_skip); break;
                    case GGUFType::UINT32:  HandleScalar<uint32_t>(key, should_skip); break;
                    case GGUFType::INT32:   HandleScalar<int32_t>(key, should_skip); break;
                    case GGUFType::FLOAT32: HandleScalar<float>(key, should_skip); break;
                    case GGUFType::UINT64:  HandleScalar<uint64_t>(key, should_skip); break;
                    case GGUFType::INT64:   HandleScalar<int64_t>(key, should_skip); break;
                    case GGUFType::FLOAT64: HandleScalar<double>(key, should_skip); break;
                    case GGUFType::BOOL:    HandleScalar<bool>(key, should_skip); break;
                    
                    case GGUFType::STRING: {
                        if (!HandleString(key, should_skip)) return false;
                        break;
                    }

                    case GGUFType::ARRAY: {
                        if (!HandleArray(key, should_skip)) return false;
                        break;
                    }

                    default:
                        fprintf(stderr, "[GGUF] Unknown type %u, aborting\n", type_val);
                        return false;
                }

                stats_.pairs_parsed++;
            }
            return true;

        } catch (const std::bad_alloc& e) {
            fprintf(stderr, "[GGUF] CRITICAL: bad_alloc during metadata parse: %s\n", e.what());
            stats_.memory_pressure_triggered = true;
            return false;
        } catch (const std::exception& e) {
            fprintf(stderr, "[GGUF] ParseMetadata exception: %s\n", e.what());
            return false;
        }
    }

    // Accessors
    const Header& GetHeader() const { return header_; }
    const std::unordered_map<std::string, std::string>& GetMetadata() const { return metadata_; }
    const LoadStats& GetStats() const { return stats_; }
    
    std::string GetMetadataValue(const std::string& key, const std::string& default_val = "") const {
        auto it = metadata_.find(key);
        return (it != metadata_.end()) ? it->second : default_val;
    }

private:
    template<typename T>
    bool ReadValue(T& val) {
        return static_cast<bool>(file_.read(reinterpret_cast<char*>(&val), sizeof(T)));
    }

    template<typename T>
    void HandleScalar(const std::string& key, bool skip) {
        T val;
        if (!ReadValue(val)) {
            throw std::runtime_error("Scalar read failed for: " + key);
        }
        
        if (!skip) {
            // Store in metadata map as string
            if constexpr (std::is_same_v<T, bool>) {
                metadata_[key] = val ? "true" : "false";
            } else {
                metadata_[key] = std::to_string(val);
            }
        }
    }

    bool HandleString(const std::string& key, bool should_skip) {
        uint64_t str_len;
        if (!ReadValue(str_len)) {
            fprintf(stderr, "[GGUF] Failed to read string length for '%s'\n", key.c_str());
            return false;
        }

        // =================================================================
        // CORRUPTION DETECTION: strings > 100MB are likely damaged headers
        // =================================================================
        if (str_len > MAX_STRING) {
            fprintf(stderr, "[GGUF] CORRUPTION: String '%s' claims %llu MB, marking corrupt\n",
                key.c_str(), (unsigned long long)(str_len / (1024*1024)));
            
            // Emergency seek to recover file position
            file_.seekg(static_cast<std::streamoff>(str_len), std::ios::cur);
            metadata_[key] = "[CORRUPTED:" + std::to_string(str_len) + "]";
            stats_.corrupted_detected++;
            stats_.bytes_skipped += str_len;
            return true; // Continue parsing (non-fatal)
        }

        // =================================================================
        // AUTO-SKIP: strings > 10MB (or dangerous keys) - NO ALLOCATION
        // =================================================================
        if (should_skip || str_len > MAX_SAFE_STRING) {
            file_.seekg(static_cast<std::streamoff>(str_len), std::ios::cur);
            metadata_[key] = "[skipped:" + std::to_string(str_len) + "]";
            stats_.pairs_skipped++;
            stats_.bytes_skipped += str_len;
            
            if (verbose_) {
                fprintf(stderr, "[GGUF] Skipped %llu byte string '%s'\n",
                    (unsigned long long)str_len, key.c_str());
            }
            return true;
        }

        // =================================================================
        // SAFE READ: Small strings with bad_alloc protection
        // =================================================================
        std::string val;
        try {
            val.resize(str_len);
        } catch (const std::bad_alloc&) {
            fprintf(stderr, "[GGUF] bad_alloc on %llu bytes for '%s', skipping\n",
                (unsigned long long)str_len, key.c_str());
            file_.seekg(static_cast<std::streamoff>(str_len), std::ios::cur);
            metadata_[key] = "[alloc_failed:" + std::to_string(str_len) + "]";
            stats_.pairs_skipped++;
            return true;
        }
        
        if (!file_.read(val.data(), str_len)) {
            fprintf(stderr, "[GGUF] Short read on string '%s'\n", key.c_str());
            return false;
        }
        
        metadata_[key] = std::move(val);
        return true;
    }

    bool HandleArray(const std::string& key, bool should_skip) {
        uint32_t arr_type;
        uint64_t arr_len;
        
        if (!ReadValue(arr_type) || !ReadValue(arr_len)) {
            fprintf(stderr, "[GGUF] Failed to read array header for '%s'\n", key.c_str());
            return false;
        }

        // =================================================================
        // ARRAY CORRUPTION CHECK
        // =================================================================
        if (arr_len > MAX_ARRAY_ELEMENTS) {
            fprintf(stderr, "[GGUF] CORRUPTION: Array '%s' claims %llu elements\n",
                key.c_str(), (unsigned long long)arr_len);
            stats_.corrupted_detected++;
            return false; // Fatal - can't safely skip unknown-size array
        }

        // Calculate skip size based on element type
        size_t elem_size = GetGGUFTypeSize(static_cast<GGUFType>(arr_type));
        
        // =================================================================
        // STRING ARRAYS: Variable length - iterate and skip each
        // =================================================================
        if (arr_type == static_cast<uint32_t>(GGUFType::STRING)) {
            for (uint64_t j = 0; j < arr_len; ++j) {
                uint64_t elem_len;
                if (!ReadValue(elem_len)) {
                    fprintf(stderr, "[GGUF] Failed to read string array element %llu\n", 
                        (unsigned long long)j);
                    return false;
                }
                
                // Bounds check each element
                if (elem_len > MAX_STRING) {
                    fprintf(stderr, "[GGUF] Corrupted string array element %llu (len=%llu)\n",
                        (unsigned long long)j, (unsigned long long)elem_len);
                    stats_.corrupted_detected++;
                    return false;
                }
                
                file_.seekg(static_cast<std::streamoff>(elem_len), std::ios::cur);
                stats_.bytes_skipped += elem_len;
            }
        } else {
            // =================================================================
            // FIXED-SIZE ARRAYS: Calculate and seek
            // =================================================================
            uint64_t skip_bytes = arr_len * elem_size;
            
            // Overflow check
            if (elem_size > 0 && arr_len > UINT64_MAX / elem_size) {
                fprintf(stderr, "[GGUF] Array size overflow for '%s'\n", key.c_str());
                stats_.corrupted_detected++;
                return false;
            }
            
            file_.seekg(static_cast<std::streamoff>(skip_bytes), std::ios::cur);
            stats_.bytes_skipped += skip_bytes;
        }
        
        metadata_[key] = "[array_skipped:" + std::to_string(arr_len) + "]";
        stats_.pairs_skipped++;
        return true;
    }

    static size_t GetGGUFTypeSize(GGUFType type) {
        switch (type) {
            case GGUFType::UINT8: case GGUFType::INT8: case GGUFType::BOOL: return 1;
            case GGUFType::UINT16: case GGUFType::INT16: return 2;
            case GGUFType::UINT32: case GGUFType::INT32: case GGUFType::FLOAT32: return 4;
            case GGUFType::UINT64: case GGUFType::INT64: case GGUFType::FLOAT64: return 8;
            default: return 0; // STRING, ARRAY are variable
        }
    }
};

// =============================================================================
// Convenience Function: Quick Parse with Stats
// =============================================================================
inline bool QuickParseGGUF(const std::string& filepath, 
                           std::unordered_map<std::string, std::string>& metadata_out,
                           bool verbose = false) {
    GGUFMemorySession session; // RAII memory protection
    
    HardenedGGUFLoader loader(verbose);
    if (!loader.Open(filepath)) return false;
    if (!loader.ParseHeader()) return false;
    if (!loader.ParseMetadata()) return false;
    
    metadata_out = loader.GetMetadata();
    
    const auto& stats = loader.GetStats();
    if (verbose) {
        fprintf(stderr, "[GGUF] Parse complete: %llu pairs, %llu skipped, %llu bytes bypassed\n",
            (unsigned long long)stats.pairs_parsed,
            (unsigned long long)stats.pairs_skipped,
            (unsigned long long)stats.bytes_skipped);
    }
    
    return true;
}

} // namespace rawrxd::gguf
