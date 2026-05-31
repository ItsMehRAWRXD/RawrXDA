// ============================================================================
// interactive_refactoring.cpp — Interactive Refactoring Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "refactoring/interactive_refactoring.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <random>
#include <regex>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Refactoring {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

std::string generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 32; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string readFileContent(const std::string& uri) {
    std::ifstream file(uri);
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool writeFileContent(const std::string& uri, const std::string& content) {
    std::ofstream file(uri);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return true;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream result;
    for (size_t i = 0; i < lines.size(); i++) {
        result << lines[i];
        if (i < lines.size() - 1) {
            result << "\n";
        }
    }
    return result.str();
}

std::string applyEditsToContent(const std::string& content, 
                                const std::vector<TextEdit>& edits) {
    auto lines = splitLines(content);
    
    // Sort edits by position (reverse order to apply from bottom to top)
    std::vector<TextEdit> sortedEdits = edits;
    std::sort(sortedEdits.begin(), sortedEdits.end(), 
              [](const TextEdit& a, const TextEdit& b) {
                  return a.range.start > b.range.start;
              });
    
    for (const auto& edit : sortedEdits) {
        uint32_t startLine = edit.range.start.line;
        uint32_t endLine = edit.range.end.line;
        
        if (startLine >= lines.size()) {
            continue;
        }
        
        // Remove old lines
        auto newLines = splitLines(edit.newText);
        
        if (endLine >= lines.size()) {
            endLine = static_cast<uint32_t>(lines.size()) - 1;
        }
        
        // Replace lines
        lines.erase(lines.begin() + startLine, lines.begin() + endLine + 1);
        lines.insert(lines.begin() + startLine, newLines.begin(), newLines.end());
    }
    
    return joinLines(lines);
}

} // anonymous namespace

// ============================================================================
// UnifiedDiff Implementation
// ============================================================================

std::string UnifiedDiff::toString() const {
    std::ostringstream result;
    
    result << "--- " << oldFile << "\n";
    result << "+++ " << newFile << "\n";
    
    for (const auto& hunk : hunks) {
        result << "@@ -" << hunk.oldStart << "," << hunk.oldCount
              << " +" << hunk.newStart << "," << hunk.newCount << " @@\n";
        
        for (const auto& line : hunk.lines) {
            switch (line.type) {
                case DiffType::Addition:
                    result << "+" << line.content << "\n";
                    break;
                case DiffType::Deletion:
                    result << "-" << line.content << "\n";
                    break;
                case DiffType::Modification:
                    result << "~" << line.content << "\n";
                    break;
                case DiffType::Context:
                    result << " " << line.content << "\n";
                    break;
            }
        }
    }
    
    return result.str();
}

std::optional<UnifiedDiff> UnifiedDiff::fromString(const std::string& diff) {
    // Parse unified diff format
    UnifiedDiff result;
    std::istringstream stream(diff);
    std::string line;
    
    enum class ParseState { Header, Hunk };
    ParseState state = ParseState::Header;
    
    DiffHunk currentHunk;
    
    while (std::getline(stream, line)) {
        if (line.substr(0, 4) == "--- ") {
            result.oldFile = line.substr(4);
        } else if (line.substr(0, 4) == "+++ ") {
            result.newFile = line.substr(4);
        } else if (line.substr(0, 3) == "@@ ") {
            if (!currentHunk.lines.empty()) {
                result.hunks.push_back(currentHunk);
                currentHunk = DiffHunk();
            }
            
            // Parse hunk header
            std::regex hunkRegex(R"(@@ -(\d+),(\d+) \+(\d+),(\d+) @@)");
            std::smatch match;
            if (std::regex_search(line, match, hunkRegex)) {
                currentHunk.oldStart = std::stoul(match[1].str());
                currentHunk.oldCount = std::stoul(match[2].str());
                currentHunk.newStart = std::stoul(match[3].str());
                currentHunk.newCount = std::stoul(match[4].str());
            }
            
            state = ParseState::Hunk;
        } else if (state == ParseState::Hunk) {
            DiffLine diffLine;
            diffLine.content = line.substr(1);
            
            if (line[0] == '+') {
                diffLine.type = DiffType::Addition;
            } else if (line[0] == '-') {
                diffLine.type = DiffType::Deletion;
            } else if (line[0] == '~') {
                diffLine.type = DiffType::Modification;
            } else {
                diffLine.type = DiffType::Context;
            }
            
            currentHunk.lines.push_back(diffLine);
        }
    }
    
    if (!currentHunk.lines.empty()) {
        result.hunks.push_back(currentHunk);
    }
    
    result.timestamp = std::chrono::system_clock::now();
    return result;
}

// ============================================================================
// Interactive Refactoring Engine Implementation
// ============================================================================

class InteractiveRefactoringEngine : public IInteractiveRefactoringEngine {
private:
    std::unordered_map<std::string, RefactoringSession> sessions_;
    std::unordered_map<std::string, std::vector<ConversationMessage>> conversations_;
    std::unordered_map<std::string, std::vector<Checkpoint>> checkpoints_;
    std::vector<RefactoringOperation> refactoringOps_;
    
    void initializeRefactoringOperations() {
        refactoringOps_ = {
            {RefactoringKind::Rename, "Rename", 
             "Rename a symbol across all references",
             {"oldName", "newName"}, {"scope"}},
            
            {RefactoringKind::ExtractFunction, "Extract Function",
             "Extract selected code into a new function",
             {"selection", "functionName"}, {"parameters"}},
            
            {RefactoringKind::ExtractVariable, "Extract Variable",
             "Extract expression into a new variable",
             {"expression", "variableName"}, {"scope"}},
            
            {RefactoringKind::ExtractMethod, "Extract Method",
             "Extract selected code into a new method",
             {"selection", "methodName"}, {"parameters", "visibility"}},
            
            {RefactoringKind::InlineFunction, "Inline Function",
             "Replace function call with function body",
             {"functionName"}, {"keepOriginal"}},
            
            {RefactoringKind::InlineVariable, "Inline Variable",
             "Replace variable reference with its value",
             {"variableName"}, {"scope"}},
            
            {RefactoringKind::MoveFunction, "Move Function",
             "Move function to a different file or class",
             {"functionName", "destination"}, {"updateImports"}},
            
            {RefactoringKind::MoveClass, "Move Class",
             "Move class to a different file",
             {"className", "destination"}, {"updateImports"}},
            
            {RefactoringKind::ChangeSignature, "Change Signature",
             "Modify function signature",
             {"functionName"}, {"addParams", "removeParams", "reorderParams"}},
            
            {RefactoringKind::IntroduceParameter, "Introduce Parameter",
             "Convert local variable to function parameter",
             {"variableName", "functionName"}, {"defaultValue"}},
            
            {RefactoringKind::IntroduceField, "Introduce Field",
             "Convert local variable to class field",
             {"variableName", "className"}, {"visibility"}},
            
            {RefactoringKind::EncapsulateField, "Encapsulate Field",
             "Create getter/setter for field",
             {"fieldName", "className"}, {"visibility"}},
            
            {RefactoringKind::ReplaceMagicNumber, "Replace Magic Number",
             "Replace magic number with named constant",
             {"value", "constantName"}, {"scope"}},
            
            {RefactoringKind::SplitVariable, "Split Variable",
             "Split variable into multiple variables",
             {"variableName"}, {"newNames"}},
            
            {RefactoringKind::MergeVariables, "Merge Variables",
             "Merge multiple variables into one",
             {"variableNames"}, {"newName"}},
            
            {RefactoringKind::SplitFunction, "Split Function",
             "Split function into multiple functions",
             {"functionName"}, {"splitPoints"}},
            
            {RefactoringKind::MergeFunctions, "Merge Functions",
             "Merge multiple functions into one",
             {"functionNames"}, {"newName"}},
            
            {RefactoringKind::AddParameter, "Add Parameter",
             "Add parameter to function",
             {"functionName", "paramName"}, {"defaultValue"}},
            
            {RefactoringKind::RemoveParameter, "Remove Parameter",
             "Remove parameter from function",
             {"functionName", "paramName"}, {}},
            
            {RefactoringKind::ReorderParameters, "Reorder Parameters",
             "Change order of function parameters",
             {"functionName", "newOrder"}, {}},
            
            {RefactoringKind::ConvertToArrowFunction, "Convert to Arrow Function",
             "Convert regular function to arrow function",
             {"functionName"}, {}},
            
            {RefactoringKind::ConvertToRegularFunction, "Convert to Regular Function",
             "Convert arrow function to regular function",
             {"functionName"}, {}},
            
            {RefactoringKind::AddTypeAnnotation, "Add Type Annotation",
             "Add type annotation to variable/parameter",
             {"target", "type"}, {}},
            
            {RefactoringKind::RemoveTypeAnnotation, "Remove Type Annotation",
             "Remove type annotation from variable/parameter",
             {"target"}, {}},
            
            {RefactoringKind::AddDocumentation, "Add Documentation",
             "Add documentation comment",
             {"target"}, {"format"}},
            
            {RefactoringKind::RemoveDocumentation, "Remove Documentation",
             "Remove documentation comment",
             {"target"}, {}},
            
            {RefactoringKind::FormatCode, "Format Code",
             "Format code according to style rules",
             {}, {"style", "range"}},
            
            {RefactoringKind::SortImports, "Sort Imports",
             "Sort import statements",
             {}, {"order"}},
            
            {RefactoringKind::OrganizeImports, "Organize Imports",
             "Remove unused imports and sort",
             {}, {}},
            
            {RefactoringKind::Custom, "Custom Refactoring",
             "Execute custom refactoring",
             {"operation"}, {}}
        };
    }
    
public:
    InteractiveRefactoringEngine() {
        initializeRefactoringOperations();
    }
    
    // Session management
    std::string createSession(const std::string& description) override {
        std::string id = generateId();
        
        RefactoringSession session;
        session.id = id;
        session.description = description;
        session.startTime = std::chrono::system_clock::now();
        session.completed = false;
        session.rolledBack = false;
        
        sessions_[id] = session;
        conversations_[id] = std::vector<ConversationMessage>();
        checkpoints_[id] = std::vector<Checkpoint>();
        
        return id;
    }
    
    bool endSession(const std::string& sessionId) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        it->second.endTime = std::chrono::system_clock::now();
        it->second.completed = true;
        return true;
    }
    
    std::optional<RefactoringSession> getSession(const std::string& sessionId) const override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    
    std::vector<RefactoringSession> getActiveSessions() const override {
        std::vector<RefactoringSession> result;
        for (const auto& [id, session] : sessions_) {
            if (!session.completed) {
                result.push_back(session);
            }
        }
        return result;
    }
    
    // Conversation
    void addMessage(const std::string& sessionId, 
                   const ConversationMessage& message) override {
        auto it = conversations_.find(sessionId);
        if (it == conversations_.end()) {
            return;
        }
        
        ConversationMessage msg = message;
        msg.id = generateId();
        msg.timestamp = std::chrono::system_clock::now();
        it->second.push_back(msg);
    }
    
    std::vector<ConversationMessage> getConversation(
        const std::string& sessionId) const override {
        auto it = conversations_.find(sessionId);
        if (it == conversations_.end()) {
            return {};
        }
        return it->second;
    }
    
    ConversationContext getContext(const std::string& sessionId) const override {
        ConversationContext context;
        
        auto sessionIt = sessions_.find(sessionId);
        if (sessionIt == sessions_.end()) {
            return context;
        }
        
        context.activeFiles = sessionIt->second.contextFiles;
        
        // Extract referenced symbols from conversation
        auto convIt = conversations_.find(sessionId);
        if (convIt != conversations_.end()) {
            for (const auto& msg : convIt->second) {
                // Simple symbol extraction (could be enhanced)
                std::regex symbolRegex(R"(\b([A-Z][a-zA-Z0-9_]*)\b)");
                std::smatch match;
                std::string content = msg.content;
                while (std::regex_search(content, match, symbolRegex)) {
                    context.referencedSymbols.push_back(match[1].str());
                    content = match.suffix();
                }
            }
        }
        
        // Load file contents
        for (const auto& uri : context.activeFiles) {
            context.fileContents[uri] = readFileContent(uri);
        }
        
        return context;
    }
    
    // File operations
    bool addFile(const std::string& sessionId, const std::string& uri) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        // Check if file exists
        if (!fs::exists(uri)) {
            return false;
        }
        
        // Add to context files
        auto& files = it->second.contextFiles;
        if (std::find(files.begin(), files.end(), uri) == files.end()) {
            files.push_back(uri);
        }
        
        // Create initial file change
        FileChange change;
        change.uri = uri;
        change.originalContent = readFileContent(uri);
        change.modifiedContent = change.originalContent;
        change.timestamp = std::chrono::system_clock::now();
        
        it->second.changes.push_back(change);
        return true;
    }
    
    bool removeFile(const std::string& sessionId, const std::string& uri) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        auto& files = it->second.contextFiles;
        auto fileIt = std::find(files.begin(), files.end(), uri);
        if (fileIt != files.end()) {
            files.erase(fileIt);
        }
        
        return true;
    }
    
    std::vector<std::string> getActiveFiles(const std::string& sessionId) const override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return {};
        }
        return it->second.contextFiles;
    }
    
    // Edit operations
    bool proposeEdit(const std::string& sessionId, const TextEdit& edit) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        // Find file change for this URI
        FileChange* fileChange = nullptr;
        for (auto& change : it->second.changes) {
            if (change.uri == edit.source) {
                fileChange = &change;
                break;
            }
        }
        
        if (!fileChange) {
            return false;
        }
        
        TextEdit newEdit = edit;
        newEdit.id = generateId();
        newEdit.timestamp = std::chrono::system_clock::now();
        newEdit.applied = false;
        newEdit.confirmed = false;
        
        fileChange->edits.push_back(newEdit);
        return true;
    }
    
    bool applyEdit(const std::string& sessionId, const std::string& editId) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        for (auto& change : it->second.changes) {
            for (auto& edit : change.edits) {
                if (edit.id == editId) {
                    edit.applied = true;
                    edit.confirmed = true;
                    
                    // Apply to modified content
                    change.modifiedContent = applyEditsToContent(
                        change.modifiedContent, {edit});
                    
                    // Update statistics
                    auto lines = splitLines(edit.newText);
                    change.linesAdded += static_cast<uint32_t>(lines.size());
                    
                    lines = splitLines(edit.oldText);
                    change.linesRemoved += static_cast<uint32_t>(lines.size());
                    
                    return true;
                }
            }
        }
        
        return false;
    }
    
    bool rejectEdit(const std::string& sessionId, const std::string& editId) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        for (auto& change : it->second.changes) {
            auto editIt = std::find_if(change.edits.begin(), change.edits.end(),
                                       [&editId](const TextEdit& e) { return e.id == editId; });
            if (editIt != change.edits.end()) {
                change.edits.erase(editIt);
                return true;
            }
        }
        
        return false;
    }
    
    std::vector<TextEdit> getPendingEdits(const std::string& sessionId) const override {
        std::vector<TextEdit> result;
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return result;
        }
        
        for (const auto& change : it->second.changes) {
            for (const auto& edit : change.edits) {
                if (!edit.applied) {
                    result.push_back(edit);
                }
            }
        }
        
        return result;
    }
    
    // Batch operations
    bool applyAllEdits(const std::string& sessionId) override {
        auto pending = getPendingEdits(sessionId);
        for (const auto& edit : pending) {
            if (!applyEdit(sessionId, edit.id)) {
                return false;
            }
        }
        return true;
    }
    
    bool rejectAllEdits(const std::string& sessionId) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        for (auto& change : it->second.changes) {
            change.edits.clear();
        }
        
        return true;
    }
    
    bool applyEdits(const std::string& sessionId, 
                   const std::vector<std::string>& editIds) override {
        for (const auto& editId : editIds) {
            if (!applyEdit(sessionId, editId)) {
                return false;
            }
        }
        return true;
    }
    
    // Diff operations
    UnifiedDiff generateDiff(const std::string& sessionId) const override {
        UnifiedDiff diff;
        diff.timestamp = std::chrono::system_clock::now();
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return diff;
        }
        
        for (const auto& change : it->second.changes) {
            auto fileDiff = generateDiff(sessionId, change.uri);
            for (const auto& hunk : fileDiff.hunks) {
                diff.hunks.push_back(hunk);
            }
        }
        
        return diff;
    }
    
    UnifiedDiff generateDiff(const std::string& sessionId, 
                            const std::string& uri) const override {
        UnifiedDiff diff;
        diff.oldFile = uri;
        diff.newFile = uri;
        diff.timestamp = std::chrono::system_clock::now();
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return diff;
        }
        
        // Find file change
        const FileChange* fileChange = nullptr;
        for (const auto& change : it->second.changes) {
            if (change.uri == uri) {
                fileChange = &change;
                break;
            }
        }
        
        if (!fileChange) {
            return diff;
        }
        
        // Generate diff hunks
        auto oldLines = splitLines(fileChange->originalContent);
        auto newLines = splitLines(fileChange->modifiedContent);
        
        // Simple line-by-line diff (could be enhanced with LCS algorithm)
        size_t maxLines = std::max(oldLines.size(), newLines.size());
        uint32_t oldLine = 1;
        uint32_t newLine = 1;
        
        DiffHunk currentHunk;
        currentHunk.oldStart = 1;
        currentHunk.newStart = 1;
        
        for (size_t i = 0; i < maxLines; i++) {
            DiffLine line;
            
            if (i < oldLines.size() && i < newLines.size()) {
                if (oldLines[i] == newLines[i]) {
                    line.type = DiffType::Context;
                    line.content = oldLines[i];
                    line.oldLine = oldLine++;
                    line.newLine = newLine++;
                } else {
                    // Modification
                    DiffLine oldLineDiff;
                    oldLineDiff.type = DiffType::Deletion;
                    oldLineDiff.content = oldLines[i];
                    oldLineDiff.oldLine = oldLine++;
                    currentHunk.lines.push_back(oldLineDiff);
                    
                    line.type = DiffType::Addition;
                    line.content = newLines[i];
                    line.newLine = newLine++;
                }
            } else if (i < oldLines.size()) {
                line.type = DiffType::Deletion;
                line.content = oldLines[i];
                line.oldLine = oldLine++;
            } else {
                line.type = DiffType::Addition;
                line.content = newLines[i];
                line.newLine = newLine++;
            }
            
            currentHunk.lines.push_back(line);
        }
        
        currentHunk.oldCount = static_cast<uint32_t>(oldLines.size());
        currentHunk.newCount = static_cast<uint32_t>(newLines.size());
        
        if (!currentHunk.lines.empty()) {
            diff.hunks.push_back(currentHunk);
        }
        
        return diff;
    }
    
    PreviewResult previewChanges(const std::string& sessionId,
                                const PreviewOptions& options) const override {
        PreviewResult result;
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return result;
        }
        
        std::ostringstream output;
        
        if (options.groupByFile) {
            for (const auto& change : it->second.changes) {
                output << "=== " << change.uri << " ===\n\n";
                
                auto diff = generateDiff(sessionId, change.uri);
                for (const auto& hunk : diff.hunks) {
                    output << "@@ -" << hunk.oldStart << "," << hunk.oldCount
                          << " +" << hunk.newStart << "," << hunk.newCount << " @@\n";
                    
                    for (const auto& line : hunk.lines) {
                        if (options.showLineNumbers) {
                            if (line.type == DiffType::Addition) {
                                output << std::setw(4) << line.newLine << " + ";
                            } else if (line.type == DiffType::Deletion) {
                                output << std::setw(4) << line.oldLine << " - ";
                            } else {
                                output << std::setw(4) << line.oldLine << "   ";
                            }
                        }
                        
                        switch (line.type) {
                            case DiffType::Addition:
                                output << "\033[32m" << line.content << "\033[0m\n";
                                break;
                            case DiffType::Deletion:
                                output << "\033[31m" << line.content << "\033[0m\n";
                                break;
                            default:
                                output << line.content << "\n";
                                break;
                        }
                    }
                    
                    output << "\n";
                }
            }
        }
        
        result.formatted = output.str();
        
        // Calculate statistics
        for (const auto& change : it->second.changes) {
            result.statistics["filesChanged"]++;
            result.statistics["linesAdded"] += change.linesAdded;
            result.statistics["linesRemoved"] += change.linesRemoved;
            result.statistics["linesModified"] += change.linesModified;
        }
        
        // Generate summary
        std::ostringstream summary;
        summary << "Changes: " << result.statistics["filesChanged"] << " files, "
                << "+" << result.statistics["linesAdded"] << " "
                << "-" << result.statistics["linesRemoved"] << " lines";
        result.summary = summary.str();
        
        return result;
    }
    
    // Checkpoint and rollback
    std::string createCheckpoint(const std::string& sessionId,
                                const std::string& description) override {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return "";
        }
        
        std::string id = generateId();
        
        Checkpoint checkpoint;
        checkpoint.id = id;
        checkpoint.timestamp = std::chrono::system_clock::now();
        checkpoint.changes = it->second.changes;
        checkpoint.description = description;
        checkpoint.canRollback = true;
        
        checkpoints_[sessionId].push_back(checkpoint);
        return id;
    }
    
    RollbackResult rollback(const std::string& sessionId,
                           const std::string& checkpointId) override {
        RollbackResult result;
        
        auto sessionIt = sessions_.find(sessionId);
        if (sessionIt == sessions_.end()) {
            result.success = false;
            result.errorMessage = "Session not found";
            return result;
        }
        
        auto checkpointIt = checkpoints_.find(sessionId);
        if (checkpointIt == checkpoints_.end()) {
            result.success = false;
            result.errorMessage = "No checkpoints found";
            return result;
        }
        
        // Find checkpoint
        Checkpoint* targetCheckpoint = nullptr;
        for (auto& cp : checkpointIt->second) {
            if (cp.id == checkpointId) {
                targetCheckpoint = &cp;
                break;
            }
        }
        
        if (!targetCheckpoint) {
            result.success = false;
            result.errorMessage = "Checkpoint not found";
            return result;
        }
        
        if (!targetCheckpoint->canRollback) {
            result.success = false;
            result.errorMessage = "Checkpoint cannot be rolled back";
            return result;
        }
        
        // Restore files to checkpoint state
        for (const auto& change : targetCheckpoint->changes) {
            if (writeFileContent(change.uri, change.originalContent)) {
                result.rolledBackFiles.push_back(change.uri);
            } else {
                result.failedFiles.push_back(change.uri);
            }
        }
        
        // Restore session state
        sessionIt->second.changes = targetCheckpoint->changes;
        sessionIt->second.rolledBack = true;
        
        result.success = result.failedFiles.empty();
        return result;
    }
    
    std::vector<Checkpoint> getCheckpoints(const std::string& sessionId) const override {
        auto it = checkpoints_.find(sessionId);
        if (it == checkpoints_.end()) {
            return {};
        }
        return it->second;
    }
    
    // Conflict detection
    std::vector<Conflict> detectConflicts(const std::string& sessionId) const override {
        std::vector<Conflict> conflicts;
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return conflicts;
        }
        
        // Check for overlapping edits in same file
        for (const auto& change : it->second.changes) {
            for (size_t i = 0; i < change.edits.size(); i++) {
                for (size_t j = i + 1; j < change.edits.size(); j++) {
                    if (change.edits[i].range.overlaps(change.edits[j].range)) {
                        Conflict conflict;
                        conflict.uri = change.uri;
                        conflict.range = change.edits[i].range;
                        conflict.ourContent = change.edits[i].newText;
                        conflict.theirContent = change.edits[j].newText;
                        conflict.baseContent = change.edits[i].oldText;
                        conflict.description = "Overlapping edits detected";
                        conflicts.push_back(conflict);
                    }
                }
            }
        }
        
        return conflicts;
    }
    
    bool resolveConflict(const std::string& sessionId,
                        const ConflictResolution& resolution) override {
        // Implementation would merge conflicting edits
        // For now, return true to indicate resolution accepted
        return true;
    }
    
    // Refactoring operations
    bool executeRefactoring(const std::string& sessionId,
                           RefactoringKind kind,
                           const std::unordered_map<std::string, std::string>& params) override {
        // Implementation would execute specific refactoring
        // This is a placeholder that creates a proposed edit
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        // Create a placeholder edit
        TextEdit edit;
        edit.id = generateId();
        edit.source = params.count("file") ? params.at("file") : "";
        edit.description = "Refactoring: " + std::to_string(static_cast<int>(kind));
        edit.timestamp = std::chrono::system_clock::now();
        edit.applied = false;
        edit.confirmed = false;
        
        // Add to first file in session
        if (!it->second.changes.empty()) {
            it->second.changes[0].edits.push_back(edit);
        }
        
        return true;
    }
    
    std::vector<RefactoringOperation> getAvailableRefactorings() const override {
        return refactoringOps_;
    }
    
    // Statistics
    std::unordered_map<std::string, uint32_t> getStatistics(
        const std::string& sessionId) const override {
        std::unordered_map<std::string, uint32_t> stats;
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return stats;
        }
        
        stats["totalFiles"] = static_cast<uint32_t>(it->second.changes.size());
        
        uint32_t totalEdits = 0;
        uint32_t appliedEdits = 0;
        uint32_t pendingEdits = 0;
        
        for (const auto& change : it->second.changes) {
            totalEdits += static_cast<uint32_t>(change.edits.size());
            for (const auto& edit : change.edits) {
                if (edit.applied) {
                    appliedEdits++;
                } else {
                    pendingEdits++;
                }
            }
        }
        
        stats["totalEdits"] = totalEdits;
        stats["appliedEdits"] = appliedEdits;
        stats["pendingEdits"] = pendingEdits;
        
        return stats;
    }
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IInteractiveRefactoringEngine> createInteractiveRefactoringEngine() {
    return std::make_unique<InteractiveRefactoringEngine>();
}

} // namespace Refactoring
} // namespace RawrXD
