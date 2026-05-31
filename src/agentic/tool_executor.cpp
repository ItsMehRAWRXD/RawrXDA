#include "agentic/tool_executor.h"

#include <algorithm>
#include <future>

namespace RawrXD::Agentic
{

ToolExecutor::ToolExecutor(ToolRegistry& registry, ExecutorConfig config) : m_registry(registry), m_config(config) {}

ValidationResult ToolExecutor::validate(const std::string& toolName, const nlohmann::json& params)
{
    ValidationResult r;

    ToolDefinition def{};
    if (!m_registry.getToolDefinition(toolName, def))
    {
        r.errors.push_back("Tool not found: " + toolName);
        return r;
    }

    if (!m_config.enableValidation)
    {
        r.valid = true;
        return r;
    }

    if (def.parameterSchema.is_null() || def.parameterSchema.empty())
    {
        r.valid = true;
        return r;
    }

    r = validateAgainstSchema_(params, def.parameterSchema);
    return r;
}

ValidationResult ToolExecutor::validateAgainstSchema_(const nlohmann::json& params, const nlohmann::json& schema)
{
    ValidationResult r;

    if (!params.is_object())
    {
        r.errors.push_back("Parameters must be a JSON object");
        return r;
    }

    // Required fields.
    if (schema.contains("required") && schema["required"].is_array())
    {
        for (const auto& req : schema["required"])
        {
            if (!req.is_string())
                continue;
            const std::string k = req.get<std::string>();
            if (!params.contains(k))
                r.errors.push_back("Missing required field: " + k);
        }
    }

    // Type checks for properties (lightweight JSON-schema subset).
    if (schema.contains("properties") && schema["properties"].is_object())
    {
        const auto& props = schema["properties"];
        for (auto it = props.begin(); it != props.end(); ++it)
        {
            const std::string key = it.key();
            const nlohmann::json& prop = it.value();
            if (!params.contains(key))
                continue;

            const nlohmann::json& v = params[key];
            if (prop.contains("type") && prop["type"].is_string())
            {
                const std::string t = prop["type"].get<std::string>();
                bool ok = true;
                if (t == "string")
                    ok = v.is_string();
                else if (t == "number")
                    ok = v.is_number();
                else if (t == "integer")
                    ok = v.is_number_integer();
                else if (t == "boolean")
                    ok = v.is_boolean();
                else if (t == "array")
                    ok = v.is_array();
                else if (t == "object")
                    ok = v.is_object();
                else if (t == "null")
                    ok = v.is_null();
                if (!ok)
                    r.errors.push_back("Field '" + key + "' has wrong type (expected " + t + ")");
            }
        }
    }

    r.valid = r.errors.empty();
    return r;
}

bool ToolExecutor::requestConfirmation_(const ToolDefinition& def, const nlohmann::json& params)
{
    if (!m_config.enableConfirmation || !def.requiresConfirmation)
        return true;
    if (!m_confirmationCallback)
        return false;  // safe default
    return m_confirmationCallback(def.name, params, def.description);
}

ExecutionResult ToolExecutor::executeWithTimeout_(const ToolDefinition& def, const nlohmann::json& params)
{
    ExecutionResult r;
    r.toolName = def.name;
    r.confirmed = true;
    r.validation.valid = true;

    const std::uint32_t timeoutMs =
        (m_config.enableTimeouts
             ? static_cast<std::uint32_t>(def.timeoutMs > 0 ? def.timeoutMs : (int)m_config.defaultTimeoutMs)
             : 0u);

    const auto t0 = std::chrono::steady_clock::now();
    if (m_progressCallback)
        m_progressCallback(def.name, "starting");

    if (timeoutMs == 0u)
    {
        r.toolResult = m_registry.invoke(def.name, params);
    }
    else
    {
        // Note: std::future timeout cannot cancel execution; it only returns control to caller.
        auto fut = std::async(std::launch::async, [&]() { return m_registry.invoke(def.name, params); });
        const auto st = fut.wait_for(std::chrono::milliseconds(timeoutMs));
        if (st == std::future_status::timeout)
        {
            r.timedOut = true;
            r.toolResult.success = false;
            r.toolResult.errorMessage = "Tool timed out after " + std::to_string(timeoutMs) + "ms";
        }
        else
        {
            r.toolResult = fut.get();
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    if (m_progressCallback)
        m_progressCallback(def.name, r.toolResult.success ? "completed" : "failed");

    return r;
}

ExecutionResult ToolExecutor::execute(const std::string& toolName, const nlohmann::json& params)
{
    ExecutionResult r;
    r.toolName = toolName;

    r.validation = validate(toolName, params);
    if (!r.validation.valid)
    {
        r.toolResult.success = false;
        r.toolResult.errorMessage =
            r.validation.errors.empty() ? "Validation failed" : ("Validation failed: " + r.validation.errors.front());
        return r;
    }

    ToolDefinition def{};
    if (!m_registry.getToolDefinition(toolName, def))
    {
        r.toolResult.success = false;
        r.toolResult.errorMessage = "Tool not found: " + toolName;
        return r;
    }

    if (!requestConfirmation_(def, params))
    {
        r.toolResult.success = false;
        r.toolResult.errorMessage = "Tool execution not confirmed";
        r.confirmed = false;
        return r;
    }

    r = executeWithTimeout_(def, params);
    r.validation = validate(toolName, params);
    return r;
}

std::vector<ExecutionResult> ToolExecutor::executeBatch(
    const std::vector<std::pair<std::string, nlohmann::json>>& tools, ExecutionPolicy policy)
{
    std::vector<ExecutionResult> results;
    results.reserve(tools.size());
    if (tools.empty())
        return results;

    ExecutionPolicy actual = policy;
    if (actual == ExecutionPolicy::Auto)
        actual = m_config.defaultPolicy;
    if (actual == ExecutionPolicy::Auto)
        actual = ExecutionPolicy::Sequential;

    if (actual == ExecutionPolicy::Sequential)
    {
        for (const auto& [name, params] : tools)
            results.push_back(execute(name, params));
        return results;
    }

    // Parallel (bounded fanout)
    std::vector<std::future<ExecutionResult>> futs;
    futs.reserve(std::min(tools.size(), m_config.maxParallelTools));

    auto drain = [&]()
    {
        for (auto& f : futs)
            results.push_back(f.get());
        futs.clear();
    };

    for (const auto& [name, params] : tools)
    {
        futs.push_back(std::async(std::launch::async, [&]() { return execute(name, params); }));
        if (futs.size() >= m_config.maxParallelTools)
            drain();
    }
    drain();
    return results;
}

std::vector<ExecutionResult> ToolExecutor::executeToolCalls(const std::vector<ParsedToolCall>& toolCalls,
                                                            ExecutionPolicy policy)
{
    std::vector<std::pair<std::string, nlohmann::json>> batch;
    batch.reserve(toolCalls.size());
    for (const auto& tc : toolCalls)
        batch.emplace_back(tc.name, tc.arguments);
    return executeBatch(batch, policy);
}

}  // namespace RawrXD::Agentic
