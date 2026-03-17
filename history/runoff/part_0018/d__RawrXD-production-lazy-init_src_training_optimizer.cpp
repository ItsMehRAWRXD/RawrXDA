#include "training_optimizer.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <psapi.h>
#include <immintrin.h>

#pragma comment(lib, "psapi.lib")

namespace TrainingOptimizer {

// ==================== Hardware Detection Implementation ====================

HardwareProfile HardwareDetector::detectHardware() {
    HardwareProfile profile;
    
    // Detect CPU cores
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    profile.cpuCores = sysInfo.dwNumberOfProcessors;
    profile.cpuThreads = std::thread::hardware_concurrency();
    profile.cpuCacheLineSize = sysInfo.dwPageSize;
    
    // Detect CPU capability
    profile.cpuCapability = detectCPUCapability();
    
    // Detect RAM
    profile.totalRamBytes = detectTotalRAM();
    
    // Get available RAM
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        profile.availableRamBytes = memStatus.ullAvailPhys;
    }
    
    // Detect GPU
    detectGPU(profile);
    
    // Estimate performance
    estimatePerformance(profile);
    
    return profile;
}

ComputeCapability HardwareDetector::detectCPUCapability() {
    int cpuInfo[4] = {0};
    
    // Check AVX-512
    __cpuidex(cpuInfo, 7, 0);
    if ((cpuInfo[1] & (1 << 16)) != 0) {  // AVX-512F
        std::cout << "[HardwareDetector] CPU supports AVX-512" << std::endl;
        return ComputeCapability::CPU_AVX512;
    }
    
    // Check AVX2
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] & (1 << 28)) != 0) {  // AVX
        __cpuid(cpuInfo, 7);
        if ((cpuInfo[1] & (1 << 5)) != 0) {  // AVX2
            std::cout << "[HardwareDetector] CPU supports AVX2" << std::endl;
            return ComputeCapability::CPU_AVX2;
        }
        std::cout << "[HardwareDetector] CPU supports AVX" << std::endl;
        return ComputeCapability::CPU_AVX;
    }
    
    // Check SSE4.2
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] & (1 << 20)) != 0) {
        std::cout << "[HardwareDetector] CPU supports SSE4.2" << std::endl;
        return ComputeCapability::CPU_SSE4_2;
    }
    
    std::cout << "[HardwareDetector] CPU: baseline (no SIMD)" << std::endl;
    return ComputeCapability::CPU_ONLY;
}

void HardwareDetector::detectGPU(HardwareProfile& profile) {
    // Try NVIDIA CUDA
    HMODULE cudaModule = LoadLibraryA("nvcuda.dll");
    if (cudaModule) {
        profile.hasGPU = true;
        profile.gpuType = "NVIDIA";
        profile.gpuCount = 1;  // TODO: Query actual count via CUDA API
        profile.gpuMemoryBytes = 8LL * 1024 * 1024 * 1024;  // 8GB estimate
        profile.gpuPeakTFlops = 10000;  // FP32 estimate for modern GPU
        profile.supportsMixedPrecision = true;
        std::cout << "[HardwareDetector] Detected NVIDIA GPU" << std::endl;
        FreeLibrary(cudaModule);
        return;
    }
    
    // Try AMD HIP
    HMODULE hipModule = LoadLibraryA("amdhip64.dll");
    if (hipModule) {
        profile.hasGPU = true;
        profile.gpuType = "AMD";
        profile.gpuCount = 1;
        profile.gpuMemoryBytes = 8LL * 1024 * 1024 * 1024;
        profile.gpuPeakTFlops = 10000;
        profile.supportsMixedPrecision = true;
        std::cout << "[HardwareDetector] Detected AMD GPU" << std::endl;
        FreeLibrary(hipModule);
        return;
    }
    
    std::cout << "[HardwareDetector] No GPU detected, using CPU only" << std::endl;
}

uint64_t HardwareDetector::detectTotalRAM() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullTotalPhys;
    }
    
    return 16ULL * 1024 * 1024 * 1024;  // 16GB default
}

void HardwareDetector::estimatePerformance(HardwareProfile& profile) {
    // Estimate CPU performance (FLOPs)
    // Base: 1 GHz per core, SIMD width varies
    float cpuGhz = 2.5f;  // Typical boost clock
    float simdWidth = 1.0f;
    
    switch (profile.cpuCapability) {
        case ComputeCapability::CPU_AVX512:
            simdWidth = 16.0f;  // 512-bit / 32-bit
            break;
        case ComputeCapability::CPU_AVX2:
            simdWidth = 8.0f;   // 256-bit / 32-bit
            break;
        case ComputeCapability::CPU_AVX:
            simdWidth = 4.0f;   // 128-bit / 32-bit
            break;
        default:
            simdWidth = 1.0f;
    }
    
    // FMA: 2 operations per instruction
    profile.cpuGFlops = profile.cpuCores * cpuGhz * simdWidth * 2.0f;
    
    profile.totalGFlops = profile.cpuGFlops;
    if (profile.hasGPU) {
        profile.totalGFlops += profile.gpuPeakTFlops;
    }
}

bool HardwareDetector::validateHardware(const HardwareProfile& profile) {
    if (profile.cpuCores == 0) {
        std::cerr << "[HardwareDetector] ERROR: No CPU cores detected" << std::endl;
        return false;
    }
    
    if (profile.totalRamBytes < 4ULL * 1024 * 1024 * 1024) {
        std::cerr << "[HardwareDetector] WARNING: Less than 4GB RAM detected" << std::endl;
    }
    
    return true;
}

std::string HardwareDetector::getHardwareDescription(const HardwareProfile& profile) {
    std::stringstream ss;
    
    ss << "\n========== HARDWARE PROFILE ==========\n";
    ss << "CPU Cores: " << profile.cpuCores << " cores, " << profile.cpuThreads << " threads\n";
    
    switch (profile.cpuCapability) {
        case ComputeCapability::CPU_AVX512:
            ss << "CPU Capability: AVX-512\n";
            break;
        case ComputeCapability::CPU_AVX2:
            ss << "CPU Capability: AVX2\n";
            break;
        case ComputeCapability::CPU_AVX:
            ss << "CPU Capability: AVX\n";
            break;
        case ComputeCapability::CPU_SSE4_2:
            ss << "CPU Capability: SSE4.2\n";
            break;
        default:
            ss << "CPU Capability: None (baseline)\n";
    }
    
    ss << "CPU Peak Performance: " << std::fixed << std::setprecision(2) << profile.cpuGFlops << " GFLOPS\n";
    
    ss << "RAM: " << (profile.totalRamBytes / 1024 / 1024 / 1024) << " GB total, "
       << (profile.availableRamBytes / 1024 / 1024 / 1024) << " GB available\n";
    
    if (profile.hasGPU) {
        ss << "GPU: " << profile.gpuType << " (1x"
           << (profile.gpuMemoryBytes / 1024 / 1024 / 1024) << "GB)\n";
        ss << "GPU Peak Performance: " << profile.gpuPeakTFlops << " GFLOPS\n";
        ss << "Mixed Precision: " << (profile.supportsMixedPrecision ? "Yes" : "No") << "\n";
    } else {
        ss << "GPU: None (CPU-only)\n";
    }
    
    ss << "Total Peak Performance: " << std::fixed << std::setprecision(2) << profile.totalGFlops << " GFLOPS\n";
    ss << "======================================\n";
    
    return ss.str();
}

// ==================== Training Profiler Implementation ====================

TrainingProfiler::TrainingProfiler() {}

void TrainingProfiler::startOperation(const std::string& opName) {
    startTimes_[opName] = std::chrono::high_resolution_clock::now();
}

void TrainingProfiler::endOperation(const std::string& opName) {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto startIt = startTimes_.find(opName);
    
    if (startIt == startTimes_.end()) {
        std::cerr << "[TrainingProfiler] Operation '" << opName << "' never started!" << std::endl;
        return;
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startIt->second
    );
    double timeMs = duration.count() / 1000.0;
    
    auto& timing = timings_[opName];
    timing.operationName = opName;
    timing.totalTimeMs += timeMs;
    timing.callCount++;
    timing.avgTimeMs = timing.totalTimeMs / timing.callCount;
    timing.maxTimeMs = std::max(timing.maxTimeMs, timeMs);
    timing.minTimeMs = timing.minTimeMs > 0 ? std::min(timing.minTimeMs, timeMs) : timeMs;
    
    startTimes_.erase(startIt);
}

void TrainingProfiler::recordEpochComplete(double totalTimeMs) {
    epochTimes_.push_back(totalTimeMs);
}

TrainingProfile TrainingProfiler::getProfile() const {
    TrainingProfile profile;
    
    double totalMs = 0;
    std::vector<OpTiming> timings;
    
    for (const auto& [opName, timing] : timings_) {
        totalMs += timing.totalTimeMs;
        timings.push_back(timing);
    }
    
    // Calculate percentages
    for (auto& timing : timings) {
        timing.percentOfTotal = (timing.totalTimeMs / totalMs) * 100.0;
    }
    
    std::sort(timings.begin(), timings.end(),
        [](const OpTiming& a, const OpTiming& b) {
            return a.totalTimeMs > b.totalTimeMs;
        }
    );
    
    profile.operationTimings = timings;
    
    if (!epochTimes_.empty()) {
        for (double t : epochTimes_) {
            profile.totalEpochTimeMs += t;
        }
        profile.totalEpochTimeMs /= epochTimes_.size();
    }
    
    return profile;
}

std::vector<OpTiming> TrainingProfiler::getBottlenecks(int topN) const {
    std::vector<OpTiming> bottlenecks;
    
    for (const auto& [_, timing] : timings_) {
        bottlenecks.push_back(timing);
    }
    
    std::sort(bottlenecks.begin(), bottlenecks.end(),
        [](const OpTiming& a, const OpTiming& b) {
            return a.totalTimeMs > b.totalTimeMs;
        }
    );
    
    if (bottlenecks.size() > (size_t)topN) {
        bottlenecks.resize(topN);
    }
    
    return bottlenecks;
}

std::string TrainingProfiler::generateReport() const {
    std::stringstream ss;
    auto profile = getProfile();
    
    ss << "\n========== TRAINING PROFILE REPORT ==========\n";
    ss << "Average Epoch Time: " << std::fixed << std::setprecision(2) 
       << profile.totalEpochTimeMs << " ms\n\n";
    
    ss << "Operation Timings (Top 10):\n";
    ss << std::left << std::setw(30) << "Operation"
       << std::setw(15) << "Total (ms)"
       << std::setw(15) << "Calls"
       << std::setw(15) << "Avg (ms)"
       << std::setw(10) << "% of Total\n";
    ss << std::string(70, '-') << "\n";
    
    int count = 0;
    for (const auto& timing : profile.operationTimings) {
        if (count++ >= 10) break;
        
        ss << std::left << std::setw(30) << timing.operationName.substr(0, 29)
           << std::setw(15) << std::fixed << std::setprecision(2) << timing.totalTimeMs
           << std::setw(15) << timing.callCount
           << std::setw(15) << timing.avgTimeMs
           << std::setw(10) << timing.percentOfTotal << "%\n";
    }
    
    ss << "=============================================\n";
    return ss.str();
}

// ==================== SIMD Optimization Implementation ====================

void SIMDOptimizer::matmul(const float* A, const float* B, float* C,
                           int M, int N, int K, const HardwareProfile& hw) {
    switch (hw.cpuCapability) {
        case ComputeCapability::CPU_AVX512:
            matmul_avx512(A, B, C, M, N, K);
            break;
        case ComputeCapability::CPU_AVX2:
        case ComputeCapability::CPU_AVX:
            matmul_avx2(A, B, C, M, N, K);
            break;
        default: {
            // Baseline scalar implementation
            for (int i = 0; i < M; i++) {
                for (int j = 0; j < N; j++) {
                    float sum = 0;
                    for (int k = 0; k < K; k++) {
                        sum += A[i * K + k] * B[k * N + j];
                    }
                    C[i * N + j] = sum;
                }
            }
        }
    }
}

void SIMDOptimizer::matmul_avx2(const float* A, const float* B, float* C,
                                int M, int N, int K) {
    // Column-major access pattern for B for better cache locality
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j += 8) {
            __m256 sum = _mm256_setzero_ps();
            
            for (int k = 0; k < K; k++) {
                __m256 a = _mm256_set1_ps(A[i * K + k]);
                __m256 b = _mm256_loadu_ps(&B[k * N + j]);
                sum = _mm256_fmadd_ps(a, b, sum);
            }
            
            _mm256_storeu_ps(&C[i * N + j], sum);
        }
    }
}

void SIMDOptimizer::matmul_avx512(const float* A, const float* B, float* C,
                                  int M, int N, int K) {
    // 16-way SIMD with AVX-512 (16 floats per instruction)
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j += 16) {
            __m512 sum = _mm512_setzero_ps();
            
            for (int k = 0; k < K; k++) {
                __m512 a = _mm512_set1_ps(A[i * K + k]);
                __m512 b = _mm512_loadu_ps(&B[k * N + j]);
                sum = _mm512_fmadd_ps(a, b, sum);
            }
            
            _mm512_storeu_ps(&C[i * N + j], sum);
        }
    }
}

void SIMDOptimizer::gelu(float* data, size_t size, const HardwareProfile& hw) {
    // GELU: x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    const float sqrt2pi = 0.7978845608f;
    const float a = 0.044715f;
    
    for (size_t i = 0; i < size; i++) {
        float x = data[i];
        float x3 = x * x * x;
        float inner = sqrt2pi * (x + a * x3);
        float result = x * 0.5f * (1.0f + std::tanh(inner));
        data[i] = result;
    }
}

void SIMDOptimizer::relu(float* data, size_t size, const HardwareProfile& hw) {
    for (size_t i = 0; i < size; i++) {
        if (data[i] < 0) data[i] = 0;
    }
}

void SIMDOptimizer::softmax(float* data, int rows, int cols, const HardwareProfile& hw) {
    for (int i = 0; i < rows; i++) {
        float* row = data + i * cols;
        
        // Find max for numerical stability
        float maxVal = row[0];
        for (int j = 1; j < cols; j++) {
            maxVal = std::max(maxVal, row[j]);
        }
        
        // Compute exp and sum
        float sum = 0;
        for (int j = 0; j < cols; j++) {
            row[j] = std::exp(row[j] - maxVal);
            sum += row[j];
        }
        
        // Normalize
        for (int j = 0; j < cols; j++) {
            row[j] /= sum;
        }
    }
}

void SIMDOptimizer::layer_norm(float* x, const float* weight, const float* bias,
                               int batch_size, int hidden_size, const HardwareProfile& hw) {
    const float eps = 1e-5f;
    
    for (int b = 0; b < batch_size; b++) {
        float* x_b = x + b * hidden_size;
        
        // Compute mean and variance
        float mean = 0, var = 0;
        for (int i = 0; i < hidden_size; i++) {
            mean += x_b[i];
        }
        mean /= hidden_size;
        
        for (int i = 0; i < hidden_size; i++) {
            float diff = x_b[i] - mean;
            var += diff * diff;
        }
        var = var / hidden_size + eps;
        var = std::sqrt(var);
        
        // Normalize and scale
        for (int i = 0; i < hidden_size; i++) {
            x_b[i] = ((x_b[i] - mean) / var) * weight[i] + bias[i];
        }
    }
}

void SIMDOptimizer::attention(float* Q, float* K, float* V, float* output,
                              int batch_size, int seq_len, int dim, const HardwareProfile& hw) {
    int head_dim = dim / 8;  // Assume 8 attention heads
    float scale = 1.0f / std::sqrt(head_dim);
    
    for (int b = 0; b < batch_size; b++) {
        for (int h = 0; h < 8; h++) {
            float* q = Q + b * seq_len * dim + h * head_dim;
            float* k = K + b * seq_len * dim + h * head_dim;
            float* v = V + b * seq_len * dim + h * head_dim;
            float* out = output + b * seq_len * dim + h * head_dim;
            
            // Compute QK^T
            std::vector<float> scores(seq_len * seq_len);
            for (int i = 0; i < seq_len; i++) {
                for (int j = 0; j < seq_len; j++) {
                    float score = 0;
                    for (int d = 0; d < head_dim; d++) {
                        score += q[i * dim + d] * k[j * dim + d];
                    }
                    scores[i * seq_len + j] = score * scale;
                }
            }
            
            // Softmax
            SIMDOptimizer::softmax(scores.data(), seq_len, seq_len, hw);
            
            // Weighted sum
            for (int i = 0; i < seq_len; i++) {
                for (int d = 0; d < head_dim; d++) {
                    float sum = 0;
                    for (int j = 0; j < seq_len; j++) {
                        sum += scores[i * seq_len + j] * v[j * dim + d];
                    }
                    out[i * dim + d] = sum;
                }
            }
        }
    }
}

// ==================== Mixed Precision Implementation ====================

MixedPrecisionTrainer::MixedPrecisionTrainer(PrecisionMode mode, const HardwareProfile& hw)
    : mode_(mode), hw_(hw) {
    
    if (mode == PrecisionMode::FP16 || mode == PrecisionMode::BF16) {
        std::cout << "[MixedPrecisionTrainer] Initializing mixed precision training" << std::endl;
    }
}

void MixedPrecisionTrainer::toHalfPrecision(float* fp32_data, void* fp16_data, size_t size) {
    uint16_t* fp16_ptr = (uint16_t*)fp16_data;
    
    for (size_t i = 0; i < size; i++) {
        float f = fp32_data[i];
        // Simple FP32 to FP16 conversion (without special handling)
        uint32_t* f32 = (uint32_t*)&f;
        uint32_t s = (*f32 >> 31) & 0x1;
        uint32_t e = (*f32 >> 23) & 0xFF;
        uint32_t m = *f32 & 0x7FFFFF;
        
        uint16_t fp16 = (s << 15) | ((e - 112) << 10) | (m >> 13);
        fp16_ptr[i] = fp16;
    }
}

void MixedPrecisionTrainer::toBF16(float* fp32_data, void* bf16_data, size_t size) {
    uint16_t* bf16_ptr = (uint16_t*)bf16_data;
    
    for (size_t i = 0; i < size; i++) {
        float f = fp32_data[i];
        uint32_t* f32 = (uint32_t*)&f;
        bf16_ptr[i] = (*f32) >> 16;  // Simple truncation
    }
}

void MixedPrecisionTrainer::toFullPrecision(void* fp16_data, float* fp32_data, size_t size) {
    uint16_t* fp16_ptr = (uint16_t*)fp16_data;
    
    for (size_t i = 0; i < size; i++) {
        uint16_t fp16 = fp16_ptr[i];
        uint32_t s = (fp16 >> 15) & 0x1;
        uint32_t e = (fp16 >> 10) & 0x1F;
        uint32_t m = fp16 & 0x3FF;
        
        uint32_t fp32 = (s << 31) | ((e + 112) << 23) | (m << 13);
        fp32_data[i] = *(float*)&fp32;
    }
}

void MixedPrecisionTrainer::scaleLoss(float& loss, float scale_factor) {
    loss *= scale_factor;
}

void MixedPrecisionTrainer::unscaleGradients(float* gradients, size_t size, float scale_factor) {
    for (size_t i = 0; i < size; i++) {
        gradients[i] /= scale_factor;
    }
}

float MixedPrecisionTrainer::getNextLossScale() {
    // Adjust loss scale based on overflow detection
    if (overflowCount_ > 0) {
        currentLossScale_ *= 0.5f;  // Reduce by half on overflow
        overflowCount_ = 0;
        std::cout << "[MixedPrecisionTrainer] Reduced loss scale to " << currentLossScale_ << std::endl;
    } else if (successfulStepCount_ > 100) {
        currentLossScale_ *= 2.0f;  // Increase on success
        successfulStepCount_ = 0;
        std::cout << "[MixedPrecisionTrainer] Increased loss scale to " << currentLossScale_ << std::endl;
    }
    
    return currentLossScale_;
}

void MixedPrecisionTrainer::recordOverflow(bool overflowed) {
    if (overflowed) {
        overflowCount_++;
    } else {
        successfulStepCount_++;
    }
}

double MixedPrecisionTrainer::getMemorySavingsPercent() const {
    switch (mode_) {
        case PrecisionMode::FP32:
            return 0.0;
        case PrecisionMode::FP16:
            return 50.0;  // 50% memory savings
        case PrecisionMode::BF16:
            return 50.0;
        case PrecisionMode::TF32:
            return 33.0;
        case PrecisionMode::INT8:
            return 75.0;
        default:
            return 0.0;
    }
}

// ==================== Gradient Accumulation Implementation ====================

GradientAccumulator::GradientAccumulator(size_t numParameters, int accumSteps)
    : accumSteps_(accumSteps) {
    
    accumulatedGradients_.resize(numParameters, 0.0f);
}

void GradientAccumulator::accumulateGradients(const float* gradients, size_t size) {
    for (size_t i = 0; i < size; i++) {
        accumulatedGradients_[i] += gradients[i];
    }
    
    currentStep_++;
}

bool GradientAccumulator::isReadyForUpdate() const {
    return currentStep_ >= accumSteps_;
}

std::vector<float> GradientAccumulator::getAccumulatedGradients() const {
    std::vector<float> result = accumulatedGradients_;
    
    // Normalize by accumulation steps
    for (auto& g : result) {
        g /= accumSteps_;
    }
    
    return result;
}

void GradientAccumulator::reset() {
    std::fill(accumulatedGradients_.begin(), accumulatedGradients_.end(), 0.0f);
    currentStep_ = 0;
}

int GradientAccumulator::getEffectiveBatchSize(int miniBatchSize) const {
    return miniBatchSize * accumSteps_;
}

// ==================== Adaptive Scheduler Implementation ====================

TrainingSchedule AdaptiveScheduler::optimize(
    const HardwareProfile& hw,
    int numParameters,
    int seqLength,
    int datasetSize,
    float targetTimePerEpoch) {
    
    TrainingSchedule schedule;
    
    // Calculate optimal batch size based on hardware
    int hiddenDim = 768;  // Default
    if (numParameters < 100000000) {
        hiddenDim = 512;
    } else if (numParameters > 500000000) {
        hiddenDim = 1024;
    }
    
    schedule.batchSize = calculateOptimalBatchSize(hw, seqLength, hiddenDim, 12);
    
    // Estimate computation time per batch
    double computePerBatch = (numParameters * seqLength * 2.0) / (hw.totalGFlops * 1e9);  // seconds
    double batchesPerEpoch = datasetSize / schedule.batchSize;
    double estimatedEpochTime = computePerBatch * batchesPerEpoch;
    
    // Adjust accumulation steps if needed
    if (estimatedEpochTime > targetTimePerEpoch * 60) {
        schedule.useGradAccumulation = true;
        schedule.accumSteps = std::max(1, (int)(estimatedEpochTime / (targetTimePerEpoch * 60)));
    }
    
    // Enable optimizations based on hardware
    if (hw.hasGPU) {
        schedule.useMixedPrecision = true;
        schedule.precisionMode = hw.supportsMixedPrecision ? PrecisionMode::FP16 : PrecisionMode::FP32;
    }
    
    // Set learning rate
    schedule.learningRate = 1e-3f * std::sqrt(schedule.batchSize / 32.0f);
    
    // Estimate total time
    schedule.estimatedEpochTimeMins = estimatedEpochTime / 60.0;
    schedule.estimatedTotalTimeHours = (schedule.estimatedEpochTimeMins * schedule.numEpochs) / 60.0;
    
    std::cout << "[AdaptiveScheduler] Optimized Schedule:\n"
              << "  Batch Size: " << schedule.batchSize << "\n"
              << "  Accumulation Steps: " << schedule.accumSteps << "\n"
              << "  Learning Rate: " << std::scientific << schedule.learningRate << "\n"
              << "  Estimated Epoch Time: " << std::fixed << std::setprecision(2) 
              << schedule.estimatedEpochTimeMins << " mins\n"
              << "  Estimated Total Time: " << schedule.estimatedTotalTimeHours << " hours\n";
    
    return schedule;
}

int AdaptiveScheduler::calculateOptimalBatchSize(
    const HardwareProfile& hw,
    int seqLength,
    int hiddenDim,
    int numLayers) {
    
    // Memory per token: 2 * hidden_dim * num_layers * batch_size bytes (FP32)
    uint64_t memPerBatch = 4 * seqLength * hiddenDim * numLayers;
    
    // Use 80% of available memory for safety
    uint64_t availableMem = hw.availableRamBytes * 0.8;
    
    int maxBatchSize = availableMem / memPerBatch;
    
    // Cap at reasonable values
    maxBatchSize = std::min(maxBatchSize, 256);
    maxBatchSize = std::max(maxBatchSize, 1);
    
    // Round to power of 2
    int powerOf2 = 1;
    while (powerOf2 * 2 <= maxBatchSize) {
        powerOf2 *= 2;
    }
    
    return powerOf2;
}

int AdaptiveScheduler::calculateAccumSteps(int optimalBatch, int hardwareLimitBatch) {
    if (hardwareLimitBatch >= optimalBatch) {
        return 1;
    }
    
    return (optimalBatch + hardwareLimitBatch - 1) / hardwareLimitBatch;
}

double AdaptiveScheduler::estimateTrainingTime(
    const TrainingSchedule& schedule,
    int numParameters,
    int datasetSize,
    const HardwareProfile& hw) {
    
    // Simplified estimation: (parameters * 2 FLOPs per parameter) / peak GFlops
    double flopsPerSample = numParameters * 2.0;
    double totalFlops = flopsPerSample * datasetSize * schedule.numEpochs;
    double effectiveGFlops = hw.totalGFlops;
    
    // Account for communication overhead in distributed setting
    if (schedule.useDistributed) {
        effectiveGFlops *= 0.9;  // 10% communication overhead
    }
    
    double timeSeconds = totalFlops / (effectiveGFlops * 1e9);
    return timeSeconds / 3600.0;  // Convert to hours
}

// ==================== Training Optimizer Main Implementation ====================

TrainingOptimizer::TrainingOptimizer() {
    profiler_ = std::make_unique<TrainingProfiler>();
}

void TrainingOptimizer::detectHardware() {
    hw_ = HardwareDetector::detectHardware();
    HardwareDetector::validateHardware(hw_);
    std::cout << HardwareDetector::getHardwareDescription(hw_);
}

void TrainingOptimizer::profileTraining(std::function<void(TrainingProfiler&)> trainingFn) {
    trainingFn(*profiler_);
    profile_ = profiler_->getProfile();
}

TrainingSchedule TrainingOptimizer::optimizeSchedule(
    int numParameters,
    int seqLength,
    int datasetSize) {
    
    return AdaptiveScheduler::optimize(hw_, numParameters, seqLength, datasetSize);
}

TrainingOptimizer::OptimizationRecommendations TrainingOptimizer::getRecommendations() {
    OptimizationRecommendations recs;
    
    auto bottlenecks = profiler_->getBottlenecks(5);
    
    for (const auto& timing : bottlenecks) {
        if (timing.operationName.find("matmul") != std::string::npos && timing.percentOfTotal > 50) {
            recs.recommendations.push_back("Enable GPU acceleration for matrix multiplication");
            recs.timeReductionPercent += 30;
        }
        
        if (timing.operationName.find("attention") != std::string::npos && timing.percentOfTotal > 30) {
            recs.recommendations.push_back("Optimize attention mechanism (flash attention, etc.)");
            recs.timeReductionPercent += 25;
        }
        
        if (timing.operationName.find("communication") != std::string::npos) {
            recs.recommendations.push_back("Use gradient compression for distributed training");
            recs.timeReductionPercent += 15;
        }
    }
    
    if (!hw_.hasGPU) {
        recs.recommendations.push_back("Consider GPU acceleration for 10x speedup");
        recs.timeReductionPercent = std::min(90.0, recs.timeReductionPercent + 50);
    }
    
    if (profile_.utilizationPercent < 50) {
        recs.recommendations.push_back("Increase batch size to improve hardware utilization");
        recs.timeReductionPercent += 20;
    }
    
    return recs;
}

const HardwareProfile& TrainingOptimizer::getHardware() const {
    return hw_;
}

const TrainingProfile& TrainingOptimizer::getProfile() const {
    return profile_;
}

std::string TrainingOptimizer::generateOptimizationReport() const {
    std::stringstream ss;
    
    ss << profiler_->generateReport();
    
    double timeReduction = 100 - (profile_.utilizationPercent / 100 * 100);
    ss << "\nEstimated Optimization Potential: " << std::fixed << std::setprecision(1) 
       << timeReduction << "% time reduction\n";
    
    return ss.str();
}

} // namespace TrainingOptimizer
