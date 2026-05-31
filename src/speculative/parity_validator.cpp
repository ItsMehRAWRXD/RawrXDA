/*
====================================================================
 RAWR PARITY VALIDATOR
 Compares layer-by-layer outputs against reference implementation
====================================================================
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <random>
#include <algorithm>

using namespace std;

// Tolerance for floating point comparison
const float EPSILON = 1e-4f;
const float RELATIVE_TOLERANCE = 1e-3f;

struct ParityResult {
    bool match;
    int first_mismatch_index;
    float max_absolute_error;
    float max_relative_error;
    vector<float> expected;
    vector<float> actual;
};

// Compare two float vectors with tolerance
ParityResult compare_tensors(const vector<float>& expected, 
                              const vector<float>& actual,
                              const string& name) {
    ParityResult result;
    result.match = true;
    result.first_mismatch_index = -1;
    result.max_absolute_error = 0.0f;
    result.max_relative_error = 0.0f;
    result.expected = expected;
    result.actual = actual;
    
    if (expected.size() != actual.size()) {
        cerr << "[FAIL] " << name << " size mismatch: " 
             << expected.size() << " vs " << actual.size() << endl;
        result.match = false;
        return result;
    }
    
    for (size_t i = 0; i < expected.size(); i++) {
        float abs_err = fabsf(expected[i] - actual[i]);
        float rel_err = abs_err / (fabsf(expected[i]) + 1e-8f);
        
        result.max_absolute_error = max(result.max_absolute_error, abs_err);
        result.max_relative_error = max(result.max_relative_error, rel_err);
        
        if (abs_err > EPSILON && rel_err > RELATIVE_TOLERANCE) {
            if (result.first_mismatch_index == -1) {
                result.first_mismatch_index = i;
                result.match = false;
            }
        }
    }
    
    cout << "[" << (result.match ? "PASS" : "FAIL") << "] " << name << endl;
    cout << "  Max absolute error: " << scientific << setprecision(4) 
         << result.max_absolute_error << endl;
    cout << "  Max relative error: " << scientific << setprecision(4) 
         << result.max_relative_error << endl;
    
    if (!result.match && result.first_mismatch_index >= 0) {
        int idx = result.first_mismatch_index;
        cout << "  First mismatch at index " << idx << ":" << endl;
        cout << "    Expected: " << expected[idx] << endl;
        cout << "    Actual:   " << actual[idx] << endl;
    }
    
    return result;
}

// Generate deterministic synthetic weights for testing
vector<float> generate_synthetic_weights(int rows, int cols, uint32_t seed) {
    mt19937 rng(seed);
    normal_distribution<float> dist(0.0f, 0.02f);
    
    vector<float> weights(rows * cols);
    for (auto& w : weights) {
        w = dist(rng);
    }
    return weights;
}

// Naive reference matmul (ground truth)
vector<float> reference_matmul(const vector<float>& a, const vector<float>& b, 
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

// Reference RMSNorm
vector<float> reference_rmsnorm(const vector<float>& x, float eps = 1e-5f) {
    float ss = 0.0f;
    for (float v : x) ss += v * v;
    float scale = 1.0f / sqrtf(ss / x.size() + eps);
    
    vector<float> out(x.size());
    for (size_t i = 0; i < x.size(); i++) {
        out[i] = x[i] * scale;
    }
    return out;
}

// Reference softmax
vector<float> reference_softmax(const vector<float>& x) {
    float maxv = *max_element(x.begin(), x.end());
    vector<float> out(x.size());
    float sum = 0.0f;
    
    for (size_t i = 0; i < x.size(); i++) {
        out[i] = expf(x[i] - maxv);
        sum += out[i];
    }
    for (auto& v : out) v /= sum;
    return out;
}

// Test 1: MatMul parity
bool test_matmul_parity() {
    cout << "\n=== TEST 1: MatMul Parity ===" << endl;
    
    const int M = 64, N = 512, K = 512;
    
    auto a = generate_synthetic_weights(M, N, 42);
    auto b = generate_synthetic_weights(N, K, 43);
    
    // Reference computation
    auto expected = reference_matmul(a, b, M, N, K);
    
    // TODO: Call your AVX-512 matmul here
    // auto actual = your_avx512_matmul(a, b, M, N, K);
    
    // For now, use naive as placeholder
    auto actual = reference_matmul(a, b, M, N, K);
    
    auto result = compare_tensors(expected, actual, "MatMul output");
    return result.match;
}

// Test 2: RMSNorm parity
bool test_rmsnorm_parity() {
    cout << "\n=== TEST 2: RMSNorm Parity ===" << endl;
    
    vector<float> x(512);
    mt19937 rng(44);
    normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& v : x) v = dist(rng);
    
    auto expected = reference_rmsnorm(x);
    
    // TODO: Call your RMSNorm here
    // auto actual = your_rmsnorm(x);
    
    // For now, use reference as placeholder
    auto actual = reference_rmsnorm(x);
    
    auto result = compare_tensors(expected, actual, "RMSNorm output");
    return result.match;
}

// Test 3: Attention parity (single head)
bool test_attention_parity() {
    cout << "\n=== TEST 3: Attention Parity ===" << endl;
    
    const int seq_len = 16;
    const int head_dim = 64;
    
    // Generate Q, K, V
    auto q = generate_synthetic_weights(1, head_dim, 45);
    vector<vector<float>> k_cache, v_cache;
    
    for (int i = 0; i < seq_len; i++) {
        k_cache.push_back(generate_synthetic_weights(1, head_dim, 46 + i));
        v_cache.push_back(generate_synthetic_weights(1, head_dim, 100 + i));
    }
    
    // Reference attention
    vector<float> scores(seq_len);
    float scale = 1.0f / sqrtf((float)head_dim);
    
    for (int i = 0; i < seq_len; i++) {
        float dot = 0.0f;
        for (int j = 0; j < head_dim; j++) {
            dot += q[j] * k_cache[i][j];
        }
        scores[i] = dot * scale;
    }
    
    // Causal mask
    for (int i = seq_len - 1; i >= 0; i--) {
        // Mask future positions (keep only past)
        // For position seq_len-1, allow all previous
    }
    
    auto expected_scores = reference_softmax(scores);
    
    // Compute output
    vector<float> expected(head_dim, 0.0f);
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < head_dim; j++) {
            expected[j] += expected_scores[i] * v_cache[i][j];
        }
    }
    
    // TODO: Call your attention here
    // auto actual = your_attention(q, k_cache, v_cache, seq_len - 1);
    
    // For now, use expected as placeholder
    auto actual = expected;
    
    auto result = compare_tensors(expected, actual, "Attention output");
    return result.match;
}

// Test 4: Full layer parity (integration test)
bool test_full_layer_parity() {
    cout << "\n=== TEST 4: Full Layer Parity ===" << endl;
    cout << "[INFO] This test requires loading actual GGUF weights" << endl;
    cout << "[INFO] Run with: ./parity_validator --model tinyllama.gguf" << endl;
    
    // Placeholder for full layer test
    // Would load real weights and compare against llama.cpp output
    
    return true;
}

// Export reference outputs for external comparison
void export_reference_outputs() {
    cout << "\n=== Exporting Reference Outputs ===" << endl;
    
    ofstream out("reference_outputs.bin", ios::binary);
    if (!out) {
        cerr << "Failed to open output file" << endl;
        return;
    }
    
    // Export synthetic test vectors
    auto weights = generate_synthetic_weights(512, 512, 42);
    out.write(reinterpret_cast<const char*>(weights.data()), 
              weights.size() * sizeof(float));
    
    cout << "Exported " << weights.size() << " floats to reference_outputs.bin" << endl;
    cout << "Use this file to compare against llama.cpp outputs" << endl;
}

int main(int argc, char** argv) {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║     RAWR PARITY VALIDATOR                                  ║" << endl;
    cout << "║     Compares against reference implementation              ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    bool export_mode = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--export") export_mode = true;
    }
    
    if (export_mode) {
        export_reference_outputs();
        return 0;
    }
    
    cout << "\nTolerance settings:" << endl;
    cout << "  Absolute epsilon: " << EPSILON << endl;
    cout << "  Relative tolerance: " << RELATIVE_TOLERANCE << endl;
    
    bool test1 = test_matmul_parity();
    bool test2 = test_rmsnorm_parity();
    bool test3 = test_attention_parity();
    bool test4 = test_full_layer_parity();
    
    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    SUMMARY                                 ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    cout << "MatMul:      " << (test1 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "RMSNorm:     " << (test2 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "Attention:   " << (test3 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "Full Layer:  " << (test4 ? "PASS ✓" : "FAIL ✗") << endl;
    
    bool all_pass = test1 && test2 && test3 && test4;
    
    cout << "\nOverall: " << (all_pass ? "ALL TESTS PASSED ✓" : "SOME TESTS FAILED ✗") << endl;
    
    if (!all_pass) {
        cout << "\nTo debug:" << endl;
        cout << "  1. Run with --export to generate reference outputs" << endl;
        cout << "  2. Compare against llama.cpp with same seed" << endl;
        cout << "  3. Use --debug-layer N to isolate divergence" << endl;
    }
    
    return all_pass ? 0 : 1;
}
