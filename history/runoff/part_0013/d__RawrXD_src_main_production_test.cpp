#include <iostream>
#include <memory>
#include "ai_integration_hub.h"
#include "production_test_suite.h"

const char* agentic_model_key = "key_1ce98c83a13f1b7168db8cb5792c459bd23cd4a8ba36500ac31dede1539fafec";

int main() {


    try {
        // Create AI Integration Hub
        auto aiHub = std::make_shared<AIIntegrationHub>();

        // Initialize with default model
        
        bool initialized = aiHub->initialize("llama3:latest");

        if (!initialized) {
            
            return 1;
        }


        // Create and run test suite
        auto testSuite = std::make_unique<ProductionTestSuite>(aiHub);


        bool allTestsPassed = testSuite->runFullTestSuite();

        // Generate report
        std::string report = testSuite->generateReport();


        // Check production readiness
        bool productionReady = testSuite->isProductionReady();


        if (productionReady) {
            
        } else {
            
        }


        auto results = testSuite->getResults();
        int passed = 0;
        for (const auto& r : results) {
            if (r.passed) passed++;
        }
        printf("%d/%lu (%.1f%%)\n", passed, results.size(), 
               results.size() > 0 ? (double)passed / results.size() * 100.0 : 0.0);


        return allTestsPassed ? 0 : 1;

    } catch (const std::exception& e) {
        
        return 1;
    }
}
