/**
 * @file agentic_tools.h
 * @brief Complete tool definitions for agentic execution (C++20, no Qt, no common/ deps)
 */
#ifndef AGENTIC_TOOLS_HPP_INCLUDED
#define AGENTIC_TOOLS_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <filesystem>

/** Simple callback list (replaces common/callback_system.hpp) — invoke all registered handlers */
template<typename... Args>
struct CallbackList {
    std::vector<std::function<void(Args...)>> handlers;
    void add(std::function<void(Args...)> f) { if (f) handlers.push_back(std::move(f)); }
    void invoke(Args... args) { for (auto& h : handlers) h(args...); }
};

/**
 * @struct ToolResult
 * @brief Result of tool execution
 */
struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
    int exitCode = 0;
    double executionTimeMs = 0.0;
};

/**
 * @class AgenticToolExecutor
 * @brief Complete tool executor with file, git, build, and analysis capabilities
 */
class AgenticToolExecutor {
public:
    AgenticToolExecutor();

    ToolResult executeTool(const std::string& toolName, const std::vector<std::string>& arguments);
    void registerTool(const std::string& name,
                      std::function<ToolResult(const std::vector<std::string>&)> executor);

    // Built-in tools
    ToolResult readFile(const std::string& filePath);
    ToolResult writeFile(const std::string& filePath, const std::string& content);
    ToolResult listDirectory(const std::string& dirPath);
    ToolResult executeCommand(const std::string& command, const std::vector<std::string>& args);
    ToolResult grepSearch(const std::string& pattern, const std::string& path);
    ToolResult gitStatus(const std::string& repoPath);
    ToolResult runTests(const std::string& testPath);
    ToolResult analyzeCode(const std::string& filePath);

    // Callbacks (replacing Qt signals)
    CallbackList<const std::string&, const ToolResult&> onToolExecuted;
    CallbackList<const std::string&, const std::string&> onToolFailed;
    CallbackList<const std::string&, const std::string&> onToolProgress;
    CallbackList<const std::string&, const std::string&> onToolExecutionCompleted;
    CallbackList<const std::string&, const std::string&> onToolExecutionError;

private:
    std::unordered_map<std::string, std::function<ToolResult(const std::vector<std::string>&)>> m_tools;

    void initializeBuiltInTools();
    ToolResult executeProcess(const std::string& program, const std::vector<std::string>& args,
                              int timeoutMs = 30000);
    std::string detectLanguage(const std::string& filePath);
};

#endif // AGENTIC_TOOLS_HPP_INCLUDED
