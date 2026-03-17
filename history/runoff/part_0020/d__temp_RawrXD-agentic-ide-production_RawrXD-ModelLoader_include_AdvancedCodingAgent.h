#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace IDE {

// Agent task result
struct AgentTaskResult {
    bool success;
    std::string result;
    std::vector<std::string> steps;        // Step-by-step breakdown
    std::string explanation;
    float confidence;
    std::vector<std::string> warnings;
    bool requiresReview;
};

// Test case
struct TestCase {
    std::string name;
    std::string setup;
    std::string testCode;
    std::string expectedOutput;
    std::string assertions;
};

// Security vulnerability
struct Vulnerability {
    std::string type;                // "injection", "buffer_overflow", "xss", etc.
    std::string severity;            // "critical", "high", "medium", "low"
    std::string description;
    int lineNumber;
    std::string suggestedFix;
};

// Advanced AI agent for coding tasks
class AdvancedCodingAgent {
public:
    AdvancedCodingAgent();
    ~AdvancedCodingAgent() = default;

    // Initialize agent with models
    bool initialize(const std::string& mainModelUrl, const std::string& toolModelUrl);

    // ===== FEATURE IMPLEMENTATION =====
    // Generate code to implement a feature description
    AgentTaskResult implementFeature(
        const std::string& description,
        const std::string& context,
        const std::string& language,
        const std::vector<std::string>& existingCode = {}
    );

    // ===== TEST GENERATION =====
    // Generate test cases for a function
    std::vector<TestCase> generateTests(
        const std::string& functionCode,
        const std::string& functionName,
        const std::string& language,
        int minCases = 3
    );

    // Generate comprehensive test suite
    std::string generateTestSuite(
        const std::string& sourceFile,
        const std::string& language
    );

    // ===== DOCUMENTATION GENERATION =====
    // Generate documentation for code
    std::string generateDocumentation(
        const std::string& code,
        const std::string& language,
        const std::string& style = "markdown"  // "markdown", "doxygen", "javadoc", etc.
    );

    // Generate API documentation
    std::string generateAPIDocumentation(
        const std::vector<std::string>& functions,
        const std::string& language
    );

    // ===== BUG DETECTION & FIXING =====
    // Detect bugs in code
    std::vector<std::pair<int, std::string>> detectBugs(
        const std::string& code,
        const std::string& language,
        const std::string& context = ""
    );

    // Suggest fixes for bugs
    AgentTaskResult suggestBugFixes(
        const std::string& code,
        const std::string& bugDescription,
        const std::string& language
    );

    // Auto-fix detected issues
    AgentTaskResult autoFixCode(
        const std::string& code,
        const std::string& language
    );

    // ===== PERFORMANCE OPTIMIZATION =====
    // Analyze performance bottlenecks
    std::vector<std::pair<int, std::string>> analyzePerformance(
        const std::string& code,
        const std::string& language
    );

    // Generate optimized version
    AgentTaskResult optimizeForPerformance(
        const std::string& code,
        const std::string& language,
        const std::string& targetMetric = "execution_time"  // "execution_time", "memory", "cpu"
    );

    // ===== SECURITY ANALYSIS =====
    // Detect security vulnerabilities
    std::vector<Vulnerability> scanForVulnerabilities(
        const std::string& code,
        const std::string& language
    );

    // Generate security fix
    AgentTaskResult generateSecurityFix(
        const std::string& code,
        const Vulnerability& vulnerability,
        const std::string& language
    );

    // ===== CODE REVIEW =====
    // Comprehensive code review
    struct CodeReviewResult {
        std::vector<std::string> issues;
        std::vector<std::string> suggestions;
        std::vector<std::string> bestPractices;
        int overallScore;  // 0-100
    };

    CodeReviewResult reviewCode(
        const std::string& code,
        const std::string& language,
        const std::string& codeStyle = "google"
    );

    // ===== REFACTORING =====
    // Suggest refactorings
    struct RefactoringSuggestion {
        std::string type;           // "extract_method", "rename", "consolidate", etc.
        std::string description;
        std::string beforeCode;
        std::string afterCode;
        float estimatedBenefit;     // 0.0-1.0
    };

    std::vector<RefactoringSuggestion> suggestRefactorings(
        const std::string& code,
        const std::string& language
    );

    // ===== EXPLANATION GENERATION =====
    // Explain what code does
    std::string explainCode(
        const std::string& code,
        const std::string& language,
        const std::string& targetAudience = "developer"
    );

    // Generate inline comments
    std::string generateInlineComments(
        const std::string& code,
        const std::string& language
    );

    // ===== MULTI-STEP TASK EXECUTION =====
    // Execute complex multi-step task
    AgentTaskResult executeComplexTask(
        const std::string& taskDescription,
        const std::vector<std::string>& availableTools,
        int maxSteps = 10
    );

    // Configure agent
    void setMaxGenerationTokens(int tokens);
    void setTemperature(float temp);
    void setTopP(float p);

private:
    // Model inference
    std::string inferCode(const std::string& prompt);
    std::vector<std::string> inferMultipleOptions(const std::string& prompt, int count);

    // Analysis
    std::string analyzeCode(const std::string& code, const std::string& analysisType);

    // Validation
    bool validateGeneratedCode(
        const std::string& code,
        const std::string& language,
        const std::string& context
    );

    std::string m_mainModelUrl;
    std::string m_toolModelUrl;
    int m_maxTokens;
    float m_temperature;
    float m_topP;
};

} // namespace IDE
} // namespace RawrXD
