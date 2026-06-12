// Win32IDE_Stubs.h — Runtime stubs for missing symbols in Win32IDE.cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Pulse ring-buffer cycle counter (stub)
inline uint64_t PulseGetCycles() {
    return 0;
}

// GhostCompletionContext extension stubs
namespace rawrxd {
namespace ghost_completion {

struct GhostCompletionContext;

inline std::string toPromptFragment(const GhostCompletionContext& /*ctx*/, size_t /*maxLen*/) {
    return "";
}

} // namespace ghost_completion
} // namespace rawrxd

// RawrXD::UI VCS drain stub
namespace RawrXD {
namespace UI {

inline void drainPendingVcsIndexSnapshots(void* /*hwnd*/ = nullptr) {}

} // namespace UI
} // namespace RawrXD
