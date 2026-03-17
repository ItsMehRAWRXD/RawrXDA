#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <unordered_map>

// Helper forward declarations
class Logger {
public:
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
};
class Metrics {
public:
    virtual void incrementCounter(const std::string& name, int val) = 0;
    virtual void incrementGauge(const std::string& name, int val) = 0;
};

// Data Structures
enum class ToolCategory {
    Utility,
    DataProcessing,
    Network,
    System,
    AI,
    Other
};

struct ToolConfig {
    std::string toolName;
    int timeoutMs = 5000;
    int maxRetries = 3;
};

struct ToolDefinition {
    std::string name;
    ToolCategory category;
    bool experimental;
    ToolConfig config;
    std::function<std::string(const std::string&)> handler;
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

struct ToolStats {
    int calls = 0;
    int errors = 0;
    double avgLatencyMs = 0.0;
};

class ToolRegistry {
public:
    ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    ~ToolRegistry();

    bool registerTool(const ToolDefinition& toolDef);
    // Add other public methods that might be needed based on usage
    
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
    
    // Caches and history placeholders
    std::map<std::string, std::string> m_cache;
    std::vector<std::string> m_executionHistory;
};
