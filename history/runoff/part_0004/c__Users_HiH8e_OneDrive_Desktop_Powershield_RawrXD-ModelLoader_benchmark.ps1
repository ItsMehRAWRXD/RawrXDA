# Benchmark script to compare scalar vs AVX2 performance
# Usage: .\benchmark.ps1

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  MATRIX MULTIPLICATION BENCHMARK: SCALAR vs AVX2" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Test dimensions (typical LLM embedding size)
$N = 1      # Batch size (single token)
$M = 4096   # Embedding dimension
$K = 4096   # Output dimension

Write-Host "📊 Test Configuration:" -ForegroundColor Green
Write-Host "   Dimensions: $N × $M @ $M × $K" -ForegroundColor Gray
Write-Host "   Operations: $([math]::Round($N * $M * $K * 2 / 1e9, 2)) GFLOP" -ForegroundColor Gray
Write-Host "   Memory: $([math]::Round(($N*$M + $M*$K + $N*$K) * 4 / 1024 / 1024, 1)) MB" -ForegroundColor Gray
Write-Host ""

# Create C++ benchmark
$code = @'
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <cmath>

#if defined(__AVX2__) || defined(__AVX2)
#include <immintrin.h>
#define HAS_AVX2 1
#endif

void matmul_scalar(const float* A, const float* B, float* C, int N, int M, int K) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            float s = 0.0f;
            for (int k = 0; k < M; ++k) s += A[i * M + k] * B[k * K + j];
            C[i * K + j] = s;
        }
    }
}

#ifdef HAS_AVX2
void matmul_avx2(const float* A, const float* B, float* C, int N, int M, int K) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            __m256 sum = _mm256_setzero_ps();
            int k = 0;
            for (; k + 7 < M; k += 8) {
                __m256 a = _mm256_loadu_ps(&A[i * M + k]);
                __m256 b = _mm256_set_ps(B[(k+7)*K+j], B[(k+6)*K+j], B[(k+5)*K+j], B[(k+4)*K+j],
                                         B[(k+3)*K+j], B[(k+2)*K+j], B[(k+1)*K+j], B[k*K+j]);
                sum = _mm256_fmadd_ps(a, b, sum);
            }
            float r[8]; _mm256_storeu_ps(r, sum);
            float s = r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+r[7];
            for (; k < M; ++k) s += A[i*M+k] * B[k*K+j];
            C[i*K+j] = s;
        }
    }
}
#endif

int main() {
    const int N = 1, M = 4096, K = 4096, iters = 10;
    std::vector<float> A(N*M), B(M*K), C(N*K);
    std::mt19937 rng(42); std::uniform_real_distribution<float> d(-1,1);
    for(auto&v:A)v=d(rng); for(auto&v:B)v=d(rng);
    
    matmul_scalar(A.data(), B.data(), C.data(), N, M, K);
    auto t0 = std::chrono::high_resolution_clock::now();
    for(int i=0; i<iters; ++i) matmul_scalar(A.data(), B.data(), C.data(), N, M, K);
    auto t1 = std::chrono::high_resolution_clock::now();
    double scalar_ms = std::chrono::duration<double,std::milli>(t1-t0).count() / iters;
    std::cout << "SCALAR:" << scalar_ms << "\n";
    
#ifdef HAS_AVX2
    matmul_avx2(A.data(), B.data(), C.data(), N, M, K);
    t0 = std::chrono::high_resolution_clock::now();
    for(int i=0; i<iters; ++i) matmul_avx2(A.data(), B.data(), C.data(), N, M, K);
    t1 = std::chrono::high_resolution_clock::now();
    double avx2_ms = std::chrono::duration<double,std::milli>(t1-t0).count() / iters;
    std::cout << "AVX2:" << avx2_ms << "\n";
    std::cout << "SPEEDUP:" << (scalar_ms/avx2_ms) << "\n";
#else
    std::cout << "AVX2:DISABLED\n";
#endif
    return 0;
}
'@

Set-Content -Path "bench_temp.cpp" -Value $code

# Compile and run scalar version
Write-Host "🔨 Building scalar version..." -ForegroundColor Cyan
cmake -B build-bench-scalar -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_VULKAN=OFF 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Using direct compilation..." -ForegroundColor Yellow
    # Fallback: compile directly
    & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" /O2 /std:c++17 /Fe:bench_scalar.exe bench_temp.cpp 2>&1 | Out-Null
}

# Compile and run AVX2 version  
Write-Host "🔨 Building AVX2 version..." -ForegroundColor Cyan
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" /O2 /arch:AVX2 /std:c++17 /Fe:bench_avx2.exe bench_temp.cpp 2>&1 | Out-Null

# Run benchmarks
if (Test-Path "bench_scalar.exe") {
    Write-Host "⚡ Running scalar benchmark..." -ForegroundColor Yellow
    $scalar_out = & .\bench_scalar.exe
    $scalar_ms = [double]($scalar_out | Select-String "SCALAR:(\d+\.?\d*)" | ForEach-Object { $_.Matches.Groups[1].Value })
    Write-Host "   Result: $scalar_ms ms/iteration" -ForegroundColor Gray
}

if (Test-Path "bench_avx2.exe") {
    Write-Host "⚡ Running AVX2 benchmark..." -ForegroundColor Yellow
    $avx2_out = & .\bench_avx2.exe
    $avx2_ms = [double]($avx2_out | Select-String "AVX2:(\d+\.?\d*)" | ForEach-Object { $_.Matches.Groups[1].Value })
    $speedup = [double]($avx2_out | Select-String "SPEEDUP:(\d+\.?\d*)" | ForEach-Object { $_.Matches.Groups[1].Value })
    Write-Host "   Result: $avx2_ms ms/iteration" -ForegroundColor Gray
    Write-Host ""
    Write-Host "📈 PERFORMANCE SUMMARY:" -ForegroundColor Green
    Write-Host "   Scalar:  $scalar_ms ms" -ForegroundColor Gray
    Write-Host "   AVX2:    $avx2_ms ms" -ForegroundColor Gray
    Write-Host "   Speedup: $([math]::Round($speedup, 2))×" -ForegroundColor Cyan
    Write-Host ""
    if ($speedup -ge 1.8) {
        Write-Host "✓ TARGET ACHIEVED: $([math]::Round($speedup, 2))× ≥ 1.8× goal" -ForegroundColor Green
    } else {
        Write-Host "⚠ Below target: $([math]::Round($speedup, 2))× < 1.8× goal" -ForegroundColor Yellow
    }
}

# Cleanup
Remove-Item bench_temp.cpp -ErrorAction SilentlyContinue
Remove-Item bench_scalar.exe -ErrorAction SilentlyContinue
Remove-Item bench_avx2.exe -ErrorAction SilentlyContinue
Remove-Item *.obj -ErrorAction SilentlyContinue

Write-Host ""
