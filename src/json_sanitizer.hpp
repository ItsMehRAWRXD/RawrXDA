#pragma once

#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

namespace RawrXD {
namespace JSON {

/**
 * @brief Sanitize malformed JSON by removing markdown fences and other artifacts
 * 
 * Handles common issues:
 * - Markdown code fences (```json ... ```)
 * - Extra whitespace and newlines
 * - BOM markers
 * - Embedded commentary
 * 
 * @param input Raw string potentially containing malformed JSON
 * @return Cleaned string ready for JSON parsing
 */
class JSONSanitizer {
public:
    /**
     * @brief Find the next balanced JSON object/array in the input.
     *
     * This is structure-first extraction: it ignores markdown fences,
     * commentary, and trailing junk and only returns a balanced JSON region.
     */
    static bool FindNextJSONStructureBounds(
        const std::string& input,
        size_t search_offset,
        size_t& start_out,
        size_t& end_out)
    {
        bool in_string = false;
        bool escape = false;
        std::vector<char> expected_closers;
        size_t start = std::string::npos;

        for (size_t i = search_offset; i < input.size(); ++i) {
            const char ch = input[i];

            if (start == std::string::npos) {
                if (ch == '{' || ch == '[') {
                    start = i;
                    expected_closers.push_back(ch == '{' ? '}' : ']');
                }
                continue;
            }

            if (in_string) {
                if (escape) {
                    escape = false;
                    continue;
                }

                if (ch == '\\') {
                    escape = true;
                    continue;
                }

                if (ch == '"') {
                    in_string = false;
                }
                continue;
            }

            if (ch == '"') {
                in_string = true;
                continue;
            }

            if (ch == '{') {
                expected_closers.push_back('}');
                continue;
            }

            if (ch == '[') {
                expected_closers.push_back(']');
                continue;
            }

            if ((ch == '}' || ch == ']') && !expected_closers.empty()) {
                if (expected_closers.back() != ch) {
                    start = std::string::npos;
                    expected_closers.clear();
                    in_string = false;
                    escape = false;
                    continue;
                }

                expected_closers.pop_back();
                if (expected_closers.empty()) {
                    start_out = start;
                    end_out = i;
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * @brief Extract the first balanced JSON object from the input.
     */
    static std::string ExtractFirstJSONObject(const std::string& input) {
        size_t start = std::string::npos;
        size_t end = std::string::npos;

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] != '{') {
                continue;
            }

            size_t candidate_start = std::string::npos;
            size_t candidate_end = std::string::npos;
            if (FindNextJSONStructureBounds(input, i, candidate_start, candidate_end) &&
                candidate_start == i && candidate_end >= candidate_start) {
                start = candidate_start;
                end = candidate_end;
                break;
            }
        }

        if (start == std::string::npos || end == std::string::npos) {
            return "";
        }

        return input.substr(start, end - start + 1);
    }

    /**
     * @brief Extract the first balanced JSON object or array from the input.
     */
    static std::string ExtractFirstJSONStructure(const std::string& input) {
        size_t start = std::string::npos;
        size_t end = std::string::npos;
        if (!FindNextJSONStructureBounds(input, 0, start, end)) {
            return "";
        }

        return input.substr(start, end - start + 1);
    }

    /**
     * @brief Remove markdown code fences from input
     */
    static std::string StripMarkdownFences(const std::string& input) {
        std::string out = input;
        
        // Strip triple-backtick fences
        constexpr const char* fence = "```";
        size_t start = out.find(fence);
        
        if (start != std::string::npos) {
            // Skip optional "json" language specifier after opening fence
            size_t content_start = start + 3;  // Length of fence
            
            // Skip whitespace and identifier after opening fence
            while (content_start < out.length() && 
                   (std::isspace(out[content_start]) || std::isalpha(out[content_start]))) {
                content_start++;
            }
            
            // Find closing fence
            size_t end = out.rfind(fence);
            
            if (end != std::string::npos && end > start) {
                // Extract content between fences, trimming boundary whitespace
                std::string content = out.substr(content_start, end - content_start);
                
                // Trim leading/trailing whitespace from content
                size_t first = content.find_first_not_of(" \n\r\t");
                size_t last = content.find_last_not_of(" \n\r\t");
                
                if (first != std::string::npos) {
                    out = content.substr(first, last - first + 1);
                } else {
                    out = "";
                }
            }
        }
        
        return out;
    }
    
    /**
     * @brief Trim leading and trailing whitespace
     */
    static std::string TrimWhitespace(const std::string& input) {
        std::string out = input;
        
        // Trim from start
        out.erase(0, out.find_first_not_of(" \n\r\t\x00\x01\x02\x03"));
        
        // Trim from end
        size_t end = out.find_last_not_of(" \n\r\t\x00\x01\x02\x03");
        if (end != std::string::npos) {
            out.erase(end + 1);
        }
        
        return out;
    }
    
    /**
     * @brief Remove UTF-8 BOM if present
     */
    static std::string RemoveBOM(const std::string& input) {
        if (input.length() >= 3 && 
            (unsigned char)input[0] == 0xEF &&
            (unsigned char)input[1] == 0xBB &&
            (unsigned char)input[2] == 0xBF) {
            return input.substr(3);
        }
        return input;
    }
    
    /**
     * @brief Strip comment-like content (e.g., "JSON response: {...}")
     * 
     * Preserves valid JSON structure while removing prefixes
     */
    static std::string StripCommentaryPrefix(const std::string& input) {
        std::string out = input;
        
        // Common prefixes to strip:
        const std::string prefixes[] = {
            "JSON response:",
            "JSON:",
            "Response:",
            "Here's the JSON:",
            "Here is the JSON:",
            "Valid JSON:",
            "Output:",
            "Result:",
            "Error:",
        };
        
        for (const auto& prefix : prefixes) {
            if (out.substr(0, prefix.length()) == prefix) {
                out = out.substr(prefix.length());
                // Trim whitespace after prefix
                out = TrimWhitespace(out);
                break;
            }
        }
        
        return out;
    }
    
    /**
     * @brief Full sanitization pipeline
     * 
     * Applies all cleanup steps:
     * 1. Remove BOM
     * 2. Strip markdown fences
     * 3. Strip commentary prefix
     * 4. Trim whitespace
     * 
     * @param raw Input string that may be malformed
     * @return Cleaned string ready for JSON parsing
     */
    static std::string Sanitize(const std::string& raw) {
        std::string result = raw;
        
        // Remove UTF-8 BOM
        result = RemoveBOM(result);
        
        // Strip markdown fences
        result = StripMarkdownFences(result);
        
        // Strip common commentary prefixes
        result = StripCommentaryPrefix(result);

        // Prefer a structurally balanced JSON payload over naive cleaned text.
        std::string extracted = ExtractFirstJSONStructure(result);
        if (!extracted.empty()) {
            result = extracted;
        }
        
        // Final whitespace trim
        result = TrimWhitespace(result);
        
        return result;
    }
    
    /**
     * @brief Validate that string looks like JSON
     * 
     * Quick check to see if string starts with { or [ 
     * after sanitization. Prevents parse attempts on invalid data.
     * 
     * @param sanitized Already-sanitized string
     * @return true if string looks like it could be JSON
     */
    static bool LooksLikeJSON(const std::string& sanitized) {
        if (sanitized.empty()) {
            return false;
        }
        
        char first = sanitized[0];
        return first == '{' || first == '[' || first == '"' || 
               (first >= '0' && first <= '9') || first == '-' ||
               first == 't' || first == 'f' || first == 'n';
    }
};

} // namespace JSON
} // namespace RawrXD
