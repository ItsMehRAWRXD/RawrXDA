// ============================================================================
// treesitter_parser.h — Phase 3: AST-Based Symbol Extraction
// ============================================================================
// Lightweight recursive-descent parser providing Tree-sitter-like AST nodes
// for C/C++, Python, JavaScript/TypeScript, and Rust. Falls back to regex
// for unsupported languages. Parse trees are cached for incremental updates.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp/workspace_symbol_index.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace RawrXD::LSP {

// ---------------------------------------------------------------------------
// AST Node Types
// ---------------------------------------------------------------------------
enum class ASTNodeKind : uint8_t {
    Root = 0,
    FunctionDecl = 1,
    FunctionDef = 2,
    ClassDecl = 3,
    StructDecl = 4,
    EnumDecl = 5,
    VariableDecl = 6,
    Parameter = 7,
    Import = 8,
    Namespace = 9,
    Block = 10,
    Expression = 11,
    Statement = 12,
    TypeRef = 13,
    Comment = 14,
    Unknown = 15,
};

// ---------------------------------------------------------------------------
// AST Node
// ---------------------------------------------------------------------------
struct ASTNode {
    ASTNodeKind kind = ASTNodeKind::Unknown;
    std::string text;
    std::string name;           // Symbol name (for declarations)
    std::string typeName;       // Type annotation
    uint32_t startLine = 0;
    uint32_t startCol = 0;
    uint32_t endLine = 0;
    uint32_t endCol = 0;
    uint32_t scopeDepth = 0;
    std::vector<std::shared_ptr<ASTNode>> children;
    std::weak_ptr<ASTNode> parent;

    bool isDeclaration() const {
        return kind == ASTNodeKind::FunctionDecl ||
               kind == ASTNodeKind::FunctionDef ||
               kind == ASTNodeKind::ClassDecl ||
               kind == ASTNodeKind::StructDecl ||
               kind == ASTNodeKind::EnumDecl ||
               kind == ASTNodeKind::VariableDecl ||
               kind == ASTNodeKind::Parameter;
    }

    bool isScope() const {
        return kind == ASTNodeKind::FunctionDef ||
               kind == ASTNodeKind::ClassDecl ||
               kind == ASTNodeKind::StructDecl ||
               kind == ASTNodeKind::Namespace ||
               kind == ASTNodeKind::Block;
    }
};

// ---------------------------------------------------------------------------
// Parse Tree Cache Entry
// ---------------------------------------------------------------------------
struct ParseTreeCacheEntry {
    std::shared_ptr<ASTNode> root;
    uint64_t contentHash = 0;
    uint32_t version = 0;
    int64_t timestampMs = 0;
    std::string languageId;
};

// ---------------------------------------------------------------------------
// Language ID
// ---------------------------------------------------------------------------
enum class LanguageId : uint8_t {
    Unknown = 0,
    C = 1,
    Cpp = 2,
    Python = 3,
    JavaScript = 4,
    TypeScript = 5,
    Rust = 6,
    Go = 7,
    Java = 8,
    CSharp = 9,
};

// ---------------------------------------------------------------------------
// TreeSitterParser — Main AST Parser Interface
// ---------------------------------------------------------------------------
class TreeSitterParser {
public:
    TreeSitterParser();
    ~TreeSitterParser();

    // Parse source into AST (language auto-detected if not specified)
    std::shared_ptr<ASTNode> parse(const std::string& uri,
                                    const std::string& content,
                                    LanguageId lang = LanguageId::Unknown);

    // Incremental parse: reuse existing tree where unchanged
    std::shared_ptr<ASTNode> parseIncremental(const std::string& uri,
                                               const std::string& oldContent,
                                               const std::string& newContent,
                                               uint32_t changeStartLine,
                                               uint32_t changeEndLine);

    // Extract symbols from AST
    std::vector<SymbolInfo> extractSymbols(const std::shared_ptr<ASTNode>& root,
                                            const std::string& uri);

    // Find node at position
    std::shared_ptr<ASTNode> nodeAtPosition(const std::shared_ptr<ASTNode>& root,
                                             uint32_t line,
                                             uint32_t column);

    // Find all references to a symbol within AST
    std::vector<Location> findReferences(const std::shared_ptr<ASTNode>& root,
                                           const std::string& symbolName);

    // Scope analysis: find enclosing scope at position
    std::shared_ptr<ASTNode> enclosingScope(const std::shared_ptr<ASTNode>& root,
                                            uint32_t line,
                                            uint32_t column);

    // Cache management
    void invalidateCache(const std::string& uri);
    void clearCache();
    size_t cacheSize() const;

    // Language detection
    static LanguageId detectLanguage(const std::string& uri,
                                      const std::string& content = "");

    // Metrics
    struct ParseMetrics {
        double avgParseTimeMs = 0.0;
        size_t totalParses = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
    };
    ParseMetrics getMetrics() const;

private:
    std::unordered_map<std::string, ParseTreeCacheEntry> m_cache;
    mutable std::shared_mutex m_cacheMutex;

    ParseMetrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Language-specific parsers
    std::shared_ptr<ASTNode> parseCpp(const std::string& content);
    std::shared_ptr<ASTNode> parsePython(const std::string& content);
    std::shared_ptr<ASTNode> parseJavaScript(const std::string& content);
    std::shared_ptr<ASTNode> parseTypeScript(const std::string& content);
    std::shared_ptr<ASTNode> parseRust(const std::string& content);
    std::shared_ptr<ASTNode> parseGo(const std::string& content);
    std::shared_ptr<ASTNode> parseJava(const std::string& content);
    std::shared_ptr<ASTNode> parseCSharp(const std::string& content);

    // Recursive descent helpers
    struct Token {
        enum Type { Identifier, Keyword, Number, String, Operator,
                    Punctuation, Comment, Whitespace, End } type;
        std::string text;
        uint32_t line = 0;
        uint32_t col = 0;
    };

    class TokenStream {
    public:
        explicit TokenStream(const std::string& content);
        Token next();
        Token peek() const;
        bool eof() const;
        void skipWhitespace();
        void skipComments();
    private:
        std::string m_content;
        size_t m_pos = 0;
        uint32_t m_line = 0;
        uint32_t m_col = 0;
        mutable Token m_peek;
        mutable bool m_hasPeek = false;
        Token readToken();
    };

    // C++ recursive descent
    std::shared_ptr<ASTNode> parseCppTranslationUnit(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppDeclaration(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppFunction(TokenStream& ts, bool isDef);
    std::shared_ptr<ASTNode> parseCppClass(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppStruct(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppEnum(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppVariable(TokenStream& ts);
    std::shared_ptr<ASTNode> parseCppNamespace(TokenStream& ts);

    // Python recursive descent
    std::shared_ptr<ASTNode> parsePythonModule(TokenStream& ts);
    std::shared_ptr<ASTNode> parsePythonFunction(TokenStream& ts);
    std::shared_ptr<ASTNode> parsePythonClass(TokenStream& ts);
    std::shared_ptr<ASTNode> parsePythonImport(TokenStream& ts);
    std::shared_ptr<ASTNode> parsePythonVariable(TokenStream& ts);

    // Generic helpers
    static uint64_t hashContent(const std::string& content);
    void updateMetrics(double parseTimeMs, bool cacheHit);
    void collectSymbolsRecursive(const std::shared_ptr<ASTNode>& node,
                                  const std::string& uri,
                                  std::vector<SymbolInfo>& out);
    void collectReferencesRecursive(const std::shared_ptr<ASTNode>& node,
                                     const std::string& symbolName,
                                     std::vector<Location>& out);
};

} // namespace RawrXD::LSP
