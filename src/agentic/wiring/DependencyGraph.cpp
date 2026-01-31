#include "DependencyGraph.hpp"
#include <algorithm>
#include <sstream>
#include <functional>


namespace RawrXD::Agentic::Wiring {

void DependencyGraph::addNode(const std::string& name) {
    m_nodes.insert(name);
}

void DependencyGraph::addEdge(const std::string& from, const std::string& to) {
    m_edges[from].push_back(to);
    m_reverseEdges[to].push_back(from);
    addNode(from);
    addNode(to);
}

std::vector<std::string> DependencyGraph::topologicalSort() const {
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    
    for (const auto& node : m_nodes) {
        if (visited.find(node) == visited.end()) {
            topologicalSortDFS(node, visited, result);
        }
    }
    
    std::reverse(result.begin(), result.end());
    return result;
}

bool DependencyGraph::hasCircularDependency() const {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> stack;
    
    for (const auto& node : m_nodes) {
        if (visited.find(node) == visited.end()) {
            if (hasCycleDFS(node, visited, stack)) {
                return true;
            }
        }
    }
    
    return false;
}

std::vector<std::string> DependencyGraph::getAllDependencies(const std::string& name) const {
    std::vector<std::string> deps;
    std::unordered_set<std::string> visited;
    
    std::function<void(const std::string&)> collect = [&](const std::string& node) {
        if (visited.find(node) != visited.end()) {
            return;
        }
        visited.insert(node);
        
        auto it = m_edges.find(node);
        if (it != m_edges.end()) {
            for (const auto& dep : it->second) {
                deps.push_back(dep);
                collect(dep);
            }
        }
    };
    
    collect(name);
    return deps;
}

std::vector<std::string> DependencyGraph::getAllDependents(const std::string& name) const {
    std::vector<std::string> dependents;
    std::unordered_set<std::string> visited;
    
    std::function<void(const std::string&)> collect = [&](const std::string& node) {
        if (visited.find(node) != visited.end()) {
            return;
        }
        visited.insert(node);
        
        auto it = m_reverseEdges.find(node);
        if (it != m_reverseEdges.end()) {
            for (const auto& dep : it->second) {
                dependents.push_back(dep);
                collect(dep);
            }
        }
    };
    
    collect(name);
    return dependents;
}

bool DependencyGraph::hasPath(const std::string& from, const std::string& to) const {
    if (from == to) return true;
    
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;
    queue.push(from);
    visited.insert(from);
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        auto it = m_edges.find(current);
        if (it != m_edges.end()) {
            for (const auto& neighbor : it->second) {
                if (neighbor == to) {
                    return true;
                }
                if (visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }
    }
    
    return false;
}

std::vector<std::string> DependencyGraph::getMissingDependencies(const std::string& name) const {
    std::vector<std::string> missing;
    auto it = m_edges.find(name);
    if (it != m_edges.end()) {
        for (const auto& dep : it->second) {
            if (m_nodes.find(dep) == m_nodes.end()) {
                missing.push_back(dep);
            }
        }
    }
    return missing;
}

int DependencyGraph::getDepth(const std::string& name) const {
    std::unordered_map<std::string, int> depths;
    
    std::function<int(const std::string&)> computeDepth = [&](const std::string& node) -> int {
        auto it = depths.find(node);
        if (it != depths.end()) {
            return it->second;
        }
        
        int maxDepth = 0;
        auto edgeIt = m_edges.find(node);
        if (edgeIt != m_edges.end()) {
            for (const auto& dep : edgeIt->second) {
                maxDepth = std::max(maxDepth, computeDepth(dep) + 1);
            }
        }
        
        depths[node] = maxDepth;
        return maxDepth;
    };
    
    return computeDepth(name);
}

void DependencyGraph::clear() {
    m_nodes.clear();
    m_edges.clear();
    m_reverseEdges.clear();
}

std::string DependencyGraph::toDot() const {
    std::ostringstream dot;
    dot << "digraph Dependencies {\n";
    dot << "  rankdir=LR;\n";
    
    for (const auto& [from, tos] : m_edges) {
        for (const auto& to : tos) {
            dot << "  \"" << from << "\" -> \"" << to << "\";\n";
        }
    }
    
    dot << "}\n";
    return dot.str();
}

bool DependencyGraph::hasCycleDFS(const std::string& node,
                                   std::unordered_set<std::string>& visited,
                                   std::unordered_set<std::string>& stack) const {
    visited.insert(node);
    stack.insert(node);
    
    auto it = m_edges.find(node);
    if (it != m_edges.end()) {
        for (const auto& neighbor : it->second) {
            if (stack.find(neighbor) != stack.end()) {
                return true;
            }
            if (visited.find(neighbor) == visited.end()) {
                if (hasCycleDFS(neighbor, visited, stack)) {
                    return true;
                }
            }
        }
    }
    
    stack.erase(node);
    return false;
}

void DependencyGraph::topologicalSortDFS(const std::string& node,
                                         std::unordered_set<std::string>& visited,
                                         std::vector<std::string>& result) const {
    visited.insert(node);
    
    auto it = m_edges.find(node);
    if (it != m_edges.end()) {
        for (const auto& neighbor : it->second) {
            if (visited.find(neighbor) == visited.end()) {
                topologicalSortDFS(neighbor, visited, result);
            }
        }
    }
    
    result.push_back(node);
}

} // namespace RawrXD::Agentic::Wiring

