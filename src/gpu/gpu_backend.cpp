#include "gpu_backend.h"
#include "../gpu_masm/gpu_masm_bridge.h"
#include "../ggml_masm/ggml_masm_bridge.h"
#include "../../include/vulkan_compute.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <limits>
#include <vector>

// Static member initialization
GpuBackend::Backend GpuBackend::s_currentBackend = GpuBackend::CPU;
bool GpuBackend::s_initialized = false;

bool GpuBackend::initialize(Backend backend)
{
    if (s_initialized && s_currentBackend == backend) {
        // // qDebug:  "GPU backend already initialized:" << backendName(backend);
        return true;
    }

    // Initialize GPU detection system first
    if (GPU_Initialize() != 0) {
        // // qWarning:  "Failed to initialize GPU detection system";
        return initializeCpu();
    }

    // Detect available GPUs
    int32_t gpuCount = GPU_Detect();
    // // qInfo:  "Detected" << gpuCount << "GPU(s)";

    // Map our backend enum to MASM backend codes (0=CPU, 1=Vulkan, 2=CUDA, 3=ROCm)
    int64_t masmBackend = 0;
    switch (backend) {
    case Vulkan:
        masmBackend = 1;
        break;
    case CUDA:
        masmBackend = 2;
        break;
    case CPU:
        masmBackend = 0;
        break;
    }

    // Call MASM backend initialization
    int64_t result = InitializeGPUBackend(masmBackend);
    
    if (result == 0) {
        // Success - update state
        s_currentBackend = backend;
        s_initialized = true;
        // // qInfo:  "✓ GPU Backend initialized with" << backendName(backend);
        return true;
    } else {
        // Failed - try CPU fallback
        // // qWarning:  "Failed to initialize" << backendName(backend) << "backend, falling back to CPU";
        result = InitializeGPUBackend(0); // Force CPU mode
        if (result == 0) {
            s_currentBackend = CPU;
            s_initialized = true;
            return true;
        }
        return false;
    }
}

GpuBackend::Backend GpuBackend::currentBackend()
{
    return s_currentBackend;
}

bool GpuBackend::isBackendAvailable(Backend backend)
{
    // Initialize GPU detection if not already done
    static bool detectionInitialized = false;
    if (!detectionInitialized) {
        GPU_Initialize();
        GPU_Detect();
        detectionInitialized = true;
    }

    int32_t gpuCount = GPU_GetDeviceCount();
    
    switch (backend) {
    case Vulkan:
        // Check if any GPU is available (Vulkan supports NVIDIA, AMD, Intel)
        // // qDebug:  "Checking Vulkan availability...";
        return gpuCount > 0;
        
    case CUDA:
        // Check for NVIDIA GPU specifically
        // // qDebug:  "Checking CUDA availability...";
        for (int32_t i = 0; i < gpuCount; i++) {
            GpuDeviceInfo devInfo;
            if (GPU_GetDevice(i, &devInfo) == 0) {
                // NVIDIA vendor ID is 0x10DE
                if (devInfo.vendorId == 0x10DE) {
                    return true;
                }
            }
        }
        return false;
        
    case CPU:
        return true;  // CPU always available
    }
    return false;
}

std::string GpuBackend::backendName(Backend backend)
{
    switch (backend) {
    case CPU:
        return "CPU";
    case Vulkan:
        return "Vulkan";
    case CUDA:
        return "CUDA";
    }
    return "Unknown";
}

bool GpuBackend::initializeWithFallback()
{
    // Try Vulkan first (most compatible)
    if (isBackendAvailable(Vulkan)) {
        // // qInfo:  "Attempting Vulkan initialization...";
        if (initialize(Vulkan)) {
            return true;
        }
    }

    // Fall back to CUDA (NVIDIA only)
    if (isBackendAvailable(CUDA)) {
        // // qInfo:  "Attempting CUDA initialization...";
        if (initialize(CUDA)) {
            return true;
        }
    }

    // Final fallback to CPU
    // // qWarning:  "No GPU backends available, falling back to CPU (slower performance)";
    return initialize(CPU);
}

bool GpuBackend::initializeVulkan()
{
    // // qDebug:  "Attempting to initialize Vulkan...";
    
    // Create Vulkan instance through MASM
    int64_t result = VK_CreateInstance();
    if (result != 0) {
        // // qWarning:  "Failed to create Vulkan instance";
        return false;
    }

    // Enumerate physical devices
    int32_t deviceCount = VK_EnumeratePhysicalDevices();
    if (deviceCount == 0) {
        // // qWarning:  "No Vulkan-compatible devices found";
        VK_DestroyInstance();
        return false;
    }

    // // qInfo:  "✓ Vulkan initialized successfully with" << deviceCount << "device(s)";
    return true;
}

bool GpuBackend::initializeCuda()
{
    // // qDebug:  "Attempting to initialize CUDA...";
    
    // Initialize CUDA runtime through MASM
    int64_t result = CUDA_Initialize();
    if (result != 0) {
        // // qWarning:  "Failed to initialize CUDA runtime";
        return false;
    }

    // Get CUDA device count
    int32_t deviceCount = CUDA_GetDeviceCount();
    if (deviceCount == 0) {
        // // qWarning:  "No CUDA-compatible devices found";
        return false;
    }

    // Set device 0 as active
    result = CUDA_SetDevice(0);
    if (result != 0) {
        // // qWarning:  "Failed to set CUDA device";
        return false;
    }

    // // qInfo:  "✓ CUDA initialized successfully with" << deviceCount << "device(s)";
    return true;
}

bool GpuBackend::initializeCpu()
{
    // // qDebug:  "Initializing CPU backend";
    
    // Still call MASM backend with CPU mode for consistency
    int64_t result = InitializeGPUBackend(0); // 0 = CPU mode
    
    s_currentBackend = CPU;
    s_initialized = true;
    // // qInfo:  "✓ CPU backend initialized";
    return true;
}

namespace {
    using Clock = std::chrono::steady_clock;
    namespace fs = std::filesystem;

    std::unique_ptr<VulkanCompute> g_vulkanCompute;
    std::once_flag g_vulkanInitFlag;
    std::atomic<bool> g_vulkanReady{false};
    std::atomic<uint64_t> g_gpuMatmulCount{0};
    std::atomic<uint64_t> g_cpuMatmulCount{0};
    std::atomic<uint64_t> g_lastMatmulLatencyUs{0};

    fs::path resolveRepoRoot()
    {
        fs::path here(__FILE__);
        return here.parent_path().parent_path();
    }

    std::optional<std::string> resolveMatmulSpvPath()
    {
        std::vector<fs::path> candidates;
        const char* envPath = std::getenv("RAWRXD_MATMUL_SPV");
        if (envPath && *envPath) {
            fs::path envCandidate(envPath);
            if (fs::is_directory(envCandidate)) {
                candidates.emplace_back(envCandidate / "mul_mm.spv");
                candidates.emplace_back(envCandidate / "matmul_f16.spv");
                candidates.emplace_back(envCandidate / "matmul.spv");
            } else {
                candidates.emplace_back(envCandidate);
            }
        }

        const fs::path root = resolveRepoRoot();
        candidates.emplace_back(root / "3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/mul_mm.spv");
        candidates.emplace_back(root / "3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/matmul_f16.spv");
        candidates.emplace_back(root / "3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/matmul.spv");

        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                return candidate.string();
            }
        }

        return std::nullopt;
    }
}

extern "C" int64_t HybridGPU_Init()
{
    std::call_once(g_vulkanInitFlag, [] {
        g_vulkanCompute = std::make_unique<VulkanCompute>();
        if (!g_vulkanCompute->Initialize()) {
            // // qWarning:  "[GPU-BRIDGE] Vulkan initialization failed; CPU fallback will be used";
            g_vulkanCompute.reset();
            return;
        }
        g_vulkanReady.store(true, std::memory_order_release);
    });

    return g_vulkanReady.load(std::memory_order_acquire) ? 1 : 0;
}

extern "C" int64_t HybridCPU_MatMul(const float* A, const float* B, float* C,
                                     int64_t M, int64_t N, int64_t K)
{
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) {
        // // qWarning:  "[GPU-BRIDGE] Invalid CPU matmul arguments" << M << N << K;
        return -1;
    }

    ggml_masm_gemm_f32(A, B, C, M, N, K, 1.0f, 0.0f);
    g_cpuMatmulCount.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

extern "C" int64_t HybridGPU_MatMul(const float* A, const float* B, float* C,
                                     int64_t M, int64_t N, int64_t K)
{
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) {
        // // qWarning:  "[GPU-BRIDGE] Invalid GPU matmul arguments" << M << N << K;
        return -1;
    }

    const auto start = Clock::now();
    bool usedGpu = false;
    const bool dimsFit = M > 0 && N > 0 && K > 0 &&
        M <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) &&
        N <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) &&
        K <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max());

    if (HybridGPU_Init() != 0 && g_vulkanCompute && dimsFit) {
        const auto spvPath = resolveMatmulSpvPath();
        if (spvPath) {
            const bool dispatched = g_vulkanCompute->DispatchMatMulHost(
                A, B, C,
                static_cast<uint32_t>(M),
                static_cast<uint32_t>(K),
                static_cast<uint32_t>(N),
                *spvPath);

            if (dispatched) {
                usedGpu = true;
                g_gpuMatmulCount.fetch_add(1, std::memory_order_relaxed);
                // // qInfo:  "[GPU-BRIDGE] Vulkan matmul offload" << M << "x" << K << "*" << K << "x" << N
                        << "shader:" << std::string::fromStdString(*spvPath);
            } else {
                // // qWarning:  "[GPU-BRIDGE] Vulkan matmul dispatch failed; using CPU fallback";
            }
        } else {
            // // qWarning:  "[GPU-BRIDGE] No matmul SPIR-V shader found; using CPU fallback";
        }
    } else if (!dimsFit) {
        // // qWarning:  "[GPU-BRIDGE] Matmul dimensions exceed uint32_t; forcing CPU path";
    }

    if (!usedGpu) {
        ggml_masm_gemm_f32(A, B, C, M, N, K, 1.0f, 0.0f);
        g_cpuMatmulCount.fetch_add(1, std::memory_order_relaxed);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
    g_lastMatmulLatencyUs.store(static_cast<uint64_t>(elapsed.count()), std::memory_order_relaxed);
    return 0;
}

extern "C" int64_t HybridGPU_Synchronize()
{
    if (g_vulkanReady.load(std::memory_order_acquire) && g_vulkanCompute) {
        return g_vulkanCompute->FlushAsyncCommands() ? 0 : -1;
    }
    return 0;
}



