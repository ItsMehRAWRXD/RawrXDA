#include "multi_file_edit_plan.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <unordered_set>

namespace RawrXD::Agentic {

namespace {

std::vector<std::string> splitLinesPreserveEmpty(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    if (!text.empty() && text.back() == '\n') {
        lines.push_back("");
    }
    return lines;
}

} // namespace

// ============================================================================
// FileEdit Implementation
// ============================================================================

json FileEdit::toJson() const {
    json obj = json::object();
    obj["file"] = filePath;
    obj["type"] = (type == EditType::INSERT) ? "insert" :
                  (type == EditType::DELETE_RANGE) ? "delete" :
                  (type == EditType::REPLACE) ? "replace" : "modify";
    obj["lineStart"] = lineStart;
    obj["lineEnd"] = lineEnd;
    obj["reason"] = reason;
    if (type == EditType::INSERT || type == EditType::REPLACE || type == EditType::MODIFY) {
        obj["content"] = newContent;
    }
    obj["executed"] = executed;
    if (!error.empty()) {
        obj["error"] = error;
    }
    return obj;
}

// ============================================================================
// MultiFileEditPlan Implementation
// ============================================================================

MultiFileEditPlan::MultiFileEditPlan() : status(PlanStatus::PENDING) {
}

void MultiFileEditPlan::addEdit(const FileEdit& edit) {
    edits.push_back(edit);
    status = PlanStatus::PENDING;  // Reset status when new edit added
}

bool MultiFileEditPlan::editsConflict(const FileEdit& a, const FileEdit& b) {
    if (a.filePath != b.filePath) {
        return false;  // Different files can't have line-based conflicts
    }
    
    // Check if ranges overlap
    // Edit a: [aStart, aEnd], Edit b: [bStart, bEnd]
    bool overlap = !(a.lineEnd < b.lineStart || b.lineEnd < a.lineStart);
    return overlap;
}

void MultiFileEditPlan::detectConflicts() {
    conflicts.clear();
    
    for (size_t i = 0; i < edits.size(); i++) {
        for (size_t j = i + 1; j < edits.size(); j++) {
            if (editsConflict(edits[i], edits[j])) {
                ConflictInfo conflict;
                conflict.filePath = edits[i].filePath;
                conflict.conflictingEdits = {(int)i, (int)j};
                conflict.description = "Line ranges overlap in " + edits[i].filePath;
                conflicts.push_back(conflict);
            }
        }
    }
}

void MultiFileEditPlan::determineExecutionOrder() {
    // Deterministic ordering: file path asc, then line start desc (safe for in-place line edits).
    std::sort(edits.begin(), edits.end(),
        [](const FileEdit& a, const FileEdit& b) {
            if (a.filePath != b.filePath) {
                return a.filePath < b.filePath;
            }
            if (a.lineStart != b.lineStart) {
                return a.lineStart > b.lineStart;
            }
            return a.lineEnd > b.lineEnd;
        });
}

std::string MultiFileEditPlan::sequence() {
    status = PlanStatus::SEQUENCING;
    
    detectConflicts();
    if (!conflicts.empty()) {
        status = PlanStatus::FAILED;
        return "Conflicts detected: " + 
               std::to_string(conflicts.size()) + " conflict(s)";
    }
    
    determineExecutionOrder();
    
    status = PlanStatus::READY;
    return "";  // Success
}

std::vector<ConflictInfo> MultiFileEditPlan::checkConflicts() const {
    return conflicts;
}

json MultiFileEditPlan::toPreviewJson() const {
    json preview = json::object();
    preview["status"] = (status == PlanStatus::PENDING) ? "pending" :
                        (status == PlanStatus::READY) ? "ready" :
                        (status == PlanStatus::IN_PROGRESS) ? "executing" :
                        (status == PlanStatus::COMPLETED) ? "completed" :
                        (status == PlanStatus::FAILED) ? "failed" : "unknown";
    preview["totalEdits"] = edits.size();
    
    json editsJson = json::array();
    for (const auto& edit : edits) {
        editsJson.push_back(edit.toJson());
    }
    preview["edits"] = editsJson;
    
    json conflictsJson = json::array();
    for (const auto& conf : conflicts) {
        json cj = json::object();
        cj["file"] = conf.filePath;
        cj["description"] = conf.description;
        conflictsJson.push_back(cj);
    }
    preview["conflicts"] = conflictsJson;
    
    return preview;
}

std::string MultiFileEditPlan::readFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (...) {
        return "";
    }
}

bool MultiFileEditPlan::writeFile(const std::string& path, const std::string& content) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << content;
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool MultiFileEditPlan::applyEdit(const FileEdit& edit, std::string& outError) {
    try {
        std::string content = readFile(edit.filePath);
        if (content.empty() && edit.type != EditType::INSERT) {
            outError = "File not found or empty: " + edit.filePath;
            return false;
        }
        
        // Split content into lines
        std::vector<std::string> lines = splitLinesPreserveEmpty(content);
        std::vector<std::string> replacement = splitLinesPreserveEmpty(edit.newContent);

        const int lineCount = static_cast<int>(lines.size());
        int start = edit.lineStart;
        int end = edit.lineEnd;

        // Planner line numbers are 1-based. Convert to 0-based for vector operations.
        if (start > 0) --start;
        if (end > 0) --end;
        
        // Apply edit based on type
        switch (edit.type) {
            case EditType::INSERT: {
                if (start < 0 || start > lineCount) {
                    outError = "Insert line out of range";
                    return false;
                }
                lines.insert(lines.begin() + start, replacement.begin(), replacement.end());
                break;
            }
            
            case EditType::DELETE_RANGE: {
                if (start < 0 || end < 0 || start > end || end >= lineCount) {
                    outError = "Delete range out of range";
                    return false;
                }
                lines.erase(lines.begin() + start, lines.begin() + end + 1);
                break;
            }
            
            case EditType::REPLACE: {
                if (start < 0 || end < 0 || start > end || end >= lineCount) {
                    outError = "Replace range out of range";
                    return false;
                }
                lines.erase(lines.begin() + start, lines.begin() + end + 1);
                lines.insert(lines.begin() + start, replacement.begin(), replacement.end());
                break;
            }
            
            case EditType::MODIFY: {
                if (start < 0 || end < 0 || start > end || end >= lineCount) {
                    outError = "Modify range out of range";
                    return false;
                }
                const std::string joined = edit.newContent;
                for (int i = start; i <= end; i++) {
                    lines[i] = joined;
                }
                break;
            }
        }
        
        // Reconstruct content
        std::ostringstream result;
        for (size_t i = 0; i < lines.size(); i++) {
            result << lines[i];
            if (i < lines.size() - 1) result << "\n";
        }
        
        // Write back to file
        if (!writeFile(edit.filePath, result.str())) {
            outError = "Failed to write file: " + edit.filePath;
            return false;
        }
        
        return true;
        
    } catch (...) {
        outError = "Exception during edit";
        return false;
    }
}

void MultiFileEditPlan::createCheckpoint(int editIndex) {
    ExecutionCheckpoint cp;
    cp.editIndex = editIndex;
    cp.filePath = edits[editIndex].filePath;
    cp.stateSnapshot = readFile(cp.filePath);
    cp.success = false;
    checkpoints.push_back(cp);
}

int MultiFileEditPlan::execute() {
    if (status == PlanStatus::PENDING) {
        const std::string seqError = sequence();
        if (!seqError.empty()) {
            status = PlanStatus::FAILED;
            return 0;
        }
    }
    if (status != PlanStatus::READY) {
        status = PlanStatus::FAILED;
        return 0;
    }
    
    status = PlanStatus::IN_PROGRESS;
    int successCount = 0;
    
    std::unordered_set<std::string> checkpointedFiles;

    for (size_t i = 0; i < edits.size(); i++) {
        if (!checkpointedFiles.count(edits[i].filePath)) {
            createCheckpoint(static_cast<int>(i));
            checkpointedFiles.insert(edits[i].filePath);
        }
        std::string error;
        if (applyEdit(edits[i], error)) {
            edits[i].executed = true;
            if (!checkpoints.empty()) {
                checkpoints.back().success = true;
            }
            successCount++;
        } else {
            edits[i].error = error;
            if (!checkpoints.empty()) {
                checkpoints.back().success = false;
            }

            // Transactional behavior: fail-fast and rollback all file snapshots.
            rollbackAll();
            status = PlanStatus::ROLLED_BACK;
            return successCount;
        }
    }
    
    if (successCount == (int)edits.size()) {
        status = PlanStatus::COMPLETED;
    } else {
        status = PlanStatus::FAILED;
    }
    
    return successCount;
}

bool MultiFileEditPlan::rollbackTo(int checkpointIndex) {
    if (checkpointIndex < 0 || checkpointIndex >= (int)checkpoints.size()) {
        return false;
    }
    
    int successCount = 0;
    for (int i = (int)checkpoints.size() - 1; i >= checkpointIndex; i--) {
        const auto& cp = checkpoints[i];
        if (!writeFile(cp.filePath, cp.stateSnapshot)) {
            return false;  // Rollback failed
        }
        successCount++;
    }
    
    status = PlanStatus::ROLLED_BACK;
    return true;
}

bool MultiFileEditPlan::rollbackAll() {
    if (checkpoints.empty()) {
        return true;  // Nothing to rollback
    }
    
    return rollbackTo(checkpoints.size() - 1);
}

MultiFileEditPlan::Summary MultiFileEditPlan::getSummary() const {
    Summary s;
    s.totalEdits = edits.size();
    s.successfulEdits = 0;
    s.failedEdits = 0;
    s.fullyCompleted = (status == PlanStatus::COMPLETED);
    
    for (const auto& edit : edits) {
        if (edit.executed && edit.error.empty()) {
            s.successfulEdits++;
        } else if (!edit.error.empty()) {
            s.failedEdits++;
            s.errors.push_back(edit.error);
        }
    }
    
    return s;
}

// ============================================================================
// MultiFileEditPlanBuilder Implementation
// ============================================================================

MultiFileEditPlanBuilder& MultiFileEditPlanBuilder::withEditFile(
    const std::string& filePath) {
    if (!plan) {
        plan = std::make_shared<MultiFileEditPlan>();
    }
    currentFile = filePath;
    return *this;
}

MultiFileEditPlanBuilder& MultiFileEditPlanBuilder::insertAtLine(
    int lineNumber, const std::string& content, const std::string& reason) {
    if (!plan) plan = std::make_shared<MultiFileEditPlan>();
    FileEdit edit;
    edit.filePath = currentFile;
    edit.lineStart = lineNumber;
    edit.lineEnd = lineNumber;
    edit.type = EditType::INSERT;
    edit.newContent = content;
    edit.reason = reason;
    plan->addEdit(edit);
    return *this;
}

MultiFileEditPlanBuilder& MultiFileEditPlanBuilder::deleteLinesRange(
    int lineStart, int lineEnd, const std::string& reason) {
    if (!plan) plan = std::make_shared<MultiFileEditPlan>();
    FileEdit edit;
    edit.filePath = currentFile;
    edit.lineStart = lineStart;
    edit.lineEnd = lineEnd;
    edit.type = EditType::DELETE_RANGE;
    edit.reason = reason;
    plan->addEdit(edit);
    return *this;
}

MultiFileEditPlanBuilder& MultiFileEditPlanBuilder::replaceLines(
    int lineStart, int lineEnd, const std::string& newContent,
    const std::string& reason) {
    if (!plan) plan = std::make_shared<MultiFileEditPlan>();
    FileEdit edit;
    edit.filePath = currentFile;
    edit.lineStart = lineStart;
    edit.lineEnd = lineEnd;
    edit.type = EditType::REPLACE;
    edit.newContent = newContent;
    edit.reason = reason;
    plan->addEdit(edit);
    return *this;
}

MultiFileEditPlanBuilder& MultiFileEditPlanBuilder::modifyFile(
    int lineStart, int lineEnd, const std::string& newContent,
    const std::string& reason) {
    if (!plan) plan = std::make_shared<MultiFileEditPlan>();
    FileEdit edit;
    edit.filePath = currentFile;
    edit.lineStart = lineStart;
    edit.lineEnd = lineEnd;
    edit.type = EditType::MODIFY;
    edit.newContent = newContent;
    edit.reason = reason;
    plan->addEdit(edit);
    return *this;
}

std::shared_ptr<MultiFileEditPlan> MultiFileEditPlanBuilder::build() {
    if (!plan) {
        plan = std::make_shared<MultiFileEditPlan>();
    }
    return plan;
}

} // namespace RawrXD::Agentic
