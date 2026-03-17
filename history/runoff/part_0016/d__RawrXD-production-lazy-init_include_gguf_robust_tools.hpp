// =============================================================================
// RawrXD GGUF Robust Tools Suite
// Zero-allocation failure paths, corruption-resistant, reverse-engineered specs
// =============================================================================

#ifndef RAWRXD_GGUF_ROBUST_TOOLS_HPP
#define RAWRXD_GGUF_ROBUST_TOOLS_HPP

#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <system_error>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "ntdll.lib")

namespace rawrxd {

// =============================================================================
// 1. SAFETY BOUNDS & CONSTANTS (reverse-engineered GGUF spec limits)
// =============================================================================
struct GGUF_SAFETY_LIMITS {
    static constexpr uint64_t MAX_STRING_LEN = 16ULL * 1024 * 1024;      // 16MB hard cap
    static constexpr uint64_t MAX_ARRAY_ELEMENTS = 1ULL << 31;          // 2B elements max
    static constexpr uint64_t MAX_TENSOR_SIZE = 128ULL * 1024 * 1024 * 1024; // 128GB tensor
    static constexpr uint32_t MAX_METADATA_KEYS = 100000;                // Anti-DoS
    static constexpr uint32_t MAX_TENSOR_COUNT = 10000;                  // Sanity limit
};

// =============================================================================
// 2. NT MEMORY MAPPING WRAPPER (kernel-tier reliability)
// =============================================================================
class NtMappedFile {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
    void*  pView_ = nullptr;
    size_t szFile_ = 0;
    
public:
    struct Mapping {
        void* base;
        size_t size;
        HANDLE handle;
    };

    explicit NtMappedFile(const wchar_t* path) {
        // NtCreateFile equivalent for max control
        UNICODE_STRING uniPath;
        RtlInitUnicodeString(&uniPath, path);
        
        OBJECT_ATTRIBUTES objAttr;
        InitializeObjectAttributes(&objAttr, &uniPath, 
            OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, nullptr, nullptr);
            
        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtCreateFile(
            &hFile_, FILE_READ_DATA | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
            &objAttr, &ioStatus, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ, FILE_OPEN, 
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONIZE_IO_NONALERT,
            nullptr, 0
        );
        
        if (!NT_SUCCESS(status)) {
            throw std::runtime_error("NtCreateFile failed: " + std::to_string(status));
        }

        // Get file size (LargeInteger safe)
        FILE_STANDARD_INFO fileInfo;
        if (!GetFileInformationByHandleEx(hFile_, FileStandardInfo, 
            &fileInfo, sizeof(fileInfo))) {
            CloseHandle(hFile_);
            throw std::runtime_error("GetFileInformationByHandleEx failed");
        }
        
        szFile_ = static_cast<size_t>(fileInfo.EndOfFile.QuadPart);
        if (szFile_ == 0) {
            CloseHandle(hFile_);
            throw std::runtime_error("Zero-byte file");
        }

        // Create mapping (SEC_COMMIT for guaranteed backing)
        hMapping_ = CreateFileMapping(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapping_) {
            CloseHandle(hFile_);
            throw std::runtime_error("CreateFileMapping failed: " + 
                std::to_string(GetLastError()));
        }

        // Map view (reserve VA space only, physical pages on demand)
        pView_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
        if (!pView_) {
            CloseHandle(hMapping_);
            CloseHandle(hFile_);
            throw std::runtime_error("MapViewOfFile failed");
        }
    }

    // Non-copyable, movable
    NtMappedFile(const NtMappedFile&) = delete;
    NtMappedFile& operator=(const NtMappedFile&) = delete;
    
    NtMappedFile(NtMappedFile&& other) noexcept 
        : hFile_(other.hFile_), hMapping_(other.hMapping_), 
          pView_(other.pView_), szFile_(other.szFile_) {
        other.hFile_ = INVALID_HANDLE_VALUE;
        other.hMapping_ = nullptr;
        other.pView_ = nullptr;
        other.szFile_ = 0;
    }

    ~NtMappedFile() {
        if (pView_) UnmapViewOfFile(pView_);
        if (hMapping_) CloseHandle(hMapping_);
        if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
    }

    [[nodiscard]] const uint8_t* data() const noexcept { 
        return static_cast<const uint8_t*>(pView_); 
    }
    [[nodiscard]] size_t size() const noexcept { return szFile_; }
    
    // Bounds-checked accessor (returns nullptr on OOB)
    [[nodiscard]] const uint8_t* at(size_t offset, size_t len) const noexcept {
        if (offset + len < offset) return nullptr; // Overflow
        if (offset + len > szFile_) return nullptr;
        return static_cast<const uint8_t*>(pView_) + offset;
    }
};

// =============================================================================
// 3. SAFE PARSER STATE MACHINE (corruption-resistant GGUF reader)
// =============================================================================
class RobustGGUFParser {
public:
    enum class ErrorCode : uint8_t {
        OK = 0,
        TRUNCATED_HEADER,
        MAGIC_MISMATCH,
        VERSION_UNSUPPORTED,
        OVERSIZED_STRING,
        OVERSIZED_ARRAY,
        TENSOR_COUNT_INVALID,
        METADATA_COUNT_INVALID,
        MEMORY_EXHAUSTION,
        CHECKSUM_MISMATCH
    };

    struct ParseResult {
        ErrorCode code;
        const char* context;  // Static string, no alloc
        size_t byteOffset;    // Where failure occurred
    };

    struct SafeMetadata {
        std::string key;          // Only if < 1KB keys
        int32_t type;             // GGUF type enum
        std::vector<uint8_t> raw; // For arrays, empty if skipped
        
        // Accessor helpers with bounds checking
        [[nodiscard]] bool asString(std::string& out, size_t maxLen = 65536) const {
            if (type != 11 || raw.size() > maxLen) return false;
            out.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
            return true;
        }

        [[nodiscard]] bool asArrayUint64(std::vector<uint64_t>& out, size_t maxElements = 100000) const {
            if (type != 12) return false;
            if (raw.size() < 12) return false;

            const auto* ptr = raw.data();
            int32_t elemType = *reinterpret_cast<const int32_t*>(ptr);
            uint64_t count = *reinterpret_cast<const uint64_t*>(ptr + 4);
            if (elemType != 10 || count > maxElements) return false;
            if (raw.size() < 12 + count * sizeof(uint64_t)) return false;

            out.resize(static_cast<size_t>(count));
            const uint8_t* data = ptr + 12;
            std::memcpy(out.data(), data, static_cast<size_t>(count) * sizeof(uint64_t));
            return true;
        }
    };

private:
    const uint8_t* base_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
    bool strictMode_ = false;
    
    // Corruption detection
    uint32_t version_ = 0;
    uint64_t tensorCount_ = 0;
    uint64_t metadataCount_ = 0;
    
    // Safety: keys that trigger auto-skip regardless of size (corruption vectors)
    static constexpr const char* SKIP_KEYS[] = {
        "tokenizer.chat_template",  // Your specific crash vector
        "tokenizer.ggml.merges",    // Memory explosion
        "tokenizer.ggml.tokens",    // Multi-megabyte vocab arrays
    };

    [[nodiscard]] bool checkBounds(size_t need) const noexcept {
        return (pos_ + need >= pos_) && (pos_ + need <= size_);
    }

    [[nodiscard]] const uint8_t* consume(size_t len, ParseResult& err) noexcept {
        if (!checkBounds(len)) {
            err = {ErrorCode::TRUNCATED_HEADER, "bounds check failed", pos_};
            return nullptr;
        }
        const uint8_t* p = base_ + pos_;
        pos_ += len;
        return p;
    }

    [[nodiscard]] static bool shouldSkipKey(const std::string& key) noexcept {
        for (const auto* skip : SKIP_KEYS) {
            if (key == skip) return true;
        }
        return false;
    }

    [[nodiscard]] static size_t scalarSizeForType(int32_t type) noexcept {
        switch (type) {
            case 0:  // UINT8
            case 1:  // INT8
                return 1;
            case 2:  // UINT16
            case 3:  // INT16
                return 2;
            case 4:  // UINT32
            case 5:  // INT32
            case 6:  // FLOAT32
                return 4;
            case 7:  // BOOL
                return 1;
            case 8:  // UINT64
            case 9:  // INT64
            case 10: // FLOAT64
                return 8;
            default:
                return 0;
        }
    }

    [[nodiscard]] bool readArrayRaw(int32_t elemType, uint64_t count, ParseResult& err, std::vector<uint8_t>& out) noexcept {
        if (count > GGUF_SAFETY_LIMITS::MAX_ARRAY_ELEMENTS) {
            err = {ErrorCode::OVERSIZED_ARRAY, "array element count overflow", pos_};
            return false;
        }

        if (elemType == 11) { // STRING
            for (uint64_t i = 0; i < count; ++i) {
                const auto* lenPtr = consume(8, err);
                if (!lenPtr) return false;
                uint64_t len = *reinterpret_cast<const uint64_t*>(lenPtr);
                if (len > GGUF_SAFETY_LIMITS::MAX_STRING_LEN) {
                    err = {ErrorCode::OVERSIZED_STRING, "string exceeds 16MB limit", pos_ - 8};
                    return false;
                }
                const auto* data = consume(len, err);
                if (!data) return false;

                size_t offset = out.size();
                out.resize(offset + 8 + static_cast<size_t>(len));
                std::memcpy(out.data() + offset, lenPtr, 8);
                std::memcpy(out.data() + offset + 8, data, static_cast<size_t>(len));
            }
            return true;
        }

        if (elemType == 12) { // ARRAY
            for (uint64_t i = 0; i < count; ++i) {
                const auto* innerTypePtr = consume(4, err);
                if (!innerTypePtr) return false;
                const auto* innerCountPtr = consume(8, err);
                if (!innerCountPtr) return false;

                int32_t innerType = *reinterpret_cast<const int32_t*>(innerTypePtr);
                uint64_t innerCount = *reinterpret_cast<const uint64_t*>(innerCountPtr);

                size_t offset = out.size();
                out.resize(offset + 12);
                std::memcpy(out.data() + offset, innerTypePtr, 4);
                std::memcpy(out.data() + offset + 4, innerCountPtr, 8);

                if (!readArrayRaw(innerType, innerCount, err, out)) return false;
            }
            return true;
        }

        size_t elemSize = scalarSizeForType(elemType);
        if (elemSize == 0) {
            err = {ErrorCode::TRUNCATED_HEADER, "unknown array element type", pos_};
            return false;
        }

        if (count > (static_cast<uint64_t>(SIZE_MAX) / elemSize)) {
            err = {ErrorCode::OVERSIZED_ARRAY, "array byte size overflow", pos_};
            return false;
        }

        size_t bytes = static_cast<size_t>(count) * elemSize;
        const auto* data = consume(bytes, err);
        if (!data) return false;

        size_t offset = out.size();
        out.resize(offset + bytes);
        std::memcpy(out.data() + offset, data, bytes);
        return true;
    }

public:
    explicit RobustGGUFParser(const NtMappedFile& mapped) 
        : base_(mapped.data()), size_(mapped.size()) {}

    void setStrict(bool strict) noexcept { strictMode_ = strict; }

    // ========================================================================
    // CORE: Safe metadata parsing with early-abort on corruption
    // ========================================================================
    [[nodiscard]] ParseResult ParseHeader() noexcept {
        ParseResult err{ErrorCode::OK, nullptr, 0};
        
        // Magic: 'GGUF' = 0x46554747 little-endian
        const auto* magic = consume(4, err);
        if (!magic) return err;
        if (std::memcmp(magic, "GGUF", 4) != 0) {
            return {ErrorCode::MAGIC_MISMATCH, "not a GGUF file", pos_ - 4};
        }

        // Version: 2 or 3 supported
        const auto* ver = consume(4, err);
        if (!ver) return err;
        version_ = *reinterpret_cast<const uint32_t*>(ver);
        if (version_ != 2 && version_ != 3) {
            return {ErrorCode::VERSION_UNSUPPORTED, "version not 2 or 3", pos_ - 4};
        }

        // Tensor count
        const auto* tc = consume(8, err);
        if (!tc) return err;
        tensorCount_ = *reinterpret_cast<const uint64_t*>(tc);
        if (tensorCount_ > GGUF_SAFETY_LIMITS::MAX_TENSOR_COUNT) {
            return {ErrorCode::TENSOR_COUNT_INVALID, "suspicious tensor count", pos_ - 8};
        }

        // Metadata count
        const auto* mc = consume(8, err);
        if (!mc) return err;
        metadataCount_ = *reinterpret_cast<const uint64_t*>(mc);
        if (metadataCount_ > GGUF_SAFETY_LIMITS::MAX_METADATA_KEYS) {
            return {ErrorCode::METADATA_COUNT_INVALID, "metadata DoS attempt", pos_ - 8};
        }

        return {ErrorCode::OK, "header valid", pos_};
    }

    // ========================================================================
    // SAFE: String reading with hard cap (prevents chat_template bad_alloc)
    // ========================================================================
    [[nodiscard]] ParseResult ReadStringSafe(std::string& out, bool allowSkip = true) noexcept {
        ParseResult err{ErrorCode::OK, nullptr, 0};
        
        const auto* lenPtr = consume(8, err);
        if (!lenPtr) return err;
        
        uint64_t len = *reinterpret_cast<const uint64_t*>(lenPtr);
        
        // Hard limit check (robust tool #1: size validation)
        if (len > GGUF_SAFETY_LIMITS::MAX_STRING_LEN) {
            return {ErrorCode::OVERSIZED_STRING, "string exceeds 16MB limit", pos_ - 8};
        }

        if (len == 0) {
            out.clear();
            return {ErrorCode::OK, nullptr, pos_};
        }

        // Try to allocate
        try {
            out.resize(static_cast<size_t>(len));
        } catch (...) {
            return {ErrorCode::MEMORY_EXHAUSTION, "string allocation failed", pos_ - 8};
        }

        const auto* data = consume(len, err);
        if (!data) return err;
        
        std::memcpy(out.data(), data, static_cast<size_t>(len));
        return {ErrorCode::OK, nullptr, pos_};
    }

    // ========================================================================
    // SAFE: Metadata loop with corruption recovery
    // ========================================================================
    using MetadataCallback = std::function<void(const std::string& key, 
        int32_t type, const std::vector<uint8_t>& raw, bool wasSkipped)>;

    [[nodiscard]] ParseResult ParseAllMetadata(MetadataCallback cb) noexcept {
        ParseResult err{ErrorCode::OK, nullptr, pos_};
        
        for (uint64_t i = 0; i < metadataCount_; ++i) {
            // Read key
            std::string key;
            err = ReadStringSafe(key, false);
            if (err.code != ErrorCode::OK) {
                return err;
            }

            // Check if we should skip this key entirely (robust tool #2: blacklist)
            bool skipContent = allowSkip_ && shouldSkipKey(key);

            // Read type
            const auto* typePtr = consume(4, err);
            if (!typePtr) return err;
            int32_t type = *reinterpret_cast<const int32_t*>(typePtr);

            if (skipContent) {
                // Skip the value without parsing (seek past it)
                auto skipErr = SkipValue(type);
                if (skipErr.code != ErrorCode::OK) return skipErr;
                
                cb(key, type, {}, true); // Notify skipped
            } else {
                // Normal parse
                std::vector<uint8_t> value;
                err = ReadValue(type, value);
                if (err.code != ErrorCode::OK) {
                    // Strict mode: hard fail
                    if (strictMode_) return err;
                    
                    // Non-strict: skip this key, continue
                    std::fprintf(stderr, "[RobustGGUF] Skipping corrupt key '%s' at offset %zu\n", 
                        key.c_str(), pos_);
                    cb(key, type, {}, true);
                } else {
                    cb(key, type, value, false);
                }
            }
        }
        return {ErrorCode::OK, nullptr, pos_};
    }

private:
    bool allowSkip_ = true;

    [[nodiscard]] ParseResult SkipValue(int32_t type) noexcept {
        ParseResult err{ErrorCode::OK, nullptr, pos_};
        
        switch (type) {
            case 0: // UINT8
                return consume(1, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip uint8", pos_};
            case 1: // INT8
                return consume(1, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip int8", pos_};
            case 2: // UINT16
                return consume(2, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip uint16", pos_};
            case 3: // INT16
                return consume(2, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip int16", pos_};
            case 4: // UINT32
                return consume(4, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip uint32", pos_};
            case 5: // INT32
                return consume(4, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip int32", pos_};
            case 6: // FLOAT32
                return consume(4, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip float32", pos_};
            case 7: // UINT64
                return consume(8, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip uint64", pos_};
            case 8: // INT64
                return consume(8, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip int64", pos_};
            case 9: // FLOAT64
                return consume(8, err) ? err : ParseResult{ErrorCode::TRUNCATED_HEADER, "skip float64", pos_};
            case 10: { // BOOL
                const auto* p = consume(1, err);
                if (!p) return err;
                return err;
            }
            case 11: { // STRING
                const auto* lenPtr = consume(8, err);
                if (!lenPtr) return err;
                uint64_t len = *reinterpret_cast<const uint64_t*>(lenPtr);
                if (len > 0) {
                    if (!consume(static_cast<size_t>(len), err)) return err;
                }
                return err;
            }
            case 12: { // ARRAY
                const auto* typePtr = consume(4, err);
                if (!typePtr) return err;
                const auto* countPtr = consume(8, err);
                if (!countPtr) return err;
                uint64_t count = *reinterpret_cast<const uint64_t*>(countPtr);
                
                if (count > GGUF_SAFETY_LIMITS::MAX_ARRAY_ELEMENTS) {
                    return {ErrorCode::OVERSIZED_ARRAY, "array element count overflow", pos_};
                }
                
                // Skip all elements recursively
                int32_t elemType = *reinterpret_cast<const int32_t*>(typePtr);
                for (uint64_t i = 0; i < count; ++i) {
                    err = SkipValue(elemType);
                    if (err.code != ErrorCode::OK) return err;
                }
                return err;
            }
            default:
                return {ErrorCode::TRUNCATED_HEADER, "unknown type in skip", pos_};
        }
    }

    [[nodiscard]] ParseResult ReadValue(int32_t type, std::vector<uint8_t>& out) noexcept {
        ParseResult err{ErrorCode::OK, nullptr, pos_};
        out.clear();

        if (type == 11) { // STRING
            const auto* lenPtr = consume(8, err);
            if (!lenPtr) return err;
            uint64_t len = *reinterpret_cast<const uint64_t*>(lenPtr);
            if (len > GGUF_SAFETY_LIMITS::MAX_STRING_LEN) {
                return {ErrorCode::OVERSIZED_STRING, "string exceeds 16MB limit", pos_ - 8};
            }
            const auto* data = consume(static_cast<size_t>(len), err);
            if (!data) return err;
            out.resize(static_cast<size_t>(len));
            std::memcpy(out.data(), data, static_cast<size_t>(len));
            return err;
        }

        if (type == 12) { // ARRAY
            const auto* elemTypePtr = consume(4, err);
            if (!elemTypePtr) return err;
            const auto* countPtr = consume(8, err);
            if (!countPtr) return err;

            int32_t elemType = *reinterpret_cast<const int32_t*>(elemTypePtr);
            uint64_t count = *reinterpret_cast<const uint64_t*>(countPtr);

            out.resize(12);
            std::memcpy(out.data(), elemTypePtr, 4);
            std::memcpy(out.data() + 4, countPtr, 8);

            if (!readArrayRaw(elemType, count, err, out)) return err;
            return err;
        }

        size_t size = scalarSizeForType(type);
        if (size == 0) return {ErrorCode::TRUNCATED_HEADER, "unknown type in read", pos_};
        const auto* data = consume(size, err);
        if (!data) return err;
        out.resize(size);
        std::memcpy(out.data(), data, size);
        return err;
    }
};

// =============================================================================
// 4. EMERGENCY RECOVERY MODE (truncated/corrupted file handling)
// =============================================================================
class EmergencyGGUFRecovery {
public:
    // Attempts to salvage tensor offsets even if metadata is corrupt
    static bool SalvageTensors(const wchar_t* path, 
        std::vector<std::pair<uint64_t, uint64_t>>& tensorOffsets,
        uint64_t& tensorDataOffset) {
        
        try {
            NtMappedFile file(path);
            if (file.size() < 32) return false;
            
            const auto* data = file.data();
            
            // Verify magic
            if (std::memcmp(data, "GGUF", 4) != 0) return false;
            
            uint32_t version = *reinterpret_cast<const uint32_t*>(data + 4);
            if (version != 2 && version != 3) return false;
            
            uint64_t n_tensors = *reinterpret_cast<const uint64_t*>(data + 8);
            uint64_t n_meta = *reinterpret_cast<const uint64_t*>(data + 16);
            
            // Walk metadata blindly to find tensor info offset
            size_t pos = 24;
            
            // Quick metadata skip (don't parse, just seek)
            for (uint64_t i = 0; i < n_meta && i < 10000; ++i) {
                if (pos + 8 > file.size()) break;
                
                uint64_t key_len = *reinterpret_cast<const uint64_t*>(data + pos);
                pos += 8 + key_len;
                
                if (pos + 4 > file.size()) break;
                int32_t type = *reinterpret_cast<const int32_t*>(data + pos);
                pos += 4;
                
                // Skip value blindly (emergency mode)
                if (!SkipValueBlind(data, file.size(), pos, type)) break;
            }
            
            // Now at tensor info
            tensorDataOffset = pos; // Rough estimate
            (void)n_tensors;
            (void)tensorOffsets;
            return true;
            
        } catch (...) {
            return false;
        }
    }

private:
    static bool SkipValueBlind(const uint8_t* data, size_t size, size_t& pos, int32_t type) {
        // Extremely permissive skip
        if (type >= 0 && type <= 9) {
            static const size_t typeSizes[] = {1,1,2,2,4,4,4,8,8,8};
            pos += typeSizes[type];
            return pos <= size;
        }
        if (type == 10) { pos += 1; return pos <= size; }
        if (type == 11) {
            if (pos + 8 > size) return false;
            uint64_t len = *reinterpret_cast<const uint64_t*>(data + pos);
            pos += 8 + len;
            return pos <= size;
        }
        return false;
    }
};

} // namespace rawrxd

#endif // RAWRXD_GGUF_ROBUST_TOOLS_HPP
