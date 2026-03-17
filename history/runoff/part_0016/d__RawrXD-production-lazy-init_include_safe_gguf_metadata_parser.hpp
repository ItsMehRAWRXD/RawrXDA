// ============================================================================
// SAFE_GGUF_METADATA_PARSER.HPP
// Reverse-engineered robust parser with OOM-prevention guards
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <Windows.h>

namespace rawrxd::gguf {

constexpr uint64_t GGUF_MAX_SAFE_STRING = 16ULL * 1024 * 1024;      // 16MB cap
constexpr uint64_t GGUF_MAX_ARRAY_ELEMS = 1000000ULL;               // 1M elements max
constexpr uint64_t GGUF_MAGIC = 0x46554747;                         // "GGUF" little-endian

struct SafeMetadata {
    std::unordered_map<std::string, std::string> kv_string;
    std::unordered_map<std::string, uint64_t> kv_uint64;
    std::vector<std::pair<std::string, uint64_t>> skipped_entries;  // Audit trail
    uint64_t tensor_offset = 0;
    uint32_t version = 0;
    bool integrity_verified = false;
};

class RobustGGUFParser {
public:
    explicit RobustGGUFParser(const wchar_t* wide_path, bool verbose = false);
    ~RobustGGUFParser();
    
    SafeMetadata ParseWithGuards();
    bool ValidateMagicAndVersion();
    void EnableMemoryPressureMonitoring(size_t max_system_ram_gb = 64);
    
    // MASM-accelerated validation
    static bool __vectorcall ValidateStringLengthAVX512(uint64_t len, uint64_t max_val);
    
private:
    std::ifstream stream_;
    std::wstring path_;
    bool verbose_ = false;
    size_t max_ram_bytes_ = 64ULL * 1024 * 1024 * 1024;
    int64_t file_size_ = 0;
    
    // Defensive read helpers
    bool SafeRead(void* dst, size_t bytes);
    void SafeSkip(uint64_t bytes);
    std::string SafeReadString(uint64_t max_len = GGUF_MAX_SAFE_STRING);
    
    // Corruption detection
    bool CheckIfCorruptedOOB(uint64_t claimed_offset);
    void EmergencyPartialClose();
};

} // namespace rawrxd::gguf
