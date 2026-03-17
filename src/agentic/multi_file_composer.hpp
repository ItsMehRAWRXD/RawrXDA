#pragma once
#include <string>
#include <vector>
#include <map>
#include <future>
#include "diff_viewer.hpp"
#include "embedded_terminal.hpp"

namespace rawrxd::agentic {

struct ChangeProposal {
    std::string filePath;
    std::string originalContent;
    std::string proposedContent;
    std::string rationale;
    uint32_t lineStart;
    uint32_t lineCount;
};

struct ExecutionPlan {
    std::string planId;
    std::string description;
    std::vector<ChangeProposal> proposals;
    bool requiresTerminalExecution;
    std::string validationCommand;
};

class MultiFileComposer {
public:
    static MultiFileComposer& instance() {
        static MultiFileComposer instance;
        return instance;
    }

    // Step 1: Analyze user request and build execution plan
    std::future<ExecutionPlan> buildPlan(const std::string& user_request,
                                         const std::vector<std::string>& files);

    // Step 2: Render diffs into the DiffViewer component
    void stagePlanToDiffViewer(const ExecutionPlan& plan, 
                               rawrxd::ui::DiffViewer& viewer);

    // Step 3: Atomic commit of all accepted changes across files
    bool commitPlan(const ExecutionPlan& plan, 
                    const std::vector<uint32_t>& acceptedIndexes);

    // Step 4: Validate changes using terminal bridge (ConPTY)
    std::future<bool> validateChanges(const ExecutionPlan& plan,
                                     rawrxd::terminal::EmbeddedTerminal& terminal);

private:
    std::vector<ChangeProposal> current_proposals;
    std::string active_plan_id;
};

} // namespace rawrxd::agentic
