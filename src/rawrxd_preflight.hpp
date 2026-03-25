#pragma once
// rawrxd_preflight.hpp
// Stone MASM64 hardware preflight interface for RawrXD.
//
// Provides concrete, instruction-level hardware probes:
//   rawrxd_has_avx512        — CPUID leaf 7 + XGETBV OS-save check
//   rawrxd_has_vulkan        — LoadLibraryA probe for vulkan-1.dll
//   rawrxd_preflight_lock    — VirtualLock dry-run (64 KB arena)
//
// All functions return 1 (true) or 0 (false), matching bool ABI on MSVC x64.
// Guard with RAWR_ENABLE_STONE_PREFLIGHT before including.

#ifdef __cplusplus
extern "C" {
#endif

/// Returns 1 if AVX-512F is both CPU-present and OS-enabled (XGETBV ZMM lanes active).
int rawrxd_has_avx512(void);

/// Returns 1 if the Vulkan ICD (vulkan-1.dll) is loadable on this system.
/// Performs LoadLibraryA + FreeLibrary probe; no Vulkan context is created.
int rawrxd_has_vulkan(void);

/// Returns 1 if VirtualLock succeeds on a 64 KB probe page.
/// Confirms the process working-set policy allows arena pinning.
int rawrxd_preflight_lock(void);

#ifdef __cplusplus
}

// Inline C++ shim: run all three stone probes and return summary.
// All three checks are independent; both GPU and CPU paths must be validated.
#include <cstdint>
struct StonePreflight {
    bool avx512_ok  = false;
    bool vulkan_ok  = false;
    bool lockable   = false;
};
inline StonePreflight rawrxd_preflight_execute() noexcept {
    StonePreflight r{};
    r.avx512_ok = rawrxd_has_avx512()     != 0;
    r.vulkan_ok = rawrxd_has_vulkan()     != 0;
    r.lockable  = rawrxd_preflight_lock() != 0;
    return r;
}
#endif // __cplusplus
