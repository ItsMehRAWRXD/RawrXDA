// Cross-file rename planning and conflict reporting over indexed symbols.
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
#pragma once

#include "lsp/workspace_symbol_index.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

namespace RawrXD::LSP {

enum class RenameError : uint8_t {
    None = 0,
    SymbolNotFound = 1,
    NameCollision = 2,
    InvalidNewName = 3,
    ReferenceUpdateFailed = 4,
    FileLocked = 5,
    PermissionDenied = 6,
    Unknown = 255,
};

struct TextEdit {
    Location location;
    std::string replacementText;
    bool isApplied = false;
    RenameError lastError = RenameError::None;
};

struct RenameConflict {
    std::string existingSymbol;  // Name that would collide
    Location conflictLocation;    // Where the collision would occur
    std::string suggestion;       // Suggested alternative name
};

struct RenameResult {
    bool success = false;
    RenameError error = RenameError::None;
    std::string errorMessage;
    
    // Which edits were applied
    std::vector<TextEdit> appliedEdits;
    std::vector<TextEdit> failedEdits;
    
    // Conflicts detected
    std::vector<RenameConflict> conflicts;
    
    // Performance metrics
    int64_t durationMs = 0;
    size_t filesAffected = 0;
    size_t referencesUpdated = 0;
};

class CrossFileRenameEngine {
public:
    explicit CrossFileRenameEngine(WorkspaceSymbolIndex* index);
    ~CrossFileRenameEngine();

    // === Core Rename Operations ===
    
    // Prepare rename: check feasibility without applying changes
    struct PrepareRenameRequest {
        std::string oldFqn;        // Fully-qualified name to rename
        std::string newName;       // New simple name
        bool dryRun = false;       // If true, don't apply but simulate
    };
    
    struct PrepareRenameResult {
        bool canRename = false;
        std::vector<RenameConflict> potentialConflicts;
        size_t estimatedReferencesToUpdate = 0;
        std::string reason;  // Why rename might not be possible
    };
    
    PrepareRenameResult prepareRename(const PrepareRenameRequest& req);
    
    // Execute rename: apply changes and update all references
    RenameResult executeRename(const std::string& oldFqn,
                               const std::string& newName);
    
    // Dry run: simulate rename without applying
    RenameResult dryRunRename(const std::string& oldFqn,
                              const std::string& newName);
    
    // Rollback: revert previous rename operation
    RenameResult rollback(const RenameResult& previousResult);
    
    // === Validation ===
    
    bool isValidSymbolName(const std::string& name) const;
    bool wouldCauseNameCollision(const std::string& fqn,
                                 const std::string& newName) const;
    
    // === Batch Rename ===
    
    // Rename multiple symbols at once (atomic operation)
    struct BatchRenameRequest {
        std::vector<std::pair<std::string, std::string>> renames;  // {oldFqn, newName}
    };
    
    struct BatchRenameResult {
        bool success = false;
        std::vector<RenameResult> individualResults;
        bool allSucceeded = false;
    };
    
    BatchRenameResult executeBatchRename(const BatchRenameRequest& req);

private:
    WorkspaceSymbolIndex* m_index;
    
    // For rollback support
    struct RenameSnapshot {
        std::string oldFqn;
        std::string newFqn;
        std::vector<std::pair<std::string, std::string>> originalContent;  // {uri, content}
        int64_t timestamp;
    };
    
    std::vector<RenameSnapshot> m_history;
    
    // === Helper Methods ===
    
    bool validateNewName(const std::string& name) const;
    std::vector<TextEdit> findAllReferences(const std::string& oldFqn);
    bool updateReferences(const std::string& oldFqn, const std::string& newFqn,
                          const std::vector<TextEdit>& edits,
                          RenameResult& result);
    std::string generateAlternativeName(const std::string& preferred) const;
    void captureSnapshot(const std::string& oldFqn, const std::string& newFqn);
};

struct SearchOptions {
    bool caseSensitive = false;
    bool wholeWordsOnly = false;
    bool useRegex = false;
    size_t maxResults = 1000;
    std::string filePattern;  // Limit search to files matching pattern
};

struct SearchResult {
    SymbolInfo symbol;
    std::vector<Location> locations;  // Where this symbol is referenced
    float relevanceScore;              // 0.0 - 1.0 based on context match
    int64_t distanceFromCurrentFile;   // Files apart from working document (heuristic)
};

class GlobalSymbolSearch {
public:
    explicit GlobalSymbolSearch(WorkspaceSymbolIndex* index);
    ~GlobalSymbolSearch();

    // === Search Operations ===
    
    // Find symbol by name (fast prefix/exact match)
    std::vector<SearchResult> findSymbol(const std::string& name,
                                         const SearchOptions& opts = SearchOptions());
    
    // Find all references to a symbol
    std::vector<SearchResult> findReferences(const std::string& fqn,
                                             const SearchOptions& opts = SearchOptions());
    
    // Find symbols by pattern (regex if enabled in options)
    std::vector<SearchResult> findByPattern(const std::string& pattern,
                                            const SearchOptions& opts = SearchOptions());
    
    // Find symbols in specific scope/container
    std::vector<SearchResult> findInScope(const std::string& scopeName,
                                          const SearchOptions& opts = SearchOptions());
    
    // === Ranking and Results ===
    
    // Get search results ranked by relevance
    struct RankedResults {
        std::vector<SearchResult> results;
        double avgSearchTimeMs;
        size_t totalMatches;
        bool isComplete;  // false if results were truncated
    };
    
    RankedResults getRankedResults(const std::vector<SearchResult>& results,
                                   const std::string& currentFileUri = "",
                                   bool sortByDistance = true);
    
    // === Performance ===
    
    struct SearchMetrics {
        double totalSearchTimeMs;
        size_t queriesExecuted;
        double avgQueryTimeMs;
        double p99QueryTimeMs;
        double p95QueryTimeMs;
    };
    
    SearchMetrics getMetrics() const;
    void clearMetrics();

private:
    WorkspaceSymbolIndex* m_index;
    
    // Performance tracking
    mutable struct {
        double totalTimeMs = 0.0;
        std::vector<double> queryTimes;
        size_t totalQueries = 0;
    } m_metrics;
    
    // === Helper Methods ===
    
    float calculateRelevance(const SearchResult& result,
                            const std::string& query) const;
    int64_t calculateDistance(const Location& loc1,
                             const Location& loc2) const;
    bool matchesPattern(const std::string& text, const std::string& pattern,
                       bool regex, bool caseSensitive) const;
    std::vector<SearchResult> rankResults(std::vector<SearchResult>& results,
                                          const std::string& currentFileUri);
};

} // namespace RawrXD::LSP
