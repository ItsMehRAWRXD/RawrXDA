#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace RawrXD {

/**
 * @brief Semantic Dependency Graph for repository-scale RAG.
 * Maps symbols (classes, functions) to their definition files and usage sites,
 * allowing the CodebaseVectorIndex to retrieve "vertically coupled" context.
 */
class SemanticDependencyGraph {
public:
    struct DependencyNode {
        std::string symbol;
        std::string definitionFile;
        std::unordered_set<std::string> references; // Files that use this symbol
    };

    void add_dependency(const std::string& symbol, const std::string& defFile, const std::string& refFile) {
        auto& node = m_nodes[symbol];
        node.symbol = symbol;
        node.definitionFile = defFile;
        node.references.insert(refFile);
    }

    std::vector<std::string> get_coupled_files(const std::string& symbol) const {
        auto it = m_nodes.find(symbol);
        if (it == m_nodes.end()) return {};
        
        std::vector<std::string> coupled = { it->second.definitionFile };
        coupled.insert(coupled.end(), it->second.references.begin(), it->second.references.end());
        return coupled;
    }

private:
    std::unordered_map<std::string, DependencyNode> m_nodes;
};

} // namespace RawrXD
