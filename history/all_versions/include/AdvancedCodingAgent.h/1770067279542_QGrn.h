#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace IDE {

// Test case
struct TestCase {
    std::string name;
    std::string setup;
    std::string testCode;
    std::string expectedOutput;
    std::string assertions;
    std::string expectedResult; // Added for compatibility
};

// Security vulnerability
struct Vulnerability {
    std::string type;                // "injection", "buffer_overflow", "xss", etc.
    std::string severity;            // "critical", "high", "medium", "low"
    std::string description;
    int lineNumber;
    std::string suggestedFix;
};

// Agent task result
struct AgentTaskResult {
    bool success = false;
    std::string taskType;
    std::string result;
    std::string generatedCode;
    std::vector<std::string> steps;        // Step-by-step breakdown
    std::string explanation;
    float confidence = 0.0f;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    bool requiresReview = false;
    
    // Detailed results
    std::vector<TestCase> testCases;
    std::vector<Vulnerability> vulnerabilities;
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
    AgentTaskResult generateTests(
        const std::string& code,
        const std::string& language,
        int minCoverage = 50
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
    AgentTaskResult generateDocumentation(
        const std::string& code,
        const std::string& language
    );

    // ===== QUALITY ASSURANCE =====
    AgentTaskResult detectBugs(
        const std::string& code,
        const std::string& language
    );

    AgentTaskResult optimizeForPerformance(
        const std::string& code,
        const std::string& language
    );

    AgentTaskResult scanForVulnerabilities(
        const std::string& code,
        const std::string& language
    );
    
    AgentTaskResult refactorCode(
        const std::string& code,
        const std::string& language,
        const std::string& strategy
    );

private:
    // Helpers
    std::string inferCode(const std::string& prompt);
    bool validateGeneratedCode(const std::string& code, const std::string& language, const std::string& context);
    
    // Internal generators
    std::string generateFeatureBoilerplate(const std::string& spec, const std::string& lang);
    std::vector<TestCase> identifyTestCases(const std::string& code, const std::string& lang);
    std::string generateTestCode(const std::string& code, const std::vector<TestCase>& cases, const std::string& lang);
    std::string extractDocumentation(const std::string& code, const std::string& lang);
    std::vector<Vulnerability> performStaticAnalysis(const std::string& code, const std::string& lang);
    std::vector<Vulnerability> scanSecurityIssues(const std::string& code, const std::string& lang);
    
    // Refactoring helpers
    std::string extractMethodRefactoring(const std::string& c, const std::string& l);
    std::string reduceComplexity(const std::string& c, const std::string& l);
    std::string improveNaming(const std::string& c, const std::string& l);
    std::string replaceStringConcatenation(const std::string& c);
};

} // namespace IDE
} // namespace RawrXD
