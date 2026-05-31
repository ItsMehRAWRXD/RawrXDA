// rust_parser.hpp / rust_parser.cpp
// Lightweight pattern-based Rust parser for AST Graph Engine.
// No external dependencies. Extracts declarations, scopes, and symbols.
// Integrates with ASTGraphEngine::registerFile / updateFile.

#pragma once
#include "rust_ast_nodes.hpp"
#include "ast_graph_engine.hpp"
#include "symbol_table.hpp"
#include <string_view>
#include <vector>
#include <optional>
#include <functional>

namespace rawrxd::ast::rust {

using RawrXD::AST::ASTNode;
using RawrXD::AST::SourceLocation;
using RawrXD::AST::SourceRange;
using RawrXD::AST::NodeType;
using RawrXD::AST::ASTGraphEngine;

// Parse result: AST nodes + diagnostics
struct RustParseResult {
    std::vector<ASTNode::Ptr> nodes;
    std::vector<std::string> diagnostics;
    bool success{false};
};

// Lightweight Rust parser (pattern-based, not full grammar)
class RustParser {
public:
    RustParser() = default;

    // Parse entire file content into AST nodes
    RustParseResult parse(std::string_view content, std::string_view file_path);

    // Parse with symbol table population (indexing layer)
    RustParseResult parse(std::string_view content, std::string_view file_path,
                          rawrxd::ast::SymbolTable* symbols);

    // Incremental: re-parse only changed region
    RustParseResult parseIncremental(std::string_view content,
                                     const std::vector<ASTNode::Ptr>& old_nodes,
                                     size_t change_start, size_t change_end);

private:
    struct Token {
        enum Type { Ident, Keyword, Symbol, String, Number, Lifetime, Comment, DocComment, Whitespace, Eof };
        Type type{Eof};
        std::string_view text;
        size_t offset{0};
        size_t line{0};
        size_t column{0};
    };

    std::vector<Token> tokenize(std::string_view content);
    ASTNode::Ptr parseItem(const std::vector<Token>& tokens, size_t& pos);
    ASTNode::Ptr parseFunction(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseStruct(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseEnum(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseTrait(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseImpl(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseUse(const std::vector<Token>& tokens, size_t& pos);
    ASTNode::Ptr parseMod(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseTypeAlias(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseConstOrStatic(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta);
    ASTNode::Ptr parseLet(const std::vector<Token>& tokens, size_t& pos);
    ASTNode::Ptr parseBlock(const std::vector<Token>& tokens, size_t& pos);

    RustSymbolMeta parseAttributesAndVisibility(const std::vector<Token>& tokens, size_t& pos);
    void skipWhitespaceAndComments(const std::vector<Token>& tokens, size_t& pos);
    void skipWhitespaceCommentsAndDoc(const std::vector<Token>& tokens, size_t& pos);
    bool peek(const std::vector<Token>& tokens, size_t pos, std::string_view text);
    bool consume(const std::vector<Token>& tokens, size_t& pos, std::string_view text);

    static SourceLocation tokenLoc(const Token& t, uint32_t file_id);
    static SourceRange tokenRange(const Token& start, const Token& end, uint32_t file_id);

    uint32_t file_id_{0};
    std::string file_path_;
};

// Integration helper: register Rust file with ASTGraphEngine
void registerRustFile(ASTGraphEngine& engine, const std::string& path, const std::string& content);
void updateRustFile(ASTGraphEngine& engine, const std::string& path, const std::string& content);

} // namespace rawrxd::ast::rust
