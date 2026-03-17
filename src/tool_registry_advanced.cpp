#include "tool_registry.hpp"

#include <chrono>

namespace RawrXD {

namespace {
static std::map<std::string, std::string> jsonToStringMap(const json& params) {
    std::map<std::string, std::string> out;
    if (!params.is_object()) {
        return out;
    }
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (it.value().is_string()) {
            out[it.key()] = it.value().get<std::string>();
        } else {
            out[it.key()] = it.value().dump();
        }
    }
    return out;
}

static json mapToJson(const std::map<std::string, std::string>& args) {
    json obj = json::object();
    for (const auto& [k, v] : args) {
        obj[k] = v;
    }
    return obj;
}
}  // namespace

ToolRegistry::ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics)
    : m_logger(std::move(logger)), m_metrics(std::move(metrics)), m_globalTimeout(30000) {}

ToolRegistry::~ToolRegistry() = default;

bool ToolRegistry::registerTool(const ToolDefinition& toolDef) {
    if (toolDef.metadata.name.empty()) {
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    RegisteredTool entry;
    entry.metadata = toolDef.metadata;
    entry.config = toolDef.config;
    entry.handler = toolDef.handler;
    entry.executor = toolDef.executor;
    entry.inputValidation = toolDef.inputValidation;
    tools_[toolDef.metadata.name] = std::move(entry);
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    return tools_.erase(name) > 0;
}

bool ToolRegistry::hasTool(const std::string& toolName) const {
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    return tools_.find(toolName) != tools_.end();
}

ToolResult ToolRegistry::executeTool(const std::string& name, const json& params) {
    return executeToolWithTrace(name, params, "");
}

ToolResult ToolRegistry::executeToolWithConfig(
    const std::string& name, const json& params, const ToolConfig& config) {
    (void)config;
    return executeTool(name, params);
}

ToolResult ToolRegistry::executeToolWithTrace(
    const std::string& name, const json& params, const std::string& traceId) {
    ToolContext ctx;
    ctx.traceId = traceId;
    return executeInternal(name, jsonToStringMap(params), ctx);
}

ToolResult ToolRegistry::executeTool(
    const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
    return executeInternal(name, args, ctx);
}

ToolResult ToolRegistry::executeInternal(
    const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
    (void)ctx;
    ToolResult result;
    result.executionContext.status = ToolExecutionStatus::Running;
    const auto start = std::chrono::steady_clock::now();

    RegisteredTool tool;
    {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it == tools_.end()) {
            result.success = false;
            result.error = "Tool not found: " + name;
            result.statusCode = 404;
            result.executionContext.status = ToolExecutionStatus::Failed;
            return result;
        }
        tool = it->second;
    }

    try {
        if (tool.executor) {
            result = tool.executor(args, ctx);
        } else if (tool.handler) {
            const json out = tool.handler(mapToJson(args));
            result.success = !out.contains("error");
            result.data = out;
            result.metadata = out;
            result.output = out.is_string() ? out.get<std::string>() : out.dump();
            result.error = out.contains("error") ? out["error"].get<std::string>() : "";
            result.statusCode = result.success ? 0 : 1;
        } else {
            result.success = false;
            result.error = "Tool has no executable handler: " + name;
            result.statusCode = 500;
        }
    } catch (const std::exception& ex) {
        result.success = false;
        result.error = ex.what();
        result.statusCode = 500;
    } catch (...) {
        result.success = false;
        result.error = "Unknown tool execution error";
        result.statusCode = 500;
    }

    const auto end = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    result.executionContext.status =
        result.success ? ToolExecutionStatus::Completed : ToolExecutionStatus::Failed;
    updateStats(name, result);
    return result;
}

std::future<ToolResult> ToolRegistry::executeToolAsync(
    const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
    return std::async(std::launch::async, [this, name, args, ctx]() {
        return executeInternal(name, args, ctx);
    });
}

std::vector<ToolMetadata> ToolRegistry::getAllToolMetadata() const {
    std::vector<ToolMetadata> out;
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    out.reserve(tools_.size());
    for (const auto& [_, tool] : tools_) {
        out.push_back(tool.metadata);
    }
    return out;
}

std::vector<ToolMetadata> ToolRegistry::getToolsByCategory(const std::string& category) const {
    std::vector<ToolMetadata> out;
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    for (const auto& [_, tool] : tools_) {
        if (tool.metadata.category == category) {
            out.push_back(tool.metadata);
        }
    }
    return out;
}

std::optional<ToolMetadata> ToolRegistry::getToolMetadata(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return std::nullopt;
    }
    return it->second.metadata;
}

void ToolRegistry::updateStats(const std::string& name, const ToolResult& result) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    auto& stats = m_statistics[name];
    stats.totalExecutions++;
    if (result.success) {
        stats.successCount++;
    } else {
        stats.failureCount++;
        stats.lastError = result.error;
    }
    stats.totalDurationMs += result.durationMs;
    if (stats.totalExecutions > 0) {
        stats.averageDurationMs = stats.totalDurationMs / static_cast<double>(stats.totalExecutions);
    }
    stats.lastExecutionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
}

ToolStats ToolRegistry::getToolStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    auto it = m_statistics.find(name);
    if (it == m_statistics.end()) {
        return {};
    }
    return it->second;
}

std::map<std::string, ToolStats> ToolRegistry::getAllStats() const {
    std::map<std::string, ToolStats> out;
    std::lock_guard<std::mutex> lock(m_statsMutex);
    for (const auto& [k, v] : m_statistics) {
        out[k] = v;
    }
    return out;
}

std::vector<ToolDefinition> ToolRegistry::getAllTools() const {
    std::vector<ToolDefinition> out;
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    out.reserve(tools_.size());
    for (const auto& [_, t] : tools_) {
        ToolDefinition def;
        def.metadata = t.metadata;
        def.config = t.config;
        def.handler = t.handler;
        def.executor = t.executor;
        def.inputValidation = t.inputValidation;
        out.push_back(std::move(def));
    }
    return out;
}

std::shared_ptr<const ToolDefinition> ToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }
    auto def = std::make_shared<ToolDefinition>();
    def->metadata = it->second.metadata;
    def->config = it->second.config;
    def->handler = it->second.handler;
    def->executor = it->second.executor;
    def->inputValidation = it->second.inputValidation;
    return def;
}

std::vector<std::string> ToolRegistry::getRegisteredTools() const {
    std::vector<std::string> out;
    std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
    out.reserve(tools_.size());
    for (const auto& [name, _] : tools_) {
        out.push_back(name);
    }
    return out;
}

}  // namespace RawrXD
