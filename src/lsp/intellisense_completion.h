// Completion ranking and filtering built on workspace symbol data.
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
#pragma once

#include "lsp/workspace_symbol_index.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <chrono>

namespace RawrXD::LSP {

// Completion kinds and request/response structs (LSP-aligned).

enum class CompletionItemKind : uint8_t {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum class CompletionTriggerKind : uint8_t {
    Invoked = 1,                      // Ctrl+Space explicit trigger
    TriggerCharacter = 2,              // Character like '.' triggered completion
    TriggerForIncompleteCompletions = 3,  // Incomplete previous result
};

struct Location {
    std::string uri;
    uint32_t line;
    uint32_t character;
    
    bool operator==(const Location& other) const {
        return uri == other.uri && line == other.line && character == other.character;
    }
};

struct CompletionContext {
    CompletionTriggerKind triggerKind;
    std::string triggerCharacter;  // ".", ":", etc.
};

struct CompletionParams {
    std::string uri;
    uint32_t line;
    uint32_t character;
    CompletionContext context;
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind;
    std::string detail;
    std::string documentation;
    std::string insertText;
    std::string filterText;
    int sortText;  // For sorting
    float relevanceScore;  // 0.0 - 1.0
    bool deprecated = false;
    std::string commitCharacters;  // Characters that complete the item
};

struct CompletionList {
    bool isIncomplete = false;
    std::vector<CompletionItem> items;
    int64_t responseTimeMs = 0;
};

class AdvancedCodeCompletion {
public:
    explicit AdvancedCodeCompletion(WorkspaceSymbolIndex* index);
    ~AdvancedCodeCompletion();

    
    // Get completion suggestions at cursor position
    CompletionList getCompletions(const CompletionParams& params,
                                  const std::string& currentFileContent);
    
    // Resolve additional detail for completion item (LSP 3.17)
    CompletionItem resolveCompletion(const CompletionItem& item);
    
    
    struct ContextAnalysis {
        std::string scopeName;
        std::string lastToken;
        std::string tokenBeforeCursor;
        SymbolKind expectedType;  // What type should we complete
        bool inFunctionCall;
        bool inObjectLiteral;
        bool isTypeContext;
    };
    
    ContextAnalysis analyzeContext(const std::string& content,
                                   uint32_t line,
                                   uint32_t character);
    
    
    struct SnippetTemplate {
        std::string name;
        std::string body;
        std::string description;
    };
    
    std::vector<SnippetTemplate> getSnippets(CompletionItemKind kind);
    
    
    struct CompletionMetrics {
        double avgCompletionTimeMs;
        double p99CompletionTimeMs;
        size_t totalCompletions;
        float successRate;  // % of completions with results
    };
    
    CompletionMetrics getMetrics() const;
    void clearMetrics();

private:
    WorkspaceSymbolIndex* m_index;
    
    // Completion metrics
    mutable struct {
        std::vector<double> completionTimes;
        size_t totalCompletions = 0;
        size_t successful = 0;
    } m_metrics;
    
    // Snippet database
    std::unordered_map<int, std::vector<SnippetTemplate>> m_snippets;
    
    
    void initializeSnippets();
    std::string extractWord(const std::string& content,
                           uint32_t line,
                           uint32_t character);
    std::vector<CompletionItem> filterAndRank(
        const std::vector<SymbolInfo>& symbols,
        const ContextAnalysis& context,
        const std::string& prefix);
    float calculateCompletionScore(const SymbolInfo& symbol,
                                   const ContextAnalysis& context,
                                   const std::string& prefix);
    bool isVisible(const SymbolInfo& symbol,
                  const std::string& currentScope);
};

struct SignatureInformation {
    std::string label;
    std::string documentation;
    std::vector<std::string> parameters;
    int activeParameter = 0;
};

struct HoverInformation {
    std::string contents;
    std::optional<Location> location;
};

struct CodeLensInfo {
    Location location;
    std::string title;
    std::string command;
    std::vector<std::string> arguments;
};

struct DiagnosticInfo {
    Location location;
    std::string message;
    int severity;  // 1=error, 2=warning, 3=info, 4=hint
    std::string code;
    std::string source;
};

class IntelliSenseEnhancer {
public:
    explicit IntelliSenseEnhancer(WorkspaceSymbolIndex* index);
    ~IntelliSenseEnhancer();

    
    std::optional<HoverInformation> getHoverInfo(const Location& loc,
                                                 const std::string& content);
    
    
    std::optional<SignatureInformation> getSignatureHelp(
        const Location& loc,
        const std::string& content);
    
    
    std::vector<CodeLensInfo> getCodeLens(const std::string& uri,
                                          const std::string& content);
    
    
    std::vector<DiagnosticInfo> getDiagnostics(const std::string& uri,
                                               const std::string& content);
    
    
    struct SemanticToken {
        uint32_t line;
        uint32_t character;
        uint32_t length;
        int type;    // Token type index (0-based)
        int modifiers; // Bitmask of modifier indices
    };
    
    std::vector<SemanticToken> getSemanticTokens(const std::string& content);
    
    
    std::optional<Location> goToDefinition(const Location& loc,
                                           const std::string& content);
    
    
    std::vector<Location> findAllReferences(const Location& loc,
                                            const std::string& content,
                                            bool includeDeclaration = true);
    
    
    struct DocumentSymbol {
        std::string name;
        SymbolKind kind;
        Location location;
        std::optional<Location> selectionRange;
        std::vector<DocumentSymbol> children;
    };
    
    std::vector<DocumentSymbol> getDocumentSymbols(const std::string& uri,
                                                    const std::string& content);
    
    
    struct RecoveryStrategy {
        bool useCache = true;
        bool partialResults = true;
        int maxWaitTimeMs = 500;
    };
    
    void setRecoveryStrategy(const RecoveryStrategy& strategy);

private:
    WorkspaceSymbolIndex* m_index;
    RecoveryStrategy m_recoveryStrategy;
    
    // Cache for frequently accessed info
    mutable std::unordered_map<std::string, HoverInformation> m_hoverCache;
    
    
    std::string extractTokenAtLocation(const std::string& content,
                                      const Location& loc);
    bool isLargeFile(const std::string& content) const;
    void gracefullyDegrade(const std::string& reason);
};

} // namespace RawrXD::LSP
