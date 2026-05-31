// ============================================================================
// composer_mode.h — AI-Powered Multi-File Composition Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// Reverse-engineered Composer Mode inspired by Cursor IDE
// Handles multi-file changes, autonomous editing, and "Crazy Mode" refactoring
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <optional>

namespace RawrXD::Composer {

// ============================================================================
// Core Types
// ============================================================================

enum class ChangeType {
    Insert,
    Delete,
    Replace,
    Move,
    Rename
};

enum class ConflictResolution {
    AcceptOurs,
    AcceptTheirs,
    Merge,
    Manual
};

enum class CompositionMode {
    Normal,         // Standard multi-file editing
    Aggressive,     // More autonomous, less confirmation
    Crazy           // Full autonomous refactoring with minimal guardrails
};

enum class CompositionStatus {
    Pending,
    Analyzing,
    Planning,
    Executing,
    Reviewing,
    Completed,
    Failed,
    RolledBack
};

// ============================================================================
// File Change Representation
// ============================================================================

struct TextRange {
    uint32_t startLine = 0;
    uint32_t startColumn = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    
    bool operator==(const TextRange& other) const {
        return startLine == other.startLine && 
               startColumn == other.startColumn &&
               endLine == other.endLine && 
               endColumn == other.endColumn;
    }
    
    bool overlaps(const TextRange& other) const {
        if (endLine < other.startLine || startLine > other.endLine) return false;
        if (endLine == other.startLine && endColumn < other.startColumn) return false;
        if (startLine == other.endLine && startColumn > other.endColumn) return false;
        return true;
    }
};

struct FileChange {
    std::string uri;
    ChangeType type = ChangeType::Replace;
    TextRange range;
    std::string originalContent;
    std::string newContent;
    std::string description;
    uint32_t changeId = 0;
    uint32_t groupId = 0;
    bool isApplied = false;
    bool isConfirmed = false;
    float confidence = 0.0f;
    
    // For move/rename operations
    std::string targetUri;
    
    // Metadata
    std::string author;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

struct ChangeGroup {
    uint32_t groupId = 0;
    std::string name;
    std::string description;
    std::vector<uint32_t> changeIds;
    bool isAtomic = true;  // All or nothing
    bool isApplied = false;
    bool isConfirmed = false;
    float overallConfidence = 0.0f;
};

struct FileConflict {
    std::string uri;
    std::vector<FileChange> ourChanges;
    std::vector<FileChange> theirChanges;
    ConflictResolution resolution = ConflictResolution::Manual;
    std::string mergedContent;
};

// ============================================================================
// Composition Plan
// ============================================================================

struct CompositionStep {
    uint32_t stepId = 0;
    std::string description;
    std::vector<uint32_t> changeIds;
    std::vector<uint32_t> dependencies;
    bool isComplete = false;
    bool isSkipped = false;
    std::string skipReason;
    std::optional<std::string> error;
};

struct CompositionPlan {
    uint32_t planId = 0;
    std::string title;
    std::string description;
    std::string userPrompt;
    CompositionMode mode = CompositionMode::Normal;
    CompositionStatus status = CompositionStatus::Pending;
    
    std::vector<CompositionStep> steps;
    std::vector<FileChange> changes;
    std::vector<ChangeGroup> groups;
    std::vector<FileConflict> conflicts;
    
    uint32_t currentStep = 0;
    uint32_t totalChanges = 0;
    uint32_t appliedChanges = 0;
    uint32_t failedChanges = 0;
    
    float overallConfidence = 0.0f;
    float estimatedComplexity = 0.0f;
    
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point started;
    std::chrono::system_clock::time_point completed;
    
    std::unordered_map<std::string, std::string> metadata;
};

// ============================================================================
// Context & Dependencies
// ============================================================================

struct FileContext {
    std::string uri;
    std::string content;
    std::string language;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;
    std::unordered_map<std::string, std::string> symbols;
};

struct ProjectContext {
    std::vector<FileContext> files;
    std::unordered_map<std::string, std::vector<std::string>> dependencyGraph;
    std::unordered_map<std::string, std::vector<std::string>> reverseDependencyGraph;
    std::unordered_set<std::string> modifiedFiles;
    std::unordered_set<std::string> affectedFiles;
};

// ============================================================================
// Composition Result
// ============================================================================

struct CompositionResult {
    uint32_t planId = 0;
    CompositionStatus status = CompositionStatus::Pending;
    
    uint32_t totalChanges = 0;
    uint32_t appliedChanges = 0;
    uint32_t skippedChanges = 0;
    uint32_t failedChanges = 0;
    uint32_t rolledBackChanges = 0;
    
    std::vector<FileChange> applied;
    std::vector<FileChange> skipped;
    std::vector<FileChange> failed;
    std::vector<FileChange> rolledBack;
    
    std::vector<FileConflict> unresolvedConflicts;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    std::chrono::milliseconds duration{0};
    float finalConfidence = 0.0f;
    
    std::string summary;
    std::string detailedReport;
};

// ============================================================================
// Callbacks & Events
// ============================================================================

enum class CompositionEvent {
    PlanCreated,
    PlanStarted,
    StepStarted,
    StepCompleted,
    ChangeApplied,
    ChangeFailed,
    ConflictDetected,
    ConflictResolved,
    PlanCompleted,
    PlanFailed,
    PlanRolledBack
};

struct CompositionEventArgs {
    CompositionEvent event;
    uint32_t planId = 0;
    uint32_t stepId = 0;
    uint32_t changeId = 0;
    std::string message;
    std::optional<FileChange> change;
    std::optional<FileConflict> conflict;
    float progress = 0.0f;
};

using CompositionCallback = std::function<void(const CompositionEventArgs&)>;

// ============================================================================
// Composer Configuration
// ============================================================================

struct ComposerConfig {
    CompositionMode defaultMode = CompositionMode::Normal;
    uint32_t maxChangesPerPlan = 500;
    uint32_t maxFilesPerPlan = 50;
    uint32_t maxConcurrentChanges = 10;
    float minConfidenceThreshold = 0.7f;
    bool autoConfirmHighConfidence = true;
    float autoConfirmThreshold = 0.95f;
    bool enableAutoRollback = true;
    bool enableConflictDetection = true;
    bool enableDependencyAnalysis = true;
    bool enableSemanticValidation = true;
    uint32_t maxRollbackDepth = 100;
    bool preserveUndoHistory = true;
    bool enableCrazyMode = false;
    uint32_t crazyModeMaxChanges = 1000;
    float crazyModeConfidenceThreshold = 0.5f;
    std::vector<std::string> excludedPatterns;
    std::vector<std::string> protectedFiles;
};

// ============================================================================
// Composer Engine Interface
// ============================================================================

class IComposerEngine {
public:
    virtual ~IComposerEngine() = default;
    
    // Configuration
    virtual void setConfig(const ComposerConfig& config) = 0;
    virtual ComposerConfig getConfig() const = 0;
    
    // Event handling
    virtual void setCallback(CompositionCallback callback) = 0;
    
    // Core operations
    virtual std::unique_ptr<CompositionPlan> createPlan(
        const std::string& prompt,
        const std::vector<std::string>& fileUris,
        CompositionMode mode = CompositionMode::Normal) = 0;
    
    virtual bool executePlan(CompositionPlan& plan) = 0;
    virtual bool rollbackPlan(CompositionPlan& plan) = 0;
    virtual bool pausePlan(CompositionPlan& plan) = 0;
    virtual bool resumePlan(CompositionPlan& plan) = 0;
    
    // Change management
    virtual bool applyChange(FileChange& change) = 0;
    virtual bool revertChange(FileChange& change) = 0;
    virtual bool confirmChange(FileChange& change) = 0;
    
    // Conflict resolution
    virtual bool detectConflicts(CompositionPlan& plan) = 0;
    virtual bool resolveConflict(FileConflict& conflict, ConflictResolution resolution) = 0;
    
    // Analysis
    virtual ProjectContext analyzeProject(const std::vector<std::string>& fileUris) = 0;
    virtual std::vector<FileChange> suggestChanges(
        const std::string& prompt,
        const ProjectContext& context) = 0;
    
    // Status
    virtual CompositionStatus getStatus(uint32_t planId) const = 0;
    virtual float getProgress(uint32_t planId) const = 0;
    virtual std::optional<CompositionResult> getResult(uint32_t planId) const = 0;
};

// ============================================================================
// Composer Factory
// ============================================================================

std::unique_ptr<IComposerEngine> createComposerEngine();

} // namespace RawrXD::Composer
