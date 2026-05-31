#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>

/**
 * Multi-File Edit Planning System
 * 
 * Sequences edits across multiple files with:
 * - Conflict detection (overlapping ranges in same file)
 * - Dependency ordering (A before B if A affects B)
 * - Checkpoint system for partial failure recovery
 * - Rollback mechanism
 * 
 * NO EXTERNAL DEPENDENCIES beyond nlohmann/json (already linked)
 */

namespace RawrXD::Agentic {

using json = nlohmann::json;

enum class EditType {
    INSERT,    // Insert at line
    DELETE_RANGE, // Delete range
    REPLACE,   // Replace range with new content
    MODIFY,    // Modify existing content
};

enum class PlanStatus {
    PENDING,        // Not started
    SEQUENCING,     // Determining order
    CONFLICT_CHECK, // Checking for conflicts
    READY,          // Ready to execute
    IN_PROGRESS,    // Executing
    COMPLETED,      // All edits completed
    FAILED,         // Some edits failed
    ROLLED_BACK,    // Rolled back
};

struct FileEdit {
    std::string filePath;
    int lineStart;
    int lineEnd;  // Inclusive
    EditType type;
    std::string newContent;
    std::string reason;  // Why this edit is needed
    bool executed = false;
    std::string error;   // If execution failed
    
    // Snapshot for rollback
    std::string originalContent;
    
    // JSON serialization for plan preview
    json toJson() const;
};

struct ConflictInfo {
    std::string filePath;
    std::vector<int> conflictingEdits; // indices into edits list
    std::string description;
};

struct ExecutionCheckpoint {
    int editIndex;
    std::string filePath;
    std::string stateSnapshot;  // For rollback
    bool success;
};

class MultiFileEditPlan {
public:
    /**
     * Create a new edit plan
     */
    MultiFileEditPlan();
    
    /**
     * Add an edit to the plan
     */
    void addEdit(const FileEdit& edit);
    
    /**
     * Sequence edits based on dependencies and conflicts
     * Returns: empty string on success, error message on failure
     */
    std::string sequence();
    
    /**
     * Check for conflicts in the plan
     * Returns: list of conflicts found
     */
    std::vector<ConflictInfo> checkConflicts() const;
    
    /**
     * Check if plan is ready to execute
     */
    bool isReady() const { return status == PlanStatus::READY; }
    
    /**
     * Get plan as JSON for preview/approval
     */
    json toPreviewJson() const;
    
    /**
     * Execute the plan (applies all edits sequentially)
     * Can be stopped at any point for incremental execution
     * Returns: number of edits successfully executed
     */
    int execute();
    
    /**
     * Get current status
     */
    PlanStatus getStatus() const { return status; }
    
    /**
     * Rollback to a specific checkpoint
     */
    bool rollbackTo(int checkpointIndex);
    
    /**
     * Rollback all edits (undo everything)
     */
    bool rollbackAll();
    
    /**
     * Get list of checkpoints
     */
    const std::vector<ExecutionCheckpoint>& getCheckpoints() const {
        return checkpoints;
    }
    
    /**
     * Get all edits
     */
    const std::vector<FileEdit>& getEdits() const {
        return edits;
    }
    
    /**
     * Get all conflicts
     */
    const std::vector<ConflictInfo>& getConflicts() const {
        return conflicts;
    }
    
    /**
     * Get execution summary
     */
    struct Summary {
        int totalEdits;
        int successfulEdits;
        int failedEdits;
        std::vector<std::string> errors;
        bool fullyCompleted;
    };
    Summary getSummary() const;

private:
    std::vector<FileEdit> edits;
    std::vector<ConflictInfo> conflicts;
    std::vector<ExecutionCheckpoint> checkpoints;
    PlanStatus status;
    
    // Helper: Detect conflicts between edits in same file
    void detectConflicts();
    
    // Helper: Determine execution order (topological sort)
    void determineExecutionOrder();
    
    // Helper: Check if two edits overlap
    static bool editsConflict(const FileEdit& a, const FileEdit& b);
    
    // Helper: Create checkpoint before edit
    void createCheckpoint(int editIndex);
    
    // Helper: Apply single edit to filesystem
    static bool applyEdit(const FileEdit& edit, std::string& outError);
    
    // Helper: Read file from disk
    static std::string readFile(const std::string& path);
    
    // Helper: Write file to disk
    static bool writeFile(const std::string& path, const std::string& content);
};

/**
 * Plan Builder - Fluent interface for creating plans
 */
class MultiFileEditPlanBuilder {
public:
    MultiFileEditPlanBuilder& withEditFile(const std::string& filePath);
    
    MultiFileEditPlanBuilder& insertAtLine(int lineNumber, const std::string& content, 
                                          const std::string& reason = "");
    
    MultiFileEditPlanBuilder& deleteLinesRange(int lineStart, int lineEnd,
                                              const std::string& reason = "");
    
    MultiFileEditPlanBuilder& replaceLines(int lineStart, int lineEnd, 
                                          const std::string& newContent,
                                          const std::string& reason = "");
    
    MultiFileEditPlanBuilder& modifyFile(int lineStart, int lineEnd,
                                        const std::string& newContent,
                                        const std::string& reason = "");
    
    std::shared_ptr<MultiFileEditPlan> build();

private:
    std::shared_ptr<MultiFileEditPlan> plan;
    std::string currentFile;
};

} // namespace RawrXD::Agentic
