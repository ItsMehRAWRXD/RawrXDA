#include "lsp/inlay_hint_provider.h"
#include <sstream>
#include <regex>

namespace RawrXD::LSP {

InlayHintProvider::InlayHintProvider() = default;
InlayHintProvider::~InlayHintProvider() = default;

void InlayHintProvider::setOptions(const InlayHintOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options = options;
}

InlayHintOptions InlayHintProvider::getOptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_options;
}

std::vector<InlayHint> InlayHintProvider::provideInlayHints(const std::string& uri,
                                                             const std::string& content,
                                                             uint32_t startLine,
                                                             uint32_t endLine) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<InlayHint> hints;

    if (m_options.showTypeHints) {
        auto typeHints = getTypeHints(content, startLine, endLine);
        hints.insert(hints.end(), typeHints.begin(), typeHints.end());
    }

    if (m_options.showChainingHints) {
        auto chainHints = getChainingHints(content, startLine, endLine);
        hints.insert(hints.end(), chainHints.begin(), chainHints.end());
    }

    if (m_options.showReturnTypeHints) {
        auto returnHints = getReturnTypeHints(content, startLine, endLine);
        hints.insert(hints.end(), returnHints.begin(), returnHints.end());
    }

    return hints;
}

std::optional<InlayHint> InlayHintProvider::resolveInlayHint(const InlayHint& hint) {
    // Resolve any additional data for the hint
    return hint;
}

std::vector<InlayHint> InlayHintProvider::getCppInlayHints(const std::string& uri,
                                                            const std::string& content,
                                                            uint32_t startLine,
                                                            uint32_t endLine) {
    return provideInlayHints(uri, content, startLine, endLine);
}

std::optional<std::string> InlayHintProvider::inferType(const std::string& content,
                                                      uint32_t line,
                                                      uint32_t column) {
    auto lines = splitLines(content);
    if (line >= lines.size()) return std::nullopt;

    std::string currentLine = lines[line];

    // Check for auto variable
    std::regex autoRegex("auto\\s+(\\w+)\\s*=\\s*(.+);");
    std::smatch match;

    if (std::regex_search(currentLine, match, autoRegex)) {
        std::string value = match[2];

        // Infer type from value
        if (value.find('"') != std::string::npos) return "std::string";
        if (std::regex_match(value, std::regex("-?\\d+"))) return "int";
        if (std::regex_match(value, std::regex("-?\\d+\\.\\d+"))) return "double";
        if (value == "true" || value == "false") return "bool";
        if (value.find("{") != std::string::npos) return "auto";

        // Check for function call
        std::regex funcCallRegex("(\\w+)\\s*\\(");
        std::smatch funcMatch;
        if (std::regex_search(value, funcMatch, funcCallRegex)) {
            return funcMatch[1] + "::return_type";
        }
    }

    return std::nullopt;
}

std::vector<InlayHint> InlayHintProvider::getParameterHints(const std::string& content,
                                                            uint32_t line,
                                                            uint32_t column) {
    std::vector<InlayHint> hints;

    auto lines = splitLines(content);
    if (line >= lines.size()) return hints;

    std::string currentLine = lines[line];

    // Find function call
    size_t parenPos = currentLine.rfind('(', column);
    if (parenPos == std::string::npos) return hints;

    // Extract function name
    std::string beforeParen = currentLine.substr(0, parenPos);
    size_t nameStart = beforeParen.find_last_of(" \t.->::");
    if (nameStart == std::string::npos) nameStart = 0;
    else nameStart++;

    std::string funcName = beforeParen.substr(nameStart);

    // Get parameter names (simplified)
    std::vector<std::string> params = getFunctionParameters(funcName);

    // Find arguments in the call
    std::string afterParen = currentLine.substr(parenPos + 1);
    std::vector<std::string> args;
    size_t argStart = 0;
    int parenDepth = 0;

    for (size_t i = 0; i < afterParen.length(); ++i) {
        if (afterParen[i] == '(') parenDepth++;
        else if (afterParen[i] == ')') {
            if (parenDepth == 0) {
                args.push_back(afterParen.substr(argStart, i - argStart));
                break;
            }
            parenDepth--;
        }
        else if (afterParen[i] == ',' && parenDepth == 0) {
            args.push_back(afterParen.substr(argStart, i - argStart));
            argStart = i + 1;
        }
    }

    // Create hints for each argument
    for (size_t i = 0; i < std::min(args.size(), params.size()); ++i) {
        InlayHint hint;
        hint.line = line;
        hint.column = static_cast<uint32_t>(parenPos + 1 + argStart);
        hint.kind = InlayHintKind::Parameter;

        InlayHintLabelPart part;
        part.value = params[i] + ": ";
        hint.labelParts.push_back(part);

        hint.paddingRight = true;
        hints.push_back(hint);
    }

    return hints;
}

std::vector<InlayHint> InlayHintProvider::getTypeHints(const std::string& content,
                                                      uint32_t startLine,
                                                      uint32_t endLine) {
    std::vector<InlayHint> hints;
    auto lines = splitLines(content);

    for (uint32_t i = startLine; i <= endLine && i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Check for auto variables
        std::regex autoRegex("auto\\s+(\\w+)");
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());

        while (std::regex_search(searchStart, line.cend(), match, autoRegex)) {
            auto inferredType = inferType(content, i,
                static_cast<uint32_t>(match.position(1) + match[1].length()));

            if (inferredType) {
                InlayHint hint;
                hint.line = i;
                hint.column = static_cast<uint32_t>(match.position(1) + match[1].length());
                hint.kind = InlayHintKind::Type;

                InlayHintLabelPart part;
                part.value = ": " + *inferredType;
                hint.labelParts.push_back(part);

                hint.paddingLeft = false;
                hint.paddingRight = true;
                hints.push_back(hint);
            }

            searchStart = match.suffix().first;
        }
    }

    return hints;
}

std::vector<InlayHint> InlayHintProvider::getChainingHints(const std::string& content,
                                                         uint32_t startLine,
                                                         uint32_t endLine) {
    std::vector<InlayHint> hints;
    auto lines = splitLines(content);

    for (uint32_t i = startLine; i <= endLine && i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Check for method chains
        std::regex chainRegex("\\)\\s*\\.\\s*(\\w+)");
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());

        while (std::regex_search(searchStart, line.cend(), match, chainRegex)) {
            InlayHint hint;
            hint.line = i;
            hint.column = static_cast<uint32_t>(match.position(1));
            hint.kind = InlayHintKind::Type;

            InlayHintLabelPart part;
            part.value = "→ " + match[1].str() + "()";
            hint.labelParts.push_back(part);

            hint.paddingLeft = true;
            hint.paddingRight = false;
            hints.push_back(hint);

            searchStart = match.suffix().first;
        }
    }

    return hints;
}

std::vector<InlayHint> InlayHintProvider::getReturnTypeHints(const std::string& content,
                                                           uint32_t startLine,
                                                           uint32_t endLine) {
    std::vector<InlayHint> hints;
    auto lines = splitLines(content);

    for (uint32_t i = startLine; i <= endLine && i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Check for return statements
        std::regex returnRegex("return\\s+(.+);");
        std::smatch match;

        if (std::regex_search(line, match, returnRegex)) {
            // Infer return type from expression
            std::string expr = match[1];

            InlayHint hint;
            hint.line = i;
            hint.column = static_cast<uint32_t>(match.position(1));
            hint.kind = InlayHintKind::Type;

            InlayHintLabelPart part;
            part.value = "→ " + inferExpressionType(expr);
            hint.labelParts.push_back(part);

            hint.paddingLeft = true;
            hint.paddingRight = false;
            hints.push_back(hint);
        }
    }

    return hints;
}

std::string InlayHintProvider::inferExpressionType(const std::string& expr) {
    if (expr.find('"') != std::string::npos) return "std::string";
    if (std::regex_match(expr, std::regex("-?\\d+"))) return "int";
    if (std::regex_match(expr, std::regex("-?\\d+\\.\\d+"))) return "double";
    if (expr == "true" || expr == "false") return "bool";
    if (expr.find("{") != std::string::npos) return "auto";
    return "auto";
}

std::vector<std::string> InlayHintProvider::getFunctionParameters(const std::string& functionName) {
    // Simplified parameter lookup
    static std::map<std::string, std::vector<std::string>> knownFunctions = {
        {"printf", {"format", "..."}},
        {"scanf", {"format", "..."}},
        {"memcpy", {"dest", "src", "count"}},
        {"memset", {"dest", "ch", "count"}},
        {"strlen", {"str"}},
        {"strcmp", {"lhs", "rhs"}},
        {"strcpy", {"dest", "src"}},
        {"vector", {"size"}},
        {"push_back", {"value"}},
        {"emplace_back", {"args"}},
        {"insert", {"pos", "value"}},
        {"erase", {"pos"}},
        {"find", {"value"}},
        {"at", {"index"}},
        {"resize", {"size"}},
        {"reserve", {"capacity"}},
        {"shrink_to_fit", {}},
        {"clear", {}},
        {"empty", {}},
        {"size", {}},
        {"capacity", {}},
        {"begin", {}},
        {"end", {}},
        {"cbegin", {}},
        {"cend", {}}
    };

    auto it = knownFunctions.find(functionName);
    if (it != knownFunctions.end()) {
        return it->second;
    }

    return {};
}

std::vector<std::string> InlayHintProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
InlayHintProvider& getInlayHintProvider() {
    static InlayHintProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
