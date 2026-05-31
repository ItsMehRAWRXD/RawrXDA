#include "RefactoringEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace RawrXD::LSP {

RefactoringEngine& RefactoringEngine::GetInstance() {
    static RefactoringEngine instance;
    return instance;
}

bool RefactoringEngine::ApplyWorkspaceEdit(const WorkspaceEdit& edit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[Refactor] Applying workspace edit across " << edit.changes.size() << " files." << std::endl;

    for (auto const& [uri, edits] : edit.changes) {
        for (const auto& textEdit : edits) {
            if (!ApplyTextEditToFile(uri, textEdit)) {
                std::cerr << "[Refactor] Failed to apply edit to: " << uri << std::endl;
                return false;
            }
        }
    }

    m_history.push_back({"Global Refactor", edit, true});
    return true;
}

bool RefactoringEngine::ApplyTextEditToFile(const std::string& uri, const nlohmann::json& edit) {
    // In a real IDE, this would interact with the buffer manager or VFS
    // For Phase 3.3, we simulate the coordinate-aware replacement
    std::cout << "[Refactor] Patching file: " << uri << " with new text: " << edit["newText"] << std::endl;
    return true;
}

bool RefactoringEngine::UndoLastOperation() {
    if (m_history.empty()) return false;
    // Undo logic would reverse the text edits stored in history
    m_history.pop_back();
    return true;
}

void RefactoringEngine::BeginTransaction() {
    std::cout << "[Refactor] Transaction started." << std::endl;
}

void RefactoringEngine::CommitTransaction() {
    std::cout << "[Refactor] Transaction committed." << std::endl;
}

void RefactoringEngine::RollbackTransaction() {
    std::cout << "[Refactor] Transaction rolled back." << std::endl;
}

}
