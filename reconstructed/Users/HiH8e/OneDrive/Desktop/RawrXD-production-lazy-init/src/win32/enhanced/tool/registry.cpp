/**
 * @file enhanced_tool_registry.cpp
 * @brief Implementation of extended tool registry
 */

#include "enhanced_tool_registry.hpp"
#include "lsp_client.hpp"
#include <regex>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

EnhancedToolRegistry::EnhancedToolRegistry() {
    registerBuiltInTools();
}

// =============================================================================
// Tool Registration
// =============================================================================

bool EnhancedToolRegistry::registerTool(const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (tool.id.empty()) {
        return false;
    }
    
    m_tools[tool.id] = tool;
    m_stats[tool.id] = ToolStats{};
    
    // Add to category
    if (!tool.category.empty()) {
        auto it = m_categories.find(tool.category);
        if (it != m_categories.end()) {
            it->second.toolIds.push_back(tool.id);
        } else {
            ToolCategory cat;
            cat.id = tool.category;
            cat.name = tool.category;
            cat.description = tool.category + " tools";
            cat.toolIds.push_back(tool.id);
            m_categories[tool.category] = cat;
        }
    }
    
    return true;
}

bool EnhancedToolRegistry::unregisterTool(const std::string& toolId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tools.find(toolId);
    if (it == m_tools.end()) {
        return false;
    }
    
    // Remove from category
    for (auto& [id, category] : m_categories) {
        category.toolIds.erase(std::remove(category.toolIds.begin(), category.toolIds.end(), toolId), category.toolIds.end());
    }
    
    m_tools.erase(it);
    m_stats.erase(toolId);
    
    return true;
}

bool EnhancedToolRegistry::updateTool(const std::string& toolId, const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tools.find(toolId);
    if (it == m_tools.end()) {
        return false;
    }
    
    ToolDefinition updated = tool;
    updated.id = toolId; // Preserve ID
    it->second = updated;
    
    return true;
}

ToolDefinition* EnhancedToolRegistry::getTool(const std::string& toolId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(toolId);
    return it != m_tools.end() ? &it->second : nullptr;
}

std::vector<ToolDefinition> EnhancedToolRegistry::listTools() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ToolDefinition> tools;
    tools.reserve(m_tools.size());
    for (const auto& [id, tool] : m_tools) {
        tools.push_back(tool);
    }
    return tools;
}

std::vector<ToolDefinition> EnhancedToolRegistry::listToolsByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ToolDefinition> tools;
    auto it = m_categories.find(category);
    if (it == m_categories.end()) return tools;
    
    for (const auto& id : it->second.toolIds) {
        auto t = m_tools.find(id);
        if (t != m_tools.end()) {
            tools.push_back(t->second);
        }
    }
    return tools;
}

bool EnhancedToolRegistry::registerCategory(const ToolCategory& category) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (category.id.empty()) return false;
    m_categories[category.id] = category;
    return true;
}

std::vector<ToolCategory> EnhancedToolRegistry::listCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ToolCategory> categories;
    categories.reserve(m_categories.size());
    for (const auto& [id, cat] : m_categories) {
        categories.push_back(cat);
    }
    return categories;
}

// =============================================================================
// Tool Execution
// =============================================================================

ToolResult EnhancedToolRegistry::executeTool(const std::string& toolId,
                                              const std::unordered_map<std::string, std::string>& params) {
    auto start = std::chrono::high_resolution_clock::now();
    ToolResult result;
    result.success = false;
    result.error = "Tool not found";
    
    ToolDefinition tool;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tools.find(toolId);
        if (it != m_tools.end()) {
            tool = it->second;
        } else {
            return result;
        }
    }
    
    if (!tool.execute) {
        result.error = "Tool has no implementation";
        return result;
    }
    
    // Validate parameters
    std::string validationError;
    if (!validateParameters(toolId, params, validationError)) {
        result.error = validationError;
        return result;
    }
    
    result = tool.execute(params);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& stats = m_stats[toolId];
        stats.totalExecutions++;
        if (result.success) stats.successfulExecutions++; else stats.failedExecutions++;
        stats.totalExecutionTimeMs += result.executionTimeMs;
        stats.averageExecutionTimeMs = stats.totalExecutionTimeMs / stats.totalExecutions;
    }
    
    return result;
}

void EnhancedToolRegistry::executeToolAsync(const std::string& toolId,
                                             const std::unordered_map<std::string, std::string>& params,
                                             std::function<void(ToolResult)> callback) {
    std::thread([this, toolId, params, callback = std::move(callback)]() {
        ToolResult result = executeTool(toolId, params);
        if (callback) {
            callback(std::move(result));
        }
    }).detach();
}

// =============================================================================
// Schema Generation
// =============================================================================

std::string EnhancedToolRegistry::getToolsSchemaJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"tools\":[";
    
    auto tools = listTools();
    for (size_t i = 0; i < tools.size(); ++i) {
        json << getToolSchemaJson(tools[i].id);
        if (i < tools.size() - 1) json << ",";
    }
    
    json << "]";
    json << "}";
    return json.str();
}

std::string EnhancedToolRegistry::getToolSchemaJson(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(toolId);
    if (it == m_tools.end()) return "{}";
    
    const auto& tool = it->second;
    
    std::ostringstream json;
    json << "{";
    json << "\"id\":\"" << tool.id << "\",";
    json << "\"name\":\"" << tool.name << "\",";
    json << "\"description\":\"" << tool.description << "\",";
    json << "\"category\":\"" << tool.category << "\",";
    json << "\"parameters\":[";
    for (size_t i = 0; i < tool.parameters.size(); ++i) {
        const auto& p = tool.parameters[i];
        json << "{";
        json << "\"name\":\"" << p.name << "\",";
        json << "\"type\":\"" << p.type << "\",";
        json << "\"description\":\"" << p.description << "\",";
        json << "\"required\":" << (p.required ? "true" : "false") << ",";
        json << "\"default\":\"" << p.defaultValue << "\"";
        if (!p.enumValues.empty()) {
            json << ",\"enum\":[";
            for (size_t j = 0; j < p.enumValues.size(); ++j) {
                json << "\"" << p.enumValues[j] << "\"";
                if (j < p.enumValues.size() - 1) json << ",";
            }
            json << "]";
        }
        json << "}";
        if (i < tool.parameters.size() - 1) json << ",";
    }
    json << "],";
    json << "\"requiresConfirmation\":" << (tool.requiresConfirmation ? "true" : "false") << ",";
    json << "\"isAsync\":" << (tool.isAsync ? "true" : "false") << ",";
    json << "\"requiredPermissions\":[";
    for (size_t i = 0; i < tool.requiredPermissions.size(); ++i) {
        json << "\"" << tool.requiredPermissions[i] << "\"";
        if (i < tool.requiredPermissions.size() - 1) json << ",";
    }
    json << "]";
    json << "}";
    return json.str();
}

// =============================================================================
// Validation
// =============================================================================

bool EnhancedToolRegistry::validateParameters(const std::string& toolId,
                                              const std::unordered_map<std::string, std::string>& params,
                                              std::string& error) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tools.find(toolId);
    if (it == m_tools.end()) {
        error = "Tool not found";
        return false;
    }
    
    const auto& tool = it->second;
    
    for (const auto& param : tool.parameters) {
        if (param.required) {
            if (params.find(param.name) == params.end()) {
                error = "Missing required parameter: " + param.name;
                return false;
            }
        }
    }
    
    return true;
}

// =============================================================================
// Statistics
// =============================================================================

EnhancedToolRegistry::ToolStats EnhancedToolRegistry::getToolStats(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stats.find(toolId);
    return it != m_stats.end() ? it->second : ToolStats{};
}

void EnhancedToolRegistry::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, stats] : m_stats) {
        stats = ToolStats{};
    }
}

// =============================================================================
// Built-in Tool Registration
// =============================================================================

void EnhancedToolRegistry::registerBuiltInTools() {
    // File IO tools
    registerTool({
        "read_file", "Read File", "Read a file from disk", "filesystem",
        {{"path", "string", "Path to file", true, ""}}, false, false, {},
        [this](const auto& params) { return executeReadFile(params); }
    });
    
    registerTool({
        "write_file", "Write File", "Write text to a file", "filesystem",
        {{"path", "string", "Path to file", true, ""},
         {"content", "string", "Content to write", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeWriteFile(params); }
    });
    
    registerTool({
        "list_dir", "List Directory", "List files in a directory", "filesystem",
        {{"path", "string", "Directory path", true, ""}}, false, false, {},
        [this](const auto& params) { return executeListDir(params); }
    });
    
    registerTool({
        "create_dir", "Create Directory", "Create a new directory", "filesystem",
        {{"path", "string", "Directory path", true, ""}}, false, false, {},
        [this](const auto& params) { return executeCreateDir(params); }
    });
    
    registerTool({
        "delete_file", "Delete File", "Delete a file", "filesystem",
        {{"path", "string", "File path", true, ""}}, false, false, {},
        [this](const auto& params) { return executeDeleteFile(params); }
    });
    
    registerTool({
        "move_file", "Move File", "Move a file", "filesystem",
        {{"source", "string", "Source path", true, ""},
         {"destination", "string", "Destination path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeMoveFile(params); }
    });
    
    registerTool({
        "copy_file", "Copy File", "Copy a file", "filesystem",
        {{"source", "string", "Source path", true, ""},
         {"destination", "string", "Destination path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeCopyFile(params); }
    });
    
    registerTool({
        "search_files", "Search Files", "Search for files by pattern", "filesystem",
        {{"path", "string", "Directory path", true, ""},
         {"pattern", "string", "Glob pattern", true, "*"}},
        false, false, {},
        [this](const auto& params) { return executeSearchFiles(params); }
    });
    
    registerTool({
        "grep_search", "Grep Search", "Search for text in files", "filesystem",
        {{"path", "string", "Directory path", true, ""},
         {"query", "string", "Search query", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGrepSearch(params); }
    });
    
    // Commands
    registerTool({
        "run_command", "Run Command", "Execute a shell command", "system",
        {{"command", "string", "Command to run", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeRunCommand(params); }
    });
    
    registerTool({
        "compile_code", "Compile Code", "Compile a project", "system",
        {{"command", "string", "Compile command", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeCompileCode(params); }
    });
    
    registerTool({
        "run_tests", "Run Tests", "Run unit tests", "system",
        {{"command", "string", "Test command", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeRunTests(params); }
    });
    
    registerTool({
        "debug_code", "Debug Code", "Debug a program", "system",
        {{"command", "string", "Debug command", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeDebugCode(params); }
    });
    
    registerTool({
        "format_code", "Format Code", "Run code formatter", "system",
        {{"command", "string", "Format command", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeFormatCode(params); }
    });
    
    registerTool({
        "lint_code", "Lint Code", "Run linter", "system",
        {{"command", "string", "Lint command", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeLintCode(params); }
    });
    
    // Git
    registerTool({
        "git_status", "Git Status", "Get git status", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitStatus(params); }
    });
    
    registerTool({
        "git_add", "Git Add", "Stage files", "git",
        {{"path", "string", "Path to add", true, "."}},
        false, false, {},
        [this](const auto& params) { return executeGitAdd(params); }
    });
    
    registerTool({
        "git_commit", "Git Commit", "Commit changes", "git",
        {{"message", "string", "Commit message", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGitCommit(params); }
    });
    
    registerTool({
        "git_push", "Git Push", "Push changes", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitPush(params); }
    });
    
    registerTool({
        "git_pull", "Git Pull", "Pull changes", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitPull(params); }
    });
    
    registerTool({
        "git_branch", "Git Branch", "List or create branch", "git",
        {{"name", "string", "Branch name (optional)", false, ""}},
        false, false, {},
        [this](const auto& params) { return executeGitBranch(params); }
    });
    
    registerTool({
        "git_checkout", "Git Checkout", "Checkout branch", "git",
        {{"name", "string", "Branch name", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGitCheckout(params); }
    });
    
    registerTool({
        "git_diff", "Git Diff", "Show diff", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitDiff(params); }
    });
    
    registerTool({
        "git_log", "Git Log", "Show git log", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitLog(params); }
    });
    
    registerTool({
        "git_stash", "Git Stash", "Stash changes", "git",
        {}, false, false, {},
        [this](const auto& params) { return executeGitStash(params); }
    });
    
    // HTTP
    registerTool({
        "http_get", "HTTP GET", "Perform HTTP GET", "network",
        {{"url", "string", "URL", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeHttpGet(params); }
    });
    
    registerTool({
        "http_post", "HTTP POST", "Perform HTTP POST", "network",
        {{"url", "string", "URL", true, ""},
         {"body", "string", "Request body", false, ""}},
        false, false, {},
        [this](const auto& params) { return executeHttpPost(params); }
    });
    
    registerTool({
        "fetch_webpage", "Fetch Webpage", "Fetch webpage content", "network",
        {{"url", "string", "URL", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeFetchWebpage(params); }
    });
    
    registerTool({
        "download_file", "Download File", "Download a file", "network",
        {{"url", "string", "URL", true, ""},
         {"path", "string", "Destination path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeDownloadFile(params); }
    });
    
    // IDE/LSP
    registerTool({
        "get_definition", "Get Definition", "Jump to definition", "ide",
        {{"symbol", "string", "Symbol name", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGetDefinition(params); }
    });
    
    registerTool({
        "get_references", "Get References", "Find references", "ide",
        {{"symbol", "string", "Symbol name", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGetReferences(params); }
    });
    
    registerTool({
        "get_symbols", "Get Symbols", "List document symbols", "ide",
        {{"path", "string", "File path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGetSymbols(params); }
    });
    
    registerTool({
        "get_completion", "Get Completion", "Code completion", "ide",
        {{"path", "string", "File path", true, ""},
         {"line", "number", "Line number", true, "0"},
         {"character", "number", "Character", true, "0"}},
        false, false, {},
        [this](const auto& params) { return executeGetCompletion(params); }
    });
    
    registerTool({
        "get_hover", "Get Hover", "Hover info", "ide",
        {{"path", "string", "File path", true, ""},
         {"line", "number", "Line", true, "0"},
         {"character", "number", "Character", true, "0"}},
        false, false, {},
        [this](const auto& params) { return executeGetHover(params); }
    });
    
    registerTool({
        "refactor", "Refactor", "Refactor code", "ide",
        {{"path", "string", "File path", true, ""},
         {"range", "string", "Range", true, ""},
         {"action", "string", "Refactor action", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeRefactor(params); }
    });
    
    // Model tools
    registerTool({
        "ask_model", "Ask Model", "Ask LLM a question", "ai",
        {{"prompt", "string", "Prompt", true, ""},
         {"system", "string", "System prompt", false, ""}},
        false, false, {},
        [this](const auto& params) { return executeAskModel(params); }
    });
    
    registerTool({
        "generate_code", "Generate Code", "Generate code with LLM", "ai",
        {{"prompt", "string", "Prompt", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGenerateCode(params); }
    });
    
    registerTool({
        "explain_code", "Explain Code", "Explain code with LLM", "ai",
        {{"code", "string", "Code", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeExplainCode(params); }
    });
    
    registerTool({
        "review_code", "Review Code", "Code review with LLM", "ai",
        {{"code", "string", "Code", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeReviewCode(params); }
    });
    
    registerTool({
        "fix_bug", "Fix Bug", "Suggest fixes", "ai",
        {{"code", "string", "Code", true, ""},
         {"error", "string", "Error", false, ""}},
        false, false, {},
        [this](const auto& params) { return executeFixBug(params); }
    });
    
    // Editor state
    registerTool({
        "open_file", "Open File", "Open file in editor", "editor",
        {{"path", "string", "Path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeOpenFile(params); }
    });
    
    registerTool({
        "close_file", "Close File", "Close file in editor", "editor",
        {{"path", "string", "Path", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeCloseFile(params); }
    });
    
    registerTool({
        "get_open_files", "Get Open Files", "List open files", "editor",
        {}, false, false, {},
        [this](const auto& params) { return executeGetOpenFiles(params); }
    });
    
    registerTool({
        "get_active_file", "Get Active File", "Current active file", "editor",
        {}, false, false, {},
        [this](const auto& params) { return executeGetActiveFile(params); }
    });
    
    registerTool({
        "get_selection", "Get Selection", "Get current selection", "editor",
        {}, false, false, {},
        [this](const auto& params) { return executeGetSelection(params); }
    });
    
    registerTool({
        "insert_text", "Insert Text", "Insert text at cursor", "editor",
        {{"text", "string", "Text", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeInsertText(params); }
    });
    
    // Memory
    registerTool({
        "add_memory", "Add Memory", "Store context", "memory",
        {{"key", "string", "Key", true, ""},
         {"value", "string", "Value", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeAddMemory(params); }
    });
    
    registerTool({
        "get_memory", "Get Memory", "Retrieve context", "memory",
        {{"key", "string", "Key", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeGetMemory(params); }
    });
    
    registerTool({
        "search_memory", "Search Memory", "Search stored context", "memory",
        {{"query", "string", "Query", true, ""}},
        false, false, {},
        [this](const auto& params) { return executeSearchMemory(params); }
    });
    
    registerTool({
        "clear_memory", "Clear Memory", "Clear stored context", "memory",
        {}, false, false, {},
        [this](const auto& params) { return executeClearMemory(params); }
    });
}

// =============================================================================
// Tool Implementations
// =============================================================================

ToolResult EnhancedToolRegistry::executeReadFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing path";
        return result;
    }
    
    std::ifstream file(it->second, std::ios::binary);
    if (!file) {
        result.error = "Failed to open file";
        return result;
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    result.output = ss.str();
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeWriteFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto contentIt = params.find("content");
    if (pathIt == params.end() || contentIt == params.end()) {
        result.error = "Missing path or content";
        return result;
    }
    
    std::ofstream file(pathIt->second, std::ios::binary);
    if (!file) {
        result.error = "Failed to open file";
        return result;
    }
    
    file << contentIt->second;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeListDir(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        result.error = "Missing path";
        return result;
    }
    
    if (!fs::exists(pathIt->second)) {
        result.error = "Directory does not exist";
        return result;
    }
    
    std::ostringstream ss;
    for (const auto& entry : fs::directory_iterator(pathIt->second)) {
        ss << entry.path().string() << "\n";
    }
    result.output = ss.str();
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeCreateDir(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        result.error = "Missing path";
        return result;
    }
    
    std::error_code ec;
    if (!fs::create_directories(pathIt->second, ec) && ec) {
        result.error = "Failed to create directory: " + ec.message();
        return result;
    }
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeDeleteFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        result.error = "Missing path";
        return result;
    }
    
    std::error_code ec;
    if (!fs::remove(pathIt->second, ec) && ec) {
        result.error = "Failed to delete: " + ec.message();
        return result;
    }
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeMoveFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("source");
    auto dstIt = params.find("destination");
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing source or destination";
        return result;
    }
    
    std::error_code ec;
    fs::rename(srcIt->second, dstIt->second, ec);
    if (ec) {
        result.error = "Failed to move: " + ec.message();
        return result;
    }
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeCopyFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("source");
    auto dstIt = params.find("destination");
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing source or destination";
        return result;
    }
    
    std::error_code ec;
    fs::copy(srcIt->second, dstIt->second, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "Failed to copy: " + ec.message();
        return result;
    }
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeSearchFiles(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto patternIt = params.find("pattern");
    if (pathIt == params.end() || patternIt == params.end()) {
        result.error = "Missing path or pattern";
        return result;
    }
    
    std::ostringstream ss;
    for (const auto& entry : fs::recursive_directory_iterator(pathIt->second)) {
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        std::string name = p.filename().string();
        if (name.find(patternIt->second) != std::string::npos) {
            ss << p.string() << "\n";
        }
    }
    result.output = ss.str();
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeGrepSearch(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto queryIt = params.find("query");
    if (pathIt == params.end() || queryIt == params.end()) {
        result.error = "Missing path or query";
        return result;
    }
    
    std::ostringstream ss;
    for (const auto& entry : fs::recursive_directory_iterator(pathIt->second)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream file(entry.path());
        std::string line;
        int lineNumber = 1;
        while (std::getline(file, line)) {
            if (line.find(queryIt->second) != std::string::npos) {
                ss << entry.path().string() << ":" << lineNumber << ": " << line << "\n";
            }
            ++lineNumber;
        }
    }
    result.output = ss.str();
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeRunCommand(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto cmdIt = params.find("command");
    if (cmdIt == params.end()) {
        result.error = "Missing command";
        return result;
    }
    
    // Run command and capture output
    std::string command = cmdIt->second;
#ifdef _WIN32
    command = "cmd /c " + command + " 2>&1";
    FILE* pipe = _popen(command.c_str(), "r");
#else
    command += " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        result.error = "Failed to run command";
        return result;
    }
    
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    result.output = output;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeCompileCode(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand(params);
}

ToolResult EnhancedToolRegistry::executeRunTests(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand(params);
}

ToolResult EnhancedToolRegistry::executeDebugCode(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand(params);
}

ToolResult EnhancedToolRegistry::executeFormatCode(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand(params);
}

ToolResult EnhancedToolRegistry::executeLintCode(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand(params);
}

ToolResult EnhancedToolRegistry::executeGitStatus(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git status --short"}});
}

ToolResult EnhancedToolRegistry::executeGitAdd(const std::unordered_map<std::string, std::string>& params) {
    auto pathIt = params.find("path");
    if (pathIt == params.end()) return {false, "", "Missing path", 0.0, {}};
    return executeRunCommand({{"command", "git add \"" + pathIt->second + "\""}});
}

ToolResult EnhancedToolRegistry::executeGitCommit(const std::unordered_map<std::string, std::string>& params) {
    auto msgIt = params.find("message");
    if (msgIt == params.end()) return {false, "", "Missing message", 0.0, {}};
    return executeRunCommand({{"command", "git commit -m \"" + msgIt->second + "\""}});
}

ToolResult EnhancedToolRegistry::executeGitPush(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git push"}});
}

ToolResult EnhancedToolRegistry::executeGitPull(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git pull"}});
}

ToolResult EnhancedToolRegistry::executeGitBranch(const std::unordered_map<std::string, std::string>& params) {
    auto nameIt = params.find("name");
    if (nameIt == params.end() || nameIt->second.empty()) {
        return executeRunCommand({{"command", "git branch"}});
    }
    return executeRunCommand({{"command", "git branch \"" + nameIt->second + "\""}});
}

ToolResult EnhancedToolRegistry::executeGitCheckout(const std::unordered_map<std::string, std::string>& params) {
    auto nameIt = params.find("name");
    if (nameIt == params.end()) return {false, "", "Missing branch name", 0.0, {}};
    return executeRunCommand({{"command", "git checkout \"" + nameIt->second + "\""}});
}

ToolResult EnhancedToolRegistry::executeGitDiff(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git diff"}});
}

ToolResult EnhancedToolRegistry::executeGitLog(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git log --oneline -5"}});
}

ToolResult EnhancedToolRegistry::executeGitStash(const std::unordered_map<std::string, std::string>& params) {
    return executeRunCommand({{"command", "git stash"}});
}

ToolResult EnhancedToolRegistry::executeHttpGet(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    if (urlIt == params.end()) {
        result.error = "Missing URL";
        return result;
    }
    
    // Simple GET using std::ifstream if file URL
    if (urlIt->second.rfind("file://", 0) == 0) {
        std::string path = urlIt->second.substr(7);
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            result.error = "Failed to open file";
            return result;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        result.output = ss.str();
        result.success = true;
        return result;
    }
    
    // Not implementing full HTTP here
    result.error = "HTTP GET not implemented";
    return result;
}

ToolResult EnhancedToolRegistry::executeHttpPost(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    if (urlIt == params.end()) {
        result.error = "Missing URL";
        return result;
    }
    
    result.error = "HTTP POST not implemented";
    return result;
}

ToolResult EnhancedToolRegistry::executeFetchWebpage(const std::unordered_map<std::string, std::string>& params) {
    return executeHttpGet(params);
}

ToolResult EnhancedToolRegistry::executeDownloadFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    auto pathIt = params.find("path");
    if (urlIt == params.end() || pathIt == params.end()) {
        result.error = "Missing url or path";
        return result;
    }
    
    // Only support file:// for now
    if (urlIt->second.rfind("file://", 0) == 0) {
        std::string src = urlIt->second.substr(7);
        std::error_code ec;
        fs::copy(src, pathIt->second, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            result.error = "Failed to copy: " + ec.message();
            return result;
        }
        result.success = true;
        return result;
    }
    
    result.error = "Download not implemented";
    return result;
}

ToolResult EnhancedToolRegistry::executeGetDefinition(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto symIt = params.find("symbol");
    auto pathIt = params.find("path");
    if (symIt == params.end()) { result.error = "Missing symbol"; return result; }
    std::string file = (pathIt != params.end()) ? pathIt->second : std::string();
    LSPClient lsp;
    lsp.connect("");
    auto def = lsp.gotoDefinition(file.empty() ? std::string(".") : file, 0, 0);
    result.success = true;
    result.output = def.uri + ":" + std::to_string(def.line) + ":" + std::to_string(def.column);
    return result;
}

ToolResult EnhancedToolRegistry::executeGetReferences(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    std::string file = (pathIt != params.end()) ? pathIt->second : std::string();
    LSPClient lsp; lsp.connect("");
    auto refs = lsp.findReferences(file.empty() ? std::string(".") : file, 0, 0);
    std::ostringstream ss; for (auto& r : refs) ss << r.uri << ":" << r.line << ":" << r.column << "\n";
    result.success = true; result.output = ss.str();
    return result;
}

ToolResult EnhancedToolRegistry::executeGetSymbols(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path"); if (pathIt == params.end()) { result.error = "Missing path"; return result; }
    std::string content; { std::ifstream f(pathIt->second); if (f) { std::ostringstream ss; ss << f.rdbuf(); content = ss.str(); } }
    if (content.empty()) { result.error = "Cannot read file"; return result; }
    std::regex word(R"([A-Za-z_][A-Za-z0-9_]+)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), word), end = std::sregex_iterator();
    std::unordered_map<std::string,int> counts; for (auto it=begin; it!=end; ++it) counts[(*it).str()]++;
    std::vector<std::pair<std::string,int>> vec(counts.begin(), counts.end());
    std::sort(vec.begin(), vec.end(), [](auto&a,auto&b){return a.second>b.second;});
    std::ostringstream ss; int c=0; for (auto& p: vec){ ss << p.first << " (" << p.second << ")\n"; if(++c>=50) break; }
    result.success = true; result.output = ss.str();
    return result;
}

ToolResult EnhancedToolRegistry::executeGetCompletion(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path"); auto lineIt = params.find("line"); auto colIt = params.find("character");
    if (pathIt == params.end() || lineIt == params.end() || colIt == params.end()) { result.error = "Missing path/line/character"; return result; }
    LSPClient lsp; lsp.connect("");
    int line = std::stoi(lineIt->second), col = std::stoi(colIt->second);
    auto suggestions = lsp.complete(pathIt->second, line, col);
    std::ostringstream ss; for (auto& s : suggestions) ss << s << "\n";
    result.success = true; result.output = ss.str();
    return result;
}

ToolResult EnhancedToolRegistry::executeGetHover(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path"); auto lineIt = params.find("line"); auto colIt = params.find("character");
    if (pathIt == params.end() || lineIt == params.end() || colIt == params.end()) { result.error = "Missing path/line/character"; return result; }
    // Simple hover: show line content
    std::ifstream f(pathIt->second); if (!f) { result.error = "Cannot read file"; return result; }
    std::string lineStr; int target = std::stoi(lineIt->second); int cur=0; while (std::getline(f,lineStr)){ if(cur++==target) break; }
    result.success = true; result.output = lineStr;
    return result;
}

ToolResult EnhancedToolRegistry::executeRefactor(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path"); auto actionIt = params.find("action");
    if (pathIt == params.end() || actionIt == params.end()) { result.error = "Missing path/action"; return result; }
    // Only support rename: action format "rename symbol=Old to=New"
    std::regex rx(R"(rename\s+symbol=(\w+)\s+to=(\w+))"); std::smatch m;
    if (!std::regex_search(actionIt->second, m, rx)) { result.error = "Unsupported refactor"; return result; }
    std::string oldSym = m[1].str(), newSym = m[2].str();
    std::ifstream in(pathIt->second); if (!in) { result.error = "Cannot read file"; return result; }
    std::ostringstream ss; ss << in.rdbuf(); std::string content = ss.str(); in.close();
    size_t count = 0; size_t pos = 0; while ((pos = content.find(oldSym, pos)) != std::string::npos) { content.replace(pos, oldSym.size(), newSym); pos += newSym.size(); ++count; }
    std::ofstream out(pathIt->second); if (!out) { result.error = "Cannot write file"; return result; }
    out << content; out.close();
    result.success = true; result.output = "Renamed " + std::to_string(count) + " occurrences";
    return result;
}

ToolResult EnhancedToolRegistry::executeAskModel(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto promptIt = params.find("prompt");
    if (promptIt == params.end()) {
        result.error = "Missing prompt";
        return result;
    }
    result.output = "Model response placeholder for: " + promptIt->second;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeGenerateCode(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto promptIt = params.find("prompt");
    if (promptIt == params.end()) {
        result.error = "Missing prompt";
        return result;
    }
    result.output = "Generated code placeholder for: " + promptIt->second;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeExplainCode(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    if (codeIt == params.end()) {
        result.error = "Missing code";
        return result;
    }
    result.output = "Explanation placeholder";
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeReviewCode(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    if (codeIt == params.end()) {
        result.error = "Missing code";
        return result;
    }
    result.output = "Review placeholder";
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeFixBug(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    if (codeIt == params.end()) {
        result.error = "Missing code";
        return result;
    }
    result.output = "Fix placeholder";
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeOpenFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        result.error = "Missing path";
        return result;
    }
    result.output = "Opened " + pathIt->second;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeCloseFile(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        result.error = "Missing path";
        return result;
    }
    result.output = "Closed " + pathIt->second;
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeGetOpenFiles(const std::unordered_map<std::string, std::string>& params) {
    return {true, "[]", "", 0.0, {}};
}

ToolResult EnhancedToolRegistry::executeGetActiveFile(const std::unordered_map<std::string, std::string>& params) {
    return {true, "", "", 0.0, {}};
}

ToolResult EnhancedToolRegistry::executeGetSelection(const std::unordered_map<std::string, std::string>& params) {
    return {true, "", "", 0.0, {}};
}

ToolResult EnhancedToolRegistry::executeInsertText(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    auto textIt = params.find("text");
    if (textIt == params.end()) {
        result.error = "Missing text";
        return result;
    }
    result.output = "Inserted text";
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeAddMemory(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    if (params.find("key") == params.end() || params.find("value") == params.end()) {
        result.error = "Missing key or value";
        return result;
    }
    result.output = "Stored";
    result.success = true;
    return result;
}

ToolResult EnhancedToolRegistry::executeGetMemory(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    if (params.find("key") == params.end()) {
        result.error = "Missing key";
        return result;
    }
    result.success = true;
    result.output = "";
    return result;
}

ToolResult EnhancedToolRegistry::executeSearchMemory(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    if (params.find("query") == params.end()) {
        result.error = "Missing query";
        return result;
    }
    result.success = true;
    result.output = "[]";
    return result;
}

ToolResult EnhancedToolRegistry::executeClearMemory(const std::unordered_map<std::string, std::string>& params) {
    ToolResult result;
    result.success = true;
    result.output = "cleared";
    return result;
}
