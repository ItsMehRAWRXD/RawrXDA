#pragma once
/**
 * @file hover_provider.h
 * @brief Hover information and documentation
 * Batch 3 - Item 40: Hover provider
 */

#include <string>
#include <vector>
#include <optional>

namespace RawrXD::LSP {

enum class MarkupKind {
    PlainText,
    Markdown
};

struct MarkupContent {
    MarkupKind kind;
    std::string value;
};

struct Hover {
    MarkupContent contents;
    struct Range {
        uint32_t startLine;
        uint32_t startColumn;
        uint32_t endLine;
        uint32_t endColumn;
    };
    std::optional<Range> range;
};

struct SymbolDocumentation {
    std::string name;
    std::string kind;
    std::string signature;
    std::string documentation;
    std::vector<std::string> parameters;
    std::optional<std::string> returnType;
    std::optional<std::string> deprecated;
    std::vector<std::string> examples;
    std::vector<std::string> seeAlso;
};

class HoverProvider {
public:
    HoverProvider();
    ~HoverProvider();

    // Configuration
    void setPreferredFormat(MarkupKind kind);
    MarkupKind getPreferredFormat() const;

    // Hover
    std::optional<Hover> provideHover(const std::string& uri,
                                        const std::string& content,
                                        uint32_t line,
                                        uint32_t column);

    // Documentation
    std::optional<SymbolDocumentation> getDocumentation(const std::string& symbol);
    void registerDocumentation(const std::string& symbol,
                               const SymbolDocumentation& doc);

    // C++ specific
    std::optional<Hover> getCppHover(const std::string& uri,
                                      const std::string& content,
                                      uint32_t line,
                                      uint32_t column);

    // Markdown generation
    std::string generateMarkdown(const SymbolDocumentation& doc);
    std::string generatePlainText(const SymbolDocumentation& doc);

    // Symbol extraction
    std::string getSymbolAtPosition(const std::string& content,
                                    uint32_t line,
                                    uint32_t column);
    std::string getWordUnderCursor(const std::string& line,
                                     uint32_t column);

private:
    MarkupKind m_preferredFormat{MarkupKind::Markdown};
    std::map<std::string, SymbolDocumentation> m_documentation;
    mutable std::mutex m_mutex;

    std::string extractTypeInfo(const std::string& content,
                                const std::string& symbol);
    std::string extractFunctionInfo(const std::string& content,
                                    const std::string& symbol);
    std::string extractVariableInfo(const std::string& content,
                                    const std::string& symbol);
};

// Global provider
HoverProvider& getHoverProvider();

} // namespace RawrXD::LSP
