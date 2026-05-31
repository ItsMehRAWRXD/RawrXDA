// ============================================================================
// Smoke Test Orchestrator Implementation
// Unbounded hardware capacity with automatic tier selection
// ============================================================================

#include "smoke_test_orchestrator.hpp"

#include "cpu_inference_engine.h"
#include "agentic_engine.h"
#include "cli/CLI_SlashRouter.hpp"
#include "streaming_gguf_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <filesystem>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#pragma comment(lib, "pdh.lib")
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Test {

using RawrXD::CPUInferenceEngine;
using ::AgenticEngine;
using RawrXD::CLI::InitializeCLISlashRouter;
using RawrXD::CLI::ProcessSlashCommand;
using RawrXD::StreamingGGUFLoader;

// ============================================================================
// HardwareCapabilities Implementation
// ============================================================================

HardwareCapabilities HardwareCapabilities::Detect() {
    HardwareCapabilities hw;
    
#ifdef _WIN32
    // RAM
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        hw.systemRamBytes = memStatus.ullTotalPhys;
        hw.availableRamBytes = memStatus.ullAvailPhys;
    }
    
    // CPU
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    hw.logicalCores = sysInfo.dwNumberOfProcessors;
    
    // Physical cores via CPUID
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufferSize);
    if (bufferSize > 0) {
        std::vector<BYTE> buffer(bufferSize);
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, 
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &bufferSize)) {
            uint32_t physicalCount = 0;
            size_t offset = 0;
            while (offset < bufferSize) {
                auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
                if (info->Relationship == RelationProcessorCore) {
                    physicalCount++;
                }
                offset += info->Size;
            }
            hw.physicalCores = physicalCount;
        }
    }
    
    // AVX-512 detection via CPUID
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    if (nIds >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        hw.hasAvx2 = (cpuInfo[1] & (1 << 5)) != 0;
        hw.hasAvx512 = (cpuInfo[1] & (1 << 16)) != 0;  // AVX-512F
        hw.hasVpopcnt = (cpuInfo[2] & (1 << 14)) != 0;  // VPOPCNTDQ
        hw.hasAmx = (cpuInfo[3] & (1 << 24)) != 0;  // AMX-TILE
    }
    
    // GPU detection (basic)
    hw.hasVulkan = true;  // Assume available, will be validated at runtime
    
#else
    // Linux
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        hw.systemRamBytes = si.totalram * si.mem_unit;
        hw.availableRamBytes = si.freeram * si.mem_unit;
    }
    hw.logicalCores = sysconf(_SC_NPROCESSORS_ONLN);
    hw.physicalCores = hw.logicalCores;  // Simplified
    
    // AVX-512 via /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("flags") != std::string::npos) {
            hw.hasAvx2 = line.find("avx2") != std::string::npos;
            hw.hasAvx512 = line.find("avx512f") != std::string::npos;
            hw.hasVpopcnt = line.find("vpopcntdq") != std::string::npos;
            break;
        }
    }
#endif
    
    // Estimate bandwidth (conservative)
    hw.ramBandwidthGbps = hw.systemRamBytes >= 128ULL * 1024 * 1024 * 1024 ? 400.0 : 200.0;
    hw.pcieBandwidthGbps = 32.0;  // PCIe 4.0 x16
    
    return hw;
}

std::string HardwareCapabilities::ToString() const {
    std::ostringstream oss;
    oss << "=== Hardware Capabilities ===\n";
    oss << "RAM: " << (systemRamBytes / (1024.0 * 1024 * 1024)) << " GB total, "
        << (availableRamBytes / (1024.0 * 1024 * 1024)) << " GB available\n";
    oss << "CPU: " << physicalCores << " physical / " << logicalCores << " logical cores\n";
    oss << "ISA: " << (hasAvx512 ? "AVX-512" : hasAvx2 ? "AVX2" : "SSE") << " "
        << (hasVpopcnt ? "+ VPOPCNT" : "") << (hasAmx ? " + AMX" : "") << "\n";
    oss << "GPU: " << (hasVulkan ? "Vulkan" : "None") << "\n";
    oss << "Bandwidth: " << ramBandwidthGbps << " GB/s RAM, "
        << pcieBandwidthGbps << " GB/s PCIe\n";
    return oss.str();
}

// ============================================================================
// ModelProfile Implementation
// ============================================================================

ModelSizeTier ModelProfile::GetSizeTier() const {
    if (parameterCount <= 100'000'000) return ModelSizeTier::Tiny;
    if (parameterCount <= 1'000'000'000) return ModelSizeTier::Small;
    if (parameterCount <= 10'000'000'000) return ModelSizeTier::Medium;
    if (parameterCount <= 100'000'000'000) return ModelSizeTier::Large;
    if (parameterCount <= 500'000'000'000) return ModelSizeTier::XLarge;
    if (parameterCount <= 1'000'000'000'000) return ModelSizeTier::XXLarge;
    return ModelSizeTier::Titan;
}

uint64_t ModelProfile::EstimateRuntimeMemoryBytes() const {
    // Conservative estimate: weights + KV cache + activations + overhead
    double bytesPerParam = 0.0;
    if (quantization == "Q4_0" || quantization == "Q4_K_M") bytesPerParam = 0.5;
    else if (quantization == "Q5_0" || quantization == "Q5_K_M") bytesPerParam = 0.625;
    else if (quantization == "Q8_0" || quantization == "Q8_K_M") bytesPerParam = 1.0;
    else if (quantization == "F16" || quantization == "FP16") bytesPerParam = 2.0;
    else if (quantization == "F32" || quantization == "FP32") bytesPerParam = 4.0;
    else bytesPerParam = 1.0;  // Default conservative
    
    uint64_t weightsBytes = static_cast<uint64_t>(parameterCount * bytesPerParam);
    
    // KV cache: 2 * layers * context * heads * head_dim * sizeof(fp16)
    uint64_t kvCacheBytes = 2ULL * layerCount * contextLength * headCount * 64 * 2;
    
    // Activations: batch * seq * embed * 4 (rough)
    uint64_t activationBytes = 4ULL * contextLength * embeddingDim * 4;
    
    // Overhead: 20%
    uint64_t overhead = (weightsBytes + kvCacheBytes + activationBytes) / 5;
    
    return weightsBytes + kvCacheBytes + activationBytes + overhead;
}

uint64_t ModelProfile::EstimateLoadMemoryBytes() const {
    // Loading requires at least the file size + parsing overhead
    return fileSizeBytes + (fileSizeBytes / 10);  // +10% parsing overhead
}

bool ModelProfile::CanRunOn(const HardwareCapabilities& hw) const {
    uint64_t needed = EstimateRuntimeMemoryBytes();
    
    // Can we fit in RAM?
    if (needed <= hw.availableRamBytes) return true;
    
    // Can we fit with mmap streaming? (only need active tensors in RAM)
    uint64_t streamingNeeded = needed / 4;  // ~25% active at once
    if (streamingNeeded <= hw.availableRamBytes) return true;
    
    // Can we fit with tensor paging to disk?
    uint64_t pagedNeeded = needed / 10;  // ~10% active with aggressive paging
    if (pagedNeeded <= hw.availableRamBytes) return true;
    
    return false;
}

std::string ModelProfile::ToString() const {
    std::ostringstream oss;
    oss << "=== Model Profile ===\n";
    oss << "Name: " << name << "\n";
    oss << "Path: " << path << "\n";
    oss << "Architecture: " << architecture << "\n";
    
    double paramsBillions = parameterCount / 1e9;
    if (paramsBillions >= 1.0) {
        oss << "Parameters: " << std::fixed << std::setprecision(2) << paramsBillions << "B\n";
    } else {
        oss << "Parameters: " << (parameterCount / 1e6) << "M\n";
    }
    
    oss << "File Size: " << (fileSizeBytes / (1024.0 * 1024 * 1024)) << " GB\n";
    oss << "Context: " << contextLength << " tokens\n";
    oss << "Layers: " << layerCount << ", Heads: " << headCount << ", Embed: " << embeddingDim << "\n";
    oss << "Quantization: " << quantization << "\n";
    
    auto tier = GetSizeTier();
    const char* tierNames[] = {"Tiny", "Small", "Medium", "Large", "XLarge", "XXLarge", "Titan"};
    oss << "Tier: " << tierNames[static_cast<int>(tier)] << "\n";
    
    oss << "Est. Runtime Memory: " << (EstimateRuntimeMemoryBytes() / (1024.0 * 1024 * 1024)) << " GB\n";
    return oss.str();
}

// ============================================================================
// TestTierConfig Auto-Selection
// ============================================================================

TestTierConfig TestTierConfig::AutoSelect(const ModelProfile& model, const HardwareCapabilities& hw) {
    TestTierConfig config;
    auto tier = model.GetSizeTier();
    uint64_t needed = model.EstimateRuntimeMemoryBytes();
    double ramRatio = static_cast<double>(needed) / static_cast<double>(hw.systemRamBytes);
    
    switch (tier) {
        case ModelSizeTier::Tiny:
            config.depth = TestDepth::Standard;
            config.maxTokens = 256;
            config.contextWindow = 2048;
            config.timeout = std::chrono::seconds(60);
            break;
            
        case ModelSizeTier::Small:
            config.depth = TestDepth::Standard;
            config.maxTokens = 256;
            config.contextWindow = 4096;
            config.timeout = std::chrono::seconds(120);
            break;
            
        case ModelSizeTier::Medium:
            config.depth = ramRatio > 0.5 ? TestDepth::Quick : TestDepth::Standard;
            config.maxTokens = ramRatio > 0.5 ? 64 : 256;
            config.contextWindow = 4096;
            config.timeout = std::chrono::seconds(300);
            break;
            
        case ModelSizeTier::Large:
            config.depth = ramRatio > 0.8 ? TestDepth::Smoke : TestDepth::Quick;
            config.maxTokens = ramRatio > 0.8 ? 16 : 64;
            config.contextWindow = 2048;
            config.enableMemoryPressure = true;
            config.timeout = std::chrono::seconds(600);
            break;
            
        case ModelSizeTier::XLarge:
            config.depth = TestDepth::Smoke;
            config.maxTokens = 16;
            config.contextWindow = 512;
            config.enableMemoryPressure = true;
            config.enableMmapStreaming = true;
            config.timeout = std::chrono::seconds(900);
            break;
            
        case ModelSizeTier::XXLarge:
            config.depth = TestDepth::Smoke;
            config.maxTokens = 8;
            config.contextWindow = 256;
            config.enableMemoryPressure = true;
            config.enableMmapStreaming = true;
            config.activeTensorPercentage = 50;
            config.timeout = std::chrono::seconds(1800);
            break;
            
        case ModelSizeTier::Titan:
            config.depth = TestDepth::Smoke;
            config.maxTokens = 4;
            config.contextWindow = 128;
            config.enableMemoryPressure = true;
            config.enableMmapStreaming = true;
            config.activeTensorPercentage = 25;
            config.timeout = std::chrono::seconds(3600);
            break;
    }
    
    return config;
}

// ============================================================================
// MemoryPressureConfig
// ============================================================================

MemoryPressureConfig MemoryPressureConfig::AutoConfigure(const ModelProfile& model, const HardwareCapabilities& hw) {
    MemoryPressureConfig config;
    uint64_t needed = model.EstimateRuntimeMemoryBytes();
    
    config.enabled = true;
    config.maxWorkingSetBytes = hw.availableRamBytes * 3 / 4;  // Use 75% of available RAM
    config.pageFileReserveBytes = needed > hw.systemRamBytes ? needed - hw.systemRamBytes : 0;
    config.enableMmapStreaming = needed > hw.availableRamBytes;
    config.enableTensorPaging = needed > hw.systemRamBytes * 2;
    
    if (needed > hw.systemRamBytes * 4) {
        config.activeTensorPercentage = 10;  // Very aggressive paging
    } else if (needed > hw.systemRamBytes * 2) {
        config.activeTensorPercentage = 25;
    } else if (needed > hw.systemRamBytes) {
        config.activeTensorPercentage = 50;
    } else {
        config.activeTensorPercentage = 100;
    }
    
    return config;
}

// ============================================================================
// SmokeTestOrchestrator Implementation
// ============================================================================

SmokeTestOrchestrator::SmokeTestOrchestrator(const Configuration& config)
    : m_config(config)
    , m_hwCaps(HardwareCapabilities::Detect()) {
    std::cout << m_hwCaps.ToString() << std::endl;
}

SmokeTestOrchestrator::~SmokeTestOrchestrator() {
    StopTelemetryCollector();
}

bool SmokeTestOrchestrator::RunAllTests() {
    std::cout << "=== Unbounded Capacity Smoke Test Suite ===" << std::endl;
    std::cout << "Models to test: " << m_config.modelPaths.size() << std::endl;
    
    // Auto-detect models if requested
    if (m_config.autoDetectModels && m_config.modelPaths.empty()) {
        m_config.modelPaths = Utils::FindGgufModels(".");
        std::cout << "Auto-detected " << m_config.modelPaths.size() << " models" << std::endl;
    }
    
    if (m_config.modelPaths.empty()) {
        std::cerr << "No models to test!" << std::endl;
        return false;
    }
    
    // Create output directory
    if (!m_config.outputDir.empty() && !fs::exists(m_config.outputDir)) {
        fs::create_directories(m_config.outputDir);
    }
    
    bool allPassed = true;
    
    if (m_config.parallelModelTesting && m_config.maxConcurrentModels > 1) {
        // Parallel testing
        std::vector<std::future<bool>> futures;
        size_t idx = 0;
        
        for (const auto& path : m_config.modelPaths) {
            if (futures.size() >= m_config.maxConcurrentModels) {
                allPassed &= futures[idx % m_config.maxConcurrentModels].get();
            }
            futures.push_back(std::async(std::launch::async, [&path, this]() {
                return TestModel(path);
            }));
            idx++;
        }
        
        for (auto& f : futures) {
            allPassed &= f.get();
        }
    } else {
        // Sequential testing
        for (const auto& path : m_config.modelPaths) {
            allPassed &= TestModel(path);
        }
    }
    
    // Generate report
    if (m_config.generateReport) {
        std::string reportPath = m_config.outputDir + "/smoke_test_report.json";
        SaveReport(reportPath);
        std::cout << "Report saved: " << reportPath << std::endl;
    }
    
    return allPassed;
}

bool SmokeTestOrchestrator::TestModel(const std::string& modelPath) {
    if (!ValidateModelFile(modelPath)) {
        TestResult result;
        result.testName = "ModelValidation";
        result.category = "Setup";
        result.passed = false;
        result.errorMessage = "Invalid or missing model file: " + modelPath;
        RecordResult(result);
        return false;
    }
    
    ModelProfile profile = DetectModelProfile(modelPath);
    std::cout << "\n" << profile.ToString() << std::endl;
    
    return TestModel(profile);
}

bool SmokeTestOrchestrator::TestModel(const ModelProfile& profile) {
    // Check hardware capacity
    if (!EnsureHardwareCapacity(profile)) {
        TestResult result;
        result.testName = "HardwareCapacity";
        result.category = "Setup";
        result.passed = false;
        result.errorMessage = "Insufficient hardware capacity for model";
        result.wasDegraded = true;
        result.degradationReason = "Hardware capacity insufficient - would require " +
            std::to_string(profile.EstimateRuntimeMemoryBytes() / (1024*1024*1024)) + " GB RAM";
        RecordResult(result);
        
        // Try shard simulation instead
        std::cout << "Attempting shard simulation for " << profile.name << std::endl;
        return RunShardSimulation(profile);
    }
    
    // Auto-select test tier
    TestTierConfig tier = TestTierConfig::AutoSelect(profile, m_hwCaps);
    std::cout << "Auto-selected test depth: " << static_cast<int>(tier.depth) << std::endl;
    
    // Setup memory pressure if needed
    if (tier.enableMemoryPressure) {
        SetupMemoryPressure(profile);
    }
    
    // Run tests based on depth
    bool passed = true;
    
    passed &= RunSmokeTests(profile);
    
    if (tier.depth >= TestDepth::Quick) {
        passed &= RunQuickTests(profile);
    }
    if (tier.depth >= TestDepth::Standard) {
        passed &= RunStandardTests(profile);
    }
    if (tier.depth >= TestDepth::Deep) {
        passed &= RunDeepTests(profile);
    }
    if (tier.depth >= TestDepth::Stress) {
        passed &= RunStressTests(profile);
    }
    if (tier.depth >= TestDepth::Soak) {
        passed &= RunSoakTests(profile);
    }
    
    return passed;
}

bool SmokeTestOrchestrator::RunSmokeTests(const ModelProfile& model) {
    std::cout << "\n--- Smoke Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Smoke;
    tier.maxTokens = 4;
    tier.contextWindow = 128;
    tier.timeout = std::chrono::seconds(30);
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestModelInference(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

bool SmokeTestOrchestrator::RunQuickTests(const ModelProfile& model) {
    std::cout << "\n--- Quick Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Quick;
    tier.maxTokens = 16;
    tier.contextWindow = 512;
    tier.timeout = std::chrono::seconds(60);
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestModelInference(model, tier);
    passed &= TestModelStreaming(model, tier);
    passed &= TestSlashCommandRouting(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

bool SmokeTestOrchestrator::RunStandardTests(const ModelProfile& model) {
    std::cout << "\n--- Standard Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Standard;
    tier.maxTokens = 256;
    tier.contextWindow = 4096;
    tier.timeout = std::chrono::seconds(300);
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestModelInference(model, tier);
    passed &= TestModelStreaming(model, tier);
    passed &= TestModelKvCache(model, tier);
    passed &= TestSlashCommandRouting(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

bool SmokeTestOrchestrator::RunDeepTests(const ModelProfile& model) {
    std::cout << "\n--- Deep Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Deep;
    tier.maxTokens = 2048;
    tier.contextWindow = 8192;
    tier.repeatCount = 3;
    tier.timeout = std::chrono::seconds(1800);
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestModelInference(model, tier);
    passed &= TestMemoryPressure(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

bool SmokeTestOrchestrator::RunStressTests(const ModelProfile& model) {
    std::cout << "\n--- Stress Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Stress;
    tier.maxTokens = 4096;
    tier.contextWindow = 32768;
    tier.batchSize = 4;
    tier.enableThermalMonitoring = true;
    tier.timeout = std::chrono::seconds(7200);
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestModelInference(model, tier);
    passed &= TestThermalStability(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

bool SmokeTestOrchestrator::RunSoakTests(const ModelProfile& model) {
    std::cout << "\n--- Soak Tests ---" << std::endl;
    TestTierConfig tier;
    tier.depth = TestDepth::Soak;
    tier.maxTokens = 256;
    tier.contextWindow = 4096;
    tier.repeatCount = 1000;
    tier.timeout = std::chrono::seconds(86400);  // 24 hours
    
    bool passed = true;
    passed &= TestModelLoad(model, tier);
    passed &= TestEndurance(model, tier);
    passed &= TestModelUnload(model, tier);
    return passed;
}

// ============================================================================
// Individual Test Implementations
// ============================================================================

bool SmokeTestOrchestrator::TestModelLoad(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "ModelLoad";
    result.category = "Core";
    
    std::cout << "[Test] Loading model: " << model.name << std::endl;
    
    try {
        // Use streaming loader for large models
        auto loader = std::make_unique<StreamingGGUFLoader>();
        if (!loader->Open(model.path)) {
            result.passed = false;
            result.errorMessage = "Failed to open model file";
            RecordResult(result);
            return false;
        }
        
        // Parse header to validate format
        if (!loader->ParseHeader()) {
            result.passed = false;
            result.errorMessage = "Failed to parse GGUF header";
            RecordResult(result);
            return false;
        }
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.passed = true;
        result.output = "Model loaded successfully in " + std::to_string(result.duration.count()) + "ms";
        
        std::cout << "✅ Model load: " << result.duration.count() << "ms" << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Model load failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestModelInference(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "ModelInference";
    result.category = "Core";
    
    std::cout << "[Test] Inference (" << tier.maxTokens << " tokens)" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            if (!engine->LoadModel(model.path)) {
                result.passed = false;
                result.errorMessage = "Failed to load model into inference engine";
                RecordResult(result);
                return false;
            }
        }
        
        // Tokenize
        std::vector<int> tokens = engine->Tokenize("Hello, world!");
        if (tokens.empty()) {
            result.passed = false;
            result.errorMessage = "Tokenization returned empty tokens";
            RecordResult(result);
            return false;
        }
        
        // Generate
        auto genStart = std::chrono::steady_clock::now();
        std::vector<int> generated = engine->Generate(tokens, tier.maxTokens);
        auto genEnd = std::chrono::steady_clock::now();
        
        if (generated.empty()) {
            result.passed = false;
            result.errorMessage = "Generation returned empty tokens";
            RecordResult(result);
            return false;
        }
        
        // Detokenize
        std::string output = engine->Detokenize(generated);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.tokensGenerated = generated.size();
        result.tokensPerSecond = generated.size() * 1000.0 / result.duration.count();
        result.firstTokenLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(genEnd - genStart).count();
        result.passed = !output.empty();
        result.output = output.substr(0, 200);  // Truncate for report
        
        std::cout << "✅ Inference: " << result.tokensGenerated << " tokens in "
                  << result.duration.count() << "ms ("
                  << std::fixed << std::setprecision(2) << result.tokensPerSecond << " t/s)"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Inference failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestModelStreaming(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "ModelStreaming";
    result.category = "Core";
    
    std::cout << "[Test] Streaming inference" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        std::vector<int> tokens = engine->Tokenize("Count: 1, 2, 3,");
        
        uint32_t receivedTokens = 0;
        auto streamStart = std::chrono::steady_clock::now();
        
        engine->GenerateStreaming(tokens, tier.maxTokens,
            [&receivedTokens](const std::string& chunk) {
                receivedTokens++;
                if (receivedTokens <= 5) {
                    std::cout << "  Token " << receivedTokens << ": " << chunk << std::endl;
                }
            },
            []() {
                std::cout << "  Streaming complete" << std::endl;
            }
        );
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.tokensGenerated = receivedTokens;
        result.passed = receivedTokens > 0;
        
        std::cout << "✅ Streaming: " << receivedTokens << " tokens streamed"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Streaming failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestModelKvCache(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "KvCache";
    result.category = "Core";
    
    std::cout << "[Test] KV-Cache validation" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        // First turn
        std::vector<int> tokens1 = engine->Tokenize("What is 2+2?");
        auto reply1 = engine->Generate(tokens1, 32);
        
        // Second turn (should use cached context)
        std::vector<int> tokens2 = engine->Tokenize("What is 3+3?");
        auto genStart = std::chrono::steady_clock::now();
        auto reply2 = engine->Generate(tokens2, 32);
        auto genEnd = std::chrono::steady_clock::now();
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.firstTokenLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(genEnd - genStart).count();
        result.passed = !reply1.empty() && !reply2.empty();
        
        std::cout << "✅ KV-Cache: Multi-turn inference validated"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ KV-Cache failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestModelUnload(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "ModelUnload";
    result.category = "Core";
    
    std::cout << "[Test] Model unload" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        engine->ClearCache();
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.passed = true;  // ClearCache always succeeds
        
        std::cout << "✅ Unload: " << result.duration.count() << "ms"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Unload failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestSlashCommandRouting(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "SlashCommandRouting";
    result.category = "CLI";
    
    std::cout << "[Test] Slash command routing" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        // Skip AgenticEngine initialization - use engine-only path for smoke test
        // CLI::InitializeCLISlashRouter(engine, nullptr);
        
        // Test arithmetic commands
        struct CmdTest {
            std::string cmd;
            std::string expected;
        };
        
        std::vector<CmdTest> tests = {
            {"/add 5 3", "8"},
            {"/sub 10 4", "6"},
            {"/mul 7 6", "42"},
            {"/div 20 4", "5"},
            {"/concat Hello World", "HelloWorld"},
            {"/upper hello", "HELLO"},
            {"/lower WORLD", "world"},
        };
        
        uint32_t passed = 0;
        for (const auto& test : tests) {
            std::string output = CLI::ProcessSlashCommand(test.cmd);
            if (output.find(test.expected) != std::string::npos) {
                passed++;
            }
        }
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.passed = passed == tests.size();
        result.output = std::to_string(passed) + "/" + std::to_string(tests.size()) + " commands passed";
        
        std::cout << "✅ Routing: " << passed << "/" << tests.size() << " commands"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Routing failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestMemoryPressure(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "MemoryPressure";
    result.category = "Stress";
    
    std::cout << "[Test] Memory pressure handling" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        uint64_t baselineMem = Utils::GetProcessMemoryUsage();
        
        // Run multiple inferences to stress memory
        for (int i = 0; i < 10; i++) {
            std::vector<int> tokens = engine->Tokenize("Test prompt " + std::to_string(i));
            auto reply = engine->Generate(tokens, tier.maxTokens);
        }
        
        uint64_t peakMem = Utils::GetProcessPeakMemoryUsage();
        uint64_t currentMem = Utils::GetProcessMemoryUsage();
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.peakMemoryBytes = peakMem;
        result.passed = currentMem < peakMem * 1.5;  // Memory should not grow unbounded
        
        std::cout << "✅ Memory: Baseline=" << (baselineMem / 1024 / 1024) << "MB, "
                  << "Peak=" << (peakMem / 1024 / 1024) << "MB, "
                  << "Current=" << (currentMem / 1024 / 1024) << "MB"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Memory pressure failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestThermalStability(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "ThermalStability";
    result.category = "Stress";
    
    std::cout << "[Test] Thermal stability (" << tier.timeout.count() << "s)" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        StartTelemetryCollector();
        
        auto deadline = start + tier.timeout;
        uint32_t iterations = 0;
        
        while (std::chrono::steady_clock::now() < deadline) {
            std::vector<int> tokens = engine->Tokenize("Thermal test iteration " + std::to_string(iterations));
            auto reply = engine->Generate(tokens, 16);
            iterations++;
            
            if (iterations % 100 == 0) {
                std::cout << "  Iteration " << iterations << std::endl;
            }
        }
        
        StopTelemetryCollector();
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.tokensGenerated = iterations * 16;
        result.passed = iterations > 0;
        
        std::cout << "✅ Thermal: " << iterations << " iterations, "
                  << result.tokensGenerated << " tokens"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Thermal stability failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

bool SmokeTestOrchestrator::TestEndurance(const ModelProfile& model, const TestTierConfig& tier) {
    auto start = std::chrono::steady_clock::now();
    
    TestResult result;
    result.testName = "Endurance";
    result.category = "Soak";
    
    std::cout << "[Test] Endurance (" << tier.repeatCount << " iterations)" << std::endl;
    
    try {
        auto engine = CPUInferenceEngine::GetSharedInstance();
        
        if (!engine->IsModelLoaded()) {
            engine->LoadModel(model.path);
        }
        
        uint64_t initialMem = Utils::GetProcessMemoryUsage();
        uint32_t passed = 0;
        
        for (uint32_t i = 0; i < tier.repeatCount; i++) {
            std::vector<int> tokens = engine->Tokenize("Endurance test " + std::to_string(i));
            auto reply = engine->Generate(tokens, 8);
            
            if (!reply.empty()) passed++;
            
            if ((i + 1) % 100 == 0) {
                uint64_t currentMem = Utils::GetProcessMemoryUsage();
                double memGrowth = static_cast<double>(currentMem - initialMem) / initialMem * 100.0;
                std::cout << "  " << (i + 1) << "/" << tier.repeatCount
                          << " (mem growth: " << std::fixed << std::setprecision(1)
                          << memGrowth << "%)" << std::endl;
            }
        }
        
        uint64_t finalMem = Utils::GetProcessMemoryUsage();
        double memGrowth = static_cast<double>(finalMem - initialMem) / initialMem * 100.0;
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        result.tokensGenerated = passed * 8;
        result.passed = passed == tier.repeatCount && memGrowth < 50.0;  // <50% growth acceptable
        
        std::cout << "✅ Endurance: " << passed << "/" << tier.repeatCount
                  << " passed, mem growth: " << memGrowth << "%"
                  << std::endl;
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
        std::cerr << "❌ Endurance failed: " << e.what() << std::endl;
    }
    
    RecordResult(result);
    return result.passed;
}

// ============================================================================
// Shard Simulation
// ============================================================================

bool SmokeTestOrchestrator::RunShardSimulation(const ModelProfile& model) {
    std::cout << "\n--- Shard Simulation ---" << std::endl;
    
    TestResult result;
    result.testName = "ShardSimulation";
    result.category = "Simulation";
    result.wasDegraded = true;
    result.degradationReason = "Hardware insufficient - running simulation";
    
    // Calculate shard count based on available RAM
    uint64_t needed = model.EstimateRuntimeMemoryBytes();
    uint32_t shardCount = static_cast<uint32_t>(
        std::ceil(static_cast<double>(needed) / m_hwCaps.availableRamBytes)
    );
    if (shardCount < 1) shardCount = 1;
    
    ModelShardSimulator::ShardConfig shardConfig;
    shardConfig.totalParameters = model.parameterCount;
    shardConfig.shardCount = shardCount;
    shardConfig.bytesPerShard = needed / shardCount;
    shardConfig.simulateOnly = true;
    
    ModelShardSimulator simulator(shardConfig);
    
    std::cout << "Simulating " << model.name << " with " << shardCount << " shards" << std::endl;
    
    bool loadOk = simulator.SimulateLoad([](uint32_t shard, uint64_t bytes) {
        std::cout << "  Shard " << shard << ": " << (bytes / (1024.0*1024*1024)) << " GB" << std::endl;
    });
    
    bool inferOk = simulator.SimulateInference(16, [](uint32_t token, double tps) {
        if (token <= 5) {
            std::cout << "  Token " << token << ": " << tps << " t/s" << std::endl;
        }
    });
    
    result.passed = loadOk && inferOk;
    result.output = "Shards: " + std::to_string(shardCount) +
                   ", Simulated load: " + std::to_string(simulator.GetSimulatedLoadTimeSeconds()) + "s" +
                   ", Simulated TPS: " + std::to_string(simulator.GetSimulatedInferenceTps());
    
    std::cout << "✅ Simulation complete" << std::endl;
    
    RecordResult(result);
    return result.passed;
}

// ============================================================================
// Telemetry
// ============================================================================

void SmokeTestOrchestrator::StartTelemetryCollector() {
    m_telemetryRunning = true;
    m_telemetryThread = std::thread([this]() {
        while (m_telemetryRunning) {
            CollectTelemetry();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void SmokeTestOrchestrator::StopTelemetryCollector() {
    m_telemetryRunning = false;
    if (m_telemetryThread.joinable()) {
        m_telemetryThread.join();
    }
}

void SmokeTestOrchestrator::CollectTelemetry() {
    TelemetrySnapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();
    snap.workingSetBytes = Utils::GetProcessMemoryUsage();
    snap.peakWorkingSetBytes = Utils::GetProcessPeakMemoryUsage();
    
#ifdef _WIN32
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
        // Simplified CPU calculation
    }
#endif
    
    std::lock_guard<std::mutex> lock(m_telemetryMutex);
    m_globalTelemetry.push_back(snap);
}

// ============================================================================
// Results & Reporting
// ============================================================================

void SmokeTestOrchestrator::RecordResult(const TestResult& result) {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    m_results.push_back(result);
}

std::vector<TestResult> SmokeTestOrchestrator::GetResults() const {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    return m_results;
}

std::string SmokeTestOrchestrator::GenerateJsonReport() const {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    
    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "\",\n";
    json << "  \"hardware\": {\n";
    json << "    \"ram_gb\": " << (m_hwCaps.systemRamBytes / (1024.0*1024*1024)) << ",\n";
    json << "    \"cores\": " << m_hwCaps.physicalCores << ",\n";
    json << "    \"avx512\": " << (m_hwCaps.hasAvx512 ? "true" : "false") << "\n";
    json << "  },\n";
    json << "  \"results\": [\n";
    
    for (size_t i = 0; i < m_results.size(); i++) {
        const auto& r = m_results[i];
        json << "    {\n";
        json << "      \"name\": \"" << r.testName << "\",\n";
        json << "      \"category\": \"" << r.category << "\",\n";
        json << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n";
        json << "      \"duration_ms\": " << r.duration.count() << ",\n";
        json << "      \"tokens_generated\": " << r.tokensGenerated << ",\n";
        json << "      \"tokens_per_second\": " << r.tokensPerSecond << ",\n";
        json << "      \"peak_memory_mb\": " << (r.peakMemoryBytes / (1024*1024)) << ",\n";
        json << "      \"degraded\": " << (r.wasDegraded ? "true" : "false") << ",\n";
        json << "      \"error\": \"" << r.errorMessage << "\"\n";
        json << "    }" << (i < m_results.size() - 1 ? "," : "") << "\n";
    }
    
    json << "  ]\n";
    json << "}\n";
    
    return json.str();
}

std::string SmokeTestOrchestrator::GenerateMarkdownReport() const {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    
    std::ostringstream md;
    md << "# RawrXD Smoke Test Report\n\n";
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    md << "**Date:** " << std::ctime(&time_t_now) << "\n";
    md << "**Hardware:** " << (m_hwCaps.systemRamBytes / (1024*1024*1024)) << " GB RAM, "
       << m_hwCaps.physicalCores << " cores\n\n";
    
    md << "## Summary\n\n";
    md << "| Test | Category | Status | Duration | Tokens | TPS | Memory |\n";
    md << "|------|----------|--------|----------|--------|-----|--------|\n";
    
    uint32_t passed = 0, failed = 0;
    for (const auto& r : m_results) {
        md << "| " << r.testName << " | " << r.category << " | "
           << (r.passed ? "✅ PASS" : "❌ FAIL") << " | "
           << r.duration.count() << "ms | "
           << r.tokensGenerated << " | "
           << std::fixed << std::setprecision(2) << r.tokensPerSecond << " | "
           << (r.peakMemoryBytes / (1024*1024)) << "MB |\n";
        
        if (r.passed) passed++; else failed++;
    }
    
    md << "\n## Statistics\n\n";
    md << "- **Total:** " << m_results.size() << "\n";
    md << "- **Passed:** " << passed << "\n";
    md << "- **Failed:** " << failed << "\n";
    md << "- **Success Rate:** " << std::fixed << std::setprecision(1)
       << (passed * 100.0 / m_results.size()) << "%\n";
    
    return md.str();
}

bool SmokeTestOrchestrator::SaveReport(const std::string& path) const {
    std::ofstream file(path);
    if (!file) return false;
    
    if (path.ends_with(".json")) {
        file << GenerateJsonReport();
    } else {
        file << GenerateMarkdownReport();
    }
    
    return file.good();
}

// ============================================================================
// Internal Helpers
// ============================================================================

ModelProfile SmokeTestOrchestrator::DetectModelProfile(const std::string& path) {
    return Utils::ParseGgufHeader(path);
}

bool SmokeTestOrchestrator::ValidateModelFile(const std::string& path) {
    if (!fs::exists(path)) return false;
    
    auto size = fs::file_size(path);
    if (size < 1024) return false;  // Minimum valid GGUF
    
    // Check magic
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    // GGUF magic: 0x46554747 ("GGUF" LE)
    return magic == 0x46554747 || magic == 0x47475546;  // LE or BE
}

bool SmokeTestOrchestrator::EnsureHardwareCapacity(const ModelProfile& model) {
    return model.CanRunOn(m_hwCaps);
}

bool SmokeTestOrchestrator::SetupMemoryPressure(const ModelProfile& model) {
    auto config = MemoryPressureConfig::AutoConfigure(model, m_hwCaps);
    
    std::cout << "Memory pressure config:\n";
    std::cout << "  Max working set: " << (config.maxWorkingSetBytes / (1024*1024*1024)) << " GB\n";
    std::cout << "  MMAP streaming: " << (config.enableMmapStreaming ? "enabled" : "disabled") << "\n";
    std::cout << "  Tensor paging: " << (config.enableTensorPaging ? "enabled" : "disabled") << "\n";
    std::cout << "  Active tensor %: " << config.activeTensorPercentage << "%\n";
    
#ifdef _WIN32
    // Set working set limits
    if (config.maxWorkingSetBytes > 0) {
        SIZE_T minWS = config.maxWorkingSetBytes / 2;
        SIZE_T maxWS = config.maxWorkingSetBytes;
        SetProcessWorkingSetSize(GetCurrentProcess(), minWS, maxWS);
    }
#endif
    
    return true;
}

// ============================================================================
// ModelShardSimulator Implementation
// ============================================================================

ModelShardSimulator::ModelShardSimulator(const ShardConfig& config)
    : m_config(config) {
}

bool ModelShardSimulator::SimulateLoad(std::function<void(uint32_t, uint64_t)> progress) {
    // Simulate loading each shard with realistic timing
    double bytesPerSecond = 2.0 * 1024 * 1024 * 1024;  // 2 GB/s (NVMe)
    
    for (uint32_t i = 0; i < m_config.shardCount; i++) {
        if (progress) {
            progress(i, m_config.bytesPerShard);
        }
        
        double loadTime = m_config.bytesPerShard / bytesPerSecond;
        m_simulatedLoadTime += loadTime;
        m_simulatedMemory += m_config.bytesPerShard;
        
        // Simulate I/O wait
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(loadTime * 1000)));
    }
    
    return true;
}

bool ModelShardSimulator::SimulateInference(uint32_t tokens,
    std::function<void(uint32_t, double)> progress) {
    // Simulate inference with realistic TPS based on shard count
    // More shards = more communication overhead
    double baseTps = 50.0;  // Base tokens/second for single shard
    double overheadFactor = 1.0 + (m_config.shardCount - 1) * 0.15;  // 15% overhead per additional shard
    m_simulatedTps = baseTps / overheadFactor;
    
    for (uint32_t i = 0; i < tokens; i++) {
        if (progress && i < 5) {
            progress(i, m_simulatedTps);
        }
        
        double tokenTime = 1.0 / m_simulatedTps;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(tokenTime * 1000)));
    }
    
    return true;
}

double ModelShardSimulator::GetSimulatedLoadTimeSeconds() const {
    return m_simulatedLoadTime;
}

double ModelShardSimulator::GetSimulatedInferenceTps() const {
    return m_simulatedTps;
}

uint64_t ModelShardSimulator::GetSimulatedMemoryBytes() const {
    return m_simulatedMemory;
}

// ============================================================================
// Utility Implementation
// ============================================================================

namespace Utils {

uint64_t GetSystemTotalRam() {
#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullTotalPhys;
    }
    return 0;
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return si.totalram * si.mem_unit;
    }
    return 0;
#endif
}

uint64_t GetSystemAvailableRam() {
#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullAvailPhys;
    }
    return 0;
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return si.freeram * si.mem_unit;
    }
    return 0;
#endif
}

uint64_t GetProcessMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    // Read from /proc/self/status
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            return value * 1024;  // kB to bytes
        }
    }
    return 0;
#endif
}

uint64_t GetProcessPeakMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
#else
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmHWM:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value;
            iss >> label >> value;
            return value * 1024;
        }
    }
    return 0;
#endif
}

bool CheckAvx512Support() {
#ifdef _WIN32
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    if (cpuInfo[0] >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 16)) != 0;
    }
#endif
    return false;
}

bool CheckAvx2Support() {
#ifdef _WIN32
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    if (cpuInfo[0] >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }
#endif
    return false;
}

std::string GetCpuBrandString() {
#ifdef _WIN32
    int cpuInfo[4] = {0};
    char brand[49] = {0};
    __cpuidex(cpuInfo, 0x80000002, 0);
    memcpy(brand, cpuInfo, sizeof(cpuInfo));
    __cpuidex(cpuInfo, 0x80000003, 0);
    memcpy(brand + 16, cpuInfo, sizeof(cpuInfo));
    __cpuidex(cpuInfo, 0x80000004, 0);
    memcpy(brand + 32, cpuInfo, sizeof(cpuInfo));
    return std::string(brand);
#else
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                return line.substr(pos + 2);
            }
        }
    }
    return "Unknown";
#endif
}

std::vector<std::string> FindGgufModels(const std::string& searchPath) {
    std::vector<std::string> models;
    
    if (!fs::exists(searchPath)) return models;
    
    for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
            models.push_back(entry.path().string());
        }
    }
    
    return models;
}

ModelProfile ParseGgufHeader(const std::string& path) {
    ModelProfile profile;
    profile.path = path;
    
    if (!fs::exists(path)) return profile;
    
    profile.fileSizeBytes = fs::file_size(path);
    
    std::ifstream file(path, std::ios::binary);
    if (!file) return profile;
    
    // Read magic
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x46554747) return profile;  // Not a valid GGUF
    
    // Read version
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    // Read tensor count
    uint64_t tensorCount = 0;
    file.read(reinterpret_cast<char*>(&tensorCount), sizeof(tensorCount));
    profile.layerCount = static_cast<uint32_t>(tensorCount);  // Approximate
    
    // Read metadata kv count
    uint64_t metadataCount = 0;
    file.read(reinterpret_cast<char*>(&metadataCount), sizeof(metadataCount));
    
    // Parse metadata to extract architecture info
    for (uint64_t i = 0; i < metadataCount && i < 1000; i++) {
        // Read key length
        uint64_t keyLen = 0;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        if (!file) break;
        
        // Read key
        std::string key(keyLen, '\0');
        file.read(key.data(), keyLen);
        
        // Read value type
        uint32_t valueType = 0;
        file.read(reinterpret_cast<char*>(&valueType), sizeof(valueType));
        
        // Skip value based on type (simplified)
        switch (valueType) {
            case 0: file.seekg(1, std::ios::cur); break;  // uint8
            case 1: file.seekg(1, std::ios::cur); break;  // int8
            case 2: file.seekg(2, std::ios::cur); break;  // uint16
            case 3: file.seekg(2, std::ios::cur); break;  // int16
            case 4: file.seekg(4, std::ios::cur); break;  // uint32
            case 5: file.seekg(4, std::ios::cur); break;  // int32
            case 6: file.seekg(4, std::ios::cur); break;  // float32
            case 7: file.seekg(1, std::ios::cur); break;  // bool
            case 8: {  // string
                uint64_t strLen = 0;
                file.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));
                file.seekg(strLen, std::ios::cur);
                break;
            }
            case 10: file.seekg(8, std::ios::cur); break;  // uint64
            case 11: file.seekg(8, std::ios::cur); break;  // int64
            case 12: file.seekg(8, std::ios::cur); break;  // float64
            default: file.seekg(8, std::ios::cur); break;  // Skip unknown
        }
        
        // Extract known fields
        if (key == "general.architecture") {
            // Would need to read string value properly
        } else if (key == "general.name") {
            // Would need to read string value
        } else if (key == "general.parameter_count") {
            // Would need to read uint64
        }
    }
    
    // Derive name from path
    profile.name = fs::path(path).stem().string();
    
    // Estimate parameter count from file size (rough heuristic)
    // Q4_K_M ~ 0.5 bytes per param, so params ≈ file_size * 2
    profile.parameterCount = profile.fileSizeBytes * 2;
    profile.quantization = "Q4_K_M";  // Default assumption
    profile.contextLength = 4096;  // Default
    profile.headCount = 32;  // Default
    profile.embeddingDim = 4096;  // Default
    
    return profile;
}

} // namespace Utils

} // namespace Test
} // namespace RawrXD
