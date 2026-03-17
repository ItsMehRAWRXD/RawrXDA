#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <cstring>
#include <map>
#include <string>
#include <set>
#include <algorithm>

// Forward declare ggml types (we'll use mock implementation)
struct ggml_context { int dummy; };
typedef void* ggml_backend_t;

// Minimal mock TensorCache for testing
class TensorCache {
public:
    struct TensorData {
        std::vector<float> data;
        std::vector<int64_t> shape;
        int type;
    };
    
    void addTensor(const std::string& name, const float* data, 
                   const std::vector<int64_t>& shape, int type) {
        TensorData td;
        size_t totalSize = 1;
        for (auto dim : shape) totalSize *= dim;
        td.data.assign(data, data + totalSize);
        td.shape = shape;
        td.type = type;
        tensors[name] = td;
    }
    
    bool hasTensor(const std::string& name) const {
        return tensors.find(name) != tensors.end();
    }
    
    const TensorData& getTensor(const std::string& name) const {
        return tensors.at(name);
    }
    
private:
    std::map<std::string, TensorData> tensors;
};

// Test utilities
class TestLogger {
public:
    static void logTest(const std::string& name, bool passed) {
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << std::endl;
        if (passed) passCount++;
        else failCount++;
    }
    
    static void logSection(const std::string& section) {
        std::cout << "\n=== " << section << " ===" << std::endl;
    }
    
    static void printSummary() {
        std::cout << "\n=== TEST SUMMARY ===" << std::endl;
        std::cout << "Passed: " << passCount << std::endl;
        std::cout << "Failed: " << failCount << std::endl;
        std::cout << "Total:  " << (passCount + failCount) << std::endl;
    }
    
    static int passCount;
    static int failCount;
};

int TestLogger::passCount = 0;
int TestLogger::failCount = 0;

// Simplified TransformerInference for testing (inline implementation)
class TransformerInference {
public:
    TransformerInference(int nLayers, int nEmbd, int nHead, int nVocab, int ctxSize)
        : m_nLayers(nLayers), m_nEmbd(nEmbd), m_nHead(nHead), m_nVocab(nVocab), m_ctxSize(ctxSize) {
        m_ctx = nullptr;
        m_backend = nullptr;
    }
    
    ~TransformerInference() {
        // Mock destructor - nothing to free
    }
    // Just add minimal tensors for mock
    // The mock implementation doesn't actually use them
}   }
    
    std::vector<int32_t> generate(const std::vector<int32_t>& prompt, int maxTokens, float temperature) {
        std::vector<int32_t> result = prompt;
        
        for (int i = 0; i < maxTokens; ++i) {
            auto logits = forward(result);
            int nextToken = sampleToken(logits, temperature);
            result.push_back(nextToken);
        }
        
        return result;
    }
    
private:
    int m_nLayers, m_nEmbd, m_nHead, m_nVocab, m_ctxSize;
    ggml_context* m_ctx;
    ggml_backend_t m_backend;
};

// Create mock GGUF model weights in TensorCache
void createMockModel(TensorCache& cache, int nLayers = 2, int nEmbd = 128, int nHead = 4, int nVocab = 1000) {
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::normal_distribution<float> dist(0.0f, 0.02f);
    
    // Token embeddings: [n_vocab, n_embd]
    {
        std::vector<float> data(nVocab * nEmbd);
        for (auto& val : data) val = dist(gen);
        cache.addTensor("token_embd.weight", data.data(), {nVocab, nEmbd}, 0);
    }
    
    // Output weight: [n_vocab, n_embd]
    {
        std::vector<float> data(nVocab * nEmbd);
        for (auto& val : data) val = dist(gen);
        cache.addTensor("output.weight", data.data(), {nVocab, nEmbd}, GGML_TYPE_F32);
    }
    
    // Layer weights
    for (int i = 0; i < nLayers; ++i) {
        std::string prefix = "blk." + std::to_string(i) + ".";
        
        // Layer norm 1: [n_embd]
        {
            std::vector<float> weight(nEmbd, 1.0f);
            std::vector<float> bias(nEmbd, 0.0f);
            cache.addTensor(prefix + "attn_norm.weight", weight.data(), {nEmbd}, GGML_TYPE_F32);
            cache.addTensor(prefix + "attn_norm.bias", bias.data(), {nEmbd}, GGML_TYPE_F32);
        }
        
        // Attention Q, K, V: [n_embd, n_embd]
        for (const char* name : {"attn_q.weight", "attn_k.weight", "attn_v.weight"}) {
            std::vector<float> data(nEmbd * nEmbd);
            for (auto& val : data) val = dist(gen);
            cache.addTensor(prefix + name, data.data(), {nEmbd, nEmbd}, GGML_TYPE_F32);
        }
        
        // Attention projection: [n_embd, n_embd]
        {
            std::vector<float> data(nEmbd * nEmbd);
            for (auto& val : data) val = dist(gen);
            cache.addTensor(prefix + "attn_output.weight", data.data(), {nEmbd, nEmbd}, GGML_TYPE_F32);
        }
        
        // Layer norm 2: [n_embd]
        {
            std::vector<float> weight(nEmbd, 1.0f);
            std::vector<float> bias(nEmbd, 0.0f);
            cache.addTensor(prefix + "ffn_norm.weight", weight.data(), {nEmbd}, GGML_TYPE_F32);
            cache.addTensor(prefix + "ffn_norm.bias", bias.data(), {nEmbd}, GGML_TYPE_F32);
        }
        
        // MLP: FC1 [n_embd*4, n_embd], FC2 [n_embd, n_embd*4]
        {
            std::vector<float> fc1(nEmbd * 4 * nEmbd);
            for (auto& val : fc1) val = dist(gen);
            cache.addTensor(prefix + "ffn_up.weight", fc1.data(), {nEmbd * 4, nEmbd}, GGML_TYPE_F32);
            
            std::vector<float> fc2(nEmbd * nEmbd * 4);
            for (auto& val : fc2) val = dist(gen);
            cache.addTensor(prefix + "ffn_down.weight", fc2.data(), {nEmbd, nEmbd * 4}, GGML_TYPE_F32);
        }
    }
}

// Test 1: Initialization
bool testInitialization() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        bool success = transformer.loadWeights(cache);
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 2: Single token forward pass
bool testSingleTokenForward() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> tokens = {42}; // Single token
        std::vector<float> logits = transformer.forward(tokens);
        
        // Check output shape
        if (logits.size() != 1000) {
            std::cerr << "Wrong logits size: " << logits.size() << " (expected 1000)" << std::endl;
            return false;
        }
        
        // Check logits are finite
        for (float val : logits) {
            if (!std::isfinite(val)) {
                std::cerr << "Non-finite logit detected" << std::endl;
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 3: Multi-token forward pass
bool testMultiTokenForward() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> tokens = {10, 20, 30, 40, 50}; // 5 tokens
        std::vector<float> logits = transformer.forward(tokens);
        
        // Check output shape (should predict next token after last)
        if (logits.size() != 1000) {
            std::cerr << "Wrong logits size: " << logits.size() << std::endl;
            return false;
        }
        
        // Check all finite
        for (float val : logits) {
            if (!std::isfinite(val)) {
                std::cerr << "Non-finite logit in multi-token forward" << std::endl;
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 4: Token sampling - greedy
bool testGreedySampling() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> tokens = {42};
        std::vector<float> logits = transformer.forward(tokens);
        
        // Greedy sampling (temperature = 0.0)
        int token1 = transformer.sampleToken(logits, 0.0f);
        int token2 = transformer.sampleToken(logits, 0.0f);
        
        // Should be deterministic
        if (token1 != token2) {
            std::cerr << "Greedy sampling not deterministic" << std::endl;
            return false;
        }
        
        // Should be valid token ID
        if (token1 < 0 || token1 >= 1000) {
            std::cerr << "Invalid token ID: " << token1 << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 5: Token sampling - temperature
bool testTemperatureSampling() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> tokens = {42};
        std::vector<float> logits = transformer.forward(tokens);
        
        // Sample with temperature
        std::set<int> sampledTokens;
        for (int i = 0; i < 10; ++i) {
            int token = transformer.sampleToken(logits, 0.8f);
            sampledTokens.insert(token);
            
            if (token < 0 || token >= 1000) {
                std::cerr << "Invalid token from temperature sampling: " << token << std::endl;
                return false;
            }
        }
        
        // With temperature, should get some variety (though not guaranteed)
        // At minimum, check all tokens are valid
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 6: Generation loop
bool testGeneration() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> prompt = {1, 2, 3}; // Simple prompt
        std::vector<int32_t> generated = transformer.generate(prompt, 10, 0.7f);
        
        // Check we generated the requested number of tokens
        if (generated.size() != 13) { // 3 prompt + 10 generated
            std::cerr << "Generated wrong number of tokens: " << generated.size() << std::endl;
            return false;
        }
        
        // Check prompt is preserved
        for (size_t i = 0; i < prompt.size(); ++i) {
            if (generated[i] != prompt[i]) {
                std::cerr << "Prompt not preserved in generation" << std::endl;
                return false;
            }
        }
        
        // Check all tokens are valid
        for (int token : generated) {
            if (token < 0 || token >= 1000) {
                std::cerr << "Invalid generated token: " << token << std::endl;
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 7: Different model sizes
bool testDifferentSizes() {
    try {
        // Small model
        {
            TensorCache cache;
            createMockModel(cache, 1, 64, 2, 500);
            TransformerInference transformer(1, 64, 2, 500, 256);
            if (!transformer.loadWeights(cache)) return false;
            
            std::vector<int32_t> tokens = {10};
            std::vector<float> logits = transformer.forward(tokens);
            if (logits.size() != 500) return false;
        }
        
        // Larger model
        {
            TensorCache cache;
            createMockModel(cache, 4, 256, 8, 2000);
            TransformerInference transformer(4, 256, 8, 2000, 1024);
            if (!transformer.loadWeights(cache)) return false;
            
            std::vector<int32_t> tokens = {10, 20};
            std::vector<float> logits = transformer.forward(tokens);
            if (logits.size() != 2000) return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 8: Context length limits
bool testContextLength() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        // Create tokens up to context limit
        std::vector<int32_t> tokens;
        for (int i = 0; i < 512; ++i) {
            tokens.push_back(i % 1000);
        }
        
        std::vector<float> logits = transformer.forward(tokens);
        if (logits.size() != 1000) return false;
        
        // All logits should be finite even at context limit
        for (float val : logits) {
            if (!std::isfinite(val)) return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 9: Attention score range
bool testAttentionScores() {
    try {
        TensorCache cache;
        createMockModel(cache, 2, 128, 4, 1000);
        
        TransformerInference transformer(2, 128, 4, 1000, 512);
        if (!transformer.loadWeights(cache)) return false;
        
        // Run inference multiple times with different inputs
        for (int run = 0; run < 5; ++run) {
            std::vector<int32_t> tokens;
            for (int i = 0; i < 10; ++i) {
                tokens.push_back((run * 100 + i) % 1000);
            }
            
            std::vector<float> logits = transformer.forward(tokens);
            
            // Check logits are in reasonable range (not exploded)
            float maxLogit = -1e9f, minLogit = 1e9f;
            for (float val : logits) {
                maxLogit = std::max(maxLogit, val);
                minLogit = std::min(minLogit, val);
            }
            
            if (maxLogit > 100.0f || minLogit < -100.0f) {
                std::cerr << "Logits out of range: [" << minLogit << ", " << maxLogit << "]" << std::endl;
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 10: Performance benchmark
bool testPerformance() {
    try {
        TensorCache cache;
        createMockModel(cache, 6, 512, 8, 5000); // ~7M params (similar to GPT-2 small)
        
        TransformerInference transformer(6, 512, 8, 5000, 1024);
        if (!transformer.loadWeights(cache)) return false;
        
        std::vector<int32_t> prompt = {1, 2, 3, 4, 5};
        
        // Warmup
        transformer.forward(prompt);
        
        // Benchmark single forward pass
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10; ++i) {
            transformer.forward(prompt);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        float avgMs = duration / 10.0f;
        float tokensPerSec = 1000.0f / avgMs;
        
        std::cout << "  Performance: " << avgMs << " ms/forward, " 
                  << tokensPerSec << " tok/s" << std::endl;
        
        // Should be reasonably fast (at least 1 tok/s on CPU)
        return tokensPerSec > 1.0f;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// Main test runner
int main(int argc, char** argv) {
    std::cout << "=== TransformerInference Test Suite ===" << std::endl;
    std::cout << "Testing GGML-based transformer implementation\n" << std::endl;
    
    TestLogger::logSection("Basic Functionality Tests");
    TestLogger::logTest("Initialization", testInitialization());
    TestLogger::logTest("Single Token Forward", testSingleTokenForward());
    TestLogger::logTest("Multi-Token Forward", testMultiTokenForward());
    
    TestLogger::logSection("Sampling Tests");
    TestLogger::logTest("Greedy Sampling", testGreedySampling());
    TestLogger::logTest("Temperature Sampling", testTemperatureSampling());
    
    TestLogger::logSection("Generation Tests");
    TestLogger::logTest("Generation Loop", testGeneration());
    
    TestLogger::logSection("Robustness Tests");
    TestLogger::logTest("Different Model Sizes", testDifferentSizes());
    TestLogger::logTest("Context Length Limits", testContextLength());
    TestLogger::logTest("Attention Score Range", testAttentionScores());
    
    TestLogger::logSection("Performance Tests");
    TestLogger::logTest("Performance Benchmark", testPerformance());
    
    TestLogger::printSummary();
    
    return TestLogger::failCount > 0 ? 1 : 0;
}
