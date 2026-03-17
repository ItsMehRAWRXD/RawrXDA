/**
 * @file agent_diagnostic_parser.hpp
 * @brief Parses compiler/linter output into structured diagnostics for self-healing
 */

#pragma once

#include <string>
#include <vector>

namespace RawrXD {

/**
 * @struct Diagnostic
 * @brief Represents a single error or warning
 */
struct Diagnostic {
    std::string filePath;
    int line = 0;
    int column = 0;
    std::string severity; // "error", "warning"
    std::string code;
    std::string message;
};

/**
 * @class AgentDiagnosticParser
 * @brief Utility to turn messy terminal output into actionable agent context
 */
class AgentDiagnosticParser {
public:
    /**
     * @brief Parse output from MSVC/MASM style compilers
     */
    std::vector<Diagnostic> parseMSVC(const std::string& output);

    /**
     * @brief Parse output from GCC/Clang style compilers
     */
    std::vector<Diagnostic> parseGCC(const std::string& output);

    /**
     * @brief Generic parse method - auto-detects compiler type
     */
    std::vector<Diagnostic> parse(const std::string& output);

    /**
     * @brief Convert diagnostics to a compact string for LLM feedback
     */
    static std::string toContextString(const std::vector<Diagnostic>& diagnostics);
};

} // namespace RawrXD
