/**
 * @file tool_registry_full_impl.cpp
 * @brief Test-only implementation of the ToolRegistry API defined in include/tool_registry.hpp.
 *
 * This file is compiled only with the test_tool_registry target.
 * The production Win32IDE target uses src/tool_registry_advanced.cpp which implements
 * the older src/tool_registry.hpp interface.
 */
#include "tool_registry.hpp"

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string genId() {
    static std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    return oss.str();
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
ToolRegistry::ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics)
    : m_logger(std::move(logger)), m_metrics(std::move(metrics)) {}

ToolRegistry::~ToolRegistry() = default;

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
bool ToolRegistry::registerTool(const ToolDefinition& toolDef) {
    if (toolDef.name.empty()) return false;
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto entry = std::make_shared<ToolDefinition>(toolDef);
    m_tools[toolDef.name] = std::move(entry);
    // Seed stats entry
    {
        std::lock_guard<std::mutex> sl(m_statsMutex);
        m_statistics.emplace(toolDef.name, ToolStats{});
    }
    if (m_logger) m_logger->info("ToolRegistry: Tool registered: " + toolDef.name);
    if (m_metrics) m_metrics->incrementCounter("tools_registered");
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& toolName) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    return m_tools.erase(toolName) > 0;
}

bool ToolRegistry::hasTool(const std::string& toolName) const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    return m_tools.count(toolName) > 0;
}

std::shared_ptr<const ToolDefinition> ToolRegistry::getTool(const std::string& toolName) const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    return it != m_tools.end() ? it->second : nullptr;
}

std::vector<std::string> ToolRegistry::getRegisteredTools() const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    std::vector<std::string> out;
    out.reserve(m_tools.size());
    for (const auto& [name, _] : m_tools) out.push_back(name);
    return out;
}

// ---------------------------------------------------------------------------
// Configuration helpers
// ---------------------------------------------------------------------------
void ToolRegistry::setGlobalTimeout(int32_t timeoutMs) {
    m_globalTimeout = timeoutMs;
    if (m_logger) m_logger->info("ToolRegistry: Global timeout configured: " + std::to_string(timeoutMs) + "ms");
}

bool ToolRegistry::setToolTimeout(const std::string& toolName, int32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->config.timeoutMs = timeoutMs;
    return true;
}

bool ToolRegistry::setToolEnabled(const std::string& toolName, bool enabled) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->config.enableExecution = enabled;
    if (m_logger)
        m_logger->info("ToolRegistry: Tool " + std::string(enabled ? "enabled" : "disabled") + ": " + toolName);
    return true;
}

bool ToolRegistry::setDetailedLogging(const std::string& toolName, bool enabled) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->config.enableDetailedLogging = enabled;
    return true;
}

bool ToolRegistry::setInputValidation(const std::string& toolName, const ToolInputValidation& validation) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->inputValidation = validation;
    return true;
}

bool ToolRegistry::enableCaching(const std::string& toolName, int32_t cacheValidityMs) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->config.enableCaching = true;
    it->second->config.cacheValidityMs = cacheValidityMs;
    return true;
}

bool ToolRegistry::setRetryEnabled(const std::string& toolName, bool enabled) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) return false;
    it->second->config.retryOnFailure = enabled;
    return true;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------
std::string ToolRegistry::generateExecutionId() const {
    return genId();
}

std::string ToolRegistry::generateCacheKey(const json& parameters) const {
    return parameters.dump();
}

bool ToolRegistry::validateToolInput(const std::string& toolName, const json& parameters, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) { errorMsg = "Tool not found: " + toolName; return false; }
    const auto& validation = it->second->inputValidation;
    for (const auto& field : validation.requiredFields) {
        if (!parameters.contains(field)) {
            errorMsg = "Missing required field: " + field + "; value is required";
            return false;
        }
    }
    if (validation.customValidator && !validation.customValidator(parameters)) {
        errorMsg = "Custom validation failed";
        return false;
    }
    return true;
}

std::optional<ToolResult> ToolRegistry::getCachedResult(const std::string& toolName, const json& parameters) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto tit = m_cache.find(toolName);
    if (tit == m_cache.end()) return std::nullopt;
    std::string key = generateCacheKey(parameters);
    auto cit = tit->second.find(key);
    if (cit == tit->second.end()) return std::nullopt;
    int64_t validityMs = 60000;
    {
        std::lock_guard<std::mutex> tl(m_toolRegistryMutex);
        auto ti = m_tools.find(toolName);
        if (ti != m_tools.end()) validityMs = ti->second->config.cacheValidityMs;
    }
    if (nowMs() - cit->second.createdAtMs > validityMs) {
        tit->second.erase(cit);
        return std::nullopt;
    }
    ToolResult r;
    r.success = true;
    r.data = cit->second.data;
    r.metrics.fromCache = true;
    r.executionContext.status = ToolExecutionStatus::Completed;
    return r;
}

void ToolRegistry::cacheResult(const std::string& toolName, const json& parameters, const ToolResult& result) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    CacheEntry entry;
    entry.data = result.data;
    entry.createdAtMs = nowMs();
    m_cache[toolName][generateCacheKey(parameters)] = std::move(entry);
}

void ToolRegistry::logExecutionStart(const std::string& toolName, const json&, const std::string& execId, bool detailed) {
    if (!m_logger) return;
    if (detailed) m_logger->debug("ToolRegistry: Starting tool: " + toolName + " [" + execId + "]");
}

void ToolRegistry::logExecutionCompletion(const ToolResult& result, const std::string& toolName, bool detailed) {
    if (!m_logger) return;
    std::string status = result.success ? "success" : "failed";
    if (detailed) m_logger->debug("ToolRegistry: Tool " + toolName + ": " + status);
    else          m_logger->info("ToolRegistry: Tool " + toolName + ": " + status);
}

ToolResult ToolRegistry::executeToolInternal(
    const std::string& toolName,
    const json& parameters,
    const ToolExecutionConfig& config,
    const std::string& traceId,
    const std::string& parentSpanId)
{
    ToolResult result;
    result.executionContext.executionId = generateExecutionId();
    result.executionContext.spanId      = generateExecutionId();
    result.executionContext.traceId     = traceId;
    result.executionContext.parentSpanId = parentSpanId;
    result.executionContext.status      = ToolExecutionStatus::Running;
    result.metrics.startTimeMs          = nowMs();

    if (!config.enableExecution) {
        result.executionContext.status = ToolExecutionStatus::SkippedByToggle;
        result.metrics.endTimeMs = nowMs();
        result.metrics.executionTimeMs = result.metrics.endTimeMs - result.metrics.startTimeMs;
        return result;
    }

    // Validate
    if (config.validateInputs) {
        std::string errMsg;
        if (!validateToolInput(toolName, parameters, errMsg)) {
            result.success = false;
            result.error = errMsg;
            result.executionContext.status = ToolExecutionStatus::Failed;
            result.metrics.endTimeMs = nowMs();
            result.metrics.executionTimeMs = result.metrics.endTimeMs - result.metrics.startTimeMs;
            if (m_metrics) m_metrics->incrementCounter("tool_executions_failed");
            return result;
        }
    }

    // Check cache
    if (config.enableCaching) {
        auto cached = getCachedResult(toolName, parameters);
        if (cached.has_value()) {
            cached->executionContext.executionId = result.executionContext.executionId;
            cached->executionContext.traceId = traceId;
            cached->executionContext.parentSpanId = parentSpanId;
            cached->executionContext.spanId = result.executionContext.spanId;
            if (m_metrics) {
                m_metrics->incrementCounter("tool_executions_total");
                m_metrics->incrementCounter("tool_executions_successful");
            }
            std::lock_guard<std::mutex> sl(m_statsMutex);
            auto& stats = m_statistics[toolName];
            stats.totalExecutions++;
            stats.successfulExecutions++;
            stats.cacheHits++;
            return cached.value();
        }
    }

    // Log start
    logExecutionStart(toolName, parameters, result.executionContext.executionId, config.enableDetailedLogging);

    // Retrieve handler
    std::function<json(const json&)> handler;
    ToolInputValidation inputValidation;
    {
        std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
        auto it = m_tools.find(toolName);
        if (it == m_tools.end()) {
            result.success = false;
            result.error = "Tool not found: " + toolName;
            result.executionContext.status = ToolExecutionStatus::Failed;
            result.metrics.endTimeMs = nowMs();
            result.metrics.executionTimeMs = result.metrics.endTimeMs - result.metrics.startTimeMs;
            return result;
        }
        handler = it->second->handler;
        inputValidation = it->second->inputValidation;
    }

    if (!handler) {
        result.success = false;
        result.error = "Tool has no handler: " + toolName;
        result.executionContext.status = ToolExecutionStatus::Failed;
        result.metrics.endTimeMs = nowMs();
        result.metrics.executionTimeMs = result.metrics.endTimeMs - result.metrics.startTimeMs;
        return result;
    }

    // Execute with retry
    int attempt = 0;
    int maxAttempts = config.retryOnFailure ? config.maxRetries + 1 : 1;
    while (attempt < maxAttempts) {
        try {
            result.data = handler(parameters);
            result.success = true;
            result.executionContext.status = ToolExecutionStatus::Completed;
            break;
        } catch (const std::exception& ex) {
            attempt++;
            if (attempt < maxAttempts) {
                result.wasRetried = true;
                result.retryCount = attempt;
                if (config.retryDelayMs > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(config.retryDelayMs));
            } else {
                result.success = false;
                result.error = ex.what();
                result.executionContext.status = ToolExecutionStatus::Failed;
                result.retryCount = attempt - 1;
            }
        }
    }

    result.metrics.endTimeMs = nowMs();
    result.metrics.executionTimeMs = result.metrics.endTimeMs - result.metrics.startTimeMs;

    // Cache on success
    if (result.success && config.enableCaching) {
        cacheResult(toolName, parameters, result);
    }

    // Update stats
    {
        std::lock_guard<std::mutex> sl(m_statsMutex);
        auto& stats = m_statistics[toolName];
        stats.totalExecutions++;
        if (result.success) stats.successfulExecutions++;
        else                stats.failedExecutions++;
        stats.totalLatencyMs += result.metrics.executionTimeMs;
        if (!result.success) stats.lastError = result.error;
        stats.retryCount += result.retryCount;
        stats.lastExecutionTimeMs = result.metrics.endTimeMs;
    }

    // Update metrics
    if (m_metrics) {
        m_metrics->incrementCounter("tool_executions_total");
        if (result.success) m_metrics->incrementCounter("tool_executions_successful");
        else                m_metrics->incrementCounter("tool_executions_failed");
    }

    logExecutionCompletion(result, toolName, config.enableDetailedLogging);
    return result;
}

ToolResult ToolRegistry::executeTool(const std::string& toolName, const json& parameters) {
    ToolExecutionConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
        auto it = m_tools.find(toolName);
        if (it != m_tools.end()) cfg = it->second->config;
    }
    cfg.timeoutMs = (cfg.timeoutMs > 0) ? cfg.timeoutMs : m_globalTimeout;
    return executeToolInternal(toolName, parameters, cfg, "", "");
}

ToolResult ToolRegistry::executeToolWithConfig(
    const std::string& toolName, const json& parameters, const ToolExecutionConfig& config) {
    return executeToolInternal(toolName, parameters, config, "", "");
}

ToolResult ToolRegistry::executeToolWithTrace(
    const std::string& toolName,
    const json& parameters,
    const std::string& traceId,
    const std::string& parentSpanId)
{
    ToolExecutionConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
        auto it = m_tools.find(toolName);
        if (it != m_tools.end()) cfg = it->second->config;
    }
    return executeToolInternal(toolName, parameters, cfg, traceId, parentSpanId);
}

std::vector<ToolResult> ToolRegistry::executeBatch(
    const std::vector<std::pair<std::string, json>>& toolExecution,
    bool stopOnError)
{
    std::vector<ToolResult> results;
    results.reserve(toolExecution.size());
    for (const auto& [name, params] : toolExecution) {
        auto r = executeTool(name, params);
        results.push_back(std::move(r));
        if (stopOnError && !results.back().success) break;
    }
    return results;
}

// ---------------------------------------------------------------------------
// Statistics & Monitoring
// ---------------------------------------------------------------------------
json ToolRegistry::getToolStatistics(const std::string& toolName) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    json out = json::object();
    out["tool_name"] = toolName;
    auto it = m_statistics.find(toolName);
    if (it == m_statistics.end()) {
        out["total_executions"] = 0;
        out["successful_executions"] = 0;
        out["failed_executions"] = 0;
        out["success_rate"] = 0.0;
        out["average_latency_ms"] = 0.0;
        out["retry_count"] = 0;
        out["cache_hits"] = 0;
        return out;
    }
    const auto& s = it->second;
    out["total_executions"]      = static_cast<int>(s.totalExecutions);
    out["successful_executions"] = static_cast<int>(s.successfulExecutions);
    out["failed_executions"]     = static_cast<int>(s.failedExecutions);
    double rate = (s.totalExecutions > 0)
        ? static_cast<double>(s.successfulExecutions) / static_cast<double>(s.totalExecutions)
        : 0.0;
    out["success_rate"] = rate;
    double avgLatency = (s.totalExecutions > 0)
        ? static_cast<double>(s.totalLatencyMs) / static_cast<double>(s.totalExecutions)
        : 0.0;
    out["average_latency_ms"] = avgLatency;
    out["retry_count"]  = static_cast<int>(s.retryCount);
    out["cache_hits"]   = static_cast<int>(s.cacheHits);
    out["last_error"]   = s.lastError;
    return out;
}

json ToolRegistry::getMetricsSnapshot() const {
    json out = json::object();
    {
        std::lock_guard<std::mutex> sl(m_statsMutex);
        uint64_t total = 0, success = 0, failed = 0;
        json toolsJson = json::object();
        for (const auto& [name, s] : m_statistics) {
            total   += s.totalExecutions;
            success += s.successfulExecutions;
            failed  += s.failedExecutions;
            json tj = json::object();
            tj["total"]   = static_cast<int>(s.totalExecutions);
            tj["success"] = static_cast<int>(s.successfulExecutions);
            tj["failed"]  = static_cast<int>(s.failedExecutions);
            toolsJson[name] = std::move(tj);
        }
        out["total_executions"]      = static_cast<int>(total);
        out["successful_executions"] = static_cast<int>(success);
        out["failed_executions"]     = static_cast<int>(failed);
        out["tools"] = std::move(toolsJson);
    }
    return out;
}

json ToolRegistry::getHealthStatus() const {
    std::lock_guard<std::mutex> lock(m_toolRegistryMutex);
    json out = json::object();
    out["tool_count"] = static_cast<int>(m_tools.size());
    out["status"] = "HEALTHY";
    return out;
}

void ToolRegistry::clearExecutionHistory(const std::string& toolName) {
    std::lock_guard<std::mutex> lock(m_executionMutex);
    if (toolName.empty()) {
        m_executionHistory.clear();
    } else {
        m_executionHistory.erase(toolName);
    }
}

json ToolRegistry::runToolSelfTest(const std::string& toolName) {
    json out = json::object();
    out["tool"] = toolName;
    out["available"] = hasTool(toolName);
    return out;
}
