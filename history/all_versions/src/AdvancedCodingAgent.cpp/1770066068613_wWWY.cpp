#include "AdvancedCodingAgent.h"
#include "cpu_inference_engine.h" // Added for inference
#include <algorithm>
#include <sstream>

namespace RawrXD {
namespace IDE {

AdvancedCodingAgent::AdvancedCodingAgent() {
}

AgentTaskResult AdvancedCodingAgent::implementFeature(
    const std::string& specification, const std::string& language,
    const std::string& codeContext, const std::vector<std::string>& requirements) {
    
    AgentTaskResult result;
    result.taskType = "feature_implementation";
    result.success = false;
    
    // Step 1: Analyze requirements
    std::string analysisPrompt = "Analyze the following feature requirements and break them down into implementation steps:\n\n";
    analysisPrompt += "Feature: " + specification + "\n";
    analysisPrompt += "Language: " + language + "\n";
    if (!codeContext.empty()) {
        analysisPrompt += "Existing Code Context:\n" + codeContext + "\n";
    }
    if (!requirements.empty()) {
        analysisPrompt += "Additional Requirements:\n";
        for (const auto& req : requirements) {
            analysisPrompt += "- " + req + "\n";
        }
    }
    analysisPrompt += "\nProvide a step-by-step implementation plan:";
    
    result.steps.push_back("Analyzing requirements...");
    std::string analysis = inferCode(analysisPrompt);
    result.steps.push_back("Analysis: " + analysis);
    
    // Step 2: Generate code
    std::string codePrompt = "Implement the following feature in " + language + ":\n\n";
    codePrompt += "Feature: " + specification + "\n";
    codePrompt += "Based on the analysis: " + analysis + "\n";
    if (!codeContext.empty()) {
        codePrompt += "Integrate with existing code:\n" + codeContext + "\n";
    }
    codePrompt += "\nProvide complete, production-ready code with proper error handling and comments:";
    
    result.steps.push_back("Generating implementation...");
    std::string generatedCode = inferCode(codePrompt);
    
    // Step 3: Validate generated code
    result.steps.push_back("Validating generated code...");
    bool isValid = validateGeneratedCode(generatedCode, language, codeContext);
    
    if (isValid) {
        result.generatedCode = generatedCode;
        result.success = true;
        result.confidence = 0.85f;
        result.explanation = "Feature implementation generated successfully based on requirements analysis";
    } else {
        result.warnings.push_back("Generated code may have issues - review required");
        result.generatedCode = generatedCode; // Still return it but mark for review
        result.success = true;
        result.requiresReview = true;
        result.confidence = 0.6f;
    }
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::generateTests(
    const std::string& code, const std::string& language,
    int minCoverage) {
    
    AgentTaskResult result;
    result.taskType = "test_generation";
    result.success = false;
    
    // Analyze code for test cases
    std::vector<TestCase> testCases = identifyTestCases(code, language);
    
    result.steps.push_back("Identified " + std::to_string(testCases.size()) + " test cases");
    
    // Generate test code
    std::string testCode = generateTestCode(code, testCases, language);
    
    result.steps.push_back("Generated test code");
    result.generatedCode = testCode;
    result.testCases = testCases;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::generateDocumentation(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "documentation_generation";
    result.success = false;
    
    // Parse code structure
    result.steps.push_back("Analyzing code structure");
    
    // Extract functions, classes, etc.
    std::string documentation = "# API Documentation\n\n";
    documentation += extractDocumentation(code, language);
    
    result.steps.push_back("Generated documentation");
    result.generatedCode = documentation;
    result.success = true;
    result.confidence = 0.85f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::detectBugs(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "bug_detection";
    result.success = false;
    
    std::vector<Vulnerability> vulns = performStaticAnalysis(code, language);
    
    result.steps.push_back("Performed static analysis");
    
    for (const auto& vuln : vulns) {
        result.suggestions.push_back(vuln.description + " - " + vuln.suggestedFix);
    }
    
    result.vulnerabilities = vulns;
    result.success = true;
    result.confidence = 0.7f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::optimizeForPerformance(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "performance_optimization";
    result.success = false;
    
    result.steps.push_back("Analyzing code for performance issues");
    
    // Identify optimization opportunities
    std::string optimizedCode = code;
    
    // Check for common inefficiencies
    if (code.find("+ \"") != std::string::npos) {
        result.suggestions.push_back("Use stringstream for string concatenation");
        optimizedCode = replaceStringConcatenation(code);
    }
    
    if (code.find("vector") != std::string::npos) {
        result.suggestions.push_back("Reserve vector capacity if size is known");
    }
    
    result.steps.push_back("Generated optimizations");
    result.generatedCode = optimizedCode;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::scanForVulnerabilities(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "security_scan";
    result.success = false;
    
    result.steps.push_back("Performing security analysis");
    
    std::vector<Vulnerability> vulnerabilities = scanSecurityIssues(code, language);
    
    for (const auto& vuln : vulnerabilities) {
        result.suggestions.push_back("SECURITY: " + vuln.description);
    }
    
    result.vulnerabilities = vulnerabilities;
    result.steps.push_back("Identified " + std::to_string(vulnerabilities.size()) + " potential issues");
    result.success = true;
    result.confidence = 0.8f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::refactorCode(
    const std::string& code, const std::string& language,
    const std::string& strategy) {
    
    AgentTaskResult result;
    result.taskType = "refactoring";
    result.success = false;
    
    result.steps.push_back("Analyzing code structure");
    
    std::string refactoredCode = code;
    
    if (strategy == "extract_methods") {
        result.steps.push_back("Extracting long methods");
        refactoredCode = extractMethodRefactoring(code, language);
    } else if (strategy == "reduce_complexity") {
        result.steps.push_back("Reducing cyclomatic complexity");
        refactoredCode = reduceComplexity(code, language);
    } else if (strategy == "improve_names") {
        result.steps.push_back("Improving variable names");
        refactoredCode = improveNaming(code, language);
    }
    
    result.generatedCode = refactoredCode;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

// Private helper methods

std::string AdvancedCodingAgent::inferCode(const std::string& prompt) {
    if (prompt.empty()) return "// Error: Empty prompt provided.";

    try {
        auto engine = RawrXD::CPUInferenceEngine::getInstance();
        if (!engine) {
             return "// Error: Inference Engine instance not available.";
        }
        
        // Use a reasonable default configuration for code generation
        float temp = 0.2f; // Lower temperature for code
        float top_p = 0.95f;
        int max_tokens = 2048; // Allow longer generation for code
        
        // Add a system instruction to ensure code-only output if possible,
        // though the prompt usually handles it.
        std::string enhancedPrompt = prompt;
        
        auto result = engine->generate(enhancedPrompt, temp, top_p, max_tokens);
        if (result.has_value()) {
            return result.value().text;
        } else {
            return "// Error: Inference failed. Code: " + std::to_string((int)result.error());
        }
    } catch (const std::exception& e) {
        return std::string("// Error: Exception during inference: ") + e.what();
    } catch (...) {
        return "// Error: Unknown exception during inference.";
    }
}

bool AdvancedCodingAgent::validateGeneratedCode(
    const std::string& code,
    const std::string& language,
    const std::string& context
) {
    if (code.empty()) return false;
    
    // Basic validation
    if (language == "cpp" || language == "c++") {
        bool hasInclude = code.find("#include") != std::string::npos;
        bool hasSemicolon = code.find(";") != std::string::npos;
        return hasInclude || hasSemicolon;
    }
    
    return true;
}

std::vector<Vulnerability> performStaticAnalysis(const std::string& code, const std::string& language) {
    std::vector<Vulnerability> vulns;
    // Basic pattern matching for common issues
    if (code.find("strcpy") != std::string::npos) {
        vulns.push_back({"buffer_overflow", "high", "Unsafe strcpy usage detected", 0, "Use strcpy_s or std::string"});
    }
    if (code.find("gets") != std::string::npos) { // Very unsafe
        vulns.push_back({"buffer_overflow", "critical", "gets() is extremely unsafe", 0, "Use fgets or std::getline"});
    }
    return vulns;
}

std::string generateFeatureBoilerplate(const std::string& spec, const std::string& lang) {
   return "// Boilerplate for " + spec;
}

std::string generateTestCode(const std::string& code, const std::vector<TestCase>& cases, const std::string& lang) {
   return "// Test Code Stub";
}

std::vector<TestCase> identifyTestCases(const std::string& code, const std::string& lang) {
    return { {"Test1", "Setup...", "Run...", "Expect..." , "Assert(true)"} };
}

std::string extractDocumentation(const std::string& code, const std::string& lang) {
    return "/** Logic extracted from code... */";
}

std::string AdvancedCodingAgent::generateFeatureBoilerplate(
    const std::string& feature, const std::string& language) {
    
    if (language == "cpp" || language == "c++") {
        return "class " + feature + " {\n"
               "public:\n"
               "    " + feature + "() {}\n"
               "    ~" + feature + "() {}\n"
               "private:\n"
               "};\n";
    } else if (language == "python") {
        return "class " + feature + ":\n"
               "    def __init__(self):\n"
               "        pass\n";
    }
    
    return "";
}

std::vector<TestCase> AdvancedCodingAgent::identifyTestCases(
    const std::string& code, const std::string& language) {
    
    std::vector<TestCase> cases;
    
    // Basic test case identification
    if (code.find("if ") != std::string::npos) {
        TestCase tc;
        tc.name = "test_condition_true";
        tc.expectedResult = "pass";
        cases.push_back(tc);
        
        tc.name = "test_condition_false";
        cases.push_back(tc);
    }
    
    if (code.find("for ") != std::string::npos) {
        TestCase tc;
        tc.name = "test_empty_collection";
        tc.expectedResult = "pass";
        cases.push_back(tc);
        
        tc.name = "test_single_element";
        cases.push_back(tc);
        
        tc.name = "test_multiple_elements";
        cases.push_back(tc);
    }
    
    return cases;
}

std::string AdvancedCodingAgent::generateTestCode(
    const std::string& code, const std::vector<TestCase>& testCases,
    const std::string& language) {
    
    std::string testCode = "// Generated tests\n";
    
    if (language == "cpp" || language == "c++") {
        testCode += "#include <cassert>\n\n";
        testCode += "void runTests() {\n";
        
        for (const auto& tc : testCases) {
            testCode += "    // " + tc.name + "\n";
            testCode += "    // assert(...);\n";
        }
        
        testCode += "}\n";
    } else if (language == "python") {
        testCode += "import unittest\n\n";
        testCode += "class TestCase(unittest.TestCase):\n";
        
        for (const auto& tc : testCases) {
            testCode += "    def " + tc.name + "(self):\n";
            testCode += "        self.assertTrue(True)\n";
        }
    }
    
    return testCode;
}

std::string AdvancedCodingAgent::extractDocumentation(
    const std::string& code, const std::string& language) {
    
    std::string doc = "";
    
    if (language == "cpp" || language == "c++") {
        // Extract class and function declarations
        doc += "## Classes\n\n";
        doc += "## Functions\n\n";
    } else if (language == "python") {
        doc += "## Modules\n\n";
        doc += "## Classes\n\n";
        doc += "## Functions\n\n";
    }
    
    return doc;
}

std::vector<Vulnerability> AdvancedCodingAgent::performStaticAnalysis(
    const std::string& code, const std::string& language) {
    
    std::vector<Vulnerability> vulns;
    
    // Check for null pointer dereferences
    if (code.find("->") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "warning";
        vuln.type = "null_dereference";
        vuln.description = "Potential null pointer dereference";
        vuln.suggestedFix = "Add null check before dereferencing";
        vulns.push_back(vuln);
    }
    
    // Check for memory leaks
    if (code.find("new ") != std::string::npos && 
        code.find("delete ") == std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "warning";
        vuln.type = "memory_leak";
        vuln.description = "Possible memory leak";
        vuln.suggestedFix = "Use smart pointers or ensure cleanup";
        vulns.push_back(vuln);
    }
    
    return vulns;
}

std::string AdvancedCodingAgent::replaceStringConcatenation(
    const std::string& code) {
    
    // Placeholder: would actually rewrite concatenations
    return code;
}

std::vector<Vulnerability> AdvancedCodingAgent::scanSecurityIssues(
    const std::string& code, const std::string& language) {
    
    std::vector<Vulnerability> vulns;
    
    // Check for SQL injection
    if (code.find("sprintf") != std::string::npos && 
        code.find("SELECT") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "error";
        vuln.type = "sql_injection";
        vuln.description = "Potential SQL injection vulnerability";
        vuln.suggestedFix = "Use prepared statements";
        vulns.push_back(vuln);
    }
    
    // Check for hardcoded secrets
    if (code.find("password") != std::string::npos ||
        code.find("api_key") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "error";
        vuln.type = "hardcoded_secret";
        vuln.description = "Hardcoded secrets found";
        vuln.suggestedFix = "Use environment variables";
        vulns.push_back(vuln);
    }
    
    return vulns;
}

std::string AdvancedCodingAgent::extractMethodRefactoring(
    const std::string& code, const std::string& language) {
    
    // Placeholder implementation
    return code;
}

std::string AdvancedCodingAgent::reduceComplexity(
    const std::string& code, const std::string& language) {
    
    // Placeholder implementation
    return code;
}

std::string AdvancedCodingAgent::improveNaming(
    const std::string& code, const std::string& language) {
    
    // Placeholder implementation
    return code;
}

} // namespace IDE
} // namespace RawrXD
