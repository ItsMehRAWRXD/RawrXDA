#include "model_memory_hotpatch.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>

PatchResult apply_memory_patch(void* addr, size_t size, const void* data) {
    if (!addr || !data || size == 0) {
        return PatchResult::error("Invalid parameters", 1);
    }

    DWORD oldProtect;
    if (!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return PatchResult::error("VirtualProtect failed", GetLastError());
    }

    std::memcpy(addr, data, size);

    DWORD temp;
    if (!VirtualProtect(addr, size, oldProtect, &temp)) {
        // Even if we fail to restoral, the patch is applied.
        return PatchResult::ok("Applied but failed to restore memory protection");
    }

    return PatchResult::ok();
}

PatchResult revert_memory_patch(void* addr, size_t size, const void* original_data) {
    return apply_memory_patch(addr, size, original_data);
}
