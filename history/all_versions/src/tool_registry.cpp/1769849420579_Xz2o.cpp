#include "tool_registry.hpp"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <future>
#include <random>
#include <mutex>

// Platform specific UUID
#ifdef _WIN32
#include <objbase.h>
#else
#include <uuid/uuid.h>
#endif

namespace RawrXD {

namespace ToolRegistryHelpers {
    std::string generateUUID() {
#ifdef _WIN32
        GUID guid;
        CoCreateGuid(&guid);
        char buffer[128];
        snprintf(buffer, sizeof(buffer), 
                 "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
                 guid.Data1, guid.Data2, guid.Data3, 
                 guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::string(buffer);
#else
        // Fallback or linux impl
        return "00000000-0000-0000-0000-000000000000"; 
#endif
    }

    int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    std::string statusToString(ToolExecutionStatus status) {
        switch(status) {
            case ToolExecutionStatus::NotStarted: return "NotStarted";
            case ToolExecutionStatus::Running: return "Running";
            case ToolExecutionStatus::Completed: return "Completed";
            case ToolExecutionStatus::Failed: return "Failed";
            case ToolExecutionStatus::TimedOut: return "TimedOut";
            default: return "Unknown";
        }
    }
    
    std::string hashJson(const json& obj) {
        std::hash<std::string> hasher;
        return std::to_string(hasher(obj.dump()));
    }
}

ToolRegistry::ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics), m_globalTimeout(60000) {
}

ToolRegistry::~ToolRegistry() {}

bool ToolRegistry::registerTool(const ToolDefinition& toolDef) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    m_tools[toolDef.metadata.name] = std::make_shared<ToolDefinition>(toolDef);
    m_toolConfigs[toolDef.metadata.name] = toolDef.config;
    m_statistics[toolDef.metadata.name] = ToolStats();
    m_validations[toolDef.metadata.name] = toolDef.inputValidation;
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    return m_tools.erase(name) > 0;
}

bool ToolRegistry::hasTool(const std::string& toolName) const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    return m_tools.find(toolName) != m_tools.end();
}

std::shared_ptr<const ToolDefinition> ToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(name);
    if (it != m_tools.end()) return it->second;
    return nullptr;
}

std::vector<std::string> ToolRegistry::getRegisteredTools() const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    std::vector<std::string> tools;
    for (const auto& kv : m_tools) tools.push_back(kv.first);
    return tools;
}

ToolResult ToolRegistry::executeTool(const std::string& name, const json& params) {
    ToolConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
        if (m_toolConfigs.find(name) != m_toolConfigs.end()) {
            cfg = m_toolConfigs[name];
        } else {
            ToolResult res; res.error = "Tool not found"; return res;
        }
    }
    return executeToolInternal(name, params, cfg);
}

ToolResult ToolRegistry::executeToolWithConfig(const std::string& name, const json& params, const ToolConfig& config) {
    return executeToolInternal(name, params, config);
}

ToolResult ToolRegistry::executeToolWithTrace(const std::string& name, const json& params, const std::string& traceId) {
    ToolConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
        if (m_toolConfigs.find(name) != m_toolConfigs.end()) {
            cfg = m_toolConfigs[name];
        } else {
             ToolResult res; res.error = "Tool not found"; return res;
        }
    }
    return executeToolInternal(name, params, cfg, traceId, "");
}

ToolResult ToolRegistry::executeToolInternal(const std::string& name, const json& params, const ToolConfig& config) {
    return executeToolInternal(name, params, config, "", "");
}

ToolResult ToolRegistry::executeToolInternal(
    const std::string& name, 
    const json& params, 
    const ToolConfig& config, 
    const std::string& traceId, 
    const std::string& parentSpanId
) {
    std::string valError;
    if (!validateToolInput(name, params, valError)) {
        ToolResult res; res.success = false; res.error = valError; return res;
    }
    
    // Caching check
    if (config.enableCaching) {
        auto cached = getCachedResult(name, params);
        if (cached) return *cached;
    }
    
    return executeWithRetry(name, params, config);
}

ToolResult ToolRegistry::executeWithRetry(const std::string& toolName, const json& params, const ToolConfig& config) {
    ToolResult finalResult;
    int maxRetries = config.retryOnFailure ? config.maxRetries : 0;
    
    for(int attempt = 0; attempt <= maxRetries; ++attempt) {
        std::string execId = generateExecutionId();
        logExecutionStart(toolName, params, execId, config.enableDetailedLogging);
        
        ToolResult result;
        result.executionContext.executionId = execId;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::shared_ptr<ToolDefinition> toolDef;
        {
             std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
             if (m_tools.count(toolName)) toolDef = m_tools[toolName];
        }
        
        if (!toolDef || !toolDef->handler) {
            result.success = false; result.error = "Tool handler missing";
            return result;
        }

        try {
            // Sync execution for this implementation (could be async in future)
            result.data = toolDef->handler(params);
            result.success = true;
        } catch (const std::exception& e) {
            result.success = false;
            result.error = e.what();
        } catch (...) {
            result.success = false;
            result.error = "Unknown exception";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        result.executionContext.metrics.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        result.retryCount = attempt;
        
        logExecutionCompletion(result, toolName, config.enableDetailedLogging);
        updateStatistics(result, toolName);
        
        if (result.success) {
            if (config.enableCaching) cacheResult(toolName, params, result);
            return result;
        }
        
        finalResult = result;
        if (attempt < maxRetries) {
             std::this_thread::sleep_for(std::chrono::milliseconds(config.retryDelayMs));
        }
    }
    return finalResult;
}

bool ToolRegistry::validateToolInput(const std::string& toolName, const json& params, std::string& error) {
    return true; // Simplified for now
}

std::optional<ToolResult> ToolRegistry::getCachedResult(const std::string& toolName, const json& parameters) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    std::string key = generateCacheKey(parameters);
    if (m_cache.count(toolName) && m_cache[toolName].count(key)) {
        return ToolResult{ true, m_cache[toolName][key].data }; // Simplify reconstruction
    }
    return std::nullopt;
}

void ToolRegistry::cacheResult(const std::string& toolName, const json& parameters, const ToolResult& result) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache[toolName][generateCacheKey(parameters)] = { result.data, ToolRegistryHelpers::getCurrentTimeMs() };
}

std::string ToolRegistry::generateCacheKey(const json& parameters) const {
    return ToolRegistryHelpers::hashJson(parameters);
}

std::string ToolRegistry::generateExecutionId() const {
    return ToolRegistryHelpers::generateUUID();
}

void ToolRegistry::logExecutionStart(const std::string& toolName, const json& params, const std::string& executionId, bool detailed) {
    if (m_logger) m_logger->info("START " + toolName + " [" + executionId + "]");
}

void ToolRegistry::logExecutionCompletion(const ToolResult& result, const std::string& toolName, bool detailed) {
    if (m_logger) {
        if (result.success) m_logger->info("SUCCESS " + toolName);
        else m_logger->error("FAILED " + toolName + ": " + result.error);
    }
}

void ToolRegistry::updateStatistics(const ToolResult& result, const std::string& toolName) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_statistics[toolName].totalExecutions++;
    if (result.success) m_statistics[toolName].successCount++;
    else m_statistics[toolName].failureCount++;
}

// Accessors
std::vector<ToolDefinition> ToolRegistry::getAllTools() const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    std::vector<ToolDefinition> res;
    for(const auto& kv : m_tools) res.push_back(*kv.second);
    return res;
}

std::vector<std::string> ToolRegistry::getToolsByCategory(ToolCategory category) const { return {}; }
void ToolRegistry::setGlobalTimeout(int32_t timeoutMs) { m_globalTimeout = timeoutMs; }
bool ToolRegistry::setToolTimeout(const std::string& toolName, int32_t timeoutMs) { return true; }
bool ToolRegistry::setToolEnabled(const std::string& toolName, bool enabled) { return true; }
bool ToolRegistry::setDetailedLogging(const std::string& toolName, bool enabled) { return true; }
bool ToolRegistry::loadConfiguration(const std::string& configPath) { return true; }
bool ToolRegistry::saveConfiguration(const std::string& configPath) const { return true; }
bool ToolRegistry::setInputValidation(const std::string& toolName, const ToolInputValidation& validation) { return true; }
ToolRegistry::ValidationResult ToolRegistry::validateInput(const std::string& toolName, const json& input) const { return {true, ""}; }
ToolRegistry::ValidationResult ToolRegistry::validateOutput(const std::string& toolName, const json& output) const { return {true, ""}; }
json ToolRegistry::getToolStatistics(const std::string& toolName) const { return json(); }
json ToolRegistry::getAllStatistics() const { return json(); }
std::vector<ToolResult> ToolRegistry::getExecutionHistory(const std::string& toolName, size_t limit) const { return {}; }
void ToolRegistry::clearExecutionHistory(const std::string& toolName) {}
json ToolRegistry::getMetricsSnapshot() const { return json(); }
void ToolRegistry::resetMetrics() {}
bool ToolRegistry::setRetryPolicy(const std::string& toolName, int32_t maxRetries, int32_t initialDelayMs) { return true; }
bool ToolRegistry::setRetryEnabled(const std::string& toolName, bool enabled) { return true; }
bool ToolRegistry::enableCaching(const std::string& toolName, int32_t cacheValidityMs) { return true; }
bool ToolRegistry::disableCaching(const std::string& toolName) { return true; }
void ToolRegistry::clearCache() {}
void ToolRegistry::clearCache(const std::string& toolName) {}
json ToolRegistry::getCacheStatistics() const { return json(); }
json ToolRegistry::getHealthStatus() const { return json(); }
json ToolRegistry::runSelfTest() { return json(); }
json ToolRegistry::runToolSelfTest(const std::string& toolName) { return json(); }
std::vector<ToolResult> ToolRegistry::executeBatch(const std::vector<std::pair<std::string, json>>& toolExecution, bool stopOnError) { return {}; }

    std::vector<ToolMetadata> ToolRegistry::getAllToolMetadata() const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::vector<ToolMetadata> result;
        result.reserve(tools_.size());
        
        for (const auto& pair : tools_) {
            result.push_back(pair.second.metadata);
        }
        return result;
    }


    std::vector<ToolMetadata> ToolRegistry::getToolsByCategory(const std::string& category) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::vector<ToolMetadata> result;
        
        for (const auto& pair : tools_) {
            if (pair.second.metadata.category == category) {
                result.push_back(pair.second.metadata);
            }
        }
        return result;
    }

    std::optional<ToolMetadata> ToolRegistry::getToolMetadata(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second.metadata;
        }
        return std::nullopt;
    }

    // =========================================================================================
    // Execution Logic
    // =========================================================================================

    ToolResult ToolRegistry::executeTool(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        // 1. Discovery & Validation
        {
            std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
            if (tools_.find(name) == tools_.end()) {
                return {false, "", "Tool not found: " + name, 404, 0.0, {}};
            }
        }

        // 2. Pre-execution Hooks (Middleware)
        for (const auto& hook : pre_hooks_) {
            try {
                if (!hook(name, args)) {
                    return {false, "", "Execution cancelled by pre-execution hook", 403, 0.0, {}};
                }
            } catch (const std::exception& e) {
                return {false, "", std::string("Middleware exception: ") + e.what(), 500, 0.0, {}};
            }
        }

        // 3. Execution
        auto start = std::chrono::high_resolution_clock::now();
        ToolResult result = executeInternal(name, args, ctx);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        result.execution_time_ms = elapsed.count();

        // 4. Telemetry Update
        updateStats(name, result);

        // 5. Post-execution Hooks (Middleware)
        for (const auto& hook : post_hooks_) {
            try {
                hook(name, result);
            } catch (const std::exception& e) {
                std::cerr << "[ToolRegistry] Post-execution hook failed: " << e.what() << std::endl;
            }
        }

        return result;
    }

    std::future<ToolResult> ToolRegistry::executeToolAsync(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        // Enforce async policy or thread pool usage here if needed. For now, std::async default.
        return std::async(std::launch::async, [this, name, args, ctx]() {
            return this->executeTool(name, args, ctx);
        });
    }

    ToolResult ToolRegistry::executeInternal(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        ToolExecutor executor;
        ToolMetadata metadata;

        {
            std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
            auto it = tools_.find(name);
            if (it == tools_.end()) return {false, "", "Tool disappeared during execution setup", 500, 0.0, {}};
            executor = it->second.executor;
            metadata = it->second.metadata;
        }

        // Validate Arguments
        for (const auto& reqArg : metadata.arguments) {
            if (reqArg.required && args.find(reqArg.name) == args.end()) {
                 // Check if it has a default value
                 if (!reqArg.default_value.has_value()) {
                     return {false, "", "Missing required argument: " + reqArg.name, 400, 0.0, {}};
                 }
            }
            
            // Basic Type/Value Validation (Stub for string conversions)
            if (args.find(reqArg.name) != args.end()) {
                const std::string& val = args.at(reqArg.name);
                
                // Validate Allowed Values
                if (!reqArg.allowed_values.empty()) {
                    bool found = false;
                    for (const auto& allowed : reqArg.allowed_values) {
                        if (allowed == val) { found = true; break; }
                    }
                    if (!found) {
                        return {false, "", "Invalid value for argument '" + reqArg.name + "'. See allowlist.", 400, 0.0, {}};
                    }
                }

                // Validate Min/Max for numbers
                if ((reqArg.type == ToolArgType::INTEGER || reqArg.type == ToolArgType::FLOAT) && (reqArg.min_value.has_value() || reqArg.max_value.has_value())) {
                    try {
                        double numVal = std::stod(val);
                        if (reqArg.min_value.has_value() && numVal < reqArg.min_value.value()) {
                            return {false, "", "Argument '" + reqArg.name + "' is below minimum value", 400, 0.0, {}};
                        }
                        if (reqArg.max_value.has_value() && numVal > reqArg.max_value.value()) {
                            return {false, "", "Argument '" + reqArg.name + "' is above maximum value", 400, 0.0, {}};
                        }
                    } catch (...) {
                         return {false, "", "Argument '" + reqArg.name + "' must be a number", 400, 0.0, {}};
                    }
                }
            }
        }

        // Prepare context args (merging defaults if necessary)
        std::map<std::string, std::string> finalArgs = args;
        for (const auto& arg : metadata.arguments) {
            if (finalArgs.find(arg.name) == finalArgs.end() && arg.default_value.has_value()) {
                finalArgs[arg.name] = arg.default_value.value();
            }
        }

        try {
            if (ctx.verbose_logging) {
                std::cout << "[ToolExecution] Running " << name << "..." << std::endl;
            }
            return executor(finalArgs, ctx);
        } catch (const std::exception& e) {
            return {false, "", std::string("Unhandled exception in tool '") + name + "': " + e.what(), 500, 0.0, {}};
        } catch (...) {
            return {false, "", "Unknown fatal error in tool '" + name + "'", 500, 0.0, {}};
        }
    }

    // =========================================================================================
    // Middleware & Stats
    // =========================================================================================

    void ToolRegistry::addPreHook(PreExecutionHook hook) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        pre_hooks_.push_back(hook);
    }

    void ToolRegistry::addPostHook(PostExecutionHook hook) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        post_hooks_.push_back(hook);
    }

    void ToolRegistry::clearHooks() {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        pre_hooks_.clear();
        post_hooks_.clear();
    }

    void ToolRegistry::updateStats(const std::string& name, const ToolResult& result) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            ToolStats& stats = it->second.stats;
            stats.totalExecutions++;
            if (!result.success) {
                stats.failureCount++;
            }
            stats.totalDurationMs += result.durationMs;
            
            // Recalculate average
            if (stats.totalExecutions > 0) {
                stats.averageDurationMs = stats.totalDurationMs / stats.totalExecutions;
            }
        }
    }

    ToolStats ToolRegistry::getToolStats(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second.stats;
        }
        return {0, 0, 0, 0.0, 0.0, 0, ""};
    }

    std::map<std::string, ToolStats> ToolRegistry::getAllStats() const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::map<std::string, ToolStats> result;
        for (const auto& pair : tools_) {
            result[pair.first] = pair.second.stats;
        }
        return result;
    }

} // namespace RawrXD


