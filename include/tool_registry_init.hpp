/**
 * @file tool_registry_init.hpp
 * @brief Complete tool registration system for agentic IDE
 *
 * This module provides comprehensive tool registration including:
 * - File system operations (read, write, list, delete, search, grep)
 * - Version control operations (git status, diff, log, commit, push)
 * - Build and test operations (run tests, analyze code, compile)
 * - Execution operations (execute command, run process, shell)
 * - Model operations (list models, load model, run inference, unload model)
 * - Code analysis operations (lint, format, refactor suggestions)
 * - Deployment operations (docker build, kubernetes deploy)
 *
 * All tools follow production-ready patterns:
 * - Structured logging at DEBUG/INFO/WARN/ERROR levels
 * - Input validation and schema checking
 * - Error recovery and retry strategies
 * - Resource guards (files closed immediately, processes terminated)
 * - Execution metrics (latency, input/output sizes)
 * - Feature toggles for dangerous operations
 *
 * @author RawrXD Agent Team
 * @version 2.0.0
 * @date 2025-12-12
 */

#pragma once

#include "tool_registry.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// Main Initialization Function
// ============================================================================

/**
 * @brief Initialize all built-in tools in the registry
 * 
 * This function registers all available tools with proper configurations,
 * validation schemas, and execution handlers. Tools are grouped by category
 * for better organization and monitoring.
 * 
 * @param registry Pointer to the tool registry instance
 * @return true if all tools were registered successfully, false otherwise
 */
bool initializeAllTools(ToolRegistry* registry);

// ============================================================================
// Category-Specific Registration Functions
// ============================================================================

/**
 * @brief Register file system operation tools
 * 
 * Registers: readFile, writeFile, listDirectory, deleteFile, searchFiles,
 * grepSearch, copyFile, moveFile, createDirectory, removeDirectory
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerFileSystemTools(ToolRegistry* registry);

/**
 * @brief Register version control operation tools
 * 
 * Registers: gitStatus, gitDiff, gitLog, gitCommit, gitPush, gitPull,
 * gitBranch, gitCheckout, gitMerge, gitStash
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerVersionControlTools(ToolRegistry* registry);

/**
 * @brief Register build and test operation tools
 * 
 * Registers: runTests, analyzeCode, compileBuild, cleanBuild, runBenchmarks,
 * generateCoverage, lintCode, formatCode
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerBuildTestTools(ToolRegistry* registry);

/**
 * @brief Register execution operation tools
 * 
 * Registers: executeCommand, runProcess, runShellScript, killProcess,
 * processStatus, executeWithTimeout
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerExecutionTools(ToolRegistry* registry);

/**
 * @brief Register model operation tools
 * 
 * Registers: listModels, loadModel, unloadModel, runInference, getModelInfo,
 * testModelPerformance, warmupModel
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerModelTools(ToolRegistry* registry);

/**
 * @brief Register code analysis operation tools
 * 
 * Registers: detectCodeSmells, suggestRefactoring, findDuplicates,
 * analyzeComplexity, generateDocumentation, findUnusedCode
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerCodeAnalysisTools(ToolRegistry* registry);

/**
 * @brief Register deployment operation tools
 * 
 * Registers: dockerBuild, dockerPush, kubernetesApply, deployToProduction,
 * rollbackDeployment, healthCheck
 * 
 * @param registry Tool registry instance
 * @return Number of tools registered
 */
int registerDeploymentTools(ToolRegistry* registry);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Execute a process safely with timeout and resource management (C++20 / Win32)
 *
 * @param program Program to execute
 * @param arguments Arguments to pass
 * @param timeoutMs Timeout in milliseconds
 * @param workingDir Working directory (empty for current)
 * @return JSON result with {success, output, error, exitCode, executionTimeMs}
 */
json executeProcessSafely(
    const std::string& program,
    const std::vector<std::string>& arguments,
    int timeoutMs = 30000,
    const std::string& workingDir = {}
);

/**
 * @brief Read file contents with size limit and validation
 *
 * @param filePath Path to file
 * @param maxSizeBytes Maximum file size to read (default 10MB)
 * @return JSON result with {success, content, error, sizeBytes}
 */
json readFileSafely(
    const std::string& filePath,
    int64_t maxSizeBytes = 10 * 1024 * 1024
);

/**
 * @brief Write file contents with backup and atomic write
 *
 * @param filePath Path to file
 * @param content Content to write
 * @param createBackup Whether to create a backup of existing file
 * @return JSON result with {success, bytesWritten, error, backupPath}
 */
json writeFileSafely(
    const std::string& filePath,
    const std::string& content,
    bool createBackup = true
);

/**
 * @brief Validate that a path is within allowed workspace boundaries
 *
 * @param path Path to validate
 * @param workspaceRoot Root directory of workspace
 * @return true if path is valid and within workspace, false otherwise
 */
bool validatePathSafety(const std::string& path, const std::string& workspaceRoot = {});

/**
 * @brief Check if a command is potentially destructive
 *
 * @param program Program name
 * @param arguments Command arguments
 * @return true if command is destructive (rm, del, format, etc.)
 */
bool isDestructiveCommand(const std::string& program, const std::vector<std::string>& arguments);

/**
 * @brief Get Git repository root directory from a given path
 *
 * @param path Path within repository
 * @return Repository root path, or empty string if not in a git repo
 */
std::string getGitRepositoryRoot(const std::string& path);

/**
 * @brief Parse git status output into structured JSON
 *
 * @param gitOutput Raw output from git status --porcelain
 * @return JSON with {modified, added, deleted, renamed, untracked} arrays
 */
json parseGitStatus(const std::string& gitOutput);

/**
 * @brief Detect programming language from file extension
 *
 * @param filePath Path to file
 * @return Language identifier (cpp, python, javascript, etc.)
 */
std::string detectLanguage(const std::string& filePath);

/**
 * @brief Simple code complexity analyzer
 *
 * @param code Source code to analyze
 * @param language Programming language
 * @return JSON with {linesOfCode, cyclomaticComplexity, nestingDepth, functions}
 */
json analyzeCodeComplexity(const std::string& code, const std::string& language);

// End of file
