#pragma once

#include <string>

namespace RawrXD::Preflight {

struct BootPreflightResult {
    bool ok = false;
    bool gpu_capable = false;
    bool avx512_capable = false;
    bool pages_lockable = false;
    std::string detail;
};

// Executes a one-time hardware preflight latch for process startup.
// This runs before GUI creation to lock runtime assumptions early.
BootPreflightResult RunBootPreflightLatch();

} // namespace RawrXD::Preflight
