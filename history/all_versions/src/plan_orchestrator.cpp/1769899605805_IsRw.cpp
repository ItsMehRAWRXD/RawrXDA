/**
 * \file plan_orchestrator.cpp
 * \brief Implementation of AI-driven multi-file edit coordinator
 * \author RawrXD Team
 * \date 2025-12-07
 * 
 * Ported to standard C++ from Qt
 */

#include "plan_orchestrator.h"
#include "universal_model_router.h" // Added for advanced model routing
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <regex>
#include <windows.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace RawrXD;

// DLL Function Typedefs
// -----------------------------------------------------------------------------
typedef BOOL (WINAPI *FnSwarmInitialize)();
typedef UINT64 (WINAPI *FnSwarmSubmitJob)(void* pJob);
typedef void (WINAPI *FnAgentRouterInitialize)();
typedef DWORD (WINAPI *FnAgentRouterClassifyIntent)(const char* text, DWORD length);

// SwarmJob Structure (Must match MASM layout)
// -----------------------------------------------------------------------------
#pragma pack(push, 8)
struct SwarmJob {
    uint64_t JobId;
    uint64_t ModelId;       // -1 for auto
    uint32_t Priority;
    uint32_t _pad1;         // Alignment for 8-byte boundary
    uint64_t SubmitTick;
    uint64_t DeadlineTick;
    const char* PromptData;
    uint32_t PromptLength;
    uint32_t MaxTokens;
    float Temperature;
    float TopP;
    uint8_t StreamMode;
    uint8_t _pad2[7];       // Alignment for QWORD CompletionPort
    uint64_t CompletionPort;
    uint32_t Status;
    uint32_t AssignedModel;
    char* ResultBuffer;
    uint64_t ResultCapacity;
    uint64_t CancellationToken;
};
#pragma pack(pop)

// Interconnect DLL Path
// -----------------------------------------------------------------------------
const std::string INTERCONNECT_DLL_PATH = "D:\\rawrxd\\interconnect\\build\\RawrXD_Interconnect.dll";


namespace RawrXD {

PlanOrchestrator::PlanOrchestrator() : m_lspClient(nullptr), m_inferenceEngine(nullptr), m_initialized(false) {
}

void PlanOrchestrator::initialize() {
    m_initialized = true;
    std::cout << "[PlanOrchestrator] Initialized." << std::endl;
}

void PlanOrchestrator::setLSPClient(LSPClient* client) {
    m_lspClient = client;
}

void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine) {
    m_inferenceEngine = engine;
}

void PlanOrchestrator::setModelRouter(RawrXD::UniversalModelRouter* router) {
    m_modelRouter = router;
}

void PlanOrchestrator::setWorkspaceRoot(const std::string& root) {
    m_workspaceRoot = root;
}

// Signals
void PlanOrchestrator::planningStarted(const std::string& prompt) {
    std::cout << "[PlanOrchestrator] PLANNING: " << prompt << std::endl;
}

void PlanOrchestrator::planningCompleted(const PlanningResult& result) {
    std::cout << "[PlanOrchestrator] PLAN COMPLETE. Tasks: " << result.tasks.size() << std::endl;
}

void PlanOrchestrator::executionStarted(int taskCount) {
    std::cout << "[PlanOrchestrator] EXECUTION START. Count: " << taskCount << std::endl;
}

void PlanOrchestrator::taskExecuted(int taskIndex, bool success, const std::string& description) {
    std::cout << " -> Task " << taskIndex << " [" << (success ? "OK" : "FAIL") << "] " << description << std::endl;
}

void PlanOrchestrator::executionCompleted(const ExecutionResult& result) {
    std::cout << "[PlanOrchestrator] EXECUTION FINISHED. Success: " << result.success << std::endl;
}

void PlanOrchestrator::errorOccurred(const std::string& error) {
    std::cerr << "[PlanOrchestrator] ERROR: " << error << std::endl;
}

// -----------------------------------------------------------------------------
// Core Logic
// -----------------------------------------------------------------------------

PlanningResult PlanOrchestrator::generatePlan(const std::string& prompt, 
                                            const std::string& workspaceRoot,
                                            const std::vector<std::string>& contextFiles) {
    planningStarted(prompt);
    PlanningResult result;
    result.success = true;
    
    // AI-driven planning using Universal Model Router
    if (m_modelRouter) {
        std::string systemPrompt = R"(You are an expert software architect. 
Analyze the user request and generate a strictly formatted JSON execution plan.
Your response MUST be valid JSON only. No markdown formatting.
Format:
{
  "description": "High level plan description",
  "tasks": [
    {
      "file": "path/to/file.cpp",
      "operation": "replace|insert|delete|rename",
      "start_line": 1, 
      "end_line": 1,
      "search_text": "optional text to find context", 
      "new_text": "text to insert or replace with",
      "description": "Task explanation"
    }
  ]
}
)";
        std::string contextStr = "Workspace: " + workspaceRoot + "\nFiles: ";
        for(const auto& f : contextFiles) contextStr += f + ", ";
        
        std::string fullPrompt = systemPrompt + "\nContext: " + contextStr + "\nRequest: " + prompt;
        
        try {
            std::string response = m_modelRouter->routeQuery("gpt-4", fullPrompt); 
            
            size_t jsonStart = response.find("{");
            size_t jsonEnd = response.rfind("}");
            if (jsonStart != std::string::npos && jsonEnd != std::string::npos) {
                response = response.substr(jsonStart, jsonEnd - jsonStart + 1);
            }
            
            json plan = json::parse(response);
            if (plan.contains("description")) result.planDescription = plan.value("description", "");
            
            if (plan.contains("tasks")) {
                for (const auto& t : plan["tasks"]) {
                    EditTask task;
                    task.filePath = t.value("file", "unknown");
                    if (!std::filesystem::path(task.filePath).is_absolute()) {
                         task.filePath = workspaceRoot + "/" + task.filePath;
                    }
                    task.operation = t.value("operation", "replace");
                    task.startLine = t.value("start_line", 1) - 1;
                    task.endLine = t.value("end_line", 1);
                    task.oldText = t.value("search_text", "");
                    task.newText = t.value("new_text", "");
                    task.description = t.value("description", "AI Task");
                    result.tasks.push_back(task);
                }
                planningCompleted(result);
                return result;
            }
        } catch (const std::exception& e) {
             std::cerr << "[PlanOrchestrator] AI Planning failed: " << e.what() << ". Falling back to heuristics." << std::endl;
        }
    }
    
    // Heuristic planning fallback
    std::string lowP = prompt;
    std::transform(lowP.begin(), lowP.end(), lowP.begin(), ::tolower);
    
    // Simple heuristic parser
    if (lowP.find("rename") != std::string::npos) {
        // e.g. "Rename class Foo to Bar in src/foo.cpp"
        EditTask task;
        task.operation = "rename";
        task.description = "Rename symbol based on request";
        task.priority = 1;
        // Mock parsing
        task.filePath = workspaceRoot + "/src/target.cpp"; // Dummy
        result.tasks.push_back(task);
    } 
    else if (lowP.find("create") != std::string::npos && lowP.find("file") != std::string::npos) {
         EditTask task;
         task.operation = "insert";
         task.description = "Create new file";
         task.filePath = workspaceRoot + "/new_file.cpp";
         task.newText = "// New file content";
         task.startLine = 0;
         result.tasks.push_back(task);
    }
    else {
        // General investigation task? 
        // For now, return empty or dummy
        result.planDescription = "No clear actionable tasks parsed from prompt.";
    }
    
    planningCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    ExecutionResult result;
    executionStarted((int)plan.tasks.size());
    
    int index = 0;
    for (const auto& task : plan.tasks) {
        bool ok = executeTask(task, dryRun);
        taskExecuted(index++, ok, task.description);
        
        if (ok) {
            result.successCount++;
            result.successfulFiles.push_back(task.filePath);
        } else {
            result.failureCount++;
            result.failedFiles.push_back(task.filePath);
        }
    }
    
    result.success = (result.failureCount == 0);
    executionCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::planAndExecute(const std::string& prompt,
                                               const std::string& workspaceRoot,
                                               bool dryRun) {
    PlanningResult plan = generatePlan(prompt, workspaceRoot);
    if (!plan.success || plan.tasks.empty()) {
        ExecutionResult er;
        er.success = false;
        er.errorMessage = "Planning failed or produced no tasks.";
        return er;
    }
    return executePlan(plan, dryRun);
}

// -----------------------------------------------------------------------------
// Task Execution
// -----------------------------------------------------------------------------

bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun) {
    // Backup for rollback
    if (!dryRun && m_originalFileContents.find(task.filePath) == m_originalFileContents.end()) {
         if (fs::exists(task.filePath)) {
             m_originalFileContents[task.filePath] = readFileContent(task.filePath);
         }
    }

    if (task.operation == "replace") {
        return applyReplace(task, dryRun);
    } else if (task.operation == "insert") {
        return applyInsert(task, dryRun);
    } else if (task.operation == "delete") {
        return applyDelete(task, dryRun);
    } else if (task.operation == "rename") {
        return applyRename(task, dryRun);
    }
    return false;
}

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun) {
    if (dryRun) return true;
    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;
    
    // Very naive line-based replacement
    // In real LSP, we use range. Here we assume generic replace of text
    std::string search = task.oldText;
    size_t pos = content.find(search);
    if (pos != std::string::npos) {
        content.replace(pos, search.length(), task.newText);
        return writeFileContent(task.filePath, content);
    }
    return false;
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) {
    if (dryRun) return true;
    // Append or Create
    if (!fs::exists(task.filePath)) {
        return writeFileContent(task.filePath, task.newText);
    }
    std::string content = readFileContent(task.filePath);
    content += "\n" + task.newText;
    return writeFileContent(task.filePath, content);
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) {
    if (dryRun) return true;
    if (fs::exists(task.filePath)) {
        return fs::remove(task.filePath);
    }
    return false; 
}

std::string PlanOrchestrator::buildPlanningPrompt(const std::string& userPrompt, const std::vector<std::string>& contextFiles) {
    std::stringstream ss;
    ss << "Plan tasks for: " << userPrompt << "\nContext Files:\n";
    for(const auto& f : contextFiles) ss << "- " << f << "\n";
    return ss.str();
}

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles) {
    std::vector<std::string> files;
    if (fs::exists(workspaceRoot)) {
        for(const auto& entry : fs::recursive_directory_iterator(workspaceRoot)) {
             if (entry.is_regular_file()) files.push_back(entry.path().string());
             if (files.size() >= maxFiles) break;
        }
    }
    return files;
}

void PlanOrchestrator::abort() {
    std::cout << "[PlanOrchestrator] Abort requested." << std::endl;
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) {
    if (task.symbolName.empty()) {
        if (dryRun) return true;
        try { 
            fs::rename(task.filePath, task.newSymbolName); 
            return true; 
        } catch(...) { return false; }
    }
    if (dryRun) return true;
    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;
    try {
        std::regex re("\\b" + task.symbolName + "\\b");
        std::string newContent = std::regex_replace(content, re, task.newSymbolName);
        return content != newContent && writeFileContent(task.filePath, newContent);
    } catch(...) { return false; }
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const std::string& response) {
    PlanningResult result;
    result.success = false;
    try {
        std::string jsonStr = response;
        size_t jsonStart = response.find("```json");
        if (jsonStart != std::string::npos) {
            size_t start = jsonStart + 7;
            size_t end = response.find("```", start);
            if (end != std::string::npos) jsonStr = response.substr(start, end - start);
        } else {
             size_t start = response.find("{");
             size_t end = response.rfind("}");
             if (start != std::string::npos && end != std::string::npos) 
                 jsonStr = response.substr(start, end - start + 1);
        }
        auto j = json::parse(jsonStr);
        if (j.contains("description")) result.planDescription = j.value("description", "");
        json tasks = j.contains("tasks") ? j["tasks"] : (j.is_array() ? j : json::array());
        for (const auto& item : tasks) {
            EditTask task;
            task.filePath = item.value("file", "");
            if (!task.filePath.empty() && !fs::path(task.filePath).is_absolute() && !m_workspaceRoot.empty()) {
                 task.filePath = (fs::path(m_workspaceRoot) / task.filePath).string();
            }
            task.operation = item.value("operation", item.value("op", "replace"));
            task.startLine = item.value("start_line", item.value("start", 1));
            if (task.startLine > 0) task.startLine--; 
            task.endLine = item.value("end_line", item.value("end", task.startLine));
            task.oldText = item.value("search_text", "");
            task.newText = item.value("new_text", item.value("content", ""));
            task.description = item.value("description", "AI Edit");
            if (task.operation == "rename") {
                task.symbolName = item.value("symbol", "");
                task.newSymbolName = item.value("new_symbol", "");
            }
            result.tasks.push_back(task);
        }
        result.success = !result.tasks.empty();
    } catch (...) {}
    return result;
}

} // namespace RawrXD

}
