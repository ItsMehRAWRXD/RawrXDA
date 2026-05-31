// ============================================================================
// test_code_transformer.cpp — Smoke test for Code Transformer
// Validates transformation engine, safety gates, and learning system.
// ============================================================================
#include "../src/ai/code_transformer.hpp"
#include "../src/ai/code_transformer_integration.hpp"
#include <iostream>
#include <cassert>
#include <chrono>

using namespace RawrXD::AI;

// ============================================================================
// Test Helpers
// ============================================================================
static bool g_testsPassed = true;
static int g_testCount = 0;
static int g_passCount = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running: " #name "... "; \
    g_testCount++; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        g_passCount++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        g_testsPassed = false; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        throw std::runtime_error("Assertion failed: " #expr); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))

// ============================================================================
// Tests
// ============================================================================

TEST(transformer_initialization) {
    CodeTransformer transformer(nullptr);
    ASSERT_TRUE(transformer.Initialize());
    ASSERT_TRUE(transformer.IsInitialized());
}

TEST(transformer_basic_refactor) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void processData() {
            int x = 1;
            int y = 2;
            int z = x + y;
        }
    )";
    
    TransformationConstraints constraints;
    constraints.SetMethodName("extractedMethod");
    
    auto result = transformer.TransformCode(code, 
        TransformationType::RefactorExtractMethod, constraints);
    
    // Without inference client, should use learned patterns
    ASSERT_TRUE(result.success || !result.errorMessage.empty());
}

TEST(transformer_bounds_check) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void processArray(int arr[], int index) {
            arr[index] = 42;
        }
    )";
    
    auto result = transformer.TransformCode(code,
        TransformationType::SecurityBoundsCheck, {});
    
    // Should succeed with learned pattern
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.syntaxValid);
}

TEST(transformer_null_check) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void processPointer(int* ptr) {
            *ptr = 42;
        }
    )";
    
    auto result = transformer.TransformCode(code,
        TransformationType::SecurityNullCheck, {});
    
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.syntaxValid);
}

TEST(transformer_simd_optimization) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void addArrays(float a[], float b[], float c[], int n) {
            for (int i = 0; i < n; i++) {
                c[i] = a[i] + b[i];
            }
        }
    )";
    
    auto result = transformer.TransformCode(code,
        TransformationType::OptimizeSIMD, {});
    
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.syntaxValid);
}

TEST(transformer_safety_gates) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    // Test with invalid code
    std::string invalidCode = R"(
        void broken() {
            missing_semicolon
        }
    )";
    
    auto result = transformer.TransformCode(invalidCode,
        TransformationType::RefactorExtractMethod, {});
    
    // Should either succeed or fail gracefully
    ASSERT_TRUE(result.success || !result.errorMessage.empty());
}

TEST(transformer_learn_from_example) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string before = "auto temp = calculate();\nuse(temp);";
    std::string after = "use(calculate());";
    
    transformer.LearnFromExample(before, after, TransformationType::RefactorInlineVariable);
    
    auto patterns = transformer.GetPatternsByType(TransformationType::RefactorInlineVariable);
    ASSERT_GT(patterns.size(), 0);
}

TEST(transformer_suggestions) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void complexFunction() {
            int a = 1;
            int b = 2;
            int c = 3;
            int result = a + b + c;
        }
    )";
    
    auto suggestions = transformer.SuggestTransformations(code);
    
    // Should return learned pattern suggestions
    ASSERT_GT(suggestions.size(), 0);
}

TEST(transformer_metrics) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void test() {
            int x = 1;
        }
    )";
    
    auto result = transformer.TransformCode(code,
        TransformationType::SecurityNullCheck, {});
    
    // Should have metrics
    ASSERT_TRUE(result.duration.count() >= 0);
}

TEST(transformer_integration) {
    auto transformer = std::make_shared<CodeTransformer>(nullptr);
    CodeTransformerIntegration integration(transformer);
    
    ASSERT_TRUE(integration.Initialize());
    ASSERT_TRUE(integration.IsInitialized());
    
    integration.Shutdown();
    ASSERT_FALSE(integration.IsInitialized());
}

TEST(transformer_performance) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    std::string code = R"(
        void test() {
            int x = 1;
        }
    )";
    
    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = transformer.TransformCode(code,
        TransformationType::SecurityNullCheck, {});
    auto t1 = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    
    ASSERT_TRUE(result.success);
    ASSERT_LT(duration.count(), 1000);  // Should complete in < 1 second
}

TEST(transformer_pattern_management) {
    CodeTransformer transformer(nullptr);
    transformer.Initialize();
    
    // Get patterns by type
    auto patterns = transformer.GetPatternsByType(TransformationType::SecurityBoundsCheck);
    ASSERT_GT(patterns.size(), 0);
    
    // Get patterns by tag
    auto taggedPatterns = transformer.GetPatternsByTag("security");
    ASSERT_GT(taggedPatterns.size(), 0);
    
    // Increment usage
    if (!patterns.empty()) {
        transformer.IncrementPatternUsage(patterns[0].patternId);
        auto updatedPatterns = transformer.GetPatternsByType(TransformationType::SecurityBoundsCheck);
        ASSERT_EQ(updatedPatterns[0].usageCount, 1);
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Code Transformer Tests\n";
    std::cout << "========================================\n\n";
    
    RUN_TEST(transformer_initialization);
    RUN_TEST(transformer_basic_refactor);
    RUN_TEST(transformer_bounds_check);
    RUN_TEST(transformer_null_check);
    RUN_TEST(transformer_simd_optimization);
    RUN_TEST(transformer_safety_gates);
    RUN_TEST(transformer_learn_from_example);
    RUN_TEST(transformer_suggestions);
    RUN_TEST(transformer_metrics);
    RUN_TEST(transformer_integration);
    RUN_TEST(transformer_performance);
    RUN_TEST(transformer_pattern_management);
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << g_passCount << "/" << g_testCount << " passed\n";
    std::cout << "========================================\n";
    
    return g_testsPassed ? 0 : 1;
}
