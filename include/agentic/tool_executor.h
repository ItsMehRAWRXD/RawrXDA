#pragma once

#include "agentic/tool_call_parser.h"
#include "agentic/tool_registry.h"

#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace RawrXD::Agentic
{

struct ValidationResult
{
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct ExecutionResult
{
    ToolResult toolResult;
    std::string toolName;
    std::chrono::milliseconds durationMs{0};
    bool timedOut = false;
    bool confirmed = false;
    ValidationResult validation;
};

enum class ExecutionPolicy
{
    Sequential,
    Parallel,
    Auto
};

struct ExecutorConfig
{
    bool enableValidation = true;
    bool enableTimeouts = true;
    bool enableConfirmation = true;
    ExecutionPolicy defaultPolicy = ExecutionPolicy::Auto;
    std::uint32_t defaultTimeoutMs = 30000;
    std::size_t maxParallelTools = 8;
};

using ConfirmationCallback =
    std::function<bool(const std::string& toolName, const nlohmann::json& params, const std::string& description)>;
using ProgressCallback = std::function<void(const std::string& toolName, const std::string& status)>;

class ToolExecutor
{
  public:
    explicit ToolExecutor(ToolRegistry& registry, ExecutorConfig config = {});

    void setConfirmationCallback(ConfirmationCallback cb) { m_confirmationCallback = std::move(cb); }
    void setProgressCallback(ProgressCallback cb) { m_progressCallback = std::move(cb); }

    [[nodiscard]] bool hasTool(const std::string& name) const { return m_registry.hasTool(name); }
    [[nodiscard]] bool getToolDefinition(const std::string& name, ToolDefinition& outDef) const
    {
        return m_registry.getToolDefinition(name, outDef);
    }

    [[nodiscard]] ValidationResult validate(const std::string& toolName, const nlohmann::json& params);

    [[nodiscard]] ExecutionResult execute(const std::string& toolName, const nlohmann::json& params);

    [[nodiscard]] std::vector<ExecutionResult> executeToolCalls(const std::vector<ParsedToolCall>& toolCalls,
                                                                ExecutionPolicy policy = ExecutionPolicy::Auto);

    [[nodiscard]] std::vector<ExecutionResult> executeBatch(
        const std::vector<std::pair<std::string, nlohmann::json>>& tools,
        ExecutionPolicy policy = ExecutionPolicy::Auto);

  private:
    [[nodiscard]] ValidationResult validateAgainstSchema_(const nlohmann::json& params, const nlohmann::json& schema);
    [[nodiscard]] bool requestConfirmation_(const ToolDefinition& def, const nlohmann::json& params);
    [[nodiscard]] ExecutionResult executeWithTimeout_(const ToolDefinition& def, const nlohmann::json& params);

    ToolRegistry& m_registry;
    ExecutorConfig m_config;
    ConfirmationCallback m_confirmationCallback;
    ProgressCallback m_progressCallback;
};

}  // namespace RawrXD::Agentic
