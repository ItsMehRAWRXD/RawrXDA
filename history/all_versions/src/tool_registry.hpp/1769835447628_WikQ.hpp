#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include "nlohmann/json.hpp"

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
    virtual void incrementCounter(const std::string& metric, int value) = 0;
    virtual void incrementGauge(const std::string& metric, int value) = 0;
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

struct ToolConfig {
    std::string toolName;
    int timeoutMs = 30000;
    int maxRetries = 3;
    bool requiresAuth = false;
};

using ToolHandler = std::function<nlohmann::json(const nlohmann::json&)>;

struct ToolDefinition {
    std::string name;
    std::string description;
    ToolCategory category = ToolCategory::General;
    bool experimental = false;
    nlohmann::json inputSchema;
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

enum class ToolExecutionStatus {
    NotStarted,
    Running,
    Completed,
    Failed,
    TimedOut,
    Cancelled,
    SkippedByToggle
};

namespace ToolRegistryHelpers {
    std::string generateUUID();
    int64_t getCurrentTimeMs();
    std::string statusToString(ToolExecutionStatus status);
    std::string hashJson(const nlohmann::json& obj);
}

class ToolRegistry {
public:
    ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    ~ToolRegistry();

    bool registerTool(const ToolDefinition& toolDef);
    bool unregisterTool(const std::string& name);
    
    // Execution
    nlohmann::json executeTool(const std::string& name, const nlohmann::json& params);
    
    // Query
    std::vector<ToolDefinition> getAllTools() const;
    std::shared_ptr<ToolDefinition> getTool(const std::string& name) const;
    
    // State
    void clearCache();
    
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
    
    // Cache: hash(params) -> result_json_string
    std::unordered_map<std::string, std::string> m_cache;
    
    struct ExecutionRecord {
        std::string executionId;
        std::string toolName;
        int64_t startTime;
        int64_t endTime;
        ToolExecutionStatus status;
    };
    std::vector<ExecutionRecord> m_executionHistory;
};
