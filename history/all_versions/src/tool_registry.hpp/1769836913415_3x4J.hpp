#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Forward declarations of interface classes
class Logger {
public:
    virtual void info(const std::string& msg) = 0;
    virtual void warn(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
    virtual ~Logger() = default;
};

class Metrics {
public:
    virtual void incrementCounter(const std::string& metric, int value = 1) = 0;
    virtual void incrementGauge(const std::string& metric, int value = 1) = 0;
    virtual void decrementGauge(const std::string& metric, int value = 1) = 0;
    virtual ~Metrics() = default;
};

// Tool Definitions
enum class ToolCategory {
    General,
    FileSystem,
    Network,
    Analysis,
    Editing,
    System
};

enum class ToolExecutionStatus {
    NotStarted,
    Running,
    Completed,
    Failed,
    TimedOut,
    Cancelled,
    SkippedByToggle
};

struct ToolConfig {
    std::string toolName;
    int timeoutMs = 30000;
    int maxRetries = 3;
    bool requiresAuth = false;
    bool enabled = true;
    bool detailedLogging = false;
    int cacheValidityMs = 0; // 0 = no cache
    int retryDelayMs = 1000;
    bool retryEnabled = true;
};

using ToolHandler = std::function<json(const json&)>;

struct ToolDefinition {
    std::string name;
    std::string description;
    ToolCategory category = ToolCategory::General;
    bool experimental = false;
    json inputSchema;
    ToolConfig config;
    ToolHandler handler;
};

struct ToolStats {
    uint64_t totalCalls = 0;
    uint64_t successes = 0;
    uint64_t failures = 0;
    uint64_t timeouts = 0;
    double avgLatencyMs = 0.0;
};

struct ToolResult {
    bool success;
    json output;
    std::string error;
    ToolExecutionStatus status;
    int64_t durationMs;
    std::string executionId;
};

struct ToolInputValidation {
    bool strictSchema = true;
    bool allowUnknownProperties = false;
};

namespace ToolRegistryHelpers {
    std::string generateUUID();
    int64_t getCurrentTimeMs();
    std::string statusToString(ToolExecutionStatus status);
    std::string hashJson(const json& obj);
}

class ToolRegistry {
public:
    struct ValidationResult {
        bool valid;
        std::vector<std::string> errors;
    };

    ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    ~ToolRegistry();

    bool registerTool(const ToolDefinition& toolDef);
    bool unregisterTool(const std::string& name);
    bool hasTool(const std::string& toolName) const;
    
    // Execution
    ToolResult executeTool(const std::string& name, const json& params);
    ToolResult executeToolWithConfig(const std::string& name, const json& params, const ToolConfig& config);
    ToolResult executeToolWithTrace(const std::string& name, const json& params, const std::string& traceId);
    
    // Batch
    std::vector<ToolResult> executeBatch(const std::vector<std::pair<std::string, json>>& toolExecution, bool stopOnError = false);

    // Query
    std::vector<ToolDefinition> getAllTools() const;
    std::shared_ptr<const ToolDefinition> getTool(const std::string& name) const; 
    std::vector<std::string> getRegisteredTools() const;
    std::vector<std::string> getToolsByCategory(ToolCategory category) const;

    // Configuration & Meta
    void setGlobalTimeout(int32_t timeoutMs);
    bool setToolTimeout(const std::string& toolName, int32_t timeoutMs);
    bool setToolEnabled(const std::string& toolName, bool enabled);
    bool setDetailedLogging(const std::string& toolName, bool enabled);
    bool loadConfiguration(const std::string& configPath);
    bool saveConfiguration(const std::string& configPath) const;
    
    // Validation
    bool setInputValidation(const std::string& toolName, const ToolInputValidation& validation);
    ValidationResult validateInput(const std::string& toolName, const json& input);
    ValidationResult validateOutput(const std::string& toolName, const json& output);

    // Stats & History
    json getToolStatistics(const std::string& toolName) const;
    json getAllStatistics() const;
    std::vector<ToolResult> getExecutionHistory(const std::string& toolName, size_t limit) const;
    void clearExecutionHistory(const std::string& toolName);
    json getMetricsSnapshot() const;
    void resetMetrics();
    
    // Reliability
    bool setRetryPolicy(const std::string& toolName, int32_t maxRetries, int32_t initialDelayMs);
    bool setRetryEnabled(const std::string& toolName, bool enabled);
    
    // Caching
    bool enableCaching(const std::string& toolName, int32_t cacheValidityMs);
    bool disableCaching(const std::string& toolName);
    void clearCache();
    void clearCache(const std::string& toolName);
    json getCacheStatistics() const;
    
    // Diagnostics
    json getHealthStatus() const;
    json runSelfTest();
    json runToolSelfTest(const std::string& toolName);

private:
    ToolResult executeToolInternal(const std::string& name, const json& params, const ToolConfig* overrideConfig = nullptr, const std::string* traceId = nullptr);
    bool validateToolInput(const std::string& toolName, const json& params, std::string& error);
    
    std::optional<ToolResult> getCachedResult(const std::string& toolName, const json& parameters);
    void cacheResult(const std::string& toolName, const json& parameters, const ToolResult& result);
    std::string generateCacheKey(const json& parameters) const;
    
    std::string generateExecutionId() const;
    void logExecutionStart(const std::string& toolName, const json& params, const std::string& executionId, bool detailed);
    void logExecutionCompletion(const ToolResult& result, const std::string& toolName, bool detailed);
    void recordMetrics(const ToolResult& result, const std::string& toolName);
    void updateStatistics(const ToolResult& result, const std::string& toolName);
    
    ToolResult executeWithRetry(const std::string& toolName, const json& params, const ToolConfig& config);

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    
    mutable std::mutex m_toolRegistryMutex;
    mutable std::mutex m_cacheMutex;
    mutable std::mutex m_executionMutex;
    mutable std::mutex m_statsMutex;
    
    std::unordered_map<std::string, std::shared_ptr<ToolDefinition>> m_tools;
    std::unordered_map<std::string, ToolConfig> m_toolConfigs;
    std::unordered_map<std::string, ToolStats> m_statistics;
    std::unordered_map<std::string, ToolInputValidation> m_validations;
    
    struct CacheEntry {
        std::string resultJson;
        int64_t timestamp;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, CacheEntry>> m_cache;
    
    struct ExecutionRecord {
        std::string executionId;
        std::string toolName;
        int64_t startTime;
        int64_t endTime;
        ToolExecutionStatus status;
        ToolResult result;
    };
    std::unordered_map<std::string, std::vector<ExecutionRecord>> m_executionHistory;
    
    int32_t m_globalTimeoutMs = 30000;
};
