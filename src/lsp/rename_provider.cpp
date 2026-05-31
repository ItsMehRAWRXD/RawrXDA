#include "lsp/rename_provider.h"
#include <sstream>
#include <regex>
#include <set>

namespace RawrXD::LSP {

RenameProvider::RenameProvider() = default;
RenameProvider::~RenameProvider() = default;

std::optional<std::string> RenameProvider::validateRename(const std::string& uri,
                                                            const std::string& content,
                                                            uint32_t line,
                                                            uint32_t column,
                                                            const std::string& newName) {
    // Check if new name is valid
    if (!isValidCppIdentifier(newName)) {
        return "Invalid identifier: '" + newName + "'";
    }

    if (isReservedCppKeyword(newName)) {
        return "Cannot use reserved keyword: '" + newName + "'";
    }

    // Check if symbol exists at position
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) {
        return "No symbol found at position";
    }

    // Check scope validity
    if (!isScopeValid(content, line, symbol)) {
        return "Cannot rename symbol in this scope";
    }

    return std::nullopt;
}

std::optional<WorkspaceEdit> RenameProvider::prepareRename(const std::string& uri,
                                                           const std::string& content,
                                                           uint32_t line,
                                                           uint32_t column) {
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return std::nullopt;

    // Find all occurrences to preview
    auto edits = findOccurrences(uri, content, symbol);

    WorkspaceEdit edit;
    edit.changes[uri] = edits;

    return edit;
}

std::optional<WorkspaceEdit> RenameProvider::provideRename(const std::string& uri,
                                                           const std::string& content,
                                                           uint32_t line,
                                                           uint32_t column,
                                                           const std::string& newName) {
    // Validate first
    auto error = validateRename(uri, content, line, column, newName);
    if (error) return std::nullopt;

    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return std::nullopt;

    // Find all occurrences
    auto edits = findOccurrences(uri, content, symbol);

    // Update all edits with new name
    for (auto& edit : edits) {
        edit.newText = newName;
    }

    // Also search in workspace if configured
    if (!m_options.honorExcludes) {
        auto workspaceEdits = findOccurrencesInWorkspace(symbol);
        edits.insert(edits.end(), workspaceEdits.begin(), workspaceEdits.end());
    }

    WorkspaceEdit result;
    result.changes[uri] = edits;

    return result;
}

std::vector<std::pair<uint32_t, uint32_t>> RenameProvider::getLinkedEditingRanges(
    const std::string& uri,
    const std::string& content,
    uint32_t line,
    uint32_t column) {

    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    std::string symbol = getSymbolAtPosition(content, line, column);
    if (symbol.empty()) return ranges;

    auto lines = splitLines(content);
    for (uint32_t i = 0; i < lines.size(); ++i) {
        size_t pos = 0;
        while ((pos = lines[i].find(symbol, pos)) != std::string::npos) {
            // Check if this is the same symbol (not part of another word)
            bool wordStart = (pos == 0) || !std::isalnum(lines[i][pos - 1]);
            bool wordEnd = (pos + symbol.length() >= lines[i].length()) ||
                          !std::isalnum(lines[i][pos + symbol.length()]);

            if (wordStart && wordEnd) {
                ranges.push_back({i, static_cast<uint32_t>(pos)});
            }
            pos += symbol.length();
        }
    }

    return ranges;
}

void RenameProvider::setOptions(const RenameOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options = options;
}

RenameOptions RenameProvider::getOptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_options;
}

bool RenameProvider::isValidCppIdentifier(const std::string& name) {
    if (name.empty()) return false;

    // First character must be letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_') return false;

    // Rest can be alphanumeric or underscore
    for (size_t i = 1; i < name.length(); ++i) {
        if (!std::isalnum(name[i]) && name[i] != '_') return false;
    }

    return true;
}

bool RenameProvider::isReservedCppKeyword(const std::string& name) {
    static const std::set<std::string> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
        "public", "register", "reinterpret_cast", "requires", "return", "short",
        "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
        "switch", "template", "this", "thread_local", "throw", "true", "try",
        "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    };

    return keywords.find(name) != keywords.end();
}

std::vector<TextEdit> RenameProvider::findCppOccurrences(const std::string& uri,
                                                          const std::string& content,
                                                          const std::string& oldName) {
    return findOccurrences(uri, content, oldName);
}

std::string RenameProvider::getSymbolAtPosition(const std::string& content,
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

std::vector<TextEdit> RenameProvider::findOccurrences(const std::string& uri,
                                                     const std::string& content,
                                                     const std::string& symbol) {
    std::vector<TextEdit> edits;
    auto lines = splitLines(content);

    for (uint32_t i = 0; i < lines.size(); ++i) {
        size_t pos = 0;
        while ((pos = lines[i].find(symbol, pos)) != std::string::npos) {
            // Check if this is the same symbol (not part of another word)
            bool wordStart = (pos == 0) || !std::isalnum(lines[i][pos - 1]);
            bool wordEnd = (pos + symbol.length() >= lines[i].length()) ||
                          !std::isalnum(lines[i][pos + symbol.length()]);

            if (wordStart && wordEnd) {
                TextEdit edit;
                edit.uri = uri;
                edit.startLine = i;
                edit.startColumn = static_cast<uint32_t>(pos);
                edit.endLine = i;
                edit.endColumn = static_cast<uint32_t>(pos + symbol.length());
                edit.newText = symbol; // Will be replaced with new name
                edits.push_back(edit);
            }
            pos += symbol.length();
        }
    }

    return edits;
}

std::vector<TextEdit> RenameProvider::findOccurrencesInWorkspace(const std::string& symbol) {
    // TODO: Search across all files in workspace
    return {};
}

bool RenameProvider::isScopeValid(const std::string& content,
                                  uint32_t line,
                                  const std::string& symbol) {
    // Check if we're in a valid scope for renaming
    // For now, allow renaming in most contexts
    return true;
}

std::vector<std::string> RenameProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
RenameProvider& getRenameProvider() {
    static RenameProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
