#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "gguf_preflight_guard.hpp"

namespace RawrXD {

constexpr uint32_t GGML_TYPE_F32 = 0;
constexpr uint32_t GGML_TYPE_F16 = 1;
constexpr uint32_t GGML_TYPE_Q4_0 = 2;
constexpr uint32_t GGML_TYPE_Q8_0 = 8;
constexpr uint32_t GGML_TYPE_Q2_K = 10;
constexpr uint32_t GGML_TYPE_Q3_K = 11;
constexpr uint32_t GGML_TYPE_Q4_K = 12;
constexpr uint32_t GGML_TYPE_Q5_K = 13;
constexpr uint32_t GGML_TYPE_Q6_K = 14;

static bool endsWithCaseInsensitive(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) {
        return false;
    }
    const size_t start = s.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = s[start + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

static uint64_t queryDedicatedVramBytes() {
    IDXGIFactory6* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&factory))) || !factory) {
        return 0;
    }

    uint64_t bestBytes = 0;
    for (UINT idx = 0;; ++idx) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(idx, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (!adapter) {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            const bool software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (!software && desc.DedicatedVideoMemory > bestBytes) {
                bestBytes = static_cast<uint64_t>(desc.DedicatedVideoMemory);
            }
        }
        adapter->Release();
    }

    factory->Release();
    return bestBytes;
}

PreflightResult GGUFPreflightGuard::executeStrictPreflight(
    const std::string& model_path,
    ComputeBackend requested_backend,
    uint64_t estimated_vram_required) {
    std::string error_msg;

    if (!validateFileAccessAndMagic(model_path, error_msg)) {
        return PreflightResult::fail(error_msg);
    }

    if (!validateMemoryHeadroom(requested_backend, estimated_vram_required, error_msg)) {
        return PreflightResult::fail(error_msg);
    }

    std::string bound_backend = "UNKNOWN";
    std::ostringstream success_log;

    if (requested_backend == ComputeBackend::CPU_ONLY) {
        bound_backend = "CPU";
        success_log << "Backend explicitly bound to CPU. Preflight passed.";
    } else if (requested_backend == ComputeBackend::GPU_ONLY) {
        if (estimated_vram_required == 0) {
            return PreflightResult::fail("GPU_ONLY requested but VRAM requirement is uncalculated.");
        }
        bound_backend = "VULKAN_GPU";
        success_log << "Backend explicitly bound to VULKAN_GPU. VRAM verified with 15% safety margin.";
    } else {
        if (validateMemoryHeadroom(ComputeBackend::GPU_ONLY, estimated_vram_required, error_msg)) {
            bound_backend = "VULKAN_GPU";
            success_log << "AUTO_STRICT bound to VULKAN_GPU. Hardware capabilities verified.";
        } else {
            bound_backend = "CPU";
            success_log << "AUTO_STRICT bound to CPU. Reason: " << error_msg;
        }
    }

    std::cout << "[Preflight] ACTIVE BACKEND: " << bound_backend << "\n"
              << "[Preflight] STATUS: " << success_log.str() << std::endl;

    return PreflightResult::success(bound_backend, success_log.str());
}

bool GGUFPreflightGuard::validateFileAccessAndMagic(const std::string& path, std::string& out_error) {
    if (!endsWithCaseInsensitive(path, ".gguf")) {
        out_error = "Only .gguf model files are accepted.";
        return false;
    }

    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            out_error = "Permission denied for runtime user. Access check failed.";
        } else if (err == ERROR_FILE_NOT_FOUND) {
            out_error = "Target model file does not exist.";
        } else {
            out_error = "Failed to obtain read handle. Win32 Error: " + std::to_string(err);
        }
        return false;
    }

    const bool is_valid = validateGGUFHeader(hFile, out_error);
    CloseHandle(hFile);
    return is_valid;
}

bool GGUFPreflightGuard::validateGGUFHeader(HANDLE hFile, std::string& out_error) {
    uint32_t magic = 0;
    uint32_t version = 0;
    DWORD bytesRead = 0;

    if (!ReadFile(hFile, &magic, sizeof(magic), &bytesRead, nullptr) || bytesRead != sizeof(magic)) {
        out_error = "Failed to read file signature.";
        return false;
    }

    if (magic != 0x46554747) {
        out_error = "Non-GGUF format rejected. Invalid magic bytes.";
        return false;
    }

    if (!ReadFile(hFile, &version, sizeof(version), &bytesRead, nullptr) || bytesRead != sizeof(version)) {
        out_error = "Failed to read GGUF version.";
        return false;
    }

    if (version < 2 || version > 3) {
        out_error = "Unsupported GGUF version: " + std::to_string(version);
        return false;
    }

    return true;
}

bool GGUFPreflightGuard::validateMemoryHeadroom(ComputeBackend backend, uint64_t vram_required, std::string& out_error) {
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memInfo)) {
        out_error = "Unable to query system memory.";
        return false;
    }

    const uint64_t total_ram = static_cast<uint64_t>(memInfo.ullTotalPhys);
    const uint64_t avail_ram = static_cast<uint64_t>(memInfo.ullAvailPhys);
    const uint64_t safety_reserve_ram = total_ram / 5;

    if (avail_ram < safety_reserve_ram) {
        out_error = "Insufficient System RAM. Required safety headroom: " +
            std::to_string(safety_reserve_ram / (1024 * 1024)) + "MB, Available: " +
            std::to_string(avail_ram / (1024 * 1024)) + "MB.";
        return false;
    }

    if (backend == ComputeBackend::GPU_ONLY) {
        const uint64_t total_vram = queryDedicatedVramBytes();
        if (total_vram == 0) {
            out_error = "Unable to query dedicated VRAM budget for GPU preflight.";
            return false;
        }

        const uint64_t safety_reserve_vram = (total_vram * 15) / 100;
        const uint64_t required = vram_required + safety_reserve_vram;
        if (vram_required == 0 || total_vram < required) {
            out_error = "Insufficient VRAM. Model + 15% safety reserve requires " +
                std::to_string(required / (1024 * 1024)) + "MB, Available: " +
                std::to_string(total_vram / (1024 * 1024)) + "MB.";
            return false;
        }
    }

    return true;
}

bool GGUFPreflightGuard::validateQuantizationSupport(uint32_t ggml_type) {
    switch (ggml_type) {
    case GGML_TYPE_F32:
    case GGML_TYPE_F16:
    case GGML_TYPE_Q4_0:
    case GGML_TYPE_Q8_0:
    case GGML_TYPE_Q2_K:
    case GGML_TYPE_Q3_K:
    case GGML_TYPE_Q4_K:
    case GGML_TYPE_Q5_K:
    case GGML_TYPE_Q6_K:
        return true;
    default:
        return false;
    }
}

} // namespace RawrXD
