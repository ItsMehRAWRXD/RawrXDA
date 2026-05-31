#pragma once
/**
 * @file ai_test_generator.h
 * @brief AI-powered test generation
 * Batch 5 - Item 75: AI test generator
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::AI {

enum class TestType {
    Unit,
    Integration,
    Property,
    Fuzz,
    Benchmark,
    Snapshot
};

enum class TestFramework {
    Catch2,
    GoogleTest,
    Doctest,
    BoostTest,
    Custom
};

struct TestCase {
    std::string name;
    std::string description;
    std::string code;
    std::vector<std::string> tags;
    bool isParameterized;
    std::vector<std::string> parameters;
};

struct TestSuite {
    std::string name;
    std::string filePath;
    TestFramework framework;
    std::vector<TestCase> tests;
    std::string setupCode;
    std::string teardownCode;
};

struct TestGenerationRequest {
    std::string code;
    std::string language;
    TestType type;
    TestFramework framework;
    std::string targetFunction;
    int maxTests;
    bool includeEdgeCases;
    bool includeErrorCases;
    bool includePropertyTests;
};

struct TestGenerationResult {
    TestSuite suite;
    int coverage;
    std::vector<std::string> edgeCases;
    std::vector<std::string> errorCases;
    bool isComplete;
    std::string error;
};

class AITestGenerator {
public:
    AITestGenerator();
    ~AITestGenerator();

    // Initialization
    bool initialize();
    void shutdown();

    // Test generation
    TestGenerationResult generateTests(const TestGenerationRequest& request);
    std::future<TestGenerationResult> generateTestsAsync(const TestGenerationRequest& request);

    // Quick generation
    TestGenerationResult generateUnitTests(const std::string& code,
                                             const std::string& language,
                                             TestFramework framework = TestFramework::Catch2);
    TestGenerationResult generateIntegrationTests(const std::string& code,
                                                   const std::string& language);
    TestGenerationResult generatePropertyTests(const std::string& code,
                                               const std::string& language);

    // Test improvement
    TestGenerationResult improveTests(const TestSuite& existingTests,
                                       const std::string& code);
    TestGenerationResult addEdgeCases(const TestSuite& existingTests,
                                       const std::string& code);
    TestGenerationResult addErrorCases(const TestSuite& existingTests,
                                      const std::string& code);

    // Coverage analysis
    int estimateCoverage(const TestSuite& suite, const std::string& code);
    std::vector<std::string> findUncoveredPaths(const TestSuite& suite,
                                                   const std::string& code);

    // Test execution
    bool runTests(const TestSuite& suite);
    std::vector<std::string> getFailedTests();
    std::string getTestOutput();

    // Configuration
    void setModel(const std::string& model);
    void setDefaultFramework(TestFramework framework);
    void setMaxTests(int maxTests);
    void setIncludeEdgeCases(bool include);
    void setIncludeErrorCases(bool include);

    // Templates
    void registerTemplate(TestFramework framework, const std::string& template_);
    std::string getTemplate(TestFramework framework) const;

    // Events
    using GenerationCallback = std::function<void(const TestGenerationResult&)>;
    void onGenerationComplete(GenerationCallback callback);

private:
    std::string m_model;
    TestFramework m_defaultFramework{TestFramework::Catch2};
    int m_maxTests{10};
    bool m_includeEdgeCases{true};
    bool m_includeErrorCases{true};
    std::map<TestFramework, std::string> m_templates;

    GenerationCallback m_generationCallback;

    TestGenerationResult performGeneration(const TestGenerationRequest& request);
    std::string buildPrompt(const TestGenerationRequest& request);
    TestSuite parseResponse(const std::string& response);
    void notifyGenerationComplete(const TestGenerationResult& result);
};

// Global instance
AITestGenerator& getAITestGenerator();

} // namespace RawrXD::AI
