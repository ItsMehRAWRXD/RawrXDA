#include "agentic_executor.cpp"
#include "logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

// ============================================================================
// Tool Implementations
// ============================================================================

ToolResult readFileTool(const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing 'path' parameter";
        result.success = false;
        return result;
    }

    try {
        std::ifstream file(it->second, std::ios::binary);
        if (!file.is_open()) {
            result.error = "File not found: " + it->second;
            result.success = false;
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        result.output = buffer.str();
        result.success = true;
        log_info("File read: " + it->second + " (" + std::to_string(result.output.size()) + " bytes)");
    } catch (const std::exception& e) {
        result.error = e.what();
        result.success = false;
        log_error("Read file failed: " + std::string(e.what()));
    }

    return result;
}

ToolResult writeFileTool(const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    auto pathIt = params.find("path");
    auto contentIt = params.find("content");
    
    if (pathIt == params.end() || contentIt == params.end()) {
        result.error = "Missing 'path' or 'content' parameter";
        result.success = false;
        return result;
    }

    try {
        std::ofstream file(pathIt->second, std::ios::binary);
        if (!file.is_open()) {
            result.error = "Cannot write to: " + pathIt->second;
            result.success = false;
            return result;
        }

        file.write(contentIt->second.c_str(), contentIt->second.size());
        file.close();
        result.output = "Written " + std::to_string(contentIt->second.size()) + " bytes";
        result.success = true;
        log_info("File written: " + pathIt->second);
    } catch (const std::exception& e) {
        result.error = e.what();
        result.success = false;
        log_error("Write file failed: " + std::string(e.what()));
    }

    return result;
}

ToolResult listDirectoryTool(const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing 'path' parameter";
        result.success = false;
        return result;
    }

    try {
        std::stringstream ss;
        int count = 0;
        for (const auto& entry : fs::directory_iterator(it->second)) {
            ss << (entry.is_directory() ? "[DIR] " : "[FILE] ")
               << entry.path().filename().string() << "\n";
            count++;
        }
        result.output = ss.str();
        result.success = true;
        log_info("Directory listed: " + it->second + " (" + std::to_string(count) + " items)");
    } catch (const std::exception& e) {
        result.error = e.what();
        result.success = false;
        log_error("List directory failed: " + std::string(e.what()));
    }

    return result;
}

ToolResult analyzeCodeTool(const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing 'path' parameter";
        result.success = false;
        return result;
    }

    // Placeholder for code analysis
    result.output = "Analysis placeholder for: " + it->second;
    result.success = true;
    log_debug("Code analysis: " + it->second);

    return result;
}

ToolResult grepTool(const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    
    if (patternIt == params.end() || pathIt == params.end()) {
        result.error = "Missing 'pattern' or 'path' parameter";
        result.success = false;
        return result;
    }

    // Placeholder for grep implementation
    result.output = "Grep placeholder: looking for " + patternIt->second;
    result.success = true;
    log_debug("Grep search: " + patternIt->second + " in " + pathIt->second);

    return result;
}

// ============================================================================
// Tool Executor
// ============================================================================

ToolResult AgenticToolExecutor::executeTool(const std::string& name, 
                                            const std::map<std::string, std::string>& params) {
    ToolResult result;
    
    log_info("Executing tool: " + name);
    
    auto start = std::chrono::high_resolution_clock::now();

    if (name == "read_file") {
        result = readFileTool(params);
    } else if (name == "write_file") {
        result = writeFileTool(params);
    } else if (name == "list_directory") {
        result = listDirectoryTool(params);
    } else if (name == "analyze_code") {
        result = analyzeCodeTool(params);
    } else if (name == "grep") {
        result = grepTool(params);
    } else {
        result.error = "Unknown tool: " + name;
        result.success = false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    if (m_onToolExecuted) {
        m_onToolExecuted(name, result);
    }

    return result;
}

void AgenticToolExecutor::setOnToolExecuted(std::function<void(const std::string&, const ToolResult&)> cb) {
    m_onToolExecuted = cb;
}

void AgenticToolExecutor::setOnToolFailed(std::function<void(const std::string&, const std::string&)> cb) {
    m_onToolFailed = cb;
}
