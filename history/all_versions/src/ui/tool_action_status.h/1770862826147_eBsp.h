// ============================================================================
// tool_action_status.h — Tool Action Status Rendering for Chat Responses
// ============================================================================
// Renders inline status lines for tool/agent actions in chat responses:
//   "✅ Finished with 1 step"
//   "⚡ Executed command in terminal"
//   "📋 Managed todo list"
//   "📝 Reviewed 'main.cpp' lines 42-128"
//   "🔍 Searched for regex 'handleInit' in *.cpp"
//   "📂 Reviewed changed files in the active git repo"
//   "🔧 Ran command: cmake --build ."
//   "📖 Reviewed 'kernel.ASM' lines 100-250"
//
// Three rendering modes:
//   1. PlainText — Win32 EDIT controls (native IDE sidebar + ChatPanel)
//   2. HTML      — ChatMessageRenderer / WebView2 chatbot
//   3. JSON      — AgentTranscript serialization for replay
//
// Consumes: ToolCallResult, TranscriptStep
// Pattern:  PatchResult-style, no exceptions, factory results.
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace RawrXD {
namespace UI {

// ============================================================================
// Tool Action Kind — what category of action was performed
// ============================================================================
enum class ToolActionKind : uint8_t {
    ReadFile          = 0,   // Read/reviewed a file
    EditFile          = 1,   // Edited/replaced in a file
    CreateFile        = 2,   // Created a new file
    DeleteFile        = 3,   // Deleted a file
    RunTerminal       = 4,   // Executed a terminal command
    SearchGrep        = 5,   // Text/regex search in workspace
    SearchSemantic    = 6,   // Semantic code search
    SearchFiles       = 7,   // File glob search
    ListDirectory     = 8,   // Listed directory contents
    ManagedTodoList   = 9,   // Updated/managed todo list
    ReviewedChanges   = 10,  // Reviewed git changed files
    ListCodeUsages    = 11,  // Found code references/usages
    GetErrors         = 12,  // Retrieved compile/lint errors
    FetchWebpage      = 13,  // Fetched a webpage
    RunSubagent       = 14,  // Launched a sub-agent
    NotebookRun       = 15,  // Ran a notebook cell
    NotebookEdit      = 16,  // Edited a notebook cell
    GitOperation      = 17,  // Git commit/push/pull/diff
    BuildProject      = 18,  // Build/compile project
    FinishedStep      = 19,  // Completed a step (summary)
    MultiReplace      = 20,  // Multi-file replace
    OpenBrowser       = 21,  // Opened browser preview
    AgentThinking     = 22,  // Agent is reasoning/thinking
    Custom            = 255  // Custom/other action
};

// ============================================================================
// Tool Action State — lifecycle of a single action
// ============================================================================
enum class ToolActionState : uint8_t {
    Pending    = 0,   // Queued, not yet started
    Running    = 1,   // Currently executing
    Completed  = 2,   // Finished successfully
    Failed     = 3,   // Finished with error
    Skipped    = 4    // Skipped (e.g., cached)
};

// ============================================================================
// Tool Action Status — single action status entry
// ============================================================================
struct ToolActionStatus {
    ToolActionKind   kind         = ToolActionKind::Custom;
    ToolActionState  state        = ToolActionState::Pending;
    std::string      summary;          // "Reviewed 'main.cpp' lines 42-128"
    std::string      detail;           // Optional: extra detail (file path, error msg)
    int64_t          durationMs   = 0; // Execution time in milliseconds
    int              stepNumber   = 0; // Step index in agent loop
    int              lineStart    = 0; // For file reads: start line
    int              lineEnd      = 0; // For file reads: end line
    std::string      filePath;         // For file operations: target file
    std::string      command;          // For terminal: the command run
    std::string      searchQuery;      // For search: the query/pattern
    int              matchCount   = 0; // For search: number of results

    // ---- Factory Methods ----

    static ToolActionStatus ReadFileAction(const std::string& path, int startLine, int endLine) {
        ToolActionStatus s;
        s.kind = ToolActionKind::ReadFile;
        s.state = ToolActionState::Completed;
        s.filePath = path;
        s.lineStart = startLine;
        s.lineEnd = endLine;
        // Extract filename from path
        std::string name = path;
        auto pos = name.find_last_of("/\\");
        if (pos != std::string::npos) name = name.substr(pos + 1);
        s.summary = "Reviewed \"" + name + "\" lines " +
                    std::to_string(startLine) + "-" + std::to_string(endLine);
        return s;
    }

    static ToolActionStatus EditFileAction(const std::string& path, int linesAffected = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::EditFile;
        s.state = ToolActionState::Completed;
        s.filePath = path;
        std::string name = path;
        auto pos = name.find_last_of("/\\");
        if (pos != std::string::npos) name = name.substr(pos + 1);
        if (linesAffected > 0)
            s.summary = "Edited \"" + name + "\" (" + std::to_string(linesAffected) + " lines affected)";
        else
            s.summary = "Edited \"" + name + "\"";
        return s;
    }

    static ToolActionStatus CreateFileAction(const std::string& path) {
        ToolActionStatus s;
        s.kind = ToolActionKind::CreateFile;
        s.state = ToolActionState::Completed;
        s.filePath = path;
        std::string name = path;
        auto pos = name.find_last_of("/\\");
        if (pos != std::string::npos) name = name.substr(pos + 1);
        s.summary = "Created \"" + name + "\"";
        return s;
    }

    static ToolActionStatus RunTerminalAction(const std::string& cmd, int64_t durationMs = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::RunTerminal;
        s.state = ToolActionState::Completed;
        s.command = cmd;
        s.durationMs = durationMs;
        // Truncate long commands
        std::string display = cmd;
        if (display.size() > 60) display = display.substr(0, 57) + "...";
        s.summary = "Executed command in terminal";
        s.detail = display;
        return s;
    }

    static ToolActionStatus SearchGrepAction(const std::string& query, const std::string& pattern,
                                              int matches, bool isRegex = false) {
        ToolActionStatus s;
        s.kind = ToolActionKind::SearchGrep;
        s.state = ToolActionState::Completed;
        s.searchQuery = query;
        s.matchCount = matches;
        std::string type = isRegex ? "regex" : "text";
        if (pattern.empty())
            s.summary = "Searched for " + type + " \"" + query + "\"";
        else
            s.summary = "Searched for " + type + " \"" + query + "\" in " + pattern;
        if (matches > 0)
            s.summary += ", " + std::to_string(matches) + " results";
        else
            s.summary += ", no results";
        return s;
    }

    static ToolActionStatus SearchSemanticAction(const std::string& query, int matches = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::SearchSemantic;
        s.state = ToolActionState::Completed;
        s.searchQuery = query;
        s.matchCount = matches;
        s.summary = "Semantic search: \"" + query + "\"";
        return s;
    }

    static ToolActionStatus SearchFilesAction(const std::string& glob, int matches = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::SearchFiles;
        s.state = ToolActionState::Completed;
        s.searchQuery = glob;
        s.matchCount = matches;
        s.summary = "File search: \"" + glob + "\"";
        if (matches > 0) s.summary += ", " + std::to_string(matches) + " files";
        return s;
    }

    static ToolActionStatus ListDirAction(const std::string& path) {
        ToolActionStatus s;
        s.kind = ToolActionKind::ListDirectory;
        s.state = ToolActionState::Completed;
        s.filePath = path;
        std::string name = path;
        auto pos = name.find_last_of("/\\");
        if (pos != std::string::npos) name = name.substr(pos + 1);
        if (name.empty()) name = path;
        s.summary = "Listed directory \"" + name + "\"";
        return s;
    }

    static ToolActionStatus ManagedTodoAction(int totalItems = 0, int completed = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::ManagedTodoList;
        s.state = ToolActionState::Completed;
        if (totalItems > 0)
            s.summary = "Managed todo list (" + std::to_string(completed) +
                        "/" + std::to_string(totalItems) + " completed)";
        else
            s.summary = "Managed todo list";
        return s;
    }

    static ToolActionStatus ReviewedChangesAction(int changedFiles = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::ReviewedChanges;
        s.state = ToolActionState::Completed;
        if (changedFiles > 0)
            s.summary = "Reviewed changed files in the active git repo (" +
                        std::to_string(changedFiles) + " files)";
        else
            s.summary = "Reviewed changed files in the active git repo";
        return s;
    }

    static ToolActionStatus ListCodeUsagesAction(const std::string& symbol, int usages = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::ListCodeUsages;
        s.state = ToolActionState::Completed;
        s.searchQuery = symbol;
        s.matchCount = usages;
        s.summary = "Found usages of \"" + symbol + "\"";
        if (usages > 0) s.summary += ", " + std::to_string(usages) + " references";
        return s;
    }

    static ToolActionStatus GetErrorsAction(int errorCount = 0) {
        ToolActionStatus s;
        s.kind = ToolActionKind::GetErrors;
        s.state = ToolActionState::Completed;
        if (errorCount > 0)
            s.summary = "Retrieved " + std::to_string(errorCount) + " compile/lint errors";
        else
            s.summary = "Checked for errors (none found)";
        return s;
    }

    static ToolActionStatus FetchWebAction(const std::string& url) {
        ToolActionStatus s;
        s.kind = ToolActionKind::FetchWebpage;
        s.state = ToolActionState::Completed;
        s.detail = url;
        // Truncate URL for display
        std::string display = url;
        if (display.size() > 50) display = display.substr(0, 47) + "...";
        s.summary = "Fetched webpage: " + display;
        return s;
    }

    static ToolActionStatus SubagentAction(const std::string& description) {
        ToolActionStatus s;
        s.kind = ToolActionKind::RunSubagent;
        s.state = ToolActionState::Completed;
        s.summary = "Ran sub-agent: " + description;
        return s;
    }

    static ToolActionStatus BuildAction(const std::string& target, bool success = true) {
        ToolActionStatus s;
        s.kind = ToolActionKind::BuildProject;
        s.state = success ? ToolActionState::Completed : ToolActionState::Failed;
        s.summary = success ? ("Built project: " + target) : ("Build failed: " + target);
        return s;
    }

    static ToolActionStatus FinishedAction(int totalSteps = 1) {
        ToolActionStatus s;
        s.kind = ToolActionKind::FinishedStep;
        s.state = ToolActionState::Completed;
        s.summary = "Finished with " + std::to_string(totalSteps) +
                    (totalSteps == 1 ? " step" : " steps");
        return s;
    }

    static ToolActionStatus MultiReplaceAction(int filesChanged, int replacements) {
        ToolActionStatus s;
        s.kind = ToolActionKind::MultiReplace;
        s.state = ToolActionState::Completed;
        s.summary = "Multi-replace: " + std::to_string(replacements) +
                    " replacements across " + std::to_string(filesChanged) + " files";
        return s;
    }

    static ToolActionStatus ThinkingAction() {
        ToolActionStatus s;
        s.kind = ToolActionKind::AgentThinking;
        s.state = ToolActionState::Running;
        s.summary = "Thinking...";
        return s;
    }

    static ToolActionStatus CustomAction(const std::string& summary) {
        ToolActionStatus s;
        s.kind = ToolActionKind::Custom;
        s.state = ToolActionState::Completed;
        s.summary = summary;
        return s;
    }
};

// ============================================================================
// Tool Action Status Formatter — renders status lines for all 3 IDE layers
// ============================================================================

class ToolActionStatusFormatter {
public:
    // ---- Icon lookups ----
    static const char* iconForKind(ToolActionKind kind);
    static const char* stateIcon(ToolActionState state);
    static const char* cssClassForState(ToolActionState state);

    // ---- Plain Text (Win32 EDIT controls) ----
    // Returns: "✅ Reviewed 'main.cpp' lines 42-128\r\n"
    static std::string formatPlainText(const ToolActionStatus& action);

    // Format a complete list of actions as plain text block
    static std::string formatPlainTextBlock(const std::vector<ToolActionStatus>& actions,
                                             int totalSteps = 0);

    // ---- HTML (ChatMessageRenderer / WebView2) ----
    // Returns: <div class="tool-action-step done">...</div>
    static std::string formatHtml(const ToolActionStatus& action);

    // Format a complete list as collapsible HTML block
    static std::string formatHtmlBlock(const std::vector<ToolActionStatus>& actions,
                                        int totalSteps = 0);

    // ---- CSS for tool action steps (injected into ChatMessageRenderer) ----
    static std::string generateCSS();

    // ---- JSON (AgentTranscript serialization) ----
    static std::string formatJson(const ToolActionStatus& action);
};

// ============================================================================
// Tool Action Accumulator — collects actions during a chat response
// ============================================================================

class ToolActionAccumulator {
public:
    void addAction(const ToolActionStatus& action);
    void clear();

    int  totalActions() const { return static_cast<int>(m_actions.size()); }
    int  completedActions() const;
    int  failedActions() const;
    int64_t totalDurationMs() const;

    const std::vector<ToolActionStatus>& actions() const { return m_actions; }

    // Render all accumulated actions
    std::string renderPlainText() const;
    std::string renderHtml() const;

private:
    std::vector<ToolActionStatus> m_actions;
};

} // namespace UI
} // namespace RawrXD
