#include <iostream>
#include <cassert>
#include "AICodeIntelligence.hpp"

void test_basic_analysis() {
    AICodeIntelligence intelligence;
    
    // Test code with security vulnerability
    std::string vulnerableCode = R"(
        char buffer[10];
        strcpy(buffer, userInput);  // SQL injection vulnerability
        sprintf(output, "%s", buffer);  // Format string vulnerability
    )";
    
    // Test security detection
    bool isSec = intelligence.isSecurityVulnerability(vulnerableCode, "cpp");
    std::cout << "Security vulnerability detected: " << (isSec ? "YES" : "NO") << std::endl;
    
    // Test performance detection
    std::string perfCode = R"(
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                data[i][j] = process(i, j);
            }
        }
    )";
    
    bool isPerfIssue = intelligence.isPerformanceIssue(perfCode, "cpp");
    std::cout << "Performance issue detected: " << (isPerfIssue ? "YES" : "NO") << std::endl;
    
    // Test pattern detection
    auto patterns = intelligence.detectPatterns(vulnerableCode, "cpp");
    std::cout << "Patterns detected: " << patterns.size() << std::endl;
}

void test_code_quality_metrics() {
    AICodeIntelligence intelligence;
    
    std::string testCode = R"(
        #include <iostream>
        
        class Calculator {
        public:
            int add(int a, int b) {
                return a + b;
            }
            
            int multiply(int a, int b) {
                return a * b;
            }
        };
        
        int main() {
            Calculator calc;
            int result = calc.add(5, 3);
            std::cout << result << std::endl;
            return 0;
        }
    )";
    
    // Create a temporary test file
    std::ofstream testFile("test_code.cpp");
    testFile << testCode;
    testFile.close();
    
    // Test metrics calculation
    auto metrics = intelligence.calculateCodeMetrics("test_code.cpp");
    std::cout << "\nCode Metrics:" << std::endl;
    for (const auto& [key, value] : metrics) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    // Test code quality prediction
    auto quality = intelligence.predictCodeQuality("test_code.cpp");
    std::cout << "\nCode Quality:" << std::endl;
    for (const auto& [key, value] : quality) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    // Test best practices
    auto practices = intelligence.getEnterpriseBestPractices("cpp");
    std::cout << "\nBest Practices for C++:" << std::endl;
    for (const auto& [key, value] : practices) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    // Cleanup
    std::remove("test_code.cpp");
}

void test_security_analysis() {
    AICodeIntelligence intelligence;
    
    std::string sqlInjectionCode = R"(
        std::string query = "SELECT * FROM users WHERE id = " + userInput;
        executeQuery(query);  // SQL injection
    )";
    
    std::string xssCode = R"(
        response.write("<script>" + userInput + "</script>");  // XSS
    )";
    
    std::string bufferOverflow = R"(
        char buffer[16];
        gets(buffer);  // Buffer overflow
    )";
    
    std::ofstream file("sec_test.cpp");
    file << sqlInjectionCode;
    file.close();
    
    auto secInsights = intelligence.analyzeSecurity("sec_test.cpp");
    std::cout << "\nSecurity Issues Found:" << std::endl;
    std::cout << "Total: " << secInsights.size() << std::endl;
    
    for (const auto& insight : secInsights) {
        std::cout << "  Type: " << insight.type << std::endl;
        std::cout << "  Severity: " << insight.severity << std::endl;
        std::cout << "  Description: " << insight.description << std::endl;
    }
    
    // Test security report generation
    auto report = intelligence.generateSecurityReport(".");
    std::cout << "\nSecurity Report:" << std::endl;
    for (const auto& [key, value] : report) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    std::remove("sec_test.cpp");
}

void test_performance_analysis() {
    AICodeIntelligence intelligence;
    
    std::string perfCode = R"(
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                result += arr[i] + arr[j];
            }
        }
        
        std::vector<std::string> strs;
        std::string combined = "";
        for (const auto& s : strs) {
            combined += s;  // String concatenation in loop
        }
    )";
    
    std::ofstream file("perf_test.cpp");
    file << perfCode;
    file.close();
    
    auto perfInsights = intelligence.analyzePerformance("perf_test.cpp");
    std::cout << "\nPerformance Issues Found:" << std::endl;
    std::cout << "Total: " << perfInsights.size() << std::endl;
    
    for (const auto& insight : perfInsights) {
        std::cout << "  Type: " << insight.type << std::endl;
        std::cout << "  Description: " << insight.description << std::endl;
        std::cout << "  Suggestion: " << insight.suggestion << std::endl;
    }
    
    auto perfReport = intelligence.generatePerformanceReport(".");
    std::cout << "\nPerformance Report:" << std::endl;
    for (const auto& [key, value] : perfReport) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    std::remove("perf_test.cpp");
}

void test_maintainability_analysis() {
    AICodeIntelligence intelligence;
    
    std::string maintCode = R"(
        int complexFunction(int x, int y, int z) {
            if (x > 10) {
                if (y < 5) {
                    if (z == 0) {
                        return x + y;
                    } else if (z == 1) {
                        return x - y;
                    } else if (z == 2) {
                        return x * y;
                    }
                } else {
                    return y * z;
                }
            }
            return 0;
        }
    )";
    
    std::ofstream file("maint_test.cpp");
    file << maintCode;
    file.close();
    
    auto maintInsights = intelligence.analyzeMaintainability("maint_test.cpp");
    std::cout << "\nMaintainability Issues Found:" << std::endl;
    std::cout << "Total: " << maintInsights.size() << std::endl;
    
    auto maintIndex = intelligence.calculateMaintainabilityIndex("maint_test.cpp");
    std::cout << "\nMaintainability Index:" << std::endl;
    for (const auto& [key, value] : maintIndex) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    auto maintReport = intelligence.generateMaintainabilityReport(".");
    std::cout << "\nMaintainability Report:" << std::endl;
    for (const auto& [key, value] : maintReport) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    
    std::remove("maint_test.cpp");
}

int main() {
    std::cout << "=== AICodeIntelligence Integration Test ===" << std::endl;
    
    try {
        std::cout << "\n1. Testing basic analysis..." << std::endl;
        test_basic_analysis();
        
        std::cout << "\n2. Testing code quality metrics..." << std::endl;
        test_code_quality_metrics();
        
        std::cout << "\n3. Testing security analysis..." << std::endl;
        test_security_analysis();
        
        std::cout << "\n4. Testing performance analysis..." << std::endl;
        test_performance_analysis();
        
        std::cout << "\n5. Testing maintainability analysis..." << std::endl;
        test_maintainability_analysis();
        
        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
