#include "plan_orchestrator.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace RawrXD {

PlanOrchestrator::PlanOrchestrator() {}

void PlanOrchestrator::initialize() {
    m_initialized = true;
}

void PlanOrchestrator::setLSPClient(LSPClient* client) {
    m_lspClient = client;
}

void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine) {
    m_inferenceEngine = engine;
}

PlanningResult PlanOrchestrator::generatePlan(const std::string& prompt, 
                                            const std::string& workspaceRoot,
                                            const std::vector<std::string>& contextFiles) {
    if (planningStarted) planningStarted(prompt);
    
    PlanningResult result;
    result.planDescription = "Plan for: " + prompt;
    result.success = true;in_strings(const std::vector<std::string>& strings, const std::string& delimiter) {
           std::string result;
    // Stub implementation        for (size_t i = 0; i < strings.size(); ++i) {
    sult += strings[i];
    if (planningCompleted) planningCompleted(result);
    return result;+= delimiter;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    if (executionStarted) executionStarted(plan.tasks.size());
    
    ExecutionResult result;   bool has_extension(const std::string& filename, const std::vector<std::string>& extensions) {
    result.success = true;        for (const auto& ext : extensions) {
    lename.size() >= ext.size() && 
    for (size_t i = 0; i < plan.tasks.size(); ++i) {                filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
        bool success = executeTask(plan.tasks[i], dryRun);
        // if (taskExecuted) taskExecuted(i, success, plan.tasks[i].description);
        // Fix: taskExecuted call might specific signature? No, header says: void taskExecuted(int, bool, string)       }
        if (taskExecuted) taskExecuted(static_cast<int>(i), success, plan.tasks[i].description);        return false;
        
        if (success) {
            result.successCount++;   std::string replace_all(std::string str, const std::string& from, const std::string& to) {
            result.successfulFiles.push_back(plan.tasks[i].filePath);        if(from.empty()) return str;
        } else {
            result.failureCount++;= str.find(from, start_pos)) != std::string::npos) {
            result.failedFiles.push_back(plan.tasks[i].filePath);           str.replace(start_pos, from.length(), to);
            result.success = false;            start_pos += to.length();
        }
    }
       }
    if (executionCompleted) executionCompleted(result);}
    return result;
}

ExecutionResult PlanOrchestrator::planAndExecute(const std::string& prompt,PlanOrchestrator::PlanOrchestrator()
                                               const std::string& workspaceRoot,
                                               bool dryRun) {defer initialization
    PlanningResult plan = generatePlan(prompt, workspaceRoot);
    if (!plan.success) {
        ExecutionResult res;ialize() {
        res.success = false;ess we add state tracking
        res.errorMessage = plan.errorMessage;
        return res;
    }
    return executePlan(plan, dryRun);not stored as member in this stripped implementation
} if needed later.
ethod exists to satisfy the interface.
void PlanOrchestrator::setWorkspaceRoot(const std::string& root) {
    m_workspaceRoot = root;
}void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine) {

// Signals (functions in this case, but header defines them as methods?) 
// Wait, the header declares them as member functions: 
// void planningStarted(const std::string& prompt);
// But usually signal/slot means they are public methods that emit something or callbacks?Root,
// Implementation:std::string>& contextFiles) {
void PlanOrchestrator::planningStarted(const std::string& prompt) {    // planningStarted(prompt);
    // No-op or log
}

void PlanOrchestrator::planningCompleted(const PlanningResult& result) {
}/ if (!m_inferenceEngine) ...

void PlanOrchestrator::executionStarted(int taskCount) {   // Gather context files if not provided
}    std::vector<std::string> filesToAnalyze = contextFiles;

void PlanOrchestrator::taskExecuted(int taskIndex, bool success, const std::string& description) {atherContextFiles(workspaceRoot);
}

void PlanOrchestrator::executionCompleted(const ExecutionResult& result) { context
}Prompt(prompt, filesToAnalyze);

void PlanOrchestrator::errorOccurred(const std::string& error) {gine to generate plan
}

// Private helpers

std::string PlanOrchestrator::buildPlanningPrompt(const std::string& userPrompt, const std::vector<std::string>& contextFiles) {
    return userPrompt;
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const std::string& response) {
    return PlanningResult();
}
t.estimatedChanges = 5;
bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun) {
    if (task.operation == "replace") return applyReplace(task, dryRun);
    if (task.operation == "insert") return applyInsert(task, dryRun);
    if (task.operation == "delete") return applyDelete(task, dryRun);tubTask.filePath = workspaceRoot + "/test.cpp";
    if (task.operation == "rename") return applyRename(task, dryRun);Line = 0;
    return false;   stubTask.endLine = 0;
}    stubTask.operation = "insert";

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun) { return true; }
bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) { return true; }
bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) { return true; }
bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) { return true; }(stubTask);

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles) {
    return {};
}

std::string PlanOrchestrator::readFileContent(const std::string& filePath) {anOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    std::ifstream file(filePath);   // executionStarted(plan.tasks.size());
    if (!file.is_open()) return "";    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}orMessage = "Invalid plan: " + plan.errorMessage;
age);
bool PlanOrchestrator::writeFileContent(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << content; file contents could be implemented here
    return true;
}
EditTask> sortedTasks = plan.tasks;
} // namespace RawrXD sortedTasks.end(), 
            if(taskObj.contains("oldText")) task.oldText = taskObj["oldText"].get<std::string>();
            if(taskObj.contains("newText")) task.newText = taskObj["newText"].get<std::string>();
            if(taskObj.contains("symbolName")) task.symbolName = taskObj["symbolName"].get<std::string>();
            if(taskObj.contains("newSymbolName")) task.newSymbolName = taskObj["newSymbolName"].get<std::string>();
            if(taskObj.contains("description")) task.description = taskObj["description"].get<std::string>();
            if(taskObj.contains("priority")) task.priority = taskObj["priority"].get<int>();
            
            result.tasks.push_back(task);
            
            bool found = false;
            for(const auto& f : result.affectedFiles) {
                if(f == task.filePath) { found = true; break; }
            }
            if (!found) {
                result.affectedFiles.push_back(task.filePath);
            }
        }
        
        result.estimatedChanges = result.tasks.size();
        result.success = !result.tasks.empty();
        
    } catch (const std::exception& e) {
        result.errorMessage = std::string("JSON parsing error: ") + e.what();
        result.success = false;
    }
    
    return result;
}

bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun) {
    if (task.operation == "replace") {
        return applyReplace(task, dryRun);
    } else if (task.operation == "insert") {
        return applyInsert(task, dryRun);
    } else if (task.operation == "delete") {
        return applyDelete(task, dryRun);
    } else if (task.operation == "rename") {
        return applyRename(task, dryRun);
    } else {
        return false;
    }
}

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun) {
    std::cout << "Applying replace on " << task.filePath << " lines " << task.startLine << "-" << task.endLine << std::endl;
    
    if (dryRun) {
        std::cout << "Dry run replace with: " << task.newText << std::endl;
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;
    
    // Simple replace - in a real engine, we'd use lines, but for now replace all occurrences
    std::string newContent = replace_all(content, task.oldText, task.newText);
    
    return writeFileContent(task.filePath, newContent);
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) {
    std::cout << "Applying insert on " << task.filePath << " at line " << task.startLine << std::endl;
    
    if (dryRun) {
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;
    
    std::vector<std::string> lines = split_string(content, '\n');
    
    if (task.startLine < 0 || task.startLine > (int)lines.size()) {
        return false;
    }
    
    auto it = lines.begin() + task.startLine;
    lines.insert(it, task.newText);
    
    return writeFileContent(task.filePath, join_strings(lines, "\n"));
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) {
    std::cout << "Applying delete on " << task.filePath << " lines " << task.startLine << "-" << task.endLine << std::endl;
    
    if (dryRun) {
        return true;
    }

    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;

    std::vector<std::string> lines = split_string(content, '\n');
    
    if (task.startLine < 0 || task.endLine >= (int)lines.size() || task.startLine > task.endLine) {
        return false;
    }

    // Remove lines from startLine to endLine (inclusive) - carefully with iterator invalidation
    lines.erase(lines.begin() + task.startLine, lines.begin() + task.endLine + 1);

    return writeFileContent(task.filePath, join_strings(lines, "\n"));
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) {
    std::cout << "Applying rename on " << task.filePath << " from " << task.symbolName << " to " << task.newSymbolName << std::endl;
    
    if (dryRun) {
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.empty()) return false;
    
    std::string newContent = replace_all(content, task.symbolName, task.newSymbolName);
    
    return writeFileContent(task.filePath, newContent);
}

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles) {
    std::vector<std::string> files;
    std::vector<std::string> extensions = {".cpp", ".h", ".hpp", ".cc", ".cxx", ".py", ".js", ".ts", ".java", ".cs"};
    
    
    try {
        if (!std::filesystem::exists(workspaceRoot)) return files;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspaceRoot)) {
            if (files.size() >= (size_t)maxFiles) break;
            
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string filename = entry.path().filename().string();
                
                // Skip build directories - simple heuristic
                if (path.find("/build/") != std::string::npos || path.find("\\build\\") != std::string::npos) {
                    continue;
                }
                
                if (has_extension(filename, extensions)) {
                    files.push_back(path);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error gathering files: " << e.what() << std::endl;
    }
    
    return files;
}

std::string PlanOrchestrator::readFileContent(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool PlanOrchestrator::writeFileContent(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    file << content;
    return true;
}

} // namespace RawrXD
