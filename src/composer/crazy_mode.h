// ============================================================================
// crazy_mode.h — Autonomous Multi-File Refactoring Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// "Crazy Mode" - Full autonomous refactoring with minimal guardrails
// Inspired by Cursor's Composer but with more aggressive autonomous operation
// ============================================================================

#pragma once

#include "composer/composer_mode.h"
#include <functional>
#include <future>
#include <queue>

namespace RawrXD::Composer {

// ============================================================================
// Crazy Mode Configuration
// ============================================================================

struct CrazyModeConfig {
    // Autonomy settings
    bool enableFullAutonomy = true;
    bool enableCrossFileRefactoring = true;
    bool enableSymbolicRename = true;
    bool enableExtractMethod = true;
    bool enableInlineMethod = true;
    bool enableMoveSymbol = true;
    bool enableDeleteDeadCode = true;
    bool enableOptimizeImports = true;
    bool enableFixStyle = true;
    bool enableAddTypeAnnotations = true;
    
    // Safety limits
    uint32_t maxFilesPerOperation = 100;
    uint32_t maxChangesPerFile = 50;
    uint32_t maxTotalChanges = 2000;
    float minConfidenceForAutoApply = 0.6f;
    float minConfidenceForDestructiveOps = 0.8f;
    
    // Protected patterns
    std::vector<std::string> protectedFilePatterns = {
        "*.lock", "*.min.js", "*.min.css", "package-lock.json",
        "yarn.lock", "pnpm-lock.yaml", "*.generated.*"
    };
    
    std::vector<std::string> protectedDirectories = {
        "node_modules", "vendor", "third_party", "3rdparty",
        "external", "deps", "build", "dist"
    };
    
    // Rollback settings
    bool enableAutoCheckpoint = true;
    uint32_t checkpointInterval = 10; // Every N changes
    uint32_t maxCheckpoints = 20;
    bool enableAutoRollbackOnError = true;
    float errorThresholdForRollback = 0.3f; // Rollback if >30% errors
    
    // Performance
    uint32_t maxConcurrentAnalysis = 4;
    uint32_t analysisTimeoutMs = 30000;
    uint32_t applyTimeoutMs = 5000;
};

// ============================================================================
// Refactoring Operation Types
// ============================================================================

enum class RefactorType {
    RenameSymbol,
    ExtractMethod,
    ExtractVariable,
    InlineMethod,
    InlineVariable,
    MoveSymbol,
    DeleteDeadCode,
    OptimizeImports,
    FixStyleIssues,
    AddTypeAnnotations,
    ConvertToArrowFunction,
    ConvertToAsync,
    SimplifyCondition,
    RemoveUnused,
    ExtractInterface,
    ImplementInterface,
    GenerateDocumentation,
    Custom
};

struct RefactorOperation {
    RefactorType type = RefactorType::Custom;
    std::string name;
    std::string description;
    
    // Target information
    std::string symbolName;
    std::string sourceUri;
    TextRange sourceRange;
    std::string targetUri;
    std::string newName;
    
    // Generated changes
    std::vector<FileChange> changes;
    
    // Metadata
    float confidence = 0.0f;
    bool isDestructive = false;
    bool requiresConfirmation = true;
    std::vector<std::string> affectedFiles;
    std::vector<std::string> warnings;
};

// ============================================================================
// Symbol Analysis
// ============================================================================

struct SymbolInfo {
    std::string name;
    std::string qualifiedName;
    std::string type;
    std::string signature;
    std::string documentation;
    
    std::string sourceUri;
    TextRange definitionRange;
    std::vector<TextRange> referenceRanges;
    
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;
    
    bool isExported = false;
    bool isUsed = false;
    bool isDeprecated = false;
};

struct SymbolTable {
    std::unordered_map<std::string, SymbolInfo> symbols;
    std::unordered_map<std::string, std::vector<std::string>> symbolsByFile;
    std::unordered_map<std::string, std::vector<std::string>> referencesToSymbol;
};

// ============================================================================
// Analysis Results
// ============================================================================

struct DeadCodeAnalysis {
    std::vector<SymbolInfo> unusedSymbols;
    std::vector<TextRange> unreachableCode;
    std::vector<std::string> unusedImports;
    std::vector<std::string> redundantCode;
};

struct StyleIssue {
    std::string uri;
    TextRange range;
    std::string ruleId;
    std::string message;
    std::string suggestedFix;
    Severity severity = Severity::Info;
};

struct ComplexityHotspot {
    std::string uri;
    std::string symbolName;
    uint32_t cyclomaticComplexity = 0;
    uint32_t cognitiveComplexity = 0;
    uint32_t linesOfCode = 0;
    uint32_t nestingLevel = 0;
    float maintainabilityIndex = 0.0f;
    std::vector<std::string> issues;
};

// ============================================================================
// Checkpoint System
// ============================================================================

struct Checkpoint {
    uint32_t checkpointId = 0;
    std::chrono::system_clock::time_point created;
    std::string description;
    std::unordered_map<std::string, std::string> fileSnapshots;
    std::vector<FileChange> appliedChanges;
    uint32_t changeCount = 0;
};

// ============================================================================
// Crazy Mode Engine Interface
// ============================================================================

class ICrazyModeEngine {
public:
    virtual ~ICrazyModeEngine() = default;
    
    // Configuration
    virtual void setConfig(const CrazyModeConfig& config) = 0;
    virtual CrazyModeConfig getConfig() const = 0;
    
    // Core operations
    virtual std::future<std::vector<RefactorOperation>> analyzeCodebase(
        const std::vector<std::string>& fileUris) = 0;
    
    virtual std::future<bool> executeRefactoring(
        std::vector<RefactorOperation> operations,
        bool autoConfirm = false) = 0;
    
    // Specific refactoring operations
    virtual RefactorOperation renameSymbol(
        const std::string& symbolName,
        const std::string& newName,
        const std::vector<std::string>& fileUris) = 0;
    
    virtual RefactorOperation extractMethod(
        const std::string& uri,
        const TextRange& range,
        const std::string& methodName) = 0;
    
    virtual RefactorOperation inlineMethod(
        const std::string& uri,
        const TextRange& callSite) = 0;
    
    virtual RefactorOperation moveSymbol(
        const std::string& symbolName,
        const std::string& sourceUri,
        const std::string& targetUri) = 0;
    
    // Analysis operations
    virtual DeadCodeAnalysis findDeadCode(
        const std::vector<std::string>& fileUris) = 0;
    
    virtual std::vector<StyleIssue> findStyleIssues(
        const std::vector<std::string>& fileUris) = 0;
    
    virtual std::vector<ComplexityHotspot> findComplexityHotspots(
        const std::vector<std::string>& fileUris) = 0;
    
    virtual SymbolTable buildSymbolTable(
        const std::vector<std::string>& fileUris) = 0;
    
    // Checkpoint management
    virtual uint32_t createCheckpoint(const std::string& description) = 0;
    virtual bool restoreCheckpoint(uint32_t checkpointId) = 0;
    virtual std::vector<Checkpoint> getCheckpoints() const = 0;
    virtual bool deleteCheckpoint(uint32_t checkpointId) = 0;
    
    // Status
    virtual bool isRunning() const = 0;
    virtual float getProgress() const = 0;
    virtual std::string getCurrentOperation() const = 0;
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<ICrazyModeEngine> createCrazyModeEngine();

} // namespace RawrXD::Composer
