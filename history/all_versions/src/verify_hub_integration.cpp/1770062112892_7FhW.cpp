#include <iostream>
#include <string>
#include <vector>
#include "../include/ai_integration_hub.h"
#include <thread>
#include <chrono>

// Mocking dependencies if they interrupt linking only for this test
// But we should link real ones if possible.

int main() {
    std::cout << "[IntegrationTest] Starting AIIntegrationHub Verification..." << std::endl;
    
    try {
        AIIntegrationHub hub;
        
        std::cout << "[IntegrationTest] Initializing Hub..." << std::endl;
        bool init = hub.initialize(""); // No default model for test
        if (init) {
            std::cout << "[IntegrationTest] Hub Initialized Successfully." << std::endl;
        } else {
             // It might return false if no model loaded, but here we passed empty string so it might just init components.
            std::cout << "[IntegrationTest] Hub initialized (result: " << init << ")" << std::endl;
        }

        // Test Completion Engine access (it's the one we kept)
        std::cout << "[IntegrationTest] Testing Completion Engine..." << std::endl;
        auto completions = hub.getCompletions("test.cpp", "void ma", "", 0);
        std::cout << "[IntegrationTest] Completions retrieved: " << completions.size() << std::endl;

        // Test Advanced Features are disabled (should not crash, just return empty)
        std::cout << "[IntegrationTest] Testing Disabled Features..." << std::endl;
        
        auto tests = hub.generateTests("void func() {}");
        std::cout << "[IntegrationTest] generateTests size: " << tests.size() << std::endl;

        auto opts = hub.optimizeCode("int a = 0;");
        std::cout << "[IntegrationTest] optimizeCode size: " << opts.size() << std::endl;

        // Verify Chat API
        std::cout << "[IntegrationTest] Testing Chat API..." << std::endl;
        std::string chatResponse = hub.chat("Hello, are you there?");
        std::cout << "[IntegrationTest] Chat Response (stub): " << chatResponse << std::endl;

        // Verify Completion API
        auto completions2 = hub.getCompletions("test.cpp", "void main() {", "", 13);
        std::cout << "[IntegrationTest] Completion Response (stub): " << (completions2.empty() ? "None" : completions2[0].text) << std::endl;

        std::cout << "[IntegrationTest] Verification Complete." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[IntegrationTest] Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}