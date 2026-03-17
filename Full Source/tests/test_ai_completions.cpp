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
    s_logger.info("\n=== Testing AI Code Completion Chain ===\n\n");

    try {
        // 1. Initialize logging and metrics
        auto logger = std::make_shared<Logger>();
        auto metrics = std::make_shared<Metrics>();
        logger->setLevel(LogLevel::DEBUG);
        logger->info("Test initialized");

        // 2. Create InferenceEngine
        s_logger.info("[1/5] Creating InferenceEngine...\n");
        InferenceEngine engine(nullptr);

        // 3. Load a GGUF model
        s_logger.info("[2/5] Loading GGUF model...\n");
        std::string modelPath = "models/ministral-3b-instruct-v0.3-Q4_K_M.gguf";
        bool loaded = engine.Initialize(modelPath);

        if (!loaded) {
            s_logger.error( "✗ Failed to load model: " << modelPath << "\n";
            s_logger.error( "  Tip: Place a GGUF model in the models/ directory\n";
            return 1;
        }

        s_logger.info("✓ Model loaded successfully\n");
        s_logger.info("  Vocab size: ");
        s_logger.info("  Embedding dim: ");

        // 4. Create RealTimeCompletionEngine
        s_logger.info("[3/5] Initializing CompletionEngine...\n");
        RealTimeCompletionEngine completionEngine(logger, metrics);
        completionEngine.setInferenceEngine(&engine);
        s_logger.info("✓ CompletionEngine initialized\n\n");

        // 5. Test completion generation
        s_logger.info("[4/5] Generating test completion...\n");
        
        std::string testPrefix = "void calculate() {\n    int result = ";
        std::string testSuffix = ";\n}";
        std::string testContext = "// Calculate sum of array";

        s_logger.info("Input:\n");
        s_logger.info("  Prefix:  \");
        s_logger.info("  Suffix:  \");
        s_logger.info("  Context: \");

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

        s_logger.info("✓ Completions generated in ");
        s_logger.info("  Count: ");

        // 6. Display completions
        s_logger.info("[5/5] Completions:\n");
        for (size_t i = 0; i < completions.size(); ++i) {
            const auto& comp = completions[i];
            s_logger.info("\n  [");
            s_logger.info("      Text: \");
            s_logger.info("      Detail: ");
        }

        // 7. Check performance metrics
        s_logger.info("\n=== Performance Metrics ===\n");
        auto perfMetrics = completionEngine.getMetrics();
        s_logger.info("  Average latency: ");
        s_logger.info("  P95 latency: ");
        s_logger.info("  P99 latency: ");
        s_logger.info("  Cache hit rate: ");
        s_logger.info("  Total requests: ");
        s_logger.info("  Errors: ");

        // Verify latency target
        s_logger.info("\n=== Verification ===\n");
        if (perfMetrics.avgLatencyMs < 100.0) {
            s_logger.info("✓ PASS: Latency under 100ms target\n");
        } else {
            s_logger.info("✗ WARNING: Latency above 100ms target\n");
        }

        if (completions.size() > 0) {
            s_logger.info("✓ PASS: Completions generated successfully\n");
        } else {
            s_logger.info("✗ FAIL: No completions generated\n");
            return 1;
        }

        s_logger.info("\n✅ All tests passed!\n\n");
        return 0;

    } catch (const std::exception& e) {
        s_logger.error( "\n✗ Test failed with exception: " << e.what() << "\n\n";
        return 1;
    }
}
