#include "byte_level_hotpatcher.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <algorithm>
#include <iostream>

// C++ fallback for find_pattern_asm when ASM module is not linked.
// The real SIMD Boyer-Moore version lives in src/asm/byte_search.asm
// and is used when building with ml64 (MASM64).
static const void* find_pattern_asm(const void* haystack, size_t haystack_len,
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
