#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <chrono>

namespace IDE_AI {

// Project Knowledge Graph for storing and querying project information
class ProjectKnowledgeGraph {
public:
    ProjectKnowledgeGraph() = default;
    virtual ~ProjectKnowledgeGraph() = default;
    
    // Node structure
    struct Node {
        std::string id;
        std::string type;
        std::map<std::string, std::string> properties;
        std::chrono::steady_clock::time_point created_at;
        
        Node() {
            created_at = std::chrono::steady_clock::now();
        }
    };
    
    // Add a node to the knowledge graph
    virtual void add_node(const Node& node) = 0;
    
    // Find nodes by type
    virtual std::vector<Node> find_nodes_by_type(const std::string& type) = 0;
    
    // Find nodes by property
    virtual std::vector<Node> find_nodes_by_property(const std::string& property, const std::string& value) = 0;
    
    // Get all nodes
    virtual std::vector<Node> get_all_nodes() = 0;
    
    // Clear the knowledge graph
    virtual void clear() = 0;
    
    // Get statistics
    virtual size_t get_node_count() const = 0;
};

} // namespace IDE_AI
