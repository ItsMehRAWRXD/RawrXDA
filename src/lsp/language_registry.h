// ============================================================================
// language_registry.h — Phase 3: Multi-Language Support
// ============================================================================
// Language detection from file extension and shebang, per-language symbol
// extraction rules, and language-specific completion providers.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp/treesitter_parser.h"
#include "lsp/workspace_symbol_index.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>

namespace RawrXD::LSP {

// ---------------------------------------------------------------------------
// Language Capability Flags
// ---------------------------------------------------------------------------
enum class LanguageCapability : uint32_t {
    None = 0,
    Hover = 1 << 0,
    Completion = 1 << 1,
    Definition = 1 << 2,
    Diagnostics = 1 << 3,
    Formatting = 1 << 4,
    Rename = 1 << 5,
    References = 1 << 6,
    SemanticTokens = 1 << 7,
    InlayHints = 1 << 8,
    CodeActions = 1 << 9,
    SignatureHelp = 1 << 10,
    All = 0xFFFFFFFF,
};

inline LanguageCapability operator|(LanguageCapability a, LanguageCapability b) {
    return static_cast<LanguageCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline LanguageCapability operator&(LanguageCapability a, LanguageCapability b) {
    return static_cast<LanguageCapability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasCapability(LanguageCapability caps, LanguageCapability cap) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(cap)) != 0;
}

// ---------------------------------------------------------------------------
// Symbol Extraction Rule
// ---------------------------------------------------------------------------
struct SymbolExtractionRule {
    std::string pattern;           // Regex or AST matcher pattern
    SymbolKind kind = SymbolKind::Variable;
    int priority = 0;              // Higher = more specific
    bool useAst = false;           // Use AST instead of regex
};

// ---------------------------------------------------------------------------
// Completion Trigger
// ---------------------------------------------------------------------------
struct CompletionTrigger {
    std::string characters;        // e.g., ".", "::", "->"
    bool isMemberAccess = false;
    bool isScopeResolution = false;
};

// ---------------------------------------------------------------------------
// Language Info
// ---------------------------------------------------------------------------
struct LanguageInfo {
    std::string id;                // e.g., "cpp", "python"
    std::string name;              // Human-readable name
    std::vector<std::string> extensions;  // e.g., ".cpp", ".h"
    std::vector<std::string> shebangPatterns;  // e.g., "python", "python3"
    LanguageId astLanguage;        // Mapping to TreeSitterParser language
    LanguageCapability capabilities = LanguageCapability::All;
    std::vector<SymbolExtractionRule> extractionRules;
    std::vector<CompletionTrigger> completionTriggers;
    std::vector<std::string> keywords;
    std::vector<std::string> builtinTypes;
    bool caseSensitive = true;
    std::string lineComment;       // e.g., "//"
    std::string blockCommentStart; // e.g., "/*"
    std::string blockCommentEnd;   // e.g., "*/"
};

// ---------------------------------------------------------------------------
// Language Registry — Singleton
// ---------------------------------------------------------------------------
class LanguageRegistry {
public:
    static LanguageRegistry& instance();

    // Registration
    void registerLanguage(const LanguageInfo& info);
    void unregisterLanguage(const std::string& id);

    // Detection
    std::optional<LanguageInfo> detectLanguage(const std::string& uri,
                                                const std::string& content = "") const;
    std::optional<LanguageInfo> getLanguageById(const std::string& id) const;
    std::optional<LanguageInfo> getLanguageByExtension(const std::string& ext) const;

    // Symbol extraction
    std::vector<SymbolInfo> extractSymbols(const std::string& uri,
                                             const std::string& content,
                                             TreeSitterParser* parser = nullptr) const;

    // Completion triggers
    std::vector<CompletionTrigger> getCompletionTriggers(const std::string& languageId) const;

    // Keywords and builtins
    std::vector<std::string> getKeywords(const std::string& languageId) const;
    std::vector<std::string> getBuiltinTypes(const std::string& languageId) const;

    // Capabilities
    bool supportsCapability(const std::string& languageId,
                            LanguageCapability cap) const;

    // All registered languages
    std::vector<std::string> getRegisteredLanguageIds() const;

    // Built-in registration
    void registerBuiltInLanguages();

private:
    LanguageRegistry();
    ~LanguageRegistry() = default;
    LanguageRegistry(const LanguageRegistry&) = delete;
    LanguageRegistry& operator=(const LanguageRegistry&) = delete;

    std::unordered_map<std::string, LanguageInfo> m_languages;
    std::unordered_map<std::string, std::string> m_extensionToLanguage;
    mutable std::shared_mutex m_mutex;

    // Built-in language definitions
    static LanguageInfo makeCppInfo();
    static LanguageInfo makePythonInfo();
    static LanguageInfo makeJavaScriptInfo();
    static LanguageInfo makeTypeScriptInfo();
    static LanguageInfo makeRustInfo();
    static LanguageInfo makeGoInfo();
    static LanguageInfo makeJavaInfo();
    static LanguageInfo makeCSharpInfo();

    // Regex fallback extraction
    std::vector<SymbolInfo> extractSymbolsRegex(const std::string& uri,
                                                 const std::string& content,
                                                 const LanguageInfo& lang) const;
};

} // namespace RawrXD::LSP
