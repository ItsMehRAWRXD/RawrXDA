// ============================================================================
// fp8_quantizer_avx512.cpp - AVX-512 FP8 Quantization Implementation
// ============================================================================

#include "kernels/fp8_quantizer_avx512.hpp"
#include <cstring>
#include <cmath>
#include <chrono>
#include <cstdio>

// For CPUID
#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace RawrXD {
namespace Kernels {

// ============================================================================
// CPU Feature Detection
// ============================================================================
void CPUFeatures::Detect() {
    int cpuInfo[4] = {0};
    
    // Check max function
    __cpuid(cpuInfo, 0);
    int maxFunc = cpuInfo[0];
    
    if (maxFunc >= 1) {
        __cpuid(cpuInfo, 1);
        // Check AVX2 (bit 5 of EBX)
        has_avx2 = (cpuInfo[2] & (1 << 28)) != 0;  // Actually AVX, check properly below
    }
    
    if (maxFunc >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        // EBX bits:
        // bit 5 = AVX2
        // bit 16 = AVX-512F
        // bit 17 = AVX-512DQ
        // bit 30 = AVX-512BW
        // bit 31 = AVX-512VL
        has_avx2 = (cpuInfo[1] & (1 << 5)) != 0;
        has_avx512f = (cpuInfo[1] & (1 << 16)) != 0;
        has_avx512dq = (cpuInfo[1] & (1 << 17)) != 0;
        has_avx512bw = (cpuInfo[1] & (1 << 30)) != 0;
        has_avx512vl = (cpuInfo[1] & (1 << 31)) != 0;
    }
    
    // Check XCR0 for ZMM state (OS support)
    if (has_avx512f) {
        #ifdef _WIN32
        unsigned __int64 xcr0 = _xgetbv(0);
        #else
        unsigned int eax, edx;
        __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
        unsigned __int64 xcr0 = ((unsigned __int64)edx << 32) | eax;
        #endif
        
        // Check ZMM[0:15] (bit 5) and ZMM[16:31] (bit 6) and opmask (bit 7)
        if ((xcr0 & 0xE0) != 0xE0) {
            has_avx512f = false;
            has_avx512vl = false;
            has_avx512bw = false;
            has_avx512dq = false;
        }
    }
}

// ============================================================================
// Scalar reference implementation (for fallback)
// ============================================================================
static constexpr float E4M3_MAX = 448.0f;

static uint8_t FloatToE4M3_Scalar(float f) {
    uint8_t sign = (f < 0) ? 0x80 : 0;
    f = std::abs(f);
    if (f > E4M3_MAX) f = E4M3_MAX;
    int val = static_cast<int>(std::nearbyint(f));
    if (val > 255) val = 255;
    if (val < 0) val = 0;
    return sign | static_cast<uint8_t>(val);
}

static void QuantizeScalar(const float* input, uint8_t* output, size_t count, float scale) {
    for (size_t i = 0; i < count; i++) {
        output[i] = FloatToE4M3_Scalar(input[i] * scale);
    }
}

// ============================================================================
// AVX2 implementation (8-wide)
// ============================================================================
#ifdef __AVX2__
#include <immintrin.h>

static void QuantizeAVX2(const float* input, uint8_t* output, size_t count, float scale) {
    const __m256 scale_vec = _mm256_set1_ps(scale);
    const __m256 max_val = _mm256_set1_ps(E4M3_MAX);
    const __m256i sign_mask = _mm256_set1_epi32(0x80000000);
    const __m256i abs_mask = _mm256_set1_epi32(0x7FFFFFFF);
    const __m256i sign_shift = _mm256_set1_epi32(24);
    
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        __m256 vec = _mm256_loadu_ps(input + i);
        
        // Extract sign
        __m256i sign_bits = _mm256_and_si256(_mm256_castps_si256(vec), sign_mask);
        sign_bits = _mm256_srli_epi32(sign_bits, 24);
        
        // Absolute value
        __m256 abs_val = _mm256_and_ps(vec, _mm256_castsi256_ps(abs_mask));
        
        // Scale
        __m256 scaled = _mm256_mul_ps(abs_val, scale_vec);
        
        // Clamp
        __m256 clamped = _mm256_min_ps(scaled, max_val);
        
        // Round and convert to int
        __m256i int_vals = _mm256_cvtps_epi32(clamped);
        
        // Clamp to 0-255
        int_vals = _mm256_max_epi32(int_vals, _mm256_setzero_si256());
        int_vals = _mm256_min_epi32(int_vals, _mm256_set1_epi32(255));
        
        // Pack to bytes
        __m256i packed16 = _mm256_packus_epi32(int_vals, int_vals);
        __m256i packed8 = _mm256_packus_epi16(packed16, packed16);
        
        // Extract sign bytes
        __m256i sign_bytes = _mm256_packus_epi32(sign_bits, sign_bits);
        sign_bytes = _mm256_packus_epi16(sign_bytes, sign_bytes);
        
        // Combine
        __m256i result = _mm256_or_si256(packed8, sign_bytes);
        
        // Store
        int temp[8];
        _mm256_storeu_si256((__m256i*)temp, result);
        for (int j = 0; j < 8; j++) {
            output[i + j] = (uint8_t)temp[j];
        }
    }
    
    // Scalar tail
    for (; i < count; i++) {
        output[i] = FloatToE4M3_Scalar(input[i] * scale);
    }
}
#else
static void QuantizeAVX2(const float* input, uint8_t* output, size_t count, float scale) {
    QuantizeScalar(input, output, count, scale);
}
#endif

// ============================================================================
// AVX-512 implementation (16-wide)
// ============================================================================
#ifdef __AVX512F__
#include <immintrin.h>

static void QuantizeAVX512(const float* input, uint8_t* output, size_t count, float scale) {
    const __m512 scale_vec = _mm512_set1_ps(scale);
    const __m512 max_val = _mm512_set1_ps(E4M3_MAX);
    const __m512i sign_mask = _mm512_set1_epi32(0x80000000);
    const __m512i abs_mask = _mm512_set1_epi32(0x7FFFFFFF);
    
    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        __m512 vec = _mm512_loadu_ps(input + i);
        
        // Extract sign bits
        __m512i sign_bits = _mm512_and_si512(_mm512_castps_si512(vec), sign_mask);
        sign_bits = _mm512_srli_epi32(sign_bits, 24);
        
        // Absolute value
        __m512 abs_val = _mm512_and_ps(vec, _mm512_castsi512_ps(abs_mask));
        
        // Scale
        __m512 scaled = _mm512_mul_ps(abs_val, scale_vec);
        
        // Clamp
        __m512 clamped = _mm512_min_ps(scaled, max_val);
        
        // Round to nearest even and convert to int
        __m512i int_vals = _mm512_cvtps_epi32(clamped);
        
        // Clamp to 0-255 and pack
        __m512i zero = _mm512_setzero_si512();
        int_vals = _mm512_max_epi32(int_vals, zero);
        
        // Pack 16 int32 -> 16 uint8 with saturation
        __m128i packed = _mm512_cvtusepi32_epi8(int_vals);
        
        // Pack sign bits
        __m128i sign_packed = _mm512_cvtusepi32_epi8(sign_bits);
        
        // Combine
        __m128i result = _mm_or_si128(packed, sign_packed);
        
        // Store
        _mm_storeu_si128((__m128i*)(output + i), result);
    }
    
    // AVX2 tail
    QuantizeAVX2(input + i, output + i, count - i, scale);
}
#else
static void QuantizeAVX512(const float* input, uint8_t* output, size_t count, float scale) {
    QuantizeAVX2(input, output, count, scale);
}
#endif

// ============================================================================
// FP8QuantizerAVX512 Implementation
// ============================================================================
FP8QuantizerAVX512::FP8QuantizerAVX512() = default;
FP8QuantizerAVX512::~FP8QuantizerAVX512() = default;

bool FP8QuantizerAVX512::Initialize(QuantizeStrategy strategy) {
    requestedStrategy_ = strategy;
    features_.Detect();
    SelectStrategy();
    initialized_ = true;
    
    if (!silent_) {
        printf("[FP8Quantizer] Initialized with strategy: %s\n",
               currentStrategy_ == QuantizeStrategy::AVX512 ? "AVX-512" :
               currentStrategy_ == QuantizeStrategy::AVX2 ? "AVX2" : "Scalar");
        printf("[FP8Quantizer] CPU Features: AVX512F=%d AVX2=%d\n",
               features_.has_avx512f, features_.has_avx2);
    }
    
    return true;
}

void FP8QuantizerAVX512::Shutdown() {
    if (initialized_) {
        PrintReport();
        initialized_ = false;
    }
}

void FP8QuantizerAVX512::SelectStrategy() {
    switch (requestedStrategy_) {
        case QuantizeStrategy::AVX512:
            currentStrategy_ = features_.HasAVX512() ? QuantizeStrategy::AVX512 : QuantizeStrategy::AVX2;
            break;
        case QuantizeStrategy::AVX2:
            currentStrategy_ = features_.has_avx2 ? QuantizeStrategy::AVX2 : QuantizeStrategy::Scalar;
            break;
        case QuantizeStrategy::Scalar:
            currentStrategy_ = QuantizeStrategy::Scalar;
            break;
        case QuantizeStrategy::Auto:
        default:
            if (features_.HasAVX512()) {
                currentStrategy_ = QuantizeStrategy::AVX512;
            } else if (features_.has_avx2) {
                currentStrategy_ = QuantizeStrategy::AVX2;
            } else {
                currentStrategy_ = QuantizeStrategy::Scalar;
            }
            break;
    }
}

void FP8QuantizerAVX512::Quantize(const float* input, uint8_t* output, size_t count, float scale) {
    if (!initialized_) {
        Initialize();
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    switch (currentStrategy_) {
        case QuantizeStrategy::AVX512:
            QuantizeAVX512(input, output, count, scale);
            break;
        case QuantizeStrategy::AVX2:
            QuantizeAVX2(input, output, count, scale);
            break;
        case QuantizeStrategy::Scalar:
        default:
            QuantizeScalar(input, output, count, scale);
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    UpdateMetrics(currentStrategy_, count, nanos);
}

void FP8QuantizerAVX512::QuantizeWithStrategy(const float* input, uint8_t* output,
                                               size_t count, float scale,
                                               QuantizeStrategy strategy) {
    auto start = std::chrono::high_resolution_clock::now();
    
    switch (strategy) {
        case QuantizeStrategy::AVX512:
            QuantizeAVX512(input, output, count, scale);
            break;
        case QuantizeStrategy::AVX2:
            QuantizeAVX2(input, output, count, scale);
            break;
        case QuantizeStrategy::Scalar:
            QuantizeScalar(input, output, count, scale);
            break;
        case QuantizeStrategy::Auto:
        default:
            Quantize(input, output, count, scale);
            return;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    UpdateMetrics(strategy, count, nanos);
}

void FP8QuantizerAVX512::UpdateMetrics(QuantizeStrategy used, size_t elements, uint64_t nanoseconds) {
    metrics_.totalCalls++;
    metrics_.totalElements += elements;
    
    switch (used) {
        case QuantizeStrategy::AVX512:
            metrics_.avx512Calls++;
            break;
        case QuantizeStrategy::AVX2:
            metrics_.avx2Calls++;
            break;
        case QuantizeStrategy::Scalar:
            metrics_.scalarCalls++;
            break;
        default:
            break;
    }
    
    // Update running average throughput
    double seconds = nanoseconds / 1e9;
    double throughput = elements / seconds;
    metrics_.avgThroughput = (metrics_.avgThroughput * (metrics_.totalCalls - 1) + throughput) / metrics_.totalCalls;
}

size_t FP8QuantizerAVX512::GetOptimalBatchSize() const {
    switch (currentStrategy_) {
        case QuantizeStrategy::AVX512:
            return 256;  // 16 elements * 16 for cache line efficiency
        case QuantizeStrategy::AVX2:
            return 128;  // 8 elements * 16
        case QuantizeStrategy::Scalar:
        default:
            return 64;
    }
}

bool FP8QuantizerAVX512::IsAVX512Available() {
    CPUFeatures features;
    features.Detect();
    return features.HasAVX512();
}

void QuantizeMetrics::RecordCall(QuantizeStrategy strategy, size_t elements, uint64_t nanoseconds) {
    totalCalls++;
    totalElements += elements;
    
    switch (strategy) {
        case QuantizeStrategy::AVX512:
            avx512Calls++;
            break;
        case QuantizeStrategy::AVX2:
            avx2Calls++;
            break;
        case QuantizeStrategy::Scalar:
            scalarCalls++;
            break;
        default:
            break;
    }
    
    double seconds = nanoseconds / 1e9;
    double throughput = elements / seconds;
    avgThroughput = (avgThroughput * (totalCalls - 1) + throughput) / totalCalls;
}

void QuantizeMetrics::PrintReport() const {
    printf("\n");
    printf("========================================\n");
    printf("FP8 Quantization Metrics\n");
    printf("========================================\n");
    printf("Total calls:       %llu\n", (unsigned long long)totalCalls);
    printf("Total elements:    %llu\n", (unsigned long long)totalElements);
    printf("\n");
    printf("Strategy breakdown:\n");
    printf("  AVX-512:         %llu (%.1f%%)\n", 
           (unsigned long long)avx512Calls,
           totalCalls > 0 ? (100.0 * avx512Calls / totalCalls) : 0.0);
    printf("  AVX2:            %llu (%.1f%%)\n",
           (unsigned long long)avx2Calls,
           totalCalls > 0 ? (100.0 * avx2Calls / totalCalls) : 0.0);
    printf("  Scalar:          %llu (%.1f%%)\n",
           (unsigned long long)scalarCalls,
           totalCalls > 0 ? (100.0 * scalarCalls / totalCalls) : 0.0);
    printf("\n");
    printf("Avg throughput:    %.2f M elements/sec\n", avgThroughput / 1e6);
    printf("========================================\n");
}

void FP8QuantizerAVX512::PrintReport() const {
    metrics_.PrintReport();
}

// ============================================================================
// Global functions
// ============================================================================

// Forward declaration for benchmark speedup calculation
double BenchmarkFP8Quantization_ScalarRef(size_t elementCount);

size_t GetRecommendedFP8BatchSize() {
    FP8QuantizerAVX512 quantizer;
    quantizer.Initialize();
    return quantizer.GetOptimalBatchSize();
}

void BenchmarkFP8Quantization(size_t elementCount) {
    printf("\n========================================\n");
    printf("FP8 Quantization Benchmark\n");
    printf("========================================\n");
    printf("Elements: %zu\n\n", elementCount);
    
    // Allocate aligned memory
    float* input = (float*)_aligned_malloc(elementCount * sizeof(float), 64);
    uint8_t* output = (uint8_t*)_aligned_malloc(elementCount, 64);
    
    // Initialize with test data
    for (size_t i = 0; i < elementCount; i++) {
        input[i] = (float)(i % 100) * 0.1f;
    }
    
    // Test each strategy
    const int iterations = 100;
    
    for (auto strategy : {QuantizeStrategy::Scalar, QuantizeStrategy::AVX2, QuantizeStrategy::AVX512}) {
        FP8QuantizerAVX512 quantizer;
        if (!quantizer.Initialize(strategy)) {
            printf("Strategy %d: Not available\n", (int)strategy);
            continue;
        }
        
        // Warmup
        quantizer.Quantize(input, output, elementCount, 1.0f);
        
        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            quantizer.Quantize(input, output, elementCount, 1.0f);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double seconds = nanos / 1e9;
        double avgTime = seconds / iterations;
        double throughput = elementCount / avgTime;
        
        const char* name = strategy == QuantizeStrategy::AVX512 ? "AVX-512" :
                          strategy == QuantizeStrategy::AVX2 ? "AVX2" : "Scalar";
        
        printf("%s:\n", name);
        printf("  Avg time:        %.3f ms\n", avgTime * 1000);
        printf("  Throughput:      %.2f M elements/sec\n", throughput / 1e6);
        double scalarTime = BenchmarkFP8Quantization_ScalarRef(elementCount);
        printf("  Speedup vs scalar: %.2fx\n", 
               strategy == QuantizeStrategy::Scalar ? 1.0 : scalarTime / avgTime);
        printf("\n");
    }
    
    _aligned_free(input);
    _aligned_free(output);
}

// Helper for speedup calculation
double BenchmarkFP8Quantization_ScalarRef(size_t elementCount) {
    float* input = (float*)_aligned_malloc(elementCount * sizeof(float), 64);
    uint8_t* output = (uint8_t*)_aligned_malloc(elementCount, 64);
    
    for (size_t i = 0; i < elementCount; i++) {
        input[i] = (float)(i % 100) * 0.1f;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    QuantizeScalar(input, output, elementCount, 1.0f);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    _aligned_free(input);
    _aligned_free(output);
    
    return nanos / 1e9;
}

} // namespace Kernels
} // namespace RawrXD
