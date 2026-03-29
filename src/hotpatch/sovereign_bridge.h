#pragma once

#include <cstdint>

#ifdef __cplusplus
#include <cstring>
#endif

static constexpr std::uint64_t RAWRXD_SOVEREIGN_MARKER = 0x1751431337ULL;
static constexpr std::uint32_t RAWRXD_WM_DIAGNOSTIC_MARKER = 0x0400u + 0x1337u;

#ifdef __cplusplus
extern "C" {
#endif

// Pure x64 MASM bridge. The ABI is integer-only so the assembly TU does not depend on
// mixed float/integer register conventions from C++ callers.
long long CheckAndNotify(std::uint64_t status, std::uint64_t confidenceBits,
                         void* hwnd, void* postMessageFn);

#ifdef __cplusplus
}

inline bool EmitSovereignMarker(void* viewHandle, float confidence, void* postMessageFn) {
    if (!viewHandle || !postMessageFn) {
        return false;
    }

    std::uint32_t confidenceBits32 = 0;
    std::memcpy(&confidenceBits32, &confidence, sizeof(confidence));
    return CheckAndNotify(RAWRXD_SOVEREIGN_MARKER,
                          static_cast<std::uint64_t>(confidenceBits32),
                          viewHandle, postMessageFn) != 0;
}
#endif