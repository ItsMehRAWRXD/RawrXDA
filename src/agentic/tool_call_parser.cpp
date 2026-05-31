#include "agentic/tool_call_parser.h"

#include <algorithm>
#include <cctype>

namespace RawrXD::Agentic
{

std::size_t ToolCallParser::findMatchingBrace_(const std::string& text, const std::size_t openBrace)
{
    if (openBrace >= text.size() || text[openBrace] != '{')
        return std::string::npos;

    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (std::size_t i = openBrace; i < text.size(); ++i)
    {
        const char c = text[i];

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (inString && c == '\\')
        {
            escaped = true;
            continue;
        }

        if (c == '"')
        {
            inString = !inString;
            continue;
        }

        if (inString)
            continue;

        if (c == '{')
        {
            ++depth;
        }
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
                return i;
            if (depth < 0)
                return std::string::npos;
        }
    }

    return std::string::npos;
}

bool ToolCallParser::extractBalancedJsonRange(const std::string& text, std::size_t searchStart, std::size_t& outStart,
                                              std::size_t& outEnd)
{
    if (searchStart >= text.size())
        return false;

    const std::size_t open = text.find('{', searchStart);
    if (open == std::string::npos)
        return false;

    const std::size_t close = findMatchingBrace_(text, open);
    if (close == std::string::npos || close < open)
        return false;

    outStart = open;
    outEnd = close + 1;
    return true;
}

nlohmann::json ToolCallParser::parseArgumentsValue_(const nlohmann::json& v)
{
    if (v.is_object() || v.is_array() || v.is_null() || v.is_boolean() || v.is_number())
        return v;

    if (v.is_string())
    {
        const std::string s = v.get<std::string>();
        auto parsed = nlohmann::json::parse(s, nullptr, false);
        if (!parsed.is_discarded())
            return parsed;
        return nlohmann::json::object();  // best-effort
    }

    return nlohmann::json::object();
}

bool ToolCallParser::parseOneJsonObject_(const nlohmann::json& parsed, std::vector<ParsedToolCall>& outCalls)
{
    auto appendInline = [&](const nlohmann::json& obj) -> bool
    {
        if (!obj.is_object())
            return false;

        ParsedToolCall c;

        // Name
        if (obj.contains("name") && obj["name"].is_string())
        {
            c.name = obj["name"].get<std::string>();
        }
        else if (obj.contains("function") && obj["function"].is_object() && obj["function"].contains("name") &&
                 obj["function"]["name"].is_string())
        {
            c.name = obj["function"]["name"].get<std::string>();
        }

        if (c.name.empty())
            return false;

        // Arguments
        if (obj.contains("arguments"))
        {
            c.arguments = parseArgumentsValue_(obj["arguments"]);
        }
        else if (obj.contains("function") && obj["function"].is_object() && obj["function"].contains("arguments"))
        {
            c.arguments = parseArgumentsValue_(obj["function"]["arguments"]);
        }
        else
        {
            c.arguments = nlohmann::json::object();
        }

        // ID (optional)
        if (obj.contains("id") && obj["id"].is_string())
        {
            c.id = obj["id"].get<std::string>();
            c.hasId = true;
        }

        outCalls.push_back(std::move(c));
        return true;
    };

    bool any = false;

    if (parsed.is_object() && parsed.contains("tool_calls"))
    {
        const auto& tc = parsed["tool_calls"];
        if (!tc.is_array())
            return false;
        for (const auto& el : tc)
            any = appendInline(el) || any;
        return any;
    }

    if (parsed.is_array())
    {
        for (const auto& el : parsed)
            any = appendInline(el) || any;
        return any;
    }

    // Inline format
    any = appendInline(parsed) || any;
    return any;
}

ToolCallParseResult ToolCallParser::parse(const std::string& modelOutput)
{
    ToolCallParseResult result;
    if (modelOutput.empty())
        return result;

    // Strategy:
    // - Scan for `"tool_calls"` markers; for each, find the nearest preceding `{` and extract balanced JSON.
    // - If none found, scan for balanced JSON objects and accept inline `{name,arguments}` shapes.
    // - Never throw; collect best-effort calls.

    std::vector<std::pair<std::size_t, std::size_t>> ranges;

    std::size_t pos = 0;
    while (pos < modelOutput.size())
    {
        const std::size_t m = modelOutput.find("\"tool_calls\"", pos);
        if (m == std::string::npos)
            break;

        const std::size_t open = modelOutput.rfind('{', m);
        if (open != std::string::npos)
        {
            const std::size_t close = findMatchingBrace_(modelOutput, open);
            if (close != std::string::npos)
                ranges.emplace_back(open, close + 1);
        }

        pos = m + 1;
    }

    if (ranges.empty())
    {
        // Fallback: walk all balanced JSON objects and pick inline call objects.
        std::size_t s = 0;
        while (true)
        {
            std::size_t a = 0, b = 0;
            if (!extractBalancedJsonRange(modelOutput, s, a, b))
                break;
            ranges.emplace_back(a, b);
            s = b;
        }
    }

    // Deduplicate ranges (sort + unique).
    std::sort(ranges.begin(), ranges.end());
    ranges.erase(std::unique(ranges.begin(), ranges.end()), ranges.end());

    std::size_t lastAcceptedEnd = std::string::npos;
    for (const auto& [a, b] : ranges)
    {
        if (b <= a || b > modelOutput.size())
            continue;

        const std::string jsonStr = modelOutput.substr(a, b - a);
        auto parsed = nlohmann::json::parse(jsonStr, nullptr, false);
        if (parsed.is_discarded())
            continue;

        const std::size_t before = result.toolCalls.size();
        if (parseOneJsonObject_(parsed, result.toolCalls))
        {
            lastAcceptedEnd = std::max(lastAcceptedEnd == std::string::npos ? 0u : lastAcceptedEnd, b);
        }
        else
        {
            // If it was a random JSON object, roll back any accidental appends.
            if (result.toolCalls.size() != before)
                result.toolCalls.resize(before);
        }
    }

    result.hasToolCalls = !result.toolCalls.empty();

    if (result.hasToolCalls && lastAcceptedEnd != std::string::npos && lastAcceptedEnd < modelOutput.size())
    {
        std::string tail = modelOutput.substr(lastAcceptedEnd);
        auto first = tail.find_first_not_of(" \t\r\n");
        if (first != std::string::npos)
            tail = tail.substr(first);
        else
            tail.clear();
        result.remainingText = std::move(tail);
    }

    return result;
}

}  // namespace RawrXD::Agentic
