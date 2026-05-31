#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>

namespace RawrXD {

struct ChainStep {
    std::string thought;
    float confidence;
    std::string reasoningType; // "Deduction", "Induction", "Critique"
    double durationMs;
};

struct ChainResult {
    float overallConfidence;
    std::vector<ChainStep> steps;
    std::string finalConclusion;
    nlohmann::json metadata;
};

struct CoTNode {
    std::string content;
    float score;
    int depth;
    std::vector<std::shared_ptr<CoTNode>> children;
    std::weak_ptr<CoTNode> parent;
    
    CoTNode(std::string c, float s, int d) : content(c), score(s), depth(d) {}
};

class ChainOfThought {
public:
    ChainOfThought();
    virtual ~ChainOfThought();

    // Configuration
    struct Config {
        int maxDepth = 5;
        int branchingFactor = 3;
        float confidenceThreshold = 0.7f;
        bool enableReflection = true;
    };
    
    void setConfig(const Config& config);

    // Main API
    virtual std::optional<ChainResult> generateChain(
        const std::string& task,
        const std::unordered_map<std::string, std::string>& context
    );
    
    virtual nlohmann::json getStatus() const;
    
    // Visualization/Debug
    nlohmann::json exportTree() const;

private:
    Config m_config;
    std::shared_ptr<CoTNode> m_root;

    // Internal AI helpers (mocked or hooked to model engine)
    std::vector<std::string> generateThoughts(const std::string& input, int n);
    float evaluateThought(const std::string& thought, const std::string& goal);
    
    // Recursive search
    void expandNode(std::shared_ptr<CoTNode> node, const std::string& goal);
    std::vector<ChainStep> traceBestPath() const;
};

} // namespace RawrXD
