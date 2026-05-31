#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace RawrXD::Agentic
{

struct ParsedToolCall
{
    std::string name;
    nlohmann::json arguments = nlohmann::json::object();
    std::string id;
    bool hasId = false;
};

struct ToolCallParseResult
{
    std::vector<ParsedToolCall> toolCalls;
    bool hasToolCalls = false;
    std::string parseError;     // informational; best-effort parser never throws
    std::string remainingText;  // any trailing non-JSON assistant text (best effort)
};

/// Robust extraction of tool-call JSON from model output.
/// Supports:
/// - `{"tool_calls":[{"name":"...","arguments":{...}}]}`
/// - OpenAI-ish: `{"tool_calls":[{"id":"...","function":{"name":"...","arguments":"{...}"}}]}`
/// - Inline single call: `{"name":"...","arguments":{...}}`
/// - Trailing text before/after JSON.
class ToolCallParser
{
  public:
    [[nodiscard]] static ToolCallParseResult parse(const std::string& modelOutput);

    /// Extract a balanced `{...}` JSON object range [start,end) at or after \p searchStart.
    [[nodiscard]] static bool extractBalancedJsonRange(const std::string& text, std::size_t searchStart,
                                                       std::size_t& outStart, std::size_t& outEnd);

  private:
    [[nodiscard]] static std::size_t findMatchingBrace_(const std::string& text, std::size_t openBrace);
    [[nodiscard]] static bool parseOneJsonObject_(const nlohmann::json& parsed, std::vector<ParsedToolCall>& outCalls);
    [[nodiscard]] static nlohmann::json parseArgumentsValue_(const nlohmann::json& v);
};

}  // namespace RawrXD::Agentic
