// ============================================================================
// advanced_intellisense.h — Phase 3: Advanced IntelliSense
// ============================================================================
// Context-aware completion ranking, snippet expansion, signature help with
// overload resolution, type inference, and ML-enhanced suggestion ranking
// (simple TF-IDF based).
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp/intellisense_completion.h"
#include "lsp/treesitter_parser.h"
#include "lsp/language_registry.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <cmath>

namespace RawrXD::LSP {

// ---------------------------------------------------------------------------
// Snippet Placeholder
// ---------------------------------------------------------------------------
struct SnippetPlaceholder {
    int index = 0;
    std::string defaultValue;
    std::string description;
    uint32_t startPos = 0;
    uint32_t endPos = 0;
};

// ---------------------------------------------------------------------------
// Snippet
// ---------------------------------------------------------------------------
struct Snippet {
    std::string name;
    std::string prefix;
    std::string body;
    std::string description;
    std::vector<SnippetPlaceholder> placeholders;
    CompletionItemKind kind = CompletionItemKind::Snippet;
    std::vector<std::string> scopes;  // e.g., "global", "class", "function"
};

// ---------------------------------------------------------------------------
// Signature Help with Overload Resolution
// ---------------------------------------------------------------------------
struct OverloadInfo {
    std::string label;
    std::string documentation;
    std::vector<std::string> parameters;
    std::vector<std::string> parameterTypes;
    std::vector<std::string> parameterDocs;
    int activeParameter = 0;
    bool isVariadic = false;
};

struct SignatureHelpResult {
    std::vector<OverloadInfo> overloads;
    int activeOverload = 0;
    int activeParameter = 0;
    bool hasExactMatch = false;
};

// ---------------------------------------------------------------------------
// Type Inference Result
// ---------------------------------------------------------------------------
struct InferredType {
    std::string typeName;
    float confidence = 0.0f;  // 0.0 - 1.0
    bool isNullable = false;
    bool isArray = false;
    bool isGeneric = false;
    std::vector<std::string> typeParameters;
};

// ---------------------------------------------------------------------------
// TF-IDF Model for ML Ranking
// ---------------------------------------------------------------------------
struct TfIdfEntry {
    std::unordered_map<std::string, float> termFreq;
    float docNorm = 0.0f;
};

class TfIdfModel {
public:
    TfIdfModel();
    ~TfIdfModel();

    void addDocument(const std::string& docId,
                      const std::vector<std::string>& tokens);
    void removeDocument(const std::string& docId);

    // Score a query against all documents
    std::vector<std::pair<std::string, float>> scoreQuery(
        const std::vector<std::string>& queryTokens) const;

    void clear();
    size_t documentCount() const;

private:
    std::unordered_map<std::string, TfIdfEntry> m_documents;
    std::unordered_map<std::string, size_t> m_docFreq;
    mutable std::shared_mutex m_mutex;

    std::vector<std::string> tokenize(const std::string& text) const;
    float computeIdf(size_t docFreq, size_t totalDocs) const;
};

// ---------------------------------------------------------------------------
// Advanced IntelliSense Engine
// ---------------------------------------------------------------------------
class AdvancedIntelliSense {
public:
    explicit AdvancedIntelliSense(WorkspaceSymbolIndex* index,
                                   TreeSitterParser* parser = nullptr);
    ~AdvancedIntelliSense();

    // === Context-Aware Completion ===
    CompletionList getContextCompletions(
        const CompletionParams& params,
        const std::string& currentFileContent,
        const std::string& languageId = "cpp");

    // === Snippet Expansion ===
    std::vector<Snippet> getSnippets(const std::string& languageId,
                                       const std::string& scope = "global");
    std::optional<Snippet> resolveSnippet(const std::string& name,
                                            const std::string& languageId);
    std::string expandSnippet(const Snippet& snippet,
                               const std::vector<std::string>& values);

    // === Signature Help ===
    SignatureHelpResult getSignatureHelp(const std::string& uri,
                                          const std::string& content,
                                          uint32_t line,
                                          uint32_t column,
                                          const std::string& languageId = "cpp");

    // === Type Inference ===
    std::optional<InferredType> inferTypeAtPosition(
        const std::string& uri,
        const std::string& content,
        uint32_t line,
        uint32_t column,
        const std::string& languageId = "cpp");

    std::optional<InferredType> inferTypeOfSymbol(
        const std::string& symbolName,
        const std::shared_ptr<ASTNode>& scope);

    // === ML-Enhanced Ranking (TF-IDF) ===
    void trainOnWorkspace(const std::vector<std::string>& fileUris,
                          const std::vector<std::string>& contents);
    std::vector<CompletionItem> rankWithMl(
        const std::vector<CompletionItem>& candidates,
        const std::string& context);

    // === Metrics ===
    struct IntelliSenseMetrics {
        double avgCompletionTimeMs = 0.0;
        double avgSignatureTimeMs = 0.0;
        double avgTypeInferenceTimeMs = 0.0;
        size_t totalCompletions = 0;
        size_t cacheHits = 0;
        float mlRankingAccuracy = 0.0f;  // Estimated
    };

    IntelliSenseMetrics getMetrics() const;
    void clearMetrics();

private:
    WorkspaceSymbolIndex* m_index;
    TreeSitterParser* m_parser;
    std::unique_ptr<TfIdfModel> m_tfIdf;

    // Snippet databases per language
    std::unordered_map<std::string, std::vector<Snippet>> m_snippetDb;
    mutable std::mutex m_snippetMutex;

    mutable struct {
        std::vector<double> completionTimes;
        std::vector<double> signatureTimes;
        std::vector<double> inferenceTimes;
        size_t totalCompletions = 0;
        size_t cacheHits = 0;
    } m_metrics;
    mutable std::mutex m_metricsMutex;

    // Initialization
    void initializeSnippets();
    void initializeCppSnippets();
    void initializePythonSnippets();
    void initializeJavaScriptSnippets();
    void initializeRustSnippets();

    // Ranking
    float calculateContextScore(const SymbolInfo& symbol,
                                 const ContextAnalysis& context,
                                 const std::string& prefix);
    float calculateFrequencyScore(const std::string& symbolName);
    float calculateMlScore(const std::string& symbolName,
                            const std::string& context);

    // Signature help helpers
    std::vector<OverloadInfo> resolveOverloads(const std::string& funcName,
                                                const std::string& languageId);
    int findActiveParameter(const std::string& lineContent,
                            uint32_t column);

    // Type inference helpers
    std::optional<InferredType> inferFromAssignment(
        const std::shared_ptr<ASTNode>& node);
    std::optional<InferredType> inferFromCall(
        const std::shared_ptr<ASTNode>& node);
    std::optional<InferredType> inferFromLiteral(
        const std::shared_ptr<ASTNode>& node);
};

} // namespace RawrXD::LSP
