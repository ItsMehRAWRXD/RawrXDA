#ifndef RAWRXD_TOOL_REGISTRY_HPP
#define RAWRXD_TOOL_REGISTRY_HPP

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

namespace RawrXD {

// Interface classes
class Logger {
public:
    virtual void info(const std::string& msg) = 0;
    virtual void warn(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
    virtual void debug(const std::string& msg) = 0;
    virtual ~Logger() = default;
};

class Metrics {
public:
    virtual void incrementCounter(const std::string& metric, int value = 1) = 0;
    virtual void incrementGauge(const std::string& metric, int value = 1) = 0;
    virtual void decrementGauge(const std::string& metric, int value = 1) = 0;
    virtual void recordHistogram(const std::string& metric, double value) = 0;
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
    
    // Cpp specific field names
    bool enableExecution = true;
    bool enableDetailedLogging = false;
    bool retryOnFailure = true;
    bool enableCaching = false;
    int cacheValidityMs = 0; 
    int retryDelayMs = 1000;
};

// Aliases
using ToolExecutionConfig = ToolConfig;
using ToolHandler = std::function<json(const json&)>;

struct ToolInputValidation {
    bool strictSchema = true;
    bool allowUnknownProperties = false;
};

struct ToolMetadata {
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    json inputSchema;
    json outputSchema;
    std::string version = "1.0.0";
    std::string author = "Unknown";
};

struct ToolDefinition {
    ToolMetadata metadata;
    ToolConfig config;
    ToolHandler handler;
    ToolInputValidation inputValidation;

    // Compatibility constructors/helpers could go here
};

// Hook definitions
struct ToolContext {
    std::string traceId;
    std::string userId;
    std::string sessionId;
    std::map<std::string, std::string> headers;
    void* userData = nullptr;
};

using PreExecutionHook = std::function<void(const std::string&, const json&, const ToolContext&)>;
using PostExecutionHook = std::function<void(const std::string&, const ToolResult&, const ToolContext&)>;

struct ToolStats {
    uint64_t totalExecutions = 0;
    uint64_t successfulExecutions = 0;
    uint64_t failedExecutions = 0;
    uint64_t totalLatencyMs = 0;
    uint64_t totalInputBytes = 0;
    uint64_t totalOutputBytes = 0;
    uint64_t retryCount = 0;
    uint64_t cacheHits = 0;
    std::string lastError;
    int64_t lastExecutionTimeMs = 0;
};

struct ToolExecutionMetrics {
    int64_t executionTimeMs = 0;
    int64_t inputBytes = 0;
    int64_t outputBytes = 0;
    bool fromCache = false;
};

struct ToolExecutionContext {
    ToolExecutionStatus status = ToolExecutionStatus::NotStarted;
    ToolExecutionMetrics metrics;
    std::string traceId;
    std::string executionId;
};

struct ToolResult {
    bool success = false;
    json data;
    std::string error;
    ToolExecutionContext executionContext;
    int retryCount = 0;
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
        std::string error;
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

    // Extended Execution (Hidden Logic)
    std::vector<ToolMetadata> getAllTools() const;
    std::vector<ToolMetadata> getToolsByCategory(const std::string& category) const;
    std::optional<ToolMetadata> getToolMetadata(const std::string& name) const;
    
    ToolResult executeTool(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx = {});
    ToolResult executeInternal(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx);
    std::future<ToolResult> executeToolAsync(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx = {});

    // Hooks
    void addPreHook(PreExecutionHook hook);
    void addPostHook(PostExecutionHook hook);
    void clearHooks();

    // Statistics
    void updateStats(const std::string& name, const ToolResult& result);
    ToolStats getToolStats(const std::string& name) const;
    std::map<std::string, ToolStats> getAllStats() const;

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
    ValidationResult validateInput(const std::string& toolName, const json& input) const;
    ValidationResult validateOutput(const std::string& toolName, const json& output) const;

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
    ToolResult executeToolInternal(
        const std::string& name, 
        const json& params, 
        const ToolConfig& config, 
        const std::string& traceId, 
        const std::string& parentSpanId
    );
    
    // Helper overload for by-ref Config
    ToolResult executeToolInternal(const std::string& name, const json& params, const ToolConfig& config);

    bool validateToolInput(const std::string& toolName, const json& params, std::string& error);
    
    std::optional<ToolResult> getCachedResult(const std::string& toolName, const json& parameters);
    void cacheResult(const std::string& toolName, const json& parameters, const ToolResult& result);
    std::string generateCacheKey(const json& parameters) const;
    
    std::string generateExecutionId() const;
    void logExecutionStart(const std::string& toolName, const json& params, const std::string& executionId, bool detailed);
    void logExecutionCompletion(const ToolResult& result, const std::string& toolName, bool detailed);
    void recordMetrics(const ToolResult& result, const std::string& toolName);
    void updateStatistics(const ToolResult& result, const std::string& toolName);
    
    ToolResult executeWithRetry(const std::string& toolName, const json& params, int maxRetries, std::function<ToolResult()> action);
    ToolResult executeWithRetry(const std::string& toolName, const json& params, const ToolConfig& config);

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    
    // Mutexes aligned with cpp implementation
    mutable std::mutex m_toolRegistryMutex;
    mutable std::recursive_mutex registry_mutex_; // For new cpp logic
    mutable std::mutex m_cacheMutex;
    mutable std::mutex m_executionMutex;
    mutable std::mutex m_statsMutex;
    
    // Internal storage aligned with cpp implementation
    struct RegisteredTool {
        ToolMetadata metadata;
        ToolConfig config;
        ToolHandler handler;
        ToolInputValidation inputValidation;
    };

    std::unordered_map<std::string, RegisteredTool> tools_; // For cpp logic
    std::vector<PreExecutionHook> pre_hooks_;
    std::vector<PostExecutionHook> post_hooks_;

    // Legacy/Header-original members (kept for compatibility if needed, or shadowed)
    std::unordered_map<std::string, std::shared_ptr<ToolDefinition>> m_tools;
    std::unordered_map<std::string, ToolConfig> m_toolConfigs;
    std::unordered_map<std::string, ToolStats> m_statistics;
    std::unordered_map<std::string, ToolInputValidation> m_validations;
    
    struct CacheEntry {
        json data;
        int64_t createdAtMs;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, CacheEntry>> m_cache;
    
    struct ExecutionRecord {
        std::string executionId;
        std::string toolName;
        int64_t startTime;
        int64_t endTime;
        ToolExecutionContext executionContext;
        ToolResult result;
    };
    std::unordered_map<std::string, std::vector<ExecutionRecord>> m_executionHistory;
    
    int32_t m_globalTimeout;
    std::vector<std::string> m_activeExecutions; 
};

// Initialization helper
void registerSystemTools(ToolRegistry* registry);

#endif // RAWRXD_TOOL_REGISTRY_HPP
