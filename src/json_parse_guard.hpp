#pragma once

#include "json_sanitizer.hpp"
#include "json_schema_validator.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <functional>
#include <memory>
#include <type_traits>

namespace RawrXD {
namespace JSON {

using json = nlohmann::json;

// Callback signatures for logging
using ParseErrorCallback = std::function<void(const std::string&)>;
using ParseSuccessCallback = std::function<void(const json&)>;

/**
 * @brief Safe JSON parsing guard with multi-layer defense
 * 
 * Pipeline:
 * 1. Normalize input and extract the first balanced JSON structure
 * 2. Parse extracted JSON with error capture
 * 3. Fall back to sanitization only if no valid structure is found
 * 4. Schema validation
 * 5. Self-healing trigger on failure
 * 
 * This is the CRITICAL interface between LLM output and tool dispatch.
 */
class JSONParseGuard {
public:
    static json ParseFirstValidStructure(
        const std::string& raw_input,
        std::string* error_out = nullptr)
    {
        std::string normalized = JSONSanitizer::TrimWhitespace(JSONSanitizer::RemoveBOM(raw_input));
        if (normalized.empty()) {
            if (error_out) {
                *error_out = "Input normalized to empty before extraction";
            }
            return json::object();
        }

        std::string last_error;
        size_t search_offset = 0;
        size_t start = std::string::npos;
        size_t end = std::string::npos;

        while (JSONSanitizer::FindNextJSONStructureBounds(normalized, search_offset, start, end)) {
            const std::string candidate = normalized.substr(start, end - start + 1);
            try {
                if (error_out) {
                    error_out->clear();
                }
                return json::parse(candidate);
            } catch (const std::exception& e) {
                last_error = std::string("JSON parse error while validating extracted structure: ") + e.what();
            }

            search_offset = start + 1;
        }

        std::string sanitized = JSONSanitizer::Sanitize(normalized);
        if (!sanitized.empty() && JSONSanitizer::LooksLikeJSON(sanitized)) {
            try {
                if (error_out) {
                    error_out->clear();
                }
                return json::parse(sanitized);
            } catch (const std::exception& e) {
                last_error = std::string("JSON parse error after sanitization fallback: ") + e.what();
            }
        }

        if (error_out) {
            if (last_error.empty()) {
                *error_out = "No parseable JSON structure found in input";
            } else {
                *error_out = "No parseable JSON structure found in input. Last error: " + last_error;
            }
        }

        return json::object();
    }

    /**
     * @brief Parse JSON with full hardening
     * 
    * @param raw_input Raw input from LLM (may contain markdown, fence attributes, etc.)
     * @param on_parse_error Callback if parsing fails (for logging/recovery)
     * @param on_parse_success Callback on successful parse
     * @param require_schema If true, will validate against schema
     * @param schema_validator Schema validation function (e.g., ToolCallSchema::Validate)
     * 
     * @return Parsed JSON object, or empty object on failure
     */
    template<typename SchemaValidator>
    static json SafeParse(
        const std::string& raw_input,
        ParseErrorCallback on_parse_error = nullptr,
        ParseSuccessCallback on_parse_success = nullptr,
        bool require_schema = true,
        SchemaValidator schema_validator = nullptr)
    {
        try {
            // ========== LAYER 1: STRUCTURE-FIRST EXTRACTION + SAFE PARSE ==========
            std::string parse_error;
            json parsed = ParseFirstValidStructure(raw_input, &parse_error);
            if (!parse_error.empty()) {
                if (on_parse_error) on_parse_error(parse_error);
                return json::object();
            }
            
            // ========== LAYER 2: SCHEMA VALIDATION ==========
            if (require_schema) {
                if constexpr (!std::is_same_v<std::decay_t<SchemaValidator>, std::nullptr_t>) {
                    if (schema_validator) {
                        std::string validation_error;
                        if (!schema_validator(parsed, validation_error)) {
                            std::string error = std::string("Schema validation failed: ") + validation_error;
                            if (on_parse_error) on_parse_error(error);
                            return json::object();
                        }
                    }
                }
            }
            
            // ========== SUCCESS ==========
            if (on_parse_success) on_parse_success(parsed);
            return parsed;
            
        } catch (const std::exception& e) {
            std::string error = std::string("Unhandled exception in SafeParse: ") + e.what();
            if (on_parse_error) on_parse_error(error);
            return json::object();
        }
    }
    
    /**
     * @brief Overload for when you don't need schema validation
     */
    static json SafeParse(
        const std::string& raw_input,
        ParseErrorCallback on_parse_error = nullptr,
        ParseSuccessCallback on_parse_success = nullptr)
    {
        return SafeParse(raw_input, on_parse_error, on_parse_success, false, nullptr);
    }
    
    /**
     * @brief Extract and parse JSON from streaming response
     * 
     * Handles line-delimited JSON (NDJSON) or single-object responses
     * 
     * @param streaming_chunk One chunk of streaming data
     * @return Parsed JSON, or empty on failure
     */
    static json SafeParseStreamChunk(
        const std::string& streaming_chunk,
        ParseErrorCallback on_error = nullptr)
    {
        std::string parse_error;
        json parsed = ParseFirstValidStructure(streaming_chunk, &parse_error);
        if (on_error && !parse_error.empty()) {
            on_error(parse_error);
        }
        return parsed;
    }
};

/**
 * @brief Self-healing inference recovery
 * 
 * When JSON parsing fails, this generates a repair prompt that can be 
 * submitted back to the LLM to request corrected output.
 * 
 * This transforms a crash into an autonomous retry loop.
 */
class JSONParseRecovery {
public:
    /**
     * @brief Generate a repair prompt for failed output
     * 
     * @param bad_output The output that failed to parse
     * @param parse_error The error description
     * @return A prompt that can be submitted to SubmitInference to retry
     */
    static std::string GenerateRepairPrompt(
        const std::string& bad_output,
        const std::string& parse_error)
    {
        return 
            "Return ONLY valid JSON.\n"
            "No markdown. No explanation.\n"
            "Format:\n"
            "{ \"tool\": \"...\", \"arguments\": { ... } }\n"
            "\n"
            "Parse failure:\n" + parse_error + "\n"
            "\n"
            "Fix this:\n" + bad_output;
    }
    
    /**
     * @brief Check if we should attempt recovery
     * 
     * Some parse failures are unrecoverable (e.g., completely empty output).
     * This method determines if it's worth submitting a retry.
     * 
     * @param output The output that failed
     * @param error The parse error
     * @return true if recovery attempt is worthwhile
     */
    static bool ShouldAttemptRecovery(
        const std::string& output,
        const std::string& error)
    {
        // Don't retry on empty output
        if (output.empty() || output.find_first_not_of(" \n\r\t") == std::string::npos) {
            return false;
        }
        
        // Don't retry if output is way too large (likely corrupt)
        if (output.length() > 10 * 1024 * 1024) {  // 10MB
            return false;
        }
        
        // Don't retry if error suggests fundamental type mismatch
        if (error.find("Expected") != std::string::npos && 
            error.find("but got") != std::string::npos) {
            // Some type errors might be recoverable
            return true;
        }
        
        // Recovery is reasonable for most parse errors
        return true;
    }
    
    /**
     * @brief Log recovery attempt for observability
     */
    static void LogRecoveryAttempt(
        int attempt_number,
        const std::string& error,
        std::function<void(const std::string&)> logger = nullptr)
    {
        if (!logger) return;
        
        std::string msg = "JSON parse recovery attempt #" + std::to_string(attempt_number) + 
                         ": " + error;
        logger(msg);
    }
};

} // namespace JSON
} // namespace RawrXD
