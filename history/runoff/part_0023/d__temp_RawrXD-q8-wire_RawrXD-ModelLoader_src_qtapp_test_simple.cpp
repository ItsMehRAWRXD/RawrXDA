#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

// Simple standalone test for transformer logic
class MockTransformer {
public:
    MockTransformer(int nLayers, int nEmbd, int nHead, int nVocab, int ctxSize)
        : m_nLayers(nLayers), m_nEmbd(nEmbd), m_nHead(nHead), m_nVocab(nVocab), m_ctxSize(ctxSize) {}
    
    std::vector<float> forward(const std::vector<int32_t>& tokens) {
        // Mock forward pass generating reasonable logits
        std::vector<float> logits(m_nVocab, 0.0f);
        std::mt19937 gen(tokens.empty() ? 0 : tokens.back());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (int i = 0; i < m_nVocab; ++i) {
            logits[i] = dist(gen);
        }
        return logits;
    }
    
    int sampleToken(const std::vector<float>& logits, float temperature) {
        if (temperature <= 0.0f) {
            return std::max_element(logits.begin(), logits.end()) - logits.begin();
        }
        
        std::vector<float> probs = logits;
        for (auto& p : probs) p = expf(p / temperature);
        float sum = 0.0f;
        for (auto p : probs) sum += p;
        for (auto& p : probs) p /= sum;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float r = dist(gen);
        float cumsum = 0.0f;
        for (size_t i = 0; i < probs.size(); ++i) {
            cumsum += probs[i];
            if (r < cumsum) return static_cast<int>(i);
        }
        return static_cast<int>(probs.size() - 1);
    }
    
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
};

// Test utilities
int passCount = 0, failCount = 0;

void logTest(const std::string& name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << std::endl;
    if (passed) passCount++; else failCount++;
}

void logSection(const std::string& section) {
    std::cout << "\n=== " << section << " ===" << std::endl;
}

// Tests
bool testForward() {
    try {
        MockTransformer t(2, 128, 4, 1000, 512);
        std::vector<int32_t> tokens = {42};
        auto logits = t.forward(tokens);
        
        if (logits.size() != 1000) return false;
        for (float val : logits) {
            if (!std::isfinite(val)) return false;
        }
        return true;
    } catch (...) { return false; }
}

bool testGreedySampling() {
    try {
        MockTransformer t(2, 128, 4, 1000, 512);
        std::vector<int32_t> tokens = {42};
        auto logits = t.forward(tokens);
        
        int token1 = t.sampleToken(logits, 0.0f);
        int token2 = t.sampleToken(logits, 0.0f);
        
        if (token1 != token2) return false;
        if (token1 < 0 || token1 >= 1000) return false;
        return true;
    } catch (...) { return false; }
}

bool testGeneration() {
    try {
        MockTransformer t(2, 128, 4, 1000, 512);
        std::vector<int32_t> prompt = {1, 2, 3};
        auto generated = t.generate(prompt, 10, 0.7f);
        
        if (generated.size() != 13) return false;
        for (size_t i = 0; i < prompt.size(); ++i) {
            if (generated[i] != prompt[i]) return false;
        }
        for (int token : generated) {
            if (token < 0 || token >= 1000) return false;
        }
        return true;
    } catch (...) { return false; }
}

bool testPerformance() {
    try {
        MockTransformer t(6, 512, 8, 5000, 1024);
        std::vector<int32_t> prompt = {1, 2, 3, 4, 5};
        
        // Warmup
        t.forward(prompt);
        
        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            t.forward(prompt);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        float avgMs = duration / 100.0f;
        
        std::cout << "  Performance: " << avgMs << " ms/forward (" << (1000.0f / avgMs) << " tok/s)" << std::endl;
        return avgMs < 1000.0f;  // Should be under 1 second per forward
    } catch (...) { return false; }
}

int main() {
    std::cout << "=== Transformer Logic Test Suite ===" << std::endl;
    std::cout << "Testing core transformer algorithms\n" << std::endl;
    
    logSection("Basic Tests");
    logTest("Forward Pass", testForward());
    logTest("Greedy Sampling", testGreedySampling());
    logTest("Generation Loop", testGeneration());
    
    logSection("Performance");
    logTest("Performance Benchmark", testPerformance());
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Passed: " << passCount << std::endl;
    std::cout << "Failed: " << failCount << std::endl;
    
    return failCount > 0 ? 1 : 0;
}
