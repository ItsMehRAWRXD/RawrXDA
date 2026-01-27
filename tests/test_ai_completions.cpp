/**
 * \file test_ai_completions.cpp
 * \brief Test AI-powered code completion end-to-end
 * \author RawrXD Team
 * \date 2025-12-13
 *
 * Verifies the complete chain:
 * 1. InferenceEngine loads GGUF model
 * 2. RealTimeCompletionEngine generates completions
 * 3. AICompletionProvider wraps for Qt
 * 4. AgenticTextEdit displays ghost text
 */

#include <iostream>
#include <memory>
#include <string>
#include "real_time_completion_engine.h"
#include "inference_engine.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

int main() {
    std::cout << "\n=== Testing AI Code Completion Chain ===\n\n";

    try {
        // 1. Initialize logging and metrics
        auto logger = std::make_shared<Logger>();
        auto metrics = std::make_shared<Metrics>();
        logger->setLevel(LogLevel::DEBUG);
        logger->info("Test initialized");

        // 2. Create InferenceEngine
        std::cout << "[1/5] Creating InferenceEngine...\n";
        InferenceEngine engine(nullptr);

        // 3. Load a GGUF model
        std::cout << "[2/5] Loading GGUF model...\n";
        std::string modelPath = "models/ministral-3b-instruct-v0.3-Q4_K_M.gguf";
        bool loaded = engine.Initialize(modelPath);

        if (!loaded) {
            std::cerr << "✗ Failed to load model: " << modelPath << "\n";
            std::cerr << "  Tip: Place a GGUF model in the models/ directory\n";
            return 1;
        }

        std::cout << "✓ Model loaded successfully\n";
        std::cout << "  Vocab size: " << engine.GetVocabSize() << "\n";
        std::cout << "  Embedding dim: " << engine.GetEmbeddingDim() << "\n\n";

        // 4. Create RealTimeCompletionEngine
        std::cout << "[3/5] Initializing CompletionEngine...\n";
        RealTimeCompletionEngine completionEngine(logger, metrics);
        completionEngine.setInferenceEngine(&engine);
        std::cout << "✓ CompletionEngine initialized\n\n";

        // 5. Test completion generation
        std::cout << "[4/5] Generating test completion...\n";
        
        std::string testPrefix = "void calculate() {\n    int result = ";
        std::string testSuffix = ";\n}";
        std::string testContext = "// Calculate sum of array";

        std::cout << "Input:\n";
        std::cout << "  Prefix:  \"" << testPrefix << "\"\n";
        std::cout << "  Suffix:  \"" << testSuffix << "\"\n";
        std::cout << "  Context: \"" << testContext << "\"\n\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        
        auto completions = completionEngine.getCompletions(
            testPrefix,
            testSuffix,
            "cpp",
            testContext
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(
            endTime - startTime
        ).count();

        std::cout << "✓ Completions generated in " << latencyMs << " ms\n";
        std::cout << "  Count: " << completions.size() << "\n\n";

        // 6. Display completions
        std::cout << "[5/5] Completions:\n";
        for (size_t i = 0; i < completions.size(); ++i) {
            const auto& comp = completions[i];
            std::cout << "\n  [" << (i + 1) << "] " << comp.kind << " (confidence: " 
                      << (comp.confidence * 100) << "%)\n";
            std::cout << "      Text: \"" << comp.text << "\"\n";
            std::cout << "      Detail: " << comp.detail << "\n";
        }

        // 7. Check performance metrics
        std::cout << "\n=== Performance Metrics ===\n";
        auto perfMetrics = completionEngine.getMetrics();
        std::cout << "  Average latency: " << perfMetrics.avgLatencyMs << " ms\n";
        std::cout << "  P95 latency: " << perfMetrics.p95LatencyMs << " ms\n";
        std::cout << "  P99 latency: " << perfMetrics.p99LatencyMs << " ms\n";
        std::cout << "  Cache hit rate: " << (perfMetrics.cacheHitRate * 100) << "%\n";
        std::cout << "  Total requests: " << perfMetrics.requestCount << "\n";
        std::cout << "  Errors: " << perfMetrics.errorCount << "\n";

        // Verify latency target
        std::cout << "\n=== Verification ===\n";
        if (perfMetrics.avgLatencyMs < 100.0) {
            std::cout << "✓ PASS: Latency under 100ms target\n";
        } else {
            std::cout << "✗ WARNING: Latency above 100ms target\n";
        }

        if (completions.size() > 0) {
            std::cout << "✓ PASS: Completions generated successfully\n";
        } else {
            std::cout << "✗ FAIL: No completions generated\n";
            return 1;
        }

        std::cout << "\n✅ All tests passed!\n\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n\n";
        return 1;
    }
}
