#pragma once
#include <string>
#include <vector>
#include <memory>
#include "universal_model_router.h"
#include "agentic_engine.h"

namespace RawrXD {

struct Completion {
    std::string text;
    float score;
};

class AIIntegrationHub {
    UniversalModelRouter router;
    std::shared_ptr<AgenticEngine> agenticEngine;

public:
    AIIntegrationHub();
    ~AIIntegrationHub();

    bool initialize(const std::string& configPath);

    std::vector<Completion> getCompletions(const std::string& bufferName, const std::string& prefix, const std::string& suffix, int cursorOffset);
    
    // Additional features
    std::string generateTests(const std::string& code);
    std::string findBugs(const std::string& code);
    std::string optimizeCode(const std::string& code);

    // Agentic features
    std::string planTask(const std::string& task);
    std::string chat(const std::string& message);
    std::string executePlan(const std::string& plan);

    std::shared_ptr<AgenticEngine> getAgent() { return agenticEngine; }
};

}
