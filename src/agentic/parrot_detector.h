#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace rawrxd {

class ParrotDetector {
  public:
    static std::string Normalize(const std::string& value) {
        std::string out;
        out.reserve(value.size());
        bool lastWasSpace = false;
        for (unsigned char ch : value) {
            if (std::isspace(ch)) {
                if (!lastWasSpace && !out.empty()) {
                    out.push_back(' ');
                }
                lastWasSpace = true;
            } else {
                out.push_back(static_cast<char>(std::tolower(ch)));
                lastWasSpace = false;
            }
        }
        if (!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        return out;
    }

    static double Similarity(const std::string& lhs, const std::string& rhs) {
        const size_t len = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
        if (len == 0) {
            return 0.0;
        }
        size_t matches = 0;
        for (size_t i = 0; i < len; ++i) {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) ==
                std::tolower(static_cast<unsigned char>(rhs[i]))) {
                ++matches;
            }
        }
        return static_cast<double>(matches) / static_cast<double>(len);
    }

    static bool IsDegenerate(const std::string& response) {
        if (response.size() < 10) {
            return true;
        }
        const std::string lower = Normalize(response);
        if (lower.find("...") != std::string::npos || lower.find("???") != std::string::npos) {
            return true;
        }
        if (lower.find("<tool_call>") == std::string::npos &&
            lower.find("<final_answer>") == std::string::npos &&
            lower.find('{') == std::string::npos &&
            lower.find("final:") != 0) {
            return true;
        }
        return false;
    }

    static bool IsParroting(const std::string& response, const std::string& prompt) {
        if (response.empty() || prompt.empty()) {
            return false;
        }

        const std::string normResponse = Normalize(response);
        const std::string normPrompt = Normalize(prompt);
        if (normResponse.empty() || normPrompt.empty()) {
            return false;
        }

        const size_t sampleLen = std::min<size_t>(80, normPrompt.size());
        if (sampleLen > 0 && normResponse.find(normPrompt.substr(0, sampleLen)) != std::string::npos) {
            return true;
        }
        if (Similarity(normResponse, normPrompt) > 0.85) {
            return true;
        }

        static const std::vector<std::string> markers = {
            "you are a tool-calling api",
            "mode 1 - tool needed",
            "mode 2 - direct answer",
            "available tools:",
            "your response (start with",
            "respond only with",
            "critical: output only xml",
            "tools:",
            "task:"
        };
        for (const auto& marker : markers) {
            if (normResponse.find(marker) != std::string::npos) {
                return true;
            }
        }

        return false;
    }
};

}  // namespace rawrxd
