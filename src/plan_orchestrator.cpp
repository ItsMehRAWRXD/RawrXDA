/**
 * \file plan_orchestrator.cpp
 * \brief AI-driven multi-file edit coordinator implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "plan_orchestrator.h"
#include "lsp_client.h"


namespace RawrXD {

PlanOrchestrator::PlanOrchestrator(void* parent)
    : void(parent)
{
    // Lightweight constructor - defer initialization
}

void PlanOrchestrator::initialize() {
    if (m_initialized) return;
    
    m_initialized = true;
}

void PlanOrchestrator::setLSPClient(LSPClient* client) {
    m_lspClient = client;
}

void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine) {
    m_inferenceEngine = engine;
}

void PlanOrchestrator::setWorkspaceRoot(const std::string& root) {
    m_workspaceRoot = root;
}

PlanningResult PlanOrchestrator::generatePlan(const std::string& prompt, 
                                               const std::string& workspaceRoot,
                                               const std::vector<std::string>& contextFiles) {
    planningStarted(prompt);
    
    PlanningResult result;
    
    if (!m_inferenceEngine) {
        result.errorMessage = "No inference engine available";
        errorOccurred(result.errorMessage);
        return result;
    }
    
    // Gather context files if not provided
    std::vector<std::string> filesToAnalyze = contextFiles;
    if (filesToAnalyze.empty()) {
        filesToAnalyze = gatherContextFiles(workspaceRoot);
    }
    
    // Build planning prompt with context
    std::string planningPrompt = buildPlanningPrompt(prompt, filesToAnalyze);


    // TODO: Call inference engine to generate plan
    // For now, return a stub result
    result.success = true;
    result.planDescription = "Generated plan for: " + prompt;
    result.affectedFiles = filesToAnalyze.mid(0, 3);  // Stub: first 3 files
    result.estimatedChanges = 5;
    
    // Create stub tasks
    EditTask stubTask;
    stubTask.filePath = workspaceRoot + "/test.cpp";
    stubTask.startLine = 0;
    stubTask.endLine = 0;
    stubTask.operation = "insert";
    stubTask.newText = "// Refactored by AI\n";
    stubTask.description = "Add AI refactoring comment";
    stubTask.priority = 1;
    
    result.tasks.append(stubTask);
    
    planningCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    executionStarted(plan.tasks.size());
    
    ExecutionResult result;
    
    if (!plan.success) {
        result.errorMessage = "Invalid plan: " + plan.errorMessage;
        errorOccurred(result.errorMessage);
        return result;
    }
    
    // Backup original file contents for rollback
    m_originalFileContents.clear();
    for (const std::string& filePath : plan.affectedFiles) {
        std::string content = readFileContent(filePath);
        if (!content.isNull()) {
            m_originalFileContents[filePath] = content;
        }
    }
    
    // Sort tasks by priority (higher first)
    std::vector<EditTask> sortedTasks = plan.tasks;
    std::sort(sortedTasks.begin(), sortedTasks.end(), 
              [](const EditTask& a, const EditTask& b) {
                  return a.priority > b.priority;
              });
    
    // Execute tasks
    int taskIndex = 0;
    for (const EditTask& task : sortedTasks) {
        
        bool success = executeTask(task, dryRun);
        
        if (success) {
            result.successCount++;
            if (!result.successfulFiles.contains(task.filePath)) {
                result.successfulFiles.append(task.filePath);
            }
        } else {
            result.failureCount++;
            if (!result.failedFiles.contains(task.filePath)) {
                result.failedFiles.append(task.filePath);
            }
        }
        
        taskExecuted(taskIndex, success, task.description);
        taskIndex++;
    }
    
    // Check overall success
    result.success = (result.failureCount == 0);
    
    // Rollback on failure (unless dry run)
    if (!result.success && !dryRun) {
        for (auto it = m_originalFileContents.begin(); it != m_originalFileContents.end(); ++it) {
            writeFileContent(it.key(), it.value());
        }
        result.errorMessage = std::string("Execution failed with %1 errors, changes rolled back")
                                  ;
    }
    
    executionCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::planAndExecute(const std::string& prompt,
                                                  const std::string& workspaceRoot,
                                                  bool dryRun) {
    PlanningResult plan = generatePlan(prompt, workspaceRoot);
    
    if (!plan.success) {
        ExecutionResult result;
        result.errorMessage = "Planning failed: " + plan.errorMessage;
        return result;
    }
    
    return executePlan(plan, dryRun);
}

std::string PlanOrchestrator::buildPlanningPrompt(const std::string& userPrompt, 
                                               const std::vector<std::string>& contextFiles) {
    std::string prompt = "You are an AI code refactoring assistant. ";
    prompt += "Analyze the following codebase and generate a detailed edit plan ";
    prompt += "to accomplish this task:\n\n";
    prompt += "TASK: " + userPrompt + "\n\n";
    
    prompt += "CONTEXT FILES:\n";
    for (const std::string& filePath : contextFiles) {
        std::string content = readFileContent(filePath);
        if (!content.empty()) {
            prompt += "\n=== " + filePath + " ===\n";
            prompt += content.left(2000);  // Limit to 2000 chars per file
            if (content.length() > 2000) {
                prompt += "\n... (truncated) ...\n";
            }
        }
    }
    
    prompt += "\n\nGenerate a JSON array of edit tasks with this structure:\n";
    prompt += "[\n";
    prompt += "  {\n";
    prompt += "    \"filePath\": \"absolute/path/to/file.cpp\",\n";
    prompt += "    \"startLine\": 0,\n";
    prompt += "    \"endLine\": 0,\n";
    prompt += "    \"operation\": \"replace|insert|delete|rename\",\n";
    prompt += "    \"oldText\": \"text to replace\",\n";
    prompt += "    \"newText\": \"replacement text\",\n";
    prompt += "    \"description\": \"Human-readable description\",\n";
    prompt += "    \"priority\": 1\n";
    prompt += "  }\n";
    prompt += "]\n";
    
    return prompt;
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const std::string& response) {
    PlanningResult result;
    
    // Try to parse JSON response
    void* doc = void*::fromJson(response.toUtf8());
    
    if (!doc.isArray()) {
        result.errorMessage = "Invalid JSON response (expected array)";
        return result;
    }
    
    void* tasksArray = doc.array();
    
    for (const void*& val : tasksArray) {
        if (!val.isObject()) continue;
        
        void* taskObj = val.toObject();
        
        EditTask task;
        task.filePath = taskObj["filePath"].toString();
        task.startLine = taskObj["startLine"].toInt();
        task.endLine = taskObj["endLine"].toInt();
        task.operation = taskObj["operation"].toString();
        task.oldText = taskObj["oldText"].toString();
        task.newText = taskObj["newText"].toString();
        task.symbolName = taskObj["symbolName"].toString();
        task.newSymbolName = taskObj["newSymbolName"].toString();
        task.description = taskObj["description"].toString();
        task.priority = taskObj["priority"].toInt(0);
        
        result.tasks.append(task);
        
        if (!result.affectedFiles.contains(task.filePath)) {
            result.affectedFiles.append(task.filePath);
        }
    }
    
    result.estimatedChanges = result.tasks.size();
    result.success = !result.tasks.empty();
    
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
             << "lines" << task.startLine << "-" << task.endLine;
    
    if (dryRun) {
                 << "with:" << task.newText;
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    // Simple replace (TODO: use line-based replacement)
    std::string newContent = content.replace(task.oldText, task.newText);
    
    return writeFileContent(task.filePath, newContent);
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) {
             << "at line" << task.startLine;
    
    if (dryRun) {
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    std::vector<std::string> lines = content.split('\n');
    
    if (task.startLine < 0 || task.startLine > lines.size()) {
        return false;
    }
    
    lines.insert(task.startLine, task.newText);
    
    return writeFileContent(task.filePath, lines.join('\n'));
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) {
             << "lines" << task.startLine << "-" << task.endLine;
    
    if (dryRun) {
                 << task.startLine << "-" << task.endLine;
        return true;
    }
    
    std::string content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    std::vector<std::string> lines = content.split('\n');
    
    if (task.startLine < 0 || task.endLine >= lines.size()) {
        return false;
    }
    
    // Remove lines from startLine to endLine (inclusive)
    for (int i = task.endLine; i >= task.startLine; --i) {
        lines.removeAt(i);
    }
    
    return writeFileContent(task.filePath, lines.join('\n'));
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) {
             << "to" << task.newSymbolName;
    
    if (dryRun) {
                 << task.symbolName << "to" << task.newSymbolName;
        return true;
    }
    
    // TODO: Use LSP rename request for intelligent symbol renaming
    if (m_lspClient && m_lspClient->isRunning()) {
        // m_lspClient->requestRename(task.filePath, task.startLine, task.symbolName, task.newSymbolName);
    }
    
    // Fallback: simple text replacement
    std::string content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    std::string newContent = content.replace(task.symbolName, task.newSymbolName);
    
    return writeFileContent(task.filePath, newContent);
}

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles) {
    std::vector<std::string> files;
    
    std::vector<std::string> nameFilters;
    nameFilters << "*.cpp" << "*.h" << "*.hpp" << "*.cc" << "*.cxx"
                << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs";
    
    QDirIterator it(workspaceRoot, nameFilters, std::filesystem::path::Files, 
                    QDirIterator::Subdirectories);
    
    while (itfalse && files.size() < maxFiles) {
        std::string filePath = it;
        
        // Skip build directories
        if (filePath.contains("/build/") || filePath.contains("\\build\\")) {
            continue;
        }
        
        files.append(filePath);
    }
    
    return files;
}

std::string PlanOrchestrator::readFileContent(const std::string& filePath) {
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::string();
    }
    
    QTextStream in(&file);
    std::string content = in.readAll();
    file.close();
    
    return content;
}

bool PlanOrchestrator::writeFileContent(const std::string& filePath, const std::string& content) {
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    return true;
}

} // namespace RawrXD


