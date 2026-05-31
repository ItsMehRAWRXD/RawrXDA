/*
====================================================================
 RAWR END-TO-END PARITY TEST
 Compares full model output against llama.cpp reference
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
#include <cassert>

using namespace std;

// DETERMINISTIC SETTINGS (MUST MATCH LLAMA.CPP)
const float TEMPERATURE = 0.0f;
const int TOP_K = 1;
const float TOP_P = 1.0f;
const uint32_t SEED = 42;

// TOLERANCE
const float LOGIT_TOLERANCE = 1e-4f;
const float REL_TOLERANCE = 1e-3f;

// Token comparison result
struct TokenComparison {
    int token_idx;
    int expected_token;
    int actual_token;
    float max_logit_error;
    bool match;
};

// Load reference logits from file
vector<vector<float>> load_reference_logits(const string& filename) {
    vector<vector<float>> all_logits;
    ifstream in(filename, ios::binary);
    
    if (!in) {
        cerr << "[ERROR] Cannot open reference file: " << filename << endl;
        return all_logits;
    }
    
    while (in.peek() != EOF) {
        int layer, size;
        in.read(reinterpret_cast<char*>(&layer), sizeof(int));
        in.read(reinterpret_cast<char*>(&size), sizeof(int));
        
        if (!in) break;
        
        vector<float> logits(size);
        in.read(reinterpret_cast<char*>(logits.data()), size * sizeof(float));
        all_logits.push_back(logits);
    }
    
    return all_logits;
}

// Export logits for comparison
void export_logits(const vector<float>& logits, const string& filename, int token_idx) {
    ofstream out(filename, ios::binary | ios::app);
    if (!out) {
        cerr << "[ERROR] Cannot open " << filename << endl;
        return;
    }
    
    int size = (int)logits.size();
    out.write(reinterpret_cast<const char*>(&token_idx), sizeof(int));
    out.write(reinterpret_cast<const char*>(&size), sizeof(int));
    out.write(reinterpret_cast<const char*>(logits.data()), size * sizeof(float));
}

// Compare two logit vectors
TokenComparison compare_logits(const vector<float>& expected,
                                const vector<float>& actual,
                                int token_idx) {
    TokenComparison result;
    result.token_idx = token_idx;
    result.max_logit_error = 0.0f;
    result.match = true;
    
    if (expected.size() != actual.size()) {
        cerr << "[ERROR] Size mismatch: " << expected.size() << " vs " << actual.size() << endl;
        result.match = false;
        return result;
    }
    
    // Find max error
    for (size_t i = 0; i < expected.size(); i++) {
        float err = fabsf(expected[i] - actual[i]);
        result.max_logit_error = max(result.max_logit_error, err);
    }
    
    // Find top token for each
    result.expected_token = max_element(expected.begin(), expected.end()) - expected.begin();
    result.actual_token = max_element(actual.begin(), actual.end()) - actual.begin();
    
    // Check if tokens match (greedy sampling)
    if (result.expected_token != result.actual_token) {
        result.match = false;
    }
    
    // Also check if logits are close enough
    if (result.max_logit_error > LOGIT_TOLERANCE) {
        result.match = false;
    }
    
    return result;
}

// Print comparison results
void print_comparison(const TokenComparison& comp) {
    cout << "\nToken " << comp.token_idx << ": ";
    
    if (comp.match) {
        cout << "✅ MATCH" << endl;
        cout << "  Token: " << comp.actual_token << endl;
        cout << "  Max logit error: " << scientific << setprecision(4) 
             << comp.max_logit_error << endl;
    } else {
        cout << "❌ MISMATCH" << endl;
        cout << "  Expected token: " << comp.expected_token << endl;
        cout << "  Actual token:   " << comp.actual_token << endl;
        cout << "  Max logit error: " << comp.max_logit_error << endl;
        
        // Show top 5 logits from each
        vector<pair<float, int>> expected_top, actual_top;
        // (Would need original logits to show this)
    }
}

// Main parity test
int main(int argc, char** argv) {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║     RAWR END-TO-END PARITY TEST                            ║" << endl;
    cout << "║     Compares against llama.cpp reference                   ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    if (argc < 3) {
        cerr << "\nUsage: " << argv[0] << " <reference_logits.bin> <rawr_logits.bin>" << endl;
        cerr << "\nGenerate reference with llama.cpp:" << endl;
        cerr << "  ./llama-cli -m model.gguf -p \"Hello\" -n 10 --logits-all ref.bin" << endl;
        cerr << "\nGenerate with RawrXD:" << endl;
        cerr << "  ./rawr_monolith_v2 model.gguf \"Hello\" 10" << endl;
        return 1;
    }
    
    string ref_file = argv[1];
    string rawr_file = argv[2];
    
    cout << "\n[CONFIG]" << endl;
    cout << "  Reference: " << ref_file << endl;
    cout << "  RawrXD:    " << rawr_file << endl;
    cout << "  Tolerance: " << LOGIT_TOLERANCE << endl;
    
    // Load both outputs
    auto ref_logits = load_reference_logits(ref_file);
    auto rawr_logits = load_reference_logits(rawr_file);
    
    if (ref_logits.empty() || rawr_logits.empty()) {
        cerr << "[ERROR] Failed to load logits" << endl;
        return 1;
    }
    
    if (ref_logits.size() != rawr_logits.size()) {
        cerr << "[ERROR] Token count mismatch: " << ref_logits.size() 
             << " vs " << rawr_logits.size() << endl;
        return 1;
    }
    
    cout << "  Tokens:    " << ref_logits.size() << endl;
    
    // Compare each token
    int pass_count = 0;
    int fail_count = 0;
    int first_fail = -1;
    
    for (size_t i = 0; i < ref_logits.size(); i++) {
        auto comp = compare_logits(ref_logits[i], rawr_logits[i], (int)i);
        print_comparison(comp);
        
        if (comp.match) {
            pass_count++;
        } else {
            fail_count++;
            if (first_fail == -1) first_fail = (int)i;
        }
    }
    
    // Summary
    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    SUMMARY                                 ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    cout << "Tokens checked: " << ref_logits.size() << endl;
    cout << "Passed:         " << pass_count << endl;
    cout << "Failed:         " << fail_count << endl;
    
    if (fail_count == 0) {
        cout << "\n✅ FULL PARITY ACHIEVED" << endl;
        cout << "   All tokens match reference exactly." << endl;
        return 0;
    } else {
        cout << "\n❌ PARITY FAILED" << endl;
        cout << "   First failure at token " << first_fail << endl;
        cout << "\n🔍 Debug with:" << endl;
        cout << "   --debug-layer 0" << endl;
        cout << "   Check: RMSNorm epsilon, attention scaling, RoPE base" << endl;
        return 1;
    }
}
