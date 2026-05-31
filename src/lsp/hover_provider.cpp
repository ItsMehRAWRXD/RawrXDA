#include "lsp/hover_provider.h"
#include <sstream>
#include <regex>

namespace RawrXD::LSP {

HoverProvider::HoverProvider() = default;
HoverProvider::~HoverProvider() = default;

void HoverProvider::setPreferredFormat(MarkupKind kind) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_preferredFormat = kind;
}

MarkupKind HoverProvider::getPreferredFormat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_preferredFormat;
}

std::optional<Hover> HoverProvider::provideHover(const std::string& uri,
                                                   const std::string& content,
                                                   uint32_t line,
                                                   uint32_t column) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return std::nullopt;

    // Check registered documentation
    auto it = m_documentation.find(symbol);
    if (it != m_documentation.end()) {
        Hover hover;
        hover.contents.kind = m_preferredFormat;
        if (m_preferredFormat == MarkupKind::Markdown) {
            hover.contents.value = generateMarkdown(it->second);
        } else {
            hover.contents.value = generatePlainText(it->second);
        }
        return hover;
    }

    // Try C++ hover
    return getCppHover(uri, content, line, column);
}

std::optional<SymbolDocumentation> HoverProvider::getDocumentation(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_documentation.find(symbol);
    if (it != m_documentation.end()) {
        return it->second;
    }
    return std::nullopt;
}

void HoverProvider::registerDocumentation(const std::string& symbol,
                                          const SymbolDocumentation& doc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_documentation[symbol] = doc;
}

std::optional<Hover> HoverProvider::getCppHover(const std::string& uri,
                                                 const std::string& content,
                                                 uint32_t line,
                                                 uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return std::nullopt;

    Hover hover;
    hover.contents.kind = m_preferredFormat;

    // Try to extract type info
    std::string typeInfo = extractTypeInfo(content, symbol);
    if (!typeInfo.empty()) {
        if (m_preferredFormat == MarkupKind::Markdown) {
            hover.contents.value = "```cpp\n" + typeInfo + "\n```";
        } else {
            hover.contents.value = typeInfo;
        }
        return hover;
    }

    // Try function info
    std::string funcInfo = extractFunctionInfo(content, symbol);
    if (!funcInfo.empty()) {
        if (m_preferredFormat == MarkupKind::Markdown) {
            hover.contents.value = "```cpp\n" + funcInfo + "\n```";
        } else {
            hover.contents.value = funcInfo;
        }
        return hover;
    }

    // Try variable info
    std::string varInfo = extractVariableInfo(content, symbol);
    if (!varInfo.empty()) {
        if (m_preferredFormat == MarkupKind::Markdown) {
            hover.contents.value = "```cpp\n" + varInfo + "\n```";
        } else {
            hover.contents.value = varInfo;
        }
        return hover;
    }

    return std::nullopt;
}

std::string HoverProvider::generateMarkdown(const SymbolDocumentation& doc) {
    std::stringstream ss;
    ss << "## " << doc.name << "\n\n";
    ss << "**Kind:** " << doc.kind << "\n\n";

    if (!doc.signature.empty()) {
        ss << "```cpp\n" << doc.signature << "\n```\n\n";
    }

    if (!doc.documentation.empty()) {
        ss << doc.documentation << "\n\n";
    }

    if (!doc.parameters.empty()) {
        ss << "**Parameters:**\n";
        for (const auto& param : doc.parameters) {
            ss << "- " << param << "\n";
        }
        ss << "\n";
    }

    if (doc.returnType) {
        ss << "**Returns:** " << *doc.returnType << "\n\n";
    }

    if (doc.deprecated) {
        ss << "**Deprecated:** " <> *doc.deprecated << "\n\n";
    }

    return ss.str();
}

std::string HoverProvider::generatePlainText(const SymbolDocumentation& doc) {
    std::stringstream ss;
    ss << doc.name << " (" << doc.kind << ")\n";
    if (!doc.signature.empty()) {
        ss << doc.signature << "\n";
    }
    ss << doc.documentation;
    return ss.str();
}

std::string HoverProvider::getSymbolAtPosition(const std::string& content,
                                               uint32_t line,
                                               uint32_t column) {
    auto lines = splitLines(content);
    if (line >= lines.size()) return "";

    return getWordUnderCursor(lines[line], column);
}

std::string HoverProvider::getWordUnderCursor(const std::string& line,
                                                uint32_t column) {
    if (column >= line.length()) return "";

    size_t start = column;
    while (start > 0 && (std::isalnum(line[start - 1]) || line[start - 1] == '_')) {
        start--;
    }

    size_t end = column;
    while (end < line.length() && (std::isalnum(line[end]) || line[end] == '_')) {
        end++;
    }

    return line.substr(start, end - start);
}

std::string HoverProvider::extractTypeInfo(const std::string& content,
                                           const std::string& symbol) {
    std::regex classRegex("class\\s+" + symbol + "\\s*[:{]");
    std::regex structRegex("struct\\s+" + symbol + "\\s*[:{]");
    std::regex typedefRegex("typedef\\s+.+\\s+" + symbol + "\\s*;");
    std::regex usingRegex("using\\s+" + symbol + "\\s*=\\s*.+;");

    std::smatch match;
    if (std::regex_search(content, match, classRegex)) {
        return "class " + symbol;
    }
    if (std::regex_search(content, match, structRegex)) {
        return "struct " + symbol;
    }
    if (std::regex_search(content, match, typedefRegex)) {
        return match[0];
    }
    if (std::regex_search(content, match, usingRegex)) {
        return match[0];
    }

    return "";
}

std::string HoverProvider::extractFunctionInfo(const std::string& content,
                                               const std::string& symbol) {
    std::regex funcRegex("(\\w+)\\s+" + symbol + "\\s*\\([^)]*\\)\\s*(const)?\\s*;?");
    std::smatch match;

    if (std::regex_search(content, match, funcRegex)) {
        return match[0];
    }

    return "";
}

std::string HoverProvider::extractVariableInfo(const std::string& content,
                                               const std::string& symbol) {
    std::regex varRegex("(\\w+(?:::\\w+)*)\\s+" + symbol + "\\s*[=;]");
    std::smatch match;

    if (std::regex_search(content, match, varRegex)) {
        return match[0];
    }

    return "";
}

std::vector<std::string> HoverProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
HoverProvider& getHoverProvider() {
    static HoverProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
