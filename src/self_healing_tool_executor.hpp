#pragma once

#include "json_parse_guard.hpp"
#include "json_sanitizer.hpp"
#include "json_schema_validator.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <functional>
#include <memory>

namespace RawrXD {
namespace Agentic {

using json = nlohmann::json;

/**
 * @brief Self-healing tool call executor
 * 
 * When LLM output fails to parse, this orchestrator:
 * 1. Detects the failure
 * 2. Generates a repair prompt
 * 3. Submits it back to the LLM via SubmitInference
 * 4. Validates the corrected output
 * 5. Executes the tool if valid
 * 
 * This creates a closed-loop system that recovers from LLM output errors
 * transparently, without bubbling crashes up to the caller.
 */
class SelfHealingToolExecutor {
public:
    using InferenceSubmitter = std::function<std::string(const std::string&)>;
    using ToolDispatcher = std::function<std::string(const json&)>;
    using ErrorLogger = std::function<void(const std::string&)>;
    
    struct Config {
        int max_recovery_attempts = 2;
        bool log_recovery_attempts = true;
        bool fail_closed = true;  // Return error instead of partial result on repeated failures
    };
    
    /**
     * @brief Execute a tool call with self-healing
     * 
     * @param llm_output Raw output from LLM (may be malformed)
     * @param submit_inference Callback to submit repair prompts to LLM
     * @param dispatch_tool Callback to execute validated tool calls
     * @param error_logger Callback for error logging
     * @param config Configuration for recovery behavior
     * 
     * @return Result from tool execution, or error JSON object
     */
    static std::string ExecuteWithHealing(
        const std::string& llm_output,
        InferenceSubmitter submit_inference,
        ToolDispatcher dispatch_tool,
        ErrorLogger error_logger = nullptr,
        const Config& config = Config{})
    {
        return ExecuteWithHealingInternal(
            llm_output, submit_inference, dispatch_tool, error_logger, 
            config, 0 /* attempt number */);
    }
    
private:
    static std::string ExecuteWithHealingInternal(
        const std::string& llm_output,
        InferenceSubmitter submit_inference,
        ToolDispatcher dispatch_tool,
        ErrorLogger error_logger,
        const Config& config,
        int attempt)
    {
        // ========== ATTEMPT PARSE ==========
        std::string parse_error;
        json parsed = JSON::JSONParseGuard::SafeParse(
            llm_output,
            [&parse_error](const std::string& err) {
                parse_error = err;
            },
            nullptr,
            true,
            JSON::JSONSchemaValidator::ToolCallSchema::Validate);
        
        // ========== SUCCESS PATH ==========
        if (!parsed.empty() && parsed.is_object()) {
            if (config.log_recovery_attempts && attempt > 0 && error_logger) {
                error_logger("Recovery successful after " + std::to_string(attempt) + " attempt(s)");
            }
            
            // Dispatch to tool executor
            try {
                return dispatch_tool(parsed);
            } catch (const std::exception& e) {
                std::string error_result = CreateErrorJSON(
                    "tool_exec_error",
                    "Tool execution failed: " + std::string(e.what()));
                return error_result;
            }
        }
        
        // ========== RECOVERY PATH ==========
        if (attempt >= config.max_recovery_attempts) {
            // Max attempts exceeded
            std::string error_result = CreateErrorJSON(
                "max_recovery_attempts_exceeded",
                "Failed to obtain valid tool call after " + std::to_string(config.max_recovery_attempts) + " recovery attempts. "
                "Final error: " + parse_error);
            
            if (error_logger) {
                error_logger("Self-healing exhausted: " + parse_error);
            }
            
            return error_result;
        }
        
        // ========== ATTEMPT RECOVERY ==========
        if (config.log_recovery_attempts && error_logger) {
            error_logger("JSON parse failed (attempt " + std::to_string(attempt + 1) + "/" + 
                        std::to_string(config.max_recovery_attempts + 1) + "). Error: " + parse_error);
        }
        
        // Check if recovery is worthwhile
        if (!JSON::JSONParseRecovery::ShouldAttemptRecovery(llm_output, parse_error)) {
            std::string error_result = CreateErrorJSON(
                "unrecoverable_parse_error",
                "Output not recoverable: " + parse_error);
            return error_result;
        }
        
        // Generate repair prompt
        std::string repair_prompt = JSON::JSONParseRecovery::GenerateRepairPrompt(
            llm_output, parse_error);
        
        if (config.log_recovery_attempts && error_logger) {
            error_logger("Submitting repair prompt to LLM...");
        }
        
        // ========== SUBMIT REPAIR ==========
        std::string repaired_output;
        try {
            repaired_output = submit_inference(repair_prompt);
        } catch (const std::exception& e) {
            std::string error_result = CreateErrorJSON(
                "repair_submit_error",
                "Failed to submit repair prompt: " + std::string(e.what()));
            if (error_logger) {
                error_logger(error_result);
            }
            return error_result;
        }
        
        if (repaired_output.empty()) {
            std::string error_result = CreateErrorJSON(
                "repair_empty_response",
                "LLM returned empty response during recovery");
            return error_result;
        }
        
        // ========== RECURSIVE RETRY ==========
        return ExecuteWithHealingInternal(
            repaired_output, submit_inference, dispatch_tool,
            error_logger, config, attempt + 1);
    }
    
    /**
     * @brief Create a standardized error JSON response
     */
    static std::string CreateErrorJSON(
        const std::string& error_code,
        const std::string& message)
    {
        json error_obj = {
            {"_error", true},
            {"error_code", error_code},
            {"message", message}
        };
        return error_obj.dump();
    }
};

} // namespace Agentic
} // namespace RawrXD
