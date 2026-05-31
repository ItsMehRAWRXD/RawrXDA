#include "NotebookLSPManager.h"
#include <iostream>

namespace RawrXD::LSP {

NotebookLSPManager& NotebookLSPManager::GetInstance() {
    static NotebookLSPManager instance;
    return instance;
}

void NotebookLSPManager::DidOpenNotebook(const NotebookDocument& notebook) {
    m_openNotebooks[notebook.uri] = notebook;
    std::cout << "[NotebookLSP] Opened notebook: " << notebook.uri << " with " << notebook.cells.size() << " cells." << std::endl;
}

void NotebookLSPManager::DidChangeNotebook(const std::string& uri, const nlohmann::json& changes) {
    if (m_openNotebooks.find(uri) != m_openNotebooks.end()) {
        std::cout << "[NotebookLSP] Synchronizing changes for notebook: " << uri << std::endl;
        // In a real implementation, this would update the cell contents and notify the LSP server
    }
}

void NotebookLSPManager::DidCloseNotebook(const std::string& uri) {
    m_openNotebooks.erase(uri);
    std::cout << "[NotebookLSP] Closed notebook: " << uri << std::endl;
}

std::vector<uint32_t> NotebookLSPManager::GetSemanticTokens(const std::string& uri) {
    std::cout << "[NotebookLSP] Requesting semantic tokens for: " << uri << std::endl;
    // LSP 3.17 Semantic tokens are represented as a flat array of integers (delta-encoded)
    // [deltaLine, deltaStart, length, tokenType, tokenModifiers]
    return {0, 0, 12, 1, 0, 1, 0, 8, 2, 0}; // Mock tokens
}

}
