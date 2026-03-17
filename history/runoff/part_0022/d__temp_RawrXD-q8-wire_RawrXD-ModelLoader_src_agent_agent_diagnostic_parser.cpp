#include "agent_diagnostic_parser.hpp"
#include <regex>
#include <algorithm>
#include <iostream>

namespace RawrXD {

std::vector<Diagnostic> AgentDiagnosticParser::parseMSVC(const std::string& output)
{
    std::vector<Diagnostic> diagnostics;
    std::regex re(R"(([^(]+)\((\d+)(?:,(\d+))?\)\s*:\s*(\w+)\s+(\w+)\s*:\s*(.*))");
    
    auto words_begin = std::sregex_iterator(output.begin(), output.end(), re);
    auto words_end = std::sregex_iterator();
    
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        Diagnostic d;
        d.filePath = match[1].str();
        d.line = std::stoi(match[2].str());
        if (match[3].matched) d.column = std::stoi(match[3].str());
        d.severity = match[4].str();
        std::transform(d.severity.begin(), d.severity.end(), d.severity.begin(), ::tolower);
        d.code = match[5].str();
        d.message = match[6].str();
        diagnostics.push_back(d);
    }
    
    return diagnostics;
}

std::vector<Diagnostic> AgentDiagnosticParser::parseGCC(const std::string& output)
{
    std::vector<Diagnostic> diagnostics;
    std::regex re(R"(^([^:]+):(\d+):(\d+):\s*(\w+):\s*(.*)$)", std::regex_constants::multiline);
    
    auto words_begin = std::sregex_iterator(output.begin(), output.end(), re);
    auto words_end = std::sregex_iterator();
    
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        Diagnostic d;
        d.filePath = match[1].str();
        d.line = std::stoi(match[2].str());
        d.column = std::stoi(match[3].str());
        d.severity = match[4].str();
        std::transform(d.severity.begin(), d.severity.end(), d.severity.begin(), ::tolower);
        d.message = match[5].str();
        diagnostics.push_back(d);
    }
    
    return diagnostics;
}

std::vector<Diagnostic> AgentDiagnosticParser::parse(const std::string& output)
{
    auto msvc = parseMSVC(output);
    if (!msvc.empty()) return msvc;
    return parseGCC(output);
}

std::string AgentDiagnosticParser::toContextString(const std::vector<Diagnostic>& diagnostics)
{
    std::string result;
    for (const auto& d : diagnostics) {
        std::string sev = d.severity;
        std::transform(sev.begin(), sev.end(), sev.begin(), ::toupper);
        result += "[" + sev + "] " + d.code + " at " + d.filePath + ":" + std::to_string(d.line) + " - " + d.message + "\n";
    }
    return result;
}

} // namespace RawrXD
