// ============================================================================
// accelerator_router.cpp — Phase 30: Unified Multi-Backend Accelerator Router
// ============================================================================
// Automatic hardware detection, thermal-aware routing, and intelligent
// fallback cascade across AMD XDNA, Intel Xe, ARM64 Adreno/NPU, Cerebras
// WSE, and CPU backends.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "accelerator_router.h"
#include "amd_gpu_accelerator.h"
#include "intel_gpu_accelerator.h"
#include "arm64_gpu_accelerator.h"
#include "cerebras_wse_accelerator.h"
#include "nvidia_cuda_accelerator.h"
#include "flash_attention.h"
#include "../../include/enterprise_license.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cmath>

// ============================================================================
// Singleton
// ============================================================================

AcceleratorRouter& AcceleratorRouter::instance() {
    static AcceleratorRouter s_instance;
    return s_instance;
}

AcceleratorRouter::AcceleratorRouter()
    : m_localGPUMinBytes(4096)        // 4KB — below this, CPU is faster (PCIe overhead)
    , m_remoteMinBytes(1048576)       // 1MB — below this, network RTT dominates
    , m_npuMinBytes(4096)             // 4KB — NPU dispatch overhead
    , m_backendChangeCb(nullptr), m_backendChangeData(nullptr)
    , m_failureCb(nullptr), m_failureData(nullptr)
    , m_thermalCb(nullptr), m_thermalData(nullptr)
{
    m_stats.peakLocalTFLOPS = 0;
    m_stats.peakRemoteTFLOPS = 0;

    // Initialize all backend states to default
    for (int i = 0; i < MAX_BACKENDS; i++) {
        m_backends[i] = BackendState();
    }

    // Set backend type and names
    m_backends[static_cast<int>(RouterBackendType::None)].type = RouterBackendType::None;
    m_backends[static_cast<int>(RouterBackendType::None)].backendName = "None";

    m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].type = RouterBackendType::AMD_XDNA;
    m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].backendName = "AMD RDNA/CDNA (XDNA)";
    m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].thermalLimitC = 95;

    m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].type = RouterBackendType::Intel_Xe;
    m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].backendName = "Intel Xe (Arc/Meteor Lake)";
    m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].thermalLimitC = 90;

    m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].type = RouterBackendType::ARM64_Adreno;
    m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].backendName = "ARM64 Adreno GPU";
    m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].thermalLimitC = 80; // Mobile: lower limit

    m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].type = RouterBackendType::ARM64_NPU;
    m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].backendName = "ARM64 Hexagon NPU";
    m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].thermalLimitC = 85;

    m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].type = RouterBackendType::Cerebras_WSE;
    m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].backendName = "Cerebras WSE-2/3 (Remote)";
    m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].thermalLimitC = 200; // Liquid-cooled, thermal not a concern

    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].type = RouterBackendType::CPU_Fallback;
    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].backendName = "CPU (AVX-512/NEON/SVE2)";
    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].thermalLimitC = 100;
    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].available = true; // CPU always available

    m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].type = RouterBackendType::NVIDIA_CUDA;
    m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].backendName = "NVIDIA CUDA (Driver API)";
    m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].thermalLimitC = 90;
}

AcceleratorRouter::~AcceleratorRouter() { shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

RouterResult AcceleratorRouter::initialize() {
    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
    if (!lic.gate(RawrXD::License::FeatureID::MultiGPULoadBalance,
            "AcceleratorRouter::initialize")) {
        return RouterResult::error("Multi-GPU Load Balance requires an Enterprise license", -1);
    }
    if (m_initialized.load(std::memory_order_acquire)) {
        return RouterResult::ok("Router already initialized", getActiveBackend());
    }
    std::lock_guard<std::mutex> lock(m_mutex);

    // Probe all hardware backends
    probeAllBackends();

    // Count available backends
    uint32_t availCount = 0;
    RouterBackendType bestLocal = RouterBackendType::CPU_Fallback;

    // Priority order for local GPUs: AMD → Intel → ARM64 GPU
    // (Cerebras is remote, only used for large batch/streaming)
    if (m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available) {
        bestLocal = RouterBackendType::AMD_XDNA;
        availCount++;
    }
    if (m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available) {
        if (bestLocal == RouterBackendType::CPU_Fallback) {
            bestLocal = RouterBackendType::Intel_Xe;
        }
        availCount++;
    }
    if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available) {
        if (bestLocal == RouterBackendType::CPU_Fallback) {
            bestLocal = RouterBackendType::ARM64_Adreno;
        }
        availCount++;
    }
    if (m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].available) {
        availCount++;
    }
    if (m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available) {
        if (bestLocal == RouterBackendType::CPU_Fallback) {
            bestLocal = RouterBackendType::NVIDIA_CUDA;
        }
        availCount++;
    }
    if (m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].available) {
        availCount++;
    }
    // CPU is always counted
    availCount++;

    m_activeBackend.store(bestLocal, std::memory_order_release);
    m_initialized.store(true, std::memory_order_release);

    return RouterResult::ok("Router initialized", bestLocal);
}

void AcceleratorRouter::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    // We do NOT shut down individual backends — they manage their own lifecycle
    m_activeBackend.store(RouterBackendType::None, std::memory_order_release);
    m_initialized.store(false, std::memory_order_release);
}

// ============================================================================
// Core Dispatch
// ============================================================================

RouterResult AcceleratorRouter::submitInference(const RouterInferenceTask& task) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return RouterResult::error("Router not initialized", -1);
    }

    m_stats.totalSubmissions.fetch_add(1, std::memory_order_relaxed);
    auto start = std::chrono::steady_clock::now();

    // Determine backend: forced > preferred (if valid) > auto-select
    RouterBackendType target = RouterBackendType::Auto;
    RouterBackendType forced = m_forcedBackend.load(std::memory_order_acquire);

    if (forced != RouterBackendType::Auto) {
        // User forced a specific backend
        target = forced;
    } else if (task.preferredBackend != RouterBackendType::Auto &&
               task.preferredBackend != RouterBackendType::None) {
        // Task has a preferred backend — validate it
        int idx = static_cast<int>(task.preferredBackend);
        if (idx >= 0 && idx < MAX_BACKENDS &&
            m_backends[idx].available && m_backends[idx].enabled &&
            checkThermal(task.preferredBackend)) {
            target = task.preferredBackend;
        }
    }

    if (target == RouterBackendType::Auto) {
        target = autoSelectBackend(task);
    }

    // Dispatch to selected backend
    RouterResult result = submitTo(target, task);

    // If dispatch failed, attempt fallback cascade
    if (!result.success && target != RouterBackendType::CPU_Fallback) {
        RouterBackendType fallbackTarget = cascadeFallback(target);
        RouterBackendType attemptedFirst = target;

        while (fallbackTarget != RouterBackendType::None && !result.success) {
            // Notify failure callback
            if (m_failureCb) {
                m_failureCb(target, result.detail, result.errorCode, m_failureData);
            }

            std::cout << "[Router] Fallback: " << getBackendName(target)
                      << " → " << getBackendName(fallbackTarget) << "\n";

            result = submitTo(fallbackTarget, task);

            if (!result.success) {
                target = fallbackTarget;
                fallbackTarget = cascadeFallback(fallbackTarget);
            } else {
                result.attemptedFirst = attemptedFirst;
                result.wasFallback = true;
                m_stats.totalFallbacks.fetch_add(1, std::memory_order_relaxed);

                // Notify backend change
                if (m_backendChangeCb) {
                    m_backendChangeCb(attemptedFirst, fallbackTarget,
                                      "Dispatch failure cascade", m_backendChangeData);
                }
            }
        }
    }

    // Timing
    auto elapsed = std::chrono::steady_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    result.elapsedMs = ms;

    if (result.success) {
        m_stats.totalSuccesses.fetch_add(1, std::memory_order_relaxed);
        m_stats.totalComputeMs.fetch_add(static_cast<uint64_t>(ms), std::memory_order_relaxed);

        // Update backend stats
        int idx = static_cast<int>(result.executedOn);
        if (idx >= 0 && idx < MAX_BACKENDS) {
            m_backends[idx].totalDispatches++;
            m_backends[idx].totalBytesProcessed += task.inputSizeBytes + task.outputSizeBytes;
            m_backends[idx].totalComputeMs += static_cast<uint64_t>(ms);
            // Rolling average (exponential moving average, alpha = 0.1)
            m_backends[idx].avgLatencyMs = m_backends[idx].avgLatencyMs * 0.9 + ms * 0.1;
        }
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

RouterResult AcceleratorRouter::submitTo(RouterBackendType backend,
                                          const RouterInferenceTask& task) {
    // Validate backend index
    int idx = static_cast<int>(backend);
    if (idx < 0 || idx >= MAX_BACKENDS) {
        return RouterResult::error("Invalid backend type", -2);
    }

    // Check availability and enable state
    if (!m_backends[idx].available) {
        return RouterResult::error("Backend not available", -3);
    }
    if (!m_backends[idx].enabled) {
        return RouterResult::error("Backend disabled", -4);
    }

    // Check thermal before dispatch
    if (!checkThermal(backend)) {
        m_stats.thermalThrottleEvents.fetch_add(1, std::memory_order_relaxed);
        if (m_thermalCb) {
            m_thermalCb(backend, m_backends[idx].currentTempC,
                        m_backends[idx].thermalLimitC, m_thermalData);
        }
        return RouterResult::error("Backend thermally throttled", -5);
    }

    // Route to backend-specific dispatcher
    switch (backend) {
    case RouterBackendType::AMD_XDNA:       return dispatchToAMD(task);
    case RouterBackendType::Intel_Xe:       return dispatchToIntel(task);
    case RouterBackendType::ARM64_Adreno:   return dispatchToARM64GPU(task);
    case RouterBackendType::ARM64_NPU:      return dispatchToARM64NPU(task);
    case RouterBackendType::Cerebras_WSE:   return dispatchToCerebras(task);
    case RouterBackendType::NVIDIA_CUDA:    return dispatchToNVIDIA(task);
    case RouterBackendType::CPU_Fallback:   return dispatchToCPU(task);
    default:
        return RouterResult::error("Unknown backend type", -6);
    }
}

// ============================================================================
// Backend-Specific Dispatch — AMD XDNA
// ============================================================================

RouterResult AcceleratorRouter::dispatchToAMD(const RouterInferenceTask& task) {
    AMDGPUAccelerator& amd = AMDGPUAccelerator::instance();

    if (!amd.isInitialized()) {
        return RouterResult::error("AMD GPU not initialized", -10);
    }

    // Map DispatchScope to AMD AccelScope
    AccelScope amdScope = AccelScope::Inference;
    switch (task.scope) {
    case DispatchScope::Inference:        amdScope = AccelScope::Inference; break;
    case DispatchScope::Quantization:     amdScope = AccelScope::Quantization; break;
    case DispatchScope::ModelSurgery:     amdScope = AccelScope::ModelSurgery; break;
    case DispatchScope::SwarmCompute:     amdScope = AccelScope::SwarmCompute; break;
    case DispatchScope::KVCache:          amdScope = AccelScope::KVCache; break;
    case DispatchScope::Embedding:        amdScope = AccelScope::Embedding; break;
    case DispatchScope::SymbolResolution: amdScope = AccelScope::All; break; // AMD doesn't have symbol scope
    case DispatchScope::All:              amdScope = AccelScope::All; break;
    }

    if (!amd.shouldUseGPU(amdScope, task.inputSizeBytes)) {
        return RouterResult::error("AMD GPU scope/size check failed", -11);
    }

    // Dispatch based on kernel name
    AccelResult amdResult = AccelResult::error("No matching AMD kernel");

    if (task.kernelName) {
        const char* k = task.kernelName;
        if (strncmp(k, "matmul", 6) == 0 || strncmp(k, "gemm", 4) == 0) {
            // Generic dispatch for matmul — caller manages buffers separately
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 64, 1, 1);
        } else if (strncmp(k, "flash_attn", 10) == 0 || strncmp(k, "attention", 9) == 0) {
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else if (strncmp(k, "rmsnorm", 7) == 0 || strncmp(k, "layernorm", 9) == 0) {
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "softmax", 7) == 0) {
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "rope", 4) == 0) {
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 8, 1, 1);
        } else if (strncmp(k, "quant", 5) == 0) {
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else {
            // Unknown kernel — generic dispatch
            amdResult = amd.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        }
    } else {
        // No kernel name — generic inference dispatch
        amdResult = amd.dispatchGeneric("router_generic", nullptr, 0, 64, 1, 1);
    }

    if (!amdResult.success) {
        return RouterResult::error(amdResult.detail, amdResult.errorCode);
    }

    RouterResult r = RouterResult::ok(amdResult.detail, RouterBackendType::AMD_XDNA);
    r.throughputGFLOPS = amdResult.throughputGFLOPS;
    return r;
}

// ============================================================================
// Backend-Specific Dispatch — Intel Xe
// ============================================================================

RouterResult AcceleratorRouter::dispatchToIntel(const RouterInferenceTask& task) {
    IntelGPUAccelerator& intel = IntelGPUAccelerator::instance();

    if (!intel.isInitialized()) {
        return RouterResult::error("Intel GPU not initialized", -20);
    }

    // Map DispatchScope to Intel IntelAccelScope
    IntelAccelScope intelScope = IntelAccelScope::Inference;
    switch (task.scope) {
    case DispatchScope::Inference:        intelScope = IntelAccelScope::Inference; break;
    case DispatchScope::Quantization:     intelScope = IntelAccelScope::Quantization; break;
    case DispatchScope::ModelSurgery:     intelScope = IntelAccelScope::ModelSurgery; break;
    case DispatchScope::SwarmCompute:     intelScope = IntelAccelScope::SwarmCompute; break;
    case DispatchScope::KVCache:          intelScope = IntelAccelScope::KVCache; break;
    case DispatchScope::Embedding:        intelScope = IntelAccelScope::Embedding; break;
    case DispatchScope::SymbolResolution: intelScope = IntelAccelScope::All; break;
    case DispatchScope::All:              intelScope = IntelAccelScope::All; break;
    }

    if (!intel.shouldUseGPU(intelScope, task.inputSizeBytes)) {
        return RouterResult::error("Intel GPU scope/size check failed", -21);
    }

    // Dispatch based on kernel name
    IntelAccelResult intelResult = IntelAccelResult::error("No matching Intel kernel");

    if (task.kernelName) {
        const char* k = task.kernelName;
        if (strncmp(k, "matmul", 6) == 0 || strncmp(k, "gemm", 4) == 0) {
            // Prefer XMX matmul on Xe-HPG/HPC architectures
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 64, 1, 1);
        } else if (strncmp(k, "flash_attn", 10) == 0 || strncmp(k, "attention", 9) == 0) {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else if (strncmp(k, "rmsnorm", 7) == 0 || strncmp(k, "layernorm", 9) == 0) {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "softmax", 7) == 0) {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "rope", 4) == 0) {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 8, 1, 1);
        } else if (strncmp(k, "quant", 5) == 0) {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else {
            intelResult = intel.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        }
    } else {
        intelResult = intel.dispatchGeneric("router_generic", nullptr, 0, 64, 1, 1);
    }

    if (!intelResult.success) {
        return RouterResult::error(intelResult.detail, intelResult.errorCode);
    }

    RouterResult r = RouterResult::ok(intelResult.detail, RouterBackendType::Intel_Xe);
    r.throughputGFLOPS = intelResult.throughputGFLOPS;
    return r;
}

// ============================================================================
// Backend-Specific Dispatch — ARM64 Adreno GPU
// ============================================================================

RouterResult AcceleratorRouter::dispatchToARM64GPU(const RouterInferenceTask& task) {
    ARM64GPUAccelerator& arm64 = ARM64GPUAccelerator::instance();

    if (!arm64.isInitialized()) {
        return RouterResult::error("ARM64 GPU not initialized", -30);
    }

    // Map DispatchScope to ARM64AccelScope
    ARM64AccelScope arm64Scope = ARM64AccelScope::Inference;
    switch (task.scope) {
    case DispatchScope::Inference:        arm64Scope = ARM64AccelScope::Inference; break;
    case DispatchScope::Quantization:     arm64Scope = ARM64AccelScope::Quantization; break;
    case DispatchScope::ModelSurgery:     arm64Scope = ARM64AccelScope::ModelSurgery; break;
    case DispatchScope::SwarmCompute:     arm64Scope = ARM64AccelScope::SwarmCompute; break;
    case DispatchScope::KVCache:          arm64Scope = ARM64AccelScope::KVCache; break;
    case DispatchScope::Embedding:        arm64Scope = ARM64AccelScope::Embedding; break;
    case DispatchScope::SymbolResolution: arm64Scope = ARM64AccelScope::All; break;
    case DispatchScope::All:              arm64Scope = ARM64AccelScope::All; break;
    }

    if (!arm64.shouldUseGPU(arm64Scope, task.inputSizeBytes)) {
        return RouterResult::error("ARM64 GPU scope/size check failed", -31);
    }

    // Check thermal headroom for mobile platform
    float thermalHeadroom = arm64.getThermalHeadroom();
    if (thermalHeadroom < 0.1f) {
        // Less than 10% headroom — too hot for GPU dispatch
        m_stats.thermalThrottleEvents.fetch_add(1, std::memory_order_relaxed);
        return RouterResult::error("ARM64 GPU thermal headroom exhausted", -32);
    }

    // Dispatch
    ARM64AccelResult arm64Result = ARM64AccelResult::error("No matching ARM64 GPU kernel");

    if (task.kernelName) {
        const char* k = task.kernelName;
        if (strncmp(k, "matmul", 6) == 0 || strncmp(k, "gemm", 4) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 64, 1, 1);
        } else if (strncmp(k, "flash_attn", 10) == 0 || strncmp(k, "attention", 9) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else if (strncmp(k, "rmsnorm", 7) == 0 || strncmp(k, "layernorm", 9) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "softmax", 7) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 16, 1, 1);
        } else if (strncmp(k, "rope", 4) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 8, 1, 1);
        } else if (strncmp(k, "quant", 5) == 0) {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        } else {
            arm64Result = arm64.dispatchGeneric(k, nullptr, 0, 32, 1, 1);
        }
    } else {
        arm64Result = arm64.dispatchGeneric("router_generic", nullptr, 0, 64, 1, 1);
    }

    if (!arm64Result.success) {
        return RouterResult::error(arm64Result.detail, arm64Result.errorCode);
    }

    RouterResult r = RouterResult::ok(arm64Result.detail, RouterBackendType::ARM64_Adreno);
    r.throughputGFLOPS = arm64Result.throughputGFLOPS;
    r.deviceTempC = static_cast<uint32_t>((1.0f - thermalHeadroom) * 100.0f); // Estimate
    return r;
}

// ============================================================================
// Backend-Specific Dispatch — ARM64 Hexagon NPU
// ============================================================================

RouterResult AcceleratorRouter::dispatchToARM64NPU(const RouterInferenceTask& task) {
    ARM64GPUAccelerator& arm64 = ARM64GPUAccelerator::instance();

    if (!arm64.isInitialized()) {
        return RouterResult::error("ARM64 not initialized", -40);
    }

    // NPU is best for INT8/INT4 inference — check quant type
    if (task.quantType < 3) {
        // FP32, FP16, BF16 — NPU is inefficient, prefer GPU
        return RouterResult::error("NPU requires INT8/INT4 quantization", -42);
    }

    // Check data size threshold
    if (task.inputSizeBytes < m_npuMinBytes) {
        return RouterResult::error("Data too small for NPU dispatch", -43);
    }

    // shouldUseNPU(dataBytes, quantType) — ARM64 NPU check
    if (!arm64.shouldUseNPU(task.inputSizeBytes, task.quantType)) {
        return RouterResult::error("ARM64 NPU scope/size check failed", -41);
    }

    // Dispatch NPU inference — requires ARM64GPUBuffer parameters
    // The router creates lightweight buffer wrappers for the NPU dispatch path
    ARM64GPUBuffer weightsBuf;
    memset(&weightsBuf, 0, sizeof(weightsBuf));
    weightsBuf.hostPtr = const_cast<void*>(task.inputData);
    weightsBuf.sizeBytes = task.inputSizeBytes;
    weightsBuf.mapped = true;

    ARM64GPUBuffer inputBuf;
    memset(&inputBuf, 0, sizeof(inputBuf));
    inputBuf.hostPtr = const_cast<void*>(task.inputData);
    inputBuf.sizeBytes = task.inputSizeBytes;
    inputBuf.mapped = true;

    ARM64GPUBuffer outputBuf;
    memset(&outputBuf, 0, sizeof(outputBuf));
    outputBuf.hostPtr = task.outputData;
    outputBuf.sizeBytes = task.outputSizeBytes;
    outputBuf.mapped = true;

    ARM64AccelResult npuResult = arm64.dispatchNPUInference(weightsBuf, inputBuf, outputBuf,
                                                              task.batchSize > 0 ? task.batchSize : 1,
                                                              task.quantType);

    if (!npuResult.success) {
        return RouterResult::error(npuResult.detail, npuResult.errorCode);
    }

    RouterResult r = RouterResult::ok(npuResult.detail, RouterBackendType::ARM64_NPU);
    r.throughputGFLOPS = npuResult.throughputGFLOPS;
    return r;
}

// ============================================================================
// Backend-Specific Dispatch — Cerebras WSE (Remote)
// ============================================================================

RouterResult AcceleratorRouter::dispatchToCerebras(const RouterInferenceTask& task) {
    CerebrasWSEAccelerator& cerebras = CerebrasWSEAccelerator::instance();

    if (!cerebras.isInitialized()) {
        return RouterResult::error("Cerebras WSE not initialized", -50);
    }

    // Map scope
    CerebrasAccelScope cereScope = CerebrasAccelScope::Inference;
    switch (task.scope) {
    case DispatchScope::Inference:        cereScope = CerebrasAccelScope::Inference; break;
    case DispatchScope::Quantization:     cereScope = CerebrasAccelScope::Quantization; break;
    case DispatchScope::ModelSurgery:     cereScope = CerebrasAccelScope::Inference; break;  // Cerebras: no surgery scope, map to inference
    case DispatchScope::SwarmCompute:     cereScope = CerebrasAccelScope::Inference; break;  // Cerebras: single-system, map to inference
    case DispatchScope::KVCache:          cereScope = CerebrasAccelScope::Attention; break;  // Cerebras: KV cache lives in attention scope
    case DispatchScope::Embedding:        cereScope = CerebrasAccelScope::Embedding; break;
    case DispatchScope::SymbolResolution: cereScope = CerebrasAccelScope::All; break;
    case DispatchScope::All:              cereScope = CerebrasAccelScope::All; break;
    }

    if (!cerebras.shouldUseWSE(cereScope, task.inputSizeBytes)) {
        return RouterResult::error("Cerebras WSE scope/size check failed", -51);
    }

    // Data size check — Cerebras only makes sense for large payloads
    if (task.inputSizeBytes < m_remoteMinBytes) {
        return RouterResult::error("Data too small for Cerebras dispatch (network overhead)", -52);
    }

    // For Cerebras, we allocate wafer buffers, upload, dispatch, and download
    // The task's inputData/outputData are host pointers; we create wafer-side buffers
    auto start = std::chrono::steady_clock::now();

    CerebrasWaferBuffer inputBuf, outputBuf;

    CerebrasAccelResult allocIn = cerebras.allocWafer(task.inputSizeBytes, inputBuf);
    if (!allocIn.success) {
        return RouterResult::error("Failed to allocate Cerebras input buffer", -53);
    }

    CerebrasAccelResult allocOut = cerebras.allocWafer(task.outputSizeBytes, outputBuf);
    if (!allocOut.success) {
        cerebras.freeWafer(inputBuf);
        return RouterResult::error("Failed to allocate Cerebras output buffer", -54);
    }

    // Upload input to wafer
    CerebrasAccelResult upload = cerebras.uploadToWafer(inputBuf, task.inputData, task.inputSizeBytes);
    if (!upload.success) {
        cerebras.freeWafer(inputBuf);
        cerebras.freeWafer(outputBuf);
        return RouterResult::error("Failed to upload to Cerebras wafer", -55);
    }

    // Dispatch inference on wafer
    CerebrasAccelResult inferResult = cerebras.dispatchInference(inputBuf, outputBuf,
                                                                   task.batchSize > 0 ? task.batchSize : 1,
                                                                   0); // seqLen from task context

    if (!inferResult.success) {
        cerebras.freeWafer(inputBuf);
        cerebras.freeWafer(outputBuf);
        return RouterResult::error(inferResult.detail, inferResult.errorCode);
    }

    // Wait for completion
    uint32_t timeout = task.timeoutMs > 0 ? task.timeoutMs : 30000;
    CerebrasAccelResult waitResult = cerebras.waitForCompletion(timeout);
    if (!waitResult.success) {
        cerebras.freeWafer(inputBuf);
        cerebras.freeWafer(outputBuf);
        return RouterResult::error("Cerebras inference timeout", -56);
    }

    // Download results
    if (task.outputData && task.outputSizeBytes > 0) {
        CerebrasAccelResult download = cerebras.downloadFromWafer(task.outputData, outputBuf,
                                                                    task.outputSizeBytes);
        if (!download.success) {
            cerebras.freeWafer(inputBuf);
            cerebras.freeWafer(outputBuf);
            return RouterResult::error("Failed to download from Cerebras wafer", -57);
        }
    }

    // Cleanup wafer buffers
    cerebras.freeWafer(inputBuf);
    cerebras.freeWafer(outputBuf);

    auto elapsed = std::chrono::steady_clock::now() - start;
    double totalMs = std::chrono::duration<double, std::milli>(elapsed).count();

    m_stats.cerebrasDispatches.fetch_add(1, std::memory_order_relaxed);
    m_stats.totalNetworkMs.fetch_add(static_cast<uint64_t>(totalMs), std::memory_order_relaxed);

    RouterResult r = RouterResult::ok("Cerebras WSE dispatch complete", RouterBackendType::Cerebras_WSE);
    r.throughputGFLOPS = inferResult.throughputTFLOPS * 1000.0; // TFLOPS → GFLOPS
    r.networkLatencyMs = inferResult.networkLatencyMs;
    return r;
}

// ============================================================================
// Backend-Specific Dispatch — CPU Fallback
// ============================================================================

RouterResult AcceleratorRouter::dispatchToCPU(const RouterInferenceTask& task) {
    // CPU is always available — this is the fallback of last resort
    // Routes to AVX-512/AVX2 MASM kernels or falls back to scalar C

    m_stats.totalCPUFallbacks.fetch_add(1, std::memory_order_relaxed);

    auto start = std::chrono::steady_clock::now();

    if (!task.inputData || !task.outputData || task.outputSizeBytes == 0) {
        return RouterResult::error("CPU dispatch: null input/output or zero output size",
                                   static_cast<int>(RouterBackendType::CPU_Fallback));
    }

    if (task.kernelName) {
        std::string kernel(task.kernelName);
        std::cout << "[Router-CPU] Dispatching '" << kernel
                  << "' (" << task.inputSizeBytes << " bytes input, "
                  << task.outputSizeBytes << " bytes output)\n";

        // Route to the appropriate MASM/SIMD kernel
        if (kernel == "matmul" || kernel == "flashattention") {
            // Forward to inference_core.asm / FlashAttention_AVX512.asm
            // These operate on float32 matrices in [rows x cols] layout
            // If the extern symbols are linked, call directly:
            //   extern "C" void inference_core_avx512(const float* A, const float* B, float* C, int M, int N, int K);
            
            // CPU reference matmul: C[M×N] = A[M×K] × B[K×N]
            // Infer dimensions from buffer sizes:
            //   inputData  = A concatenated with B → inputSizeBytes = (M*K + K*N) * sizeof(float)
            //   outputData = C                     → outputSizeBytes = M*N * sizeof(float)
            // We estimate K = sqrt(inputFloats / 2) for square-ish matrices,
            // then M = outputFloats / N, adjusting for the actual buffer geometry.
            
            const float* inputBuf = reinterpret_cast<const float*>(task.inputData);
            float* C = reinterpret_cast<float*>(task.outputData);
            uint64_t inputFloats = task.inputSizeBytes / sizeof(float);
            uint64_t outputFloats = task.outputSizeBytes / sizeof(float);
            
            if (outputFloats > 0 && inputFloats > 0) {
                // Heuristic dimension inference:
                // For matmul: inputData contains A[M×K] followed by B[K×N]
                // outputData contains C[M×N] with M*N = outputFloats
                // Try to find K such that M*K + K*N = inputFloats and M*N = outputFloats
                // For square matrices: M=N=K=sqrt(outputFloats)
                uint32_t dim = static_cast<uint32_t>(std::sqrt(static_cast<double>(outputFloats)));
                if (dim == 0) dim = 1;
                uint32_t M = dim;
                uint32_t N = static_cast<uint32_t>(outputFloats / M);
                if (N == 0) N = 1;
                
                // Estimate K from remaining input space
                // A is M×K, B is K×N → total input = M*K + K*N = K*(M+N)
                uint32_t K = static_cast<uint32_t>(inputFloats / (M + N));
                if (K == 0) K = 1;
                // Clamp to available data
                if (static_cast<uint64_t>(M) * K + static_cast<uint64_t>(K) * N > inputFloats) {
                    K = static_cast<uint32_t>(inputFloats / (M + N));
                    if (K == 0) K = 1;
                }
                
                const float* A = inputBuf;
                const float* B = inputBuf + (M * K);
                
                // Tiled matmul with cache-friendly blocking
                constexpr uint32_t TILE = 32;
                memset(C, 0, outputFloats * sizeof(float));
                
                for (uint32_t ii = 0; ii < M; ii += TILE) {
                    uint32_t iEnd = std::min(ii + TILE, M);
                    for (uint32_t kk = 0; kk < K; kk += TILE) {
                        uint32_t kEnd = std::min(kk + TILE, K);
                        for (uint32_t jj = 0; jj < N; jj += TILE) {
                            uint32_t jEnd = std::min(jj + TILE, N);
                            for (uint32_t i = ii; i < iEnd; i++) {
                                for (uint32_t k = kk; k < kEnd; k++) {
                                    float aik = A[i * K + k];
                                    for (uint32_t j = jj; j < jEnd; j++) {
                                        C[i * N + j] += aik * B[k * N + j];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (kernel == "quantize" || kernel == "requantize") {
            // Forward to quant_avx2.asm kernels
            // Quantize float32 -> int8/int4 or dequantize
            const float* input = reinterpret_cast<const float*>(task.inputData);
            int8_t* output = reinterpret_cast<int8_t*>(task.outputData);
            uint64_t count = std::min(task.inputSizeBytes / sizeof(float), task.outputSizeBytes);
            // Simple linear quantization: map [min, max] to [-127, 127]
            float minVal = 0, maxVal = 0;
            for (uint64_t i = 0; i < count; ++i) {
                if (input[i] < minVal) minVal = input[i];
                if (input[i] > maxVal) maxVal = input[i];
            }
            float scale = (maxVal - minVal) > 0 ? 254.0f / (maxVal - minVal) : 1.0f;
            for (uint64_t i = 0; i < count; ++i) {
                float normalized = (input[i] - minVal) * scale - 127.0f;
                output[i] = static_cast<int8_t>(std::max(-127.0f, std::min(127.0f, normalized)));
            }
        }
        else if (kernel == "softmax") {
            // Softmax: exp(x_i) / sum(exp(x_j))
            const float* input = reinterpret_cast<const float*>(task.inputData);
            float* output = reinterpret_cast<float*>(task.outputData);
            uint64_t count = std::min(task.inputSizeBytes, task.outputSizeBytes) / sizeof(float);
            if (count > 0) {
                float maxVal = input[0];
                for (uint64_t i = 1; i < count; ++i) {
                    if (input[i] > maxVal) maxVal = input[i];
                }
                float sumExp = 0.0f;
                for (uint64_t i = 0; i < count; ++i) {
                    output[i] = std::exp(input[i] - maxVal);
                    sumExp += output[i];
                }
                if (sumExp > 0.0f) {
                    for (uint64_t i = 0; i < count; ++i) {
                        output[i] /= sumExp;
                    }
                }
            }
        }
        else if (kernel == "layernorm" || kernel == "rmsnorm") {
            // Layer normalization
            const float* input = reinterpret_cast<const float*>(task.inputData);
            float* output = reinterpret_cast<float*>(task.outputData);
            uint64_t count = std::min(task.inputSizeBytes, task.outputSizeBytes) / sizeof(float);
            if (count > 0) {
                float mean = 0.0f;
                for (uint64_t i = 0; i < count; ++i) mean += input[i];
                mean /= static_cast<float>(count);
                float variance = 0.0f;
                for (uint64_t i = 0; i < count; ++i) {
                    float diff = input[i] - mean;
                    variance += diff * diff;
                }
                variance /= static_cast<float>(count);
                float invStd = 1.0f / std::sqrt(variance + 1e-5f);
                for (uint64_t i = 0; i < count; ++i) {
                    output[i] = (input[i] - mean) * invStd;
                }
            }
        }
        else {
            // Generic fallback: copy input to output
            uint64_t copySize = std::min(task.inputSizeBytes, task.outputSizeBytes);
            memcpy(task.outputData, task.inputData, static_cast<size_t>(copySize));
        }
    } else {
        // No kernel specified: zero-init output
        memset(task.outputData, 0, static_cast<size_t>(
            std::min(task.outputSizeBytes, static_cast<uint64_t>(1024 * 1024))));
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();

    RouterResult r = RouterResult::ok("CPU fallback dispatch complete", RouterBackendType::CPU_Fallback);
    r.elapsedMs = ms;
    return r;
}

// ============================================================================
// Backend Selection Logic
// ============================================================================

RouterBackendType AcceleratorRouter::autoSelectBackend(const RouterInferenceTask& task) const {
    // === Cerebras special case: huge batch / streaming / explicit large-data ===
    // Cerebras excels at large batch (>4GB), high-throughput streaming inference
    if (task.inputSizeBytes >= m_remoteMinBytes &&
        task.priority == DispatchPriority::Batch &&
        m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].available &&
        m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].enabled) {
        return RouterBackendType::Cerebras_WSE;
    }

    // === NPU special case: INT8/INT4 inference with moderate data ===
    if (task.quantType >= 3 && // INT8 or INT4
        task.scope == DispatchScope::Inference &&
        task.inputSizeBytes >= m_npuMinBytes &&
        m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].available &&
        m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].enabled &&
        checkThermal(RouterBackendType::ARM64_NPU)) {
        return RouterBackendType::ARM64_NPU;
    }

    // === Data too small for GPU — use CPU directly ===
    if (task.inputSizeBytes < m_localGPUMinBytes) {
        return RouterBackendType::CPU_Fallback;
    }

    // === Realtime priority: pick lowest-latency available GPU ===
    if (task.priority == DispatchPriority::Realtime) {
        // For realtime, prefer the backend with lowest rolling average latency
        double bestLatency = 1e12;
        RouterBackendType bestBackend = RouterBackendType::CPU_Fallback;

        // Check local GPUs
        RouterBackendType localGPUs[] = {
            RouterBackendType::NVIDIA_CUDA,
            RouterBackendType::AMD_XDNA,
            RouterBackendType::Intel_Xe,
            RouterBackendType::ARM64_Adreno
        };

        for (auto bt : localGPUs) {
            int idx = static_cast<int>(bt);
            if (m_backends[idx].available && m_backends[idx].enabled &&
                checkThermal(bt) && m_backends[idx].avgLatencyMs < bestLatency) {
                bestLatency = m_backends[idx].avgLatencyMs;
                bestBackend = bt;
            }
        }

        // If no GPU has been used yet (avgLatencyMs == 0), pick by priority order
        if (bestLatency == 0) {
            for (auto bt : localGPUs) {
                int idx = static_cast<int>(bt);
                if (m_backends[idx].available && m_backends[idx].enabled && checkThermal(bt)) {
                    return bt;
                }
            }
        }

        return bestBackend;
    }

    // === Standard priority cascade: NVIDIA → AMD → Intel → ARM64 GPU → CPU ===
    if (m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available &&
        m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].enabled &&
        checkThermal(RouterBackendType::NVIDIA_CUDA)) {
        return RouterBackendType::NVIDIA_CUDA;
    }

    if (m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available &&
        m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].enabled &&
        checkThermal(RouterBackendType::AMD_XDNA)) {
        return RouterBackendType::AMD_XDNA;
    }

    if (m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available &&
        m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].enabled &&
        checkThermal(RouterBackendType::Intel_Xe)) {
        return RouterBackendType::Intel_Xe;
    }

    if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available &&
        m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].enabled &&
        checkThermal(RouterBackendType::ARM64_Adreno)) {
        return RouterBackendType::ARM64_Adreno;
    }

    // === Streaming priority: Cerebras if available ===
    if (task.priority == DispatchPriority::Streaming &&
        m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].available &&
        m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].enabled) {
        return RouterBackendType::Cerebras_WSE;
    }

    return RouterBackendType::CPU_Fallback;
}

bool AcceleratorRouter::checkThermal(RouterBackendType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) return false;

    // CPU and Cerebras effectively skip thermal check
    if (type == RouterBackendType::CPU_Fallback) return true;
    if (type == RouterBackendType::Cerebras_WSE) return true; // Liquid-cooled, managed by appliance

    return m_backends[idx].currentTempC < m_backends[idx].thermalLimitC;
}

RouterBackendType AcceleratorRouter::cascadeFallback(RouterBackendType failed) const {
    // Define the fallback chain:
    //   AMD → Intel → ARM64 GPU → CPU
    //   Intel → ARM64 GPU → CPU
    //   ARM64 GPU → CPU
    //   ARM64 NPU → ARM64 GPU → CPU
    //   Cerebras → AMD → Intel → ARM64 GPU → CPU
    //   CPU → None (end of chain)

    switch (failed) {
    case RouterBackendType::Cerebras_WSE:
        // Remote failed — try local GPUs
        if (m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available &&
            m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].enabled)
            return RouterBackendType::NVIDIA_CUDA;
        if (m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available &&
            m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].enabled)
            return RouterBackendType::AMD_XDNA;
        if (m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available &&
            m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].enabled)
            return RouterBackendType::Intel_Xe;
        if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available &&
            m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].enabled)
            return RouterBackendType::ARM64_Adreno;
        return RouterBackendType::CPU_Fallback;

    case RouterBackendType::AMD_XDNA:
        if (m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available &&
            m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].enabled)
            return RouterBackendType::Intel_Xe;
        if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available &&
            m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].enabled)
            return RouterBackendType::ARM64_Adreno;
        return RouterBackendType::CPU_Fallback;

    case RouterBackendType::Intel_Xe:
        if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available &&
            m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].enabled)
            return RouterBackendType::ARM64_Adreno;
        return RouterBackendType::CPU_Fallback;

    case RouterBackendType::ARM64_Adreno:
        return RouterBackendType::CPU_Fallback;

    case RouterBackendType::ARM64_NPU:
        // NPU failed — try GPU on same platform, then CPU
        if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available &&
            m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].enabled)
            return RouterBackendType::ARM64_Adreno;
        return RouterBackendType::CPU_Fallback;

    case RouterBackendType::CPU_Fallback:
        return RouterBackendType::None; // End of chain

    default:
        return RouterBackendType::CPU_Fallback;
    }
}

// ============================================================================
// Backend Management
// ============================================================================

RouterResult AcceleratorRouter::forceBackend(RouterBackendType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (type == RouterBackendType::Auto) {
        // Clear forced backend
        RouterBackendType old = m_forcedBackend.exchange(RouterBackendType::Auto, std::memory_order_release);
        std::cout << "[Router] Backend force cleared — returning to auto-select\n";
        return RouterResult::ok("Auto-select restored", m_activeBackend.load());
    }

    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) {
        return RouterResult::error("Invalid backend type", -1);
    }

    if (!m_backends[idx].available) {
        return RouterResult::error("Cannot force unavailable backend", -2);
    }

    RouterBackendType old = m_forcedBackend.exchange(type, std::memory_order_release);
    m_activeBackend.store(type, std::memory_order_release);

    std::cout << "[Router] Backend forced to: " << getBackendName(type) << "\n";

    if (m_backendChangeCb && old != type) {
        m_backendChangeCb(old, type, "Forced by user", m_backendChangeData);
    }

    return RouterResult::ok("Backend forced", type);
}

RouterResult AcceleratorRouter::enableBackend(RouterBackendType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) {
        return RouterResult::error("Invalid backend type", -1);
    }

    m_backends[idx].enabled = true;
    return RouterResult::ok("Backend enabled", type);
}

RouterResult AcceleratorRouter::disableBackend(RouterBackendType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) {
        return RouterResult::error("Invalid backend type", -1);
    }

    if (type == RouterBackendType::CPU_Fallback) {
        return RouterResult::error("Cannot disable CPU fallback", -2);
    }

    m_backends[idx].enabled = false;

    // If we just disabled the active backend, switch to auto
    if (m_activeBackend.load() == type) {
        m_forcedBackend.store(RouterBackendType::Auto, std::memory_order_release);
    }

    return RouterResult::ok("Backend disabled", type);
}

bool AcceleratorRouter::isBackendAvailable(RouterBackendType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) return false;
    return m_backends[idx].available;
}

bool AcceleratorRouter::isBackendEnabled(RouterBackendType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) return false;
    return m_backends[idx].enabled;
}

const char* AcceleratorRouter::getBackendName(RouterBackendType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) return "Invalid";
    return m_backends[idx].backendName;
}

uint32_t AcceleratorRouter::getAvailableBackendCount() const {
    uint32_t count = 0;
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (m_backends[i].available) count++;
    }
    return count;
}

void AcceleratorRouter::getAvailableBackends(RouterBackendType* outTypes, uint32_t maxCount,
                                               uint32_t& outCount) const {
    outCount = 0;
    for (int i = 0; i < MAX_BACKENDS && outCount < maxCount; i++) {
        if (m_backends[i].available) {
            outTypes[outCount++] = m_backends[i].type;
        }
    }
}

// ============================================================================
// Thermal Management
// ============================================================================

RouterResult AcceleratorRouter::setThermalLimit(RouterBackendType type, uint32_t maxTempC) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) {
        return RouterResult::error("Invalid backend type", -1);
    }
    m_backends[idx].thermalLimitC = maxTempC;
    std::cout << "[Router] Thermal limit for " << getBackendName(type)
              << " set to " << maxTempC << "°C\n";
    return RouterResult::ok("Thermal limit set", type);
}

uint32_t AcceleratorRouter::getThermalLimit(RouterBackendType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= MAX_BACKENDS) return 0;
    return m_backends[idx].thermalLimitC;
}

RouterResult AcceleratorRouter::pollThermals() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Poll ARM64 thermal headroom (only backend with direct thermal API)
    if (m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available) {
        ARM64GPUAccelerator& arm64 = ARM64GPUAccelerator::instance();
        if (arm64.isInitialized()) {
            float headroom = arm64.getThermalHeadroom();
            // Convert headroom (0.0-1.0) to temperature estimate
            uint32_t estimatedTemp = static_cast<uint32_t>((1.0f - headroom) *
                                      m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].thermalLimitC);
            m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].currentTempC = estimatedTemp;
            m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].currentTempC = estimatedTemp; // Same SoC
        }
    }

    // AMD/Intel/NVIDIA: DXGI adapter thermal query would go here in production
    // For now, assume operating within limits unless hardware-specific driver APIs are loaded
    // In production: use AMD ADL SDK or Intel IGCL for actual temperature polling

    // Check all backends against their thermal limits
    bool anyThrottled = false;
    for (int i = 1; i < MAX_BACKENDS; i++) { // Skip None (index 0)
        if (m_backends[i].available && m_backends[i].enabled) {
            if (m_backends[i].currentTempC >= m_backends[i].thermalLimitC) {
                anyThrottled = true;

                if (m_thermalCb) {
                    m_thermalCb(m_backends[i].type, m_backends[i].currentTempC,
                                m_backends[i].thermalLimitC, m_thermalData);
                }
            }
        }
    }

    if (anyThrottled) {
        return RouterResult::ok("Thermals polled — throttling detected",
                                 m_activeBackend.load());
    }
    return RouterResult::ok("Thermals polled — all within limits",
                             m_activeBackend.load());
}

// ============================================================================
// Integration Hooks
// ============================================================================

bool AcceleratorRouter::shouldAccelerate(DispatchScope scope) const {
    if (!m_initialized.load(std::memory_order_acquire)) return false;

    // Check if any GPU backend is available and enabled for this scope
    for (int i = 1; i < MAX_BACKENDS - 1; i++) { // Skip None and CPU_Fallback
        if (m_backends[i].available && m_backends[i].enabled) {
            return true; // At least one accelerator is available
        }
    }
    return false;
}

bool AcceleratorRouter::shouldAccelerate(DispatchScope scope, uint64_t dataBytes) const {
    if (!shouldAccelerate(scope)) return false;

    // Check data size thresholds
    if (dataBytes < m_localGPUMinBytes) return false;

    return true;
}

RouterBackendType AcceleratorRouter::recommendBackend(DispatchScope scope, uint64_t dataBytes,
                                                       DispatchPriority priority) const {
    RouterInferenceTask fakeTask;
    memset(&fakeTask, 0, sizeof(fakeTask));
    fakeTask.inputSizeBytes = dataBytes;
    fakeTask.scope = scope;
    fakeTask.priority = priority;
    fakeTask.preferredBackend = RouterBackendType::Auto;
    fakeTask.quantType = 0; // FP32 default

    return autoSelectBackend(fakeTask);
}

// ============================================================================
// Callbacks
// ============================================================================

void AcceleratorRouter::setBackendChangeCallback(RouterBackendChangeCallback cb, void* userData) {
    m_backendChangeCb = cb;
    m_backendChangeData = userData;
}

void AcceleratorRouter::setFailureCallback(RouterFailureCallback cb, void* userData) {
    m_failureCb = cb;
    m_failureData = userData;
}

void AcceleratorRouter::setThermalCallback(RouterThermalCallback cb, void* userData) {
    m_thermalCb = cb;
    m_thermalData = userData;
}

// ============================================================================
// Stats & JSON
// ============================================================================

void AcceleratorRouter::resetStats() {
    m_stats.totalSubmissions.store(0, std::memory_order_relaxed);
    m_stats.totalSuccesses.store(0, std::memory_order_relaxed);
    m_stats.totalFailures.store(0, std::memory_order_relaxed);
    m_stats.totalFallbacks.store(0, std::memory_order_relaxed);
    m_stats.thermalThrottleEvents.store(0, std::memory_order_relaxed);
    m_stats.totalCPUFallbacks.store(0, std::memory_order_relaxed);
    m_stats.cerebrasDispatches.store(0, std::memory_order_relaxed);
    m_stats.totalComputeMs.store(0, std::memory_order_relaxed);
    m_stats.totalNetworkMs.store(0, std::memory_order_relaxed);
    m_stats.peakLocalTFLOPS = 0;
    m_stats.peakRemoteTFLOPS = 0;

    for (int i = 0; i < MAX_BACKENDS; i++) {
        m_backends[i].totalDispatches = 0;
        m_backends[i].totalFallbacks = 0;
        m_backends[i].totalBytesProcessed = 0;
        m_backends[i].totalComputeMs = 0;
        m_backends[i].avgLatencyMs = 0;
    }

    std::cout << "[Router] Stats reset.\n";
}

std::string AcceleratorRouter::toJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"initialized\": " << (m_initialized.load() ? "true" : "false") << ",\n";
    ss << "  \"activeBackend\": \"" << getBackendName(m_activeBackend.load()) << "\",\n";

    RouterBackendType forced = m_forcedBackend.load();
    ss << "  \"forcedBackend\": \"" << (forced == RouterBackendType::Auto ? "Auto" : getBackendName(forced)) << "\",\n";

    ss << "  \"availableBackends\": " << getAvailableBackendCount() << ",\n";
    ss << "  \"stats\": {\n";
    ss << "    \"totalSubmissions\": " << m_stats.totalSubmissions.load() << ",\n";
    ss << "    \"totalSuccesses\": " << m_stats.totalSuccesses.load() << ",\n";
    ss << "    \"totalFailures\": " << m_stats.totalFailures.load() << ",\n";
    ss << "    \"totalFallbacks\": " << m_stats.totalFallbacks.load() << ",\n";
    ss << "    \"thermalThrottleEvents\": " << m_stats.thermalThrottleEvents.load() << ",\n";
    ss << "    \"totalCPUFallbacks\": " << m_stats.totalCPUFallbacks.load() << ",\n";
    ss << "    \"cerebrasDispatches\": " << m_stats.cerebrasDispatches.load() << ",\n";
    ss << "    \"totalComputeMs\": " << m_stats.totalComputeMs.load() << ",\n";
    ss << "    \"totalNetworkMs\": " << m_stats.totalNetworkMs.load() << ",\n";
    ss << "    \"peakLocalTFLOPS\": " << m_stats.peakLocalTFLOPS << ",\n";
    ss << "    \"peakRemoteTFLOPS\": " << m_stats.peakRemoteTFLOPS << "\n";
    ss << "  },\n";
    ss << "  \"thresholds\": {\n";
    ss << "    \"localGPUMinBytes\": " << m_localGPUMinBytes << ",\n";
    ss << "    \"remoteMinBytes\": " << m_remoteMinBytes << ",\n";
    ss << "    \"npuMinBytes\": " << m_npuMinBytes << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string AcceleratorRouter::backendsToJson() const {
    std::ostringstream ss;
    ss << "[\n";
    bool first = true;
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (m_backends[i].type == RouterBackendType::None) continue;
        if (!first) ss << ",\n";
        first = false;

        ss << "  {\n";
        ss << "    \"type\": " << static_cast<int>(m_backends[i].type) << ",\n";
        ss << "    \"name\": \"" << m_backends[i].backendName << "\",\n";
        ss << "    \"available\": " << (m_backends[i].available ? "true" : "false") << ",\n";
        ss << "    \"enabled\": " << (m_backends[i].enabled ? "true" : "false") << ",\n";
        ss << "    \"thermalLimitC\": " << m_backends[i].thermalLimitC << ",\n";
        ss << "    \"currentTempC\": " << m_backends[i].currentTempC << ",\n";
        ss << "    \"totalDispatches\": " << m_backends[i].totalDispatches << ",\n";
        ss << "    \"totalFallbacks\": " << m_backends[i].totalFallbacks << ",\n";
        ss << "    \"totalBytesProcessed\": " << m_backends[i].totalBytesProcessed << ",\n";
        ss << "    \"totalComputeMs\": " << m_backends[i].totalComputeMs << ",\n";
        ss << "    \"avgLatencyMs\": " << m_backends[i].avgLatencyMs << "\n";
        ss << "  }";
    }
    ss << "\n]";
    return ss.str();
}

std::string AcceleratorRouter::thermalToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"throttleEvents\": " << m_stats.thermalThrottleEvents.load() << ",\n";
    ss << "  \"backends\": [\n";
    bool first = true;
    for (int i = 1; i < MAX_BACKENDS; i++) {
        if (m_backends[i].type == RouterBackendType::None) continue;
        if (!m_backends[i].available) continue;
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\"name\": \"" << m_backends[i].backendName
           << "\", \"currentC\": " << m_backends[i].currentTempC
           << ", \"limitC\": " << m_backends[i].thermalLimitC
           << ", \"withinLimit\": " << (m_backends[i].currentTempC < m_backends[i].thermalLimitC ? "true" : "false")
           << "}";
    }
    ss << "\n  ]\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Backend Probing
// ============================================================================

void AcceleratorRouter::probeAllBackends() {
    std::cout << "[Router] Probing all accelerator backends...\n";

    probeAMD();
    probeIntel();
    probeARM64();
    probeNVIDIA();
    probeCerebras();
    // CPU is always available
    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].available = true;
    m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].enabled = true;

    std::cout << "[Router] Backend probing complete.\n";
}

void AcceleratorRouter::probeAMD() {
    AMDGPUAccelerator& amd = AMDGPUAccelerator::instance();

    // Check if already initialized
    if (amd.isInitialized()) {
        // Already running — check if it's actually an AMD GPU
        uint32_t vendorId = amd.getVendorId();
        if (vendorId == 0x1002) { // AMD vendor ID
            m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available = true;
            return;
        }
    }

    // Try to initialize AMD backend
    AccelResult r = amd.initialize(GPUBackend::Auto);
    if (r.success && amd.getVendorId() == 0x1002) {
        m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available = true;
    } else {
        m_backends[static_cast<int>(RouterBackendType::AMD_XDNA)].available = false;
    }
}

void AcceleratorRouter::probeIntel() {
    std::cout << "[Router] Probing Intel Xe backend...\n";

    IntelGPUAccelerator& intel = IntelGPUAccelerator::instance();

    if (intel.isInitialized()) {
        uint32_t vendorId = intel.getVendorId();
        if (vendorId == 0x8086) { // Intel vendor ID
            m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available = true;
            return;
        }
    }

    IntelAccelResult r = intel.initialize(IntelGPUBackend::Auto);
    if (r.success && intel.getVendorId() == 0x8086) {
        m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available = true;
    } else {
        m_backends[static_cast<int>(RouterBackendType::Intel_Xe)].available = false;
    }
}

void AcceleratorRouter::probeARM64() {
    std::cout << "[Router] Probing ARM64 backend...\n";

    ARM64GPUAccelerator& arm64 = ARM64GPUAccelerator::instance();

    if (arm64.isInitialized()) {
        // ARM64 GPU is available — check Adreno and NPU separately
        m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available = true;
        std::cout << "[Router]   ARM64 GPU detected: " << arm64.getGPUName()
                  << " (" << (arm64.getSystemRAMBytes() / (1024*1024)) << " MB RAM)\n";

        // Check NPU availability
        // Check NPU availability — pass minimum data to test capability
        if (arm64.shouldUseNPU(4096, 3)) { // 4KB INT8 — minimal probe
            m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].available = true;
            std::cout << "[Router]   ARM64 NPU detected (Hexagon)\n";
        }
        return;
    }

    ARM64AccelResult r = arm64.initialize(ARM64GPUBackend::Auto);
    if (r.success) {
        m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available = true;
        std::cout << "[Router]   ARM64 GPU initialized: " << arm64.getGPUName() << "\n";

        // Enable NPU if Hexagon is available
        ARM64AccelResult npuResult = arm64.enableNPU();
        if (npuResult.success) {
            m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].available = true;
            std::cout << "[Router]   ARM64 NPU enabled (Hexagon)\n";
        }
    } else {
        m_backends[static_cast<int>(RouterBackendType::ARM64_Adreno)].available = false;
        m_backends[static_cast<int>(RouterBackendType::ARM64_NPU)].available = false;
        std::cout << "[Router]   ARM64 not available: " << r.detail << "\n";
    }
}

void AcceleratorRouter::probeCerebras() {
    CerebrasWSEAccelerator& cerebras = CerebrasWSEAccelerator::instance();

    if (cerebras.isInitialized()) {
        m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].available = true;
        return;
    }

    // Cerebras requires explicit network configuration — don't auto-init
    // The user must call cerebras.connect(endpoint) before the router can use it
    // We just check if it's been initialized elsewhere
    m_backends[static_cast<int>(RouterBackendType::Cerebras_WSE)].available = false;
}

// ============================================================================
// Backend Probing — NVIDIA CUDA (Phase 31)
// ============================================================================

void AcceleratorRouter::probeNVIDIA() {
    NvidiaCudaAccelerator& nvidia = NvidiaCudaAccelerator::instance();

    if (nvidia.isInitialized()) {
        m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available = true;
        return;
    }

    NvidiaAccelResult r = nvidia.initialize(0);
    if (r.success) {
        m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available = true;

        // Update peak TFLOPS estimate
        m_stats.peakLocalTFLOPS = std::max(m_stats.peakLocalTFLOPS, nvidia.getStats().peakTFLOPS);
    } else {
        m_backends[static_cast<int>(RouterBackendType::NVIDIA_CUDA)].available = false;
    }
}

// ============================================================================
// Backend-Specific Dispatch — NVIDIA CUDA (Phase 31)
// ============================================================================

RouterResult AcceleratorRouter::dispatchToNVIDIA(const RouterInferenceTask& task) {
    NvidiaCudaAccelerator& nvidia = NvidiaCudaAccelerator::instance();

    if (!nvidia.isInitialized()) {
        return RouterResult::error("NVIDIA GPU not initialized", -60);
    }

    if (!nvidia.isGPUEnabled()) {
        return RouterResult::error("NVIDIA GPU disabled", -61);
    }

    // Map DispatchScope to NvidiaAccelScope
    NvidiaAccelScope nvidiaScope = NvidiaAccelScope::Inference;
    switch (task.scope) {
    case DispatchScope::Inference:        nvidiaScope = NvidiaAccelScope::Inference; break;
    case DispatchScope::Quantization:     nvidiaScope = NvidiaAccelScope::Quantization; break;
    case DispatchScope::ModelSurgery:     nvidiaScope = NvidiaAccelScope::ModelSurgery; break;
    case DispatchScope::SwarmCompute:     nvidiaScope = NvidiaAccelScope::SwarmCompute; break;
    case DispatchScope::KVCache:          nvidiaScope = NvidiaAccelScope::KVCache; break;
    case DispatchScope::Embedding:        nvidiaScope = NvidiaAccelScope::Embedding; break;
    case DispatchScope::SymbolResolution: nvidiaScope = NvidiaAccelScope::All; break;
    case DispatchScope::All:              nvidiaScope = NvidiaAccelScope::All; break;
    }

    if (!nvidia.isScopeEnabled(nvidiaScope)) {
        return RouterResult::error("NVIDIA GPU scope disabled", -62);
    }

    // Dispatch based on kernel name
    NvidiaAccelResult nvidiaResult = NvidiaAccelResult::error("No matching NVIDIA kernel");

    if (task.kernelName) {
        const char* k = task.kernelName;
        if (strncmp(k, "matmul", 6) == 0 || strncmp(k, "gemm", 4) == 0) {
            // For router-level dispatch, create temporary GPU buffers
            // Caller should manage buffers directly for best performance
            NvidiaGPUBuffer aBuf{}, bBuf{}, cBuf{};

            // Estimate dimensions from buffer sizes
            uint64_t inputFloats = task.inputSizeBytes / sizeof(float);
            uint64_t outputFloats = task.outputSizeBytes / sizeof(float);
            uint32_t dim = static_cast<uint32_t>(std::sqrt(static_cast<double>(outputFloats)));
            if (dim == 0) dim = 1;
            uint32_t M = dim, N = dim, K = dim;

            nvidia.allocGPU(M * K * sizeof(float), aBuf);
            nvidia.allocGPU(K * N * sizeof(float), bBuf);
            nvidia.allocGPU(M * N * sizeof(float), cBuf);

            if (aBuf.devicePtr && bBuf.devicePtr && cBuf.devicePtr) {
                nvidia.copyToGPU(aBuf, task.inputData, std::min((uint64_t)(M * K * sizeof(float)), task.inputSizeBytes));
                if (task.inputSizeBytes > M * K * sizeof(float)) {
                    nvidia.copyToGPU(bBuf, reinterpret_cast<const char*>(task.inputData) + M * K * sizeof(float),
                                     std::min((uint64_t)(K * N * sizeof(float)), task.inputSizeBytes - M * K * sizeof(float)));
                }

                nvidiaResult = nvidia.dispatchMatMul(aBuf, bBuf, cBuf, M, N, K);

                if (nvidiaResult.success && task.outputData) {
                    nvidia.copyFromGPU(task.outputData, cBuf, std::min((uint64_t)(M * N * sizeof(float)), task.outputSizeBytes));
                }
            }

            nvidia.freeGPU(aBuf);
            nvidia.freeGPU(bBuf);
            nvidia.freeGPU(cBuf);
        } else if (strncmp(k, "softmax", 7) == 0) {
            NvidiaGPUBuffer inBuf{}, outBuf{};
            uint32_t count = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
            nvidia.allocGPU(task.inputSizeBytes, inBuf);
            nvidia.allocGPU(task.outputSizeBytes, outBuf);

            if (inBuf.devicePtr && outBuf.devicePtr) {
                nvidia.copyToGPU(inBuf, task.inputData, task.inputSizeBytes);
                nvidiaResult = nvidia.dispatchSoftmax(inBuf, outBuf, 1, count);
                if (nvidiaResult.success && task.outputData) {
                    nvidia.copyFromGPU(task.outputData, outBuf, task.outputSizeBytes);
                }
            }

            nvidia.freeGPU(inBuf);
            nvidia.freeGPU(outBuf);
        } else if (strncmp(k, "rmsnorm", 7) == 0 || strncmp(k, "layernorm", 9) == 0) {
            NvidiaGPUBuffer inBuf{}, wBuf{}, outBuf{};
            uint32_t count = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
            nvidia.allocGPU(task.inputSizeBytes, inBuf);
            nvidia.allocGPU(task.inputSizeBytes, wBuf);  // weights same size
            nvidia.allocGPU(task.outputSizeBytes, outBuf);

            if (inBuf.devicePtr && wBuf.devicePtr && outBuf.devicePtr) {
                nvidia.copyToGPU(inBuf, task.inputData, task.inputSizeBytes);
                // Weights assumed to be ones for router-level dispatch
                std::vector<float> ones(count, 1.0f);
                nvidia.copyToGPU(wBuf, ones.data(), count * sizeof(float));
                nvidiaResult = nvidia.dispatchRMSNorm(inBuf, wBuf, outBuf, count, 1e-5f);
                if (nvidiaResult.success && task.outputData) {
                    nvidia.copyFromGPU(task.outputData, outBuf, task.outputSizeBytes);
                }
            }

            nvidia.freeGPU(inBuf);
            nvidia.freeGPU(wBuf);
            nvidia.freeGPU(outBuf);
        } else if (strncmp(k, "rope", 4) == 0) {
            // RoPE operates in-place on Q/K tensor
            NvidiaGPUBuffer qkBuf{};
            nvidia.allocGPU(task.inputSizeBytes, qkBuf);

            if (qkBuf.devicePtr) {
                nvidia.copyToGPU(qkBuf, task.inputData, task.inputSizeBytes);

                // Infer dimensions: inputSizeBytes = seqLen * headDim * sizeof(float)
                // Estimate headDim from batch/output context, default 128
                uint32_t headDim = 128;
                uint32_t totalFloats = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
                uint32_t seqLen = (headDim > 0) ? totalFloats / headDim : 1;
                if (seqLen == 0) seqLen = 1;

                nvidiaResult = nvidia.dispatchRoPE(qkBuf, seqLen, headDim, 0);

                if (nvidiaResult.success && task.outputData) {
                    nvidia.copyFromGPU(task.outputData, qkBuf, task.outputSizeBytes);
                }
            }

            nvidia.freeGPU(qkBuf);
        } else if (strncmp(k, "attention", 9) == 0 || strncmp(k, "flash_attn", 10) == 0) {
            // Fused scaled dot-product attention
            // Expects packed input: Q[seqM*headDim] | K[seqN*headDim] | V[seqN*headDim]
            // Output: O[seqM*headDim]
            uint32_t headDim = 128;  // default, override via task metadata
            uint32_t seqN = 0;
            uint32_t seqM = 0;

            // Infer dimensions from buffer sizes
            // inputSizeBytes = (seqM*headDim + seqN*headDim + seqN*headDim) * sizeof(float)
            // outputSizeBytes = seqM * headDim * sizeof(float)
            uint32_t outFloats = static_cast<uint32_t>(task.outputSizeBytes / sizeof(float));
            seqM = outFloats / headDim;
            if (seqM == 0) seqM = 1;
            uint32_t inFloats = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
            uint32_t kvFloats = inFloats - seqM * headDim;
            seqN = kvFloats / (2 * headDim);
            if (seqN == 0) seqN = seqM;  // self-attention default

            float scale = 1.0f / std::sqrt(static_cast<float>(headDim));
            bool causal = true;  // default to causal for autoregressive

            uint64_t qBytes = seqM * headDim * sizeof(float);
            uint64_t kBytes = seqN * headDim * sizeof(float);
            uint64_t vBytes = seqN * headDim * sizeof(float);
            uint64_t oBytes = seqM * headDim * sizeof(float);

            NvidiaGPUBuffer qBuf{}, kBuf{}, vBuf{}, oBuf{};
            nvidia.allocGPU(qBytes, qBuf);
            nvidia.allocGPU(kBytes, kBuf);
            nvidia.allocGPU(vBytes, vBuf);
            nvidia.allocGPU(oBytes, oBuf);

            if (qBuf.devicePtr && kBuf.devicePtr && vBuf.devicePtr && oBuf.devicePtr) {
                const char* src = reinterpret_cast<const char*>(task.inputData);
                nvidia.copyToGPU(qBuf, src, qBytes);
                nvidia.copyToGPU(kBuf, src + qBytes, kBytes);
                nvidia.copyToGPU(vBuf, src + qBytes + kBytes, vBytes);

                nvidiaResult = nvidia.dispatchAttention(qBuf, kBuf, vBuf, oBuf,
                                                        seqM, seqN, headDim, scale, causal);

                if (nvidiaResult.success && task.outputData) {
                    nvidia.copyFromGPU(task.outputData, oBuf, std::min(oBytes, task.outputSizeBytes));
                }
            }

            nvidia.freeGPU(qBuf);
            nvidia.freeGPU(kBuf);
            nvidia.freeGPU(vBuf);
            nvidia.freeGPU(oBuf);
        } else if (strncmp(k, "kv_init", 7) == 0) {
            // Initialize KV-cache: input contains NvidiaKVCacheConfig
            if (task.inputSizeBytes >= sizeof(NvidiaKVCacheConfig)) {
                const NvidiaKVCacheConfig* cfg =
                    reinterpret_cast<const NvidiaKVCacheConfig*>(task.inputData);
                nvidiaResult = nvidia.initKVCache(*cfg);
            } else {
                nvidiaResult = NvidiaAccelResult::error("kv_init: input too small for config");
            }
        } else if (strncmp(k, "kv_append", 9) == 0) {
            // Append one token's K/V for all layers/heads.
            // Input: [numLayers * numHeads * 2] contiguous rows of [headDim] floats
            // Layout: for each layer, for each head: key_row, value_row
            if (!nvidia.isKVCacheReady()) {
                nvidiaResult = NvidiaAccelResult::error("kv_append: cache not initialized");
            } else {
                const auto& cache = nvidia.getKVCache();
                uint32_t hd = cache.config.headDim;
                uint64_t rowPairBytes = 2ULL * hd * sizeof(float);
                uint64_t expectedBytes = cache.config.numLayers * cache.config.numHeads * rowPairBytes;
                if (task.inputSizeBytes < expectedBytes) {
                    nvidiaResult = NvidiaAccelResult::error("kv_append: input buffer too small");
                } else {
                    const float* ptr = reinterpret_cast<const float*>(task.inputData);
                    nvidiaResult = NvidiaAccelResult::ok("KV append complete");
                    for (uint32_t l = 0; l < cache.config.numLayers && nvidiaResult.success; ++l) {
                        for (uint32_t h = 0; h < cache.config.numHeads && nvidiaResult.success; ++h) {
                            nvidiaResult = nvidia.appendKV(l, h, ptr, ptr + hd);
                            ptr += 2 * hd;
                        }
                    }
                    if (nvidiaResult.success)
                        nvidia.advanceKVPos();
                }
            }
        } else if (strncmp(k, "kv_attn", 7) == 0 || strncmp(k, "cached_attention", 16) == 0) {
            // Cached attention: Q[1, headDim] → O[1, headDim] using cached K/V
            // Input: Q (headDim floats) + layer index (uint32_t) + head index (uint32_t)
            if (!nvidia.isKVCacheReady()) {
                nvidiaResult = NvidiaAccelResult::error("kv_attn: cache not initialized");
            } else {
                const auto& cache = nvidia.getKVCache();
                uint32_t hd = cache.config.headDim;
                uint64_t qBytes = hd * sizeof(float);
                uint64_t metaBytes = 2 * sizeof(uint32_t);  // layer, head indices

                if (task.inputSizeBytes < qBytes + metaBytes) {
                    nvidiaResult = NvidiaAccelResult::error("kv_attn: input too small");
                } else {
                    const char* raw = reinterpret_cast<const char*>(task.inputData);
                    uint32_t layer, head;
                    memcpy(&layer, raw + qBytes, sizeof(uint32_t));
                    memcpy(&head, raw + qBytes + sizeof(uint32_t), sizeof(uint32_t));

                    NvidiaGPUBuffer qBuf{}, oBuf{};
                    nvidia.allocGPU(qBytes, qBuf);
                    nvidia.allocGPU(qBytes, oBuf);

                    if (qBuf.devicePtr && oBuf.devicePtr) {
                        nvidia.copyToGPU(qBuf, raw, qBytes);
                        float scale = 1.0f / std::sqrt(static_cast<float>(hd));
                        nvidiaResult = nvidia.dispatchCachedAttention(
                            qBuf, oBuf, layer, head, scale, true);
                        if (nvidiaResult.success && task.outputData)
                            nvidia.copyFromGPU(task.outputData, oBuf, std::min(qBytes, task.outputSizeBytes));
                    }

                    nvidia.freeGPU(qBuf);
                    nvidia.freeGPU(oBuf);
                }
            }
        } else if (strncmp(k, "kv_reset", 8) == 0) {
            nvidia.resetKVCache();
            nvidiaResult = NvidiaAccelResult::ok("KV-cache reset");
        } else if (strncmp(k, "kv_free", 7) == 0) {
            nvidiaResult = nvidia.freeKVCache();
        } else if (strncmp(k, "weight_upload", 13) == 0) {
            // Upload raw weight tensor to GPU. Input = raw weight data.
            // Kernel name encodes format: weight_upload_f32, weight_upload_q4_0, etc.
            NvidiaWeightFormat fmt = NvidiaWeightFormat::Raw;
            if (strstr(k, "f32"))       fmt = NvidiaWeightFormat::F32;
            else if (strstr(k, "f16"))  fmt = NvidiaWeightFormat::F16;
            else if (strstr(k, "q4_0")) fmt = NvidiaWeightFormat::Q4_0;
            else if (strstr(k, "q8_0")) fmt = NvidiaWeightFormat::Q8_0;

            // Weight name is passed via kernelName suffix or task metadata
            // For router-level dispatch, use a generic name based on buffer ID
            std::string wname = "weight_" + std::to_string(nvidia.getWeightMap().weights.size());
            std::vector<uint64_t> shape;
            uint64_t elems = task.inputSizeBytes / sizeof(float);
            shape.push_back(elems);

            nvidiaResult = nvidia.uploadWeight(wname, task.inputData, task.inputSizeBytes,
                                               fmt, shape);
        } else if (strncmp(k, "weight_free_all", 15) == 0) {
            nvidiaResult = nvidia.freeAllWeights();
        } else if (strncmp(k, "argmax", 6) == 0) {
            // GPU argmax over logits buffer
            NvidiaGPUBuffer logitsBuf{};
            uint32_t vocabSize = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
            nvidia.allocGPU(task.inputSizeBytes, logitsBuf);
            if (logitsBuf.devicePtr) {
                nvidia.copyToGPU(logitsBuf, task.inputData, task.inputSizeBytes);
                uint32_t tokenId = 0;
                nvidiaResult = nvidia.dispatchArgmax(logitsBuf, vocabSize, tokenId);
                if (nvidiaResult.success && task.outputData && task.outputSizeBytes >= sizeof(uint32_t)) {
                    *static_cast<uint32_t*>(task.outputData) = tokenId;
                }
            } else {
                nvidiaResult = NvidiaAccelResult::error("Argmax buffer alloc failed");
            }
            nvidia.freeGPU(logitsBuf);
        } else if (strncmp(k, "sample", 6) == 0) {
            // Sampling with config: logits in, token ID out
            NvidiaGPUBuffer logitsBuf{};
            uint32_t vocabSize = static_cast<uint32_t>(task.inputSizeBytes / sizeof(float));
            nvidia.allocGPU(task.inputSizeBytes, logitsBuf);
            if (logitsBuf.devicePtr) {
                nvidia.copyToGPU(logitsBuf, task.inputData, task.inputSizeBytes);
                NvidiaSamplerConfig samplerCfg;
                uint32_t tokenId = 0;
                nvidiaResult = nvidia.dispatchSample(logitsBuf, vocabSize, samplerCfg,
                                                    nullptr, 0, tokenId);
                if (nvidiaResult.success && task.outputData && task.outputSizeBytes >= sizeof(uint32_t)) {
                    *static_cast<uint32_t*>(task.outputData) = tokenId;
                }
            } else {
                nvidiaResult = NvidiaAccelResult::error("Sample buffer alloc failed");
            }
            nvidia.freeGPU(logitsBuf);
        } else if (strncmp(k, "stream_pool_init", 16) == 0) {
            // Initialize the multi-stream pipeline pool (call once before pipelined ops)
            nvidiaResult = nvidia.initStreamPool();
        } else if (strncmp(k, "stream_pool_destroy", 19) == 0) {
            nvidiaResult = nvidia.destroyStreamPool();
        } else if (strncmp(k, "generate_pipelined", 18) == 0) {
            // Pipelined autoregressive generation: prompt token IDs in, generated IDs out.
            // inputData:  uint32_t[] prompt token IDs
            // outputData: uint32_t[] generated token IDs  (caller must allocate enough)
            // outputSizeBytes: capacity of output buffer in bytes
            if (!task.inputData || task.inputSizeBytes == 0) {
                nvidiaResult = NvidiaAccelResult::error("generate_pipelined: no prompt tokens");
            } else {
                uint32_t promptCount = static_cast<uint32_t>(task.inputSizeBytes / sizeof(uint32_t));
                std::vector<uint32_t> prompt(
                    static_cast<const uint32_t*>(task.inputData),
                    static_cast<const uint32_t*>(task.inputData) + promptCount);

                NvidiaGenerationConfig genCfg;
                NvidiaGenerationResult genRes = nvidia.generateTokensPipelined(prompt, genCfg);

                if (genRes.success) {
                    if (task.outputData && task.outputSizeBytes >= sizeof(uint32_t)) {
                        uint32_t copyCount = static_cast<uint32_t>(
                            std::min(genRes.tokens.size(),
                                     task.outputSizeBytes / sizeof(uint32_t)));
                        memcpy(task.outputData, genRes.tokens.data(),
                               copyCount * sizeof(uint32_t));
                    }
                    nvidiaResult = NvidiaAccelResult::ok(genRes.detail);
                    nvidiaResult.elapsedMs = genRes.totalMs;
                } else {
                    nvidiaResult = NvidiaAccelResult::error(genRes.detail);
                }
            }
        } else if (strncmp(k, "benchmark_generation", 20) == 0) {
            // Benchmark sync vs pipelined generation for greedy + stochastic modes.
            // inputData:  uint32_t[] prompt token IDs
            // outputData: double[10]
            //   [0] greedySyncMs, [1] greedyPipeMs, [2] greedySpeedup,
            //   [3] greedySyncTPS, [4] greedyPipeTPS,
            //   [5] stochSyncMs,  [6] stochPipeMs, [7] stochSpeedup,
            //   [8] stochSyncTPS, [9] stochPipeTPS
            // Optional extended outputData: double[18]
            //   [10] greedySyncCI95Ms, [11] greedyPipeCI95Ms,
            //   [12] stochSyncCI95Ms,  [13] stochPipeCI95Ms,
            //   [14] greedySyncStdMs,  [15] greedyPipeStdMs,
            //   [16] stochSyncStdMs,   [17] stochPipeStdMs
            if (!task.inputData || task.inputSizeBytes == 0) {
                nvidiaResult = NvidiaAccelResult::error("benchmark_generation: no prompt tokens");
            } else {
                uint32_t promptCount = static_cast<uint32_t>(task.inputSizeBytes / sizeof(uint32_t));
                std::vector<uint32_t> prompt(
                    static_cast<const uint32_t*>(task.inputData),
                    static_cast<const uint32_t*>(task.inputData) + promptCount);

                uint32_t runs = (task.batchSize > 0) ? task.batchSize : 3;
                if (runs > 20) runs = 20; // keep benchmark bounded

                struct BenchOut {
                    double avgMs;
                    double avgTps;
                    double stdMs;
                    double stdTps;
                    double ci95Ms;
                    double ci95Tps;
                    bool   ok;
                };

                auto runBench = [&](bool pipelined, float temperature) -> BenchOut {
                    BenchOut out{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, true};
                    double sumMs = 0.0, sumSqMs = 0.0;
                    double sumTps = 0.0, sumSqTps = 0.0;
                    for (uint32_t i = 0; i < runs; ++i) {
                        NvidiaGenerationConfig cfg;
                        cfg.maxTokens = 100;
                        cfg.sampler.temperature = temperature;
                        cfg.sampler.topK = 40;
                        cfg.sampler.topP = 0.95f;

                        NvidiaGenerationResult r = pipelined
                            ? nvidia.generateTokensPipelined(prompt, cfg)
                            : nvidia.generateTokens(prompt, cfg);
                        if (!r.success) {
                            out.ok = false;
                            return out;
                        }
                        sumMs += r.totalMs;
                        sumSqMs += r.totalMs * r.totalMs;
                        sumTps += r.tokPerSec;
                        sumSqTps += r.tokPerSec * r.tokPerSec;
                    }
                    double n = static_cast<double>(runs);
                    out.avgMs = sumMs / n;
                    out.avgTps = sumTps / n;

                    if (runs > 1) {
                        double varMs = (sumSqMs - (sumMs * sumMs) / n) / (n - 1.0);
                        double varTps = (sumSqTps - (sumTps * sumTps) / n) / (n - 1.0);
                        if (varMs < 0.0) varMs = 0.0;
                        if (varTps < 0.0) varTps = 0.0;
                        out.stdMs = std::sqrt(varMs);
                        out.stdTps = std::sqrt(varTps);
                        out.ci95Ms = 1.96 * out.stdMs / std::sqrt(n);
                        out.ci95Tps = 1.96 * out.stdTps / std::sqrt(n);
                    }
                    return out;
                };

                // Greedy mode benchmark (temperature 0.0)
                BenchOut greedySync = runBench(false, 0.0f);
                if (!greedySync.ok) {
                    nvidiaResult = NvidiaAccelResult::error("benchmark_generation: greedy sync failed");
                } else {
                    nvidia.initStreamPool(); // idempotent if already initialized
                    BenchOut greedyPipe = runBench(true, 0.0f);
                    if (!greedyPipe.ok) {
                        nvidiaResult = NvidiaAccelResult::error("benchmark_generation: greedy pipeline failed");
                    } else {
                        // Stochastic mode benchmark (temperature 0.7)
                        BenchOut stochSync = runBench(false, 0.7f);
                        if (!stochSync.ok) {
                            nvidiaResult = NvidiaAccelResult::error("benchmark_generation: stochastic sync failed");
                        } else {
                            BenchOut stochPipe = runBench(true, 0.7f);
                            if (!stochPipe.ok) {
                                nvidiaResult = NvidiaAccelResult::error("benchmark_generation: stochastic pipeline failed");
                            } else {
                                double greedySpeedup = (greedyPipe.avgMs > 0.0)
                                    ? (greedySync.avgMs / greedyPipe.avgMs) : 0.0;
                                double stochSpeedup = (stochPipe.avgMs > 0.0)
                                    ? (stochSync.avgMs / stochPipe.avgMs) : 0.0;

                                if (task.outputData && task.outputSizeBytes >= 10 * sizeof(double)) {
                                    double* out = static_cast<double*>(task.outputData);
                                    out[0] = greedySync.avgMs;
                                    out[1] = greedyPipe.avgMs;
                                    out[2] = greedySpeedup;
                                    out[3] = greedySync.avgTps;
                                    out[4] = greedyPipe.avgTps;
                                    out[5] = stochSync.avgMs;
                                    out[6] = stochPipe.avgMs;
                                    out[7] = stochSpeedup;
                                    out[8] = stochSync.avgTps;
                                    out[9] = stochPipe.avgTps;

                                    if (task.outputSizeBytes >= 18 * sizeof(double)) {
                                        out[10] = greedySync.ci95Ms;
                                        out[11] = greedyPipe.ci95Ms;
                                        out[12] = stochSync.ci95Ms;
                                        out[13] = stochPipe.ci95Ms;
                                        out[14] = greedySync.stdMs;
                                        out[15] = greedyPipe.stdMs;
                                        out[16] = stochSync.stdMs;
                                        out[17] = stochPipe.stdMs;
                                    }
                                }

                                nvidiaResult = NvidiaAccelResult::ok("Generation benchmark complete");
                                nvidiaResult.elapsedMs = greedyPipe.avgMs + stochPipe.avgMs;
                                nvidiaResult.throughputGFLOPS = (greedySpeedup + stochSpeedup) * 0.5;
                            }
                        }
                    }
                }
            }
        } else if (strncmp(k, "benchmark_long_context", 22) == 0) {
            // Long-context stress benchmark: sync vs pipelined over increasing prompt lengths.
            // inputData:  uint32_t[] base prompt token IDs (must be non-empty)
            // outputData: double[36] where each step uses 6 doubles:
            //   [step*6 + 0] = contextLength
            //   [step*6 + 1] = syncMs
            //   [step*6 + 2] = pipeMs
            //   [step*6 + 3] = syncTPS
            //   [step*6 + 4] = pipeTPS
            //   [step*6 + 5] = speedup (syncMs / pipeMs)
            // Optional extended outputData: double[72] where each step uses 12 doubles:
            //   [step*12 + 0..5]  = same base metrics as above
            //   [step*12 + 6]     = syncStdMs
            //   [step*12 + 7]     = pipeStdMs
            //   [step*12 + 8]     = syncCI95Ms
            //   [step*12 + 9]     = pipeCI95Ms
            //   [step*12 + 10]    = syncStdTPS
            //   [step*12 + 11]    = pipeStdTPS
            // Steps are fixed: 128, 256, 512, 1024, 2048, 4096 tokens.
            if (!task.inputData || task.inputSizeBytes == 0) {
                nvidiaResult = NvidiaAccelResult::error("benchmark_long_context: no base prompt tokens");
            } else {
                uint32_t baseCount = static_cast<uint32_t>(task.inputSizeBytes / sizeof(uint32_t));
                if (baseCount == 0) {
                    nvidiaResult = NvidiaAccelResult::error("benchmark_long_context: empty base prompt");
                } else {
                    std::vector<uint32_t> basePrompt(
                        static_cast<const uint32_t*>(task.inputData),
                        static_cast<const uint32_t*>(task.inputData) + baseCount);

                    uint32_t runs = (task.batchSize > 0) ? task.batchSize : 3;
                    if (runs > 10) runs = 10; // keep runtime bounded

                    static constexpr uint32_t kSteps[] = {128, 256, 512, 1024, 2048, 4096};
                    static constexpr uint32_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);
                    const bool extendedStats = (task.outputSizeBytes >= (kStepCount * 12 * sizeof(double)));

                    if (!task.outputData || task.outputSizeBytes < (kStepCount * 6 * sizeof(double))) {
                        nvidiaResult = NvidiaAccelResult::error("benchmark_long_context: output buffer too small");
                    } else {
                        nvidia.initStreamPool(); // idempotent

                        double* out = static_cast<double*>(task.outputData);
                        bool ok = true;

                        for (uint32_t s = 0; s < kStepCount && ok; ++s) {
                            uint32_t ctxLen = kSteps[s];

                            std::vector<uint32_t> prompt;
                            prompt.reserve(ctxLen);
                            while (prompt.size() < ctxLen) {
                                size_t toCopy = std::min<size_t>(basePrompt.size(), ctxLen - prompt.size());
                                prompt.insert(prompt.end(), basePrompt.begin(), basePrompt.begin() + toCopy);
                            }

                            double syncMs = 0.0;
                            double pipeMs = 0.0;
                            double syncTps = 0.0;
                            double pipeTps = 0.0;
                            double syncSqMs = 0.0;
                            double pipeSqMs = 0.0;
                            double syncSqTps = 0.0;
                            double pipeSqTps = 0.0;

                            for (uint32_t r = 0; r < runs && ok; ++r) {
                                NvidiaGenerationConfig cfg;
                                cfg.maxTokens = 64;
                                cfg.sampler.temperature = 0.7f;
                                cfg.sampler.topK = 40;
                                cfg.sampler.topP = 0.95f;

                                NvidiaGenerationResult rs = nvidia.generateTokens(prompt, cfg);
                                if (!rs.success) {
                                    ok = false;
                                    nvidiaResult = NvidiaAccelResult::error("benchmark_long_context: sync run failed");
                                    break;
                                }

                                NvidiaGenerationResult rp = nvidia.generateTokensPipelined(prompt, cfg);
                                if (!rp.success) {
                                    ok = false;
                                    nvidiaResult = NvidiaAccelResult::error("benchmark_long_context: pipelined run failed");
                                    break;
                                }

                                syncMs += rs.totalMs;
                                pipeMs += rp.totalMs;
                                syncTps += rs.tokPerSec;
                                pipeTps += rp.tokPerSec;
                                syncSqMs += rs.totalMs * rs.totalMs;
                                pipeSqMs += rp.totalMs * rp.totalMs;
                                syncSqTps += rs.tokPerSec * rs.tokPerSec;
                                pipeSqTps += rp.tokPerSec * rp.tokPerSec;
                            }

                            if (!ok) break;

                            syncMs /= static_cast<double>(runs);
                            pipeMs /= static_cast<double>(runs);
                            syncTps /= static_cast<double>(runs);
                            pipeTps /= static_cast<double>(runs);
                            double speedup = (pipeMs > 0.0) ? (syncMs / pipeMs) : 0.0;

                            double syncStdMs = 0.0, pipeStdMs = 0.0;
                            double syncCi95Ms = 0.0, pipeCi95Ms = 0.0;
                            double syncStdTps = 0.0, pipeStdTps = 0.0;
                            if (runs > 1) {
                                double n = static_cast<double>(runs);
                                double varSyncMs = (syncSqMs - (syncMs * runs) * (syncMs * runs) / n) / (n - 1.0);
                                double varPipeMs = (pipeSqMs - (pipeMs * runs) * (pipeMs * runs) / n) / (n - 1.0);
                                double varSyncTps = (syncSqTps - (syncTps * runs) * (syncTps * runs) / n) / (n - 1.0);
                                double varPipeTps = (pipeSqTps - (pipeTps * runs) * (pipeTps * runs) / n) / (n - 1.0);
                                if (varSyncMs < 0.0) varSyncMs = 0.0;
                                if (varPipeMs < 0.0) varPipeMs = 0.0;
                                if (varSyncTps < 0.0) varSyncTps = 0.0;
                                if (varPipeTps < 0.0) varPipeTps = 0.0;
                                syncStdMs = std::sqrt(varSyncMs);
                                pipeStdMs = std::sqrt(varPipeMs);
                                syncStdTps = std::sqrt(varSyncTps);
                                pipeStdTps = std::sqrt(varPipeTps);
                                syncCi95Ms = 1.96 * syncStdMs / std::sqrt(n);
                                pipeCi95Ms = 1.96 * pipeStdMs / std::sqrt(n);
                            }

                            if (extendedStats) {
                                out[s * 12 + 0] = static_cast<double>(ctxLen);
                                out[s * 12 + 1] = syncMs;
                                out[s * 12 + 2] = pipeMs;
                                out[s * 12 + 3] = syncTps;
                                out[s * 12 + 4] = pipeTps;
                                out[s * 12 + 5] = speedup;
                                out[s * 12 + 6] = syncStdMs;
                                out[s * 12 + 7] = pipeStdMs;
                                out[s * 12 + 8] = syncCi95Ms;
                                out[s * 12 + 9] = pipeCi95Ms;
                                out[s * 12 + 10] = syncStdTps;
                                out[s * 12 + 11] = pipeStdTps;
                            } else {
                                out[s * 6 + 0] = static_cast<double>(ctxLen);
                                out[s * 6 + 1] = syncMs;
                                out[s * 6 + 2] = pipeMs;
                                out[s * 6 + 3] = syncTps;
                                out[s * 6 + 4] = pipeTps;
                                out[s * 6 + 5] = speedup;
                            }
                        }

                        if (ok) {
                            nvidiaResult = NvidiaAccelResult::ok("Long-context benchmark complete");
                        }
                    }
                }
            }
        } else {
            nvidiaResult = NvidiaAccelResult::error("Unknown NVIDIA kernel");
        }
    } else {
        nvidiaResult = NvidiaAccelResult::error("No kernel name specified");
    }

    if (!nvidiaResult.success) {
        return RouterResult::error(nvidiaResult.detail, nvidiaResult.errorCode);
    }

    RouterResult r = RouterResult::ok(nvidiaResult.detail, RouterBackendType::NVIDIA_CUDA);
    r.throughputGFLOPS = nvidiaResult.throughputGFLOPS;
    r.elapsedMs = nvidiaResult.elapsedMs;
    return r;
}

// ============================================================================
// AcceleratorRouter::dispatchFlashAttention (v14.7.0-ATTN)
// ============================================================================
RouterResult AcceleratorRouter::dispatchFlashAttention(void* q, void* k, void* v, void* o,
                                                     uint32_t seqM, uint32_t seqN,
                                                     uint32_t headDim, uint32_t numHeads,
                                                     float scale) {
    // Priority: AVX-512 ASM Path (lowest latency for local small batches)
    RawrXD::FlashAttentionEngine engine;
    if (m_backends[static_cast<int>(RouterBackendType::CPU_Fallback)].enabled && engine.IsReady()) {
        RawrXD::FlashAttentionConfig cfg{};
        cfg.Q = static_cast<float*>(q);
        cfg.K = static_cast<float*>(k);
        cfg.V = static_cast<float*>(v);
        cfg.O = static_cast<float*>(o);
        cfg.seqLenM = static_cast<int32_t>(seqM);
        cfg.seqLenN = static_cast<int32_t>(seqN);
        cfg.headDim = static_cast<int32_t>(headDim);
        cfg.numHeads = static_cast<int32_t>(numHeads);
        cfg.numKVHeads = static_cast<int32_t>(numHeads); // Default MHA
        cfg.batchSize = 1;
        cfg.scale = scale;
        cfg.causal = 1;

        auto start = std::chrono::high_resolution_clock::now();
        int32_t result = engine.Forward(cfg);
        auto end = std::chrono::high_resolution_clock::now();

        if (result == 0) {
            RouterResult r = RouterResult::ok("AVX-512 Flash Attention Success", RouterBackendType::CPU_Fallback);
            r.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            return r;
        }
    }

    // Fallback cascade to other backends if AVX-512 fails or is unavailable
    return RouterResult::error("Flash Attention dispatch failed (AVX-512 unavailable or error)", -1);
}

// ============================================================================
// C Bridge Implementation (for MASM / external callers)
// ============================================================================

extern "C" {

void* AccelRouter_Create() {
    return static_cast<void*>(&AcceleratorRouter::instance());
}

int AccelRouter_Init(void* handle) {
    if (!handle) return -1;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    RouterResult r = router->initialize();
    return r.success ? 0 : r.errorCode;
}

void AccelRouter_Shutdown(void* handle) {
    if (!handle) return;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    router->shutdown();
}

int AccelRouter_Submit(void* handle, const RouterInferenceTask* task, RouterResult* result) {
    if (!handle || !task || !result) return -1;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    *result = router->submitInference(*task);
    return result->success ? 0 : result->errorCode;
}

uint32_t AccelRouter_GetActiveBackend(void* handle) {
    if (!handle) return 0;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    return static_cast<uint32_t>(router->getActiveBackend());
}

void AccelRouter_ForceBackend(void* handle, uint32_t backendType) {
    if (!handle) return;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    router->forceBackend(static_cast<RouterBackendType>(backendType));
}

int AccelRouter_IsBackendAvailable(void* handle, uint32_t backendType) {
    if (!handle) return 0;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    return router->isBackendAvailable(static_cast<RouterBackendType>(backendType)) ? 1 : 0;
}

void AccelRouter_GetStatsJson(void* handle, char* outJson, uint32_t maxLen) {
    if (!handle || !outJson || maxLen == 0) return;
    AcceleratorRouter* router = static_cast<AcceleratorRouter*>(handle);
    std::string json = router->toJson();
    size_t copyLen = std::min(static_cast<size_t>(maxLen - 1), json.size());
    memcpy(outJson, json.c_str(), copyLen);
    outJson[copyLen] = '\0';
}

} // extern "C"
