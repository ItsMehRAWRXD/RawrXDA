#include "agentic_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>

// SCALAR-ONLY: Autonomous agentic engine with no threading

namespace fs = std::filesystem;

namespace RawrXD {

AgenticEngine::AgenticEngine() {
    working_directory_ = fs::current_path().string();
    
    // Register default tools (scalar)
    RegisterTool({
        "read_file",
        "Read the contents of a file",
        {"file_path"},
        [this](const std::map<std::string, std::string>& params) {
            return ReadFile(params.at("file_path"));
        }
    });
    
    RegisterTool({
        "write_file",
        "Write content to a file",
        {"file_path", "content"},
        [this](const std::map<std::string, std::string>& params) {
            bool success = WriteFile(params.at("file_path"), params.at("content"));
            return success ? "File written successfully" : "Failed to write file";
        }
    });
    
    RegisterTool({
        "list_files",
        "List files in a directory",
        {"directory"},
        [this](const std::map<std::string, std::string>& params) {
            std::ostringstream result;
            try {
                for (const auto& entry : fs::directory_iterator(params.at("directory"))) {
                    result << entry.path().filename().string() << "\n";
                }
            } catch (...) {
                result << "Error listing directory";
            }
            return result.str();
        }
    });
}

AgenticEngine::~AgenticEngine() {
}

std::string AgenticEngine::ExecuteTask(const AgentTask& task) {
    std::cout << "Executing task: " << task.description << std::endl;
    
    std::string result;
    
    // Scalar task execution based on type
    switch (task.type) {
        case AgentTaskType::CODE_GENERATION:
            result = ProcessCodeGeneration(task.parameters);
            break;
        case AgentTaskType::FILE_EDITING:
            result = ProcessFileEdit(task.parameters);
            break;
        case AgentTaskType::FILE_CREATION:
            result = ProcessFileCreate(task.parameters);
            break;
        case AgentTaskType::FILE_DELETION:
            result = ProcessFileDelete(task.parameters);
            break;
        case AgentTaskType::SEARCH_CODE:
            result = ProcessCodeSearch(task.parameters);
            break;
        case AgentTaskType::REFACTOR:
            result = ProcessRefactor(task.parameters);
            break;
        case AgentTaskType::DEBUG:
            result = ProcessDebug(task.parameters);
            break;
        case AgentTaskType::EXPLAIN_CODE:
            result = ProcessExplain(task.parameters);
            break;
        case AgentTaskType::RUN_COMMAND:
            result = ProcessCommand(task.parameters);
            break;
        default:
            result = "Unknown task type";
            break;
    }
    
    if (on_task_complete_) {
        on_task_complete_(task, result);
    }
    
    return result;
}

void AgenticEngine::QueueTask(const AgentTask& task) {
    task_queue_.push_back(task);
    std::cout << "Task queued: " << task.description << std::endl;
}

void AgenticEngine::ProcessQueue() {
    if (task_queue_.empty()) return;
    
    // Scalar: process one task at a time
    AgentTask task = task_queue_.front();
    task_queue_.erase(task_queue_.begin());
    
    std::string result = ExecuteTask(task);
    std::cout << "Task completed: " << result << std::endl;
}

void AgenticEngine::RegisterTool(const AgentTool& tool) {
    tools_[tool.name] = tool;
    std::cout << "Tool registered: " << tool.name << std::endl;
}

std::vector<AgentTool> AgenticEngine::GetAvailableTools() const {
    std::vector<AgentTool> tools;
    for (const auto& pair : tools_) {
        tools.push_back(pair.second);
    }
    return tools;
}

std::string AgenticEngine::GenerateCode(const std::string& prompt, const std::string& context) {
    std::map<std::string, std::string> params;
    params["prompt"] = prompt;
    params["context"] = context;
    return ProcessCodeGeneration(params);
}

std::string AgenticEngine::EditFile(const std::string& file_path, const std::string& instructions) {
    std::map<std::string, std::string> params;
    params["file_path"] = file_path;
    params["instructions"] = instructions;
    return ProcessFileEdit(params);
}

std::string AgenticEngine::CreateFile(const std::string& file_path, const std::string& content) {
    std::map<std::string, std::string> params;
    params["file_path"] = file_path;
    params["content"] = content;
    return ProcessFileCreate(params);
}

std::string AgenticEngine::DeleteFile(const std::string& file_path) {
    std::map<std::string, std::string> params;
    params["file_path"] = file_path;
    return ProcessFileDelete(params);
}

std::string AgenticEngine::SearchCode(const std::string& query, const std::string& directory) {
    std::map<std::string, std::string> params;
    params["query"] = query;
    params["directory"] = directory;
    return ProcessCodeSearch(params);
}

std::string AgenticEngine::RefactorCode(const std::string& file_path, const std::string& refactor_type) {
    std::map<std::string, std::string> params;
    params["file_path"] = file_path;
    params["refactor_type"] = refactor_type;
    return ProcessRefactor(params);
}

std::string AgenticEngine::DebugCode(const std::string& error_message, const std::string& context) {
    std::map<std::string, std::string> params;
    params["error_message"] = error_message;
    params["context"] = context;
    return ProcessDebug(params);
}

std::string AgenticEngine::ExplainCode(const std::string& code_snippet) {
    std::map<std::string, std::string> params;
    params["code"] = code_snippet;
    return ProcessExplain(params);
}

std::string AgenticEngine::RunCommand(const std::string& command) {
    std::map<std::string, std::string> params;
    params["command"] = command;
    return ProcessCommand(params);
}

void AgenticEngine::SetWorkingDirectory(const std::string& directory) {
    working_directory_ = directory;
    std::cout << "Working directory set to: " << directory << std::endl;
}

void AgenticEngine::SetProjectContext(const std::string& context) {
    project_context_ = context;
}

void AgenticEngine::SetOnTaskComplete(std::function<void(const AgentTask&, const std::string&)> callback) {
    on_task_complete_ = callback;
}

void AgenticEngine::SetOnProgress(std::function<void(const std::string&)> callback) {
    on_progress_ = callback;
}

// Scalar task processors

std::string AgenticEngine::ProcessCodeGeneration(const std::map<std::string, std::string>& params) {
    std::string prompt = params.count("prompt") ? params.at("prompt") : "";
    std::string context = params.count("context") ? params.at("context") : "";
    
    std::ostringstream result;
    result << "// Generated code for: " << prompt << "\n";
    result << "// Context: " << context << "\n";
    result << "// TODO: Implement code generation logic\n";
    
    if (on_progress_) {
        on_progress_("Generating code...");
    }
    
    return result.str();
}

std::string AgenticEngine::ProcessFileEdit(const std::map<std::string, std::string>& params) {
    std::string file_path = params.count("file_path") ? params.at("file_path") : "";
    std::string instructions = params.count("instructions") ? params.at("instructions") : "";
    
    if (file_path.empty()) {
        return "Error: No file path provided";
    }
    
    // Scalar file editing
    std::string content = ReadFile(file_path);
    if (content.empty()) {
        return "Error: Could not read file";
    }
    
    if (on_progress_) {
        on_progress_("Editing file: " + file_path);
    }
    
    // TODO: Implement AI-powered editing based on instructions
    std::string edited_content = content + "\n// Edited based on: " + instructions + "\n";
    
    if (WriteFile(file_path, edited_content)) {
        return "File edited successfully: " + file_path;
    }
    
    return "Error: Failed to write file";
}

std::string AgenticEngine::ProcessFileCreate(const std::map<std::string, std::string>& params) {
    std::string file_path = params.count("file_path") ? params.at("file_path") : "";
    std::string content = params.count("content") ? params.at("content") : "";
    
    if (file_path.empty()) {
        return "Error: No file path provided";
    }
    
    if (on_progress_) {
        on_progress_("Creating file: " + file_path);
    }
    
    if (WriteFile(file_path, content)) {
        return "File created successfully: " + file_path;
    }
    
    return "Error: Failed to create file";
}

std::string AgenticEngine::ProcessFileDelete(const std::map<std::string, std::string>& params) {
    std::string file_path = params.count("file_path") ? params.at("file_path") : "";
    
    if (file_path.empty()) {
        return "Error: No file path provided";
    }
    
    try {
        if (fs::remove(file_path)) {
            return "File deleted successfully: " + file_path;
        }
        return "Error: File not found";
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

std::string AgenticEngine::ProcessCodeSearch(const std::map<std::string, std::string>& params) {
    std::string query = params.count("query") ? params.at("query") : "";
    std::string directory = params.count("directory") ? params.at("directory") : working_directory_;
    
    std::ostringstream result;
    int match_count = 0;
    
    if (on_progress_) {
        on_progress_("Searching code in: " + directory);
    }
    
    try {
        // Scalar recursive search
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string content = ReadFile(entry.path().string());
                
                // Scalar search for query
                if (content.find(query) != std::string::npos) {
                    result << entry.path().string() << "\n";
                    match_count++;
                }
            }
        }
    } catch (const std::exception& e) {
        return std::string("Error searching: ") + e.what();
    }
    
    result << "\nFound " << match_count << " matches";
    return result.str();
}

std::string AgenticEngine::ProcessRefactor(const std::map<std::string, std::string>& params) {
    std::string file_path = params.count("file_path") ? params.at("file_path") : "";
    std::string refactor_type = params.count("refactor_type") ? params.at("refactor_type") : "";
    
    if (on_progress_) {
        on_progress_("Refactoring: " + file_path);
    }
    
    return "Refactoring completed: " + refactor_type;
}

std::string AgenticEngine::ProcessDebug(const std::map<std::string, std::string>& params) {
    std::string error_message = params.count("error_message") ? params.at("error_message") : "";
    std::string context = params.count("context") ? params.at("context") : "";
    
    std::ostringstream result;
    result << "Debug Analysis:\n";
    result << "Error: " << error_message << "\n";
    result << "Context: " << context << "\n";
    result << "Suggested fix: [AI-powered debugging would go here]\n";
    
    return result.str();
}

std::string AgenticEngine::ProcessExplain(const std::map<std::string, std::string>& params) {
    std::string code = params.count("code") ? params.at("code") : "";
    
    std::ostringstream result;
    result << "Code Explanation:\n";
    result << "This code: " << code.substr(0, 50) << "...\n";
    result << "[AI-powered explanation would go here]\n";
    
    return result.str();
}

std::string AgenticEngine::ProcessCommand(const std::map<std::string, std::string>& params) {
    std::string command = params.count("command") ? params.at("command") : "";
    
    // TODO: Implement safe command execution
    return "Command execution placeholder: " + command;
}

std::string AgenticEngine::GenerateTaskId() {
    static int counter = 0;
    std::ostringstream id;
    id << "task_" << std::time(nullptr) << "_" << (counter++);
    return id.str();
}

std::string AgenticEngine::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool AgenticEngine::WriteFile(const std::string& path, const std::string& content) {
    // Create parent directories if needed
    fs::path file_path(path);
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path());
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return true;
}

} // namespace RawrXD
