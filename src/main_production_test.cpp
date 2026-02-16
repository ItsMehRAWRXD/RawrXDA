#include <iostream>
#include <memory>
#include "ai_integration_hub.h"
#include "production_test_suite.h"

#include "logging/logger.h"
static Logger s_logger("main_production_test");

int main() {
    s_logger.info("╔════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║  RawrXD Agentic IDE - Production Validation Suite v1.0     ║\n");
    s_logger.info("╠════════════════════════════════════════════════════════════╣\n");
    s_logger.info("║  Multi-Format Model Loading + 7 AI System Integration      ║\n");
    s_logger.info("╚════════════════════════════════════════════════════════════╝\n\n");

    try {
        // Create AI Integration Hub
        auto aiHub = std::make_shared<AIIntegrationHub>();

        // Initialize with default model
        s_logger.info("🚀 Initializing AI Integration Hub...\n");
        bool initialized = aiHub->initialize("llama3:latest");

        if (!initialized) {
            s_logger.error( "❌ Failed to initialize AI Integration Hub\n";
            return 1;
        }

        s_logger.info("✅ AI Integration Hub initialized successfully\n\n");

        // Create and run test suite
        auto testSuite = std::make_unique<ProductionTestSuite>(aiHub);

        s_logger.info("🧪 Running comprehensive production test suite...\n\n");
        bool allTestsPassed = testSuite->runFullTestSuite();

        // Generate report
        std::string report = testSuite->generateReport();
        s_logger.info( report << std::endl;

        // Check production readiness
        bool productionReady = testSuite->isProductionReady();

        s_logger.info("\n╔════════════════════════════════════════════════════════════╗\n");
        s_logger.info("║  PRODUCTION READINESS ASSESSMENT                          ║\n");
        s_logger.info("╠════════════════════════════════════════════════════════════╣\n");
        
        if (productionReady) {
            s_logger.info("║  Status: ✅ PRODUCTION READY                              ║\n");
        } else {
            s_logger.info("║  Status: ⚠️  REQUIRES ADDITIONAL VALIDATION              ║\n");
        }

        s_logger.info("║  Tests Passed: ");
        auto results = testSuite->getResults();
        int passed = 0;
        for (const auto& r : results) {
            if (r.passed) passed++;
        }
        printf("%d/%lu (%.1f%%)\n", passed, results.size(), 
               results.size() > 0 ? (double)passed / results.size() * 100.0 : 0.0);
        s_logger.info("╚════════════════════════════════════════════════════════════╝\n");

        return allTestsPassed ? 0 : 1;

    } catch (const std::exception& e) {
        s_logger.error( "❌ Fatal error: " << e.what() << "\n";
        return 1;
    }
}
