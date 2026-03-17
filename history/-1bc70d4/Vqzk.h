#pragma once
#include "ckg_graph.h"
#include <string>

class CKGStorage {
public:
    static bool save(const CausalKnowledgeGraph& graph, const std::string& path);
    static bool load(CausalKnowledgeGraph& graph, const std::string& path);
};
