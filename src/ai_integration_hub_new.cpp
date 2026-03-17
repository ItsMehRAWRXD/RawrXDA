#include "ai_integration_hub.h"
#include "ai_model_caller.h" // ModelCaller
#include <iostream>

namespace RawrXD {

AIIntegrationHub::AIIntegrationHub() {}
AIIntegrationHub::~AIIntegrationHub() {}

std::vector<Completion> AIIntegrationHub::getCompletions(const std::string& bufferName, const std::string& prefix, const std::string& suffix, int cursorOffset) {
    // Calling ModelCaller::generateCompletion (Using the class ModelCaller from ai_model_caller.h)
    std::string fileType = "cpp"; 
    std::string context = bufferName; 
    
    // ModelCaller is in global namespace as per my read_file of ai_model_caller.h
    auto callerCompletions = ::ModelCaller::generateCompletion(prefix, suffix, fileType, context, 1);
    
    std::vector<Completion> results;
    for(const auto& c : callerCompletions) {
        results.push_back({c.text, c.score});
    }
    return results;
}

std::string AIIntegrationHub::generateTests(const std::string& code) {
    return ::ModelCaller::generateCode("Generate unit tests for this code", "cpp", code);
}

std::string AIIntegrationHub::findBugs(const std::string& code) {
    // ModelCaller doesn't have explicit findBugs, reuse generateCode
    return ::ModelCaller::generateCode("Analyze for bugs and security issues", "cpp", code);
}

std::string AIIntegrationHub::optimizeCode(const std::string& code) {
    return ::ModelCaller::generateRewrite(code, "Optimize this code for performance", "cpp");
}

}
