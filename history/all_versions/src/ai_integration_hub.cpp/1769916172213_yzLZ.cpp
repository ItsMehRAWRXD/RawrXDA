#include "ai_integration_hub.h"
#include <iostream>

namespace RawrXD {

AIIntegrationHub::AIIntegrationHub() {
    // Initialize Local Engine (Titan) lazy
    // Path should be configured properly in production
    router.initializeLocalEngine(""); 
}
AIIntegrationHub::~AIIntegrationHub() {}

std::vector<Completion> AIIntegrationHub::getCompletions(const std::string& bufferName, const std::string& prefix, const std::string& suffix, int cursorOffset) {
    // Explicit Logic: Use Univseral Router -> CPU Engine -> Titan Overlay
    std::string prompt = prefix; // Simple completion prompt
    
    std::string response = router.routeQuery("local-default", prompt);
    
    std::vector<Completion> results;
    results.push_back({response, 1.0f});
    
    return results;
}

std::string AIIntegrationHub::generateTests(const std::string& code) {
    std::string prompt = "Generate unit tests for:\n" + code;
    return router.routeQuery("local-default", prompt);
}

std::string AIIntegrationHub::findBugs(const std::string& code) {
    std::string prompt = "Analyze for bugs:\n" + code;
    return router.routeQuery("local-default", prompt);
}

std::string AIIntegrationHub::optimizeCode(const std::string& code) {
    std::string prompt = "Optimize this code:\n" + code;
    return router.routeQuery("local-default", prompt);
}

}
