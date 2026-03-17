#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// =============================================================================
// RawrXD::GGUF::Robust Tools v1.0
// Zero-allocation defensive parsing, MMF-based access, and corruption heuristics
// =============================================================================

namespace RawrXD::GGUF::Robust {

// -----------------------------------------------------------------------------
// 1. SafeStringParser - Zero-allocation length validation
//    Protects against oversized or corrupt string declarations.
// -----------------------------------------------------------------------------
class SafeStringParser {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    bool ownsHandle_ = false;

public:
    struct StringView {
        uint64_t fileOffset;    // Where string data begins
        uint64_t length;        // Declared length from GGUF
        bool isValid;           // Length sanity check passed
        uint32_t crc32;         // Reserved for future hashing
    };

    explicit SafeStringParser(HANDLE fileHandle, bool takeOwnership = false)
        : hFile_(fileHandle), ownsHandle_(takeOwnership) {}

    ~SafeStringParser() {
        if (ownsHandle_ && hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
        }
    }

    std::optional<uint64_t> PeekLength(int64_t offset) const {
        if (hFile_ == INVALID_HANDLE_VALUE || offset < 0) return std::nullopt;

        uint64_t len = 0;
        DWORD read = 0;
        OVERLAPPED ov{};
        ov.Offset = static_cast<DWORD>(offset);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

        if (!ReadFile(hFile_, &len, sizeof(len), &read, &ov) || read != sizeof(len)) {
            return std::nullopt;
        }
        return len;
    }

    bool ValidateString(uint64_t declaredLen, uint64_t maxSafeLen = 0x1000000ULL) const {
        if (declaredLen == 0xFFFFFFFFFFFFFFFFULL) return false;
        if (declaredLen == 0xFFFFFFFFULL) return false;
        if (declaredLen > maxSafeLen) return false;
        if (declaredLen > (1ULL << 40)) return false; // >1TB is impossible for GGUF
        return true;
    }

    bool SkipString(int64_t currentOffset, uint64_t strLen) {
        if (hFile_ == INVALID_HANDLE_VALUE || currentOffset < 0) return false;
        LARGE_INTEGER newPos{};
        newPos.QuadPart = currentOffset + static_cast<int64_t>(sizeof(uint64_t)) + static_cast<int64_t>(strLen);
        return SetFilePointerEx(hFile_, newPos, nullptr, FILE_BEGIN) != 0;
    }

    bool PeekHeader(const StringView& view, char* buffer, size_t bufSize) {
        if (!view.isValid || !buffer || bufSize == 0 || hFile_ == INVALID_HANDLE_VALUE) return false;

        DWORD toRead = static_cast<DWORD>(std::min(view.length, static_cast<uint64_t>(bufSize)));
        DWORD read = 0;
        OVERLAPPED ov{};
        ov.Offset = static_cast<DWORD>(view.fileOffset);
        ov.OffsetHigh = static_cast<DWORD>(view.fileOffset >> 32);

        return ReadFile(hFile_, buffer, toRead, &read, &ov) && (read == toRead);
    }
};

// -----------------------------------------------------------------------------
// 2. LazyMemoryMapper - Windows MMF helper for large models
// -----------------------------------------------------------------------------
class LazyMemoryMapper {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
    void* baseAddr_ = nullptr;
    size_t mapSize_ = 0;

public:
    struct MappingResult {
        void* baseAddress;
        size_t granularity;
        bool isLargePage;
        uint32_t numaNode;
    };

    bool Open(const wchar_t* path, bool enableLargePages = false) {
        hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                             nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) return false;

        DWORD highSize = 0;
        DWORD lowSize = GetFileSize(hFile_, &highSize);
        mapSize_ = (static_cast<ULONGLONG>(highSize) << 32) | lowSize;

        DWORD protect = enableLargePages ? PAGE_EXECUTE_READ | SEC_LARGE_PAGES : PAGE_READONLY;
        hMapping_ = CreateFileMappingW(hFile_, nullptr, protect, highSize, lowSize, nullptr);
        if (!hMapping_) {
            hMapping_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, highSize, lowSize, nullptr);
        }
        if (!hMapping_) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
            return false;
        }

        baseAddr_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
        return baseAddr_ != nullptr;
    }

    void PrefetchRange(uint64_t offset, size_t size) const {
        if (!baseAddr_) return;
        const char* addr = static_cast<const char*>(baseAddr_) + offset;
        WIN32_MEMORY_RANGE_ENTRY entry{};
        entry.VirtualAddress = const_cast<char*>(addr);
        entry.NumberOfBytes = size;
        PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
    }

    void SecureUnmap() {
        if (baseAddr_) {
            UnmapViewOfFile(baseAddr_);
            baseAddr_ = nullptr;
        }
        if (hMapping_) {
            CloseHandle(hMapping_);
            hMapping_ = nullptr;
        }
        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
    }

    void* GetBase() const { return baseAddr_; }
    size_t GetSize() const { return mapSize_; }

    template<typename T>
    T* GetTensorPtr(uint64_t offset) const {
        if (!baseAddr_ || offset >= mapSize_) return nullptr;
        return reinterpret_cast<T*>(static_cast<char*>(baseAddr_) + offset);
    }
};

// -----------------------------------------------------------------------------
// 3. MetadataSanitizer - Corruption detection and mitigation
// -----------------------------------------------------------------------------
class MetadataSanitizer {
public:
    struct ThreatReport {
        bool isCorrupted = false;
        bool hasOversizedTemplate = false;
        bool hasInvalidMerges = false;
        uint64_t templateSize = 0;
        uint64_t mergesCount = 0;
        std::string detectedFamily; // "llama", "qwen", "phi", etc.
    };

    static ThreatReport ScanMetadata(const std::unordered_map<std::string, std::string>& meta) {
        ThreatReport report{};

        auto it = meta.find("tokenizer.chat_template");
        if (it != meta.end()) {
            const auto& val = it->second;
            report.templateSize = val.size();

            if (val.size() > 1024 * 1024) {
                report.hasOversizedTemplate = true;
                report.isCorrupted = true;
            }

            if (val.find("<|im_start|>") != std::string::npos) {
                report.detectedFamily = "qwen";
            } else if (val.find("<|user|>") != std::string::npos) {
                report.detectedFamily = "llama-3";
            } else if (val.find("{{ messages }}") != std::string::npos) {
                report.detectedFamily = "generic";
            }
        }

        auto merges = meta.find("tokenizer.ggml.merges");
        if (merges != meta.end() && merges->second.size() > 100000) {
            report.hasInvalidMerges = true;
        }

        return report;
    }

    static bool SanitizeForInference(std::unordered_map<std::string, std::string>& meta) {
        bool modified = false;
        static const char* dangerousKeys[] = {
            "tokenizer.chat_template",
            "tokenizer.ggml.merges",
            "tokenizer.ggml.tokens",
            "tokenizer.ggml.scores",
            nullptr
        };

        for (const char** key = dangerousKeys; *key; ++key) {
            auto it = meta.find(*key);
            if (it != meta.end() && it->second.size() > 65536) {
                it->second = "[sanitized-" + std::to_string(it->second.size()) + "]";
                modified = true;
            }
        }
        return modified;
    }
};

// -----------------------------------------------------------------------------
// 4. StreamCursor - File navigation with bounds checking
// -----------------------------------------------------------------------------
class StreamCursor {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    int64_t savedPos_ = 0;

public:
    explicit StreamCursor(HANDLE hFile) : hFile_(hFile) {}

    bool Push() {
        if (hFile_ == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER zero{};
        LARGE_INTEGER pos{};
        if (!SetFilePointerEx(hFile_, zero, &pos, FILE_CURRENT)) return false;
        savedPos_ = pos.QuadPart;
        return true;
    }

    bool Pop() {
        if (hFile_ == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER pos{};
        pos.QuadPart = savedPos_;
        return SetFilePointerEx(hFile_, pos, nullptr, FILE_BEGIN) != 0;
    }

    bool SafeSeek(int64_t offset) {
        if (hFile_ == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(hFile_, &fileSize)) return false;
        if (offset > fileSize.QuadPart || offset < 0) return false;

        LARGE_INTEGER pos{};
        pos.QuadPart = offset;
        return SetFilePointerEx(hFile_, pos, nullptr, FILE_BEGIN) != 0;
    }

    bool AlignedRead(void* buffer, DWORD size, int64_t offset, DWORD alignment = 64) {
        if (hFile_ == INVALID_HANDLE_VALUE) return false;
        if (offset % alignment != 0) {
            std::vector<char> temp(size + alignment);
            void* alignedBuf = reinterpret_cast<void*>(
                (reinterpret_cast<uintptr_t>(temp.data()) + alignment - 1) & ~(alignment - 1)
            );

            if (!ReadAt(alignedBuf, size, offset)) return false;
            std::memcpy(buffer, alignedBuf, size);
            return true;
        }
        return ReadAt(buffer, size, offset);
    }

private:
    bool ReadAt(void* buffer, DWORD size, int64_t offset) {
        OVERLAPPED ov{};
        ov.Offset = static_cast<DWORD>(offset);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD read = 0;
        return ReadFile(hFile_, buffer, size, &read, &ov) && (read == size);
    }
};

} // namespace RawrXD::GGUF::Robust
