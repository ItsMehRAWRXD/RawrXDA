// ============================================================================
// tool_call_parser.hpp — Tool-Call Ingestion Hardening for bulk_fix
// ============================================================================
//
// Action Item #16: Strict parser with clear error objects, max targets per
// call, per-target path validation.
//
// Exit criteria: Malformed tool calls never crash the orchestrator;
// they produce a structured "invalid tool call" result.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_TOOL_CALL_PARSER_H
#define RAWRXD_TOOL_CALL_PARSER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// ============================================================================
// Configuration
// ============================================================================
static constexpr int    TOOL_MAX_TARGETS_PER_CALL   = 100;
static constexpr int    TOOL_MAX_TARGET_PATH_LENGTH = 4096;
static constexpr int    TOOL_MAX_STRATEGY_LENGTH    = 64;
static constexpr int    TOOL_MAX_JSON_SIZE          = 1024 * 1024; // 1MB
static constexpr int    TOOL_MAX_CONCURRENCY        = 16;

// ============================================================================
// Structs — Parsed Tool Call
// ============================================================================

struct ToolCallTarget {
    std::string path;           // Validated file path
    bool        isGlob;         // Contains * or ? glob characters
    bool        valid;          // Passed validation
    std::string error;          // Validation error detail (empty if valid)
};

struct ToolCallParseResult {
    bool                        success;
    std::string                 strategy;       // e.g., "compile", "format", "stubs"
    std::vector<ToolCallTarget> targets;
    int                         maxConcurrency; // Validated concurrency
    bool                        autoVerify;
    std::string                 parentAgentId;

    // Error info (populated on failure)
    std::string                 errorType;      // "parse", "validation", "overflow"
    std::string                 errorDetail;
    int                         invalidTargetCount;

    static ToolCallParseResult ok(const std::string& strategy,
                                   const std::vector<ToolCallTarget>& targets) {
        ToolCallParseResult r{};
        r.success = true;
        r.strategy = strategy;
        r.targets = targets;
        r.maxConcurrency = 4;
        r.autoVerify = true;
        r.invalidTargetCount = 0;
        return r;
    }

    static ToolCallParseResult error(const std::string& type, const std::string& detail) {
        ToolCallParseResult r{};
        r.success = false;
        r.errorType = type;
        r.errorDetail = detail;
        r.invalidTargetCount = 0;
        return r;
    }
};

// ============================================================================
// Tool Call Strategy Validation
// ============================================================================
namespace ToolCallStrategies {
    static const char* kValidStrategies[] = {
        "compile", "compile_error_fix",
        "format", "format_fix",
        "refactor",
        "stubs", "stub_implementation",
        "headers", "header_fix",
        "lint", "lint_fix",
        "tests", "test_generation",
        "docs", "documentation",
        "security", "security_audit_fix",
        "include_fix",
        "const_fix",
        nullptr
    };

    inline bool isValidStrategy(const std::string& s) {
        for (int i = 0; kValidStrategies[i]; ++i) {
            if (s == kValidStrategies[i]) return true;
        }
        return false;
    }
}

// ============================================================================
// Path Validation
// ============================================================================
namespace ToolCallPathValidator {

    /// Validate a single file path for safety.
    /// Blocks: path traversal, network paths, null bytes, excessive length.
    inline ToolCallTarget validatePath(const std::string& rawPath) {
        ToolCallTarget t;
        t.path = rawPath;
        t.isGlob = false;
        t.valid = false;

        // Empty check
        if (rawPath.empty()) {
            t.error = "Empty path";
            return t;
        }

        // Length check
        if (rawPath.size() > static_cast<size_t>(TOOL_MAX_TARGET_PATH_LENGTH)) {
            t.error = "Path exceeds max length (" +
                      std::to_string(TOOL_MAX_TARGET_PATH_LENGTH) + ")";
            return t;
        }

        // Null byte check
        if (rawPath.find('\0') != std::string::npos) {
            t.error = "Path contains null byte";
            return t;
        }

        // Network path check (UNC paths)
        if (rawPath.size() >= 2 &&
            ((rawPath[0] == '\\' && rawPath[1] == '\\') ||
             (rawPath[0] == '/' && rawPath[1] == '/'))) {
            t.error = "Network/UNC paths not allowed";
            return t;
        }

        // Path traversal check
        if (rawPath.find("..") != std::string::npos) {
            t.error = "Path traversal (..) not allowed";
            return t;
        }

        // Protocol check (no URLs)
        std::string lower = rawPath;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.find("://") != std::string::npos ||
            lower.find("file:") == 0 ||
            lower.find("http:") == 0 ||
            lower.find("https:") == 0) {
            t.error = "URLs/protocols not allowed as paths";
            return t;
        }

        // Check for suspicious control characters
        for (char c : rawPath) {
            if (c < 0x20 && c != '\0') {
                t.error = "Path contains control character";
                return t;
            }
        }

        // Detect glob patterns
        t.isGlob = (rawPath.find('*') != std::string::npos ||
                     rawPath.find('?') != std::string::npos);

        t.valid = true;
        return t;
    }
}

// ============================================================================
// ToolCallParser — strict parser with clear error objects
// ============================================================================
class ToolCallParser {
public:
    /// Parse a raw JSON tool call for bulk_fix operations.
    /// Does NOT use external JSON library — manual parse for safety.
    /// Returns a structured result that never crashes the orchestrator.
    static ToolCallParseResult parse(const std::string& rawJson) {
        // Size check
        if (rawJson.empty()) {
            return ToolCallParseResult::error("parse", "Empty JSON input");
        }
        if (rawJson.size() > static_cast<size_t>(TOOL_MAX_JSON_SIZE)) {
            return ToolCallParseResult::error("overflow",
                "JSON exceeds max size (" + std::to_string(TOOL_MAX_JSON_SIZE) + " bytes)");
        }

        // Basic JSON structure validation — must be an object
        size_t firstBrace = rawJson.find('{');
        size_t lastBrace = rawJson.rfind('}');
        if (firstBrace == std::string::npos || lastBrace == std::string::npos ||
            lastBrace <= firstBrace) {
            return ToolCallParseResult::error("parse", "Invalid JSON: missing braces");
        }

        // Extract strategy
        std::string strategy;
        if (!extractString(rawJson, "strategy", strategy)) {
            return ToolCallParseResult::error("validation", "Missing 'strategy' field");
        }
        if (strategy.size() > static_cast<size_t>(TOOL_MAX_STRATEGY_LENGTH)) {
            return ToolCallParseResult::error("validation",
                "Strategy name exceeds max length");
        }
        if (!ToolCallStrategies::isValidStrategy(strategy)) {
            return ToolCallParseResult::error("validation",
                "Unknown strategy: '" + strategy + "'. Valid: compile, format, refactor, "
                "stubs, headers, lint, tests, docs, security");
        }

        // Extract targets array
        std::vector<std::string> rawTargets;
        if (!extractStringArray(rawJson, "targets", rawTargets)) {
            return ToolCallParseResult::error("validation", "Missing or invalid 'targets' array");
        }

        // Target count check
        if (rawTargets.empty()) {
            return ToolCallParseResult::error("validation", "Empty targets array");
        }
        if (static_cast<int>(rawTargets.size()) > TOOL_MAX_TARGETS_PER_CALL) {
            return ToolCallParseResult::error("overflow",
                "Too many targets (" + std::to_string(rawTargets.size()) +
                "), max is " + std::to_string(TOOL_MAX_TARGETS_PER_CALL));
        }

        // Validate each target path
        std::vector<ToolCallTarget> validatedTargets;
        int invalidCount = 0;
        for (const auto& raw : rawTargets) {
            ToolCallTarget t = ToolCallPathValidator::validatePath(raw);
            if (!t.valid) invalidCount++;
            validatedTargets.push_back(t);
        }

        // Extract optional fields
        int maxConcurrency = 4;
        extractInt(rawJson, "maxConcurrency", maxConcurrency);
        maxConcurrency = (std::max)(1, (std::min)(maxConcurrency, TOOL_MAX_CONCURRENCY));

        bool autoVerify = true;
        extractBool(rawJson, "autoVerify", autoVerify);

        std::string parentAgentId;
        extractString(rawJson, "parentAgentId", parentAgentId);

        // Build result
        auto result = ToolCallParseResult::ok(strategy, validatedTargets);
        result.maxConcurrency = maxConcurrency;
        result.autoVerify = autoVerify;
        result.parentAgentId = parentAgentId;
        result.invalidTargetCount = invalidCount;

        // If ALL targets are invalid, that's an error
        if (invalidCount == static_cast<int>(validatedTargets.size())) {
            return ToolCallParseResult::error("validation",
                "All " + std::to_string(invalidCount) + " targets failed validation");
        }

        return result;
    }

    /// Serialize a parse error as JSON for the caller
    static std::string serializeError(const ToolCallParseResult& r) {
        std::string json = "{";
        json += "\"success\":false,";
        json += "\"errorType\":\"" + escapeJson(r.errorType) + "\",";
        json += "\"errorDetail\":\"" + escapeJson(r.errorDetail) + "\",";
        json += "\"invalidTargets\":" + std::to_string(r.invalidTargetCount);
        json += "}";
        return json;
    }

private:
    // Minimal JSON extractors (no external dependency)
    static bool extractString(const std::string& json, const char* key, std::string& out) {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        // Skip whitespace and colon
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t'))
            pos++;
        if (pos >= json.size() || json[pos] != '"') return false;
        pos++; // skip opening quote
        auto end = json.find('"', pos);
        if (end == std::string::npos) return false;
        out = json.substr(pos, end - pos);
        return true;
    }

    static bool extractInt(const std::string& json, const char* key, int& out) {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t'))
            pos++;
        out = std::atoi(json.c_str() + pos);
        return true;
    }

    static bool extractBool(const std::string& json, const char* key, bool& out) {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t'))
            pos++;
        out = (json.substr(pos, 4) == "true");
        return true;
    }

    static bool extractStringArray(const std::string& json, const char* key,
                                    std::vector<std::string>& out) {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find('[', pos);
        if (pos == std::string::npos) return false;
        auto end = json.find(']', pos);
        if (end == std::string::npos) return false;

        std::string arr = json.substr(pos + 1, end - pos - 1);
        // Simple string extraction from comma-separated quoted values
        size_t i = 0;
        while (i < arr.size()) {
            auto qs = arr.find('"', i);
            if (qs == std::string::npos) break;
            auto qe = arr.find('"', qs + 1);
            if (qe == std::string::npos) break;
            out.push_back(arr.substr(qs + 1, qe - qs - 1));
            i = qe + 1;
        }
        return true;
    }

    static std::string escapeJson(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        return out;
    }
};

#endif // RAWRXD_TOOL_CALL_PARSER_H
