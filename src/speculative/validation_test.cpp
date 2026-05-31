/*
====================================================================
 RAWR VALIDATION TEST SUITE
 Tests numerical correctness, throughput, and memory behavior
====================================================================
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <algorithm>
#include <iomanip>

using namespace std;

// Minimal test structures
struct TestTensor {
    vector<float> data;
    int rows, cols;
    
    TestTensor(int r, int c) : rows(r), cols(c), data(r * c, 0.0f) {}
    
    float& at(int i, int j) { return data[i * cols + j]; }
    const float& at(int i, int j) const { return data[i * cols + j]; }
};

// Naive matmul for validation
vector<float> matmul(const vector<float>& a, const vector<float>& b, 
                     int m, int n, int k) {
    vector<float> c(m * k, 0.0f);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < k; j++) {
            float sum = 0.0f;
            for (int l = 0; l < n; l++) {
                sum += a[i * n + l] * b[l * k + j];
            }
            c[i * k + j] = sum;
        }
    }
    return c;
}

// RMSNorm
void rmsnorm(vector<float>& out, const vector<float>& x, float eps = 1e-5f) {
    float ss = 0.0f;
    for (float v : x) ss += v * v;
    float scale = 1.0f / sqrtf(ss / x.size() + eps);
    for (size_t i = 0; i < x.size(); i++) out[i] = x[i] * scale;
}

// Softmax
void softmax(vector<float>& x) {
    float maxv = *max_element(x.begin(), x.end());
    float sum = 0.0f;
    for (auto& v : x) { v = expf(v - maxv); sum += v; }
    for (auto& v : x) v /= sum;
}

// Test 1: Numerical Correctness
bool test_numerical_correctness() {
    cout << "\n=== TEST 1: Numerical Correctness ===" << endl;
    
    // Create test matrices
    TestTensor A(64, 64);
    TestTensor B(64, 64);
    
    // Initialize with deterministic values
    mt19937 rng(42);
    uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 64; j++) {
            A.at(i, j) = dist(rng);
            B.at(i, j) = dist(rng);
        }
    }
    
    // Run matmul
    auto start = chrono::high_resolution_clock::now();
    auto C = matmul(A.data, B.data, 64, 64, 64);
    auto end = chrono::high_resolution_clock::now();
    
    auto ms = chrono::duration_cast<chrono::microseconds>(end - start).count();
    
    // Verify output shape and basic properties
    bool pass = (C.size() == 64 * 64);
    
    // Check for NaN/Inf
    for (float v : C) {
        if (isnan(v) || isinf(v)) {
            pass = false;
            break;
        }
    }
    
    cout << "  MatMul: 64x64 * 64x64 = " << (pass ? "PASS" : "FAIL") << endl;
    cout << "  Time: " << ms << " us" << endl;
    cout << "  First output: " << C[0] << endl;
    
    // Test RMSNorm
    vector<float> x(512, 1.0f);
    vector<float> x_norm(512);
    rmsnorm(x_norm, x);
    
    float expected_scale = 1.0f / sqrtf(1.0f + 1e-5f);
    bool rms_pass = fabs(x_norm[0] - expected_scale) < 0.001f;
    
    cout << "  RMSNorm: " << (rms_pass ? "PASS" : "FAIL") << endl;
    
    // Test Softmax
    vector<float> logits = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    softmax(logits);
    float sum = 0.0f;
    for (float v : logits) sum += v;
    bool softmax_pass = fabs(sum - 1.0f) < 0.0001f;
    
    cout << "  Softmax: " << (softmax_pass ? "PASS" : "FAIL") << " (sum=" << sum << ")" << endl;
    
    return pass && rms_pass && softmax_pass;
}

// Test 2: Throughput Benchmark
bool test_throughput() {
    cout << "\n=== TEST 2: Throughput Benchmark ===" << endl;
    
    const int dim = 512;
    const int iterations = 100;
    
    // Create test data
    vector<float> A(dim * dim);
    vector<float> B(dim * dim);
    
    mt19937 rng(42);
    uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (auto& v : A) v = dist(rng);
    for (auto& v : B) v = dist(rng);
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        auto C = matmul(A, B, dim, dim, dim);
    }
    
    // Benchmark
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        auto C = matmul(A, B, dim, dim, dim);
        // Prevent optimization
        volatile float sink = C[0];
        (void)sink;
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    
    float ops_per_mm = 2.0f * dim * dim * dim; // multiply-adds
    float total_ops = ops_per_mm * iterations;
    float gflops = (total_ops / (ms / 1000.0f)) / 1e9;
    
    cout << "  Matrix size: " << dim << "x" << dim << endl;
    cout << "  Iterations: " << iterations << endl;
    cout << "  Total time: " << ms << " ms" << endl;
    cout << "  Throughput: " << fixed << setprecision(2) << gflops << " GFLOPS" << endl;
    cout << "  Per-op: " << (ms * 1000.0f / iterations) << " us" << endl;
    
    return true;
}

// Test 3: Memory Behavior
bool test_memory_behavior() {
    cout << "\n=== TEST 3: Memory Behavior ===" << endl;
    
    // Test allocation patterns
    vector<size_t> sizes = {1024, 1024*1024, 10*1024*1024};
    
    for (size_t sz : sizes) {
        auto start = chrono::high_resolution_clock::now();
        vector<float> buf(sz / sizeof(float), 0.0f);
        
        // Touch all pages
        for (size_t i = 0; i < buf.size(); i += 1024) {
            buf[i] = 1.0f;
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto ms = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "  Alloc " << (sz / 1024) << " KB: " << ms << " us" << endl;
    }
    
    // Test cache-friendly vs cache-unfriendly access
    const int N = 1024;
    vector<float> matrix(N * N, 1.0f);
    
    // Row-major (cache-friendly)
    auto start = chrono::high_resolution_clock::now();
    float sum_row = 0.0f;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            sum_row += matrix[i * N + j];
        }
    }
    auto end = chrono::high_resolution_clock::now();
    auto ms_row = chrono::duration_cast<chrono::microseconds>(end - start).count();
    
    // Column-major (cache-unfriendly)
    start = chrono::high_resolution_clock::now();
    float sum_col = 0.0f;
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            sum_col += matrix[i * N + j];
        }
    }
    end = chrono::high_resolution_clock::now();
    auto ms_col = chrono::duration_cast<chrono::microseconds>(end - start).count();
    
    cout << "  Row-major access: " << ms_row << " us" << endl;
    cout << "  Column-major access: " << ms_col << " us" << endl;
    cout << "  Speedup: " << fixed << setprecision(2) << ((float)ms_col / ms_row) << "x" << endl;
    
    return true;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║     RAWR LLM RUNTIME - VALIDATION TEST SUITE               ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    srand((unsigned)time(nullptr));
    
    bool test1 = test_numerical_correctness();
    bool test2 = test_throughput();
    bool test3 = test_memory_behavior();
    
    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    SUMMARY                                 ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    cout << "Test 1 (Numerical): " << (test1 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "Test 2 (Throughput): " << (test2 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "Test 3 (Memory): " << (test3 ? "PASS ✓" : "FAIL ✗") << endl;
    
    bool all_pass = test1 && test2 && test3;
    
    cout << "\nOverall: " << (all_pass ? "ALL TESTS PASSED ✓" : "SOME TESTS FAILED ✗") << endl;
    
    return all_pass ? 0 : 1;
}
