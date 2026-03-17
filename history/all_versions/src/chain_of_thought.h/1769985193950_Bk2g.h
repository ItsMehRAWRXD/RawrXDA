#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace RawrXD {

struct ChainResult {
    float overallConfidence;
    std::vector<std::string> steps;
};

class ChainOfThought {
public:
    virtual ~ChainOfThought() = default;
    
    virtual std::optional<ChainResult> generateChain(const std::string& task, const std::unordered_map<std::string, std::string>& context) {
        // Stub
        return ChainResult{0.88f, {"Step 1", "Step 2"}};
    }
    
    virtual nlohmann::json getStatus() const {
        return {{"status", "ready"}};
    }
};

}
