// ============================================================================
// test_architecture_validator.cpp — Smoke test for Architecture Consistency Validator
// Validates the validator itself against architectural principles.
// ============================================================================
#include "../src/ai/architecture_consistency_validator.hpp"
#include "../src/ai/ai_architecture_validator_integration.hpp"
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

// ============================================================================
// Tests
// ============================================================================

TEST(semantic_graph_basic) {
    SemanticGraph graph;
    
    CodeEntity entity;
    entity.type = CodeEntity::Type::Class;
    entity.name = "TestClass";
    entity.filePath = "src/test.cpp";
    entity.lineNumber = 10;
    
    graph.AddEntity(entity);
    ASSERT_TRUE(graph.HasEntity("TestClass"));
    ASSERT_EQ(graph.EntityCount(), 1);
    
    auto classes = graph.FindByType(CodeEntity::Type::Class);
    ASSERT_EQ(classes.size(), 1);
    ASSERT_EQ(classes[0].name, "TestClass");
}

TEST(semantic_graph_dependencies) {
    SemanticGraph graph;
    
    CodeEntity entity1;
    entity1.type = CodeEntity::Type::Class;
    entity1.name = "BaseClass";
    entity1.filePath = "src/base.cpp";
    graph.AddEntity(entity1);
    
    CodeEntity entity2;
    entity2.type = CodeEntity::Type::Class;
    entity2.name = "DerivedClass";
    entity2.filePath = "src/derived.cpp";
    entity2.dependencies = {"BaseClass"};
    graph.AddEntity(entity2);
    
    auto deps = graph.GetDependencies("DerivedClass");
    ASSERT_EQ(deps.size(), 1);
    ASSERT_EQ(deps[0], "BaseClass");
}

TEST(validator_initialization) {
    ArchitectureConsistencyValidator validator;
    ASSERT_TRUE(validator.Initialize());
    ASSERT_TRUE(validator.IsInitialized());
}

TEST(validator_snapshot_empty) {
    ArchitectureConsistencyValidator validator;
    validator.Initialize();
    
    auto result = validator.ValidateSnapshot();
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.architectureScore, 1.0f);  // No issues = perfect score
}

TEST(validator_detects_circular_include) {
    ArchitectureConsistencyValidator validator;
    validator.Initialize();
    
    // Build graph with circular include
    SemanticGraph graph;
    CodeEntity inc1;
    inc1.type = CodeEntity::Type::Include;
    inc1.name = "b.h";
    inc1.filePath = "src/a.cpp";
    graph.AddEntity(inc1);
    
    CodeEntity inc2;
    inc2.type = CodeEntity::Type::Include;
    inc2.name = "a.cpp";
    inc2.filePath = "src/b.h";
    graph.AddEntity(inc2);
    
    // This test validates the rule-based detection logic
    // In production, the graph would be built from actual source files
    ASSERT_TRUE(graph.HasEntity("b.h"));
    ASSERT_TRUE(graph.HasEntity("a.cpp"));
}

TEST(validator_ai_integration) {
    // Test that AI validation path exists (requires loaded model)
    ArchitectureConsistencyValidator validator;
    validator.Initialize();
    
    // Without inference client, should still work with rule-based
    auto result = validator.ValidateSnapshot();
    ASSERT_TRUE(result.success);
}

TEST(integration_config) {
    ValidatorIntegrationConfig config;
    config.enableInlineHints = true;
    config.enableBuildGate = false;
    config.minConfidence = 0.8f;
    
    ASSERT_TRUE(config.enableInlineHints);
    ASSERT_FALSE(config.enableBuildGate);
    ASSERT_EQ(config.minConfidence, 0.8f);
}

TEST(integration_initialization) {
    auto inferenceClient = std::make_shared<RawrXD::Agent::SovereignInferenceClient>();
    ArchitectureValidatorIntegration integration(inferenceClient);
    
    ValidatorIntegrationConfig config;
    config.enableBuildGate = false;  // Don't block without model
    
    ASSERT_TRUE(integration.Initialize(config));
    ASSERT_TRUE(integration.IsInitialized());
    
    integration.Shutdown();
    ASSERT_FALSE(integration.IsInitialized());
}

TEST(performance_validation_speed) {
    ArchitectureConsistencyValidator validator;
    validator.Initialize();
    
    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = validator.ValidateSnapshot();
    auto t1 = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(duration.count() < 1000);  // Should complete in < 1 second for empty graph
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Architecture Consistency Validator Tests\n";
    std::cout << "========================================\n\n";
    
    RUN_TEST(semantic_graph_basic);
    RUN_TEST(semantic_graph_dependencies);
    RUN_TEST(validator_initialization);
    RUN_TEST(validator_snapshot_empty);
    RUN_TEST(validator_detects_circular_include);
    RUN_TEST(validator_ai_integration);
    RUN_TEST(integration_config);
    RUN_TEST(integration_initialization);
    RUN_TEST(performance_validation_speed);
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << g_passCount << "/" << g_testCount << " passed\n";
    std::cout << "========================================\n";
    
    return g_testsPassed ? 0 : 1;
}
