// ============================================================================
// RawrXD :: Robust GGUF Tools Suite v3.0 (UNIFIED)
// File: gguf_robust_tools_v3.hpp
// Arch: x64 Native | Zero External Dependencies | Windows 10/11+
// 
// This file integrates:
//   - v2 streaming file-based tools (fopen/fread/fseek64)
//   - v3 memory-mapped zero-copy tools (MapViewOfFile + Win32 Job objects)
//   - AVX-512 accelerated validation (Ice Lake+)
//   - Tensor salvage scanner for corrupted GGUF recovery
//
// Reverse-engineered from llama.cpp + Ollama loader internals + binary spec
// ============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <handleapi.h>
#include <memoryapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
#include <vadefs.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <optional>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstdint>

// ============================================================================
// GGUF Binary Format Constants (Reverse-Engineered from llama.cpp &amp; spec deltas)
// ============================================================================
namespace GGUFSpec {
    constexpr uint32_t MAGIC_LE = 0x46554747; // "GGUF" little-endian
    constexpr uint32_t VERSION_2 = 2;
    constexpr uint32_t VERSION_3 = 3;
    constexpr uint32_t VERSION_4 = 4;
    constexpr uint32_t MAX_SAFE_METADATA_ENTRIES = 65536; // Sanity cap
    constexpr uint64_t MAX_SAFE_STRING_LEN = 16ULL * 1024 * 1024; // 16MB safety valve
    constexpr uint64_t MAX_SAFE_ARRAY_ELEMENTS = 10'000'000; // Prevent OOM on vector resize
    constexpr uint32_t MAX_SAFE_TENSOR_COUNT = 1000000; // Sanity cap for tensors
    
    enum Type : uint32_t {
        UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3, UINT32 = 4,
        INT32 = 5, FLOAT32 = 6, BOOL = 7, STRING = 8, ARRAY = 9,
        UINT64 = 10, INT64 = 11, FLOAT64 = 12
    };
    
    // Type sizes for raw byte calculations
    constexpr size_t TypeSize[] = {
        1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8 // STRING/ARRAY variable
    };
    
    // Get type size safely
    inline constexpr uint64_t GetTypeSize(uint32_t gguf_type) noexcept {
        switch(gguf_type) {
            case 0: return 1;  // UINT8
            case 1: return 1;  // INT8
            case 2: return 2;  // UINT16
            case 3: return 2;  // INT16
            case 4: return 4;  // UINT32
            case 5: return 4;  // INT32
            case 6: return 4;  // FLOAT32
            case 7: return 1;  // BOOL
            case 8: return 8;  // STRING (length prefix only)
            case 9: return 0;  // ARRAY (handled separately)
            case 10: return 8; // UINT64
            case 11: return 8; // INT64
            case 12: return 8; // FLOAT64
            default: return 1; // Unknown - minimal skip
        }
    }
}

namespace rawrxd::gguf_robust {

// ============================================================================
// Tool 1: Memory Pressure Guardian (Prevents bad_alloc before it happens)
// ============================================================================
class MemoryPressureGuard {
    HANDLE hJob_;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits_;
    
public:
    // Default 60GB cap for 64GB system; adjust based on your target hardware
    explicit MemoryPressureGuard(size_t maxWorkingSetBytes = 60ULL * 1024 * 1024 * 1024) 
        : hJob_(nullptr) {
        ZeroMemory(&limits_, sizeof(limits_));
        
        hJob_ = CreateJobObjectW(nullptr, nullptr);
        if (!hJob_) return;
        
        limits_.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_WORKINGSET;
        limits_.ProcessMemoryLimit = maxWorkingSetBytes;
        limits_.JobMemoryLimit = maxWorkingSetBytes;
        
        // Add current process to job
        AssignProcessToJobObject(hJob_, GetCurrentProcess());
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &limits_, sizeof(limits_));
    }
    
    ~MemoryPressureGuard() {
        if (hJob_) CloseHandle(hJob_);
    }
    
    // Non-copyable
    MemoryPressureGuard(const MemoryPressureGuard&) = delete;
    MemoryPressureGuard& operator=(const MemoryPressureGuard&) = delete;
    
    // Static check: Will this allocation succeed without paging suicide?
    [[nodiscard]] static bool CanAllocate(size_t bytes) noexcept {
        MEMORYSTATUSEX status = { sizeof(status) };
        if (!GlobalMemoryStatusEx(&status)) return false;
        // Require 20% headroom after allocation
        return (status.ullAvailPhys > (bytes + (bytes / 5)));
    }
    
    // Get available physical memory
    [[nodiscard]] static uint64_t GetAvailablePhysicalMemory() noexcept {
        MEMORYSTATUSEX status = { sizeof(status) };
        if (!GlobalMemoryStatusEx(&status)) return 0;
        return status.ullAvailPhys;
    }
    
    // Get total physical memory
    [[nodiscard]] static uint64_t GetTotalPhysicalMemory() noexcept {
        MEMORYSTATUSEX status = { sizeof(status) };
        if (!GlobalMemoryStatusEx(&status)) return 0;
        return status.ullTotalPhys;
    }
};

// ============================================================================
// Tool 2A: PRE-FLIGHT CORRUPTION DETECTOR (Streaming-based, v2 compatible)
// Validates GGUF magic, version, and tensor count before any allocation
// ============================================================================
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
        result.file_size = static_cast<uint64_t>(_ftelli64(f));
        _fseeki64(f, 0, SEEK_SET);
        
        // Minimum file size check (header is at least 24 bytes)
        if (result.file_size < 24) {
            snprintf(result.error_msg, sizeof(result.error_msg), "FILE_TOO_SMALL");
            fclose(f);
            return result;
        }

        // GGUF magic: 'GGUF' = 0x46554747 (little-endian)
        uint32_t magic = 0;
        if (fread(&magic, 4, 1, f) != 1 || magic != GGUFSpec::MAGIC_LE) {
            snprintf(result.error_msg, sizeof(result.error_msg), "INVALID_MAGIC_0x%08X", magic);
            fclose(f);
            return result;
        }

        // Version check (support 2, 3, 4)
        if (fread(&result.gguf_version, 4, 1, f) != 1 || 
            (result.gguf_version < GGUFSpec::VERSION_2 || result.gguf_version > GGUFSpec::VERSION_4)) {
            snprintf(result.error_msg, sizeof(result.error_msg), "UNSUPPORTED_VERSION_%u", result.gguf_version);
            fclose(f);
            return result;
        }

        // Tensor count sanity (prevent OOM on bad header)
        if (fread(&result.tensor_count, 8, 1, f) != 1 || 
            result.tensor_count > GGUFSpec::MAX_SAFE_TENSOR_COUNT) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_TENSOR_COUNT_%llu", 
                     (unsigned long long)result.tensor_count);
            fclose(f);
            return result;
        }

        // Metadata count sanity
        if (fread(&result.metadata_kv_count, 8, 1, f) != 1 || 
            result.metadata_kv_count > GGUFSpec::MAX_SAFE_METADATA_ENTRIES * 10) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_METADATA_COUNT_%llu",
                     (unsigned long long)result.metadata_kv_count);
            fclose(f);
            return result;
        }

        result.is_valid = true;
        fclose(f);
        return result;
    }
    
    // Wide-character version for Win32 compatibility
    static CorruptionScan ScanFileW(const wchar_t* filepath) noexcept {
        CorruptionScan result{};
        FILE* f = nullptr;
        
        if (_wfopen_s(&f, filepath, L"rb") != 0 || !f) {
            snprintf(result.error_msg, sizeof(result.error_msg), "FILE_ACCESS_DENIED");
            return result;
        }

        // Same logic as ScanFile
        _fseeki64(f, 0, SEEK_END);
        result.file_size = static_cast<uint64_t>(_ftelli64(f));
        _fseeki64(f, 0, SEEK_SET);
        
        if (result.file_size < 24) {
            snprintf(result.error_msg, sizeof(result.error_msg), "FILE_TOO_SMALL");
            fclose(f);
            return result;
        }

        uint32_t magic = 0;
        if (fread(&magic, 4, 1, f) != 1 || magic != GGUFSpec::MAGIC_LE) {
            snprintf(result.error_msg, sizeof(result.error_msg), "INVALID_MAGIC_0x%08X", magic);
            fclose(f);
            return result;
        }

        if (fread(&result.gguf_version, 4, 1, f) != 1 || 
            (result.gguf_version < GGUFSpec::VERSION_2 || result.gguf_version > GGUFSpec::VERSION_4)) {
            snprintf(result.error_msg, sizeof(result.error_msg), "UNSUPPORTED_VERSION_%u", result.gguf_version);
            fclose(f);
            return result;
        }

        if (fread(&result.tensor_count, 8, 1, f) != 1 || 
            result.tensor_count > GGUFSpec::MAX_SAFE_TENSOR_COUNT) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_TENSOR_COUNT");
            fclose(f);
            return result;
        }

        if (fread(&result.metadata_kv_count, 8, 1, f) != 1 || 
            result.metadata_kv_count > GGUFSpec::MAX_SAFE_METADATA_ENTRIES * 10) {
            snprintf(result.error_msg, sizeof(result.error_msg), "CORRUPT_METADATA_COUNT");
            fclose(f);
            return result;
        }

        result.is_valid = true;
        fclose(f);
        return result;
    }
};

// ============================================================================
// Tool 2B: Corruption-Resilient Memory-Mapped File Stream (Zero-Copy)
// ============================================================================
class RobustMappedView {
    HANDLE hFile_;
    HANDLE hMapping_;
    uint8_t* baseAddr_;
    uint64_t fileSize_;
    uint64_t cursor_;
    bool writable_;
    
    // AVX-512 accelerated memcmp for magic/header validation
    [[nodiscard]] static bool FastCompare512(const void* a, const void* b, size_t len) noexcept {
        if (len < 64) return memcmp(a, b, len) == 0;
        
        // AVX-512VL + BW available on Ice Lake+
        #if defined(__AVX512BW__) && defined(__AVX512VL__)
        const uint8_t* pa = static_cast<const uint8_t*>(a);
        const uint8_t* pb = static_cast<const uint8_t*>(b);
        size_t i = 0;
        
        for (; i + 64 <= len; i += 64) {
            __m512i va = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(pa + i));
            __m512i vb = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(pb + i));
            if (_mm512_cmpneq_epi8_mask(va, vb)) return false;
        }
        // Tail
        return memcmp(pa + i, pb + i, len - i) == 0;
        #else
        return memcmp(a, b, len) == 0;
        #endif
    }

public:
    struct ValidationReport {
        bool isGGUF = false;
        uint32_t version = 0;
        uint64_t tensorCount = 0;
        uint64_t metadataKVCount = 0;
        bool hasCorruption = false;
        std::vector<std::string> corruptionLog;
        uint64_t suspiciousStringCount = 0;
        uint64_t totalMetadataSize = 0;
    };

    explicit RobustMappedView(const wchar_t* path) 
        : hFile_(INVALID_HANDLE_VALUE), hMapping_(nullptr), 
          baseAddr_(nullptr), fileSize_(0), cursor_(0), writable_(false) {
            
        hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, 
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) return;
        
        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile_, &size)) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
            return;
        }
        fileSize_ = static_cast<uint64_t>(size.QuadPart);
        
        // Create mapping
        hMapping_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapping_) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
            return;
        }
        
        baseAddr_ = static_cast<uint8_t*>(MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0));
    }
    
    // ANSI path constructor
    explicit RobustMappedView(const char* path)
        : hFile_(INVALID_HANDLE_VALUE), hMapping_(nullptr), 
          baseAddr_(nullptr), fileSize_(0), cursor_(0), writable_(false) {
            
        hFile_ = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, 
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) return;
        
        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile_, &size)) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
            return;
        }
        fileSize_ = static_cast<uint64_t>(size.QuadPart);
        
        hMapping_ = CreateFileMappingA(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapping_) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
            return;
        }
        
        baseAddr_ = static_cast<uint8_t*>(MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0));
    }
    
    ~RobustMappedView() {
        if (baseAddr_) UnmapViewOfFile(baseAddr_);
        if (hMapping_) CloseHandle(hMapping_);
        if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
    }
    
    // Non-copyable, movable
    RobustMappedView(const RobustMappedView&) = delete;
    RobustMappedView& operator=(const RobustMappedView&) = delete;
    
    [[nodiscard]] bool IsValid() const noexcept { return baseAddr_ != nullptr; }
    [[nodiscard]] uint64_t Size() const noexcept { return fileSize_; }
    [[nodiscard]] const uint8_t* Data() const noexcept { return baseAddr_; }
    [[nodiscard]] uint64_t Tell() const noexcept { return cursor_; }
    
    // Zero-copy cursor advance with bounds checking
    [[nodiscard]] bool Seek(uint64_t offset) noexcept {
        if (offset > fileSize_) return false;
        cursor_ = offset;
        return true;
    }
    
    [[nodiscard]] bool Read(void* dst, uint64_t len) noexcept {
        if (!baseAddr_ || cursor_ + len > fileSize_) return false;
        memcpy(dst, baseAddr_ + cursor_, static_cast<size_t>(len));
        cursor_ += len;
        return true;
    }
    
    // Peek without advancing cursor
    [[nodiscard]] bool Peek(void* dst, uint64_t len) const noexcept {
        if (!baseAddr_ || cursor_ + len > fileSize_) return false;
        memcpy(dst, baseAddr_ + cursor_, static_cast<size_t>(len));
        return true;
    }
    
    // Safe string read with length validation
    [[nodiscard]] std::optional<std::string> ReadStringSafe(
        uint64_t maxLen = GGUFSpec::MAX_SAFE_STRING_LEN) noexcept {
        
        uint64_t len = 0;
        if (!Read(&len, sizeof(len))) return std::nullopt;
        
        if (len > maxLen || len > (fileSize_ - cursor_)) {
            // Corruption detected - seek past it if possible
            if (len < (fileSize_ - cursor_)) cursor_ += len; // Attempt recovery
            return std::nullopt;
        }
        
        std::string result;
        try {
            result.resize(static_cast<size_t>(len));
        } catch (...) {
            return std::nullopt; // Allocation failure
        }
        
        memcpy(result.data(), baseAddr_ + cursor_, static_cast<size_t>(len));
        cursor_ += len;
        return result;
    }
    
    // Skip string without allocation (zero-copy)
    [[nodiscard]] bool SkipString() noexcept {
        uint64_t len = 0;
        if (!Read(&len, sizeof(len))) return false;
        if (len > (fileSize_ - cursor_)) return false;
        cursor_ += len;
        return true;
    }
    
    // ============================================================================
    // Deep Validator: Detects poisoned files before parsing
    // ============================================================================
    [[nodiscard]] ValidationReport DeepValidate() noexcept {
        ValidationReport report = {};
        if (!IsValid() || fileSize_ < 24) {
            report.hasCorruption = true;
            report.corruptionLog.push_back("File too small for GGUF header");
            return report;
        }
        
        cursor_ = 0;
        uint32_t magic = 0;
        Read(&magic, 4);
        
        if (magic != GGUFSpec::MAGIC_LE) {
            report.hasCorruption = true;
            char buf[64];
            snprintf(buf, sizeof(buf), "Invalid magic (0x%08X), expected GGUF", magic);
            report.corruptionLog.push_back(buf);
            return report;
        }
        report.isGGUF = true;
        
        Read(&report.version, 4);
        if (report.version < GGUFSpec::VERSION_2 || report.version > GGUFSpec::VERSION_4) {
            report.corruptionLog.push_back("Warning: Non-standard version " + std::to_string(report.version));
        }
        
        Read(&report.tensorCount, 8);
        Read(&report.metadataKVCount, 8);
        
        // Sanity tensors
        if (report.tensorCount > 100000) {
            report.hasCorruption = true;
            report.corruptionLog.push_back("Suspicious tensor count: " + std::to_string(report.tensorCount));
        }
        
        if (report.metadataKVCount > GGUFSpec::MAX_SAFE_METADATA_ENTRIES) {
            report.hasCorruption = true;
            report.corruptionLog.push_back("Metadata KV count exceeds safety limit");
        }
        
        // Parse metadata entries looking for corruption (sample first 1000)
        uint64_t metaStart = cursor_;
        for (uint64_t i = 0; i < report.metadataKVCount && i < 1000; i++) {
            auto key = ReadStringSafe(1024); // Keys should be short
            if (!key) {
                report.hasCorruption = true;
                report.corruptionLog.push_back("Failed to read metadata key at entry " + std::to_string(i));
                break;
            }
            
            uint32_t type = 0;
            if (!Read(&type, 4)) {
                report.hasCorruption = true;
                break;
            }
            
            // Value parsing with corruption detection
            if (type == GGUFSpec::STRING) {
                uint64_t strStart = cursor_;
                
                // Peek string length to check for corruption
                uint64_t strLen = 0;
                if (!Peek(&strLen, sizeof(strLen))) {
                    report.hasCorruption = true;
                    break;
                }
                
                if (strLen > GGUFSpec::MAX_SAFE_STRING_LEN) {
                    report.suspiciousStringCount++;
                    report.corruptionLog.push_back("Oversized string in key: " + *key + 
                                                   " (" + std::to_string(strLen) + " bytes)");
                }
                
                // Skip the string regardless
                if (!SkipString()) {
                    report.hasCorruption = true;
                    break;
                }
                report.totalMetadataSize += (cursor_ - strStart);
            }
            else if (type == GGUFSpec::ARRAY) {
                // Peek array dimensions without full load
                uint32_t arrType = 0;
                uint64_t arrCount = 0;
                if (!Read(&arrType, 4) || !Read(&arrCount, 8)) {
                    report.hasCorruption = true;
                    break;
                }
                
                if (arrCount > GGUFSpec::MAX_SAFE_ARRAY_ELEMENTS) {
                    report.hasCorruption = true;
                    report.corruptionLog.push_back("Array too large: " + std::to_string(arrCount) + 
                                                   " elements in key " + *key);
                }
                
                // Skip array data without allocation
                uint64_t elementSize = GGUFSpec::GetTypeSize(arrType);
                if (elementSize > 0) {
                    uint64_t skipBytes = arrCount * elementSize;
                    
                    if (arrType == GGUFSpec::STRING) {
                        // Must walk strings to skip
                        for (uint64_t j = 0; j < std::min(arrCount, 100ULL); j++) {
                            if (!SkipString()) { 
                                report.hasCorruption = true; 
                                break; 
                            }
                        }
                        // Approximate skip for remaining strings (assume average 8 bytes)
                        if (arrCount > 100 && !report.hasCorruption) {
                            cursor_ += (arrCount - 100) * 16; // Rough estimate
                            if (cursor_ > fileSize_) {
                                cursor_ = fileSize_;
                                report.hasCorruption = true;
                            }
                        }
                    } else {
                        if (cursor_ + skipBytes <= fileSize_) {
                            cursor_ += skipBytes;
                        } else {
                            report.hasCorruption = true;
                        }
                    }
                }
            }
            else if (type < 13) {
                uint64_t typeSize = GGUFSpec::GetTypeSize(type);
                if (cursor_ + typeSize <= fileSize_) {
                    cursor_ += typeSize;
                } else {
                    report.hasCorruption = true;
                    break;
                }
            } else {
                report.hasCorruption = true;
                report.corruptionLog.push_back("Invalid type " + std::to_string(type));
                break;
            }
        }
        
        return report;
    }
};

// ============================================================================
// Tool 3A: ZERO-COPY STREAMING READER (v2 compatible, file-based)
// Kernel-bypass philosophy: never allocate on oversized/corrupted fields
// ============================================================================
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
    
    explicit RobustGGUFStream(const wchar_t* path) noexcept {
        _wfopen_s(&handle_, path, L"rb");
        if (handle_) {
            _fseeki64(handle_, 0, SEEK_END);
            file_size_ = _ftelli64(handle_);
            _fseeki64(handle_, 0, SEEK_SET);
        }
    }
    
    ~RobustGGUFStream() { if (handle_) fclose(handle_); }
    
    // Non-copyable
    RobustGGUFStream(const RobustGGUFStream&) = delete;
    RobustGGUFStream& operator=(const RobustGGUFStream&) = delete;
    
    [[nodiscard]] bool IsOpen() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] int64_t Tell() const noexcept { return handle_ ? _ftelli64(handle_) : -1; }
    [[nodiscard]] int64_t Size() const noexcept { return file_size_; }
    [[nodiscard]] FILE* GetHandle() const noexcept { return handle_; }

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
        uint64_t elem_size = GGUFSpec::GetTypeSize(elem_type);
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
            out.resize(static_cast<size_t>(len));
        } catch (...) {
            return {false, 0, "ALLOCATION_FAILED"};
        }
        
        if (fread(out.data(), 1, static_cast<size_t>(len), handle_) != len) {
            out.clear();
            return {false, 0, "TRUNCATED_READ"};
        }
        
        return {true, static_cast<size_t>(len), nullptr};
    }
};

// ============================================================================
// Tool 4: METADATA SURGEON
// Selective parser that skips toxic keys without loading them
// ============================================================================
class MetadataSurgeon {
    RobustGGUFStream& stream_;
    std::unordered_map<std::string, std::string> skipped_map_; // Key -> "[skipped:N]"
    
public:
    explicit MetadataSurgeon(RobustGGUFStream& s) : stream_(s) {}
    
    struct ParseConfig {
        uint64_t max_string_budget = 16*1024;        // 16KB default for strings
        uint64_t max_array_budget = 1024*1024;       // 1MB default for arrays
        bool skip_chat_template = true;              // Skip toxic Jinja templates
        bool skip_tokenizer_merges = true;           // Skip BPE merge tables (2GB+ on some models)
        bool skip_tokenizer_tokens = false;          // Usually safe to load
        std::function<bool(const char*)> custom_filter = nullptr;
        std::function<void(const char* key, uint64_t size)> onSkip = nullptr; // Callback for skipped keys
    };

    // Entry point: Parse N metadata KV pairs with surgical skipping
    [[nodiscard]] bool ParseKvPairs(uint64_t count, const ParseConfig& cfg = {}) noexcept {
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
            if (value_type == GGUFSpec::STRING) {
                handled = should_skip ? SkipStringField(key, reason, cfg.onSkip) 
                                     : LoadStringField(key, cfg.max_string_budget, cfg.onSkip);
            } else if (value_type == GGUFSpec::ARRAY) {
                handled = should_skip ? SkipArrayField(key, reason, cfg.onSkip) 
                                     : LoadArrayField(key, cfg.max_array_budget);
            } else {
                handled = SkipScalarField(value_type);
            }
            
            if (!handled) return false;
        }
        return true;
    }

    [[nodiscard]] const auto& GetSkippedMap() const noexcept { return skipped_map_; }

private:
    enum class SkipReason { BUDGET, TOXIC_KEY, CUSTOM_FILTER };
    
    bool SkipStringField(const std::string& key, SkipReason reason, 
                        const std::function<void(const char*, uint64_t)>& onSkip) {
        auto res = stream_.SkipString();
        if (res.ok) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[skipped:%s:%lld]", 
                    (reason == SkipReason::TOXIC_KEY) ? "toxic" : 
                    (reason == SkipReason::CUSTOM_FILTER) ? "filtered" : "oversized",
                    (long long)res.bytes_skipped);
            skipped_map_[key] = buf;
            
            if (onSkip) onSkip(key.c_str(), static_cast<uint64_t>(res.bytes_skipped));
        }
        return res.ok;
    }
    
    bool LoadStringField(const std::string& key, uint64_t budget,
                        const std::function<void(const char*, uint64_t)>& onSkip) {
        std::string dummy;
        auto res = stream_.ReadStringSafe(dummy, budget);
        if (!res.ok && res.error && std::string(res.error) == "BUDGET_EXCEEDED") {
            // Fallback to skip if too big
            return SkipStringField(key, SkipReason::BUDGET, onSkip);
        }
        return res.ok;
    }
    
    bool SkipArrayField(const std::string& key, SkipReason reason,
                       const std::function<void(const char*, uint64_t)>& onSkip) {
        auto res = stream_.SkipArray();
        if (res.ok) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[skipped:array:%lld]", (long long)res.bytes_skipped);
            skipped_map_[key] = buf;
            
            if (onSkip) onSkip(key.c_str(), static_cast<uint64_t>(res.bytes_skipped));
        }
        return res.ok;
    }
    
    bool LoadArrayField(const std::string& key, uint64_t budget) {
        // For now, skip arrays (full implementation would parse into map)
        return SkipArrayField(key, SkipReason::BUDGET, nullptr);
    }
    
    bool SkipScalarField(uint32_t type) {
        uint64_t sz = GGUFSpec::GetTypeSize(type);
        return stream_.RobustSeek(static_cast<int64_t>(sz), SEEK_CUR);
    }
};

// ============================================================================
// Tool 5: Surgical Metadata Extractor (Memory-Mapped Version)
// Skips toxins, extracts gold - works with RobustMappedView
// ============================================================================
class SurgicalMetadataExtractor {
public:
    struct ExtractionPolicy {
        bool skipChatTemplate = true;      // Skip toxic Jinja templates
        bool skipMerges = true;            // Skip BPE merge tables
        bool skipVocab = false;            // Keep token IDs (usually needed)
        uint64_t maxStringBytes = 1 * 1024 * 1024; // 1MB max for any single string
        std::function<void(const char* key, uint64_t size)> onSkip;
    };

    [[nodiscard]] std::unordered_map<std::string, std::string> Extract(
        RobustMappedView& view, 
        const ExtractionPolicy& policy) noexcept {
        
        std::unordered_map<std::string, std::string> result;
        auto report = view.DeepValidate();
        
        if (!report.isGGUF) return result;
        
        // Skip to metadata (after header: 4+4+8+8 = 24 bytes)
        view.Seek(24);
        
        for (uint64_t i = 0; i < report.metadataKVCount; i++) {
            auto keyOpt = view.ReadStringSafe(512); // Keys should be short
            if (!keyOpt) break;
            std::string key = *keyOpt;
            
            uint32_t type = 0;
            if (!view.Read(&type, 4)) break;
            
            // Decision engine: Do we want this key?
            bool shouldSkip = false;
            std::string skipReason;
            
            if (type == GGUFSpec::STRING) {
                if (key == "tokenizer.chat_template" && policy.skipChatTemplate) {
                    shouldSkip = true; 
                    skipReason = "chat_template toxin";
                }
                else if (key.find("tokenizer.ggml.merges") != std::string::npos && policy.skipMerges) {
                    shouldSkip = true; 
                    skipReason = "merge_table bloat";
                }
                else if (key.find("tokenizer.ggml.tokens") != std::string::npos && policy.skipVocab) {
                    shouldSkip = true;
                    skipReason = "vocab skip";
                }
                
                if (shouldSkip) {
                    // Peek length to report
                    uint64_t len = 0;
                    view.Read(&len, 8);
                    
                    if (policy.onSkip) policy.onSkip(key.c_str(), len);
                    
                    // Skip without allocation
                    if (len < view.Size() - view.Tell()) {
                        view.Seek(view.Tell() + len);
                    }
                    result[key] = "[skipped:" + std::to_string(len) + "]";
                    continue;
                }
            }
            
            // Safe extraction based on type
            switch (type) {
                case GGUFSpec::STRING: {
                    auto val = view.ReadStringSafe(policy.maxStringBytes);
                    if (val) result[key] = *val;
                    else result[key] = "[corrupted]";
                    break;
                }
                case GGUFSpec::INT32: {
                    int32_t v = 0; view.Read(&v, 4); 
                    result[key] = std::to_string(v); 
                    break;
                }
                case GGUFSpec::UINT32: {
                    uint32_t v = 0; view.Read(&v, 4);
                    result[key] = std::to_string(v);
                    break;
                }
                case GGUFSpec::INT64: {
                    int64_t v = 0; view.Read(&v, 8);
                    result[key] = std::to_string(v);
                    break;
                }
                case GGUFSpec::UINT64: {
                    uint64_t v = 0; view.Read(&v, 8);
                    result[key] = std::to_string(v);
                    break;
                }
                case GGUFSpec::FLOAT32: {
                    float v = 0; view.Read(&v, 4); 
                    result[key] = std::to_string(v); 
                    break;
                }
                case GGUFSpec::FLOAT64: {
                    double v = 0; view.Read(&v, 8);
                    result[key] = std::to_string(v);
                    break;
                }
                case GGUFSpec::BOOL: {
                    uint8_t v = 0; view.Read(&v, 1);
                    result[key] = v ? "true" : "false"; 
                    break;
                }
                case GGUFSpec::ARRAY: {
                    // Skip arrays for now (full implementation would parse)
                    uint32_t arrType = 0;
                    uint64_t arrCount = 0;
                    view.Read(&arrType, 4);
                    view.Read(&arrCount, 8);
                    
                    uint64_t elemSize = GGUFSpec::GetTypeSize(arrType);
                    if (elemSize > 0 && arrCount < UINT64_MAX / elemSize) {
                        uint64_t skipBytes = arrCount * elemSize;
                        if (arrType == GGUFSpec::STRING) {
                            // Walk strings
                            for (uint64_t j = 0; j < arrCount && j < 10000; j++) {
                                view.SkipString();
                            }
                        } else {
                            view.Seek(view.Tell() + skipBytes);
                        }
                    }
                    result[key] = "[array:" + std::to_string(arrCount) + "]";
                    break;
                }
                default: {
                    // Skip unknown types
                    uint64_t typeSize = GGUFSpec::GetTypeSize(type);
                    if (typeSize > 0) {
                        view.Seek(view.Tell() + typeSize);
                    }
                    result[key] = "[unhandled_type:" + std::to_string(type) + "]";
                }
            }
        }
        return result;
    }
};

// ============================================================================
// Tool 6: Tensor Salvage Scanner (Recovers weights from corrupted GGUF)
// ============================================================================
class TensorSalvageScanner {
public:
    struct TensorInfo {
        uint64_t offset = 0;
        uint64_t size = 0;
        uint32_t type = 0;
        uint32_t dimCount = 0;
        std::vector<uint64_t> dimensions;
        std::string name;
        bool isValid = false;
    };

    [[nodiscard]] std::vector<TensorInfo> ScanTensors(RobustMappedView& view, uint64_t tensorCount) noexcept {
        std::vector<TensorInfo> tensors;
        
        try {
            tensors.reserve(std::min(tensorCount, 1024ULL));
        } catch (...) {
            return tensors; // Allocation failed
        }
        
        for (uint64_t i = 0; i < tensorCount; i++) {
            TensorInfo info = {};
            info.isValid = false;
            
            // Read name
            auto nameOpt = view.ReadStringSafe(1024);
            if (!nameOpt || nameOpt->empty()) {
                continue;
            }
            info.name = *nameOpt;
            
            // Dimensions
            uint32_t nDims = 0;
            if (!view.Read(&nDims, 4) || nDims > 8 || nDims == 0) {
                continue; // GGML max 8D typically
            }
            info.dimCount = nDims;
            
            try {
                info.dimensions.resize(nDims);
            } catch (...) {
                continue;
            }
            
            bool dimOk = true;
            for (uint32_t d = 0; d < nDims; d++) {
                if (!view.Read(&info.dimensions[d], 8)) {
                    dimOk = false;
                    break;
                }
            }
            if (!dimOk) continue;
            
            // Tensor type and offset
            if (!view.Read(&info.type, 4)) continue;
            if (!view.Read(&info.offset, 8)) continue;
            
            // Calculate tensor size based on dimensions and type
            // This is a simplified calculation - real GGML has complex quantization
            uint64_t elements = 1;
            for (uint32_t d = 0; d < nDims; d++) {
                if (elements > UINT64_MAX / info.dimensions[d]) {
                    elements = UINT64_MAX; // Overflow
                    break;
                }
                elements *= info.dimensions[d];
            }
            
            // Estimate size based on type (simplified)
            info.size = elements * GGUFSpec::GetTypeSize(info.type);
            
            // Validate offset is within file bounds
            if (info.offset + info.size <= view.Size()) {
                info.isValid = true;
                tensors.push_back(std::move(info));
            }
        }
        
        return tensors;
    }
    
    // Streaming version using RobustGGUFStream
    [[nodiscard]] std::vector<TensorInfo> ScanTensors(RobustGGUFStream& stream, uint64_t tensorCount) noexcept {
        std::vector<TensorInfo> tensors;
        
        try {
            tensors.reserve(std::min(tensorCount, 1024ULL));
        } catch (...) {
            return tensors;
        }
        
        for (uint64_t i = 0; i < tensorCount; i++) {
            TensorInfo info = {};
            
            // Read name
            std::string name;
            auto nameRes = stream.ReadStringSafe(name, 1024);
            if (!nameRes.ok || name.empty()) continue;
            info.name = std::move(name);
            
            // Dimensions
            uint32_t nDims = 0;
            if (fread(&nDims, 4, 1, stream.GetHandle()) != 1 || nDims > 8 || nDims == 0) {
                continue;
            }
            info.dimCount = nDims;
            
            try {
                info.dimensions.resize(nDims);
            } catch (...) {
                continue;
            }
            
            bool dimOk = true;
            for (uint32_t d = 0; d < nDims; d++) {
                if (fread(&info.dimensions[d], 8, 1, stream.GetHandle()) != 1) {
                    dimOk = false;
                    break;
                }
            }
            if (!dimOk) continue;
            
            // Type and offset
            if (fread(&info.type, 4, 1, stream.GetHandle()) != 1) continue;
            if (fread(&info.offset, 8, 1, stream.GetHandle()) != 1) continue;
            
            // Size calculation
            uint64_t elements = 1;
            for (uint32_t d = 0; d < nDims; d++) {
                if (elements > UINT64_MAX / info.dimensions[d]) {
                    elements = UINT64_MAX;
                    break;
                }
                elements *= info.dimensions[d];
            }
            info.size = elements * GGUFSpec::GetTypeSize(info.type);
            
            if (info.offset + info.size <= static_cast<uint64_t>(stream.Size())) {
                info.isValid = true;
                tensors.push_back(std::move(info));
            }
        }
        
        return tensors;
    }
    
    // Emergency extraction: Dump raw tensor data to separate files
    [[nodiscard]] bool EmergencyExtract(RobustMappedView& view, const TensorInfo& tensor, 
                                        const wchar_t* outPath) noexcept {
        if (!tensor.isValid || tensor.offset + tensor.size > view.Size()) return false;
        
        HANDLE hOut = CreateFileW(outPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hOut == INVALID_HANDLE_VALUE) return false;
        
        DWORD written = 0;
        BOOL ok = WriteFile(hOut, view.Data() + tensor.offset, 
                           static_cast<DWORD>(std::min(tensor.size, (uint64_t)MAXDWORD)), 
                           &written, nullptr);
        
        // Handle large tensors (>4GB)
        if (ok && tensor.size > MAXDWORD) {
            uint64_t remaining = tensor.size - written;
            uint64_t pos = written;
            while (remaining > 0 && ok) {
                DWORD chunkSize = static_cast<DWORD>(std::min(remaining, (uint64_t)MAXDWORD));
                ok = WriteFile(hOut, view.Data() + tensor.offset + pos, chunkSize, &written, nullptr);
                pos += written;
                remaining -= written;
            }
        }
        
        CloseHandle(hOut);
        return ok != FALSE;
    }
};

// ============================================================================
// Tool 7: DIAGNOSTIC DUMPER
// Zero-risk metadata inspector for debugging corrupted GGUFs
// ============================================================================
class GgufAutopsy {
public:
    struct Report {
        uint64_t metadata_pairs = 0;
        uint64_t toxic_keys_found = 0;
        uint64_t total_metadata_bytes = 0;
        uint64_t max_string_length = 0;
        std::vector<std::string> oversized_keys;
        std::vector<std::string> warnings;
    };

    [[nodiscard]] static Report GenerateReport(const char* filepath) noexcept {
        Report r{};
        auto scan = CorruptionScan::ScanFile(filepath);
        if (!scan.is_valid) {
            r.warnings.push_back(std::string("Pre-flight scan failed: ") + scan.error_msg);
            return r;
        }
        
        RobustGGUFStream stream(filepath);
        if (!stream.IsOpen()) {
            r.warnings.push_back("Failed to open stream");
            return r;
        }
        
        // Seek past header (magic + version + n_tensors + n_kv)
        stream.RobustSeek(4 + 4 + 8 + 8, SEEK_SET);
        
        MetadataSurgeon surgeon(stream);
        
        // Config to detect but not load anything large
        MetadataSurgeon::ParseConfig cfg;
        cfg.max_string_budget = 0; // Force all strings to report as skipped
        cfg.max_array_budget = 0;
        cfg.skip_chat_template = false; // We want to know it exists
        cfg.skip_tokenizer_merges = false;
        
        surgeon.ParseKvPairs(scan.metadata_kv_count, cfg);
        
        // Extract results from skipped_map
        r.metadata_pairs = scan.metadata_kv_count;
        r.toxic_keys_found = surgeon.GetSkippedMap().size();
        
        for (const auto& [key, val] : surgeon.GetSkippedMap()) {
            r.oversized_keys.push_back(key);
        }
        
        return r;
    }
    
    [[nodiscard]] static Report GenerateReportW(const wchar_t* filepath) noexcept {
        Report r{};
        auto scan = CorruptionScan::ScanFileW(filepath);
        if (!scan.is_valid) {
            r.warnings.push_back(std::string("Pre-flight scan failed: ") + scan.error_msg);
            return r;
        }
        
        RobustGGUFStream stream(filepath);
        if (!stream.IsOpen()) {
            r.warnings.push_back("Failed to open stream");
            return r;
        }
        
        stream.RobustSeek(4 + 4 + 8 + 8, SEEK_SET);
        
        MetadataSurgeon surgeon(stream);
        MetadataSurgeon::ParseConfig cfg;
        cfg.max_string_budget = 0;
        cfg.max_array_budget = 0;
        cfg.skip_chat_template = false;
        cfg.skip_tokenizer_merges = false;
        
        surgeon.ParseKvPairs(scan.metadata_kv_count, cfg);
        
        r.metadata_pairs = scan.metadata_kv_count;
        r.toxic_keys_found = surgeon.GetSkippedMap().size();
        
        for (const auto& [key, val] : surgeon.GetSkippedMap()) {
            r.oversized_keys.push_back(key);
        }
        
        return r;
    }
};

// ============================================================================
// HIGH-LEVEL CONVENIENCE FUNCTIONS
// ============================================================================

// Example: Integration into your existing loader
[[nodiscard]] inline bool ValidateBigDaddyGModel(const wchar_t* path) noexcept {
    MemoryPressureGuard guard(58ULL << 30); // 58GB limit
    
    RobustMappedView view(path);
    if (!view.IsValid()) {
        fprintf(stderr, "[RAW] Failed to map file\n");
        return false;
    }
    
    auto report = view.DeepValidate();
    printf("[RAW] GGUF Version: %u\n", report.version);
    printf("[RAW] Tensors: %llu | Metadata KV: %llu\n", 
           (unsigned long long)report.tensorCount, 
           (unsigned long long)report.metadataKVCount);
    
    if (report.suspiciousStringCount > 0) {
        printf("[RAW] WARNING: %llu oversized strings detected (likely corrupted chat_template)\n",
               (unsigned long long)report.suspiciousStringCount);
    }
    
    for (const auto& log : report.corruptionLog) {
        printf("[RAW] CORRUPTION: %s\n", log.c_str());
    }
    
    // Surgical extraction skipping toxins
    SurgicalMetadataExtractor extractor;
    SurgicalMetadataExtractor::ExtractionPolicy policy;
    policy.skipChatTemplate = true;
    policy.onSkip = [](const char* key, uint64_t size) {
        printf("[RAW] Skipped toxin '%s' (%llu bytes)\n", key, (unsigned long long)size);
    };
    
    auto metadata = extractor.Extract(view, policy);
    printf("[RAW] Extracted %zu safe metadata keys\n", metadata.size());
    
    return !report.hasCorruption;
}

// ANSI version
[[nodiscard]] inline bool ValidateBigDaddyGModel(const char* path) noexcept {
    MemoryPressureGuard guard(58ULL << 30);
    
    RobustMappedView view(path);
    if (!view.IsValid()) {
        fprintf(stderr, "[RAW] Failed to map file\n");
        return false;
    }
    
    auto report = view.DeepValidate();
    printf("[RAW] GGUF Version: %u\n", report.version);
    printf("[RAW] Tensors: %llu | Metadata KV: %llu\n", 
           (unsigned long long)report.tensorCount, 
           (unsigned long long)report.metadataKVCount);
    
    if (report.suspiciousStringCount > 0) {
        printf("[RAW] WARNING: %llu oversized strings detected\n",
               (unsigned long long)report.suspiciousStringCount);
    }
    
    for (const auto& log : report.corruptionLog) {
        printf("[RAW] CORRUPTION: %s\n", log.c_str());
    }
    
    SurgicalMetadataExtractor extractor;
    SurgicalMetadataExtractor::ExtractionPolicy policy;
    policy.skipChatTemplate = true;
    policy.onSkip = [](const char* key, uint64_t size) {
        printf("[RAW] Skipped '%s' (%llu bytes)\n", key, (unsigned long long)size);
    };
    
    auto metadata = extractor.Extract(view, policy);
    printf("[RAW] Extracted %zu safe metadata keys\n", metadata.size());
    
    return !report.hasCorruption;
}

// Quick pre-flight check (minimal overhead)
[[nodiscard]] inline bool QuickValidate(const char* path) noexcept {
    auto scan = CorruptionScan::ScanFile(path);
    return scan.is_valid;
}

[[nodiscard]] inline bool QuickValidateW(const wchar_t* path) noexcept {
    auto scan = CorruptionScan::ScanFileW(path);
    return scan.is_valid;
}

} // namespace rawrxd::gguf_robust
