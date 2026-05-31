// d:/rawrxd/src/gpu_enforcement.h
// =============================================================================
// RawrXD GPU Enforcement Gate
// =============================================================================
// Process-wide, no-toggle GPU requirement. Every IDE/CLI/Model entry point
// MUST call rxd_gpu_require() at startup. If no GPU backend is available,
// the process is terminated. There is no env-var, setting, or runtime flag
// that can disable this gate.
//
// Honored backends (in priority order): Vulkan, CUDA, HIP/ROCm.
// =============================================================================
#pragma once

#include <cstdint>

namespace rxd::gpu {

enum class Backend : int {
    None    = 0,
    Vulkan  = 1,
    Cuda    = 2,
    Hip     = 3,
};

struct Status {
    Backend     active;
    int         device_count;
    char        device_name[256];
    uint64_t    vram_total_bytes;
    uint64_t    vram_free_bytes;
};

// Initialize and lock GPU. Safe to call multiple times; only the first call
// performs detection. If no GPU backend is available, this function calls
// std::abort() with a fatal log line. There is intentionally no fallback.
void require();

// Returns the cached status. Calls require() if not yet initialized.
const Status& status();

// Returns true once require() has succeeded. Never returns true without a GPU.
bool active();

// Hard-locked layer count for any local model loader. Always 999.
inline int forced_n_gpu_layers() { return 999; }

// Reject any caller-supplied "n_gpu_layers" / "use_gpu" override. Returns the
// forced value regardless of input. Logs the override attempt.
int sanitize_n_gpu_layers(int requested);
bool sanitize_use_gpu(bool requested);

} // namespace rxd::gpu
