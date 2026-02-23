#include <iostream>
#include <string>
#include "universal_generator_service.h"

int main() {
    std::cout << "=== RawrXD Universal Generator Service - Smoke Test ===" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Generate a project
    std::cout << "[TEST 1] Generate Web App Project" << std::endl;
    std::string result1 = GenerateAnything("generate_project", 
        R"({"name":"TestApp","type":"web","path":"./test_output"})");
    std::cout << "Result: " << result1 << std::endl;
    std::cout << std::endl;
    
    // Test 2: Generate a guide
    std::cout << "[TEST 2] Generate Cookie Recipe Guide" << std::endl;
    std::string result2 = GenerateAnything("generate_guide", "chocolate chip cookies");
    std::cout << "Result: " << result2 << std::endl;
    std::cout << std::endl;
    
    // Test 3: Load model
    std::cout << "[TEST 3] Load Model" << std::endl;
    std::string result3 = GenerateAnything("load_model", 
        R"({"path":"./models/test.gguf"})");
    std::cout << "Result: " << result3 << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Smoke Test Complete ===" << std::endl;
    return 0;
}
