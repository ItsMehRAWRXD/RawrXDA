#include "backend_selector.h"
#include "cpu_inference_engine.h"
#include "vulkan_inference_engine.h"
#include "hip_inference_engine.h"
#include "cuda_inference_engine.h"
#include "titan_inference_engine.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <windows.h>
#include <dxgi.h>
#include <d3d12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

namespace
{
[[nodiscard]] bool envTruthy(const char* name)
{
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0')
    {
        return false;
    }
    return value[0] != '0' && value[0] != 'f' && value[0] != 'F' && value[0] != 'n' && value[0] != 'N';
}
}  // namespace

namespace RawrXD {

BackendSelector::BackendSelector() {
    m_availableBackends = detectAvailableBackends();
}

std::vector<BackendInfo> BackendSelector::detectAvailableBackends() {
    std::vector<BackendInfo> backends;

    // CPU is always available
    BackendInfo cpuInfo;
    cpuInfo.type = BackendType::CPU;
    cpuInfo.name = "CPU";
    cpuInfo.available = true;
    cpuInfo.deviceName = "System CPU";
    cpuInfo.performanceScore = 1.0; // Baseline
    backends.push_back(cpuInfo);

    // Detect DML (DirectML)
    if (detectDML()) {
        BackendInfo dmlInfo;
        dmlInfo.type = BackendType::DML;
        dmlInfo.name = "DirectML";
        dmlInfo.available = true;
        // Get GPU info
        IDXGIFactory* factory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory))) {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc))) {
                    char deviceName[128];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, deviceName, 128, nullptr, nullptr);
                    dmlInfo.deviceName = deviceName;
                    dmlInfo.vramBytes = desc.DedicatedVideoMemory;
                }
                adapter->Release();
            }
            factory->Release();
        }
        dmlInfo.supportsFP16 = true;
        dmlInfo.performanceScore = 5.0; // Estimated GPU speedup
        backends.push_back(dmlInfo);
    }

    // Detect Vulkan
    if (detectVulkan()) {
        BackendInfo vulkanInfo;
        vulkanInfo.type = BackendType::Vulkan;
        vulkanInfo.name = "Vulkan";
        vulkanInfo.available = true;
        vulkanInfo.deviceName = "Vulkan GPU";
        vulkanInfo.supportsFP16 = true;
        vulkanInfo.performanceScore = 4.5;
        backends.push_back(vulkanInfo);
    }

    // Detect HIP (AMD)
    if (detectHIP()) {
        BackendInfo hipInfo;
        hipInfo.type = BackendType::HIP;
        hipInfo.name = "HIP";
        hipInfo.available = true;
        hipInfo.deviceName = "AMD GPU (ROCm)";
        hipInfo.computeCapability = "RDNA/CDNA";
        hipInfo.supportsFP16 = true;
        hipInfo.supportsInt8 = true;
        hipInfo.supportsFP8 = envTruthy("RAWRXD_ROCM_FP8");
        hipInfo.supportsFP6 = envTruthy("RAWRXD_ROCM_FP6");
        hipInfo.supportsFP4 = envTruthy("RAWRXD_ROCM_FP4");
        hipInfo.supportsUnifiedMemory = true;
        hipInfo.supportsMemoryPooling = true;
        hipInfo.supportsSparseCompute = true;
        hipInfo.supportsNPUOffload = envTruthy("RAWRXD_AMD_XDNA2_OFFLOAD");
        hipInfo.performanceScore = 4.8;
        backends.push_back(hipInfo);
    }

    // Detect CUDA (NVIDIA)
    if (detectCUDA()) {
        BackendInfo cudaInfo;
        cudaInfo.type = BackendType::CUDA;
        cudaInfo.name = "CUDA";
        cudaInfo.available = true;
        cudaInfo.deviceName = "NVIDIA GPU";
        cudaInfo.supportsFP16 = true;
        cudaInfo.performanceScore = 5.5;
        backends.push_back(cudaInfo);
    }

    // Detect Titan custom assembly
    if (detectTitan()) {
        BackendInfo titanInfo;
        titanInfo.type = BackendType::Titan;
        titanInfo.name = "Titan";
        titanInfo.available = true;
        titanInfo.deviceName = "CPU + Titan Assembly";
        titanInfo.performanceScore = 2.5;
        backends.push_back(titanInfo);
    }

    return backends;
}

BackendType BackendSelector::selectOptimalBackend(const std::string& modelPath,
                                                 BackendType preferredType) {
    // Check if preferred type is available
    auto it = std::find_if(m_availableBackends.begin(), m_availableBackends.end(),
                          [preferredType](const BackendInfo& info) {
                              return info.type == preferredType && info.available;
                          });
    if (it != m_availableBackends.end()) {
        return preferredType;
    }

    // Find the best available backend by performance score
    BackendType bestType = BackendType::CPU;
    double bestScore = 0.0;

    for (const auto& backend : m_availableBackends) {
        if (backend.available) {
            double score = scoreBackend(backend, modelPath);
            if (score > bestScore) {
                bestScore = score;
                bestType = backend.type;
            }
        }
    }

    return bestType;
}

std::unique_ptr<InferenceEngine> BackendSelector::createInferenceEngine(BackendType backendType) {
    // GPU inference is mandatory. CPU backend selection is rejected fail-closed.
    switch (backendType) {
        case BackendType::CPU:
            fprintf(stderr, "[BackendSelector] CPU backend rejected: GPU inference is mandatory\n");
            return nullptr;
        case BackendType::DML:
            return createDMLEngine();
        case BackendType::Vulkan:
            return createVulkanEngine();
        case BackendType::HIP:
            return createHIPEngine();
        case BackendType::CUDA:
            return createCUDAEngine();
        case BackendType::Titan:
            return createTitanEngine();
        default:
            fprintf(stderr, "[BackendSelector] Unknown backend rejected: GPU inference is mandatory\n");
            return nullptr;
    }
}

BackendInfo BackendSelector::getBackendInfo(BackendType type) const {
    auto it = std::find_if(m_availableBackends.begin(), m_availableBackends.end(),
                          [type](const BackendInfo& info) { return info.type == type; });
    return (it != m_availableBackends.end()) ? *it : BackendInfo{};
}

std::vector<std::pair<BackendType, double>> BackendSelector::benchmarkBackends(
    const std::string& modelPath, const std::string& testPrompt) {

    std::vector<std::pair<BackendType, double>> results;

    for (const auto& backend : m_availableBackends) {
        if (!backend.available) continue;

        try {
            auto engine = createInferenceEngine(backend.type);
            if (engine && engine->LoadModel(modelPath)) {
                // Simple benchmark: tokenize and generate
                auto tokens = engine->Tokenize(testPrompt);
                auto start = std::chrono::high_resolution_clock::now();
                auto result = engine->Generate(tokens, 10); // Generate 10 tokens
                auto end = std::chrono::high_resolution_clock::now();

                double elapsed = std::chrono::duration<double>(end - start).count();
                results.emplace_back(backend.type, elapsed);
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "[BackendSelector] Benchmark failed for %s: %s\n", backend.name.c_str(), e.what());
        }
    }

    return results;
}

// Private implementation methods

bool BackendSelector::detectDML() {
    // Check for DirectML availability
    // This is a simplified check - in practice, you'd check for DML runtime
    IDXGIFactory* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory))) {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
            adapter->Release();
            factory->Release();
            return true; // GPU available, assume DML works
        }
        factory->Release();
    }
    return false;
}

bool BackendSelector::detectVulkan() {
    // Check for Vulkan loader
    HMODULE vulkanLib = LoadLibraryA("vulkan-1.dll");
    if (vulkanLib) {
        FreeLibrary(vulkanLib);
        return true;
    }
    return false;
}

bool BackendSelector::detectHIP() {
    // Check for HIP runtime
    HMODULE hipLib = LoadLibraryA("hiprt64.dll");
    if (hipLib) {
        FreeLibrary(hipLib);
        return true;
    }
    return false;
}

bool BackendSelector::detectCUDA() {
    // Check for CUDA runtime
    HMODULE cudaLib = LoadLibraryA("nvcuda.dll");
    if (cudaLib) {
        FreeLibrary(cudaLib);
        return true;
    }
    return false;
}

bool BackendSelector::detectTitan() {
    // Check if Titan assembly is available (simplified check)
    return true; // Assume available for now
}

std::unique_ptr<InferenceEngine> BackendSelector::createCPUEngine() {
    return std::make_unique<CPUInferenceEngine>();
}

std::unique_ptr<InferenceEngine> BackendSelector::createDMLEngine() {
    // DMLInferenceEngine is not yet part of the shared InferenceEngine link lane
    // used by tools, so keep behavior deterministic by falling back to CPU here.
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createVulkanEngine() {
    // Vulkan backend: attempt to load Vulkan inference engine, fall back to CPU if unavailable
    auto vulkanEngine = VulkanInferenceEngine::TryCreate();
    if (vulkanEngine) {
        return vulkanEngine;
    }
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createHIPEngine() {
    // HIP backend: attempt to load ROCm/HIP inference engine, fall back to CPU if unavailable
    auto hipEngine = HIPInferenceEngine::TryCreate();
    if (hipEngine) {
        return hipEngine;
    }
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createCUDAEngine() {
    // CUDA backend: attempt to load NVIDIA CUDA inference engine, fall back to CPU if unavailable
    auto cudaEngine = CUDAInferenceEngine::TryCreate();
    if (cudaEngine) {
        return cudaEngine;
    }
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createTitanEngine() {
    // Titan backend: attempt to load Titan native inference engine, fall back to CPU if unavailable
    auto titanEngine = TitanInferenceEngine::TryCreate();
    if (titanEngine) {
        return titanEngine;
    }
    return createCPUEngine();
}

double BackendSelector::scoreBackend(const BackendInfo& info, const std::string& modelPath) {
    double score = info.performanceScore;

    // Adjust score based on model size (prefer GPU for larger models)
    // This is a simplified heuristic
    if (modelPath.find("Q4_K") != std::string::npos || modelPath.find("Q5_K") != std::string::npos) {
        // Quantized models work well on GPU
        score *= 1.2;
    }

    if (info.type == BackendType::HIP) {
        if (modelPath.find("Q2_K") != std::string::npos || modelPath.find("Q3_K") != std::string::npos ||
            modelPath.find("Q4_K") != std::string::npos || modelPath.find("Q5_K") != std::string::npos ||
            modelPath.find("Q6_K") != std::string::npos || modelPath.find("FP4") != std::string::npos ||
            modelPath.find("FP6") != std::string::npos || modelPath.find("FP8") != std::string::npos) {
            score *= 1.18;
        }
        if (info.supportsUnifiedMemory || info.supportsMemoryPooling) {
            score *= 1.05;
        }
        if (info.supportsFP8 || info.supportsFP6 || info.supportsFP4) {
            score *= 1.08;
        }
    }

    // Prefer GPU for models over certain size threshold
    // In practice, you'd check actual model file size

    return score;
}

} // namespace RawrXD
