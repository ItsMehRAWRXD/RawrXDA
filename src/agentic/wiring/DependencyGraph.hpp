#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace RawrXD::Agentic::Wiring {

/// Dependency resolution engine
class DependencyGraph {
public:
    /// Add node
    void addNode(const std::string& name);
    
    /// Add dependency edge (from -> to)
    void addEdge(const std::string& from, const std::string& to);
    
    /// Topological sort (returns load order)
    std::vector<std::string> topologicalSort() const;
    
    /// Detect circular dependencies
    bool hasCircularDependency() const;
    
    /// Get all dependencies of a node (recursive)
    std::vector<std::string> getAllDependencies(const std::string& name) const;
    
    /// Get all dependents of a node (what depends on this)
    std::vector<std::string> getAllDependents(const std::string& name) const;
    
    /// Check if path exists between two nodes
    bool hasPath(const std::string& from, const std::string& to) const;
    
    /// Get missing dependencies
    std::vector<std::string> getMissingDependencies(const std::string& name) const;
    
    /// Get depth of node (for priority)
    int getDepth(const std::string& name) const;
    
    /// Clear graph
    void clear();
    
    /// Generate DOT format for GraphViz
    std::string toDot() const;
    
private:
    std::unordered_set<std::string> m_nodes;
    std::unordered_map<std::string, std::vector<std::string>> m_edges;        // adjacency list
    std::unordered_map<std::string, std::vector<std::string>> m_reverseEdges; // reverse adjacency list
    
    bool hasCycleDFS(const std::string& node, 
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& stack) const;
                     
    void topologicalSortDFS(const std::string& node,
                           std::unordered_set<std::string>& visited,
                           std::vector<std::string>& result) const;
};

} // namespace RawrXD::Agentic::Wiring
