#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <functional>
#include <vector>
#include <windows.h>

namespace RawrXD {
namespace JSON {

using json = nlohmann::json;

/**
 * @brief Schema validation for agentic tool calls and responses
 * 
 * Ensures JSON not only parses but also conforms to expected structure.
 * Prevents "valid but useless" JSON from reaching tool dispatch.
 */
class JSONSchemaValidator {
public:
    /**
     * @brief Tool call schema requirements
     * 
     * Expected structure:
     * {
     *   "tool": "tool_name",
     *   "arguments": { ... }
     * }
     */
    struct ToolCallSchema {
        static bool Validate(const json& j, std::string& error_out) {
            if (!j.is_object()) {
                error_out = "Tool call must be an object";
                return false;
            }
            
            if (!j.contains("tool")) {
                error_out = "Missing 'tool' field";
                return false;
            }
            
            if (!j["tool"].is_string()) {
                error_out = "Field 'tool' must be a string";
                return false;
            }
            
            if (j["tool"].get<std::string>().empty()) {
                error_out = "Field 'tool' cannot be empty";
                return false;
            }
            
            if (!j.contains("arguments")) {
                error_out = "Missing 'arguments' field";
                return false;
            }
            
            if (!j["arguments"].is_object()) {
                error_out = "Field 'arguments' must be an object";
                return false;
            }
            
            return true;
        }
    };
    
    /**
     * @brief Inference response schema
     * 
     * Expected structure:
     * {
     *   "response": "text",
     *   "done": true/false,
     *   ... other optional fields
     * }
     */
    struct InferenceResponseSchema {
        static bool Validate(const json& j, std::string& error_out) {
            if (!j.is_object()) {
                error_out = "Response must be an object";
                return false;
            }
            
            // At minimum, needs either "response" or "error"
            bool has_response = j.contains("response");
            bool has_error = j.contains("error");
            
            if (!has_response && !has_error) {
                error_out = "Response must contain either 'response' or 'error' field";
                return false;
            }
            
            // If has response, must be string or can be empty obj (streaming)
            if (has_response) {
                if (!j["response"].is_string() && !j["response"].is_object()) {
                    error_out = "Field 'response' must be a string or object";
                    return false;
                }
            }
            
            return true;
        }
    };
    
    /**
     * @brief Generic field validator
     * 
     * Ensures minimal structure for tool-call-like outputs from LLM
     */
    struct MinimalToolCallSchema {
        static bool Validate(const json& j, std::string& error_out) {
            if (!j.is_object()) {
                error_out = "Output must be a JSON object";
                return false;
            }
            
            // For agentic paths, check if it has tool-like fields
            // At minimum: tool name + some arguments or action
            int descriptor_count = 0;
            
            if (j.contains("tool") || j.contains("action") || j.contains("function")) {
                descriptor_count++;
            }
            
            if (j.contains("arguments") || j.contains("params") || j.contains("input")) {
                descriptor_count++;
            }
            
            if (descriptor_count < 1) {
                error_out = "Output must specify either 'tool', 'action', or 'function' AND arguments/params/input";
                return false;
            }
            
            return true;
        }
    };
    
    /**
     * @brief Validate JSON against a schema validator
     * 
     * @param j JSON object to validate
     * @param validator Function that validates JSON and sets error_out
     * @param error_out Output parameter for error message
     * @return true if validates, false otherwise
     */
    template<typename ValidatorFunc>
    static bool Validate(const json& j, ValidatorFunc validator, std::string& error_out) {
        try {
            return validator(j, error_out);
        } catch (const std::exception& e) {
            error_out = std::string("Validation exception: ") + e.what();
            return false;
        }
    }
    
    /**
     * @brief Safe extraction of string field with default
     */
    static std::string GetStringField(
        const json& j, 
        const std::string& field,
        const std::string& default_value = "") 
    {
        try {
            if (j.contains(field) && j[field].is_string()) {
                return j[field].get<std::string>();
            }
        } catch (const std::exception& e) {
            OutputDebugStringA(("[json_schema_validator] GetStringField exception: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("[json_schema_validator] GetStringField unknown exception\n");
        }
        return default_value;
    }
    
    /**
     * @brief Safe extraction of object field
     */
    static json GetObjectField(
        const json& j, 
        const std::string& field)
    {
        try {
            if (j.contains(field) && j[field].is_object()) {
                return j[field];
            }
        } catch (const std::exception& e) {
            OutputDebugStringA(("[json_schema_validator] GetObjectField exception: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("[json_schema_validator] GetObjectField unknown exception\n");
        }
        return json::object();
    }
    
    /**
     * @brief Safe extraction of array field
     */
    static json GetArrayField(
        const json& j, 
        const std::string& field)
    {
        try {
            if (j.contains(field) && j[field].is_array()) {
                return j[field];
            }
        } catch (const std::exception& e) {
            OutputDebugStringA(("[json_schema_validator] GetArrayField exception: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("[json_schema_validator] GetArrayField unknown exception\n");
        }
        return json::array();
    }
};

} // namespace JSON
} // namespace RawrXD
