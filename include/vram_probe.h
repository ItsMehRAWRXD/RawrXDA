#pragma once

#include <cstdint>

namespace RawrXD {

struct VRAMInfo {
    uint64_t dedicated_total = 0;
    uint64_t dedicated_free = 0;
    uint64_t shared_total = 0;
    uint64_t shared_free = 0;
    bool valid = false;
};

// Best-effort VRAM query. Uses DXGI on Windows and returns invalid elsewhere.
VRAMInfo QueryVRAM();

} // namespace RawrXD
