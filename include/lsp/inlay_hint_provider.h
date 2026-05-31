#pragma once
/**
 * @file inlay_hint_provider.h
 * @brief Inlay hints for types and parameter names
 * Batch 3 - Item 44: Inlay hint provider
 */

#include <string>
#include <vector>
#include <optional>

namespace RawrXD::LSP {

enum class InlayHintKind {
    Type = 1,
    Parameter = 2
};

struct InlayHintLabelPart {
    std::string value;
    std::optional<std::string> tooltip;
    std::optional<std::string> location;
    std::optional<std::string> command;
};

struct InlayHint {
    uint32_t line;
    uint32_t column;
    std::vector<InlayHintLabelPart> labelParts;
    InlayHintKind kind;
    std::optional<std::string> tooltip;
    bool paddingLeft;
    bool paddingRight;
    std::optional<std::string> data;
};

struct InlayHintOptions {
    bool showTypeHints = true;
    bool showParameterHints = true;
    bool showChainingHints = true;
    bool showReturnTypeHints = true;
};

class InlayHintProvider {
public:
    InlayHintProvider();
    ~InlayHintProvider();

    // Configuration
    void setOptions(const InlayHintOptions& options);
    InlayHintOptions getOptions() const;

    // Inlay hints
    std::vector<InlayHint> provideInlayHints(const std::string& uri,
                                              const std::string& content,
                                              uint32_t startLine,
                                              uint32_t endLine);

    // Resolve
    std::optional<InlayHint> resolveInlayHint(const InlayHint& hint);

    // C++ specific
    std::vector<InlayHint> getCppInlayHints(const std::string& uri,
                                             const std::string& content,
                                             uint32_t startLine,
                                             uint32_t endLine);

    // Type inference
    std::optional<std::string> inferType(const std::string& content,
                                        uint32_t line,
                                        uint32_t column);

    // Parameter hints
    std::vector<InlayHint> getParameterHints(const std::string& content,
                                             uint32_t line,
                                             uint32_t column);

private:
    InlayHintOptions m_options;
    mutable std::mutex m_mutex;

    std::vector<InlayHint> getTypeHints(const std::string& content,
                                       uint32_t startLine,
                                       uint32_t endLine);
    std::vector<InlayHint> getChainingHints(const std::string& content,
                                           uint32_t startLine,
                                           uint32_t endLine);
    std::vector<InlayHint> getReturnTypeHints(const std::string& content,
                                              uint32_t startLine,
                                              uint32_t endLine);

    bool isVariableDeclaration(const std::string& line);
    bool isAutoVariable(const std::string& line);
    bool isFunctionCall(const std::string& line, uint32_t column);
    std::string extractFunctionName(const std::string& line, uint32_t column);
    std::vector<std::string> getFunctionParameters(const std::string& functionName);
};

// Global provider
InlayHintProvider& getInlayHintProvider();

} // namespace RawrXD::LSP
