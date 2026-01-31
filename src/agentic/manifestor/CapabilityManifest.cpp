#include "CapabilityManifest.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>

namespace RawrXD::Agentic::Manifestor {

bool CapabilityManifest::addCapability(const CapabilityDescriptor& cap) {
    // Check if capability already exists
    for (auto& existing : capabilities) {
        if (existing.name == cap.name) {
            // Update if newer version
            if (cap.version.major > existing.version.major ||
                (cap.version.major == existing.version.major && cap.version.minor > existing.version.minor)) {
                existing = cap;
                return true;
            }
            return false;
        }
    }
    
    capabilities.push_back(cap);
    
    // Add to wiring graph
    WiringNode node;
    node.capabilityName = cap.name;
    node.dependsOn = cap.requiredCapabilities;
    wiringGraph.push_back(node);
    
    return true;
}

const CapabilityDescriptor* CapabilityManifest::getCapability(const std::string& name) const {
    for (const auto& cap : capabilities) {
        if (cap.name == name) {
            return &cap;
        }
    }
    return nullptr;
}

CapabilityDescriptor* CapabilityManifest::getCapability(const std::string& name) {
    for (auto& cap : capabilities) {
        if (cap.name == name) {
            return &cap;
        }
    }
    return nullptr;
}

std::vector<std::string> CapabilityManifest::getLoadOrder() {
    // Topological sort via Kahn's algorithm
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adjList;
    
    // Build graph
    for (const auto& node : wiringGraph) {
        inDegree[node.capabilityName] = 0;
    }
    
    for (const auto& node : wiringGraph) {
        for (const auto& dep : node.dependsOn) {
            adjList[dep].push_back(node.capabilityName);
            inDegree[node.capabilityName]++;
        }
    }
    
    // Find nodes with no incoming edges
    std::queue<std::string> queue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0) {
            queue.push(name);
        }
    }
    
    // Process queue
    std::vector<std::string> result;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);
        
        for (const auto& neighbor : adjList[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }
    
    return result;
}

bool CapabilityManifest::hasCircularDependencies() const {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recStack;
    
    std::function<bool(const std::string&)> hasCycle = [&](const std::string& node) -> bool {
        visited.insert(node);
        recStack.insert(node);
        
        for (const auto& graphNode : wiringGraph) {
            if (graphNode.capabilityName == node) {
                for (const auto& dep : graphNode.dependsOn) {
                    if (recStack.find(dep) != recStack.end()) {
                        return true; // Cycle detected
                    }
                    if (visited.find(dep) == visited.end()) {
                        if (hasCycle(dep)) {
                            return true;
                        }
                    }
                }
            }
        }
        
        recStack.erase(node);
        return false;
    };
    
    for (const auto& node : wiringGraph) {
        if (visited.find(node.capabilityName) == visited.end()) {
            if (hasCycle(node.capabilityName)) {
                return true;
            }
        }
    }
    
    return false;
}

std::string CapabilityManifest::toJson() const {
    std::ostringstream json;
    json << "{\n  \"capabilities\": [\n";
    
    for (size_t i = 0; i < capabilities.size(); ++i) {
        const auto& cap = capabilities[i];
        json << "    {\n";
        json << "      \"name\": \"" << cap.name << "\",\n";
        json << "      \"version\": \"" << cap.version.toString() << "\",\n";
        json << "      \"module\": \"" << cap.sourceModule << "\",\n";
        json << "      \"description\": \"" << cap.description << "\"\n";
        json << "    }";
        if (i < capabilities.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "  ]\n}";
    return json.str();
}

std::string CapabilityManifest::toDot() const {
    std::ostringstream dot;
    dot << "digraph CapabilityWiring {\n";
    dot << "  rankdir=LR;\n";
    dot << "  node [shape=box];\n\n";
    
    for (const auto& node : wiringGraph) {
        for (const auto& dep : node.dependsOn) {
            dot << "  \"" << dep << "\" -> \"" << node.capabilityName << "\";\n";
        }
    }
    
    dot << "}\n";
    return dot.str();
}

void CapabilityManifest::computeDepth(const std::string& name, int currentDepth) {
    for (auto& node : wiringGraph) {
        if (node.capabilityName == name) {
            if (node.depth < currentDepth) {
                node.depth = currentDepth;
                for (const auto& dep : node.dependsOn) {
                    computeDepth(dep, currentDepth + 1);
                }
            }
            break;
        }
    }
}

} // namespace RawrXD::Agentic::Manifestor

