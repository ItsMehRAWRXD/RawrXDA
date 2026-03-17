/**
 * @file tool_registry.hpp
 * @brief Complete tool registry system with full utility, production-ready observability and error handling
 *
 * This module provides:
 * - Centralized tool registration and execution
 * - Comprehensive structured logging with DEBUG/INFO/WARN/ERROR levels
 * - Metrics collection (counters, histograms, gauges)
 * - Distributed tracing hooks for observability
 * - Resource guards and cleanup
 * - Input validation and safety checks
 * - Error recovery strategies
 * - Tool-specific configuration and feature toggles
 * - Timeout and resource limits enforcement
 * - Execution statistics and profiling
 *
 * @author RawrXD Agent Team
 * @version 2.0.0
 * @date 2025-12-12
 */

#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "logging/logger.h"
#include "metrics/metrics.h"

using json = nlohmann::json;

// ============================================================================
// Enumerations and Constants
// ============================================================================

/**
 * @enum ToolExecutionStatus
 * @brief Status of tool execution
 */
enum class ToolExecutionStatus {
    NotStarted,         ///< Tool execution not yet started
    Running,            ///< Tool is currently executing
    Completed,          ///< Tool execution completed successfully
    Failed,             ///< Tool execution failed
    TimedOut,           ///< Tool execution exceeded timeout
    Cancelled,          ///< Tool execution was cancelled
    SkippedByToggle,    ///< Tool skipped due to feature toggle
};

/**
 * @enum ToolCategory
 * @brief Categorization of tools for monitoring and filtering
 */
enum class ToolCategory {
    FileSystem,         ///< File operations
    Build,              ///< Build system operations
    Testing,            ///< Test execution
    VersionControl,     ///< Git/VCS operations
    Execution,          ///< Command execution
    Analysis,           ///< Code analysis
    Deployment,         ///< Deployment operations
    Custom,             ///< Custom/user-defined
};

// ============================================================================
// Configuration and Validation Structures
// ============================================================================

/**
 * @struct ToolExecutionConfig
 * @brief Configuration for individual tool execution
 */
struct ToolExecutionConfig {
    std::string toolName;
    
    // Timeout and resource limits
    int32_t timeoutMs = 30000;              ///< Execution timeout in milliseconds
    int32_t maxMemoryMb = 512;              ///< Maximum memory usage
    int32_t maxCpuPercent = 80;             ///< Maximum CPU usage percentage
    
    // Feature flags
    bool enableExecution = true;            ///< Enable/disable tool execution
    bool enableCaching = false;             ///< Cache tool results
    int32_t cacheValidityMs = 60000;        ///< Cache validity duration
    
    // Error handling
    bool retryOnFailure = true;
    int32_t maxRetries = 3;
    int32_t retryDelayMs = 1000;
    
    // Validation
    bool validateInputs = true;
    bool validateOutputs = true;
    std::vector<std::string> allowedInputTypes;
    std::vector<std::string> blockedPaths;  ///< Paths tool cannot access
    
    // Observability
    bool enableDetailedLogging = false;
    bool enableMetrics = true;
    bool enableTracing = true;
    std::string traceSpanName;
    
    // Security
    bool requireApproval = false;           ///< Require user approval before execution
    bool sandboxExecution = false;          ///< Run in sandbox environment
};

/**
 * @struct ToolInputValidation
 * @brief Input validation rules for tools
 */
struct ToolInputValidation {
    std::vector<std::string> requiredFields;
    std::map<std::string, std::string> fieldTypes;  // field -> type (string, number, array, etc.)
    std::map<std::string, std::string> fieldDescriptions;
    std::function<bool(const json&)> customValidator;
};

/**
 * @struct ToolExecutionMetrics
 * @brief Metrics collected during tool execution
 */
struct ToolExecutionMetrics {
    int64_t startTimeMs = 0;
    int64_t endTimeMs = 0;
    int64_t executionTimeMs = 0;
    
    size_t inputBytes = 0;
    size_t outputBytes = 0;
    uint32_t memoryUsedMb = 0;
    
    uint32_t retryCount = 0;
    bool fromCache = false;
    
    // Counter increments
    uint32_t filesAccessed = 0;
    uint32_t subprocessesSpawned = 0;
    uint32_t externalApiCalls = 0;
};

/**
 * @struct ToolExecutionContext
 * @brief Complete execution context for a tool
 */
struct ToolExecutionContext {
    std::string executionId;                ///< Unique execution ID (UUID or similar)
    std::string traceId;                    ///< Distributed trace ID
    std::string spanId;                     ///< Current span ID
    std::string parentSpanId;               ///< Parent span ID if nested
    
    ToolExecutionStatus status = ToolExecutionStatus::NotStarted;
    ToolExecutionMetrics metrics;
    
    int32_t exitCode = 0;
    std::string errorMessage;
    std::string lastWarning;
};

/**
 * @struct ToolResult
 * @brief Complete result from tool execution with metadata
 */
struct ToolResult {
    bool success = false;
    json data;                              ///< Tool output data
    std::string error;                      ///< Error message if failed
    std::string warning;                    ///< Warning message if any
    
    ToolExecutionContext executionContext;
    ToolExecutionMetrics metrics;
    
    // Additional metadata
    int64_t createdAtMs = 0;
    bool wasRetried = false;
    int32_t retryCount = 0;
};

// ============================================================================
// Tool Definition
// ============================================================================

/**
 * @struct ToolDefinition
 * @brief Complete definition of a tool
 */
struct ToolDefinition {
    std::string name;
    std::string description;
    std::string version = "1.0.0";
    ToolCategory category = ToolCategory::Custom;
    
    // Tool handler function
    std::function<json(const json&)> handler;

    // Input Schema (JSON Schema)
    json inputSchema;
    
    // Metadata
    std::vector<std::string> tags;
    bool experimental = false;
    
    // Validation
    ToolInputValidation inputValidation;
    json outputSchema;
    
    // Configuration
    ToolExecutionConfig config;
};

// ============================================================================
// Tool Registry - Main Class
// ============================================================================

/**
 * @class ToolRegistry
 * @brief Complete tool registry with full utility: logging, metrics, error handling,
 *        resource management, validation, configuration, and observability
 *
 * Responsibilities:
 * - Register and manage tool definitions
 * - Execute tools with comprehensive error handling
 * - Log all operations with structured logging
 * - Collect and export metrics
 * - Validate tool inputs and outputs
 * - Manage tool execution context and state
 * - Enforce resource limits and timeouts
 * - Provide distributed tracing hooks
 * - Handle resource cleanup and guards
 * - Support feature toggles and configuration
 * - Track execution statistics
 *
 * Thread-safe: All operations are thread-safe.
 *
 * @example
 * @code
 * auto logger = std::make_shared<Logger>();
 * auto metrics = std::make_shared<Metrics>();
 * ToolRegistry registry(logger, metrics);
 *
 * // Register a tool
 * ToolDefinition toolDef;
 * toolDef.name = "list_files";
 * toolDef.handler = [](const json& params) {
 *     // Implementation
 *     return json{{"files", json::array()}};
 * };
 * registry.registerTool(toolDef);
 *
 * // Execute tool
 * auto result = registry.executeTool("list_files", json::object());
 * if (result.success) {
 *     std::cout << "Result: " << result.data.dump() << std::endl;
 * }
 * @endcode
 */
class ToolRegistry {
public:
    // ========================================================================
    // Constructor and Destructor
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param logger Shared logger instance
     * @param metrics Shared metrics instance
     */
    explicit ToolRegistry(
        std::shared_ptr<Logger> logger = nullptr,
        std::shared_ptr<Metrics> metrics = nullptr
    );
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~ToolRegistry();
    
    // ========================================================================
    // Tool Registration
    // ========================================================================
    
    /**
     * @brief Register a tool in the registry
     * @param toolDef Tool definition with handler
     * @return true if registration successful, false if tool already exists or invalid
     *
     * Logs: INFO "Tool registered: {name}", DEBUG with configuration details
     * Metrics: Increments "tools_registered" counter
     */
    bool registerTool(const ToolDefinition& toolDef);
    
    /**
     * @brief Unregister a tool from the registry
     * @param toolName Name of tool to unregister
     * @return true if unregistered successfully
     *
     * Logs: INFO "Tool unregistered: {name}"
     * Metrics: Increments "tools_unregistered" counter
     */
    bool unregisterTool(const std::string& toolName);
    
    /**
     * @brief Check if a tool is registered
     * @param toolName Name of tool to check
     * @return true if tool exists in registry
     */
    bool hasTool(const std::string& toolName) const;
    
    /**
     * @brief Get tool definition
     * @param toolName Name of tool
     * @return Tool definition or nullptr if not found
     */
    std::shared_ptr<const ToolDefinition> getTool(const std::string& toolName) const;
    
    /**
     * @brief Get all registered tools
     * @return List of all tool names
     */
    std::vector<std::string> getRegisteredTools() const;
    
    /**
     * @brief Get tools by category
     * @param category Tool category to filter by
     * @return List of tool names in category
     */
    std::vector<std::string> getToolsByCategory(ToolCategory category) const;
    
    // ========================================================================
    // Tool Execution - Main Interface
    // ========================================================================
    
    /**
     * @brief Execute a tool with full utility and error handling
     * @param toolName Name of tool to execute
     * @param parameters Input parameters as JSON
     * @return Complete result with status, data, metrics, and context
     *
     * Execution Flow:
     * 1. Log execution start with parameters (DEBUG level if detailed logging enabled)
     * 2. Check feature toggle / enablement
     * 3. Check tool exists, validate existence
     * 4. Validate input parameters against schema
     * 5. Check cache if enabled
     * 6. Create execution context with trace IDs
     * 7. Start metrics collection
     * 8. Execute tool with try-catch
     * 9. Validate output
     * 10. Collect metrics and logs
     * 11. Handle errors with retry logic if enabled
     * 12. Return complete result with all metadata
     *
     * Logging:
     * - DEBUG: Start, input params, execution details
     * - INFO: Completion with timing and status
     * - WARN: Validation failures, retries
     * - ERROR: Failures, exceptions
     *
     * Metrics:
     * - Counter: "tool_executions", "tool_executions_{status}"
     * - Histogram: "tool_execution_time_ms", "tool_input_size_bytes", "tool_output_size_bytes"
     * - Gauge: "tool_active_executions"
     */
    ToolResult executeTool(
        const std::string& toolName,
        const json& parameters
    );
    
    /**
     * @brief Execute tool with custom configuration
     * @param toolName Name of tool to execute
     * @param parameters Input parameters
     * @param config Custom execution configuration (overrides tool's default)
     * @return Complete result with status and metrics
     */
    ToolResult executeToolWithConfig(
        const std::string& toolName,
        const json& parameters,
        const ToolExecutionConfig& config
    );
    
    /**
     * @brief Execute tool with tracing context
     * @param toolName Name of tool
     * @param parameters Input parameters
     * @param traceId Distributed trace ID
     * @param parentSpanId Parent span ID for nesting
     * @return Result with trace context populated
     *
     * Used for distributed tracing integration (OpenTelemetry, etc.)
     */
    ToolResult executeToolWithTrace(
        const std::string& toolName,
        const json& parameters,
        const std::string& traceId,
        const std::string& parentSpanId = ""
    );
    
    /**
     * @brief Execute multiple tools in sequence with error handling
     * @param toolExecution Vector of {toolName, parameters} pairs
     * @param stopOnError If true, stop at first failure
     * @return Vector of results in same order as input
     *
     * Logs sequence start/end and individual tool execution
     * Metrics: Tracks batch execution
     */
    std::vector<ToolResult> executeBatch(
        const std::vector<std::pair<std::string, json>>& toolExecution,
        bool stopOnError = false
    );
    
    // ========================================================================
    // Configuration Management
    // ========================================================================
    
    /**
     * @brief Set global timeout for all tool executions
     * @param timeoutMs Timeout in milliseconds
     *
     * Logs: INFO "Global timeout configured: {timeoutMs}ms"
     */
    void setGlobalTimeout(int32_t timeoutMs);
    
    /**
     * @brief Set timeout for specific tool
     * @param toolName Name of tool
     * @param timeoutMs Timeout in milliseconds
     * @return true if tool exists and timeout was set
     */
    bool setToolTimeout(const std::string& toolName, int32_t timeoutMs);
    
    /**
     * @brief Enable/disable tool execution
     * @param toolName Name of tool
     * @param enabled true to enable, false to disable
     * @return true if tool exists
     *
     * Logs: INFO "Tool {enabled/disabled}: {toolName}"
     */
    bool setToolEnabled(const std::string& toolName, bool enabled);
    
    /**
     * @brief Enable/disable detailed logging for tool
     * @param toolName Name of tool
     * @param enabled true to enable detailed logging
     * @return true if tool exists
     */
    bool setDetailedLogging(const std::string& toolName, bool enabled);
    
    /**
     * @brief Load configuration from JSON file
     * @param configPath Path to configuration file
     * @return true if loaded successfully
     *
     * Config format:
     * {
     *   "globalTimeout": 30000,
     *   "tools": {
     *     "tool_name": {
     *       "enabled": true,
     *       "timeout": 5000,
     *       "maxRetries": 3,
     *       ...
     *     }
     *   }
     * }
     *
     * Logs: INFO "Configuration loaded", WARN for invalid entries
     */
    bool loadConfiguration(const std::string& configPath);
    
    /**
     * @brief Save current configuration to JSON file
     * @param configPath Path where to save configuration
     * @return true if saved successfully
     *
     * Logs: INFO "Configuration saved"
     */
    bool saveConfiguration(const std::string& configPath) const;
    
    // ========================================================================
    // Input/Output Validation
    // ========================================================================
    
    /**
     * @brief Validate input parameters against tool schema
     * @param toolName Name of tool
     * @param parameters Input parameters to validate
     * @return Validation result with error message if invalid
     *
     * Logs: DEBUG level validation details
     */
    struct ValidationResult {
        bool valid = false;
        std::string error;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validateInput(
        const std::string& toolName,
        const json& parameters
    ) const;
    
    /**
     * @brief Validate output against tool schema
     * @param toolName Name of tool
     * @param output Output to validate
     * @return Validation result
     */
    ValidationResult validateOutput(
        const std::string& toolName,
        const json& output
    ) const;
    
    /**
     * @brief Set input validation rules for tool
     * @param toolName Name of tool
     * @param validation Validation rules
     * @return true if tool exists
     */
    bool setInputValidation(
        const std::string& toolName,
        const ToolInputValidation& validation
    );
    
    // ========================================================================
    // Statistics and Observability
    // ========================================================================
    
    /**
     * @brief Get execution statistics for a tool
     * @param toolName Name of tool
     * @return JSON object with stats (call count, success rate, avg latency, etc.)
     */
    json getToolStatistics(const std::string& toolName) const;
    
    /**
     * @brief Get statistics for all tools
     * @return JSON object with per-tool statistics
     */
    json getAllStatistics() const;
    
    /**
     * @brief Get execution history for a tool
     * @param toolName Name of tool
     * @param maxEntries Maximum number of entries to return
     * @return Vector of recent execution results
     */
    std::vector<ToolResult> getExecutionHistory(
        const std::string& toolName,
        size_t maxEntries = 100
    ) const;
    
    /**
     * @brief Clear execution history
     * @param toolName Name of tool (if empty, clear all)
     */
    void clearExecutionHistory(const std::string& toolName = "");
    
    /**
     * @brief Get metrics snapshot for monitoring
     * @return JSON with current metrics values
     *
     * Format:
     * {
     *   "timestamp_ms": 1234567890,
     *   "total_executions": 100,
     *   "successful_executions": 95,
     *   "failed_executions": 5,
     *   "active_executions": 2,
     *   "average_latency_ms": 150.5,
     *   "tools": {
     *     "tool_name": {
     *       "execution_count": 10,
     *       "success_rate": 0.95,
     *       ...
     *     }
     *   }
     * }
     */
    json getMetricsSnapshot() const;
    
    /**
     * @brief Reset all metrics and statistics
     *
     * Logs: WARN "Metrics and statistics reset"
     */
    void resetMetrics();
    
    // ========================================================================
    // Error Recovery and Rollback
    // ========================================================================
    
    /**
     * @brief Set retry policy
     * @param toolName Name of tool
     * @param maxRetries Maximum number of retries
     * @param delayMs Delay between retries in milliseconds
     * @return true if tool exists
     */
    bool setRetryPolicy(
        const std::string& toolName,
        int32_t maxRetries,
        int32_t delayMs
    );
    
    /**
     * @brief Enable/disable automatic retries for tool
     * @param toolName Name of tool
     * @param enabled true to enable retries
     * @return true if tool exists
     */
    bool setRetryEnabled(const std::string& toolName, bool enabled);
    
    // ========================================================================
    // Caching
    // ========================================================================
    
    /**
     * @brief Enable result caching for tool
     * @param toolName Name of tool
     * @param cacheValidityMs Cache validity duration in milliseconds
     * @return true if tool exists
     */
    bool enableCaching(const std::string& toolName, int32_t cacheValidityMs);
    
    /**
     * @brief Disable result caching for tool
     * @param toolName Name of tool
     * @return true if tool exists
     */
    bool disableCaching(const std::string& toolName);
    
    /**
     * @brief Clear cache for tool
     * @param toolName Name of tool (if empty, clear all caches)
     */
    void clearCache(const std::string& toolName = "");
    
    /**
     * @brief Get cache statistics
     * @return JSON with cache hits/misses per tool
     */
    json getCacheStatistics() const;
    
    // ========================================================================
    // Health and Diagnostics
    // ========================================================================
    
    /**
     * @brief Check health of tool registry
     * @return JSON with health status
     *
     * Checks:
     * - Total tools registered
     * - Tools with errors
     * - Recent error rate
     * - Average latency
     * - Memory usage estimate
     */
    json getHealthStatus() const;
    
    /**
     * @brief Run self-test on all tools
     * @return JSON with test results for each tool
     *
     * Attempts basic execution with minimal parameters
     * Logs: INFO "Self-test results"
     */
    json runSelfTest();
    
    /**
     * @brief Run self-test on specific tool
     * @param toolName Name of tool
     * @return JSON with test result
     */
    json runToolSelfTest(const std::string& toolName);

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    
    // Tool registry
    mutable std::mutex m_toolRegistryMutex;
    std::map<std::string, std::shared_ptr<ToolDefinition>> m_tools;
    
    // Configuration
    int32_t m_globalTimeout = 30000;
    std::map<std::string, ToolExecutionConfig> m_toolConfigs;
    
    // Execution tracking
    mutable std::mutex m_executionMutex;
    std::map<std::string, std::vector<ToolResult>> m_executionHistory;
    std::map<std::string, uint32_t> m_activeExecutions;
    
    // Statistics tracking
    struct ToolStats {
        uint64_t totalExecutions = 0;
        uint64_t successfulExecutions = 0;
        uint64_t failedExecutions = 0;
        int64_t totalLatencyMs = 0;
        int64_t totalInputBytes = 0;
        int64_t totalOutputBytes = 0;
        uint32_t retryCount = 0;
        uint32_t cacheHits = 0;
        int64_t lastExecutionTimeMs = 0;
        std::string lastError;
    };
    mutable std::mutex m_statsMutex;
    std::map<std::string, ToolStats> m_statistics;
    
    // Caching
    struct CacheEntry {
        json data;
        int64_t createdAtMs = 0;
    };
    mutable std::mutex m_cacheMutex;
    std::map<std::string, std::map<std::string, CacheEntry>> m_cache;  // tool -> (params_hash -> result)
    
    // ========================================================================
    // Private Methods
    // ========================================================================
    
    /**
     * @brief Internal tool execution with full error handling
     */
    ToolResult executeToolInternal(
        const std::string& toolName,
        const json& parameters,
        const ToolExecutionConfig& config,
        const std::string& traceId = "",
        const std::string& parentSpanId = ""
    );
    
    /**
     * @brief Validate tool input
     */
    bool validateToolInput(
        const std::string& toolName,
        const json& parameters,
        std::string& errorMsg
    );
    
    /**
     * @brief Check and retrieve cached result
     */
    std::optional<ToolResult> getCachedResult(
        const std::string& toolName,
        const json& parameters
    );
    
    /**
     * @brief Cache execution result
     */
    void cacheResult(
        const std::string& toolName,
        const json& parameters,
        const ToolResult& result
    );
    
    /**
     * @brief Generate cache key from parameters
     */
    std::string generateCacheKey(const json& parameters) const;
    
    /**
     * @brief Generate unique execution ID
     */
    std::string generateExecutionId() const;
    
    /**
     * @brief Log structured execution start
     */
    void logExecutionStart(
        const std::string& toolName,
        const json& parameters,
        const std::string& executionId,
        bool detailedLogging
    );
    
    /**
     * @brief Log structured execution completion
     */
    void logExecutionCompletion(
        const ToolResult& result,
        const std::string& toolName,
        bool detailedLogging
    );
    
    /**
     * @brief Record execution metrics
     */
    void recordMetrics(const ToolResult& result, const std::string& toolName);
    
    /**
     * @brief Update statistics
     */
    void updateStatistics(const ToolResult& result, const std::string& toolName);
    
    /**
     * @brief Execute tool with retry logic
     */
    ToolResult executeWithRetry(
        const std::string& toolName,
        const json& parameters,
        const ToolExecutionConfig& config,
        std::function<ToolResult()> executeFunc
    );
};
