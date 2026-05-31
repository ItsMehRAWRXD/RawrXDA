#include "lsp/formatting_provider.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::LSP {

FormattingProvider::FormattingProvider() = default;
FormattingProvider::~FormattingProvider() = default;

void FormattingProvider::setOptions(const FormattingOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options = options;
}

FormattingOptions FormattingProvider::getOptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_options;
}

std::vector<TextEdit> FormattingProvider::formatDocument(const std::string& uri,
                                                          const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TextEdit> edits;

    std::string formatted = formatCpp(content);
    if (formatted != content) {
        TextEdit edit;
        edit.startLine = 0;
        edit.startColumn = 0;

        auto lines = splitLines(content);
        edit.endLine = static_cast<uint32_t>(lines.size() - 1);
        edit.endColumn = static_cast<uint32_t>(lines.empty() ? 0 : lines.back().length());
        edit.newText = formatted;

        edits.push_back(edit);
    }

    return edits;
}

std::vector<TextEdit> FormattingProvider::formatRange(const std::string& uri,
                                                      const std::string& content,
                                                      uint32_t startLine,
                                                      uint32_t startColumn,
                                                      uint32_t endLine,
                                                      uint32_t endColumn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TextEdit> edits;

    auto lines = splitLines(content);
    if (startLine >= lines.size()) return edits;

    // Extract range
    std::vector<std::string> rangeLines;
    for (uint32_t i = startLine; i <= endLine && i < lines.size(); ++i) {
        rangeLines.push_back(lines[i]);
    }

    std::string rangeContent = joinLines(rangeLines);
    std::string formatted = formatCpp(rangeContent);

    if (formatted != rangeContent) {
        TextEdit edit;
        edit.startLine = startLine;
        edit.startColumn = startColumn;
        edit.endLine = endLine;
        edit.endColumn = endColumn;
        edit.newText = formatted;
        edits.push_back(edit);
    }

    return edits;
}

std::vector<TextEdit> FormattingProvider::formatOnType(const std::string& uri,
                                                         const std::string& content,
                                                         uint32_t line,
                                                         uint32_t column,
                                                         char typedCharacter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TextEdit> edits;

    // Format current line when typing certain characters
    if (typedCharacter == ';' || typedCharacter == '}' || typedCharacter == '{') {
        auto lines = splitLines(content);
        if (line < lines.size()) {
            std::string formatted = formatLine(lines[line], getIndentationLevel(lines[line]), false);
            if (formatted != lines[line]) {
                TextEdit edit;
                edit.startLine = line;
                edit.startColumn = 0;
                edit.endLine = line;
                edit.endColumn = static_cast<uint32_t>(lines[line].length());
                edit.newText = formatted;
                edits.push_back(edit);
            }
        }
    }

    return edits;
}

std::string FormattingProvider::formatCpp(const std::string& code) {
    auto lines = splitLines(code);
    std::vector<std::string> formatted;
    int currentIndent = 0;
    bool nextIndent = false;

    for (auto& line : lines) {
        std::string formattedLine = formatLine(line, currentIndent, nextIndent);
        formatted.push_back(formattedLine);

        // Update indent for next line
        if (nextIndent) {
            currentIndent++;
            nextIndent = false;
        }
        if (shouldDecreaseIndent(line)) {
            currentIndent = std::max(0, currentIndent - 1);
        }
        if (shouldIncreaseIndent(line)) {
            nextIndent = true;
        }
    }

    return joinLines(formatted);
}

std::string FormattingProvider::formatLine(const std::string& line,
                                            int currentIndent,
                                            bool& nextIndent) {
    std::string trimmed = line;

    // Remove leading/trailing whitespace
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return "";

    size_t end = trimmed.find_last_not_of(" \t");
    trimmed = trimmed.substr(start, end - start + 1);

    // Add proper indentation
    return createIndent(currentIndent) + trimmed;
}

bool FormattingProvider::shouldIncreaseIndent(const std::string& line) {
    return line.find('{') != std::string::npos ||
           line.find("public:") != std::string::npos ||
           line.find("private:") != std::string::npos ||
           line.find("protected:") != std::string::npos;
}

bool FormattingProvider::shouldDecreaseIndent(const std::string& line) {
    return line.find('}') != std::string::npos;
}

bool FormattingProvider::isOpeningBrace(const std::string& line) {
    return line.find('{') != std::string::npos;
}

bool FormattingProvider::isClosingBrace(const std::string& line) {
    return line.find('}') != std::string::npos;
}

bool FormattingProvider::isNamespace(const std::string& line) {
    return line.find("namespace") != std::string::npos;
}

bool FormattingProvider::isClass(const std::string& line) {
    return line.find("class") != std::string::npos ||
           line.find("struct") != std::string::npos;
}

bool FormattingProvider::isControlStatement(const std::string& line) {
    return line.find("if") != std::string::npos ||
           line.find("for") != std::string::npos ||
           line.find("while") != std::string::npos ||
           line.find("switch") != std::string::npos;
}

std::string FormattingProvider::createIndent(int level) {
    if (m_options.insertSpaces) {
        return std::string(level * m_options.tabSize, ' ');
    } else {
        return std::string(level, '\t');
    }
}

int FormattingProvider::getIndentationLevel(const std::string& line) {
    size_t spaces = 0;
    size_t tabs = 0;

    for (char c : line) {
        if (c == ' ') spaces++;
        else if (c == '\t') tabs++;
        else break;
    }

    if (m_options.insertSpaces) {
        return static_cast<int>((spaces + tabs * m_options.tabSize) / m_options.tabSize);
    } else {
        return static_cast<int>(tabs + spaces / m_options.tabSize);
    }
}

std::string FormattingProvider::trimTrailingWhitespace(const std::string& line) {
    size_t end = line.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : line.substr(0, end + 1);
}

std::string FormattingProvider::normalizeLineEndings(const std::string& text) {
    std::string result = text;
    size_t pos = 0;
    while ((pos = result.find("\r\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    return result;
}

std::vector<std::string> FormattingProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string FormattingProvider::joinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += "\n";
        }
    }
    return result;
}

// Global provider
FormattingProvider& getFormattingProvider() {
    static FormattingProvider provider;
    return provider;
}

} // namespace RawrXD::LSP
