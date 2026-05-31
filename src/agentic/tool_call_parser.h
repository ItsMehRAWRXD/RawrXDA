// ============================================================================
// tool_call_parser.h — Agentic Tool Call Parser
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agentic {

struct ParsedToolCall {
    std::string      name;
    nlohmann::json   arguments;
    std::string      id;
    bool             hasId = false;
};

struct ToolCallParseResult {
    std::vector<ParsedToolCall> toolCalls;
    bool                        hadPartialFailure = false;
};

class ToolCallParser {
public:
    static ToolCallParseResult parse(const std::string& modelOutput);

private:
    static std::size_t findMatchingBrace_(const std::string& text, std::size_t openBrace);
    static bool        extractBalancedJsonRange(const std::string& text, std::size_t searchStart,
                                                std::size_t& outStart, std::size_t& outEnd);
    static nlohmann::json parseArgumentsValue_(const nlohmann::json& v);
    static bool           parseOneJsonObject_(const nlohmann::json& parsed,
                                              std::vector<ParsedToolCall>& outCalls);
};

} // namespace Agentic
} // namespace RawrXD
