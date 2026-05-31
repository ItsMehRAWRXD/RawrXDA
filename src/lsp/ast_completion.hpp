// =============================================================================
// lsp/ast_completion.hpp — AST-aware completion provider
// =============================================================================
// Plugs into the existing CompletionProvider source-registration API so it
// works alongside keyword/type completions without replacing anything.
//
// How it works:
//   1. On each provideCompletion() call the caller passes the cursor position.
//   2. ASTCompletionSource parses the document with the existing
//      RawrXD::LSP::TreeSitterParser (already built).
//   3. It walks the AST to determine:
//        • Which scope the cursor lives in (function body, class body, …)
//        • Which local symbols are visible at that point
//        • What's on the left side of '.' / '->' / '::' (member resolution)
//   4. Matching symbols are returned as LSP CompletionItems with correct
//      detail/documentation strings pulled from the AST node.
//
// Wiring (add once to CompletionProvider initialisation):
//   #include "lsp/ast_completion.hpp"
//   provider.registerSource(
//       RawrXD::LSP::ASTCompletionSource::make(treesitter_parser));
//
// No extra dependencies — relies only on treesitter_parser.h + CompletionItem
// types already defined in the codebase.
// =============================================================================
#pragma once

#include "lsp/completion_provider.h"   // CompletionItem, CompletionContext
#include "lsp/treesitter_parser.h"     // ASTNode, ASTNodeKind, TreeSitterParser

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RawrXD::LSP {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace detail {

// Walk *up* the parent chain to collect all symbols declared in ancestor scopes
// that are reachable from `cursor_node`.
inline void collect_scope_symbols(const ASTNode* node,
                                  std::vector<const ASTNode*>& out) {
    if (!node) return;
    // Collect siblings declared before the cursor within this scope
    if (auto parent = node->parent.lock()) {
        for (const auto& child : parent->children) {
            if (child.get() == node) break;  // stop at cursor
            if (child->isDeclaration()) out.push_back(child.get());
        }
        collect_scope_symbols(parent.get(), out);
    }
}

// Extract the member-access base identifier:  "foo." / "foo->" / "Foo::"
// Returns empty string when no access chain is found.
inline std::string member_base(const std::string& line_before_cursor) {
    for (auto [sep, sep_len] : std::initializer_list<std::pair<const char*, int>>{
             {"->", 2}, {"::", 2}, {".", 1}}) {
        auto pos = line_before_cursor.rfind(sep);
        if (pos == std::string::npos) continue;
        // Walk back to the start of the identifier before the separator
        size_t end = pos;
        while (end > 0 && line_before_cursor[end - 1] == ' ') --end;
        size_t start = end;
        while (start > 0 && (std::isalnum(line_before_cursor[start - 1])
                              || line_before_cursor[start - 1] == '_'))
            --start;
        if (start < end) return line_before_cursor.substr(start, end - start);
    }
    return {};
}

// Find the word that is currently being typed (prefix filter for candidates)
inline std::string cursor_prefix(const std::string& line_before_cursor) {
    size_t i = line_before_cursor.size();
    while (i > 0 && (std::isalnum(line_before_cursor[i - 1])
                     || line_before_cursor[i - 1] == '_'))
        --i;
    return line_before_cursor.substr(i);
}

// Build a CompletionItem from an ASTNode
inline CompletionItem item_from_node(const ASTNode& node) {
    CompletionItem ci;
    ci.label = node.name.empty() ? node.text : node.name;
    switch (node.kind) {
        case ASTNodeKind::FunctionDecl:
        case ASTNodeKind::FunctionDef:
            ci.kind   = CompletionItemKind::Function;
            ci.detail = node.typeName.empty()
                ? std::optional<std::string>("function")
                : std::optional<std::string>("function -> " + node.typeName);
            ci.insertText = ci.label + "($0)";
            break;
        case ASTNodeKind::ClassDecl:
            ci.kind       = CompletionItemKind::Class;
            ci.detail     = "class";
            ci.insertText = ci.label;
            break;
        case ASTNodeKind::StructDecl:
            ci.kind       = CompletionItemKind::Struct;
            ci.detail     = "struct";
            ci.insertText = ci.label;
            break;
        case ASTNodeKind::EnumDecl:
            ci.kind       = CompletionItemKind::Enum;
            ci.detail     = "enum";
            ci.insertText = ci.label;
            break;
        case ASTNodeKind::Parameter:
            ci.kind       = CompletionItemKind::Variable;
            ci.detail = node.typeName.empty()
                ? std::optional<std::string>("param")
                : std::optional<std::string>("param: " + node.typeName);
            ci.insertText = ci.label;
            break;
        case ASTNodeKind::VariableDecl:
            ci.kind       = CompletionItemKind::Variable;
            ci.detail = node.typeName.empty()
                ? std::optional<std::string>("var")
                : std::optional<std::string>("var: " + node.typeName);
            ci.insertText = ci.label;
            break;
        case ASTNodeKind::Namespace:
            ci.kind       = CompletionItemKind::Module;
            ci.detail     = "namespace";
            ci.insertText = ci.label;
            break;
        default:
            ci.kind       = CompletionItemKind::Text;
            ci.insertText = ci.label;
            break;
    }
    // Bake in a sortText that puts AST-derived items first
    ci.sortText = "0_" + ci.label;
    return ci;
}

// Collect all named children of a class/struct node
inline void collect_members(const ASTNode* type_node,
                             std::vector<const ASTNode*>& out) {
    if (!type_node) return;
    for (const auto& child : type_node->children) {
        if (child->isDeclaration()) out.push_back(child.get());
        // Recurse into nested scopes that aren't function bodies
        if (child->kind == ASTNodeKind::Block) continue;
        collect_members(child.get(), out);
    }
}

// Find node for symbol name in the AST (shallow search)
inline const ASTNode* find_named(const ASTNode* root, const std::string& name) {
    if (!root) return nullptr;
    for (const auto& child : root->children) {
        if (child->name == name) return child.get();
        if (auto* found = find_named(child.get(), name)) return found;
    }
    return nullptr;
}

} // namespace detail

// ---------------------------------------------------------------------------
// ASTCompletionSource
// ---------------------------------------------------------------------------
class ASTCompletionSource {
public:
    explicit ASTCompletionSource(TreeSitterParser& parser)
        : m_parser(parser) {}

    // Factory — returns a CompletionProvider source callable
    static std::function<std::vector<CompletionItem>(
        const std::string& /*uri*/,
        const std::string& /*prefix*/)>
    make(TreeSitterParser& parser,
         uint32_t cursor_line   = 0,
         uint32_t cursor_column = 0,
         const std::string& document_text = {}) {
        // Lambdas capture shared_ptr to ensure lifetime
        auto self = std::make_shared<ASTCompletionSource>(parser);
        return [self, cursor_line, cursor_column, document_text](
                   const std::string& uri,
                   const std::string& prefix) mutable {
            return self->provide(uri, document_text, prefix,
                                 cursor_line, cursor_column);
        };
    }

    // Call with a fresh document text + cursor each completion request
    std::vector<CompletionItem> provide(const std::string& uri,
                                        const std::string& document,
                                        const std::string& prefix,
                                        uint32_t line,
                                        uint32_t column) {
        std::vector<CompletionItem> items;
        if (document.empty()) return items;

        // Parse (cached internally by TreeSitterParser)
        auto root = m_parser.parse(uri, document, TreeSitterParser::detectLanguage(uri, document));
        if (!root) return items;

        // Find the AST node closest to the cursor
        auto cursor_shared = m_parser.nodeAtPosition(root, line, column);
        const ASTNode* cursor_node = cursor_shared ? cursor_shared.get() : root.get();

        // Build line-before-cursor for member resolution
        std::string line_text;
        {
            size_t start = 0, cur_line = 0;
            for (size_t i = 0; i < document.size(); ++i) {
                if (cur_line == line) {
                    size_t end = document.find('\n', i);
                    if (end == std::string::npos) end = document.size();
                    line_text = document.substr(i, std::min(end - i,
                                                static_cast<size_t>(column)));
                    break;
                }
                if (document[i] == '\n') { ++cur_line; start = i + 1; }
            }
        }

        const std::string base = detail::member_base(line_text);
        const std::string pfx  = prefix.empty()
                                ? detail::cursor_prefix(line_text)
                                : prefix;

        if (!base.empty()) {
            // Member-access completion: find the type node for `base`
            const ASTNode* base_node = detail::find_named(root.get(), base);
            std::vector<const ASTNode*> members;
            detail::collect_members(base_node, members);
            for (const auto* m : members) {
                if (m->name.empty()) continue;
                if (!pfx.empty() && m->name.find(pfx) != 0) continue;
                items.push_back(detail::item_from_node(*m));
            }
        } else {
            // Scope completion: collect everything visible from the cursor
            std::vector<const ASTNode*> visible;
            detail::collect_scope_symbols(cursor_node, visible);
            // Also add top-level declarations from root
            for (const auto& child : root->children) {
                if (child->isDeclaration()) visible.push_back(child.get());
            }
            for (const auto* sym : visible) {
                if (sym->name.empty()) continue;
                if (!pfx.empty() && sym->name.find(pfx) != 0) continue;
                items.push_back(detail::item_from_node(*sym));
            }
        }

        // De-duplicate by label
        std::sort(items.begin(), items.end(),
                  [](const CompletionItem& a, const CompletionItem& b) {
                      return a.label < b.label;
                  });
        items.erase(std::unique(items.begin(), items.end(),
                                [](const CompletionItem& a, const CompletionItem& b) {
                                    return a.label == b.label;
                                }),
                    items.end());

        return items;
    }

private:
    TreeSitterParser& m_parser;
};

} // namespace RawrXD::LSP
