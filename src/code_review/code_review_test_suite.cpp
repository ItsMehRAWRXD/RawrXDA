// ============================================================================
// code_review_test_suite.cpp — Test Suite for Code Review System
// ============================================================================
// Comprehensive tests for security analyzer and code review engine.
//
// Build (MSVC): cl /std:c++20 /EHsc code_review_test_suite.cpp 
//   security_analyzer.cpp code_review_engine.cpp /link
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "code_review/security_analyzer.h"
#include "code_review/code_review_engine.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace RawrXD::CodeReview;

// ============================================================================
// Test Framework
// ============================================================================

class TestRunner {
public:
    TestRunner(const std::string& name) 
        : m_name(name), m_passed(0), m_failed(0) {}
    
    void test(const std::string& testName, bool condition) {
        if (condition) {
            std::cout << "  ✓ " << testName << std::endl;
            m_passed++;
        } else {
            std::cout << "  ✗ " << testName << std::endl;
            m_failed++;
        }
    }
    
    void report() {
        int total = m_passed + m_failed;
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << m_name << ": " << m_passed << "/" << total << " PASSED";
        if (m_failed > 0) {
            std::cout << " (" << m_failed << " FAILED)";
        }
        std::cout << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    bool allPassed() const { return m_failed == 0; }

private:
    std::string m_name;
    int m_passed;
    int m_failed;
};

// ============================================================================
// Security Analyzer Tests
// ============================================================================

void testSecurityAnalyzer() {
    TestRunner runner("SecurityAnalyzer");
    
    SecurityAnalyzer analyzer;
    
    // Test 1: SQL Injection Detection
    {
        std::string code = R"(
            void query(const char* user) {
                char sql[256];
                sprintf(sql, "SELECT * FROM users WHERE name='%s'", user);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("SQL Injection detected", 
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::SQLInjection; 
                }));
    }
    
    // Test 2: Command Injection Detection
    {
        std::string code = R"(
            void run(const char* cmd) {
                system(cmd);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Command Injection detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::CommandInjection; 
                }));
    }
    
    // Test 3: Hardcoded Credentials
    {
        std::string code = R"(
            const char* password = "SuperSecret123!";
            const char* apiKey = "sk-1234567890abcdef1234567890abcdef";
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Hardcoded credentials detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::HardcodedSecret; 
                }));
    }
    
    // Test 4: Buffer Overflow Detection
    {
        std::string code = R"(
            void copy(const char* src) {
                char dest[100];
                strcpy(dest, src);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Buffer Overflow detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::BufferOverflow; 
                }));
    }
    
    // Test 5: Weak Cryptography
    {
        std::string code = R"(
            void hash(const char* data) {
                MD5(data);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Weak crypto detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::WeakCrypto; 
                }));
    }
    
    // Test 6: Path Traversal
    {
        std::string code = R"(
            void readFile(const char* path) {
                std::string fullPath = "/data/" + std::string(path);
                std::ifstream file(fullPath);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Path Traversal detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::PathTraversal; 
                }));
    }
    
    // Test 7: Format String
    {
        std::string code = R"(
            void log(const char* msg) {
                printf(msg);
            }
        )";
        
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        runner.test("Format String detected",
            std::any_of(result.findings.begin(), result.findings.end(),
                [](const auto& f) { 
                    return f.category == VulnerabilityCategory::FormatString; 
                }));
    }
    
    // Test 8: Statistics tracking
    {
        analyzer.resetStats();
        analyzer.analyzeFile("test.cpp", "int x = 1;", "cpp");
        auto stats = analyzer.getStats();
        runner.test("Statistics tracked", stats.filesAnalyzed == 1);
    }
    
    // Test 9: Rule management
    {
        auto rules = analyzer.getAvailableRules();
        runner.test("Rules registered", !rules.empty());
        
        analyzer.unregisterRule("SEC001");
        auto rulesAfter = analyzer.getAvailableRules();
        runner.test("Rule unregistered", rulesAfter.size() == rules.size() - 1);
    }
    
    // Test 10: JSON export
    {
        std::string code = "system(\"ls\");";
        auto result = analyzer.analyzeFile("test.cpp", code, "cpp");
        std::string json = analyzer.exportToJson({result});
        runner.test("JSON export works", !json.empty() && json.find("test.cpp") != std::string::npos);
    }
    
    runner.report();
}

// ============================================================================
// Code Review Engine Tests
// ============================================================================

void testCodeReviewEngine() {
    TestRunner runner("CodeReviewEngine");
    
    SecurityAnalyzer securityAnalyzer;
    CodeReviewEngine engine(&securityAnalyzer);
    
    // Test 1: Long Method Detection
    {
        std::string code = R"(
            void longMethod() {
                // Line 1
                int x = 1;
                int y = 2;
                int z = 3;
                int a = 4;
                int b = 5;
                int c = 6;
                int d = 7;
                int e = 8;
                int f = 9;
                int g = 10;
                int h = 11;
                int i = 12;
                int j = 13;
                int k = 14;
                int l = 15;
                int m = 16;
                int n = 17;
                int o = 18;
                int p = 19;
                int q = 20;
                int r = 21;
                int s = 22;
                int t = 23;
                int u = 24;
                int v = 25;
                int w = 26;
                int xx = 27;
                int yy = 28;
                int zz = 29;
                int aa = 30;
                int bb = 31;
                int cc = 32;
                int dd = 33;
                int ee = 34;
                int ff = 35;
                int gg = 36;
                int hh = 37;
                int ii = 38;
                int jj = 39;
                int kk = 40;
                int ll = 41;
                int mm = 42;
                int nn = 43;
                int oo = 44;
                int pp = 45;
                int qq = 46;
                int rr = 47;
                int ss = 48;
                int tt = 49;
                int uu = 50;
                int vv = 51;
                int ww = 52;
            }
        )";
        
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Long method detected",
            std::any_of(result.codeSmells.begin(), result.codeSmells.end(),
                [](const auto& s) { return s.type == CodeSmellType::LongMethod; }));
    }
    
    // Test 2: Deep Nesting Detection
    {
        std::string code = R"(
            void nested() {
                if (true) {
                    if (true) {
                        if (true) {
                            if (true) {
                                if (true) {
                                    int x = 1;
                                }
                            }
                        }
                    }
                }
            }
        )";
        
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Deep nesting detected",
            std::any_of(result.codeSmells.begin(), result.codeSmells.end(),
                [](const auto& s) { return s.type == CodeSmellType::DeepNesting; }));
    }
    
    // Test 3: Long Parameter List
    {
        std::string code = R"(
            void manyParams(int a, int b, int c, int d, int e, int f, int g, int h) {
            }
        )";
        
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Long parameter list detected",
            std::any_of(result.codeSmells.begin(), result.codeSmells.end(),
                [](const auto& s) { return s.type == CodeSmellType::LongParameterList; }));
    }
    
    // Test 4: Metrics calculation
    {
        std::string code = R"(
            int add(int a, int b) {
                return a + b;
            }
            
            int multiply(int a, int b) {
                return a * b;
            }
        )";
        
        auto metrics = engine.calculateMetrics(code, "cpp");
        runner.test("Metrics calculated", metrics.linesOfCode > 0);
        runner.test("Function count correct", metrics.functionCount >= 2);
    }
    
    // Test 5: Complexity calculation
    {
        std::string code = R"(
            int complex(int x) {
                if (x > 0) {
                    if (x < 10) {
                        for (int i = 0; i < x; i++) {
                            if (i % 2 == 0) {
                                return i;
                            }
                        }
                    }
                }
                return -1;
            }
        )";
        
        auto metrics = engine.calculateMetrics(code, "cpp");
        runner.test("Cyclomatic complexity calculated", 
            metrics.cyclomaticComplexity > 1);
        runner.test("Nesting depth calculated", metrics.nestingDepth > 0);
    }
    
    // Test 6: Security integration
    {
        std::string code = R"(
            void vulnerable(const char* input) {
                char buffer[100];
                strcpy(buffer, input);
            }
        )";
        
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Security findings integrated", 
            !result.securityFindings.empty());
    }
    
    // Test 7: Score calculation
    {
        std::string code = "int x = 1;";
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Overall score calculated", result.overallScore >= 0.0f);
        runner.test("Security score calculated", result.securityScore >= 0.0f);
        runner.test("Maintainability score calculated", 
            result.maintainabilityScore >= 0.0f);
    }
    
    // Test 8: Suggestions generation
    {
        std::string code = R"(
            void longFunc() {
                int x = 1; int y = 2; int z = 3;
                int a = 1; int b = 2; int c = 3;
                int d = 1; int e = 2; int f = 3;
                int g = 1; int h = 2; int i = 3;
                int j = 1; int k = 2; int l = 3;
                int m = 1; int n = 2; int o = 3;
                int p = 1; int q = 2; int r = 3;
                int s = 1; int t = 2; int u = 3;
                int v = 1; int w = 2; int xx = 3;
                int yy = 1; int zz = 2; int aaa = 3;
            }
        )";
        
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        runner.test("Suggestions generated", !result.suggestions.empty());
    }
    
    // Test 9: Statistics tracking
    {
        engine.resetStats();
        engine.reviewFile("test.cpp", "int x = 1;", "cpp");
        auto stats = engine.getStats();
        runner.test("Review stats tracked", stats.filesReviewed == 1);
    }
    
    // Test 10: Markdown export
    {
        std::string code = "int x = 1;";
        auto result = engine.reviewFile("test.cpp", code, "cpp");
        std::string md = engine.exportToMarkdown({result});
        runner.test("Markdown export works", 
            !md.empty() && md.find("Code Review Report") != std::string::npos);
    }
    
    runner.report();
}

// ============================================================================
// Complexity Calculator Tests
// ============================================================================

void testComplexityCalculator() {
    TestRunner runner("ComplexityCalculator");
    
    // Test 1: Simple code
    {
        std::string code = "int x = 1;";
        auto complexity = ComplexityCalculator::calculateCyclomatic(code, "cpp");
        runner.test("Simple code complexity", complexity == 1);
    }
    
    // Test 2: If statement
    {
        std::string code = R"(
            if (x > 0) {
                return 1;
            }
        )";
        auto complexity = ComplexityCalculator::calculateCyclomatic(code, "cpp");
        runner.test("If statement complexity", complexity == 2);
    }
    
    // Test 3: Multiple branches
    {
        std::string code = R"(
            if (x > 0) {
                return 1;
            } else if (x < 0) {
                return -1;
            } else {
                return 0;
            }
        )";
        auto complexity = ComplexityCalculator::calculateCyclomatic(code, "cpp");
        runner.test("Multiple branches complexity", complexity >= 3);
    }
    
    // Test 4: Loop
    {
        std::string code = R"(
            for (int i = 0; i < 10; i++) {
                sum += i;
            }
        )";
        auto complexity = ComplexityCalculator::calculateCyclomatic(code, "cpp");
        runner.test("Loop complexity", complexity == 2);
    }
    
    // Test 5: Nesting depth
    {
        std::string code = R"(
            void nested() {
                if (true) {
                    if (true) {
                        if (true) {
                            int x = 1;
                        }
                    }
                }
            }
        )";
        auto depth = ComplexityCalculator::calculateNestingDepth(code, "cpp");
        runner.test("Nesting depth calculation", depth == 4);
    }
    
    // Test 6: Halstead metrics
    {
        std::string code = "int x = a + b * c;";
        auto halstead = ComplexityCalculator::calculateHalstead(code, "cpp");
        runner.test("Halstead volume calculated", halstead.volume > 0);
        runner.test("Halstead difficulty calculated", halstead.difficulty > 0);
    }
    
    // Test 7: Maintainability index
    {
        float mi = ComplexityCalculator::calculateMaintainabilityIndex(5, 100, 50);
        runner.test("Maintainability index calculated", mi >= 0.0f && mi <= 100.0f);
    }
    
    runner.report();
}

// ============================================================================
// Integration Tests
// ============================================================================

void testIntegration() {
    TestRunner runner("Integration");
    
    SecurityAnalyzer analyzer;
    CodeReviewEngine engine(&analyzer);
    
    // Test 1: Full review workflow
    {
        std::string code = R"(
            #include <cstdio>
            #include <cstring>
            
            class DataProcessor {
            public:
                void process(const char* input, const char* filename) {
                    char buffer[256];
                    strcpy(buffer, input);
                    
                    char cmd[512];
                    sprintf(cmd, "cat %s", filename);
                    system(cmd);
                    
                    const char* password = "secret123";
                    printf(password);
                }
                
                void longMethod() {
                    int x = 1; int y = 2; int z = 3;
                    int a = 1; int b = 2; int c = 3;
                    int d = 1; int e = 2; int f = 3;
                    int g = 1; int h = 2; int i = 3;
                    int j = 1; int k = 2; int l = 3;
                    int m = 1; int n = 2; int o = 3;
                    int p = 1; int q = 2; int r = 3;
                    int s = 1; int t = 2; int u = 3;
                    int v = 1; int w = 2; int xx = 3;
                    int yy = 1; int zz = 2; int aaa = 3;
                }
            };
        )";
        
        auto result = engine.reviewFile("vulnerable.cpp", code, "cpp");
        
        runner.test("Full review completes", result.success);
        runner.test("Security issues found", !result.securityFindings.empty());
        runner.test("Code smells found", !result.codeSmells.empty());
        runner.test("Suggestions generated", !result.suggestions.empty());
        runner.test("Metrics calculated", result.metrics.linesOfCode > 0);
        runner.test("Scores calculated", result.overallScore >= 0.0f);
        
        std::cout << "\n  Summary:\n";
        std::cout << "    - Security issues: " << result.securityFindings.size() << "\n";
        std::cout << "    - Code smells: " << result.codeSmells.size() << "\n";
        std::cout << "    - Suggestions: " << result.suggestions.size() << "\n";
        std::cout << "    - Overall score: " << std::fixed << std::setprecision(1) 
                  << result.overallScore << "/100\n";
        std::cout << "    - Security score: " << result.securityScore << "/100\n";
        std::cout << "    - Maintainability: " << result.maintainabilityScore << "/100\n";
    }
    
    // Test 2: Multi-file review
    {
        std::vector<std::string> files = {
            "int x = 1;",
            "int y = 2;",
            "int z = 3;"
        };
        
        std::vector<ReviewResult> results;
        for (size_t i = 0; i < files.size(); ++i) {
            results.push_back(engine.reviewFile("file" + std::to_string(i) + ".cpp", 
                                                 files[i], "cpp"));
        }
        
        runner.test("Multi-file review works", results.size() == 3);
        
        auto stats = engine.getStats();
        runner.test("Stats accumulated", stats.filesReviewed >= 3);
    }
    
    runner.report();
}

// ============================================================================
// Performance Tests
// ============================================================================

void testPerformance() {
    TestRunner runner("Performance");
    
    SecurityAnalyzer analyzer;
    CodeReviewEngine engine(&analyzer);
    
    // Generate large file
    std::ostringstream oss;
    for (int i = 0; i < 1000; ++i) {
        oss << "void function" << i << "() { int x = " << i << "; }\n";
    }
    std::string largeFile = oss.str();
    
    // Test 1: Large file analysis time
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = analyzer.analyzeFile("large.cpp", largeFile, "cpp");
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        runner.test("Large file analysis <5s", elapsed < 5000);
        std::cout << "    Analysis time: " << elapsed << "ms for 1000 functions\n";
    }
    
    // Test 2: Large file review time
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine.reviewFile("large.cpp", largeFile, "cpp");
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        runner.test("Large file review <10s", elapsed < 10000);
        std::cout << "    Review time: " << elapsed << "ms for 1000 functions\n";
    }
    
    runner.report();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "========================================" << "\n";
    std::cout << "Code Review System Test Suite" << "\n";
    std::cout << "========================================" << "\n\n";
    
    testSecurityAnalyzer();
    std::cout << "\n";
    
    testCodeReviewEngine();
    std::cout << "\n";
    
    testComplexityCalculator();
    std::cout << "\n";
    
    testIntegration();
    std::cout << "\n";
    
    testPerformance();
    std::cout << "\n";
    
    std::cout << "========================================" << "\n";
    std::cout << "All Tests Complete" << "\n";
    std::cout << "========================================" << "\n";
    
    return 0;
}
