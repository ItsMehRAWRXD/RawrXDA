#include "byte_level_hotpatcher.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstring>
#include <algorithm>
#include <iostream>

// C++ fallback for find_pattern_asm when ASM module is not linked.
// The real SIMD Boyer-Moore version lives in src/asm/byte_search.asm
// and is used when building with ml64 (MASM64).
#ifdef _MSC_VER
// With MSVC, the real ASM object is linked — just declare extern
extern "C" const void* find_pattern_asm(const void* haystack, size_t haystack_len,
                                         const void* needle, size_t needle_len);
#else
extern "C" const void* find_pattern_asm(const void* haystack, size_t haystack_len,
                                         const void* needle, size_t needle_len) {
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return nullptr;
    }
    const uint8_t* h = static_cast<const uint8_t*>(haystack);
    const uint8_t* n = static_cast<const uint8_t*>(needle);
    for (size_t i = 0; i + needle_len <= haystack_len; ++i) {
        if (std::memcmp(h + i, n, needle_len) == 0) {
            return h + i;
        }
    }
    return nullptr;
}
#endif

PatchResult patch_bytes(const char* filename, const BytePatch& patch) {
    HANDLE hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return PatchResult::error("Could not get file size", GetLastError());
    }

    if (patch.offset + patch.data.size() > (size_t)fileSize.QuadPart) {
        CloseHandle(hFile);
        return PatchResult::error("Patch offset out of bounds", 2);
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return PatchResult::error("CreateFileMapping failed", GetLastError());
    }

    void* base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("MapViewOfFile failed", GetLastError());
    }

    std::memcpy((uint8_t*)base + patch.offset, patch.data.data(), patch.data.size());

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return PatchResult::ok();
}

PatchResult search_and_patch_bytes(const char* filename, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement) {
    if (pattern.empty() || replacement.empty() || pattern.size() != replacement.size()) {
        return PatchResult::error("Invalid pattern or replacement (must be non-empty and same size)", 3);
    }

    HANDLE hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file", GetLastError());
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    void* base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("Mapping failed", GetLastError());
    }

    uint8_t* ptr = (uint8_t*)base;
    size_t limit = (size_t)fileSize.QuadPart - pattern.size();
    bool found = false;

    for (size_t i = 0; i <= limit; ++i) {
        if (std::memcmp(ptr + i, pattern.data(), pattern.size()) == 0) {
            std::memcpy(ptr + i, replacement.data(), replacement.size());
            found = true;
            break; 
        }
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return found ? PatchResult::ok("Pattern found and replaced") : PatchResult::error("Pattern not found", 4);
}

PatchResult patch_bytes_mem(const char* filename, const uint8_t* pattern, size_t pattern_len, const uint8_t* replacement, size_t replacement_len) {
    HANDLE hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return PatchResult::error("Could not get file size", GetLastError());
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    void* base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("Mapping failed", GetLastError());
    }

    const void* found = find_pattern_asm(base, fileSize.QuadPart, pattern, pattern_len);
    if (found) {
        std::memcpy((void*)found, replacement, replacement_len);
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return PatchResult::ok("Byte patch applied via MASM");
}

// ============================================================================
// direct_read — Read bytes from a GGUF file at a specific offset
// Uses CreateFileMapping for zero-copy I/O.
// ============================================================================
PatchResult direct_read(const char* filename, size_t offset, size_t len,
                        void* outBuffer, size_t* outBytesRead) {
    if (!filename || !outBuffer || len == 0) {
        return PatchResult::error("Invalid parameters for direct_read", 1);
    }

    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file for reading", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return PatchResult::error("Could not get file size", GetLastError());
    }

    if (offset >= (size_t)fileSize.QuadPart) {
        CloseHandle(hFile);
        if (outBytesRead) *outBytesRead = 0;
        return PatchResult::error("Read offset beyond file size", 2);
    }

    // Clamp read length to remaining file size
    size_t available = (size_t)fileSize.QuadPart - offset;
    size_t readLen = (len < available) ? len : available;

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return PatchResult::error("CreateFileMapping failed", GetLastError());
    }

    const void* base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("MapViewOfFile failed", GetLastError());
    }

    std::memcpy(outBuffer, static_cast<const uint8_t*>(base) + offset, readLen);
    if (outBytesRead) *outBytesRead = readLen;

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return PatchResult::ok("Direct read complete");
}

// ============================================================================
// direct_write — Write bytes to a GGUF file at a specific offset
// Uses CreateFileMapping for zero-copy I/O. Bounds-checked.
// ============================================================================
PatchResult direct_write(const char* filename, size_t offset,
                         const void* data, size_t len) {
    if (!filename || !data || len == 0) {
        return PatchResult::error("Invalid parameters for direct_write", 1);
    }

    HANDLE hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file for writing", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return PatchResult::error("Could not get file size", GetLastError());
    }

    if (offset + len > (size_t)fileSize.QuadPart) {
        CloseHandle(hFile);
        return PatchResult::error("Write extends beyond file bounds", 2);
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return PatchResult::error("CreateFileMapping failed", GetLastError());
    }

    void* base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("MapViewOfFile failed", GetLastError());
    }

    std::memcpy(static_cast<uint8_t*>(base) + offset, data, len);

    // Flush to ensure writes hit disk
    FlushViewOfFile(static_cast<uint8_t*>(base) + offset, len);

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return PatchResult::ok("Direct write complete");
}

// ============================================================================
// direct_search — Search for a byte pattern in a GGUF file
// Uses mmap + SIMD-accelerated pattern search when available.
// ============================================================================
ByteSearchResult direct_search(const char* filename,
                               const uint8_t* pattern, size_t pattern_len) {
    if (!filename || !pattern || pattern_len == 0) {
        return ByteSearchResult::miss();
    }

    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return ByteSearchResult::miss();
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < (LONGLONG)pattern_len) {
        CloseHandle(hFile);
        return ByteSearchResult::miss();
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return ByteSearchResult::miss();
    }

    const void* base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return ByteSearchResult::miss();
    }

    // Use the SIMD-accelerated ASM pattern search if available
    const void* found = find_pattern_asm(base, (size_t)fileSize.QuadPart,
                                          pattern, pattern_len);
    ByteSearchResult result;
    if (found) {
        size_t offset = static_cast<const uint8_t*>(found) - static_cast<const uint8_t*>(base);
        result = ByteSearchResult::hit(offset, pattern_len);
    } else {
        result = ByteSearchResult::miss();
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return result;
}

// ============================================================================
// apply_byte_mutation — Apply an atomic byte mutation at a file offset
// Supports XOR, rotate, swap, and reverse operations.
// ============================================================================
PatchResult apply_byte_mutation(const char* filename, size_t offset,
                                size_t len, ByteMutation mutation,
                                uint8_t param) {
    if (!filename || len == 0) {
        return PatchResult::error("Invalid parameters for byte mutation", 1);
    }

    HANDLE hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Could not open file for mutation", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return PatchResult::error("Could not get file size", GetLastError());
    }

    if (offset + len > (size_t)fileSize.QuadPart) {
        CloseHandle(hFile);
        return PatchResult::error("Mutation range extends beyond file bounds", 2);
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return PatchResult::error("CreateFileMapping failed", GetLastError());
    }

    void* base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return PatchResult::error("MapViewOfFile failed", GetLastError());
    }

    uint8_t* target = static_cast<uint8_t*>(base) + offset;

    switch (mutation) {
        case ByteMutation::XOR:
            // XOR each byte with the param value
            for (size_t i = 0; i < len; ++i) {
                target[i] ^= param;
            }
            break;

        case ByteMutation::Rotate:
            // Rotate each byte left by (param % 8) bits
            {
                uint8_t shift = param % 8;
                for (size_t i = 0; i < len; ++i) {
                    target[i] = (target[i] << shift) | (target[i] >> (8 - shift));
                }
            }
            break;

        case ByteMutation::Swap:
            // Pairwise byte swap (swap adjacent bytes)
            for (size_t i = 0; i + 1 < len; i += 2) {
                uint8_t tmp = target[i];
                target[i] = target[i + 1];
                target[i + 1] = tmp;
            }
            break;

        case ByteMutation::Reverse:
            // Reverse all bytes in the range
            for (size_t i = 0; i < len / 2; ++i) {
                uint8_t tmp = target[i];
                target[i] = target[len - 1 - i];
                target[len - 1 - i] = tmp;
            }
            break;

        default:
            UnmapViewOfFile(base);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return PatchResult::error("Unknown mutation type", 3);
    }

    FlushViewOfFile(target, len);

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    static const char* mutationNames[] = { "XOR", "Rotate", "Swap", "Reverse" };
    char msg[128];
    snprintf(msg, sizeof(msg), "Byte mutation %s applied (%zu bytes at offset %zu)",
             mutationNames[static_cast<int>(mutation)], len, offset);
    return PatchResult::ok(msg);
}
