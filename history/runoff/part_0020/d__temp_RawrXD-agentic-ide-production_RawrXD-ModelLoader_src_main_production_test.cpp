#include <iostream>
#include <memory>
#include "ai_integration_hub.h"
#include "production_test_suite.h"

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RawrXD Agentic IDE - Production Validation Suite v1.0     ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Multi-Format Model Loading + 7 AI System Integration      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    try {
        // Create AI Integration Hub
        auto aiHub = std::make_shared<AIIntegrationHub>();

        // Initialize with default model
        std::cout << "🚀 Initializing AI Integration Hub...\n";
        bool initialized = aiHub->initialize("llama3:latest");

        if (!initialized) {
            std::cerr << "❌ Failed to initialize AI Integration Hub\n";
            return 1;
        }

        std::cout << "✅ AI Integration Hub initialized successfully\n\n";

        // Create and run test suite
        auto testSuite = std::make_unique<ProductionTestSuite>(aiHub);

        std::cout << "🧪 Running comprehensive production test suite...\n\n";
        bool allTestsPassed = testSuite->runFullTestSuite();

        // Generate report
        std::string report = testSuite->generateReport();
        std::cout << report << std::endl;

        // Check production readiness
        bool productionReady = testSuite->isProductionReady();

        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  PRODUCTION READINESS ASSESSMENT                          ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════╣\n";
        
        if (productionReady) {
            std::cout << "║  Status: ✅ PRODUCTION READY                              ║\n";
        } else {
            std::cout << "║  Status: ⚠️  REQUIRES ADDITIONAL VALIDATION              ║\n";
        }

        std::cout << "║  Tests Passed: ";
        auto results = testSuite->getResults();
        int passed = 0;
        for (const auto& r : results) {
            if (r.passed) passed++;
        }
        printf("%d/%lu (%.1f%%)\n", passed, results.size(), 
               results.size() > 0 ? (double)passed / results.size() * 100.0 : 0.0);
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";

        return allTestsPassed ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << "\n";
        return 1;
    }
}
