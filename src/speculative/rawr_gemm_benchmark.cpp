/*
====================================================================
 RAWR GEMM BENCHMARK
 Validate AVX-512 performance vs naive implementation
====================================================================
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <iomanip>

#include "rawr_gemm_avx512.h"

using namespace std;

// Naive matmul for comparison
vector<float> matmul_naive(const float* w, const vector<float>& x, int rows, int cols) {
    vector<float> out(rows, 0);
    for (int r = 0; r < rows; r++) {
        float sum = 0;
        for (int c = 0; c < cols; c++) {
            sum += w[r * cols + c] * x[c];
        }
        out[r] = sum;
    }
    return out;
}

// Numerical correctness check
bool check_correctness(const vector<float>& a, const vector<float>& b, float tol = 0.001f) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (fabs(a[i] - b[i]) > tol) return false;
    }
    return true;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║     RAWR AVX-512 GEMM BENCHMARK                            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    // Check AVX-512 support
    cout << "\nCPU Features:" << endl;
    cout << "  AVX-512: " << (rawr::has_avx512() ? "YES ✓" : "NO (using scalar fallback)") << endl;
    
    // Test configurations (typical LLM dimensions)
    vector<tuple<int, int, int>> configs = {
        {4096, 4096, 100},    // Large model FFN
        {512, 4096, 1000},    // Medium model attention
        {4096, 14336, 50},    // MoE expert (large FFN)
    };
    
    mt19937 rng(42);
    uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (auto& [rows, cols, iterations] : configs) {
        cout << "\n--- Benchmark: " << rows << "×" << cols << " (" << iterations << " iters) ---" << endl;
        
        // Allocate matrices
        vector<float> W(rows * cols);
        vector<float> x(cols);
        
        for (auto& v : W) v = dist(rng);
        for (auto& v : x) v = dist(rng);
        
        // Warmup
        for (int i = 0; i < 10; i++) {
            auto y1 = rawr::matmul_avx512(W.data(), x, rows, cols);
        }
        
        // Benchmark naive
        auto start = chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            auto y_naive = matmul_naive(W.data(), x, rows, cols);
            volatile float sink = y_naive[0];
            (void)sink;
        }
        auto end = chrono::high_resolution_clock::now();
        auto ms_naive = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        
        // Benchmark AVX-512
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            auto y_avx = rawr::matmul_avx512(W.data(), x, rows, cols);
            volatile float sink = y_avx[0];
            (void)sink;
        }
        end = chrono::high_resolution_clock::now();
        auto ms_avx = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        
        // Calculate GFLOPS
        float ops_per_mm = 2.0f * rows * cols;  // multiply-adds
        float total_ops = ops_per_mm * iterations;
        float gflops_naive = (total_ops / (ms_naive / 1000.0f)) / 1e9;
        float gflops_avx = (total_ops / (ms_avx / 1000.0f)) / 1e9;
        
        // Verify correctness
        auto y_naive = matmul_naive(W.data(), x, rows, cols);
        auto y_avx = rawr::matmul_avx512(W.data(), x, rows, cols);
        bool correct = check_correctness(y_naive, y_avx, 0.01f);
        
        cout << "  Naive:   " << ms_naive << " ms (" << fixed << setprecision(2) << gflops_naive << " GFLOPS)" << endl;
        cout << "  AVX-512: " << ms_avx << " ms (" << fixed << setprecision(2) << gflops_avx << " GFLOPS)" << endl;
        cout << "  Speedup: " << fixed << setprecision(2) << ((float)ms_naive / ms_avx) << "x" << endl;
        cout << "  Correct: " << (correct ? "PASS ✓" : "FAIL ✗") << endl;
    }
    
    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    BENCHMARK COMPLETE                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
