#include "lsp/diagnostic_provider.h"
#include <sstream>
#include <regex>
#include <chrono>

namespace RawrXD::LSP {

DiagnosticProvider::DiagnosticProvider() = default;
DiagnosticProvider::~DiagnosticProvider() = default;

void DiagnosticProvider::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool DiagnosticProvider::isEnabled() const {
    return m_enabled;
}

void DiagnosticProvider::setDelay(uint32_t milliseconds) {
    m_delay = milliseconds;
}

std::vector<Diagnostic> DiagnosticProvider::provideDiagnostics(const std::string& uri,
                                                               const std::string& content) {
    if (!m_enabled) return {};

    std::vector<Diagnostic> allDiagnostics;

    // Run all registered sources
    for (const auto& [name, source] : m_sources) {
        auto diagnostics = source(uri, content);
        allDiagnostics.insert(allDiagnostics.end(), diagnostics.begin(), diagnostics.end());
    }

    // Built-in C++ analysis
    auto cppDiagnostics = getCppDiagnostics(uri, content);
    allDiagnostics.insert(allDiagnostics.end(), cppDiagnostics.begin(), cppDiagnostics.end());

    // Notify callback
    if (m_callback) {
        PublishDiagnosticsParams params;
        params.uri = uri;
        params.version = 1; // TODO: Get actual version
        params.diagnostics = allDiagnostics;
        m_callback(params);
    }

    return allDiagnostics;
}

void DiagnosticProvider::clearDiagnostics(const std::string& uri) {
    if (m_callback) {
        PublishDiagnosticsParams params;
        params.uri = uri;
        params.version = 1;
        params.diagnostics = {};
        m_callback(params);
    }
}

std::future<std::vector<Diagnostic>> DiagnosticProvider::provideDiagnosticsAsync(
    const std::string& uri,
    const std::string& content) {

    return std::async(std::launch::async, [this, uri, content]() {
        // Apply delay
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delay));
        return provideDiagnostics(uri, content);
    });
}

void DiagnosticProvider::onDiagnostics(DiagnosticCallback callback) {
    m_callback = callback;
}

void DiagnosticProvider::registerDiagnosticSource(
    const std::string& name,
    std::function<std::vector<Diagnostic>(const std::string& uri, const std::string& content)> source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sources.push_back({name, source});
}

std::vector<Diagnostic> DiagnosticProvider::getCppDiagnostics(const std::string& uri,
                                                               const std::string& content) {
    std::vector<Diagnostic> diagnostics;

    auto syntax = analyzeSyntax(uri, content);
    diagnostics.insert(diagnostics.end(), syntax.begin(), syntax.end());

    auto semantic = analyzeSemantics(uri, content);
    diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());

    auto style = analyzeStyle(uri, content);
    diagnostics.insert(diagnostics.end(), style.begin(), style.end());

    return diagnostics;
}

std::vector<std::string> DiagnosticProvider::getCodeActions(const Diagnostic& diagnostic) {
    std::vector<std::string> actions;

    if (diagnostic.code) {
        if (*diagnostic.code == "missing-semicolon") {
            actions.push_back("Insert semicolon");
        } else if (*diagnostic.code == "unused-variable") {
            actions.push_back("Remove unused variable");
        } else if (*diagnostic.code == "missing-include") {
            actions.push_back("Add missing include");
        }
    }

    return actions;
}

std::vector<Diagnostic> DiagnosticProvider::analyzeSyntax(const std::string& uri,
                                                           const std::string& content) {
    std::vector<Diagnostic> diagnostics;
    auto lines = splitLines(content);

    int braceCount = 0;
    int parenCount = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Check for unbalanced braces
        for (char c : line) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
            else if (c == '(') parenCount++;
            else if (c == ')') parenCount--;
        }

        // Check for missing semicolons (simplified)
        if (!line.empty() && !lineEndsWithSemicolon(line) &&
            !isPreprocessorDirective(line) &&
            !isControlStatement(line) &&
            !isComment(line) &&
            !isEmpty(line)) {

            // Check if next line continues
            if (i + 1 < lines.size() && lines[i + 1].find_first_not_of(" \t") == 0) {
                Diagnostic diag;
                diag.startLine = static_cast<uint32_t>(i);
                diag.startColumn = static_cast<uint32_t>(line.length());
                diag.endLine = static_cast<uint32_t>(i);
                diag.endColumn = static_cast<uint32_t>(line.length() + 1);
                diag.severity = DiagnosticSeverity::Error;
                diag.code = "missing-semicolon";
                diag.source = "rawrxd";
                diag.message = "Missing semicolon";
                diagnostics.push_back(diag);
            }
        }
    }

    // Check for unbalanced braces at end
    if (braceCount != 0) {
        Diagnostic diag;
        diag.startLine = static_cast<uint32_t>(lines.size() - 1);
        diag.startColumn = 0;
        diag.endLine = static_cast<uint32_t>(lines.size() - 1);
        diag.endColumn = static_cast<uint32_t>(lines.back().length());
        diag.severity = DiagnosticSeverity::Error;
        diag.code = "unbalanced-braces";
        diag.source = "rawrxd";
        diag.message = braceCount > 0 ? "Unclosed brace" : "Extra closing brace";
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

std::vector<Diagnostic> DiagnosticProvider::analyzeSemantics(const std::string& uri,
                                                              const std::string& content) {
    std::vector<Diagnostic> diagnostics;
    auto lines = splitLines(content);

    // Find unused variables (simplified)
    std::map<std::string, std::pair<uint32_t, uint32_t>> declaredVars;
    std::set<std::string> usedVars;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Find variable declarations
        std::regex varDeclRegex("(int|long|short|char|bool|float|double|auto|std::\\w+)\\s+(\\w+)");
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());

        while (std::regex_search(searchStart, line.cend(), match, varDeclRegex)) {
            std::string varName = match[2];
            declaredVars[varName] = {static_cast<uint32_t>(i),
                                    static_cast<uint32_t>(match.position(2))};
            searchStart = match.suffix().first;
        }

        // Find variable usages
        for (const auto& [varName, pos] : declaredVars) {
            if (line.find(varName) != std::string::npos &&
                line.find(varName + " =") == std::string::npos) {
                usedVars.insert(varName);
            }
        }
    }

    // Report unused variables
    for (const auto& [varName, pos] : declaredVars) {
        if (usedVars.find(varName) == usedVars.end()) {
            Diagnostic diag;
            diag.startLine = pos.first;
            diag.startColumn = pos.second;
            diag.endLine = pos.first;
            diag.endColumn = pos.second + static_cast<uint32_t>(varName.length());
            diag.severity = DiagnosticSeverity::Warning;
            diag.code = "unused-variable";
            diag.source = "rawrxd";
            diag.message = "Unused variable: '" + varName + "'";
            diag.tags.push_back(DiagnosticTag::Unnecessary);
            diagnostics.push_back(diag);
        }
    }

    return diagnostics;
}

std::vector<Diagnostic> DiagnosticProvider::analyzeStyle(const std::string& uri,
                                                         const std::string& content) {
    std::vector<Diagnostic> diagnostics;
    auto lines = splitLines(content);

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Check for trailing whitespace
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            Diagnostic diag;
            diag.startLine = static_cast<uint32_t>(i);
            diag.startColumn = static_cast<uint32_t>(line.find_last_not_of(" \t") + 1);
            diag.endLine = static_cast<uint32_t>(i);
            diag.endColumn = static_cast<uint32_t>(line.length());
            diag.severity = DiagnosticSeverity::Hint;
            diag.code = "trailing-whitespace";
            diag.source = "rawrxd";
            diag.message = "Trailing whitespace";
            diagnostics.push_back(diag);
        }

        // Check for tabs (if spaces preferred)
        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            Diagnostic diag;
            diag.startLine = static_cast<uint32_t>(i);
            diag.startColumn = static_cast<uint32_t>(tabPos);
            diag.endLine = static_cast<uint32_t>(i);
            diag.endColumn = static_cast<uint32_t>(tabPos + 1);
            diag.severity = DiagnosticSeverity::Hint;
            diag.code = "tab-character";
            diag.source = "rawrxd";
            diag.message = "Tab character found (use spaces)";
            diagnostics.push_back(diag);
        }
    }

    return diagnostics;
}

bool DiagnosticProvider::lineEndsWithSemicolon(const std::string& line) {
    size_t end = line.find_last_not_of(" \t");
    return end != std::string::npos && line[end] == ';';
}

bool DiagnosticProvider::isPreprocessorDirective(const std::string& line) {
    return !line.empty() && line[0] == '#';
}

bool DiagnosticProvider::isControlStatement(const std::string& line) {
    std::regex controlRegex("^\\s*(if|for|while|switch|else|do|try|catch)\\s*[");
    return std::regex_search(line, controlRegex);
}

bool DiagnosticProvider::isComment(const std::string& line) {
    return line.find("//") != std::string::npos ||
           (line.find("/*") != std::string::npos && line.find("*/") != std::string::npos);
}

bool DiagnosticProvider::isEmpty(const std::string& line) {
    return line.find_first_not_of(" \t") == std::string::npos;
}

std::vector<std::string> DiagnosticProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
DiagnosticProvider& getDiagnosticProvider() {
    static DiagnosticProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
