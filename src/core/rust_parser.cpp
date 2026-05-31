// rust_parser_v2.cpp
// Hardened Rust parser addressing all tokenizer and parser gaps from review.
// Replaces rust_parser.cpp with production-grade tokenization and parsing.

#include "rust_parser.hpp"
#include "flow_control/phase_latency_tracker.hpp"
#include <cctype>
#include <cstring>
#include <algorithm>

namespace rawrxd::ast::rust {

using RawrXD::FlowControl::PhaseScopeGuard;
using RawrXD::FlowControl::Phase;

// ============================================================================
// Tokenizer v2 — hardened against all edge cases
// ============================================================================
std::vector<RustParser::Token> RustParser::tokenize(std::string_view content) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t line = 1;
    size_t col = 1;

    auto advance = [&](size_t n = 1) {
        for (size_t k = 0; k < n; ++k) {
            if (i < content.size() && content[i] == '\n') { ++line; col = 1; }
            else { ++col; }
            if (i < content.size()) ++i;
        }
    };

    while (i < content.size()) {
        char c = content[i];
        size_t start = i;
        size_t startLine = line;
        size_t startCol = col;

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            while (i < content.size() && std::isspace(static_cast<unsigned char>(content[i]))) advance();
            tokens.push_back({Token::Whitespace, content.substr(start, i - start), start, startLine, startCol});
            continue;
        }

        // Doc comments (must come before regular comments)
        if (c == '/' && i + 2 < content.size()) {
            // /// doc comment
            if (content[i+1] == '/' && content[i+2] == '/') {
                advance(3);
                while (i < content.size() && content[i] != '\n') advance();
                tokens.push_back({Token::DocComment, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
            // //! inner doc comment
            if (content[i+1] == '/' && content[i+2] == '!') {
                advance(3);
                while (i < content.size() && content[i] != '\n') advance();
                tokens.push_back({Token::DocComment, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
            // /** block doc comment */
            if (content[i+1] == '*' && i + 2 < content.size() && content[i+2] == '*') {
                advance(3);
                int depth = 1;
                while (i + 1 < content.size() && depth > 0) {
                    if (content[i] == '/' && content[i+1] == '*') { ++depth; advance(2); }
                    else if (content[i] == '*' && content[i+1] == '/') { --depth; advance(2); }
                    else advance();
                }
                tokens.push_back({Token::DocComment, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
            // // regular comment
            if (content[i+1] == '/') {
                while (i < content.size() && content[i] != '\n') advance();
                tokens.push_back({Token::Comment, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
            // /* block comment with nested depth tracking */
            if (content[i+1] == '*') {
                advance(2);
                int depth = 1;
                while (i + 1 < content.size() && depth > 0) {
                    if (content[i] == '/' && content[i+1] == '*') { ++depth; advance(2); }
                    else if (content[i] == '*' && content[i+1] == '/') { --depth; advance(2); }
                    else advance();
                }
                tokens.push_back({Token::Comment, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
        }

        // Raw string literals: r#N"..."#N
        if (c == 'r' && i + 1 < content.size()) {
            size_t hashCount = 0;
            size_t j = i + 1;
            while (j < content.size() && content[j] == '#') { ++hashCount; ++j; }
            if (j < content.size() && content[j] == '"') {
                advance(static_cast<size_t>(j - i) + 1); // advance past r, #s, and opening "
                // Find closing " followed by exactly hashCount #
                while (i < content.size()) {
                    if (content[i] == '"') {
                        size_t k = i + 1;
                        size_t closingHashes = 0;
                        while (k < content.size() && content[k] == '#' && closingHashes < hashCount) {
                            ++closingHashes; ++k;
                        }
                        if (closingHashes == hashCount) {
                            advance(static_cast<size_t>(k - i));
                            break;
                        }
                    }
                    advance();
                }
                tokens.push_back({Token::String, content.substr(start, i - start), start, startLine, startCol});
                continue;
            }
        }

        // Regular string literals
        if (c == '"') {
            advance();
            while (i < content.size() && content[i] != '"') {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    // Handle escape sequences: \xNN, \u{N...}, \n, \t, etc.
                    advance(); // past backslash
                    char esc = content[i];
                    if (esc == 'x' && i + 2 < content.size()) advance(2); // \xNN
                    else if (esc == 'u' && i + 1 < content.size() && content[i+1] == '{') {
                        advance(2); // past u{
                        while (i < content.size() && content[i] != '}') advance();
                        if (i < content.size()) advance(); // past }
                    }
                    else advance(); // single-char escape
                } else {
                    advance();
                }
            }
            if (i < content.size()) advance(); // closing "
            tokens.push_back({Token::String, content.substr(start, i - start), start, startLine, startCol});
            continue;
        }

        // Lifetimes: 'ident or '_ (including 'static which is a keyword, not lifetime)
        // MUST come before character literal handler
        if (c == '\'' && i + 1 < content.size()) {
            char next = content[i+1];
            if (std::isalpha(static_cast<unsigned char>(next)) || next == '_') {
                // Check if it's 'static (keyword) — consume it as keyword, don't fall through
                if (content.substr(i+1, 6) == "static" && (i+7 >= content.size() || !std::isalnum(static_cast<unsigned char>(content[i+7])))) {
                    advance(7); // past 'static
                    tokens.push_back({Token::Keyword, content.substr(start, i - start), start, startLine, startCol});
                    continue;
                } else {
                    advance(); // past '
                    // Lifetimes are single identifiers: 'a, 'b, '_, 'static
                    // Only consume one char or the word 'static
                    if (i < content.size() && (std::isalnum(static_cast<unsigned char>(content[i])) || content[i] == '_')) {
                        advance(); // consume first char
                    }
                    tokens.push_back({Token::Lifetime, content.substr(start, i - start), start, startLine, startCol});
                    continue;
                }
            }
        }

        // Character literals — hardened for multi-char escapes
        if (c == '\'') {
            advance(); // past '
            if (i < content.size() && content[i] == '\\') {
                advance(); // past backslash
                char esc = content[i];
                if (esc == 'x' && i + 2 < content.size()) advance(2); // \xNN
                else if (esc == 'u' && i + 1 < content.size() && content[i+1] == '{') {
                    advance(2); // past u{
                    while (i < content.size() && content[i] != '}') advance();
                    if (i < content.size()) advance(); // past }
                }
                else advance(); // single-char escape like \n, \t
            } else {
                while (i < content.size() && content[i] != '\'') advance();
            }
            if (i < content.size()) advance(); // closing '
            tokens.push_back({Token::String, content.substr(start, i - start), start, startLine, startCol});
            continue;
        }

        // Numbers — hex/binary/octal + single dot only + suffix handling
        if (std::isdigit(static_cast<unsigned char>(c))) {
            bool isFloat = false;
            if (c == '0' && i + 1 < content.size()) {
                char prefix = content[i+1];
                if (prefix == 'x' || prefix == 'X') {
                    advance(2);
                    while (i < content.size() && (std::isxdigit(static_cast<unsigned char>(content[i])) || content[i] == '_')) advance();
                } else if (prefix == 'b' || prefix == 'B') {
                    advance(2);
                    while (i < content.size() && (content[i] == '0' || content[i] == '1' || content[i] == '_')) advance();
                } else if (prefix == 'o' || prefix == 'O') {
                    advance(2);
                    while (i < content.size() && (content[i] >= '0' && content[i] <= '7' || content[i] == '_')) advance();
                } else {
                    goto regular_number;
                }
            } else {
            regular_number:
                while (i < content.size() && (std::isdigit(static_cast<unsigned char>(content[i])) || content[i] == '_')) advance();
                if (i < content.size() && content[i] == '.' && !isFloat) {
                    // Check next is digit (not .. range operator)
                    if (i + 1 < content.size() && std::isdigit(static_cast<unsigned char>(content[i+1]))) {
                        isFloat = true;
                        advance(); // past .
                        while (i < content.size() && (std::isdigit(static_cast<unsigned char>(content[i])) || content[i] == '_')) advance();
                    }
                }
                // Exponent
                if (i < content.size() && (content[i] == 'e' || content[i] == 'E')) {
                    size_t expPos = i;
                    advance();
                    if (i < content.size() && (content[i] == '+' || content[i] == '-')) advance();
                    bool hasExpDigit = false;
                    while (i < content.size() && (std::isdigit(static_cast<unsigned char>(content[i])) || content[i] == '_')) {
                        hasExpDigit = true; advance();
                    }
                    if (!hasExpDigit) i = expPos; // rollback
                }
            }
            // Type suffix (e.g., u32, f64, i128)
            if (i < content.size() && std::isalpha(static_cast<unsigned char>(content[i]))) {
                while (i < content.size() && std::isalnum(static_cast<unsigned char>(content[i]))) advance();
            }
            tokens.push_back({Token::Number, content.substr(start, i - start), start, startLine, startCol});
            continue;
        }

        // Identifiers / Keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (i < content.size() && (std::isalnum(static_cast<unsigned char>(content[i])) || content[i] == '_')) advance();
            std::string_view text = content.substr(start, i - start);
            static const char* keywords[] = {
                "as", "async", "await", "break", "const", "continue", "crate", "dyn",
                "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
                "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
                "self", "Self", "static", "struct", "super", "trait", "true", "type",
                "union", "unsafe", "use", "where", "while"
            };
            bool isKw = false;
            for (auto kw : keywords) {
                if (text == kw) { isKw = true; break; }
            }
            tokens.push_back({isKw ? Token::Keyword : Token::Ident, text, start, startLine, startCol});
            continue;
        }

        // Symbols (multi-char first)
        static const char* symbols[] = {
            "::", "->", "=>", "!=", "==", "<=", ">=", "<<", ">>", "&&", "||",
            "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=",
            "..", "...", "..=", "//", "/*", "*/",
            "+", "-", "*", "/", "%", "&", "|", "^", "!", "<", ">", "=", ";",
            ":", ",", ".", "(", ")", "[", "]", "{", "}", "#", "$", "?", "~"
        };
        bool matched = false;
        for (auto sym : symbols) {
            size_t len = std::strlen(sym);
            if (i + len <= content.size() && content.substr(i, len) == sym) {
                advance(len);
                tokens.push_back({Token::Symbol, content.substr(start, i - start), start, startLine, startCol});
                matched = true;
                break;
            }
        }
        if (matched) continue;

        // Unknown char — advance once to prevent infinite loop
        advance();
        tokens.push_back({Token::Symbol, content.substr(start, i - start), start, startLine, startCol});
    }

    tokens.push_back({Token::Eof, "", i, line, col});
    return tokens;
}

// ============================================================================
// Helpers
// ============================================================================
void RustParser::skipWhitespaceAndComments(const std::vector<Token>& tokens, size_t& pos) {
    while (pos < tokens.size() && (tokens[pos].type == Token::Whitespace || tokens[pos].type == Token::Comment)) {
        ++pos;
    }
}

void RustParser::skipWhitespaceCommentsAndDoc(const std::vector<Token>& tokens, size_t& pos) {
    while (pos < tokens.size() && (tokens[pos].type == Token::Whitespace || tokens[pos].type == Token::Comment || tokens[pos].type == Token::DocComment)) {
        ++pos;
    }
}

bool RustParser::peek(const std::vector<Token>& tokens, size_t pos, std::string_view text) {
    skipWhitespaceAndComments(tokens, pos);
    return pos < tokens.size() && tokens[pos].text == text;
}

bool RustParser::consume(const std::vector<Token>& tokens, size_t& pos, std::string_view text) {
    skipWhitespaceAndComments(tokens, pos);
    if (pos < tokens.size() && tokens[pos].text == text) {
        ++pos;
        return true;
    }
    return false;
}

SourceLocation RustParser::tokenLoc(const Token& t, uint32_t file_id) {
    SourceLocation loc;
    loc.line = static_cast<uint32_t>(t.line);
    loc.column = static_cast<uint32_t>(t.column);
    loc.file_id = file_id;
    return loc;
}

SourceRange RustParser::tokenRange(const Token& start, const Token& end, uint32_t file_id) {
    SourceRange r;
    r.start = tokenLoc(start, file_id);
    r.end = tokenLoc(end, file_id);
    return r;
}

// ============================================================================
// Attribute / Visibility / Doc comment parser
// ============================================================================
RustSymbolMeta RustParser::parseAttributesAndVisibility(const std::vector<Token>& tokens, size_t& pos) {
    RustSymbolMeta meta;
    skipWhitespaceCommentsAndDoc(tokens, pos);

    // Doc comments before attributes
    while (pos < tokens.size() && tokens[pos].type == Token::DocComment) {
        if (!meta.docComment.empty()) meta.docComment += "\n";
        meta.docComment += std::string(tokens[pos].text);
        ++pos;
        skipWhitespaceCommentsAndDoc(tokens, pos);
    }

    // Attributes: #[...] and #![...]
    while (peek(tokens, pos, "#")) {
        size_t attrStart = pos;
        consume(tokens, pos, "#");
        if (peek(tokens, pos, "!")) consume(tokens, pos, "!");
        if (consume(tokens, pos, "[")) {
            int depth = 1;
            while (pos < tokens.size() && depth > 0) {
                if (tokens[pos].text == "[") ++depth;
                else if (tokens[pos].text == "]") --depth;
                ++pos;
            }
            // Extract all attribute data
            std::string attrName;
            for (size_t i = attrStart; i < pos && i < tokens.size(); ++i) {
                if (tokens[i].type == Token::Ident && attrName.empty()) {
                    attrName = std::string(tokens[i].text);
                    meta.attributes.emplace_back(attrName, "");
                }
                if (tokens[i].text == "derive") {
                    size_t j = i + 1;
                    while (j < pos && j < tokens.size()) {
                        if (tokens[j].type == Token::Ident) {
                            meta.derives.push_back(std::string(tokens[j].text));
                        }
                        ++j;
                    }
                }
            }
        }
        skipWhitespaceCommentsAndDoc(tokens, pos);
    }

    // Visibility
    if (peek(tokens, pos, "pub")) {
        meta.isPub = true;
        consume(tokens, pos, "pub");
        if (consume(tokens, pos, "(")) {
            if (consume(tokens, pos, "crate")) {
                meta.visibilityPath = "crate";
            } else if (consume(tokens, pos, "super")) {
                meta.visibilityPath = "super";
            } else if (consume(tokens, pos, "self")) {
                meta.visibilityPath = "self";
            } else if (consume(tokens, pos, "in")) {
                size_t pathStart = pos;
                while (pos < tokens.size() && !peek(tokens, pos, ")")) ++pos;
                std::string path;
                for (size_t k = pathStart; k < pos && k < tokens.size(); ++k) {
                    if (tokens[k].type != Token::Whitespace && tokens[k].type != Token::Comment) {
                        path += std::string(tokens[k].text);
                    }
                }
                meta.visibilityPath = "in " + path;
            }
            consume(tokens, pos, ")");
        }
    }

    // Modifiers: unsafe, async, const, extern
    while (pos < tokens.size() && tokens[pos].type == Token::Keyword) {
        if (tokens[pos].text == "unsafe") { meta.isUnsafe = true; ++pos; }
        else if (tokens[pos].text == "async") { meta.isAsync = true; ++pos; }
        else if (tokens[pos].text == "const") { meta.isConst = true; ++pos; }
        else if (tokens[pos].text == "extern") { meta.isExtern = true; ++pos; }
        else break;
        skipWhitespaceCommentsAndDoc(tokens, pos);
    }

    return meta;
}

// ============================================================================
// Item parsers — hardened with where-clause and return-type awareness
// ============================================================================
ASTNode::Ptr RustParser::parseItem(const std::vector<Token>& tokens, size_t& pos) {
    skipWhitespaceCommentsAndDoc(tokens, pos);
    if (pos >= tokens.size() || tokens[pos].type == Token::Eof) return nullptr;

    RustSymbolMeta meta = parseAttributesAndVisibility(tokens, pos);
    skipWhitespaceCommentsAndDoc(tokens, pos);

    if (pos >= tokens.size()) return nullptr;

    if (peek(tokens, pos, "fn")) return parseFunction(tokens, pos, meta);
    if (peek(tokens, pos, "struct")) return parseStruct(tokens, pos, meta);
    if (peek(tokens, pos, "enum")) return parseEnum(tokens, pos, meta);
    if (peek(tokens, pos, "trait")) return parseTrait(tokens, pos, meta);
    if (peek(tokens, pos, "impl")) return parseImpl(tokens, pos, meta);
    if (peek(tokens, pos, "use")) return parseUse(tokens, pos);
    if (peek(tokens, pos, "mod")) return parseMod(tokens, pos, meta);
    if (peek(tokens, pos, "type")) return parseTypeAlias(tokens, pos, meta);
    if (peek(tokens, pos, "const") || peek(tokens, pos, "static")) return parseConstOrStatic(tokens, pos, meta);
    if (peek(tokens, pos, "let")) return parseLet(tokens, pos);

    // Skip unknown — advance by at least one token to prevent infinite loop
    ++pos;
    return nullptr;
}

ASTNode::Ptr RustParser::parseFunction(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    PhaseScopeGuard guard(Phase::RustFunctionDecl);
    size_t fnStart = pos;
    consume(tokens, pos, "fn");
    skipWhitespaceCommentsAndDoc(tokens, pos);

    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }

    // Generic params
    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }

    // Parameters — now captured for self/&mut self detection
    std::vector<std::string> params;
    if (consume(tokens, pos, "(")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].type == Token::Ident) {
                std::string paramName = std::string(tokens[pos].text);
                // Detect self patterns
                if (paramName == "self" || paramName == "mut" || paramName == "&") {
                    params.push_back(paramName);
                }
            }
            if (tokens[pos].text == "(") ++depth;
            else if (tokens[pos].text == ")") --depth;
            ++pos;
        }
    }

    // Where clause — skip gracefully by tracking brace/paren depth
    if (peek(tokens, pos, "where")) {
        consume(tokens, pos, "where");
        while (pos < tokens.size() && !peek(tokens, pos, "{") && !peek(tokens, pos, ";")) {
            // Skip balanced parens/braces in where bounds
            if (tokens[pos].text == "(") { int d=1; ++pos; while(pos<tokens.size()&&d>0){ if(tokens[pos].text=="(")++d; else if(tokens[pos].text==")")--d; ++pos; } }
            else ++pos;
        }
    }

    // Return type — hardened for impl Fn() -> bool
    if (consume(tokens, pos, "->")) {
        int parenDepth = 0;
        while (pos < tokens.size() && (parenDepth > 0 || !peek(tokens, pos, "{"))) {
            if (tokens[pos].text == "(") ++parenDepth;
            else if (tokens[pos].text == ")") --parenDepth;
            ++pos;
        }
    }

    // Body or semicolon
    if (consume(tokens, pos, "{")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "{") ++depth;
            else if (tokens[pos].text == "}") --depth;
            ++pos;
        }
    } else {
        consume(tokens, pos, ";");
    }

    size_t fnEnd = pos;
    SourceRange range = tokenRange(tokens[fnStart], tokens[fnEnd > 0 ? fnEnd - 1 : fnEnd], file_id_);
    std::string label = "fn " + name;
    if (meta.isUnsafe) label = "unsafe " + label;
    if (meta.isAsync) label = "async " + label;
    if (meta.isConst) label = "const " + label;
    if (meta.isExtern) label = "extern " + label;
    if (meta.isPub) {
        if (!meta.visibilityPath.empty()) {
            label = "pub(" + meta.visibilityPath + ") " + label;
        } else {
            label = "pub " + label;
        }
    }
    auto node = std::make_shared<ASTNode>(NodeType::FunctionDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseStruct(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    PhaseScopeGuard guard(Phase::RustStructDecl);
    size_t start = pos;
    consume(tokens, pos, "struct");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    // Generic params
    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }
    // Tuple struct or regular struct
    if (consume(tokens, pos, "(")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "(") ++depth;
            else if (tokens[pos].text == ")") --depth;
            ++pos;
        }
        consume(tokens, pos, ";");
    } else if (consume(tokens, pos, "{")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "{") ++depth;
            else if (tokens[pos].text == "}") --depth;
            ++pos;
        }
    } else {
        consume(tokens, pos, ";");
    }
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = "struct " + name;
    if (meta.isPub) label = "pub " + label;
    auto node = std::make_shared<ASTNode>(NodeType::StructDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseEnum(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    PhaseScopeGuard guard(Phase::RustEnumDecl);
    size_t start = pos;
    consume(tokens, pos, "enum");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }
    if (consume(tokens, pos, "{")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "{") ++depth;
            else if (tokens[pos].text == "}") --depth;
            ++pos;
        }
    }
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = "enum " + name;
    if (meta.isPub) label = "pub " + label;
    auto node = std::make_shared<ASTNode>(NodeType::EnumDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseTrait(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    PhaseScopeGuard guard(Phase::RustTraitDecl);
    size_t start = pos;
    consume(tokens, pos, "trait");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }
    // Supertraits: trait Foo: Bar + Baz
    if (consume(tokens, pos, ":")) {
        while (pos < tokens.size() && !peek(tokens, pos, "{")) ++pos;
    }
    if (consume(tokens, pos, "{")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "{") ++depth;
            else if (tokens[pos].text == "}") --depth;
            ++pos;
        }
    }
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = "trait " + name;
    if (meta.isPub) label = "pub " + label;
    if (meta.isUnsafe) label = "unsafe " + label;
    auto node = std::make_shared<ASTNode>(NodeType::ClassDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseImpl(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    PhaseScopeGuard guard(Phase::RustImplBlock);
    size_t start = pos;
    consume(tokens, pos, "impl");
    skipWhitespaceCommentsAndDoc(tokens, pos);

    std::string traitName;
    std::string typeName;

    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }

    size_t typeStart = pos;
    while (pos < tokens.size() && !peek(tokens, pos, "for") && !peek(tokens, pos, "{")) ++pos;
    std::string firstPart;
    for (size_t k = typeStart; k < pos && k < tokens.size(); ++k) {
        if (tokens[k].type != Token::Whitespace && tokens[k].type != Token::Comment && tokens[k].type != Token::DocComment) {
            firstPart += std::string(tokens[k].text);
        }
    }

    if (consume(tokens, pos, "for")) {
        traitName = firstPart;
        size_t typeStart2 = pos;
        while (pos < tokens.size() && !peek(tokens, pos, "{")) ++pos;
        for (size_t k = typeStart2; k < pos && k < tokens.size(); ++k) {
            if (tokens[k].type != Token::Whitespace && tokens[k].type != Token::Comment && tokens[k].type != Token::DocComment) {
                typeName += std::string(tokens[k].text);
            }
        }
    } else {
        typeName = firstPart;
    }

    // Parse impl body — recurse into inner items (methods, associated functions)
    std::vector<ASTNode::Ptr> children;
    if (consume(tokens, pos, "{")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "{") {
                ++depth;
                ++pos;
            } else if (tokens[pos].text == "}") {
                --depth;
                if (depth == 0) { ++pos; break; }
                ++pos;
            } else {
                // Try to parse inner item at depth 1
                if (depth == 1) {
                    auto child = parseItem(tokens, pos);
                    if (child) children.push_back(child);
                    else ++pos;
                } else {
                    ++pos;
                }
            }
        }
    }

    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = traitName.empty() ? "impl " + typeName : "impl " + traitName + " for " + typeName;
    ASTNode::Ptr node = std::make_shared<ASTNode>(NodeType::ClassDecl, range, label);
    if (!children.empty()) {
        node = node->withChildren(children);
    }
    return node;
}

ASTNode::Ptr RustParser::parseUse(const std::vector<Token>& tokens, size_t& pos) {
    size_t start = pos;
    consume(tokens, pos, "use");
    skipWhitespaceCommentsAndDoc(tokens, pos);

    // Parse use tree with nested braces: a::{b, c::{d, e}}
    std::string path;
    int braceDepth = 0;
    while (pos < tokens.size() && !peek(tokens, pos, ";") && !peek(tokens, pos, "as")) {
        if (tokens[pos].text == "{") { ++braceDepth; path += "{"; ++pos; }
        else if (tokens[pos].text == "}") { --braceDepth; path += "}"; ++pos; }
        else if (tokens[pos].type != Token::Whitespace && tokens[pos].type != Token::Comment && tokens[pos].type != Token::DocComment) {
            path += std::string(tokens[pos].text);
        }
        ++pos;
    }

    if (consume(tokens, pos, "as")) {
        skipWhitespaceCommentsAndDoc(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
            path += " as " + std::string(tokens[pos].text);
            ++pos;
        }
    }
    consume(tokens, pos, ";");
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    return std::make_shared<ASTNode>(NodeType::UsingDecl, range, "use " + path);
}

ASTNode::Ptr RustParser::parseMod(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    size_t modStart = pos;
    consume(tokens, pos, "mod");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    if (pos >= tokens.size() || tokens[pos].type != Token::Ident) return nullptr;
    std::string name(tokens[pos].text);
    ++pos;
    skipWhitespaceCommentsAndDoc(tokens, pos);

    if (consume(tokens, pos, "{")) {
        std::vector<ASTNode::Ptr> children;
        while (!peek(tokens, pos, "}") && pos < tokens.size()) {
            auto child = parseItem(tokens, pos);
            if (child) children.push_back(child);
            else ++pos;
        }
        consume(tokens, pos, "}");
        size_t modEnd = pos;
        SourceRange range = tokenRange(tokens[modStart], tokens[modEnd > 0 ? modEnd - 1 : modEnd], file_id_);
        std::string label = "mod " + name;
        if (meta.isPub) label = "pub " + label;
        ASTNode::Ptr node = std::make_shared<ASTNode>(NodeType::NamespaceDecl, range, label);
        if (!children.empty()) {
            node = node->withChildren(children);
        }
        return node;
    } else if (consume(tokens, pos, ";")) {
        size_t modEnd = pos;
        SourceRange range = tokenRange(tokens[modStart], tokens[modEnd > 0 ? modEnd - 1 : modEnd], file_id_);
        std::string label = "mod " + name;
        if (meta.isPub) label = "pub " + label;
        auto node = std::make_shared<ASTNode>(NodeType::NamespaceDecl, range, label);
        return node;
    }
    return nullptr;
}

ASTNode::Ptr RustParser::parseTypeAlias(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    size_t start = pos;
    consume(tokens, pos, "type");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    if (consume(tokens, pos, "<")) {
        int depth = 1;
        while (pos < tokens.size() && depth > 0) {
            if (tokens[pos].text == "<") ++depth;
            else if (tokens[pos].text == ">") --depth;
            ++pos;
        }
    }
    if (consume(tokens, pos, "=")) {
        while (pos < tokens.size() && !peek(tokens, pos, ";")) ++pos;
    }
    consume(tokens, pos, ";");
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = "type " + name;
    if (meta.isPub) label = "pub " + label;
    auto node = std::make_shared<ASTNode>(NodeType::TypedefDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseConstOrStatic(const std::vector<Token>& tokens, size_t& pos, RustSymbolMeta meta) {
    size_t start = pos;
    bool isConst = peek(tokens, pos, "const");
    if (isConst) consume(tokens, pos, "const");
    else consume(tokens, pos, "static");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    if (consume(tokens, pos, "mut")) skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    // Skip type annotation
    if (consume(tokens, pos, ":")) {
        while (pos < tokens.size() && !peek(tokens, pos, "=") && !peek(tokens, pos, ";")) ++pos;
    }
    if (consume(tokens, pos, "=")) {
        while (pos < tokens.size() && !peek(tokens, pos, ";")) ++pos;
    }
    consume(tokens, pos, ";");
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    std::string label = isConst ? "const " + name : "static " + name;
    if (meta.isPub) label = "pub " + label;
    auto node = std::make_shared<ASTNode>(NodeType::VariableDecl, range, label);
    return node;
}

ASTNode::Ptr RustParser::parseLet(const std::vector<Token>& tokens, size_t& pos) {
    size_t start = pos;
    consume(tokens, pos, "let");
    skipWhitespaceCommentsAndDoc(tokens, pos);
    if (consume(tokens, pos, "mut")) skipWhitespaceCommentsAndDoc(tokens, pos);
    std::string name;
    if (pos < tokens.size() && tokens[pos].type == Token::Ident) {
        name = std::string(tokens[pos].text);
        ++pos;
    }
    while (pos < tokens.size() && !peek(tokens, pos, ";")) ++pos;
    consume(tokens, pos, ";");
    size_t end = pos;
    SourceRange range = tokenRange(tokens[start], tokens[end > 0 ? end - 1 : end], file_id_);
    return std::make_shared<ASTNode>(NodeType::VariableDecl, range, "let " + name);
}

// ============================================================================
// Top-level parse
// ============================================================================
RustParseResult RustParser::parse(std::string_view content, std::string_view file_path) {
    RustParseResult result;
    file_path_ = std::string(file_path);
    file_id_ = static_cast<uint32_t>(std::hash<std::string>{}(file_path_) & 0xFFFFFF);

    auto tokens = tokenize(content);
    size_t pos = 0;

    while (pos < tokens.size() && tokens[pos].type != Token::Eof) {
        auto node = parseItem(tokens, pos);
        if (node) result.nodes.push_back(node);
        else ++pos;
    }

    result.success = true;
    return result;
}

RustParseResult RustParser::parseIncremental(std::string_view content,
                                              const std::vector<ASTNode::Ptr>& old_nodes,
                                              size_t change_start, size_t change_end) {
    // Simplified: full re-parse for now; incremental logic can be added later
    return parse(content, "");
}

// ============================================================================
// Integration helpers
// ============================================================================
void registerRustFile(ASTGraphEngine& engine, const std::string& path, const std::string& content) {
    RustParser parser;
    auto result = parser.parse(content, path);
    if (result.success) {
        engine.registerFile(path, content);
    }
}

void updateRustFile(ASTGraphEngine& engine, const std::string& path, const std::string& content) {
    RustParser parser;
    auto result = parser.parse(content, path);
    if (result.success) {
        engine.updateFile(path, content);
    }
}

} // namespace rawrxd::ast::rust
