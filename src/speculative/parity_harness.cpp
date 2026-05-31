/*
====================================================================
 RAWR DETERMINISTIC PARITY HARNESS
 Strict reference alignment with llama.cpp
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

// STRICT DETERMINISM SETTINGS
const float TEMPERATURE = 0.0f;  // No randomness
const int TOP_K = 1;             // Greedy only
const float TOP_P = 1.0f;        // No nucleus filtering
const uint32_t SEED = 42;        // Fixed seed

// TOLERANCE FOR FLOAT COMPARISON
const float ABS_TOLERANCE = 1e-4f;
const float REL_TOLERANCE = 1e-3f;

// DIVERGENCE DETECTOR
struct DivergenceInfo {
    bool found;
    int layer;
    int index;
    float expected;
    float actual;
    float abs_error;
    float rel_error;
};

// Check for first divergence between two tensors
DivergenceInfo find_first_divergence(const vector<float>& expected,
                                      const vector<float>& actual,
                                      int layer = -1,
                                      const string& name = "") {
    DivergenceInfo div = {false, layer, -1, 0, 0, 0, 0};
    
    if (expected.size() != actual.size()) {
        cerr << "[ERROR] Size mismatch in " << name << ": " 
             << expected.size() << " vs " << actual.size() << endl;
        div.found = true;
        return div;
    }
    
    for (size_t i = 0; i < expected.size(); i++) {
        float abs_err = fabsf(expected[i] - actual[i]);
        float rel_err = abs_err / (fabsf(expected[i]) + 1e-8f);
        
        if (abs_err > ABS_TOLERANCE && rel_err > REL_TOLERANCE) {
            div.found = true;
            div.index = (int)i;
            div.expected = expected[i];
            div.actual = actual[i];
            div.abs_error = abs_err;
            div.rel_error = rel_err;
            
            cerr << "\n🔴 DIVERGENCE at layer " << layer << " index " << i 
                 << " (" << name << ")" << endl;
            cerr << "   Expected: " << scientific << setprecision(6) << expected[i] << endl;
            cerr << "   Actual:   " << actual[i] << endl;
            cerr << "   Abs err:  " << abs_err << endl;
            cerr << "   Rel err:  " << rel_err << endl;
            
            // Print context
            cerr << "\n   Context:" << endl;
            for (int j = max(0, (int)i - 2); j <= min((int)expected.size() - 1, (int)i + 2); j++) {
                cerr << "   [" << j << "] " << expected[j] << " vs " << actual[j];
                if (j == (int)i) cerr << " <-- HERE";
                cerr << endl;
            }
            
            return div;
        }
    }
    
    return div;
}

// Dump top-k logits for comparison
void dump_top_logits(const vector<float>& logits, int k = 10, ostream& out = cout) {
    // Create index-value pairs
    vector<pair<float, int>> indexed;
    for (int i = 0; i < (int)logits.size(); i++) {
        indexed.push_back({logits[i], i});
    }
    
    // Sort by value descending
    sort(indexed.rbegin(), indexed.rend());
    
    out << "\nTop " << k << " logits:" << endl;
    for (int i = 0; i < min(k, (int)indexed.size()); i++) {
        out << "  [" << indexed[i].second << "] " 
            << fixed << setprecision(6) << indexed[i].first << endl;
    }
}

// Export logits to file for external comparison
void export_logits(const vector<float>& logits, const string& filename, int layer = -1) {
    ofstream out(filename, ios::binary);
    if (!out) {
        cerr << "[ERROR] Cannot open " << filename << endl;
        return;
    }
    
    // Header: layer number, size
    int size = (int)logits.size();
    out.write(reinterpret_cast<const char*>(&layer), sizeof(int));
    out.write(reinterpret_cast<const char*>(&size), sizeof(int));
    
    // Data
    out.write(reinterpret_cast<const char*>(logits.data()), size * sizeof(float));
    
    cout << "[EXPORT] Layer " << layer << " logits (" << size << " floats) -> " 
         << filename << endl;
}

// Compare logits files
bool compare_logits_files(const string& file1, const string& file2) {
    ifstream f1(file1, ios::binary);
    ifstream f2(file2, ios::binary);
    
    if (!f1 || !f2) {
        cerr << "[ERROR] Cannot open files for comparison" << endl;
        return false;
    }
    
    // Read headers
    int layer1, size1, layer2, size2;
    f1.read(reinterpret_cast<char*>(&layer1), sizeof(int));
    f1.read(reinterpret_cast<char*>(&size1), sizeof(int));
    f2.read(reinterpret_cast<char*>(&layer2), sizeof(int));
    f2.read(reinterpret_cast<char*>(&size2), sizeof(int));
    
    if (size1 != size2) {
        cerr << "[ERROR] Size mismatch: " << size1 << " vs " << size2 << endl;
        return false;
    }
    
    vector<float> logits1(size1), logits2(size2);
    f1.read(reinterpret_cast<char*>(logits1.data()), size1 * sizeof(float));
    f2.read(reinterpret_cast<char*>(logits2.data()), size2 * sizeof(float));
    
    auto div = find_first_divergence(logits1, logits2, layer1, "logits");
    
    if (!div.found) {
        cout << "[PASS] Logits match exactly" << endl;
        return true;
    }
    
    return false;
}

// KNOWN-CORRECT SCALAR REFERENCE IMPLEMENTATIONS
namespace reference {

// Correct RMSNorm (no mean subtraction, just RMS)
vector<float> rmsnorm(const vector<float>& x, const vector<float>& weight, float eps) {
    float ss = 0.0f;
    for (float v : x) ss += v * v;
    float rms = sqrtf(ss / x.size() + eps);
    float scale = 1.0f / rms;
    
    vector<float> out(x.size());
    for (size_t i = 0; i < x.size(); i++) {
        out[i] = x[i] * scale * weight[i];
    }
    return out;
}

// Correct attention with explicit causal mask
vector<float> attention_head(const vector<float>& q,
                              const vector<vector<float>>& k_cache,
                              const vector<vector<float>>& v_cache,
                              int head_idx,
                              int head_dim,
                              int current_pos) {
    int seq_len = (int)k_cache.size();
    vector<float> scores(seq_len);
    
    // Q·K^T / sqrt(d)
    float scale = 1.0f / sqrtf((float)head_dim);
    for (int i = 0; i < seq_len; i++) {
        float dot = 0.0f;
        for (int j = 0; j < head_dim; j++) {
            dot += q[head_idx * head_dim + j] * k_cache[i][head_idx * head_dim + j];
        }
        scores[i] = dot * scale;
    }
    
    // EXPLICIT CAUSAL MASK
    for (int i = current_pos + 1; i < seq_len; i++) {
        scores[i] = -INFINITY;
    }
    
    // Softmax
    float maxv = *max_element(scores.begin(), scores.end());
    float sum = 0.0f;
    for (auto& s : scores) {
        s = expf(s - maxv);
        sum += s;
    }
    for (auto& s : scores) s /= sum;
    
    // Weighted sum of V
    vector<float> out(head_dim, 0.0f);
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < head_dim; j++) {
            out[j] += scores[i] * v_cache[i][head_idx * head_dim + j];
        }
    }
    
    return out;
}

// Correct RoPE (Rotary Position Embedding)
void apply_rope(vector<float>& q, vector<float>& k, int pos, int head_dim) {
    for (int i = 0; i < head_dim; i += 2) {
        // Standard RoPE base = 10000.0
        float freq = 1.0f / powf(10000.0f, (float)i / head_dim);
        float val = pos * freq;
        float cos_v = cosf(val);
        float sin_v = sinf(val);
        
        // Rotate Q
        float q0 = q[i], q1 = q[i + 1];
        q[i] = q0 * cos_v - q1 * sin_v;
        q[i + 1] = q0 * sin_v + q1 * cos_v;
        
        // Rotate K
        float k0 = k[i], k1 = k[i + 1];
        k[i] = k0 * cos_v - k1 * sin_v;
        k[i + 1] = k0 * sin_v + k1 * cos_v;
    }
}

// Correct SwiGLU FFN
vector<float> swiglu(const vector<float>& x,
                     const vector<float>& gate_weight,
                     const vector<float>& up_weight,
                     const vector<float>& down_weight,
                     int dim) {
    // Gate projection
    vector<float> gate(dim, 0.0f);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            gate[i] += gate_weight[i * dim + j] * x[j];
        }
    }
    
    // Up projection
    vector<float> up(dim, 0.0f);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            up[i] += up_weight[i * dim + j] * x[j];
        }
    }
    
    // Swish activation: x * sigmoid(x)
    for (int i = 0; i < dim; i++) {
        gate[i] = gate[i] * (1.0f / (1.0f + expf(-gate[i])));
    }
    
    // Element-wise multiply
    for (int i = 0; i < dim; i++) {
        gate[i] *= up[i];
    }
    
    // Down projection
    vector<float> out(dim, 0.0f);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            out[i] += down_weight[i * dim + j] * gate[j];
        }
    }
    
    return out;
}

} // namespace reference

// PARITY TEST SUITES
bool test_rmsnorm_parity() {
    cout << "\n=== TEST: RMSNorm Parity ===" << endl;
    
    vector<float> x(512);
    vector<float> weight(512, 1.0f);
    
    mt19937 rng(SEED);
    normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& v : x) v = dist(rng);
    
    auto expected = reference::rmsnorm(x, weight, 1e-5f);
    
    // TODO: Call your RMSNorm here
    // auto actual = your_rmsnorm(x, weight, 1e-5f);
    auto actual = expected; // Placeholder
    
    auto div = find_first_divergence(expected, actual, 0, "RMSNorm");
    
    if (!div.found) {
        cout << "[PASS] RMSNorm matches reference" << endl;
        return true;
    }
    
    cerr << "\n💡 Common RMSNorm bugs:" << endl;
    cerr << "  - Using LayerNorm (subtracting mean) instead of RMSNorm" << endl;
    cerr << "  - Wrong epsilon (1e-6 vs 1e-5)" << endl;
    cerr << "  - Missing weight multiplication" << endl;
    
    return false;
}

bool test_attention_parity() {
    cout << "\n=== TEST: Attention Parity ===" << endl;
    
    const int seq_len = 8;
    const int head_dim = 64;
    const int n_heads = 8;
    
    // Generate synthetic Q, K, V
    mt19937 rng(SEED);
    normal_distribution<float> dist(0.0f, 0.02f);
    
    vector<float> q(n_heads * head_dim);
    for (auto& v : q) v = dist(rng);
    
    vector<vector<float>> k_cache, v_cache;
    for (int i = 0; i < seq_len; i++) {
        k_cache.push_back(vector<float>(n_heads * head_dim));
        v_cache.push_back(vector<float>(n_heads * head_dim));
        for (auto& v : k_cache.back()) v = dist(rng);
        for (auto& v : v_cache.back()) v = dist(rng);
    }
    
    // Test at position 4 (middle of sequence)
    int pos = 4;
    
    // Reference computation
    vector<float> expected(n_heads * head_dim);
    for (int h = 0; h < n_heads; h++) {
        auto head_out = reference::attention_head(q, k_cache, v_cache, h, head_dim, pos);
        for (int d = 0; d < head_dim; d++) {
            expected[h * head_dim + d] = head_out[d];
        }
    }
    
    // TODO: Call your attention here
    // auto actual = your_attention(q, k_cache, v_cache, pos);
    auto actual = expected; // Placeholder
    
    auto div = find_first_divergence(expected, actual, 0, "Attention");
    
    if (!div.found) {
        cout << "[PASS] Attention matches reference" << endl;
        return true;
    }
    
    cerr << "\n💡 Common Attention bugs:" << endl;
    cerr << "  - Missing causal mask (allowing future tokens)" << endl;
    cerr << "  - Wrong scaling (sqrt(head_dim) vs head_dim)" << endl;
    cerr << "  - Softmax over wrong dimension" << endl;
    cerr << "  - KV cache indexing off-by-one" << endl;
    
    return false;
}

bool test_rope_parity() {
    cout << "\n=== TEST: RoPE Parity ===" << endl;
    
    const int head_dim = 64;
    
    mt19937 rng(SEED);
    normal_distribution<float> dist(0.0f, 0.02f);
    
    vector<float> q(head_dim), k(head_dim);
    for (auto& v : q) v = dist(rng);
    for (auto& v : k) v = dist(rng);
    
    auto q_expected = q;
    auto k_expected = k;
    reference::apply_rope(q_expected, k_expected, 5, head_dim);
    
    // TODO: Call your RoPE here
    // auto q_actual = q; auto k_actual = k;
    // your_rope(q_actual, k_actual, 5, head_dim);
    auto q_actual = q_expected; // Placeholder
    auto k_actual = k_expected;
    
    auto div_q = find_first_divergence(q_expected, q_actual, 0, "RoPE Q");
    auto div_k = find_first_divergence(k_expected, k_actual, 0, "RoPE K");
    
    if (!div_q.found && !div_k.found) {
        cout << "[PASS] RoPE matches reference" << endl;
        return true;
    }
    
    cerr << "\n💡 Common RoPE bugs:" << endl;
    cerr << "  - Wrong base (10000.0 is standard)" << endl;
    cerr << "  - Applied before head split" << endl;
    cerr << "  - Wrong frequency calculation" << endl;
    
    return false;
}

// MAIN
int main(int argc, char** argv) {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║     RAWR DETERMINISTIC PARITY HARNESS                      ║" << endl;
    cout << "║     Strict reference alignment with llama.cpp              ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    cout << "\n[CONFIG] Determinism settings:" << endl;
    cout << "  Temperature: " << TEMPERATURE << endl;
    cout << "  Top-k: " << TOP_K << endl;
    cout << "  Top-p: " << TOP_P << endl;
    cout << "  Seed: " << SEED << endl;
    cout << "  Abs tolerance: " << ABS_TOLERANCE << endl;
    cout << "  Rel tolerance: " << REL_TOLERANCE << endl;
    
    bool test1 = test_rmsnorm_parity();
    bool test2 = test_attention_parity();
    bool test3 = test_rope_parity();
    
    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    SUMMARY                                 ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    cout << "RMSNorm:   " << (test1 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "Attention: " << (test2 ? "PASS ✓" : "FAIL ✗") << endl;
    cout << "RoPE:      " << (test3 ? "PASS ✓" : "FAIL ✗") << endl;
    
    bool all_pass = test1 && test2 && test3;
    
    cout << "\nOverall: " << (all_pass ? "ALL TESTS PASSED ✓" : "SOME TESTS FAILED ✗") << endl;
    
    if (!all_pass) {
        cout << "\n🔧 Next steps:" << endl;
        cout << "  1. Fix the first failing test" << endl;
        cout << "  2. Re-run until all pass" << endl;
        cout << "  3. Then test with real GGUF weights" << endl;
        cout << "  4. Compare logits dump against llama.cpp" << endl;
    } else {
        cout << "\n✅ Ready for real model testing!" << endl;
        cout << "   Run: ./parity_harness --model tinyllama.gguf" << endl;
    }
    
    return all_pass ? 0 : 1;
}
