#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

namespace RawrXD {

enum class ComputeBackend {
    CPU_ONLY,
    GPU_ONLY,
    AUTO_STRICT
};

struct PreflightResult {
    bool passed = false;
    std::string selected_backend;
    std::string message;

    static PreflightResult fail(const std::string& msg) {
        return { false, "NONE", msg };
    }

    static PreflightResult success(const std::string& backend, const std::string& msg) {
        return { true, backend, msg };
    }
};

class GGUFPreflightGuard {
public:
    static PreflightResult executeStrictPreflight(
        const std::string& model_path,
        ComputeBackend requested_backend,
        uint64_t estimated_vram_required = 0);

    static bool validateQuantizationSupport(uint32_t ggml_type);

private:
    static bool validateFileAccessAndMagic(const std::string& path, std::string& out_error);
    static bool validateMemoryHeadroom(ComputeBackend backend, uint64_t vram_required, std::string& out_error);
    static bool validateGGUFHeader(HANDLE hFile, std::string& out_error);
};

} // namespace RawrXD
