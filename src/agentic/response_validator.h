#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace rawrxd {

class ResponseValidator {
  public:
    struct ValidationResult {
        bool isValid = false;
        bool isEcho = false;
        bool isToolCall = false;
        bool isFinalAnswer = false;
        std::string cleanedResponse;
        std::string rejectionReason;
    };

    static bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
        if (needle.empty() || needle.size() > haystack.size()) {
            return false;
        }
        const auto toLower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [&](char lhs, char rhs) { return toLower(static_cast<unsigned char>(lhs)) == toLower(static_cast<unsigned char>(rhs)); });
        return it != haystack.end();
    }

    static std::string Trim(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }

        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }

        return value.substr(start, end - start);
    }

    static std::string StripFluff(const std::string& raw) {
        static const std::vector<std::string> fluffPrefixes = {
            "sure", "okay", "here", "i will", "let me", "alright", "well"
        };

        const std::string cleaned = Trim(raw);
        std::string lower = cleaned;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        for (const auto& prefix : fluffPrefixes) {
            if (lower.size() >= prefix.size() && lower.compare(0, prefix.size(), prefix) == 0) {
                size_t pos = prefix.size();
                while (pos < cleaned.size() &&
                       (std::isspace(static_cast<unsigned char>(cleaned[pos])) || cleaned[pos] == ',')) {
                    ++pos;
                }
                return Trim(cleaned.substr(pos));
            }
        }

        return cleaned;
    }

    static ValidationResult Validate(const std::string& rawResponse,
                                     const std::vector<std::string>& forbiddenPhrases) {
        ValidationResult result;
        std::string cleaned = StripFluff(rawResponse);

        for (const auto& phrase : forbiddenPhrases) {
            if (!phrase.empty() && ContainsIgnoreCase(cleaned, phrase)) {
                result.isEcho = true;
                result.rejectionReason = "Echo detected: '" + phrase + "'";
                return result;
            }
        }

        std::string lowerCleaned = cleaned;
        std::transform(lowerCleaned.begin(), lowerCleaned.end(), lowerCleaned.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        static const std::vector<std::string> metaPatterns = {
            "i will use", "i need to", "let me", "i should", "i can",
            "tool to", "using the", "the file_read tool", "the terminal tool"
        };
        for (const auto& meta : metaPatterns) {
            if (lowerCleaned.find(meta) != std::string::npos &&
                cleaned.find("<tool_call>") == std::string::npos) {
                result.isEcho = true;
                result.rejectionReason = "Meta-description detected: '" + meta + "'";
                return result;
            }
        }

        result.isToolCall = cleaned.find("<tool_call>") != std::string::npos ||
                            cleaned.find("TOOL_CALL:") != std::string::npos;
        result.isFinalAnswer = cleaned.find("<final_answer>") != std::string::npos ||
                               cleaned.rfind("FINAL:", 0) == 0;
        if (!result.isToolCall && !result.isFinalAnswer) {
            const bool hasOpenTool = cleaned.find("<tool_call>") != std::string::npos;
            const bool hasCloseTool = cleaned.find("</tool_call>") != std::string::npos;
            const bool hasOpenFinal = cleaned.find("<final_answer>") != std::string::npos;
            const bool hasCloseFinal = cleaned.find("</final_answer>") != std::string::npos;
            if (hasOpenTool && !hasCloseTool) {
                result.rejectionReason = "Malformed: unclosed <tool_call>";
            } else if (hasOpenFinal && !hasCloseFinal) {
                result.rejectionReason = "Malformed: unclosed <final_answer>";
            } else {
                result.rejectionReason = "No valid format tag found (<tool_call> or <final_answer>)";
            }
            return result;
        }

        if (result.isToolCall && result.isFinalAnswer) {
            result.rejectionReason = "Ambiguous: contains both <tool_call> and <final_answer>";
            return result;
        }

        result.isValid = true;
        result.cleanedResponse = cleaned;
        return result;
    }

    static bool ContainsPromptLeakage(const std::string& response) {
        static const std::vector<std::string> leakageMarkers = {
            "CRITICAL CONSTRAINTS",
            "OUTPUT FORMAT SPECIFICATION",
            "FEW-SHOT EXAMPLES",
            "DECISION PROTOCOL",
            "User:",
            "Wrong (echo):",
            "Correct:",
            "<|system|>",
            "<|user|>",
            "<|assistant|>"
        };
        for (const auto& marker : leakageMarkers) {
            if (response.find(marker) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace rawrxd
