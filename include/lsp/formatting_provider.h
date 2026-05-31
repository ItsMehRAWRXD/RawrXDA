#pragma once
/**
 * @file formatting_provider.h
 * @brief Document formatting and range formatting
 * Batch 3 - Item 38: Formatting provider
 */

#include <string>
#include <vector>
#include <optional>

namespace RawrXD::LSP {

struct FormattingOptions {
    uint32_t tabSize = 4;
    bool insertSpaces = true;
    bool trimTrailingWhitespace = true;
    bool insertFinalNewline = true;
    bool trimFinalNewlines = true;
};

struct TextEdit {
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    std::string newText;
};

class FormattingProvider {
public:
    FormattingProvider();
    ~FormattingProvider();

    // Configuration
    void setOptions(const FormattingOptions& options);
    FormattingOptions getOptions() const;

    // Formatting
    std::vector<TextEdit> formatDocument(const std::string& uri,
                                          const std::string& content);
    std::vector<TextEdit> formatRange(const std::string& uri,
                                      const std::string& content,
                                      uint32_t startLine,
                                      uint32_t startColumn,
                                      uint32_t endLine,
                                      uint32_t endColumn);
    std::vector<TextEdit> formatOnType(const std::string& uri,
                                         const std::string& content,
                                         uint32_t line,
                                         uint32_t column,
                                         char typedCharacter);

    // C++ specific formatting
    std::string formatCpp(const std::string& code);
    std::string formatCppRange(const std::string& code,
                               uint32_t startLine,
                               uint32_t endLine);

    // Indentation
    std::string calculateIndentation(uint32_t line,
                                     const std::vector<std::string>& lines);
    int getIndentationLevel(const std::string& line);

    // Utilities
    std::string trimTrailingWhitespace(const std::string& line);
    std::string normalizeLineEndings(const std::string& text);
    std::vector<std::string> splitLines(const std::string& text);
    std::string joinLines(const std::vector<std::string>& lines);

private:
    FormattingOptions m_options;
    mutable std::mutex m_mutex;

    // C++ formatting rules
    std::string formatLine(const std::string& line,
                          int currentIndent,
                          bool& nextIndent);
    bool shouldIncreaseIndent(const std::string& line);
    bool shouldDecreaseIndent(const std::string& line);
    bool isOpeningBrace(const std::string& line);
    bool isClosingBrace(const std::string& line);
    bool isNamespace(const std::string& line);
    bool isClass(const std::string& line);
    bool isControlStatement(const std::string& line);

    std::string createIndent(int level);
};

// Global provider
FormattingProvider& getFormattingProvider();

} // namespace RawrXD::LSP
