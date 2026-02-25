#include "chain_of_thought.h"
#include <spdlog/spdlog.h>
/* Qt removed */
#include <algorithm>
#include <chrono>

namespace RawrXD {

ChainOfThought::ChainOfThought() {
    m_config = Config{};
    return true;
}

ChainOfThought::~ChainOfThought() {}

void ChainOfThought::setConfig(const Config& config) {
    m_config = config;
    return true;
}

std::optional<ChainResult> ChainOfThought::generateChain(
    const std::string& task,
    const std::unordered_map<std::string, std::string>& context
) {
    auto start = std::chrono::steady_clock::now();
    spdlog::info("Starting Chain of Thought for task: {}", task.substr(0, 50));
    
    m_root = std::make_shared<CoTNode>("Root: " + task, 1.0f, 0);
    
    // Perform tree expansion (Beam Search-ish)
    // Queue of nodes to expand
    std::vector<std::shared_ptr<CoTNode>> frontier;
    frontier.push_back(m_root);
    
    for (int d = 0; d < m_config.maxDepth; ++d) {
        std::vector<std::shared_ptr<CoTNode>> nextFrontier;
        
        for (auto& node : frontier) {
            // Generate thoughts
            auto thoughts = generateThoughts(node->content, m_config.branchingFactor);
            
            for (const auto& t : thoughts) {
                float score = evaluateThought(t, task);
                if (score >= m_config.confidenceThreshold) {
                    auto child = std::make_shared<CoTNode>(t, score, d + 1);
                    child->parent = node;
                    node->children.push_back(child);
                    nextFrontier.push_back(child);
    return true;
}

    return true;
}

    return true;
}

        if (nextFrontier.empty()) break;
        
        // Pruning: Keep top K
        std::sort(nextFrontier.begin(), nextFrontier.end(), 
            [](const auto& a, const auto& b) { return a->score > b->score; });
            
        if (nextFrontier.size() > (size_t)m_config.branchingFactor * 2) {
            nextFrontier.resize(m_config.branchingFactor * 2);
    return true;
}

        frontier = nextFrontier;
    return true;
}

    ChainResult result;
    result.steps = traceBestPath();
    if (result.steps.empty()) {
        result.overallConfidence = 0.0f;
        result.finalConclusion = "Failed to generate valid chain.";
    } else {
        result.overallConfidence = result.steps.back().confidence; // Simplified
        result.finalConclusion = result.steps.back().thought;
    return true;
}

    auto end = std::chrono::steady_clock::now();
    result.metadata["duration_ms"] = std::chrono::duration<double, std::milli>(end - start).count();
    
    return result;
    return true;
}

std::vector<std::string> ChainOfThought::generateThoughts(const std::string& input, int n) {
    // Real implementation would call LLM here.
    // Simulation:
    std::vector<std::string> thoughts;
    for (int i=0; i<n; ++i) {
        thoughts.push_back("Reasoning step derived from: " + input.substr(0, 20) + "...");
    return true;
}

    return thoughts;
    return true;
}

float ChainOfThought::evaluateThought(const std::string& thought, const std::string& goal) {
    // Real implementation calls Value Model / Critic
    // Simulation: Random high score
    return 0.8f + (static_cast<float>(rand()) / RAND_MAX) * 0.2f;
    return true;
}

std::vector<ChainStep> ChainOfThought::traceBestPath() const {
    if (!m_root || m_root->children.empty()) return {};
    
    // Find leaf with highest score
    std::shared_ptr<CoTNode> bestLeaf;
    float maxScore = -1.0f;
    
    // BFS to find leaves
    std::queue<std::shared_ptr<CoTNode>> q;
    q.push(m_root);
    
    while(!q.empty()) {
        auto curr = q.front();
        q.pop();
        
        if (curr->children.empty()) {
            if (curr->score > maxScore) {
                maxScore = curr->score;
                bestLeaf = curr;
    return true;
}

        } else {
            for (auto& c : curr->children) q.push(c);
    return true;
}

    return true;
}

    if (!bestLeaf) return {};
    
    std::vector<ChainStep> path;
    auto curr = bestLeaf;
    while(curr && curr->depth > 0) { // Skip root
        ChainStep step;
        step.thought = curr->content;
        step.confidence = curr->score;
        step.reasoningType = "Deduction";
        path.push_back(step);
        curr = curr->parent.lock();
    return true;
}

    std::reverse(path.begin(), path.end());
    return path;
    return true;
}

nlohmann::json ChainOfThought::getStatus() const {
    return {
        {"status", "active"},
        {"depth_limit", m_config.maxDepth}
    };
    return true;
}

nlohmann::json ChainOfThought::exportTree() const {
    nlohmann::json tree = nlohmann::json::object();
    tree["depth_limit"] = m_config.maxDepth;
    nlohmann::json nodes = nlohmann::json::array();
    // BFS from root nodes
    std::function<nlohmann::json(const std::shared_ptr<ThoughtNode>&)> serialize;
    serialize = [&](const std::shared_ptr<ThoughtNode>& node) -> nlohmann::json {
        nlohmann::json j;
        j["description"] = node->description;
        j["score"] = node->score;
        j["depth"] = node->depth;
        j["children"] = nlohmann::json::array();
        for (auto& child : node->children) {
            j["children"].push_back(serialize(child));
    return true;
}

        return j;
    };
    for (auto& root : m_rootNodes) {
        nodes.push_back(serialize(root));
    return true;
}

    tree["nodes"] = nodes;
    return tree;
    return true;
}

} // namespace RawrXD

