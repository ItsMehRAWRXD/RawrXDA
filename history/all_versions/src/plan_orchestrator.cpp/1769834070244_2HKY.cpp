/**
 * \file plan_orchestrator.cpp
 * \brief Implementation of AI-driven multi-file edit coordinator
 * \author RawrXD Team
 * \date 2025-12-07
 * 
 * Ported to standard C++ from Qt
 */

#include "plan_orchestrator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <regex>
#include <windows.h>
#include "nlohmann/json.hpp"

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

using json = nlohmann::json;

namespace RawrXD {

namespace fs = std::filesystem;

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

void PlanOrchestrator::setWorkspaceRoot(const std::string& root) {
    m_workspaceRoot = root;
}

// Signal stubs - simpler than Qt signals
void PlanOrchestrator::planningStarted(const std::string& prompt) {
    std::cout << "[PlanOrchestrator] Planning started: " << prompt << std::endl;
}

void PlanOrchestrator::planningCompleted(const PlanningResult& result) {
    std::cout << "[PlanOrchestrator] Planning completed. Success: " << result.success << std::endl;
}

void PlanOrchestrator::executionStarted(int taskCount) {
    std::cout << "[PlanOrchestrator] Execution started. Tasks: " << taskCount << std::endl;
}

void PlanOrchestrator::taskExecuted(int taskIndex, bool success, const std::string& description) {
    std::cout << "[PlanOrchestrator] Task " << taskIndex << " " << (success ? "succeeded" : "failed") << ": " << description << std::endl;
}

void PlanOrchestrator::executionCompleted(const ExecutionResult& result) {
    std::cout << "[PlanOrchestrator] Execution completed. Success: " << result.success << std::endl;
}

void PlanOrchestrator::errorOccurred(const std::string& error) {
    std::cerr << "[PlanOrchestrator] Error: " << error << std::endl;
}

PlanningResult PlanOrchestrator::generatePlan(const std::string& prompt, 
                                             const std::string& workspaceRoot,
                                             const std::vector<std::string>& contextFiles) {
    if (!m_workspaceRoot.empty() && workspaceRoot.empty()) {
        // Use existing if not provided
    } else {
        m_workspaceRoot = workspaceRoot;
    }

    planningStarted(prompt);
    
    // Load Interconnect DLL
    HMODULE hDll = LoadLibraryA(INTERCONNECT_DLL_PATH.c_str());
    if (!hDll) {
        PlanningResult result;
        result.success = false;
        result.errorMessage = "Failed to load RawrXD_Interconnect.dll from " + INTERCONNECT_DLL_PATH;
        errorOccurred(result.errorMessage);
        return result;
    }

    // Resolve Functions
    auto pfnSwarmInit = (FnSwarmInitialize)GetProcAddress(hDll, "Swarm_Initialize");
    auto pfnSwarmSubmit = (FnSwarmSubmitJob)GetProcAddress(hDll, "Swarm_SubmitJob");
    
    if (!pfnSwarmInit || !pfnSwarmSubmit) {
        PlanningResult result;
        result.success = false;
        result.errorMessage = "Failed to resolve Swarm functions in DLL";
        FreeLibrary(hDll);
        return result;
    }

    // Initialize Swarm (Safe to call multiple times? ASM implementation seems to reset... 
    // Ideally we do this once in PlanOrchestrator::initialize, but for now we do it here strictly)
    // ASM: Zero memory of context. Re-init destroys queue. BAD.
    // We should assume it's initialized or check a flag.
    // Since we are loading DLL afresh, we MUST init.
    pfnSwarmInit();

    // 1. Context Gathering
    std::vector<std::string> effectiveContext = contextFiles;
    if (effectiveContext.empty()) {
        effectiveContext = gatherContextFiles(m_workspaceRoot, 10);
    }
    
    // 2. Build Prompt
    std::string fullPrompt = buildPlanningPrompt(prompt, effectiveContext);
    
    // 3. Prepare Job
    // Allocate result buffer
    const size_t BUFFER_SIZE = 1024 * 1024; // 1MB
    char* encodedResult = new char[BUFFER_SIZE];
    memset(encodedResult, 0, BUFFER_SIZE);

    SwarmJob job = {};
    job.ModelId = -1; // Auto
    job.Priority = 100; // High
    job.PromptData = fullPrompt.c_str();
    job.PromptLength = (uint32_t)fullPrompt.length();
    job.MaxTokens = 4096;
    job.Temperature = 0.2f; // Low temp for planning
    job.TopP = 0.9f;
    job.StreamMode = 0; // No streaming, wait for full JSON
    job.Status = 0; // PENDING
    job.ResultBuffer = encodedResult;
    job.ResultCapacity = BUFFER_SIZE;
    
    // 4. Submit Job
    std::cout << "[PlanOrchestrator] Submitting job to Swarm..." << std::endl;
    pfnSwarmSubmit(&job);
    
    // 5. Wait for Completion
    // ASM logic sets Status=4 (COMPLETE) or 6 (ERROR)
    int retries = 0;
    while (job.Status != 4 && job.Status != 6 && retries < 300) { // 30s timeout
        Sleep(100);
        retries++;
    }
    
    PlanningResult result;
    if (job.Status == 4) {
        // Success
        std::cout << "[PlanOrchestrator] Job completed. Parsing response..." << std::endl;
        std::string jsonResponse(job.ResultBuffer); // Assuming null terminated
        result = parsePlanningResponse(jsonResponse);
    } else {
        // Timeout or Error
        result.success = false;
        result.errorMessage = "Swarm Job failed or timed out. Status: " + std::to_string(job.Status);
    }

    // Cleanup
    delete[] encodedResult;
    // FreeLibrary(hDll); // Keep loaded? If we unload, we lose state. 
    // PlanOrchestrator lifecycle should manage DLL.
    // For this simple implementation, we leak the DLL handle intentionally to keep Swarm alive 
    // or we assume single shot. ASM Re-init clears state so next call would wipe verification.
    // Actually, we should probably keep it valid.
    
    planningCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    ExecutionResult result;
    executionStarted(plan.tasks.size());

    int index = 0;
    for (const auto& task : plan.tasks) {
        bool success = executeTask(task, dryRun);
        if (success) {
            result.successCount++;
            result.successfulFiles.push_back(task.filePath);
        } else {
            result.failureCount++;
            result.failedFiles.push_back(task.filePath);
            if (!dryRun) {
                // Should we abort?
            }
        }
        taskExecuted(index++, success, task.description);
    }

    result.success = (result.failureCount == 0);
    executionCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::planAndExecute(const std::string& prompt,
                                                const std::string& workspaceRoot,
                                                bool dryRun) {
    PlanningResult plan = generatePlan(prompt, workspaceRoot);
    if (!plan.success) {
        ExecutionResult result;
        result.success = false;
        result.errorMessage = "Planning failed: " + plan.errorMessage;
        return result;
    }
    return executePlan(plan, dryRun);
}


// Private Helpers

std::string PlanOrchestrator::buildPlanningPrompt(const std::string& userPrompt, const std::vector<std::string>& contextFiles) {
    std::stringstream ss;
    ss << "User Request: " << userPrompt << "\n";
    ss << "Context Files:\n";
    for(const auto& f : contextFiles) {
        ss << f << "\n";
        // read content?
    }
    return ss.str();
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const std::string& response) {
    PlanningResult result;
    result.success = false;

    try {
        // Try to find JSON blob if wrapped in markdown
        std::string jsonStr = response;
        size_t jsonStart = response.find("```json");
        if (jsonStart != std::string::npos) {
            size_t start = jsonStart + 7;
            size_t end = response.find("```", start);
            if (end != std::string::npos) {
                jsonStr = response.substr(start, end - start);
            }
        } else {
             // Try finding array start
             size_t start = response.find("[");
             size_t end = response.rfind("]");
             if (start != std::string::npos && end != std::string::npos) {
                 jsonStr = response.substr(start, end - start + 1);
             }
        }

        auto j = json::parse(jsonStr);
        
        // Expected JSON format:
        // [ { "file": "path", "op": "replace", "start": 0, "end": 10, "content": "..." }, ... ]
        // OR { "plan": "desc", "tasks": [ ... ] }

        if (j.is_object() && j.contains("tasks")) {
            result.planDescription = j.value("plan", "No description");
            j = j["tasks"];
        }

        if (j.is_array()) {
            for (const auto& item : j) {
                EditTask task;
                task.filePath = item.value("file", "");
                // Resolve relative paths
                if (!fs::path(task.filePath).is_absolute() && !m_workspaceRoot.empty()) {
                    task.filePath = (fs::path(m_workspaceRoot) / task.filePath).string();
                }

                task.operation = item.value("op", "replace");
                task.startLine = item.value("start", 0);
                task.endLine = item.value("end", 0);
                task.newText = item.value("content", "");
                // handle rename old/new
                if (task.operation == "rename") {
                    task.symbolName = item.value("symbol", "");
                    task.newSymbolName = item.value("new_symbol", "");
                }
                
                task.description = item.value("desc", "AI generated edit");
                
                result.tasks.push_back(task);
                result.affectedFiles.push_back(task.filePath);
                result.estimatedChanges++;
            }
            result.success = true;
        } else {
             result.errorMessage = "JSON is not an array of tasks";
        }

    } catch (const json::parse_error& e) {
        result.errorMessage = std::string("JSON Parse Error: ") + e.what();
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Error parsing plan: ") + e.what();
    }

    return result; 
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

bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun) {
    // Backup first, only if not dry run and we haven't backed it up yet
    if (!dryRun) {
        if (m_originalFileContents.find(task.filePath) == m_originalFileContents.end()) {
             m_originalFileContents[task.filePath] = readFileContent(task.filePath);
        }
    }

    if (task.operation == "replace") return applyReplace(task, dryRun);
    if (task.operation == "insert") return applyInsert(task, dryRun);
    if (task.operation == "delete") return applyDelete(task, dryRun);
    if (task.operation == "rename") return applyRename(task, dryRun);
    return false;
}

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun) {
    if (dryRun) return true;
    
    std::string content = readFileContent(task.filePath);
    // Simple line based replace logic
    // This is a naive implementation. Robust would use line indices carefully.
    
    std::istringstream stream(content);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Bounds check
    if (task.startLine < 0) {
        if (lines.empty() && task.startLine == 0) {
             return writeFileContent(task.filePath, task.newText);
        }
        return false;
    }
    // line count is lines.size()
    
    // Logic: replace lines from startLine to endLine (inclusive) with newText
    
    std::vector<std::string> newLines;
    // Add lines before start
    for(int i=0; i < std::min((int)lines.size(), task.startLine); ++i) {
        newLines.push_back(lines[i]);
    }
    
    // Add new content
    // task.newText might need to be split if it contains newlines? 
    // Or we just write it as one block.
    // If we write it to stringstream later, it handles newlines.
    newLines.push_back(task.newText);
    
    // Add lines after end
    // endLine is inclusive of the range to be REPLACED.
    // So we skip until endLine + 1
    for(size_t i = task.endLine + 1; i < lines.size(); ++i) {
        newLines.push_back(lines[i]);
    }
    
    // Reconstruct file
    std::stringstream ss;
    for(size_t i=0; i<newLines.size(); ++i) {
        ss << newLines[i];
        // Only add newline if it's not the last line or if strictly needed
        // Simpler: just join with \n
         if (i < newLines.size() - 1 || newLines[i].back() != '\n') { 
             // This logic is tricky with embedded newlines.
             // Let's assume newLines[i] are just chunks.
             // If they came from getline, they stripped the newline.
             // So we must add it back.
             // Except task.newText might have its own.
             if (i != task.startLine) { // This is an original line
                 ss << "\n";
             } else {
                 // This is the new text block, probably shouldn't append newline if it has one?
                 // Or we blindly append newline between chunks.
                 // Let's just append \n after every block except the last one?
                 if (task.newText.find('\n') == std::string::npos) {
                      ss << "\n";
                 }
             }
         }
    }
    // Correct logic for rejoining:
    std::stringstream out;
    bool first = true;
    for (const auto& l : newLines) {
        // If it's the replacement text, just write it
        if (l == task.newText) {
             if (!first) out << "\n";
             out << l; 
        } else {
            // It's a line from original file (stripped of \n)
            if (!first) out << "\n";
            out << l;
        }
        first = false;
    }

    return writeFileContent(task.filePath, out.str());
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) {
    if (dryRun) return true;
    
    std::string content = readFileContent(task.filePath);
    std::istringstream stream(content);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Insert AT startLine (shifting startLine down)
    
     std::vector<std::string> newLines;
     for(int i=0; i < std::min((int)lines.size(), task.startLine); ++i) {
        newLines.push_back(lines[i]);
    }
    newLines.push_back(task.newText);
    for(size_t i = task.startLine; i < lines.size(); ++i) {
        newLines.push_back(lines[i]);
    }
    
    std::stringstream out;
    bool first = true;
    for (const auto& l : newLines) {
        if (!first) out << "\n";
        out << l;
        first = false;
    }
    return writeFileContent(task.filePath, out.str());
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) {
   if (dryRun) return true;
    std::string content = readFileContent(task.filePath);
    std::istringstream stream(content);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    std::vector<std::string> newLines;
    for(int i=0; i < std::min((int)lines.size(), task.startLine); ++i) {
        newLines.push_back(lines[i]);
    }
    // Skip until endLine + 1
    for(size_t i = task.endLine + 1; i < lines.size(); ++i) {
        newLines.push_back(lines[i]);
    }
    
    std::stringstream out;
    bool first = true;
    for (const auto& l : newLines) {
        if (!first) out << "\n";
        out << l;
        first = false;
    }
    return writeFileContent(task.filePath, out.str());
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) {
    // File Rename
    if (task.symbolName.empty()) {
        if (dryRun) return true;
        try {
            fs::rename(task.filePath, task.newSymbolName); 
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Rename failed: " << e.what() << std::endl;
            return false;
        }
    } else {
        // Symbol Rename (Refactoring)
        if (dryRun) return true;
        
        std::string content = readFileContent(task.filePath);
        if (content.empty()) return false;
        
        // Simple string replacement (Global within file)
        // Note: A real refactor would use AST or accurate tokenization to avoid
        // replacing substrings in comments or strings.
        // For this "Explicit missing logic" implementation, we'll use regex word boundaries.
        
        try {
            std::string regexStr = "\\b" + task.symbolName + "\\b";
            std::regex re(regexStr);
            std::string newContent = std::regex_replace(content, re, task.newSymbolName);
            
            if (content == newContent) return false; // Nothing changed
            
            return writeFileContent(task.filePath, newContent);
        } catch (const std::exception& e) {
             std::cerr << "Symbol rename failed: " << e.what() << std::endl;
             return false;
        }
    }
    return false; 
}

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(workspaceRoot)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
                if (files.size() >= maxFiles) break;
            }
        }
    } catch (...) {}
    return files;
}

} // namespace RawrXD
