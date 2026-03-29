#include "chain_of_thought.h"
#include <spdlog/spdlog.h>
#include <queue>
#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace RawrXD {

ChainOfThought::ChainOfThought() {
    m_config = Config{};
}

ChainOfThought::~ChainOfThought() {}

void ChainOfThought::setConfig(const Config& config) {
    m_config = config;
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
                }
            }
        }
        
        if (nextFrontier.empty()) break;
        
        // Pruning: Keep top K
        std::sort(nextFrontier.begin(), nextFrontier.end(), 
            [](const auto& a, const auto& b) { return a->score > b->score; });
            
        if (nextFrontier.size() > (size_t)m_config.branchingFactor * 2) {
            nextFrontier.resize(m_config.branchingFactor * 2);
        }
        
        frontier = nextFrontier;
    }
    
    ChainResult result;
    result.steps = traceBestPath();
    if (result.steps.empty()) {
        result.overallConfidence = 0.0f;
        result.finalConclusion = "Failed to generate valid chain.";
    } else {
        result.overallConfidence = result.steps.back().confidence; // Simplified
        result.finalConclusion = result.steps.back().thought;
    }
    
    auto end = std::chrono::steady_clock::now();
    result.metadata["duration_ms"] = std::chrono::duration<double, std::milli>(end - start).count();
    
    return result;
}

std::vector<std::string> ChainOfThought::generateThoughts(const std::string& input, int n) {
    // Real implementation would call LLM here.
    // Simulation:
    std::vector<std::string> thoughts;
    for (int i=0; i<n; ++i) {
        thoughts.push_back("Reasoning step derived from: " + input.substr(0, 20) + "...");
    }
    return thoughts;
}

float ChainOfThought::evaluateThought(const std::string& thought, const std::string& goal) {
    // Real implementation calls Value Model / Critic
    // Simulation: Random high score
    return 0.8f + (static_cast<float>(rand()) / RAND_MAX) * 0.2f;
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
            }
        } else {
            for (auto& c : curr->children) q.push(c);
        }
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
    }
    std::reverse(path.begin(), path.end());
    return path;
}

nlohmann::json ChainOfThought::getStatus() const {
    return {
        {"status", "active"},
        {"depth_limit", m_config.maxDepth}
    };
}

nlohmann::json ChainOfThought::exportTree() const {
    nlohmann::json tree;
    tree["status"]       = "complete";
    tree["config"]       = {
        {"maxDepth",             m_config.maxDepth},
        {"branchingFactor",      m_config.branchingFactor},
        {"confidenceThreshold",  m_config.confidenceThreshold},
        {"enableReflection",     m_config.enableReflection}
    };

    if (!m_root) {
        tree["root"] = nullptr;
    } else {
        // Non-recursive DFS with hard depth/node guards.
        // Prevents stack overflow and protects against malformed cyclic trees.
        struct PendingNode {
            std::shared_ptr<CoTNode> node;
            nlohmann::json* out;
            int depth;
        };

        constexpr int kHardDepthCap = 128;
        constexpr size_t kHardNodeCap = 100000;
        const int safeDepthLimit = std::max(1, std::min(m_config.maxDepth, kHardDepthCap));

        tree["root"] = {
            {"content", m_root->content},
            {"score", m_root->score},
            {"depth", m_root->depth},
            {"children", nlohmann::json::array()}
        };

        std::vector<PendingNode> stack;
        stack.push_back({m_root, &tree["root"], 0});

        std::unordered_set<const CoTNode*> visited;
        visited.reserve(1024);
        visited.insert(m_root.get());

        size_t emittedNodes = 1;
        bool truncatedByDepth = false;
        bool truncatedByNodeCap = false;
        bool cycleDetected = false;

        while (!stack.empty()) {
            PendingNode cur = stack.back();
            stack.pop_back();

            if (cur.depth >= safeDepthLimit) {
                if (!cur.node->children.empty()) {
                    (*cur.out)["truncated"] = true;
                    (*cur.out)["truncation_reason"] = "depth_limit";
                    truncatedByDepth = true;
                }
                continue;
            }

            for (const auto& child : cur.node->children) {
                if (!child) {
                    continue;
                }
                if (emittedNodes >= kHardNodeCap) {
                    (*cur.out)["truncated"] = true;
                    (*cur.out)["truncation_reason"] = "node_cap";
                    truncatedByNodeCap = true;
                    break;
                }

                const CoTNode* raw = child.get();
                if (visited.find(raw) != visited.end()) {
                    cycleDetected = true;
                    continue;
                }
                visited.insert(raw);

                nlohmann::json childJson = {
                    {"content", child->content},
                    {"score", child->score},
                    {"depth", child->depth},
                    {"children", nlohmann::json::array()}
                };
                (*cur.out)["children"].push_back(std::move(childJson));
                nlohmann::json& childRef = (*cur.out)["children"].back();
                stack.push_back({child, &childRef, cur.depth + 1});
                ++emittedNodes;
            }

            if (truncatedByNodeCap) {
                break;
            }
        }

        tree["limits"] = {
            {"appliedDepthLimit", safeDepthLimit},
            {"hardDepthCap", kHardDepthCap},
            {"hardNodeCap", kHardNodeCap},
            {"emittedNodes", emittedNodes},
            {"truncatedByDepth", truncatedByDepth},
            {"truncatedByNodeCap", truncatedByNodeCap},
            {"cycleDetected", cycleDetected}
        };
    }

    // Flatten best path for quick access
    auto bestPath = traceBestPath();
    tree["bestPath"] = nlohmann::json::array();
    for (const auto& step : bestPath) {
        tree["bestPath"].push_back({
            {"thought",       step.thought},
            {"confidence",    step.confidence},
            {"reasoningType", step.reasoningType}
        });
    }
    return tree;
}

} // namespace RawrXD
