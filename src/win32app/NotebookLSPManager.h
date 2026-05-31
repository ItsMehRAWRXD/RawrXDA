#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace RawrXD::LSP {

enum class NotebookCellKind {
    Markup = 1,
    Code = 2
};

struct NotebookCell {
    NotebookCellKind kind;
    std::string document_uri;
    std::string languageId;
    std::string content;
};

struct NotebookDocument {
    std::string uri;
    std::string notebookType;
    int version;
    std::vector<NotebookCell> cells;
};

class NotebookLSPManager {
public:
    static NotebookLSPManager& GetInstance();

    // LSP 3.17 Notebook Document Sync
    void DidOpenNotebook(const NotebookDocument& notebook);
    void DidChangeNotebook(const std::string& uri, const nlohmann::json& changes);
    void DidCloseNotebook(const std::string& uri);

    // Semantic Tokens (LSP 3.17)
    std::vector<uint32_t> GetSemanticTokens(const std::string& uri);

private:
    NotebookLSPManager() = default;
    std::map<std::string, NotebookDocument> m_openNotebooks;
};

}
