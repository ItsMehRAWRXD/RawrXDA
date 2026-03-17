// =============================================================================
// RAWRXD GGUF ROBUST TOOLS v2.0
// Reverse-engineered from llama.cpp + Ollama loader internals
// Zero STL exceptions on hot paths; 64-bit overflow hardened
// =============================================================================
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <windows.h>

namespace rawrxd::gguf_robust {

// -----------------------------------------------------------------------------
// 1. PRE-FLIGHT CORRUPTION DETECTOR
// Validates GGUF magic, version, and tensor count before any allocation
// -----------------------------------------------------------------------------
struct [[nodiscard]] CorruptionScan {
    uint64_t tensor_count = 0;
    uint64_t metadata_kv_count = 0;
    uint64_t file_size = 0;
    uint32_t gguf_version = 0;
    bool     is_valid = false;
    char     error_msg[256] = {0};

    // Fast header validation without mmap overhead
    static CorruptionScan ScanFile(const char* filepath) noexcept {
        CorruptionScan result{};
        FILE* f = nullptr;
        
        if (fopen_s(&f, filepath, "rb") != 0 || !f) {
            snprintf(result.error_msg, sizeof(result.error_msg), "FILE_ACCESS_DENIED");
            return result;
        }

        // File size probe
        _fseeki64(f, 0, SEEK_END);
        result.file_size = _ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);

        // GGUF magic: 'GGUF' = 0x46554747 (little-endian)
        uint32_t magic = 0;
        if (fread(&magic, 4, 1, f) != 1 || magic != 0x46554747) {
            snprintf(result.error_msg, sizeof(result.error_msg), "INVALID_MAGIC");
            fclose(f);
            return result;
        }

        // Version check (support 2, 3, 4)
        if (fread(&result.gguf_version, 4, 1, f) != 1 || 
            (result.gguf_version < 2 || result.gguf_version > 4)) {
            snprintf(result.error_msg, sizeof(result.error_msg), "UNSUPPORTED_VERSION_%u", result.gguf_version);
            fclose(f);
            return result;
        }

        // Tensor count sanity (prevent OOM on bad header)
        if (fread(&result.tensor_count, 8, 1, f) != 1 || result.tensor_count > 1000000) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_TENSOR_COUNT_%llu", 
                     (unsigned long long)result.tensor_count);
            fclose(f);
            return result;
        }

        // Metadata count sanity
        if (fread(&result.metadata_kv_count, 8, 1, f) != 1 || result.metadata_kv_count > 10000000) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_METADATA_COUNT");
            fclose(f);
            return result;
        }

        result.is_valid = true;
        fclose(f);
        return result;
    }
};

// -----------------------------------------------------------------------------
// 2. ZERO-COPY STREAMING READER
// Kernel-bypass philosophy: never allocate on oversized/corrupted fields
// -----------------------------------------------------------------------------
class RobustGGUFStream {
    FILE* handle_ = nullptr;
    int64_t file_size_ = 0;
    int64_t safety_limit_bytes_ = 64LL * 1024 * 1024 * 1024; // 64GB max
    
public:
    struct SkipResult { bool ok; int64_t bytes_skipped; const char* error; };
    struct ReadResult { bool ok; size_t bytes_read; const char* error; };

    explicit RobustGGUFStream(const char* path) noexcept {
        fopen_s(&handle_, path, "rb");
        if (handle_) {
            _fseeki64(handle_, 0, SEEK_END);
            file_size_ = _ftelli64(handle_);
            _fseeki64(handle_, 0, SEEK_SET);
        }
    }
    
    ~RobustGGUFStream() { if (handle_) fclose(handle_); }
    bool IsOpen() const noexcept { return handle_ != nullptr; }
    int64_t Tell() const noexcept { return handle_ ? _ftelli64(handle_) : -1; }
    int64_t Size() const noexcept { return file_size_; }
    FILE* GetHandle() const noexcept { return handle_; }

    // Safe 64-bit seek with bounds checking
    [[nodiscard]] bool RobustSeek(int64_t offset, int origin) noexcept {
        if (!handle_) return false;
        
        int64_t target = (origin == SEEK_CUR) ? Tell() + offset : 
                        (origin == SEEK_END) ? file_size_ + offset : offset;
                        
        if (target < 0 || target > file_size_) return false; // Underflow/overflow guard
        
        return _fseeki64(handle_, offset, origin) == 0;
    }

    // SkipString: Read length (u64), validate against limits, seek past data
    // Zero heap allocations; handles corrupted length fields gracefully
    [[nodiscard]] SkipResult SkipString() noexcept {
        if (!handle_) return {false, 0, "NO_HANDLE"};
        
        uint64_t len = 0;
        if (fread(&len, 8, 1, handle_) != 1) 
            return {false, 0, "READ_LENGTH_FAILED"};
            
        // Corruption check: length exceeds remaining file or safety limit
        int64_t current = Tell();
        if (len > static_cast<uint64_t>(file_size_ - current) || 
            len > static_cast<uint64_t>(safety_limit_bytes_)) {
            return {false, 0, "LENGTH_SANITY_FAILED"};
        }
        
        if (!RobustSeek(static_cast<int64_t>(len), SEEK_CUR))
            return {false, 0, "SEEK_FAILED"};
            
        return {true, static_cast<int64_t>(len), nullptr};
    }

    // SkipArray: Read type+u64 count, calculate byte size, seek past
    [[nodiscard]] SkipResult SkipArray(uint64_t* out_count = nullptr) noexcept {
        if (!handle_) return {false, 0, "NO_HANDLE"};
        
        uint32_t elem_type = 0;
        uint64_t count = 0;
        
        if (fread(&elem_type, 4, 1, handle_) != 1 || fread(&count, 8, 1, handle_) != 1)
            return {false, 0, "READ_ARRAY_HEADER_FAILED"};
            
        if (out_count) *out_count = count;
        
        // Calculate total bytes safely (check overflow)
        uint64_t elem_size = GetTypeSize(elem_type);
        uint64_t total_bytes = 0;
        
        // Check for multiplication overflow
        if (elem_size > 0 && count > UINT64_MAX / elem_size)
            return {false, 0, "ARRAY_SIZE_OVERFLOW"};
            
        total_bytes = count * elem_size;
        
        if (total_bytes > static_cast<uint64_t>(file_size_ - Tell()))
            return {false, 0, "ARRAY_EXCEEDS_FILESIZE"};
            
        if (!RobustSeek(static_cast<int64_t>(total_bytes), SEEK_CUR))
            return {false, 0, "ARRAY_SEEK_FAILED"};
            
        return {true, static_cast<int64_t>(total_bytes), nullptr};
    }

    // Safe string read with explicit budget enforcement
    [[nodiscard]] ReadResult ReadStringSafe(std::string& out, uint64_t max_budget = 1*1024*1024) noexcept {
        out.clear();
        if (!handle_) return {false, 0, "NO_HANDLE"};
        
        uint64_t len = 0;
        if (fread(&len, 8, 1, handle_) != 1)
            return {false, 0, "READ_LENGTH_FAILED"};
            
        if (len > max_budget)
            return {false, 0, "BUDGET_EXCEEDED"}; // Caller should SkipString instead
            
        if (len == 0) return {true, 0, nullptr}; // Empty string valid
        
        try {
            out.resize(len);
        } catch (...) {
            return {false, 0, "ALLOCATION_FAILED"};
        }
        
        if (fread(out.data(), 1, len, handle_) != len) {
            out.clear();
            return {false, 0, "TRUNCATED_READ"};
        }
        
        return {true, len, nullptr};
    }

    static uint64_t GetTypeSize(uint32_t gguf_type) noexcept {
        switch(gguf_type) {
            case 0: return 1;  // UINT8
            case 1: return 1;  // INT8
            case 2: return 2;  // UINT16
            case 3: return 2;  // INT16
            case 4: return 4;  // UINT32
            case 5: return 4;  // INT32
            case 6: return 4;  // FLOAT32
            case 7: return 1;  // BOOL
            case 8: return 8;  // STRING (length prefix)
            case 9: return 0;  // ARRAY (handled separately)
            case 10: return 8; // UINT64
            case 11: return 8; // INT64
            case 12: return 8; // FLOAT64
            default: return 1; // Unknown
        }
    }
};

// -----------------------------------------------------------------------------
// 3. METADATA SURGEON
// Selective parser that skips toxic keys without loading them
// -----------------------------------------------------------------------------
class MetadataSurgeon {
    RobustGGUFStream& stream_;
    std::unordered_map<std::string, std::string> skipped_map_; // Key -> "[skipped:N]"
    
public:
    explicit MetadataSurgeon(RobustGGUFStream& s) : stream_(s) {}
    
    struct ParseConfig {
        uint64_t max_string_budget = 16*1024;        // 16KB default for strings
        uint64_t max_array_budget = 1024*1024;       // 1MB default for arrays
        bool skip_chat_template = true;              // Your specific workaround
        bool skip_tokenizer_merges = true;           // Your specific workaround
        bool skip_tokenizer_tokens = false;          // Usually safe to load
        std::function<bool(const char*)> custom_filter = nullptr;
    };

    // Entry point: Parse N metadata KV pairs with surgical skipping
    bool ParseKvPairs(uint64_t count, const ParseConfig& cfg = {}) noexcept {
        for (uint64_t i = 0; i < count; ++i) {
            // Read key string (names are small, safe to load)
            std::string key;
            auto key_res = stream_.ReadStringSafe(key, 512); // Keys rarely >512b
            if (!key_res.ok) return false;
            
            uint32_t value_type = 0;
            if (fread(&value_type, 4, 1, stream_.GetHandle()) != 1) return false;
            
            // Determine if we should skip based on key name and config
            bool should_skip = false;
            SkipReason reason = SkipReason::BUDGET;
            
            if (cfg.skip_chat_template && key == "tokenizer.chat_template") {
                should_skip = true;
                reason = SkipReason::TOXIC_KEY;
            } else if (cfg.skip_tokenizer_merges && key == "tokenizer.ggml.merges") {
                should_skip = true;
                reason = SkipReason::TOXIC_KEY;
            } else if (cfg.skip_tokenizer_tokens && key == "tokenizer.ggml.tokens") {
                should_skip = true;
                reason = SkipReason::TOXIC_KEY;
            }
            
            if (!should_skip && cfg.custom_filter && cfg.custom_filter(key.c_str())) {
                should_skip = true;
                reason = SkipReason::CUSTOM_FILTER;
            }
            
            // Route to handler based on type
            bool handled = false;
            if (value_type == 8) { // STRING
                handled = should_skip ? SkipStringField(key, reason) 
                                     : LoadStringField(key, cfg.max_string_budget);
            } else if (value_type == 9) { // ARRAY
                handled = should_skip ? SkipArrayField(key, reason) 
                                     : LoadArrayField(key, cfg.max_array_budget);
            } else {
                handled = SkipScalarField(value_type); // Simplified for scalars
            }
            
            if (!handled) return false;
        }
        return true;
    }

    const auto& GetSkippedMap() const noexcept { return skipped_map_; }

private:
    enum class SkipReason { BUDGET, TOXIC_KEY, CUSTOM_FILTER };
    
    bool SkipStringField(const std::string& key, SkipReason reason) {
        auto res = stream_.SkipString();
        if (res.ok) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[skipped:%s:%lld]", 
                    (reason == SkipReason::TOXIC_KEY) ? "toxic" : 
                    (reason == SkipReason::CUSTOM_FILTER) ? "filtered" : "oversized",
                    (long long)res.bytes_skipped);
            skipped_map_[key] = buf;
        }
        return res.ok;
    }
    
    bool LoadStringField(const std::string& key, uint64_t budget) {
        std::string dummy; // In real use, store to your metadata map
        auto res = stream_.ReadStringSafe(dummy, budget);
        if (!res.ok && res.error && std::string(res.error) == "BUDGET_EXCEEDED") {
            // Fallback to skip if too big
            return SkipStringField(key, SkipReason::BUDGET);
        }
        return res.ok;
    }
    
    bool SkipArrayField(const std::string& key, SkipReason reason) {
        auto res = stream_.SkipArray();
        if (res.ok) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[skipped:array:%lld]", (long long)res.bytes_skipped);
            skipped_map_[key] = buf;
        }
        return res.ok;
    }
    
    bool LoadArrayField(const std::string& key, uint64_t budget) {
        // Implementation for loading arrays within budget
        // For now, just skip
        return SkipArrayField(key, SkipReason::BUDGET);
    }
    
    bool SkipScalarField(uint32_t type) {
        uint64_t sz = RobustGGUFStream::GetTypeSize(type);
        return stream_.RobustSeek(static_cast<int64_t>(sz), SEEK_CUR);
    }
};

// -----------------------------------------------------------------------------
// 4. DIAGNOSTIC DUMPER
// Zero-risk metadata inspector for debugging corrupted GGUFs
// -----------------------------------------------------------------------------
class GgufAutopsy {
public:
    struct Report {
        uint64_t metadata_pairs = 0;
        uint64_t toxic_keys_found = 0;
        uint64_t total_metadata_bytes = 0;
        uint64_t max_string_length = 0;
        std::vector<std::string> oversized_keys;
    };

    static Report GenerateReport(const char* filepath) noexcept {
        Report r{};
        auto scan = CorruptionScan::ScanFile(filepath);
        if (!scan.is_valid) return r;
        
        RobustGGUFStream stream(filepath);
        if (!stream.IsOpen()) return r;
        
        // Seek past header (magic + version + n_tensors + n_kv)
        stream.RobustSeek(4 + 4 + 8 + 8, SEEK_SET);
        
        MetadataSurgeon surgeon(stream);
        
        // Config to detect but not load anything large
        MetadataSurgeon::ParseConfig cfg;
        cfg.max_string_budget = 0; // Force all strings to report as skipped
        cfg.max_array_budget = 0;
        cfg.skip_chat_template = false; // We want to know it exists
        
        surgeon.ParseKvPairs(scan.metadata_kv_count, cfg);
        
        // Extract results from skipped_map (which contains sizes)
        r.metadata_pairs = scan.metadata_kv_count;
        r.toxic_keys_found = surgeon.GetSkippedMap().size();
        
        for (const auto& [key, val] : surgeon.GetSkippedMap()) {
            r.oversized_keys.push_back(key);
        }
        
        return r;
    }
};

} // namespace rawrxd::gguf_robust
