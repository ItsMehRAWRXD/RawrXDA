#include "model_memory_hotpatch.hpp"
#include <windows.h>
#include <iostream>

extern "C" int apply_memory_patch_asm(void* addr, size_t size, const void* data);

PatchResult apply_memory_patch(void* addr, size_t size, const void* data) {
    if (apply_memory_patch_asm(addr, size, data)) {
        return PatchResult::ok("Applied via MASM");
    }
    return PatchResult::error("ASM patch failed", GetLastError());
}

PatchResult revert_memory_patch(void* addr, size_t size, const void* original_data) {
    return apply_memory_patch(addr, size, original_data);
}
