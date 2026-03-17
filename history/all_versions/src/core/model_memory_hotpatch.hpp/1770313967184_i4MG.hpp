#pragma once
#include <cstdint>
#include <cstddef>

struct PatchResult {
    bool success;
    const char* detail;
    int errorCode;

    static PatchResult ok(const char* detail = "Applied") {
        return {true, detail, 0};
    }
    
    static PatchResult error(const char* detail, int code = -1) {
        return {false, detail, code};
    }
};

/**
 * Memory Layer Hotpatching
 * Direct RAM patching using OS protection APIs.
 */
PatchResult apply_memory_patch(void* addr, size_t size, const void* data);
PatchResult revert_memory_patch(void* addr, size_t size, const void* original_data);
