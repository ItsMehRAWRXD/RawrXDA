#include "lsp/definition_provider.h"
#include <sstream>
#include <regex>
#include <filesystem>

namespace RawrXD::LSP {

DefinitionProvider::DefinitionProvider() = default;
DefinitionProvider::~DefinitionProvider() = default;

std::vector<Location> DefinitionProvider::provideDefinition(const std::string& uri,
                                                              const std::string& content,
                                                              uint32_t line,
                                                              uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return {};

    // Check index first
    auto it = m_definitions.find(symbol);
    if (it != m_definitions.end()) {
        return it->second;
    }

    // Search in current file
    return findCppDefinition(uri, content, symbol);
}

std::vector<Location> DefinitionProvider::provideDeclaration(const std::string& uri,
                                                               const std::string& content,
                                                               uint32_t line,
                                                               uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return {};

    auto it = m_declarations.find(symbol);
    if (it != m_declarations.end()) {
        return it->second;
    }

    return findCppDeclaration(uri, content, symbol);
}

std::vector<Location> DefinitionProvider::provideTypeDefinition(const std::string& uri,
                                                                const std::string& content,
                                                                uint32_t line,
                                                                uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return {};

    // Find type definition for the symbol
    std::vector<Location> locations;

    std::regex typeRegex("(class|struct|enum|typedef|using)\\s+" + symbol);
    std::smatch match;

    if (std::regex_search(content, match, typeRegex)) {
        Location loc;
        loc.uri = uri;
        loc.startLine = line;
        loc.startColumn = 0;
        loc.endLine = line;
        loc.endColumn = static_cast<uint32_t>(content.length());
        locations.push_back(loc);
    }

    return locations;
}

std::vector<Location> DefinitionProvider::provideImplementation(const std::string& uri,
                                                                const std::string& content,
                                                                uint32_t line,
                                                                uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return {};

    std::vector<Location> locations;

    // Find implementations (function bodies)
    std::regex implRegex(symbol + "\\s*\\([^)]*\\)\\s*\\{");
    std::smatch match;

    std::istringstream stream(content);
    std::string currentLine;
    uint32_t currentLineNum = 0;

    while (std::getline(stream, currentLine)) {
        if (std::regex_search(currentLine, match, implRegex)) {
            Location loc;
            loc.uri = uri;
            loc.startLine = currentLineNum;
            loc.startColumn = 0;
            loc.endLine = currentLineNum;
            loc.endColumn = static_cast<uint32_t>(currentLine.length());
            locations.push_back(loc);
        }
        currentLineNum++;
    }

    return locations;
}

std::vector<DefinitionLink> DefinitionProvider::provideDefinitionLinks(const std::string& uri,
                                                                         const std::string& content) {
    std::vector<DefinitionLink> links;

    // Find all symbol references and link to their definitions
    std::regex symbolRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\b");
    std::smatch match;

    std::istringstream stream(content);
    std::string line;
    uint32_t lineNum = 0;

    while (std::getline(stream, line)) {
        std::string::const_iterator searchStart(line.cbegin());
        while (std::regex_search(searchStart, line.cend(), match, symbolRegex)) {
            std::string symbol = match[1];

            // Check if this is a reference (not a definition)
            if (!isDefinition(line, symbol)) {
                auto defs = findCppDefinition(uri, content, symbol);
                for (const auto& def : defs) {
                    DefinitionLink link;
                    link.originUri = uri;
                    link.originStartLine = lineNum;
                    link.originStartColumn = static_cast<uint32_t>(match.position());
                    link.originEndLine = lineNum;
                    link.originEndColumn = link.originStartColumn + static_cast<uint32_t>(symbol.length());
                    link.targetUri = def.uri;
                    link.targetStartLine = def.startLine;
                    link.targetStartColumn = def.startColumn;
                    link.targetEndLine = def.endLine;
                    link.targetEndColumn = def.endColumn;
                    links.push_back(link);
                }
            }

            searchStart = match.suffix().first;
        }
        lineNum++;
    }

    return links;
}

void DefinitionProvider::indexDefinition(const std::string& symbol,
                                        const Location& location) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_definitions[symbol].push_back(location);
}

void DefinitionProvider::indexDeclaration(const std::string& symbol,
                                         const Location& location) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_declarations[symbol].push_back(location);
}

void DefinitionProvider::clearIndex() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_definitions.clear();
    m_declarations.clear();
}

std::vector<Location> DefinitionProvider::findCppDefinition(const std::string& uri,
                                                             const std::string& content,
                                                             const std::string& symbol) {
    std::vector<Location> locations;

    // Search for class/struct definitions
    std::regex classRegex("(class|struct)\\s+" + symbol + "\\s*[:{]");
    std::smatch match;

    std::istringstream stream(content);
    std::string line;
    uint32_t lineNum = 0;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, match, classRegex)) {
            Location loc;
            loc.uri = uri;
            loc.startLine = lineNum;
            loc.startColumn = static_cast<uint32_t>(match.position(2));
            loc.endLine = lineNum;
            loc.endColumn = loc.startColumn + static_cast<uint32_t>(symbol.length());
            locations.push_back(loc);
        }
        lineNum++;
    }

    // Search for function definitions
    std::regex funcRegex("^\\s*(?:[\\w:]+::)?" + symbol + "\\s*\\([^)]*\\)\\s*(?:const)?\\s*\\{");
    stream.clear();
    stream.str(content);
    lineNum = 0;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, match, funcRegex)) {
            Location loc;
            loc.uri = uri;
            loc.startLine = lineNum;
            loc.startColumn = static_cast<uint32_t>(line.find(symbol));
            loc.endLine = lineNum;
            loc.endColumn = loc.startColumn + static_cast<uint32_t>(symbol.length());
            locations.push_back(loc);
        }
        lineNum++;
    }

    // Search for variable definitions
    std::regex varRegex("^\\s*(?:const\\s+)?(?:[\\w:]+)\\s+" + symbol + "\\s*[=;]");
    stream.clear();
    stream.str(content);
    lineNum = 0;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, match, varRegex)) {
            Location loc;
            loc.uri = uri;
            loc.startLine = lineNum;
            loc.startColumn = static_cast<uint32_t>(line.find(symbol));
            loc.endLine = lineNum;
            loc.endColumn = loc.startColumn + static_cast<uint32_t>(symbol.length());
            locations.push_back(loc);
        }
        lineNum++;
    }

    return locations;
}

std::vector<Location> DefinitionProvider::findCppDeclaration(const std::string& uri,
                                                              const std::string& content,
                                                              const std::string& symbol) {
    std::vector<Location> locations;

    // Search for function declarations (no body)
    std::regex declRegex("^\\s*(?:[\\w:]+)?\\s*(?:[\\w:]+::)?" + symbol + "\\s*\\([^)]*\\)\\s*(?:const)?\\s*;");
    std::smatch match;

    std::istringstream stream(content);
    std::string line;
    uint32_t lineNum = 0;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, match, declRegex)) {
            Location loc;
            loc.uri = uri;
            loc.startLine = lineNum;
            loc.startColumn = static_cast<uint32_t>(line.find(symbol));
            loc.endLine = lineNum;
            loc.endColumn = loc.startColumn + static_cast<uint32_t>(symbol.length());
            locations.push_back(loc);
        }
        lineNum++;
    }

    return locations;
}

std::string DefinitionProvider::getSymbolAtPosition(const std::string& content,
                                                    uint32_t line,
                                                    uint32_t column) {
    auto lines = splitLines(content);
    if (line >= lines.size()) return "";

    std::string currentLine = lines[line];
    if (column >= currentLine.length()) return "";

    // Find word boundaries
    size_t start = column;
    while (start > 0 && (std::isalnum(currentLine[start - 1]) || currentLine[start - 1] == '_')) {
        start--;
    }

    size_t end = column;
    while (end < currentLine.length() &&
           (std::isalnum(currentLine[end]) || currentLine[end] == '_')) {
        end++;
    }

    return currentLine.substr(start, end - start);
}

bool DefinitionProvider::isDefinition(const std::string& line, const std::string& symbol) {
    // Check if this line defines the symbol
    std::regex defPatterns[] = {
        std::regex("(class|struct|enum)\\s+" + symbol + "\\s*[:{"),
        std::regex("^\\s*(?:[\\w:]+::)?" + symbol + "\\s*\\("),
        std::regex("^\\s*(?:const\\s+)?(?:[\\w:]+)\\s+" + symbol + "\\s*[=;]")
    };

    for (const auto& pattern : defPatterns) {
        if (std::regex_search(line, pattern)) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> DefinitionProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
DefinitionProvider& getDefinitionProvider() {
    static DefinitionProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
