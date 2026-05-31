// rust_parser_v2.cpp
// Cursor-based Rust parser v3 — deterministic, allocation-light, safe against infinite loops.
// Replaces token-based v2 with a single-pass character scanner.
// v3.1: captures doc comments, attributes, visibility, and modifiers into RustMeta.
// v3.2: adds SymbolTable integration for indexing.

#include "rust_parser.hpp"
#include "symbol_table.hpp"
#include <cctype>
#include <cstring>
#include <algorithm>

namespace rawrxd::ast::rust {

using RawrXD::AST::RustMeta;
using rawrxd::ast::SymbolTable;
using rawrxd::ast::Symbol;
using rawrxd::ast::CallEdge;
using rawrxd::ast::CallKind;

// ============================================================
// Minimal scanner (NO tokenizer needed)
// ============================================================

struct Cursor {
    const char* s;
    size_t i;
    size_t n;

    Cursor(std::string_view sv) : s(sv.data()), i(0), n(sv.size()) {}

    bool eof() const { return i >= n; }
    char peek() const { return i < n ? s[i] : 0; }
    char next() { return i < n ? s[i++] : 0; }

    void skip_ws() {
        while (!eof()) {
            char c = peek();

            // whitespace
            if (isspace((unsigned char)c)) { i++; continue; }

            // line comment (non-doc)
            if (c == '/' && i + 1 < n && s[i+1] == '/' && (i+2 >= n || s[i+2] != '/')) {
                i += 2;
                while (!eof() && peek() != '\n') i++;
                continue;
            }

            // block comment (nested, non-doc)
            if (c == '/' && i + 1 < n && s[i+1] == '*' && (i+2 >= n || s[i+2] != '!')) {
                i += 2;
                int depth = 1;
                while (!eof() && depth > 0) {
                    if (peek() == '/' && i + 1 < n && s[i+1] == '*') { depth++; i += 2; }
                    else if (peek() == '*' && i + 1 < n && s[i+1] == '/') { depth--; i += 2; }
                    else i++;
                }
                continue;
            }

            break;
        }
    }

    bool match_kw(const char* kw) {
        size_t j = i;
        for (size_t k = 0; kw[k]; ++k) {
            if (j >= n || s[j] != kw[k]) return false;
            j++;
        }
        if (j < n && (isalnum((unsigned char)s[j]) || s[j] == '_')) return false;
        i = j;
        return true;
    }

    std::string ident() {
        size_t start = i;
        if (!(isalpha((unsigned char)peek()) || peek() == '_')) return "";
        i++;
        while (!eof() && (isalnum((unsigned char)peek()) || peek() == '_')) i++;
        return std::string(s + start, i - start);
    }

    void skip_block(char open, char close) {
        if (peek() != open) return;
        i++;
        int depth = 1;
        while (!eof() && depth > 0) {
            if (peek() == '/' && i + 1 < n && s[i + 1] == '/') {
                i += 2;
                while (!eof() && peek() != '\n') i++;
                continue;
            }

            if (peek() == '/' && i + 1 < n && s[i + 1] == '*') {
                i += 2;
                int commentDepth = 1;
                while (!eof() && commentDepth > 0) {
                    if (peek() == '/' && i + 1 < n && s[i + 1] == '*') { commentDepth++; i += 2; }
                    else if (peek() == '*' && i + 1 < n && s[i + 1] == '/') { commentDepth--; i += 2; }
                    else i++;
                }
                continue;
            }

            if (peek() == '"') {
                i++;
                bool escaped = false;
                while (!eof()) {
                    char ch = next();
                    if (escaped) { escaped = false; continue; }
                    if (ch == '\\') { escaped = true; continue; }
                    if (ch == '"') break;
                }
                continue;
            }

            if (peek() == 'r') {
                size_t start = i;
                size_t probe = start + 1;
                while (probe < n && s[probe] == '#') probe++;
                if (probe < n && s[probe] == '"') {
                    const size_t hashCount = probe - (start + 1);
                    i = probe + 1;
                    while (!eof()) {
                        if (peek() == '"') {
                            bool matched = true;
                            for (size_t k = 0; k < hashCount; ++k) {
                                if (i + 1 + k >= n || s[i + 1 + k] != '#') {
                                    matched = false;
                                    break;
                                }
                            }
                            if (matched) {
                                i += 1 + hashCount;
                                break;
                            }
                        }
                        i++;
                    }
                    continue;
                }
            }

            char c = next();
            if (c == open) depth++;
            else if (c == close) depth--;
        }
    }

    // ---- metadata capture helpers ----

    // Collect consecutive `///` doc comments
    std::string collect_doc() {
        std::string doc;
        while (!eof()) {
            if (peek() == '/' && i + 2 < n && s[i+1] == '/' && s[i+2] == '/') {
                i += 3;
                size_t start = i;
                while (!eof() && peek() != '\n') i++;
                if (!doc.empty()) doc += "\n";
                doc.append(s + start, i - start);
                // skip newline
                if (!eof() && peek() == '\n') i++;
            } else if (isspace((unsigned char)peek())) {
                i++;
            } else {
                break;
            }
        }
        return doc;
    }

    // Collect consecutive `#[...]` or `#![...]` attributes
    std::vector<std::string> collect_attrs() {
        std::vector<std::string> attrs;
        while (!eof()) {
            skip_ws();
            if (peek() == '#' && i + 1 < n && s[i+1] == '[') {
                size_t start = i;
                i += 2;
                int depth = 1;
                while (!eof() && depth > 0) {
                    char c = next();
                    if (c == '[') depth++;
                    else if (c == ']') depth--;
                }
                attrs.emplace_back(s + start, i - start);
            } else {
                break;
            }
        }
        return attrs;
    }

    // Parse visibility: pub, pub(crate), pub(super), pub(self), pub(in path)
    std::string collect_vis() {
        skip_ws();
        if (!match_kw("pub")) return "";
        skip_ws();
        if (peek() == '(') {
            size_t start = i;
            skip_block('(', ')');
            return std::string("pub") + std::string(s + start, i - start);
        }
        return "pub";
    }

    // Parse modifiers: async, unsafe, extern, const, static
    void collect_modifiers(bool& isAsync, bool& isUnsafe, bool& isExtern,
                           bool& isConst, bool& isStatic) {
        bool loop = true;
        while (loop) {
            loop = false;
            skip_ws();
            if (match_kw("async"))   { isAsync = true;  loop = true; }
            else if (match_kw("unsafe")) { isUnsafe = true; loop = true; }
            else if (match_kw("extern")) {
                isExtern = true;
                skip_ws();
                if (peek() == '"') {
                    i++;
                    bool escaped = false;
                    while (!eof()) {
                        char ch = next();
                        if (escaped) { escaped = false; continue; }
                        if (ch == '\\') { escaped = true; continue; }
                        if (ch == '"') break;
                    }
                }
                loop = true;
            }
            else if (match_kw("const"))  { isConst = true;  loop = true; }
            else if (match_kw("static")) { isStatic = true; loop = true; }
        }
    }
};

static bool isIdentStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

static bool isIdentContinue(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

static bool startsWithKeyword(const Cursor& c, const char* kw) {
    size_t probe = c.i;
    for (size_t k = 0; kw[k]; ++k) {
        if (probe >= c.n || c.s[probe] != kw[k]) return false;
        ++probe;
    }
    return probe >= c.n || !isIdentContinue(c.s[probe]);
}

static void skipUntilTopLevel(Cursor& c,
                              std::initializer_list<char> stopChars,
                              std::initializer_list<const char*> stopKeywords = {}) {
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    int angleDepth = 0;

    while (!c.eof()) {
        if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0 && angleDepth == 0) {
            for (const char* kw : stopKeywords) {
                if (startsWithKeyword(c, kw)) return;
            }
            for (char stop : stopChars) {
                if (c.peek() == stop) return;
            }
        }

        if (c.peek() == '/' && c.i + 1 < c.n && c.s[c.i + 1] == '/') {
            c.i += 2;
            while (!c.eof() && c.peek() != '\n') c.i++;
            continue;
        }
        if (c.peek() == '/' && c.i + 1 < c.n && c.s[c.i + 1] == '*') {
            c.i += 2;
            int commentDepth = 1;
            while (!c.eof() && commentDepth > 0) {
                if (c.peek() == '/' && c.i + 1 < c.n && c.s[c.i + 1] == '*') { commentDepth++; c.i += 2; }
                else if (c.peek() == '*' && c.i + 1 < c.n && c.s[c.i + 1] == '/') { commentDepth--; c.i += 2; }
                else c.i++;
            }
            continue;
        }
        if (c.peek() == '"') {
            c.skip_block('"', '"');
            continue;
        }

        char ch = c.next();
        if (ch == '(') ++parenDepth;
        else if (ch == ')' && parenDepth > 0) --parenDepth;
        else if (ch == '[') ++bracketDepth;
        else if (ch == ']' && bracketDepth > 0) --bracketDepth;
        else if (ch == '{') ++braceDepth;
        else if (ch == '}' && braceDepth > 0) --braceDepth;
        else if (ch == '<') ++angleDepth;
        else if (ch == '>' && angleDepth > 0) --angleDepth;
    }
}

static bool skipMacroInvocation(Cursor& c) {
    size_t save = c.i;
    std::string name = c.ident();
    if (name.empty()) return false;

    c.skip_ws();
    while (c.peek() == ':' && c.i + 1 < c.n && c.s[c.i + 1] == ':') {
        c.i += 2;
        c.skip_ws();
        if (c.ident().empty()) {
            c.i = save;
            return false;
        }
        c.skip_ws();
    }

    if (c.peek() != '!') {
        c.i = save;
        return false;
    }

    c.i++;
    c.skip_ws();
    if (c.peek() == '{') c.skip_block('{', '}');
    else if (c.peek() == '(') c.skip_block('(', ')');
    else if (c.peek() == '[') c.skip_block('[', ']');
    else {
        c.i = save;
        return false;
    }

    c.skip_ws();
    if (c.peek() == ';') c.i++;
    return true;
}

static bool skipMacroRulesDefinition(Cursor& c) {
    size_t save = c.i;
    if (!c.match_kw("macro_rules")) return false;

    c.skip_ws();
    if (c.peek() != '!') {
        c.i = save;
        return false;
    }
    c.i++;
    c.skip_ws();
    c.ident();
    c.skip_ws();

    if (c.peek() == '{') c.skip_block('{', '}');
    else if (c.peek() == '(') c.skip_block('(', ')');
    else {
        c.i = save;
        return false;
    }

    c.skip_ws();
    if (c.peek() == ';') c.i++;
    return true;
}

// ============================================================
// Call graph scanner (v3.2) — lightweight, string-based, no type resolution
// ============================================================

static bool isRustKeyword(std::string_view ident) {
    static const char* kw[] = {
        "if","else","while","for","loop","match","return","break","continue",
        "let","mut","ref","const","static","async","await","unsafe","move",
        "where","impl","trait","struct","enum","fn","type","pub","use","mod",
        "as","in","box","yield","try","dyn","abstract",
        "become","do","final","macro","override","priv","typeof","unsized",
        "virtual","union"
        // NOTE: self, super, crate are NOT keywords here — they are valid path prefixes
        // for qualified calls (self.method, crate::foo, super::bar)
    };
    for (const char* k : kw) {
        if (ident == k) return true;
    }
    return false;
}

// Scan a function body range [body_start, body_end) for call sites.
// Emits CallEdge entries into the provided SymbolTable.
static void extractCallsFromBody(const char* s, size_t body_start, size_t body_end,
                                 const std::string& caller_name,
                                 SymbolTable* symbols) {
    if (!symbols || body_start >= body_end) return;

    size_t i = body_start;
    size_t n = body_end;

    while (i < n) {
        char c = s[i];
        if (!(isalpha((unsigned char)c) || c == '_')) { i++; continue; }

        size_t ident_start = i;
        i++;
        while (i < n && (isalnum((unsigned char)s[i]) || s[i] == '_')) i++;
        std::string first(s + ident_start, i - ident_start);

        if (isRustKeyword(first)) continue;

        // Look ahead for call patterns: . or :: or (
        size_t j = i;
        while (j < n && isspace((unsigned char)s[j])) j++;

        if (j < n && s[j] == '(') {
            // simple call: first(...)
            symbols->addCallEdge({caller_name, first, CallKind::Direct, nullptr, 0});
            i = j + 1;
            continue;
        }

        // Build callee chain
        std::string callee = first;
        CallKind kind = CallKind::Direct;

        if (j + 1 < n && s[j] == ':' && s[j+1] == ':') {
            // Qualified chain: first::second::third(...)
            kind = CallKind::Qualified;
            while (j + 1 < n && s[j] == ':' && s[j+1] == ':') {
                j += 2;
                while (j < n && isspace((unsigned char)s[j])) j++;
                size_t seg_start = j;
                if (j < n && (isalpha((unsigned char)s[j]) || s[j] == '_')) {
                    j++;
                    while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
                    callee += "::" + std::string(s + seg_start, j - seg_start);
                } else {
                    break;
                }
            }
        } else if (j < n && s[j] == '.') {
            // Method chain: obj.method(...)
            kind = CallKind::Method;
            j++;
            while (j < n && isspace((unsigned char)s[j])) j++;
            size_t method_start = j;
            if (j < n && (isalpha((unsigned char)s[j]) || s[j] == '_')) {
                j++;
                while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
                callee += "." + std::string(s + method_start, j - method_start);
            }
        }

        // After building chain, verify it's a call site
        while (j < n && isspace((unsigned char)s[j])) j++;
        if (j < n && s[j] == '(') {
            symbols->addCallEdge({caller_name, callee, kind, nullptr, 0});
            i = j + 1;
            continue;
        }

        // Not a call site — advance past consumed content
        i = j;
    }
}

// ============================================================
// Node helpers
// ============================================================

static ASTNode::Ptr makeNode(NodeType t, const std::string& label, uint32_t fid,
                             const RustMeta& meta = RustMeta{}) {
    SourceRange r{};
    r.start.file_id = fid;
    r.end.file_id = fid;
    auto node = std::make_shared<ASTNode>(t, r, label);
    node->rust_meta_ = meta;
    return node;
}

static void parseFunctionTail(Cursor& c, size_t& body_start, size_t& body_end) {
    body_start = 0;
    body_end = 0;

    c.skip_ws();
    if (c.peek() == '-' && c.i + 1 < c.n && c.s[c.i + 1] == '>') {
        c.i += 2;
        c.skip_ws();
        skipUntilTopLevel(c, {'{', ';'}, {"where"});
    }

    c.skip_ws();
    if (startsWithKeyword(c, "where")) {
        c.match_kw("where");
        c.skip_ws();
        skipUntilTopLevel(c, {'{', ';'});
    }

    c.skip_ws();
    if (c.peek() == '{') {
        body_start = c.i + 1;
        c.skip_block('{', '}');
        body_end = c.i - 1;
    } else if (c.peek() == ';') {
        c.i++;
    }
}

static std::vector<ASTNode::Ptr> parseAssociatedItems(Cursor& c,
                                                      uint32_t fid,
                                                      std::string_view file_path,
                                                      SymbolTable* symbols,
                                                      const std::string& parent_name = {}) {
    std::vector<ASTNode::Ptr> children;
    if (c.peek() != '{') return children;

    c.i++;
    int depth = 1;

    while (!c.eof() && depth > 0) {
        c.skip_ws();

        if (c.peek() == '{') { depth++; c.i++; continue; }
        if (c.peek() == '}') { depth--; c.i++; continue; }
        if (skipMacroRulesDefinition(c) || skipMacroInvocation(c)) continue;

        const size_t save = c.i;

        RustMeta innerMeta;
        innerMeta.doc = c.collect_doc();
        innerMeta.attributes = c.collect_attrs();
        innerMeta.visibility = c.collect_vis();

        bool isAsync = false, isUnsafe = false, isExtern = false, isConst = false, isStatic = false;
        c.collect_modifiers(isAsync, isUnsafe, isExtern, isConst, isStatic);
        innerMeta.is_async = isAsync;
        innerMeta.is_unsafe = isUnsafe;
        innerMeta.is_extern = isExtern;
        innerMeta.is_const = isConst;
        innerMeta.is_static = isStatic;
        innerMeta.is_pub = !innerMeta.visibility.empty();

        if (c.match_kw("fn")) {
            c.skip_ws();
            std::string name = c.ident();

            c.skip_ws();
            if (c.peek() == '<') c.skip_block('<', '>');
            c.skip_ws();
            if (c.peek() == '(') c.skip_block('(', ')');

            size_t body_start = 0;
            size_t body_end = 0;
            parseFunctionTail(c, body_start, body_end);

            std::string label = "fn " + name;
            if (isAsync) label = "async " + label;
            if (isUnsafe) label = "unsafe " + label;
            if (isExtern) label = "extern " + label;

            auto child = makeNode(NodeType::FunctionDecl, label, fid, innerMeta);
            children.push_back(child);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::FunctionDecl, child->getRange(), innerMeta, parent_name});
                if (body_end > body_start) {
                    extractCallsFromBody(c.s, body_start, body_end, name, symbols);
                }
            }
            continue;
        }

        c.i = save + 1;
    }

    return children;
}

// ============================================================
// Core parse
// ============================================================

RustParseResult RustParser::parse(std::string_view content, std::string_view file_path) {
    return parse(content, file_path, nullptr);
}

RustParseResult RustParser::parse(std::string_view content, std::string_view file_path,
                                  SymbolTable* symbols) {
    if (symbols) symbols->clear();

    RustParseResult out;
    uint32_t fid = (uint32_t)(std::hash<std::string>{}(std::string(file_path)) & 0xFFFFFF);

    Cursor c(content);

    std::vector<ASTNode::Ptr> nodes;

    while (!c.eof()) {
        c.skip_ws();

        // ---- metadata capture pipeline (v3.1)
        RustMeta meta;
        meta.doc = c.collect_doc();
        meta.attributes = c.collect_attrs();
        meta.visibility = c.collect_vis();

        bool isAsync=false, isUnsafe=false, isExtern=false, isConst=false, isStatic=false;
        c.collect_modifiers(isAsync, isUnsafe, isExtern, isConst, isStatic);
        meta.is_async = isAsync;
        meta.is_unsafe = isUnsafe;
        meta.is_extern = isExtern;
        meta.is_const = isConst;
        meta.is_static = isStatic;
        meta.is_pub = !meta.visibility.empty();

        if (skipMacroRulesDefinition(c) || skipMacroInvocation(c)) {
            continue;
        }

        // =====================================================
        // FUNCTION (FIX: now always detected)
        // =====================================================
        if (c.match_kw("fn")) {
            c.skip_ws();
            std::string name = c.ident();

            // generics
            c.skip_ws();
            if (c.peek() == '<') c.skip_block('<','>');

            // params
            c.skip_ws();
            if (c.peek() == '(') c.skip_block('(',')');

            size_t body_start = 0, body_end = 0;
            parseFunctionTail(c, body_start, body_end);

            std::string label = "fn " + name;
            if (isAsync) label = "async " + label;
            if (isUnsafe) label = "unsafe " + label;
            if (isExtern) label = "extern " + label;

            auto node = makeNode(NodeType::FunctionDecl, label, fid, meta);
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::FunctionDecl, node->getRange(), meta, ""});
                if (body_end > body_start) {
                    extractCallsFromBody(c.s, body_start, body_end, name, symbols);
                }
            }
            continue;
        }

        // =====================================================
        // STRUCT
        // =====================================================
        if (c.match_kw("struct")) {
            c.skip_ws();
            std::string name = c.ident();

            c.skip_ws();
            if (c.peek() == '<') c.skip_block('<','>');
            c.skip_ws();
            if (c.peek() == '{') c.skip_block('{','}');
            else if (c.peek() == '(') c.skip_block('(',')');

            auto node = makeNode(NodeType::StructDecl, "struct " + name, fid, meta);
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::StructDecl, node->getRange(), meta, ""});
            }
            continue;
        }

        // =====================================================
        // TRAIT
        // =====================================================
        if (c.match_kw("trait")) {
            c.skip_ws();
            std::string name = c.ident();

            c.skip_ws();
            if (c.peek() == '<') c.skip_block('<','>');
            c.skip_ws();
            if (c.peek() == ':') {
                c.i++;
                skipUntilTopLevel(c, {'{'});
            }
            c.skip_ws();

            auto children = parseAssociatedItems(c, fid, file_path, symbols, name);

            auto node = makeNode(NodeType::ClassDecl, "trait " + name, fid, meta);
            if (!children.empty()) {
                node = node->withChildren(std::move(children));
            }
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::ClassDecl, node->getRange(), meta, ""});
            }
            continue;
        }

        // =====================================================
        // ENUM
        // =====================================================
        if (c.match_kw("enum")) {
            c.skip_ws();
            std::string name = c.ident();

            c.skip_ws();
            if (c.peek() == '{') c.skip_block('{','}');

            auto node = makeNode(NodeType::EnumDecl, "enum " + name, fid, meta);
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::EnumDecl, node->getRange(), meta, ""});
            }
            continue;
        }

        // =====================================================
        // IMPL (FIX: inner fn parsing)
        // =====================================================
        if (c.match_kw("impl")) {
            c.skip_ws();

            if (c.peek() == '<') c.skip_block('<','>');

            skipUntilTopLevel(c, {'{'});

            auto children = parseAssociatedItems(c, fid, file_path, symbols);

            auto implNode = makeNode(NodeType::ClassDecl, "impl", fid, meta);
            if (!children.empty()) {
                implNode = implNode->withChildren(std::move(children));
            }
            nodes.push_back(implNode);
            continue;
        }

        // =====================================================
        // USE
        // =====================================================
        if (c.match_kw("use")) {
            size_t start = c.i;
            while (!c.eof() && c.peek() != ';') c.i++;
            if (!c.eof()) c.i++;
            nodes.push_back(makeNode(NodeType::UsingDecl, "use", fid, meta));
            continue;
        }

        // =====================================================
        // MOD
        // =====================================================
        if (c.match_kw("mod")) {
            c.skip_ws();
            std::string name = c.ident();

            if (c.peek() == '{') c.skip_block('{','}');
            else if (c.peek() == ';') c.i++;

            auto node = makeNode(NodeType::NamespaceDecl, "mod " + name, fid, meta);
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::NamespaceDecl, node->getRange(), meta, ""});
            }
            continue;
        }

        // =====================================================
        // LET
        // =====================================================
        if (c.match_kw("let")) {
            c.skip_ws();
            std::string name = c.ident();

            while (!c.eof() && c.peek() != ';') c.i++;
            if (!c.eof()) c.i++;

            auto node = makeNode(NodeType::VariableDecl, "let " + name, fid, meta);
            nodes.push_back(node);

            if (symbols) {
                symbols->add({name, std::string(file_path), NodeType::VariableDecl, node->getRange(), meta, ""});
            }
            continue;
        }

        // fallback (never stall)
        c.i++;
    }

    out.nodes = std::move(nodes);
    out.success = true;
    return out;
}

RustParseResult RustParser::parseIncremental(
    std::string_view content,
    const std::vector<ASTNode::Ptr>&,
    size_t, size_t) {
    return parse(content, "");
}

// ============================================================
// Engine integration (unchanged contract)
// ============================================================

void registerRustFile(ASTGraphEngine& engine,
                      const std::string& path,
                      const std::string& content)
{
    RustParser p;
    auto r = p.parse(content, path);
    if (r.success) engine.registerFile(path, content);
}

void updateRustFile(ASTGraphEngine& engine,
                    const std::string& path,
                    const std::string& content)
{
    RustParser p;
    auto r = p.parse(content, path);
    if (r.success) engine.updateFile(path, content);
}

} // namespace rawrxd::ast::rust
