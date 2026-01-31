/**
 * @file agentic_tools.hpp
 * @brief Complete tool definitions for agentic execution
 * 
 * Full Win32 API integration for native autonomous workload execution.
 */

#ifndef AGENTIC_TOOLS_HPP_INCLUDED
#define AGENTIC_TOOLS_HPP_INCLUDED

#include <functional>
#include <memory>
#include <windows.h>

/**
 * @struct ToolResult
 * @brief Result of tool execution
 */
struct ToolResult
{
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
class AgenticToolExecutor 
{

public:
    explicit AgenticToolExecutor( = nullptr);
    
    /**
     * @brief Execute a tool with given arguments
     * @param toolName The name of the tool to execute
     * @param arguments Input arguments for the tool
     * @return Result of tool execution
     */
    ToolResult executeTool(const std::string& toolName, const std::stringList& arguments);
    
    /**
     * @brief Register a custom tool
     * @param name Tool identifier
     * @param executor Function to execute
     */
    void registerTool(const std::string& name, 
                     std::function<ToolResult(const std::stringList&)> executor);

    // Built-in tools
    ToolResult readFile(const std::string& filePath);
    ToolResult writeFile(const std::string& filePath, const std::string& content);
    ToolResult listDirectory(const std::string& dirPath);
    ToolResult executeCommand(const std::string& command, const std::stringList& args);
    ToolResult grepSearch(const std::string& pattern, const std::string& path);
    ToolResult gitStatus(const std::string& repoPath);
    ToolResult runTests(const std::string& testPath);
    ToolResult analyzeCode(const std::string& filePath);
    ToolResult refactorCode(const std::string& filePath, const std::string& description);
    ToolResult createCode(const std::string& filePath, const std::string& description);
    ToolResult fixCode(const std::string& filePath, const std::string& description);
\npublic:\n    void toolExecuted(const std::string& name, const ToolResult& result);
    void toolFailed(const std::string& name, const std::string& error);
    void toolProgress(const std::string& name, const std::string& progress);
    void toolExecutionCompleted(const std::string& name, const std::string& result);
    void toolExecutionError(const std::string& name, const std::string& error);

private:
    std::map<std::string, std::function<ToolResult(const std::stringList&)>> m_tools;
    
    void initializeBuiltInTools();
    ToolResult executeProcess(const std::string& program, const std::stringList& args, int timeoutMs = 30000);
    std::string detectLanguage(const std::string& filePath);
};

#endif // AGENTIC_TOOLS_HPP_INCLUDED


