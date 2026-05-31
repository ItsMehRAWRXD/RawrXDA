// ============================================================================
// interactive_refactoring.h — Interactive Refactoring Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// Reverse-engineered from Aider's conversational code editing with:
// - Real-time diff visualization
// - Multi-file coordination
// - Rollback capabilities
// - Context-aware suggestions
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>

namespace RawrXD {
namespace Refactoring {

// ============================================================================
// Core Types
// ============================================================================

struct Position {
    uint32_t line;
    uint32_t column;
    
    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    
    bool operator<(const Position& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

struct Range {
    Position start;
    Position end;
    
    bool contains(const Position& pos) const {
        return start <= pos && pos <= end;
    }
    
    bool overlaps(const Range& other) const {
        return contains(other.start) || contains(other.end) ||
               other.contains(start) || other.contains(end);
    }
};

struct TextEdit {
    Range range;
    std::string newText;
    std::string oldText;
    std::string description;
    
    // Metadata
    std::string id;
    std::string source;
    std::chrono::system_clock::time_point timestamp;
    bool applied = false;
    bool confirmed = false;
};

struct FileChange {
    std::string uri;
    std::vector<TextEdit> edits;
    std::string originalContent;
    std::string modifiedContent;
    std::chrono::system_clock::time_point timestamp;
    
    // Diff statistics
    uint32_t linesAdded = 0;
    uint32_t linesRemoved = 0;
    uint32_t linesModified = 0;
};

struct RefactoringSession {
    std::string id;
    std::string description;
    std::vector<FileChange> changes;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    bool completed = false;
    bool rolledBack = false;
    
    // Session metadata
    std::string model;
    std::string prompt;
    std::vector<std::string> contextFiles;
};

// ============================================================================
// Diff Types
// ============================================================================

enum class DiffType {
    Addition,
    Deletion,
    Modification,
    Context
};

struct DiffLine {
    DiffType type;
    uint32_t oldLine;
    uint32_t newLine;
    std::string content;
    std::string id;
};

struct DiffHunk {
    uint32_t oldStart;
    uint32_t oldCount;
    uint32_t newStart;
    uint32_t newCount;
    std::vector<DiffLine> lines;
    std::string header;
};

struct UnifiedDiff {
    std::string oldFile;
    std::string newFile;
    std::vector<DiffHunk> hunks;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toString() const;
    static std::optional<UnifiedDiff> fromString(const std::string& diff);
};

// ============================================================================
// Conversation Types
// ============================================================================

struct ConversationMessage {
    enum class Role { User, Assistant, System } role;
    std::string content;
    std::vector<TextEdit> proposedEdits;
    std::chrono::system_clock::time_point timestamp;
    std::string id;
};

struct ConversationContext {
    std::vector<std::string> activeFiles;
    std::vector<std::string> referencedSymbols;
    std::vector<std::string> previousEdits;
    std::unordered_map<std::string, std::string> fileContents;
};

// ============================================================================
// Refactoring Operations
// ============================================================================

enum class RefactoringKind {
    Rename,
    ExtractFunction,
    ExtractVariable,
    ExtractMethod,
    InlineFunction,
    InlineVariable,
    MoveFunction,
    MoveClass,
    ChangeSignature,
    IntroduceParameter,
    IntroduceField,
    EncapsulateField,
    ReplaceMagicNumber,
    SplitVariable,
    MergeVariables,
    SplitFunction,
    MergeFunctions,
    AddParameter,
    RemoveParameter,
    ReorderParameters,
    ConvertToArrowFunction,
    ConvertToRegularFunction,
    AddTypeAnnotation,
    RemoveTypeAnnotation,
    AddDocumentation,
    RemoveDocumentation,
    FormatCode,
    SortImports,
    OrganizeImports,
    Custom
};

struct RefactoringOperation {
    RefactoringKind kind;
    std::string name;
    std::string description;
    std::vector<std::string> requiredParams;
    std::vector<std::string> optionalParams;
    std::function<bool(const std::unordered_map<std::string, std::string>&)> validator;
};

// ============================================================================
// Preview Types
// ============================================================================

struct PreviewOptions {
    bool showLineNumbers = true;
    bool showContext = true;
    uint32_t contextLines = 3;
    bool colorize = true;
    bool showStatistics = true;
    bool groupByFile = true;
};

struct PreviewResult {
    std::string formatted;
    std::vector<DiffHunk> hunks;
    std::unordered_map<std::string, uint32_t> statistics;
    std::string summary;
};

// ============================================================================
// Rollback Types
// ============================================================================

struct Checkpoint {
    std::string id;
    std::chrono::system_clock::time_point timestamp;
    std::vector<FileChange> changes;
    std::string description;
    bool canRollback = true;
};

struct RollbackResult {
    bool success;
    std::vector<std::string> rolledBackFiles;
    std::vector<std::string> failedFiles;
    std::string errorMessage;
};

// ============================================================================
// Conflict Types
// ============================================================================

struct Conflict {
    std::string uri;
    Range range;
    std::string ourContent;
    std::string theirContent;
    std::string baseContent;
    std::string description;
};

struct ConflictResolution {
    std::string conflictId;
    enum class Strategy { Ours, Theirs, Base, Manual } strategy;
    std::string resolvedContent;
};

// ============================================================================
// Interactive Refactoring Engine Interface
// ============================================================================

class IInteractiveRefactoringEngine {
public:
    virtual ~IInteractiveRefactoringEngine() = default;
    
    // Session management
    virtual std::string createSession(const std::string& description) = 0;
    virtual bool endSession(const std::string& sessionId) = 0;
    virtual std::optional<RefactoringSession> getSession(const std::string& sessionId) const = 0;
    virtual std::vector<RefactoringSession> getActiveSessions() const = 0;
    
    // Conversation
    virtual void addMessage(const std::string& sessionId, 
                           const ConversationMessage& message) = 0;
    virtual std::vector<ConversationMessage> getConversation(
        const std::string& sessionId) const = 0;
    virtual ConversationContext getContext(const std::string& sessionId) const = 0;
    
    // File operations
    virtual bool addFile(const std::string& sessionId, const std::string& uri) = 0;
    virtual bool removeFile(const std::string& sessionId, const std::string& uri) = 0;
    virtual std::vector<std::string> getActiveFiles(const std::string& sessionId) const = 0;
    
    // Edit operations
    virtual bool proposeEdit(const std::string& sessionId, const TextEdit& edit) = 0;
    virtual bool applyEdit(const std::string& sessionId, const std::string& editId) = 0;
    virtual bool rejectEdit(const std::string& sessionId, const std::string& editId) = 0;
    virtual std::vector<TextEdit> getPendingEdits(const std::string& sessionId) const = 0;
    
    // Batch operations
    virtual bool applyAllEdits(const std::string& sessionId) = 0;
    virtual bool rejectAllEdits(const std::string& sessionId) = 0;
    virtual bool applyEdits(const std::string& sessionId, 
                          const std::vector<std::string>& editIds) = 0;
    
    // Diff operations
    virtual UnifiedDiff generateDiff(const std::string& sessionId) const = 0;
    virtual UnifiedDiff generateDiff(const std::string& sessionId,
                                    const std::string& uri) const = 0;
    virtual PreviewResult previewChanges(const std::string& sessionId,
                                        const PreviewOptions& options) const = 0;
    
    // Checkpoint and rollback
    virtual std::string createCheckpoint(const std::string& sessionId,
                                        const std::string& description) = 0;
    virtual RollbackResult rollback(const std::string& sessionId,
                                   const std::string& checkpointId) = 0;
    virtual std::vector<Checkpoint> getCheckpoints(const std::string& sessionId) const = 0;
    
    // Conflict detection
    virtual std::vector<Conflict> detectConflicts(const std::string& sessionId) const = 0;
    virtual bool resolveConflict(const std::string& sessionId,
                                const ConflictResolution& resolution) = 0;
    
    // Refactoring operations
    virtual bool executeRefactoring(const std::string& sessionId,
                                   RefactoringKind kind,
                                   const std::unordered_map<std::string, std::string>& params) = 0;
    virtual std::vector<RefactoringOperation> getAvailableRefactorings() const = 0;
    
    // Statistics
    virtual std::unordered_map<std::string, uint32_t> getStatistics(
        const std::string& sessionId) const = 0;
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<IInteractiveRefactoringEngine> createInteractiveRefactoringEngine();

} // namespace Refactoring
} // namespace RawrXD
