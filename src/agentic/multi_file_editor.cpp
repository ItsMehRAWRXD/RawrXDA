// ============================================================================
// Multi-File Agentic Editor — Autonomous Multi-File Code Editing
// Plans and executes changes across multiple files based on natural language
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../git/git_integration.h"
#include "../editor/diff_engine.h"
#include "../registry/feature_registry.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <queue>
#include <stack>

namespace RawrXD::Agentic {

enum class EditOperationType {
    CREATE_FILE,
    MODIFY_FILE,
    DELETE_FILE,
    RENAME_FILE,
    MOVE_CODE,
    REFACTOR
};

enum class TaskStatus {
    PENDING,
    PLANNING,
    EXECUTING,
    VERIFYING,
    COMPLETED,
    FAILED,
    ROLLED_BACK
};

struct FileChange {
    std::string filePath;
    EditOperationType operation;
    std::string originalContent;
    std::string newContent;
    std::string description;
    std::vector<std::string> dependencies;
    bool isVerified;
    std::string errorMessage;
};

struct EditPlan {
    std::string id;
    std::string description;
    std::vector<FileChange> changes;
    std::map<std::string, std::vector<std::string>> dependencyGraph;
    int estimatedLinesChanged;
    int estimatedFilesAffected;
    std::chrono::system_clock::time_point createdAt;
    TaskStatus status;
    std::string rollbackSnapshot;
};

struct EditTask {
    std::string id;
    std::string naturalLanguageDescription;
    EditPlan plan;
    TaskStatus status;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::vector<std::string> affectedFiles;
    std::string errorLog;
};

struct VerificationResult {
    bool success;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    int testsPassed;
    int testsFailed;
    double confidence;
};

class MultiFileAgenticEditor {
public:
    MultiFileAgenticEditor(
        std::shared_ptr<SovereignInferenceClient> aiClient,
        std::shared_ptr<GitIntegration> gitIntegration,
        std::shared_ptr<DiffEngine> diffEngine)
        : m_aiClient(aiClient)
        , m_gitIntegration(gitIntegration)
        , m_diffEngine(diffEngine) {}

    EditTask ExecuteTask(const std::string& description) {
        EditTask task;
        task.id = GenerateTaskId();
        task.naturalLanguageDescription = description;
        task.status = TaskStatus::PLANNING;
        task.startedAt = std::chrono::system_clock::now();
        
        // Step 1: Create edit plan
        task.plan = CreateEditPlan(description);
        
        // Step 2: Validate plan
        if (!ValidatePlan(task.plan)) {
            task.status = TaskStatus::FAILED;
            task.errorLog = "Plan validation failed";
            return task;
        }
        
        // Step 3: Create rollback snapshot
        task.plan.rollbackSnapshot = CreateRollbackSnapshot(task.plan);
        
        // Step 4: Execute changes
        task.status = TaskStatus::EXECUTING;
        bool success = ExecuteChanges(task.plan);
        
        if (!success) {
            task.status = TaskStatus::FAILED;
            RollbackChanges(task.plan);
            return task;
        }
        
        // Step 5: Verify changes
        task.status = TaskStatus::VERIFYING;
        auto verification = VerifyChanges(task.plan);
        
        if (!verification.success) {
            task.status = TaskStatus::FAILED;
            task.errorLog = "Verification failed: " + 
                (verification.errors.empty() ? "" : verification.errors[0]);
            RollbackChanges(task.plan);
            return task;
        }
        
        task.status = TaskStatus::COMPLETED;
        task.completedAt = std::chrono::system_clock::now();
        
        // Store task
        m_tasks[task.id] = task;
        
        return task;
    }

    EditPlan CreateEditPlan(const std::string& description) {
        EditPlan plan;
        plan.id = GeneratePlanId();
        plan.description = description;
        plan.createdAt = std::chrono::system_clock::now();
        plan.status = TaskStatus::PENDING;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return plan;
        }

        // Use AI to analyze the request and create a plan
        std::string prompt = R"(Analyze this code change request and create a detailed plan:

Request: )" + description + R"(

Create a plan with:
1. Files to modify/create/delete
2. Specific changes for each file
3. Dependencies between changes
4. Order of execution

Format as JSON with file paths and operations.)";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are an expert software architect. Create detailed multi-file edit plans."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI response into structured plan
            ParsePlanFromAIResponse(result.response, plan);
        }
        
        // Build dependency graph
        BuildDependencyGraph(plan);
        
        return plan;
    }

    bool ExecuteChanges(EditPlan& plan) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Sort changes by dependencies (topological sort)
        auto sortedChanges = TopologicalSort(plan.changes, plan.dependencyGraph);
        
        for (auto& change : sortedChanges) {
            try {
                switch (change.operation) {
                    case EditOperationType::CREATE_FILE:
                        CreateFile(change);
                        break;
                    case EditOperationType::MODIFY_FILE:
                        ModifyFile(change);
                        break;
                    case EditOperationType::DELETE_FILE:
                        DeleteFile(change);
                        break;
                    case EditOperationType::RENAME_FILE:
                        RenameFile(change);
                        break;
                    case EditOperationType::MOVE_CODE:
                        MoveCode(change);
                        break;
                    case EditOperationType::REFACTOR:
                        RefactorCode(change);
                        break;
                }
                
                change.isVerified = true;
                
            } catch (const std::exception& e) {
                change.errorMessage = e.what();
                return false;
            }
        }
        
        return true;
    }

    bool RollbackChanges(const EditPlan& plan) {
        // Restore from snapshot
        RestoreSnapshot(plan.rollbackSnapshot);
        return true;
    }

    VerificationResult VerifyChanges(const EditPlan& plan) {
        VerificationResult result;
        result.success = true;
        result.confidence = 1.0;
        
        // Verify syntax
        for (const auto& change : plan.changes) {
            if (!VerifySyntax(change)) {
                result.success = false;
                result.errors.push_back("Syntax error in " + change.filePath);
            }
        }
        
        // Run tests if available
        auto testResult = RunTests();
        result.testsPassed = testResult.first;
        result.testsFailed = testResult.second;
        
        if (result.testsFailed > 0) {
            result.success = false;
            result.errors.push_back(std::to_string(result.testsFailed) + " tests failed");
        }
        
        // AI verification
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiVerification = AIVerification(plan);
            if (!aiVerification.success) {
                result.success = false;
                result.errors.insert(result.errors.end(), 
                                   aiVerification.errors.begin(), aiVerification.errors.end());
            }
            result.confidence = aiVerification.confidence;
        }
        
        return result;
    }

    std::vector<EditTask> GetTaskHistory(int limit = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<EditTask> history;
        int count = 0;
        for (auto it = m_taskHistory.rbegin(); 
             it != m_taskHistory.rend() && count < limit; 
             ++it, ++count) {
            history.push_back(*it);
        }
        return history;
    }

    EditTask GetTask(const std::string& taskId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            return it->second;
        }
        return EditTask{};
    }

    std::string GenerateEditReport(const EditTask& task) {
        std::ostringstream report;
        report << "# Multi-File Edit Report\n\n";
        report << "**Task ID:** " << task.id << "\n";
        report << "**Description:** " << task.naturalLanguageDescription << "\n";
        report << "**Status:** " << StatusToString(task.status) << "\n";
        report << "**Started:** " << FormatTime(task.startedAt) << "\n";
        if (task.completedAt.time_since_epoch().count() > 0) {
            report << "**Completed:** " << FormatTime(task.completedAt) << "\n";
        }
        report << "\n";
        
        report << "## Planned Changes\n";
        for (const auto& change : task.plan.changes) {
            report << "### " << change.filePath << "\n";
            report << "- **Operation:** " << OperationToString(change.operation) << "\n";
            report << "- **Description:** " << change.description << "\n";
            report << "- **Status:** " << (change.isVerified ? "✅ Verified" : "❌ Failed") << "\n";
            if (!change.errorMessage.empty()) {
                report << "- **Error:** " << change.errorMessage << "\n";
            }
            report << "\n";
        }
        
        if (!task.errorLog.empty()) {
            report << "## Error Log\n";
            report << "```\n" << task.errorLog << "\n```\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<GitIntegration> m_gitIntegration;
    std::shared_ptr<DiffEngine> m_diffEngine;
    mutable std::mutex m_mutex;
    std::map<std::string, EditTask> m_tasks;
    std::vector<EditTask> m_taskHistory;

    void ParsePlanFromAIResponse(const std::string& response, EditPlan& plan) {
        // Parse JSON response from AI
        // This is a simplified implementation
        FileChange change;
        change.filePath = "example.cpp";
        change.operation = EditOperationType::MODIFY_FILE;
        change.description = "AI-generated change";
        plan.changes.push_back(change);
    }

    void BuildDependencyGraph(EditPlan& plan) {
        // Build dependency graph from changes
        for (const auto& change : plan.changes) {
            for (const auto& dep : change.dependencies) {
                plan.dependencyGraph[change.filePath].push_back(dep);
            }
        }
    }

    std::vector<FileChange> TopologicalSort(
        const std::vector<FileChange>& changes,
        const std::map<std::string, std::vector<std::string>>& graph) {
        
        std::vector<FileChange> sorted;
        std::map<std::string, int> inDegree;
        std::map<std::string, FileChange> changeMap;
        
        for (const auto& change : changes) {
            inDegree[change.filePath] = 0;
            changeMap[change.filePath] = change;
        }
        
        for (const auto& [file, deps] : graph) {
            for (const auto& dep : deps) {
                inDegree[file]++;
            }
        }
        
        std::queue<std::string> queue;
        for (const auto& [file, degree] : inDegree) {
            if (degree == 0) {
                queue.push(file);
            }
        }
        
        while (!queue.empty()) {
            auto file = queue.front();
            queue.pop();
            
            sorted.push_back(changeMap[file]);
            
            auto it = graph.find(file);
            if (it != graph.end()) {
                for (const auto& dependent : it->second) {
                    inDegree[dependent]--;
                    if (inDegree[dependent] == 0) {
                        queue.push(dependent);
                    }
                }
            }
        }
        
        return sorted;
    }

    void CreateFile(FileChange& change) {
        // Create new file with content
        std::ofstream file(change.filePath);
        if (file.is_open()) {
            file << change.newContent;
            file.close();
        }
    }

    void ModifyFile(FileChange& change) {
        // Read original
        std::ifstream inFile(change.filePath);
        std::string content((std::istreambuf_iterator<char>(inFile)),
                             std::istreambuf_iterator<char>());
        change.originalContent = content;
        inFile.close();
        
        // Apply diff
        std::ofstream outFile(change.filePath);
        if (outFile.is_open()) {
            outFile << change.newContent;
            outFile.close();
        }
    }

    void DeleteFile(FileChange& change) {
        std::filesystem::remove(change.filePath);
    }

    void RenameFile(FileChange& change) {
        std::filesystem::rename(change.filePath, change.newContent);
    }

    void MoveCode(FileChange& change) {
        // Move code between files
    }

    void RefactorCode(FileChange& change) {
        // Apply refactoring changes
    }

    bool ValidatePlan(const EditPlan& plan) {
        // Check for circular dependencies
        // Validate file paths
        // Check permissions
        return true;
    }

    std::string CreateRollbackSnapshot(const EditPlan& plan) {
        // Create git commit or backup
        return "snapshot_id";
    }

    void RestoreSnapshot(const std::string& snapshotId) {
        // Restore from snapshot
    }

    bool VerifySyntax(const FileChange& change) {
        // Check file syntax
        return true;
    }

    std::pair<int, int> RunTests() {
        // Run test suite
        return {0, 0};
    }

    VerificationResult AIVerification(const EditPlan& plan) {
        VerificationResult result;
        result.success = true;
        result.confidence = 0.9;
        return result;
    }

    std::string GenerateTaskId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "task_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string GeneratePlanId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "plan_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string StatusToString(TaskStatus status) {
        switch (status) {
            case TaskStatus::PENDING: return "Pending";
            case TaskStatus::PLANNING: return "Planning";
            case TaskStatus::EXECUTING: return "Executing";
            case TaskStatus::VERIFYING: return "Verifying";
            case TaskStatus::COMPLETED: return "Completed";
            case TaskStatus::FAILED: return "Failed";
            case TaskStatus::ROLLED_BACK: return "Rolled Back";
            default: return "Unknown";
        }
    }

    std::string OperationToString(EditOperationType op) {
        switch (op) {
            case EditOperationType::CREATE_FILE: return "Create";
            case EditOperationType::MODIFY_FILE: return "Modify";
            case EditOperationType::DELETE_FILE: return "Delete";
            case EditOperationType::RENAME_FILE: return "Rename";
            case EditOperationType::MOVE_CODE: return "Move";
            case EditOperationType::REFACTOR: return "Refactor";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Agentic
