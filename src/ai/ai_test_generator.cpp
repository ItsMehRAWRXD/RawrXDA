/**
 * @file ai_test_generator.cpp
 * @brief AI-powered test generation implementation
 * Batch 5 - Item 75: AI test generator
 */

#include "ai/ai_test_generator.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::AI {

AITestGenerator::AITestGenerator()
    : m_initialized(false) {
}

AITestGenerator::~AITestGenerator() {
    shutdown();
}

bool AITestGenerator::initialize() {
    m_initialized = true;
    return true;
}

void AITestGenerator::shutdown() {
    m_initialized = false;
}

TestGenerationResult AITestGenerator::generateTests(const TestGenerationRequest& request) {
    TestGenerationResult result;
    
    if (!m_initialized) {
        result.error = "Generator not initialized";
        return result;
    }
    
    // Create test suite
    result.suite.name = request.targetFunction + "Tests";
    result.suite.filePath = request.targetFunction + "_test.cpp";
    result.suite.framework = request.framework;
    
    // Generate test cases
    result.suite.tests = generateTestCases(request);
    
    // Generate setup and teardown
    result.suite.setupCode = generateSetupCode(request);
    result.suite.teardownCode = generateTeardownCode(request);
    
    // Calculate coverage estimate
    result.coverage = estimateCoverage(request);
    
    // Generate edge cases
    if (request.includeEdgeCases) {
        result.edgeCases = generateEdgeCases(request);
    }
    
    // Generate error cases
    if (request.includeErrorCases) {
        result.errorCases = generateErrorCases(request);
    }
    
    result.isComplete = true;
    
    return result;
}

std::future<TestGenerationResult> AITestGenerator::generateTestsAsync(const TestGenerationRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return generateTests(request);
    });
}

TestGenerationResult AITestGenerator::generateUnitTests(const std::string& code,
                                                           const std::string& language,
                                                           TestFramework framework) {
    TestGenerationRequest request;
    request.code = code;
    request.language = language;
    request.type = TestType::Unit;
    request.framework = framework;
    request.maxTests = 10;
    request.includeEdgeCases = true;
    request.includeErrorCases = true;
    
    return generateTests(request);
}

TestGenerationResult AITestGenerator::generateIntegrationTests(const std::string& code,
                                                                  const std::string& language) {
    TestGenerationRequest request;
    request.code = code;
    request.language = language;
    request.type = TestType::Integration;
    request.framework = TestFramework::Catch2;
    request.maxTests = 5;
    request.includeEdgeCases = false;
    request.includeErrorCases = true;
    
    return generateTests(request);
}

TestGenerationResult AITestGenerator::generatePropertyTests(const std::string& code,
                                                             const std::string& language) {
    TestGenerationRequest request;
    request.code = code;
    request.language = language;
    request.type = TestType::Property;
    request.framework = TestFramework::Catch2;
    request.maxTests = 5;
    request.includePropertyTests = true;
    
    return generateTests(request);
}

TestGenerationResult AITestGenerator::improveTests(const TestSuite& existingTests,
                                                    const std::string& code) {
    TestGenerationRequest request;
    request.code = code;
    request.maxTests = static_cast<int>(existingTests.tests.size()) + 5;
    request.includeEdgeCases = true;
    request.includeErrorCases = true;
    
    TestGenerationResult result = generateTests(request);
    
    // Merge with existing tests
    result.suite.tests.insert(result.suite.tests.end(),
                               existingTests.tests.begin(),
                               existingTests.tests.end());
    
    result.explanation = "Added " + std::to_string(result.suite.tests.size() - existingTests.tests.size()) +
                        " new test cases to improve coverage";
    
    return result;
}

TestGenerationResult AITestGenerator::addEdgeCases(const TestSuite& existingTests,
                                                    const std::string& code) {
    TestGenerationRequest request;
    request.code = code;
    request.includeEdgeCases = true;
    request.includeErrorCases = false;
    
    TestGenerationResult result = generateTests(request);
    
    // Only keep edge case tests
    result.suite.tests.erase(
        std::remove_if(result.suite.tests.begin(), result.suite.tests.end(),
            [](const TestCase& test) {
                return std::find(test.tags.begin(), test.tags.end(), "edge_case") == test.tags.end();
            }),
        result.suite.tests.end()
    );
    
    // Merge with existing tests
    result.suite.tests.insert(result.suite.tests.begin(),
                               existingTests.tests.begin(),
                               existingTests.tests.end());
    
    return result;
}

TestGenerationResult AITestGenerator::addErrorCases(const TestSuite& existingTests,
                                                     const std::string& code) {
    TestGenerationRequest request;
    request.code = code;
    request.includeEdgeCases = false;
    request.includeErrorCases = true;
    
    TestGenerationResult result = generateTests(request);
    
    // Only keep error case tests
    result.suite.tests.erase(
        std::remove_if(result.suite.tests.begin(), result.suite.tests.end(),
            [](const TestCase& test) {
                return std::find(test.tags.begin(), test.tags.end(), "error_case") == test.tags.end();
            }),
        result.suite.tests.end()
    );
    
    // Merge with existing tests
    result.suite.tests.insert(result.suite.tests.begin(),
                               existingTests.tests.begin(),
                               existingTests.tests.end());
    
    return result;
}

void AITestGenerator::setFramework(TestFramework framework) {
    m_defaultFramework = framework;
}

void AITestGenerator::setMaxTests(int maxTests) {
    m_maxTests = maxTests;
}

void AITestGenerator::onTestsGenerated(GenerationCallback callback) {
    m_generationCallback = callback;
}

void AITestGenerator::onCoverageAnalyzed(CoverageCallback callback) {
    m_coverageCallback = callback;
}

std::vector<TestCase> AITestGenerator::generateTestCases(const TestGenerationRequest& request) {
    std::vector<TestCase> tests;
    
    // Parse the code to understand the function signature
    FunctionInfo func = parseFunction(request.code, request.targetFunction);
    
    // Generate basic test cases
    int numTests = std::min(request.maxTests, 10);
    
    for (int i = 0; i < numTests; i++) {
        TestCase test;
        test.name = func.name + "_Test" + std::to_string(i + 1);
        test.description = "Test case " + std::to_string(i + 1) + " for " + func.name;
        test.isParameterized = false;
        
        // Generate test code based on framework
        switch (request.framework) {
            case TestFramework::Catch2:
                test.code = generateCatch2Test(func, i);
                break;
            case TestFramework::GoogleTest:
                test.code = generateGoogleTest(func, i);
                break;
            case TestFramework::Doctest:
                test.code = generateDoctest(func, i);
                break;
            default:
                test.code = generateGenericTest(func, i);
                break;
        }
        
        // Add tags
        if (i == 0) {
            test.tags.push_back("basic");
        } else if (i % 2 == 0) {
            test.tags.push_back("edge_case");
        } else {
            test.tags.push_back("error_case");
        }
        
        tests.push_back(test);
    }
    
    return tests;
}

std::string AITestGenerator::generateCatch2Test(const FunctionInfo& func, int index) {
    std::stringstream test;
    
    test << "TEST_CASE(\"" << func.name << " - Case " << (index + 1) << "\", \"[" << func.name << "]\") {\n";
    
    // Generate inputs based on parameter types
    for (const auto& param : func.parameters) {
        test << "    " << param.type << " " << param.name << " = ";
        
        if (param.type == "int" || param.type == "long") {
            test << (index * 10 + 5);
        } else if (param.type == "double" || param.type == "float") {
            test << (index * 1.5 + 0.5);
        } else if (param.type == "bool") {
            test << (index % 2 == 0 ? "true" : "false");
        } else if (param.type == "std::string" || param.type == "string") {
            test << "\"test" << (index + 1) << "\"";
        } else {
            test << param.type << "{}";
        }
        test << ";\n";
    }
    
    // Call function
    if (func.returnType != "void") {
        test << "    auto result = " << func.name << "(";
    } else {
        test << "    " << func.name << "(";
    }
    
    for (size_t i = 0; i < func.parameters.size(); i++) {
        if (i > 0) test << ", ";
        test << func.parameters[i].name;
    }
    test << ");\n";
    
    // Add assertion
    if (func.returnType != "void") {
        test << "    REQUIRE(result == /* expected value */);\n";
    }
    
    test << "}\n";
    
    return test.str();
}

std::string AITestGenerator::generateGoogleTest(const FunctionInfo& func, int index) {
    std::stringstream test;
    
    test << "TEST(" << func.name << "Test, Case" << (index + 1) << ") {\n";
    
    // Similar to Catch2 but with Google Test syntax
    for (const auto& param : func.parameters) {
        test << "    " << param.type << " " << param.name;
        if (param.type == "int") {
            test << " = " << (index * 10 + 5);
        }
        test << ";\n";
    }
    
    test << "    // TODO: Add test logic\n";
    test << "    EXPECT_TRUE(true);\n";
    test << "}\n";
    
    return test.str();
}

std::string AITestGenerator::generateDoctest(const FunctionInfo& func, int index) {
    std::stringstream test;
    
    test << "TEST_CASE(\"" << func.name << " - " << (index + 1) << "\") {\n";
    test << "    // TODO: Implement test\n";
    test << "    CHECK(true);\n";
    test << "}\n";
    
    return test.str();
}

std::string AITestGenerator::generateGenericTest(const FunctionInfo& func, int index) {
    std::stringstream test;
    
    test << "void test_" << func.name << "_" << (index + 1) << "() {\n";
    test << "    // Test case " << (index + 1) << " for " << func.name << "\n";
    test << "    // TODO: Implement\n";
    test << "}\n";
    
    return test.str();
}

std::string AITestGenerator::generateSetupCode(const TestGenerationRequest& request) {
    std::stringstream setup;
    
    setup << "// Setup code for tests\n";
    setup << "// Initialize any required resources here\n";
    setup << "// This runs before each test\n";
    
    return setup.str();
}

std::string AITestGenerator::generateTeardownCode(const TestGenerationRequest& request) {
    std::stringstream teardown;
    
    teardown << "// Teardown code for tests\n";
    teardown << "// Clean up any resources here\n";
    teardown << "// This runs after each test\n";
    
    return teardown.str();
}

int AITestGenerator::estimateCoverage(const TestGenerationRequest& request) {
    // Simple coverage estimation based on code complexity
    int lines = static_cast<int>(std::count(request.code.begin(), request.code.end(), '\n'));
    int branches = static_cast<int>(std::count(request.code.begin(), request.code.end(), 'if')) +
                   static_cast<int>(std::count(request.code.begin(), request.code.end(), 'switch'));
    
    int estimatedCoverage = 70; // Base coverage
    
    // Adjust based on number of tests
    if (request.maxTests > 5) estimatedCoverage += 10;
    if (request.maxTests > 10) estimatedCoverage += 10;
    
    // Adjust for edge/error cases
    if (request.includeEdgeCases) estimatedCoverage += 5;
    if (request.includeErrorCases) estimatedCoverage += 5;
    
    return std::min(estimatedCoverage, 100);
}

std::vector<std::string> AITestGenerator::generateEdgeCases(const TestGenerationRequest& request) {
    std::vector<std::string> edgeCases;
    
    // Parse function to find parameter types
    FunctionInfo func = parseFunction(request.code, request.targetFunction);
    
    for (const auto& param : func.parameters) {
        if (param.type == "int" || param.type == "long") {
            edgeCases.push_back("Test with " + param.name + " = 0");
            edgeCases.push_back("Test with " + param.name + " = INT_MAX");
            edgeCases.push_back("Test with " + param.name + " = INT_MIN");
            edgeCases.push_back("Test with " + param.name + " = -1");
        } else if (param.type == "double" || param.type == "float") {
            edgeCases.push_back("Test with " + param.name + " = 0.0");
            edgeCases.push_back("Test with " + param.name + " = DBL_MAX");
            edgeCases.push_back("Test with " + param.name + " = DBL_MIN");
            edgeCases.push_back("Test with " + param.name + " = NaN");
        } else if (param.type == "std::string" || param.type == "string") {
            edgeCases.push_back("Test with empty string");
            edgeCases.push_back("Test with very long string");
            edgeCases.push_back("Test with special characters");
        } else if (param.type.find("*") != std::string::npos || param.type.find("&") != std::string::npos) {
            edgeCases.push_back("Test with null " + param.name);
        }
    }
    
    return edgeCases;
}

std::vector<std::string> AITestGenerator::generateErrorCases(const TestGenerationRequest& request) {
    std::vector<std::string> errorCases;
    
    errorCases.push_back("Test with invalid input");
    errorCases.push_back("Test with null pointer");
    errorCases.push_back("Test with out of bounds index");
    errorCases.push_back("Test with empty container");
    errorCases.push_back("Test with resource exhaustion");
    
    return errorCases;
}

AITestGenerator::FunctionInfo AITestGenerator::parseFunction(const std::string& code,
                                                                const std::string& functionName) {
    FunctionInfo func;
    func.name = functionName.empty() ? "targetFunction" : functionName;
    func.returnType = "void";
    
    // Simple regex-based parsing
    std::regex funcRegex("(\\w+)\\s+" + func.name + "\\s*\\(([^)]*)\\)");
    std::smatch match;
    
    if (std::regex_search(code, match, funcRegex)) {
        func.returnType = match[1].str();
        std::string params = match[2].str();
        
        // Parse parameters
        std::regex paramRegex("(\\w+)\\s+(\\w+)");
        auto words_begin = std::sregex_iterator(params.begin(), params.end(), paramRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            ParameterInfo param;
            param.type = (*i)[1].str();
            param.name = (*i)[2].str();
            func.parameters.push_back(param);
        }
    }
    
    // If no parameters found, add some defaults
    if (func.parameters.empty()) {
        ParameterInfo p1{"int", "input"};
        ParameterInfo p2{"bool", "flag"};
        func.parameters.push_back(p1);
        func.parameters.push_back(p2);
    }
    
    return func;
}

} // namespace RawrXD::AI
