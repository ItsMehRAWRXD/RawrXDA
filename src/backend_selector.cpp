#include "backend_selector.h"
#include "cpu_inference_engine.h"
// #include "vulkan_inference_engine.h" // TODO: Implement when available
// #include "hip_inference_engine.h"     // TODO: Implement when available
// #include "cuda_inference_engine.h"    // TODO: Implement when available
// #include "titan_inference_engine.h"   // TODO: Implement when available

#include <algorithm>
#include <chrono>
#include <iostream>
#include <windows.h>
#include <dxgi.h>
#include <d3d12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

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
        hipInfo.deviceName = "AMD GPU";
        hipInfo.supportsFP16 = true;
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
    switch (backendType) {
        case BackendType::CPU:
            return createCPUEngine();
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
            return createCPUEngine(); // Fallback
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
            std::cerr << "Benchmark failed for " << backend.name << ": " << e.what() << std::endl;
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
    std::cerr << "DirectML backend not wired in this build lane yet, using CPU" << std::endl;
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createVulkanEngine() {
    // TODO: Implement VulkanInferenceEngine
    // For now, fall back to CPU
    std::cerr << "Vulkan backend not implemented yet, using CPU" << std::endl;
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createHIPEngine() {
    // TODO: Implement HIPInferenceEngine
    std::cerr << "HIP backend not implemented yet, using CPU" << std::endl;
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createCUDAEngine() {
    // TODO: Implement CUDAInferenceEngine
    std::cerr << "CUDA backend not implemented yet, using CPU" << std::endl;
    return createCPUEngine();
}

std::unique_ptr<InferenceEngine> BackendSelector::createTitanEngine() {
    // TODO: Implement TitanInferenceEngine
    // For now, use CPU with Titan flag enabled
    auto engine = std::make_unique<CPUInferenceEngine>();
    // engine->SetUseTitanAssembly(true); // Would need to add this method
    return engine;
}

double BackendSelector::scoreBackend(const BackendInfo& info, const std::string& modelPath) {
    double score = info.performanceScore;

    // Adjust score based on model size (prefer GPU for larger models)
    // This is a simplified heuristic
    if (modelPath.find("Q4_K") != std::string::npos || modelPath.find("Q5_K") != std::string::npos) {
        // Quantized models work well on GPU
        score *= 1.2;
    }

    // Prefer GPU for models over certain size threshold
    // In practice, you'd check actual model file size

    return score;
}

} // namespace RawrXD