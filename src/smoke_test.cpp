#include <string>
#include "universal_generator_service.h"
#include "logging/logger.h"

int main() {
    Logger logger("SmokeTest");
    logger.info("=== RawrXD Universal Generator Service - Smoke Test ===");
    
    // Test 1: Generate a project
    logger.info("[TEST 1] Generate Web App Project");
    std::string result1 = GenerateAnything("generate_project", 
        R"({"name":"TestApp","type":"web","path":"./test_output"})");
    logger.info("Result: {}", result1);
    
    // Test 2: Generate a guide
    logger.info("[TEST 2] Generate Cookie Recipe Guide");
    std::string result2 = GenerateAnything("generate_guide", "chocolate chip cookies");
    logger.info("Result: {}", result2);
    
    // Test 3: Load model
    logger.info("[TEST 3] Load Model");
    std::string result3 = GenerateAnything("load_model", 
        R"({"path":"./models/test.gguf"})");
    logger.info("Result: {}", result3);
    
    logger.info("=== Smoke Test Complete ===");
    return 0;
}
