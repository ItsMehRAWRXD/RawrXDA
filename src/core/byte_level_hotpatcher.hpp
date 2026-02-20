// byte_level_hotpatcher.hpp — Byte-Level Hotpatching (Layer 2)
// Precision GGUF binary modification without full reparse.
// Uses pattern search (Boyer-Moore or SIMD scan) and mmap for direct I/O.
//
// Rule: Use mmap / CreateFileMapping
// Rule: Never load whole file unless required
// Rule: All writes must be bounds-checked
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// BytePatch — Describes a single byte-level patch operation
// ---------------------------------------------------------------------------
struct BytePatch {
    size_t                  offset;         // File offset to patch
    std::vector<uint8_t>    data;           // Replacement bytes
    std::vector<uint8_t>    original;       // Original bytes (for rollback)
    std::string             description;    // Human-readable description
};

// ---------------------------------------------------------------------------
// ByteSearchResult — Result of a pattern search
// ---------------------------------------------------------------------------
struct ByteSearchResult {
    bool        found;
    size_t      offset;         // File offset where pattern was found
    size_t      length;         // Length of matched pattern

    static ByteSearchResult hit(size_t off, size_t len) {
        ByteSearchResult r;
        r.found  = true;
        r.offset = off;
        r.length = len;
        return r;
    }

    static ByteSearchResult miss() {
        ByteSearchResult r;
        r.found  = false;
        r.offset = 0;
        r.length = 0;
        return r;
    }
};

// ---------------------------------------------------------------------------
// Atomic mutation types for byte-level operations
// ---------------------------------------------------------------------------
enum class ByteMutation : uint8_t {
    XOR     = 0,
    Rotate  = 1,
    Swap    = 2,
    Reverse = 3,
};

// ---------------------------------------------------------------------------
// External ASM entry points (byte_search.asm)
// ---------------------------------------------------------------------------
extern "C" {
    // SIMD-accelerated pattern search in memory-mapped file region
    const void* asm_byte_search(const void* haystack, size_t haystack_len,
                                const void* needle, size_t needle_len);
    // Boyer-Moore search with precomputed skip table
    const void* asm_boyer_moore_search(const void* haystack, size_t haystack_len,
                                       const void* needle, size_t needle_len,
                                       const int* skip_table);
}

// Already declared in byte_level_hotpatcher.cpp's extern — keep consistent
extern "C" const void* find_pattern_asm(const void* haystack, size_t haystack_len,
                                        const void* needle, size_t needle_len);

// ---------------------------------------------------------------------------
// Core API functions
// ---------------------------------------------------------------------------

// Apply a byte-level patch to a GGUF file at a specific offset.
// Uses CreateFileMapping for zero-copy I/O.
PatchResult patch_bytes(const char* filename, const BytePatch& patch);

// Search for a pattern and replace it in a GGUF file.
// Pattern and replacement must be the same size.
PatchResult search_and_patch_bytes(const char* filename,
                                   const std::vector<uint8_t>& pattern,
                                   const std::vector<uint8_t>& replacement);

// C-style overload for ASM interop (pattern + replacement as raw pointers).
PatchResult patch_bytes_mem(const char* filename,
                            const uint8_t* pattern, size_t pattern_len,
                            const uint8_t* replacement, size_t replacement_len);

// Direct read from a GGUF file at offset, returns bytes read.
PatchResult direct_read(const char* filename, size_t offset, size_t len,
                        void* outBuffer, size_t* outBytesRead);

// Direct write to a GGUF file at offset.
PatchResult direct_write(const char* filename, size_t offset,
                         const void* data, size_t len);

// Search for a byte pattern in a GGUF file.
ByteSearchResult direct_search(const char* filename,
                               const uint8_t* pattern, size_t pattern_len);

// Apply an atomic byte mutation (XOR, rotate, swap, reverse).
PatchResult apply_byte_mutation(const char* filename, size_t offset,
                                size_t len, ByteMutation mutation,
                                uint8_t param);
