// Minimal stubs to satisfy runtime_patcher dependencies for SwarmSmokeTest.
#include <cstdint>
#include <cstddef>

extern "C" {
void* __iat_hook_base[128] = {};
uint64_t __iat_hook_count = 128;
uint64_t masquerade_context[16] = {};

void* InstallIATHook(uint64_t slot, void* fn) {
    if (slot < __iat_hook_count) {
        void* prev = __iat_hook_base[slot];
        __iat_hook_base[slot] = fn;
        return prev;
    }
    return nullptr;
}

void* GetIATHook(uint64_t slot) {
    if (slot < __iat_hook_count) return __iat_hook_base[slot];
    return nullptr;
}

uint64_t GetMasqueradeStats() { return 0; }

void* g_hHeap = nullptr;
void* g_hInstance = nullptr;
int RawrXD_GlobalsInit(void*, void*) { return 0; }
}
