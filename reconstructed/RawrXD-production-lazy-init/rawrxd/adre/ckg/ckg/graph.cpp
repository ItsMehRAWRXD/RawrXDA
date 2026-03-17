#include "ckg_graph.h"

void CausalKnowledgeGraph::addNode(const CKGNode& n) {
    nodes[n.id] = n;
}

void CausalKnowledgeGraph::addEdge(const CKGEdge& e) {
    edges.push_back(e);
}

const std::unordered_map<uint64_t, CKGNode>& CausalKnowledgeGraph::getNodes() const {
    return nodes;
}

const std::vector<CKGEdge>& CausalKnowledgeGraph::getEdges() const {
    return edges;
}
