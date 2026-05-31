// ============================================================================
// workspace_operations.h — Phase 3: Enhanced Workspace Operations
// ============================================================================
// Cross-file rename with AST-based dependency tracking, global "Find All
// References", project-wide refactoring, and multi-file error detection.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp/crossfile_rename_engine.h"
#include "lsp/treesitter_parser.h"
#include "lsp/language_registry.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <chrono>

namespace RawrXD::LSP {

// ---------------------------------------------------------------------------
// Dependency Edge (for cross-file dependency graph)
// ---------------------------------------------------------------------------
struct DependencyEdge {
    std::string fromUri;
    std::string toUri;
    std::string symbolName;
    Location location;
    bool isImport = false;
    bool isExport = false;
};

// ---------------------------------------------------------------------------
// Dependency Graph
// ---------------------------------------------------------------------------
class DependencyGraph {
public:
    DependencyGraph();
    ~DependencyGraph();

    void addEdge(const DependencyEdge& edge);
    void removeFile(const std::string& uri);
    void clear();

    // Get files that depend on a given file
    std::vector<std::string> getDependents(const std::string& uri) const;

    // Get files that a given file depends on
    std::vector<std::string> getDependencies(const std::string& uri) const;

    // Get all files in dependency order (topological)
    std::vector<std::string> getTopologicalOrder() const;

    // Find strongly connected components (circular deps)
    std::vector<std::vector<std::string>> findCycles() const;

    // Check if two files are in the same SCC
    bool isMutuallyDependent(const std::string& a, const std::string& b) const;

    size_t edgeCount() const;
    size_t nodeCount() const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::vector<DependencyEdge>> m_outgoing;
    std::unordered_map<std::string, std::vector<DependencyEdge>> m_incoming;
    std::unordered_set<std::string> m_nodes;
};

// ---------------------------------------------------------------------------
// Find All References Result
// ---------------------------------------------------------------------------
struct ReferenceResult {
    Location location;
    bool isDefinition = false;
    bool isDeclaration = false;
    bool isWrite = false;
    std::string containingSymbol;
    float relevance = 1.0f;
};

// ---------------------------------------------------------------------------
// Refactoring Operation Types
// ---------------------------------------------------------------------------
enum class RefactoringKind : uint8_t {
    ExtractMethod = 0,
    InlineVariable = 1,
    ExtractInterface = 2,
    MoveMethod = 3,
    RenameNamespace = 4,
    OrganizeImports = 5,
};

struct RefactoringEdit {
    std::string uri;
    std::vector<TextEdit> edits;
    std::string description;
};

struct RefactoringResult {
    bool success = false;
    std::string errorMessage;
    std::vector<RefactoringEdit> edits;
    std::vector<RenameConflict> conflicts;
    int64_t durationMs = 0;
    size_t filesAffected = 0;
};

// ---------------------------------------------------------------------------
// Multi-File Diagnostic
// ---------------------------------------------------------------------------
struct CrossFileDiagnostic {
    std::string uri;
    Location location;
    std::string message;
    int severity = 1;  // 1=error, 2=warning, 3=info, 4=hint
    std::string code;
    std::string source;
    std::vector<std::string> relatedUris;  // Files involved
};

// ---------------------------------------------------------------------------
// Workspace Operations Engine
// ---------------------------------------------------------------------------
class WorkspaceOperations {
public:
    explicit WorkspaceOperations(WorkspaceSymbolIndex* index,
                                  TreeSitterParser* parser = nullptr);
    ~WorkspaceOperations();

    // === Cross-File Rename with AST Accuracy ===
    RenameResult executeAstRename(const std::string& oldFqn,
                                   const std::string& newName,
                                   const std::vector<std::string>& fileUris);

    RenameResult prepareAstRename(const std::string& oldFqn,
                                   const std::string& newName,
                                   const std::vector<std::string>& fileUris);

    // === Find All References (AST-accurate) ===
    std::vector<ReferenceResult> findAllReferences(
        const std::string& fqn,
        const std::vector<std::string>& fileUris,
        bool includeDeclarations = true);

    // === Project-Wide Refactoring ===
    RefactoringResult extractMethod(const std::string& uri,
                                     const std::string& content,
                                     uint32_t startLine,
                                     uint32_t startCol,
                                     uint32_t endLine,
                                     uint32_t endCol,
                                     const std::string& methodName);

    RefactoringResult inlineVariable(const std::string& uri,
                                      const std::string& content,
                                      uint32_t line,
                                      uint32_t column);

    RefactoringResult organizeImports(const std::string& uri,
                                       const std::string& content);

    // === Multi-File Diagnostics ===
    std::vector<CrossFileDiagnostic> analyzeCrossFileDependencies(
        const std::vector<std::string>& fileUris);

    std::vector<CrossFileDiagnostic> detectUnusedSymbols(
        const std::vector<std::string>& fileUris);

    // === Dependency Graph ===
    void buildDependencyGraph(const std::vector<std::string>& fileUris,
                               const std::vector<std::pair<std::string, std::string>>&
                                   fileContents);
    DependencyGraph* dependencyGraph();

    // === Workspace Symbol Search (<500ms target) ===
    struct SearchQuery {
        std::string pattern;
        bool caseSensitive = false;
        bool wholeWords = false;
        std::vector<SymbolKind> kinds;
        std::string containerFilter;
        size_t maxResults = 100;
    };

    struct WorkspaceSearchResult {
        SymbolInfo symbol;
        float relevanceScore = 0.0f;
        int64_t searchTimeUs = 0;
    };

    std::vector<WorkspaceSearchResult> workspaceSymbolSearch(
        const SearchQuery& query);

    // === Performance ===
    struct OperationMetrics {
        double avgRenameTimeMs = 0.0;
        double avgReferenceTimeMs = 0.0;
        double avgRefactorTimeMs = 0.0;
        double avgSearchTimeMs = 0.0;
        size_t totalOperations = 0;
    };

    OperationMetrics getMetrics() const;
    void clearMetrics();

private:
    WorkspaceSymbolIndex* m_index;
    TreeSitterParser* m_parser;
    std::unique_ptr<DependencyGraph> m_depGraph;
    std::unique_ptr<CrossFileRenameEngine> m_renameEngine;

    mutable struct {
        std::vector<double> renameTimes;
        std::vector<double> referenceTimes;
        std::vector<double> refactorTimes;
        std::vector<double> searchTimes;
        size_t totalOps = 0;
    } m_metrics;
    mutable std::mutex m_metricsMutex;

    // Helper methods
    std::vector<ReferenceResult> findReferencesInFile(
        const std::string& uri,
        const std::string& content,
        const std::string& symbolName);

    bool isValidRenameTarget(const std::shared_ptr<ASTNode>& node);
    std::vector<TextEdit> generateRenameEdits(
        const std::shared_ptr<ASTNode>& root,
        const std::string& oldName,
        const std::string& newName);

    float calculateRelevance(const SymbolInfo& sym,
                              const SearchQuery& query);
};

} // namespace RawrXD::LSP
