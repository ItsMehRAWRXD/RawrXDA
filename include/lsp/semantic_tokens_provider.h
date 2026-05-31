#pragma once
/**
 * @file semantic_tokens_provider.h
 * @brief Semantic token highlighting
 * Batch 3 - Item 45: Semantic tokens provider
 */

#include <string>
#include <vector>
#include <map>

namespace RawrXD::LSP {

// Token types (LSP 3.17)
enum class TokenType {
    Namespace = 0,
    Type = 1,
    Class = 2,
    Enum = 3,
    Interface = 4,
    Struct = 5,
    TypeParameter = 6,
    Parameter = 7,
    Variable = 8,
    Property = 9,
    EnumMember = 10,
    Event = 11,
    Function = 12,
    Method = 13,
    Macro = 14,
    Keyword = 15,
    Modifier = 16,
    Comment = 17,
    String = 18,
    Number = 19,
    Regexp = 20,
    Operator = 21
};

// Token modifiers
enum class TokenModifier {
    Declaration = 0,
    Definition = 1,
    Readonly = 2,
    Static = 3,
    Deprecated = 4,
    Abstract = 5,
    Async = 6,
    Modification = 7,
    Documentation = 8,
    DefaultLibrary = 9
};

struct SemanticToken {
    uint32_t line;
    uint32_t startColumn;
    uint32_t length;
    TokenType tokenType;
    uint32_t tokenModifiers;
};

struct SemanticTokens {
    std::vector<uint32_t> data; // Encoded as LSP delta
    std::optional<std::string> resultId;
};

struct SemanticTokensLegend {
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

struct SemanticTokensRange {
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
};

class SemanticTokensProvider {
public:
    SemanticTokensProvider();
    ~SemanticTokensProvider();

    // Legend
    SemanticTokensLegend getLegend() const;

    // Full document
    SemanticTokens provideSemanticTokens(const std::string& uri,
                                          const std::string& content);

    // Range
    SemanticTokens provideSemanticTokensRange(const std::string& uri,
                                               const std::string& content,
                                               const SemanticTokensRange& range);

    // Delta
    SemanticTokens provideSemanticTokensDelta(const std::string& uri,
                                               const std::string& content,
                                               const std::string& previousResultId);

    // Refresh
    void refresh();

    // C++ specific
    SemanticTokens getCppSemanticTokens(const std::string& uri,
                                        const std::string& content);

private:
    mutable std::mutex m_mutex;
    std::map<std::string, std::string> m_resultIds;
    uint32_t m_resultIdCounter{0};

    std::vector<SemanticToken> parseTokens(const std::string& content);
    std::vector<uint32_t> encodeTokens(const std::vector<SemanticToken>& tokens);
    std::vector<uint32_t> encodeDelta(const std::vector<SemanticToken>& oldTokens,
                                        const std::vector<SemanticToken>& newTokens);

    TokenType classifyToken(const std::string& word,
                            const std::string& context,
                            uint32_t line);
    uint32_t calculateModifiers(const std::string& word,
                                 const std::string& context,
                                 uint32_t line);

    bool isKeyword(const std::string& word);
    bool isType(const std::string& word);
    bool isFunction(const std::string& word, const std::string& context);
    bool isVariable(const std::string& word, const std::string& context);
    bool isDeclaration(const std::string& line, const std::string& word);
    bool isDefinition(const std::string& line, const std::string& word);
    bool isStatic(const std::string& line);
    bool isConst(const std::string& line);
};

// Global provider
SemanticTokensProvider& getSemanticTokensProvider();

// Utility
std::string tokenTypeToString(TokenType type);
std::string tokenModifierToString(TokenModifier modifier);

} // namespace RawrXD::LSP
