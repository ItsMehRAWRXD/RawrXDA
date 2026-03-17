#pragma once
#include "ckg_node.h"
#include "ckg_edge.h"
#include <unordered_map>
#include <vector>

class CausalKnowledgeGraph {
public:
    void addNode(const CKGNode& n);
    void addEdge(const CKGEdge& e);
    const std::unordered_map<uint64_t, CKGNode>& getNodes() const;
    const std::vector<CKGEdge>& getEdges() const;
private:
    std::unordered_map<uint64_t, CKGNode> nodes;
    std::vector<CKGEdge> edges;
};
