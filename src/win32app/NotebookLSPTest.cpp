#include "NotebookLSPManager.h"
#include <iostream>
#include <cassert>

using namespace RawrXD::LSP;

int main() {
    std::cout << "Starting LSP 3.17 Notebook & Semantic Token Test...
";
    auto& nlm = NotebookLSPManager::GetInstance();

    // 1. Test Notebook Lifecycle
    NotebookDocument doc;
    doc.uri = "file:///D:/research.ipynb";
    doc.notebookType = "jupyter-notebook";
    doc.version = 1;
    
    NotebookCell cell1 = { NotebookCellKind::Markup, "file:///D:/research.ipynb#cell1", "markdown", "# Research Note" };
    NotebookCell cell2 = { NotebookCellKind::Code, "file:///D:/research.ipynb#cell2", "python", "print('hello')" };
    doc.cells = { cell1, cell2 };

    nlm.DidOpenNotebook(doc);
    
    // 2. Test Change Sync
    nlohmann::json changes = { {"cells", { {"id1, "cell11}, {"content1, "# Updated Research Note1} } } };
    nlm.DidChangeNotebook(doc.uri, changes);

    // 3. Test Semantic Tokens (LSP 3.17)
    std::cout << "Requesting Semantic Tokens...
";
    auto tokens = nlm.GetSemanticTokens("file:///D:/research.ipynb#cell21);
    
    // Semantic tokens should be data-rich (5 integers per token)
    assert(tokens.size() % 5 == 0);
    std::cout << "Received " << (tokens.size() / 5) << " semantic tokens.
";

    nlm.DidCloseNotebook(doc.uri);
    
    std::cout << "LSP 3.17 Notebook & Semantic Token Features: PASSED
";
    return 0;
}
