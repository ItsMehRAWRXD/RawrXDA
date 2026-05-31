// gpu_dispatch.hpp
// Minimal Vulkan dispatch wrapper for FP8 quantizer compute shader.
// Qt-free, zero dependencies beyond Vulkan headers.

#pragma once
#include <cstdint>
#include <vector>
#include <string_view>

namespace rawrxd::gpu {

struct DispatchResult {
    bool     ok{false};
    double   elapsed_ms{0.0};
    uint64_t bytes_moved{0};
    const char* error_msg{nullptr};
};

class Fp8QuantizerDispatch {
public:
    // Initialize Vulkan device and load SPIR-V shader from disk
    bool initialize(std::string_view spv_path);

    // Dispatch FP8 quantization on GPU
    // src: host float array, dst: host uint8_t output array
    DispatchResult dispatch(const float* src, uint8_t* dst, size_t n, float scale);

    // Cleanup Vulkan resources
    void shutdown();

    ~Fp8QuantizerDispatch() { shutdown(); }

private:
    struct Impl;
    Impl* m_impl{nullptr};
};

} // namespace rawrxd::gpu
