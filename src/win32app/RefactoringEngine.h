#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "AdvancedLSPClient.h"

namespace RawrXD::LSP {

struct RefactorOperation {
    std::string title;
    WorkspaceEdit edit;
    bool isAtomic = true;
};

class RefactoringEngine {
public:
    static RefactoringEngine& GetInstance();

    // Execution Logic
    bool ApplyWorkspaceEdit(const WorkspaceEdit& edit);
    bool UndoLastOperation();

    // Transactional Safety
    void BeginTransaction();
    void CommitTransaction();
    void RollbackTransaction();

private:
    RefactoringEngine() = default;
    
    std::vector<RefactorOperation> m_history;
    std::mutex m_mutex;

    bool ApplyTextEditToFile(const std::string& uri, const nlohmann::json& edit);
};

}
