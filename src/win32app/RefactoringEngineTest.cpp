#include "RefactoringEngine.h"
#include <iostream>
#include <cassert>

using namespace RawrXD::LSP;

int main() {
    std::cout << "Starting Cross-file Refactoring Engine Test...
";
    auto& engine = RefactoringEngine::GetInstance();

    // 1. Setup Mock Workspace Edit
    WorkspaceEdit edit;
    
    nlohmann::json edit1 = {
        {"range", {{"start", {{"line", 5}, {"character", 0}}}, {"end", {{"line", 5}, {"character", 10}}}}},
        {"newText", "RenamedClass"}
    };
    
    nlohmann::json edit2 = {
        {"range", {{"start", {{"line", 12}, {"character", 5}}}, {"end", {{"line", 12}, {"character", 15}}}}},
        {"newText", "RenamedClass"}
    };

    edit.changes["file:///D:/src/header.h"].push_back(edit1);
    edit.changes["file:///D:/src/impl.cpp"].push_back(edit2);

    // 2. Test Transactional Execution
    engine.BeginTransaction();
    if (engine.ApplyWorkspaceEdit(edit)) {
        std::cout << "Workspace edit applied successfully.
";
        engine.CommitTransaction();
    } else {
        std::cerr << "Workspace edit FAILED.
";
        engine.RollbackTransaction();
        return 1;
    }

    // 3. Test History/Undo
    if (engine.UndoLastOperation()) {
        std::cout << "Undo operation verified.
";
    } else {
        std::cerr << "Undo operation FAILED.
";
        return 1;
    }

    std::cout << "Cross-file Refactoring Engine: PASSED
";
    return 0;
}
