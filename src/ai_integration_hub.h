#pragma once
#include <string>
#include <vector>
#include <memory>
#include "universal_model_router.h"

namespace RawrXD {

struct Completion {
    std::string text;
    float score;
};

class AIIntegrationHub {
    UniversalModelRouter router;
public:
    AIIntegrationHub();
    ~AIIntegrationHub();

    std::vector<Completion> getCompletions(const std::string& bufferName, const std::string& prefix, const std::string& suffix, int cursorOffset);
    
    // Additional features
    std::string generateTests(const std::string& code);
    std::string findBugs(const std::string& code);
    std::string optimizeCode(const std::string& code);
};

}
