// ============================================================================
// vscode_extension_api.h — VS Code Extension API Compatibility Layer
// ============================================================================
//
// Phase 29: VS Code Extension API Import Layer
//
// Purpose:
//   Provides a native C++ shim that mirrors the VS Code Extension API surface.
//   Extensions (VSIX-loaded or native DLL) can program against this API and
//   have their calls routed to the RawrXD Win32IDE infrastructure.
//
// Coverage:
//   vscode.commands        → CommandRegistry (IDM_ dispatch + custom)
//   vscode.window          → WindowAPI (Win32 HWND, message boxes, pickers)
//   vscode.workspace       → WorkspaceAPI (file watchers, config, folders)
//   vscode.languages       → LanguagesAPI (diagnostics, completions, hovers)
//   vscode.env             → EnvironmentAPI (shell, clipboard, URI)
//   vscode.extensions      → ExtensionsRegistry (query installed, activate)
//   vscode.debug           → DebugAPI (breakpoints, debug sessions)
//   vscode.tasks           → TaskAPI (task providers, execution)
//   vscode.scm             → SCMProvider (source control integration)
//   Disposable             → RAII disposable pattern
//   EventEmitter<T>        → Function pointer ring-buffer events
//   TreeDataProvider<T>    → Native TreeView bridge
//   StatusBarItem          → Win32 status bar segment
//   OutputChannel          → IDE output panel
//   TextDocument           → Buffer model (IEditorEngine bridge)
//   TextEditor             → Active editor state
//   Uri                    → file:// and custom scheme URIs
//   Range / Position       → Line/column geometry
//   Diagnostic             → LSP-compatible diagnostics
//   CompletionItem         → Autocomplete entries
//   CancellationToken      → Cooperative cancellation
//
// Design Principles:
//   - PatchResult-style structured results (no exceptions)
//   - Function pointer callbacks (no std::function in hot path)
//   - All COM/D2D calls on UI thread (STA)
//   - Thread-safe registration (mutex-guarded)
//   - No circular includes (depends only on PatchResult, Win32)
//   - Compatible with existing VSIXLoader + ExtensionLoader
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <filesystem>

// ============================================================================
// Forward Declarations (avoid circular includes)
// ============================================================================
class IEditorEngine;
struct EditorEngineResult;
struct IDETheme;
class Win32IDE;

// ============================================================================
// API Result (PatchResult-compatible)
// ============================================================================
struct VSCodeAPIResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static VSCodeAPIResult ok(const char* msg = "Success") {
        return { true, msg, 0 };
    }
    static VSCodeAPIResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Disposable — RAII cleanup handle (mirrors vscode.Disposable)
// ============================================================================
struct Disposable {
    uint64_t    id;                 // Unique handle
    void        (*disposeFn)(void* ctx);
    void*       context;
    bool        disposed;

    void dispose() {
        if (!disposed && disposeFn) {
            disposeFn(context);
            disposed = true;
        }
    }
};

// ============================================================================
// CancellationToken — Cooperative cancellation (mirrors vscode.CancellationToken)
// ============================================================================
struct CancellationToken {
    std::atomic<bool>*  isCancellationRequested;    // Shared flag
    uint64_t            tokenId;

    bool cancelled() const {
        return isCancellationRequested ? isCancellationRequested->load(std::memory_order_acquire) : false;
    }
};

struct CancellationTokenSource {
    std::atomic<bool>   cancelFlag;
    CancellationToken   token;
    uint64_t            sourceId;

    CancellationTokenSource() : cancelFlag(false), sourceId(0) {
        token.isCancellationRequested = &cancelFlag;
        token.tokenId = 0;
    }

    void cancel() {
        cancelFlag.store(true, std::memory_order_release);
    }

    void reset() {
        cancelFlag.store(false, std::memory_order_release);
    }
};

// ============================================================================
// Position — Line/column in a document (mirrors vscode.Position)
// ============================================================================
struct VSCodePosition {
    int     line;       // 0-based
    int     character;  // 0-based, UTF-16 code units

    bool operator==(const VSCodePosition& o) const { return line == o.line && character == o.character; }
    bool operator!=(const VSCodePosition& o) const { return !(*this == o); }
    bool operator<(const VSCodePosition& o) const {
        return line < o.line || (line == o.line && character < o.character);
    }
    bool operator<=(const VSCodePosition& o) const { return *this < o || *this == o; }
    bool operator>(const VSCodePosition& o) const { return !(*this <= o); }
    bool operator>=(const VSCodePosition& o) const { return !(*this < o); }

    VSCodePosition translate(int lineDelta, int charDelta = 0) const {
        return { line + lineDelta, character + charDelta };
    }

    VSCodePosition with(int newLine = -1, int newChar = -1) const {
        return { (newLine >= 0) ? newLine : line, (newChar >= 0) ? newChar : character };
    }
};

// ============================================================================
// Range — Start/end positions (mirrors vscode.Range)
// ============================================================================
struct VSCodeRange {
    VSCodePosition  start;
    VSCodePosition  end;

    bool isEmpty() const { return start == end; }
    bool isSingleLine() const { return start.line == end.line; }

    bool contains(const VSCodePosition& pos) const {
        return pos >= start && pos <= end;
    }

    bool contains(const VSCodeRange& other) const {
        return other.start >= start && other.end <= end;
    }

    VSCodeRange intersection(const VSCodeRange& other) const {
        VSCodePosition s = (start > other.start) ? start : other.start;
        VSCodePosition e = (end < other.end) ? end : other.end;
        if (s > e) return { {0,0}, {0,0} };
        return { s, e };
    }

    VSCodeRange unionWith(const VSCodeRange& other) const {
        VSCodePosition s = (start < other.start) ? start : other.start;
        VSCodePosition e = (end > other.end) ? end : other.end;
        return { s, e };
    }
};

// ============================================================================
// Selection — Range + anchor/active direction (mirrors vscode.Selection)
// ============================================================================
struct VSCodeSelection {
    VSCodePosition  anchor;
    VSCodePosition  active;
    bool            isReversed;

    VSCodeRange toRange() const {
        if (anchor < active) return { anchor, active };
        return { active, anchor };
    }

    bool isEmpty() const { return anchor == active; }
};

// ============================================================================
// Uri — Resource identifier (mirrors vscode.Uri)
// ============================================================================
struct VSCodeUri {
    std::string     scheme;         // "file", "untitled", "vscode", etc.
    std::string     authority;      // hostname for remote URIs
    std::string     path;           // /c:/Users/foo/bar.txt
    std::string     query;          // query string
    std::string     fragment;       // fragment

    std::string fsPath() const {
        // Convert URI path to filesystem path (Windows)
        if (scheme == "file" && path.size() > 1 && path[0] == '/') {
            std::string result = path.substr(1);  // Remove leading /
            for (auto& ch : result) {
                if (ch == '/') ch = '\\';
            }
            return result;
        }
        return path;
    }

    std::string toString() const {
        std::string result = scheme + "://";
        if (!authority.empty()) result += authority;
        result += path;
        if (!query.empty()) result += "?" + query;
        if (!fragment.empty()) result += "#" + fragment;
        return result;
    }

    static VSCodeUri file(const std::string& fsPath) {
        VSCodeUri uri;
        uri.scheme = "file";
        uri.path = "/";
        for (char ch : fsPath) {
            uri.path += (ch == '\\') ? '/' : ch;
        }
        return uri;
    }

    static VSCodeUri parse(const std::string& uriStr) {
        VSCodeUri uri;
        size_t schemeEnd = uriStr.find("://");
        if (schemeEnd != std::string::npos) {
            uri.scheme = uriStr.substr(0, schemeEnd);
            size_t pathStart = schemeEnd + 3;
            size_t queryStart = uriStr.find('?', pathStart);
            size_t fragStart = uriStr.find('#', pathStart);
            size_t pathEnd = std::min(queryStart, fragStart);
            if (pathEnd == std::string::npos) pathEnd = uriStr.size();
            uri.path = uriStr.substr(pathStart, pathEnd - pathStart);
            if (queryStart != std::string::npos) {
                size_t qEnd = (fragStart != std::string::npos) ? fragStart : uriStr.size();
                uri.query = uriStr.substr(queryStart + 1, qEnd - queryStart - 1);
            }
            if (fragStart != std::string::npos) {
                uri.fragment = uriStr.substr(fragStart + 1);
            }
        } else {
            uri.scheme = "file";
            uri.path = uriStr;
        }
        return uri;
    }

    bool operator==(const VSCodeUri& o) const {
        return scheme == o.scheme && authority == o.authority &&
               path == o.path && query == o.query && fragment == o.fragment;
    }
};

// ============================================================================
// VSCodeDiagnosticSeverity — Matches VS Code's enum values exactly
// (Named VSCode* to avoid ambiguity with LSP DiagnosticSeverity)
// ============================================================================
enum class VSCodeDiagnosticSeverity : int {
    Error       = 0,
    Warning     = 1,
    Information = 2,
    Hint        = 3
};

// ============================================================================
// Diagnostic — Problem marker (mirrors vscode.Diagnostic)
// ============================================================================
struct VSCodeDiagnostic {
    VSCodeRange             range;
    std::string             message;
    VSCodeDiagnosticSeverity severity;
    std::string             source;         // e.g. "rawr-lint"
    std::string             code;           // error code string
    std::vector<VSCodeDiagnostic> relatedInformation;   // Linked locations
    VSCodeUri               relatedUri;
};

// ============================================================================
// DiagnosticCollection — Grouped diagnostics (mirrors vscode.DiagnosticCollection)
// ============================================================================
class DiagnosticCollection {
public:
    std::string                                 name;
    std::unordered_map<std::string, std::vector<VSCodeDiagnostic>>  entries;    // uri → diagnostics
    mutable std::mutex                          mutex;

    void set(const VSCodeUri& uri, const std::vector<VSCodeDiagnostic>& diagnostics) {
        std::lock_guard<std::mutex> lock(mutex);
        entries[uri.toString()] = diagnostics;
    }

    void deleteUri(const VSCodeUri& uri) {
        std::lock_guard<std::mutex> lock(mutex);
        entries.erase(uri.toString());
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        entries.clear();
    }

    bool has(const VSCodeUri& uri) const {
        std::lock_guard<std::mutex> lock(mutex);
        return entries.count(uri.toString()) > 0;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        size_t total = 0;
        for (const auto& [k, v] : entries) total += v.size();
        return total;
    }
};

// ============================================================================
// CompletionItemKind — Matches VS Code's enum values
// ============================================================================
enum class CompletionItemKind : int {
    Text            = 0,
    Method          = 1,
    Function        = 2,
    Constructor     = 3,
    Field           = 4,
    Variable        = 5,
    Class           = 6,
    Interface       = 7,
    Module          = 8,
    Property        = 9,
    Unit            = 10,
    Value           = 11,
    Enum            = 12,
    Keyword         = 13,
    Snippet         = 14,
    Color           = 15,
    File            = 16,
    Reference       = 17,
    Folder          = 18,
    EnumMember      = 19,
    Constant        = 20,
    Struct          = 21,
    Event           = 22,
    Operator        = 23,
    TypeParameter   = 24
};

// ============================================================================
// CompletionItem — Autocomplete entry (mirrors vscode.CompletionItem)
// ============================================================================
struct VSCodeCompletionItem {
    std::string             label;
    CompletionItemKind      kind;
    std::string             detail;         // Short description
    std::string             documentation;  // Full docs (Markdown)
    std::string             insertText;     // Text to insert (may differ from label)
    std::string             filterText;     // Text used for filtering
    std::string             sortText;       // Text used for sorting
    bool                    preselect;      // Whether to preselect this item
    VSCodeRange             range;          // Range to replace

    VSCodeCompletionItem() : kind(CompletionItemKind::Text), preselect(false) {}
};

// ============================================================================
// Hover — Tooltip content (mirrors vscode.Hover)
// ============================================================================
struct VSCodeHover {
    std::vector<std::string>    contents;   // Markdown strings
    VSCodeRange                 range;      // Optional highlight range
};

// ============================================================================
// Location — Uri + Range (mirrors vscode.Location)
// ============================================================================
struct VSCodeLocation {
    VSCodeUri       uri;
    VSCodeRange     range;
};

// ============================================================================
// VSCodeSymbolKind — Matches VS Code's enum values
// ============================================================================
enum class VSCodeSymbolKind : int {
    File            = 0,
    Module          = 1,
    Namespace       = 2,
    Package         = 3,
    Class           = 4,
    Method          = 5,
    Property        = 6,
    Field           = 7,
    Constructor     = 8,
    Enum            = 9,
    Interface       = 10,
    Function        = 11,
    Variable        = 12,
    Constant        = 13,
    String          = 14,
    Number          = 15,
    Boolean         = 16,
    Array           = 17,
    Object          = 18,
    Key             = 19,
    Null            = 20,
    EnumMember      = 21,
    Struct          = 22,
    Event           = 23,
    Operator        = 24,
    TypeParameter   = 25
};

// ============================================================================
// DocumentSymbol — Hierarchical symbol in a document
// ============================================================================
struct VSCodeDocumentSymbol {
    std::string                         name;
    std::string                         detail;
    VSCodeSymbolKind                    kind;
    VSCodeRange                         range;
    VSCodeRange                         selectionRange;
    std::vector<VSCodeDocumentSymbol>   children;
};

// ============================================================================
// WorkspaceEdit — Batch of text edits across files
// ============================================================================
struct VSCodeTextEdit {
    VSCodeRange     range;
    std::string     newText;
};

struct VSCodeWorkspaceEdit {
    std::unordered_map<std::string, std::vector<VSCodeTextEdit>>    changes;    // uri → edits

    void set(const VSCodeUri& uri, const std::vector<VSCodeTextEdit>& edits) {
        changes[uri.toString()] = edits;
    }

    bool has(const VSCodeUri& uri) const {
        return changes.count(uri.toString()) > 0;
    }

    size_t size() const {
        size_t total = 0;
        for (const auto& [k, v] : changes) total += v.size();
        return total;
    }
};

// ============================================================================
// TextDocument — Read-only document model (mirrors vscode.TextDocument)
// ============================================================================
struct VSCodeTextDocument {
    VSCodeUri       uri;
    std::string     fileName;           // Short name
    std::string     languageId;         // "cpp", "python", etc.
    int             version;            // Increments on each change
    bool            isDirty;
    bool            isUntitled;
    bool            isClosed;
    uint32_t        lineCount;

    // Content access (callback-based to avoid large copies)
    typedef bool (*GetTextCallback)(const char* text, uint32_t length, void* userData);
    GetTextCallback getTextFn;
    void*           getTextCtx;

    // Line access
    typedef bool (*GetLineCallback)(int line, const char* text, uint32_t length, void* userData);
    GetLineCallback getLineFn;
    void*           getLineCtx;
};

// ============================================================================
// TextEditor — Active editor state (mirrors vscode.TextEditor)
// ============================================================================
struct VSCodeTextEditor {
    VSCodeTextDocument*         document;
    VSCodeSelection             selection;
    std::vector<VSCodeSelection> selections;
    int                         viewColumn;     // Editor group (1-based)

    // Edit operations (routed to IEditorEngine)
    typedef VSCodeAPIResult (*EditCallback)(const VSCodeTextEdit* edits, size_t count, void* ctx);
    EditCallback    editFn;
    void*           editCtx;

    // Reveal range
    typedef VSCodeAPIResult (*RevealCallback)(const VSCodeRange* range, int revealType, void* ctx);
    RevealCallback  revealFn;
    void*           revealCtx;
};

// ============================================================================
// StatusBarItem — Status bar segment (mirrors vscode.StatusBarItem)
// ============================================================================
enum class StatusBarAlignment : int {
    Left    = 1,
    Right   = 2
};

struct VSCodeStatusBarItem {
    uint64_t            id;
    StatusBarAlignment  alignment;
    int                 priority;
    std::string         text;
    std::string         tooltip;
    std::string         color;          // "#RRGGBB" or theme color name
    std::string         backgroundColor;
    std::string         command;        // Command ID to execute on click
    bool                visible;

    void show() { visible = true; }
    void hide() { visible = false; }
};

// ============================================================================
// OutputChannel — IDE output panel (mirrors vscode.OutputChannel)
// ============================================================================
struct VSCodeOutputChannel {
    uint64_t        id;
    std::string     name;
    bool            visible;
    mutable std::mutex  mutex;
    std::string     buffer;

    void append(const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        buffer += value;
    }

    void appendLine(const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        buffer += value + "\n";
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        buffer.clear();
    }

    void show(bool preserveFocus = false) { visible = true; (void)preserveFocus; }
    void hide() { visible = false; }

    void dispose() {
        clear();
        visible = false;
    }
};

// ============================================================================
// TreeItem — TreeView node (mirrors vscode.TreeItem)
// ============================================================================
enum class TreeItemCollapsibleState : int {
    None        = 0,
    Collapsed   = 1,
    Expanded    = 2
};

struct VSCodeTreeItem {
    std::string                     label;
    std::string                     description;    // Secondary text
    std::string                     tooltip;
    std::string                     iconPath;       // Path to icon
    std::string                     contextValue;   // For when-clause context
    TreeItemCollapsibleState        collapsibleState;
    std::string                     command;         // Command on click
    std::string                     id;              // Stable identity
    void*                           userData;        // Extension-specific data

    VSCodeTreeItem() : collapsibleState(TreeItemCollapsibleState::None), userData(nullptr) {}
};

// ============================================================================
// TreeDataProvider<T> — Tree content provider (mirrors vscode.TreeDataProvider)
// ============================================================================
struct VSCodeTreeDataProvider {
    // Get root elements (parent == nullptr) or children
    typedef bool (*GetChildrenCallback)(const VSCodeTreeItem* parent,
                                        VSCodeTreeItem* outItems, size_t maxItems,
                                        size_t* outCount, void* ctx);
    // Get tree item representation
    typedef bool (*GetTreeItemCallback)(const VSCodeTreeItem* element,
                                        VSCodeTreeItem* outItem, void* ctx);
    // Refresh signal
    typedef void (*OnDidChangeCallback)(const VSCodeTreeItem* changed, void* ctx);

    GetChildrenCallback     getChildrenFn;
    GetTreeItemCallback     getTreeItemFn;
    OnDidChangeCallback     onDidChangeFn;
    void*                   context;
    std::string             viewId;         // Registered view ID

    VSCodeTreeDataProvider() : getChildrenFn(nullptr), getTreeItemFn(nullptr),
                                onDidChangeFn(nullptr), context(nullptr) {}
};

// ============================================================================
// QuickPickItem — Item in quick pick list (mirrors vscode.QuickPickItem)
// ============================================================================
struct VSCodeQuickPickItem {
    std::string     label;
    std::string     description;
    std::string     detail;
    bool            picked;
    bool            alwaysShow;
    void*           userData;

    VSCodeQuickPickItem() : picked(false), alwaysShow(false), userData(nullptr) {}
};

// ============================================================================
// InputBoxOptions — Input box configuration
// ============================================================================
struct VSCodeInputBoxOptions {
    std::string     title;
    std::string     prompt;
    std::string     placeHolder;
    std::string     value;
    bool            password;
    bool            ignoreFocusOut;

    // Validation callback — returns error message or empty string for valid
    typedef const char* (*ValidateCallback)(const char* value, void* ctx);
    ValidateCallback    validateInputFn;
    void*               validateCtx;

    VSCodeInputBoxOptions() : password(false), ignoreFocusOut(false),
                               validateInputFn(nullptr), validateCtx(nullptr) {}
};

// ============================================================================
// MessageItem — Button in message dialog
// ============================================================================
struct VSCodeMessageItem {
    std::string     title;
    bool            isCloseAffordance;

    VSCodeMessageItem() : isCloseAffordance(false) {}
};

// ============================================================================
// FileSystemWatcher — File change monitor (mirrors vscode.FileSystemWatcher)
// ============================================================================
enum class FileChangeType : int {
    Created     = 1,
    Changed     = 2,
    Deleted     = 3
};

struct VSCodeFileChangeEvent {
    FileChangeType  type;
    VSCodeUri       uri;
};

struct VSCodeFileSystemWatcher {
    uint64_t        id;
    std::string     globPattern;
    bool            ignoreCreateEvents;
    bool            ignoreChangeEvents;
    bool            ignoreDeleteEvents;

    // Callback for file changes
    typedef void (*FileChangeCallback)(const VSCodeFileChangeEvent* event, void* ctx);
    FileChangeCallback  onDidChangeFn;
    FileChangeCallback  onDidCreateFn;
    FileChangeCallback  onDidDeleteFn;
    void*               callbackCtx;

    // Win32 implementation state
    HANDLE              directoryHandle;
    HANDLE              watchThread;
    std::atomic<bool>   active;

    VSCodeFileSystemWatcher()
        : id(0), ignoreCreateEvents(false), ignoreChangeEvents(false), ignoreDeleteEvents(false),
          onDidChangeFn(nullptr), onDidCreateFn(nullptr), onDidDeleteFn(nullptr),
          callbackCtx(nullptr), directoryHandle(INVALID_HANDLE_VALUE),
          watchThread(nullptr), active(false) {}
};

// ============================================================================
// TaskDefinition — Task metadata (mirrors vscode.TaskDefinition)
// ============================================================================
struct VSCodeTaskDefinition {
    std::string     type;               // e.g. "shell", "process"
    std::string     taskLabel;
    std::string     command;
    std::vector<std::string> args;
    std::string     cwd;
    bool            isBackground;
    std::string     group;              // "build", "test", "none"
    std::string     problemMatcher;

    VSCodeTaskDefinition() : isBackground(false) {}
};

// ============================================================================
// DebugConfiguration — Debug launch config
// ============================================================================
struct VSCodeDebugConfiguration {
    std::string     type;               // e.g. "cppdbg", "node"
    std::string     name;
    std::string     request;            // "launch" or "attach"
    std::string     program;
    std::vector<std::string> args;
    std::string     cwd;
    bool            stopOnEntry;
    std::string     miDebuggerPath;
    std::unordered_map<std::string, std::string> env;

    VSCodeDebugConfiguration() : stopOnEntry(false) {}
};

// ============================================================================
// Breakpoint — Debug breakpoint
// ============================================================================
struct VSCodeBreakpoint {
    uint64_t        id;
    bool            enabled;
    std::string     condition;
    std::string     hitCondition;
    std::string     logMessage;
    VSCodeUri       uri;
    int             line;
    int             column;

    VSCodeBreakpoint() : id(0), enabled(true), line(0), column(0) {}
};

// ============================================================================
// TerminalOptions — Terminal creation options
// ============================================================================
struct VSCodeTerminalOptions {
    std::string     name;
    std::string     shellPath;
    std::vector<std::string> shellArgs;
    std::string     cwd;
    std::unordered_map<std::string, std::string> env;
    bool            hideFromUser;

    VSCodeTerminalOptions() : hideFromUser(false) {}
};

// ============================================================================
// Terminal — Pseudo-terminal handle
// ============================================================================
struct VSCodeTerminal {
    uint64_t        id;
    std::string     name;
    int             processId;
    int             exitStatus;
    bool            alive;

    typedef VSCodeAPIResult (*SendTextCallback)(const char* text, bool addNewline, void* ctx);
    SendTextCallback    sendTextFn;
    void*               sendTextCtx;

    void show(bool preserveFocus = false) { (void)preserveFocus; }
    void hide() {}

    VSCodeTerminal() : id(0), processId(0), exitStatus(-1), alive(false),
                       sendTextFn(nullptr), sendTextCtx(nullptr) {}
};

// ============================================================================
// WebviewPanel — Webview panel (mirrors vscode.WebviewPanel)
// ============================================================================
struct VSCodeWebviewOptions {
    bool            enableScripts;
    bool            enableForms;
    bool            retainContextWhenHidden;
    std::vector<VSCodeUri>  localResourceRoots;

    VSCodeWebviewOptions() : enableScripts(false), enableForms(false),
                              retainContextWhenHidden(false) {}
};

struct VSCodeWebviewPanel {
    uint64_t        id;
    std::string     viewType;
    std::string     title;
    int             viewColumn;
    bool            active;
    bool            visible;
    HWND            hwnd;           // Native container

    // HTML content
    std::string     html;

    // Message passing
    typedef void (*OnDidReceiveMessageCallback)(const char* messageJson, void* ctx);
    OnDidReceiveMessageCallback onMessageFn;
    void*                       onMessageCtx;

    typedef VSCodeAPIResult (*PostMessageCallback)(const char* messageJson, void* ctx);
    PostMessageCallback postMessageFn;
    void*               postMessageCtx;

    VSCodeWebviewOptions    options;

    VSCodeWebviewPanel() : id(0), viewColumn(1), active(false), visible(false),
                            hwnd(nullptr), onMessageFn(nullptr), onMessageCtx(nullptr),
                            postMessageFn(nullptr), postMessageCtx(nullptr) {}
};

// ============================================================================
// ColorTheme — Current editor color theme info
// ============================================================================
enum class ColorThemeKind : int {
    Light   = 1,
    Dark    = 2,
    HighContrast = 3,
    HighContrastLight = 4
};

struct VSCodeColorTheme {
    std::string     name;
    ColorThemeKind  kind;
};

// ============================================================================
// ProgressOptions — Progress notification options
// ============================================================================
enum class ProgressLocation : int {
    SourceControl   = 1,
    Window          = 10,
    Notification    = 15
};

struct VSCodeProgressOptions {
    ProgressLocation    location;
    std::string         title;
    bool                cancellable;

    VSCodeProgressOptions() : location(ProgressLocation::Notification), cancellable(false) {}
};

struct VSCodeProgress {
    typedef void (*ReportCallback)(int increment, const char* message, void* ctx);
    ReportCallback  reportFn;
    void*           reportCtx;

    void report(int increment, const char* message = nullptr) {
        if (reportFn) reportFn(increment, message, reportCtx);
    }

    VSCodeProgress() : reportFn(nullptr), reportCtx(nullptr) {}
};

// ============================================================================
// Configuration — Workspace/user settings access (mirrors vscode.WorkspaceConfiguration)
// ============================================================================
struct VSCodeConfiguration {
    std::string     section;            // e.g. "editor", "rawr.agent"

    // Get a value by key. Returns empty string if not found.
    typedef const char* (*GetStringCallback)(const char* key, void* ctx);
    typedef int         (*GetIntCallback)(const char* key, int defaultVal, void* ctx);
    typedef bool        (*GetBoolCallback)(const char* key, bool defaultVal, void* ctx);
    typedef VSCodeAPIResult (*UpdateCallback)(const char* key, const char* value,
                                              int target, void* ctx);  // target: 1=global, 2=workspace

    GetStringCallback   getStringFn;
    GetIntCallback      getIntFn;
    GetBoolCallback     getBoolFn;
    UpdateCallback      updateFn;
    void*               configCtx;

    VSCodeConfiguration() : getStringFn(nullptr), getIntFn(nullptr),
                             getBoolFn(nullptr), updateFn(nullptr), configCtx(nullptr) {}
};

// ============================================================================
// SCM (Source Control Manager) types
// ============================================================================
struct VSCodeSourceControlResourceState {
    VSCodeUri       resourceUri;
    std::string     decorations;    // Status letter (M, A, D, U, etc.)
    std::string     tooltip;
    std::string     contextValue;
};

struct VSCodeSourceControlResourceGroup {
    std::string     id;
    std::string     label;
    std::vector<VSCodeSourceControlResourceState> resourceStates;
    bool            hideWhenEmpty;

    VSCodeSourceControlResourceGroup() : hideWhenEmpty(true) {}
};

struct VSCodeSourceControlInputBox {
    std::string     value;
    std::string     placeholder;
};

struct VSCodeSourceControl {
    uint64_t        id;
    std::string     scmId;          // e.g. "git"
    std::string     label;
    VSCodeSourceControlInputBox inputBox;
    std::vector<VSCodeSourceControlResourceGroup> groups;
    int             count;          // Badge count

    typedef VSCodeAPIResult (*AcceptInputCallback)(const char* value, void* ctx);
    AcceptInputCallback acceptInputFn;
    void*               acceptInputCtx;

    VSCodeSourceControl() : id(0), count(0), acceptInputFn(nullptr), acceptInputCtx(nullptr) {}
};

// ============================================================================
// Extension Context — Passed to extension activate() (mirrors vscode.ExtensionContext)
// ============================================================================
struct VSCodeExtensionContext {
    std::string                 extensionId;
    std::string                 extensionPath;      // Filesystem root of extension
    std::string                 globalStoragePath;
    std::string                 workspaceStoragePath;
    std::string                 logPath;
    std::vector<Disposable>     subscriptions;      // Auto-disposed on deactivate

    // Memento (key-value persistence)
    typedef const char* (*MementoGetCallback)(const char* key, void* ctx);
    typedef VSCodeAPIResult (*MementoUpdateCallback)(const char* key, const char* value, void* ctx);

    MementoGetCallback      globalStateGet;
    MementoUpdateCallback   globalStateUpdate;
    MementoGetCallback      workspaceStateGet;
    MementoUpdateCallback   workspaceStateUpdate;
    void*                   mementoCtx;

    VSCodeExtensionContext() : globalStateGet(nullptr), globalStateUpdate(nullptr),
                               workspaceStateGet(nullptr), workspaceStateUpdate(nullptr),
                               mementoCtx(nullptr) {}
};

// ============================================================================
// Extension Manifest (package.json subset)
// ============================================================================
struct VSCodeExtensionManifest {
    std::string     id;                 // publisher.name
    std::string     name;
    std::string     displayName;
    std::string     version;
    std::string     publisher;
    std::string     description;
    std::string     extensionKind;      // "ui", "workspace", "web"
    std::vector<std::string> activationEvents;  // "onCommand:...", "onLanguage:...", etc.
    std::vector<std::string> categories;
    std::string     main;               // Entry point (JS or DLL path)

    struct ContributedCommand {
        std::string     command;
        std::string     title;
        std::string     category;
        std::string     icon;
    };
    std::vector<ContributedCommand> commands;

    struct ContributedConfiguration {
        std::string     title;
        std::unordered_map<std::string, std::string> properties;    // key → type
    };
    std::vector<ContributedConfiguration> configuration;

    struct ContributedView {
        std::string     id;
        std::string     name;
        std::string     when;           // When clause
    };
    std::unordered_map<std::string, std::vector<ContributedView>> views;    // container → views

    struct ContributedMenu {
        std::string     command;
        std::string     when;
        std::string     group;
    };
    std::unordered_map<std::string, std::vector<ContributedMenu>> menus;

    std::vector<std::string> extensionDependencies;
};

// ============================================================================
// Provider Registration IDs
// ============================================================================
enum class ProviderType : int {
    CompletionItem      = 0,
    Hover               = 1,
    Definition          = 2,
    References          = 3,
    DocumentSymbol      = 4,
    CodeAction          = 5,
    CodeLens            = 6,
    DocumentFormatting  = 7,
    RangeFormatting     = 8,
    Rename              = 9,
    SignatureHelp       = 10,
    FoldingRange        = 11,
    DocumentLink        = 12,
    ColorProvider       = 13,
    InlayHints          = 14,
    TypeDefinition      = 15,
    Implementation      = 16,
    Declaration         = 17,
    SemanticTokens      = 18,

    Count               = 19
};

// ============================================================================
// Provider Callbacks — Language feature providers
// ============================================================================

// CompletionItemProvider
struct VSCodeCompletionProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const CancellationToken* token,
                                     VSCodeCompletionItem* outItems, size_t maxItems,
                                     size_t* outCount, void* ctx);
    typedef bool (*ResolveCallback)(VSCodeCompletionItem* item, const CancellationToken* token,
                                     void* ctx);

    ProvideCallback     provideFn;
    ResolveCallback     resolveFn;
    void*               context;
    std::vector<char>   triggerCharacters;   // e.g. '.', ':', '<'
    std::string         languageId;          // "" = all languages

    VSCodeCompletionProvider() : provideFn(nullptr), resolveFn(nullptr), context(nullptr) {}
};

// HoverProvider
struct VSCodeHoverProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const CancellationToken* token,
                                     VSCodeHover* outHover, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeHoverProvider() : provideFn(nullptr), context(nullptr) {}
};

// DefinitionProvider
struct VSCodeDefinitionProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const CancellationToken* token,
                                     VSCodeLocation* outLocations, size_t maxLocs,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeDefinitionProvider() : provideFn(nullptr), context(nullptr) {}
};

// ReferenceProvider
struct VSCodeReferenceProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     bool includeDeclaration,
                                     const CancellationToken* token,
                                     VSCodeLocation* outLocations, size_t maxLocs,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeReferenceProvider() : provideFn(nullptr), context(nullptr) {}
};

// DocumentSymbolProvider
struct VSCodeDocumentSymbolProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc,
                                     const CancellationToken* token,
                                     VSCodeDocumentSymbol* outSymbols, size_t maxSymbols,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeDocumentSymbolProvider() : provideFn(nullptr), context(nullptr) {}
};

// CodeActionProvider
struct VSCodeCodeAction {
    std::string         title;
    std::string         kind;           // "quickfix", "refactor", etc.
    std::vector<VSCodeDiagnostic> diagnostics;
    VSCodeWorkspaceEdit edit;
    std::string         command;
    bool                isPreferred;

    VSCodeCodeAction() : isPreferred(false) {}
};

struct VSCodeCodeActionProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodeRange* range,
                                     const CancellationToken* token,
                                     VSCodeCodeAction* outActions, size_t maxActions,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;
    std::vector<std::string> providedCodeActionKinds;

    VSCodeCodeActionProvider() : provideFn(nullptr), context(nullptr) {}
};

// DocumentFormattingEditProvider
struct VSCodeFormattingProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc,
                                     int tabSize, bool insertSpaces,
                                     const CancellationToken* token,
                                     VSCodeTextEdit* outEdits, size_t maxEdits,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeFormattingProvider() : provideFn(nullptr), context(nullptr) {}
};

// FoldingRangeProvider
struct VSCodeFoldingRange {
    int         start;
    int         end;
    int         kind;   // 0=Comment, 1=Imports, 2=Region

    VSCodeFoldingRange() : start(0), end(0), kind(2) {}
};

struct VSCodeFoldingRangeProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc,
                                     const CancellationToken* token,
                                     VSCodeFoldingRange* outRanges, size_t maxRanges,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeFoldingRangeProvider() : provideFn(nullptr), context(nullptr) {}
};

// RenameProvider
struct VSCodeRenameProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const char* newName,
                                     const CancellationToken* token,
                                     VSCodeWorkspaceEdit* outEdit, void* ctx);
    typedef bool (*PrepareCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const CancellationToken* token,
                                     VSCodeRange* outRange, std::string* outPlaceholder,
                                     void* ctx);

    ProvideCallback     provideFn;
    PrepareCallback     prepareFn;
    void*               context;
    std::string         languageId;

    VSCodeRenameProvider() : provideFn(nullptr), prepareFn(nullptr), context(nullptr) {}
};

// SignatureHelpProvider
struct VSCodeSignatureInformation {
    std::string     label;
    std::string     documentation;
    struct ParameterInformation {
        std::string label;
        std::string documentation;
    };
    std::vector<ParameterInformation> parameters;
};

struct VSCodeSignatureHelp {
    std::vector<VSCodeSignatureInformation>  signatures;
    int                                      activeSignature;
    int                                      activeParameter;

    VSCodeSignatureHelp() : activeSignature(0), activeParameter(0) {}
};

struct VSCodeSignatureHelpProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodePosition* pos,
                                     const CancellationToken* token,
                                     VSCodeSignatureHelp* outHelp, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;
    std::vector<char>   triggerCharacters;
    std::vector<char>   retriggerCharacters;

    VSCodeSignatureHelpProvider() : provideFn(nullptr), context(nullptr) {}
};

// InlayHintsProvider
struct VSCodeInlayHint {
    VSCodePosition  position;
    std::string     label;
    int             kind;       // 0=other, 1=type, 2=parameter
    std::string     tooltip;
    bool            paddingLeft;
    bool            paddingRight;

    VSCodeInlayHint() : kind(0), paddingLeft(false), paddingRight(false) {}
};

struct VSCodeInlayHintsProvider {
    typedef bool (*ProvideCallback)(const VSCodeTextDocument* doc, const VSCodeRange* range,
                                     const CancellationToken* token,
                                     VSCodeInlayHint* outHints, size_t maxHints,
                                     size_t* outCount, void* ctx);

    ProvideCallback     provideFn;
    void*               context;
    std::string         languageId;

    VSCodeInlayHintsProvider() : provideFn(nullptr), context(nullptr) {}
};

// ============================================================================
// EventEmitter<T> — Ring-buffer event system (mirrors vscode.EventEmitter)
// ============================================================================
// No std::function in hot path — uses function pointers with context
template <typename T>
struct VSCodeEventListener {
    uint64_t    id;
    void        (*callback)(const T* event, void* ctx);
    void*       context;
    bool        active;
};

template <typename T, size_t MaxListeners = 64>
class VSCodeEventEmitter {
public:
    VSCodeEventEmitter() : m_nextId(1), m_listenerCount(0) {}

    Disposable on(void (*callback)(const T* event, void* ctx), void* ctx) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_listenerCount >= MaxListeners) {
            return { 0, nullptr, nullptr, true };   // Already disposed — overflow
        }
        uint64_t id = m_nextId++;
        m_listeners[m_listenerCount] = { id, callback, ctx, true };
        m_listenerCount++;

        Disposable d;
        d.id = id;
        d.disposeFn = [](void* ctx) {
            // Static remove — no capture (compliant with function pointer)
            // Caller must use Disposable.id to track
        };
        d.context = nullptr;
        d.disposed = false;
        return d;
    }

    void fire(const T& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_listenerCount; ++i) {
            if (m_listeners[i].active && m_listeners[i].callback) {
                m_listeners[i].callback(&event, m_listeners[i].context);
            }
        }
    }

    void removeListener(uint64_t id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_listenerCount; ++i) {
            if (m_listeners[i].id == id) {
                m_listeners[i].active = false;
                break;
            }
        }
    }

    size_t listenerCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        for (size_t i = 0; i < m_listenerCount; ++i) {
            if (m_listeners[i].active) count++;
        }
        return count;
    }

    void dispose() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listenerCount = 0;
    }

private:
    mutable std::mutex          m_mutex;
    VSCodeEventListener<T>      m_listeners[MaxListeners];
    size_t                      m_listenerCount;
    uint64_t                    m_nextId;
};

// ============================================================================
// NAMESPACE: vscode — Master API Surface
// ============================================================================
//
// This namespace mirrors the vscode module exported to extensions.
// Each sub-namespace corresponds to a VS Code API namespace.
//
namespace vscode {

    // ========================================================================
    // vscode::commands — Command registry and execution
    // ========================================================================
    namespace commands {

        // Register a command handler
        // Returns a Disposable that unregisters the command when disposed
        VSCodeAPIResult registerCommand(const char* commandId,
                                         void (*handler)(void* ctx), void* ctx,
                                         Disposable* outDisposable);

        // Register a text editor command (receives active editor)
        VSCodeAPIResult registerTextEditorCommand(const char* commandId,
                                                    void (*handler)(VSCodeTextEditor* editor, void* ctx),
                                                    void* ctx,
                                                    Disposable* outDisposable);

        // Execute a registered command
        VSCodeAPIResult executeCommand(const char* commandId, const char* argsJson = nullptr);

        // Execute a command with arguments (JSON-encoded)
        VSCodeAPIResult executeCommandWithArgs(const char* commandId,
                                                const char* argsJson);

        // Get all registered command IDs
        VSCodeAPIResult getCommands(bool filterInternal,
                                     char** outIds, size_t maxIds, size_t* outCount);
    }

    // ========================================================================
    // vscode::window — Window management, UI, and notifications
    // ========================================================================
    namespace window {

        // Active editor
        VSCodeTextEditor* activeTextEditor();

        // Visible editors
        void getVisibleTextEditors(VSCodeTextEditor** outEditors, size_t maxEditors,
                                    size_t* outCount);

        // Show information/warning/error messages
        VSCodeAPIResult showInformationMessage(const char* message,
                                                const VSCodeMessageItem* items, size_t itemCount,
                                                int* outSelectedIndex);
        VSCodeAPIResult showWarningMessage(const char* message,
                                            const VSCodeMessageItem* items, size_t itemCount,
                                            int* outSelectedIndex);
        VSCodeAPIResult showErrorMessage(const char* message,
                                          const VSCodeMessageItem* items, size_t itemCount,
                                          int* outSelectedIndex);

        // Quick pick
        VSCodeAPIResult showQuickPick(const VSCodeQuickPickItem* items, size_t itemCount,
                                       const char* placeHolder, bool canPickMany,
                                       int* outSelectedIndices, size_t maxSelected,
                                       size_t* outSelectedCount);

        // Input box
        VSCodeAPIResult showInputBox(const VSCodeInputBoxOptions* options,
                                      char* outValue, size_t maxLen);

        // Open dialog
        VSCodeAPIResult showOpenDialog(const char* title, bool canSelectMany,
                                        bool canSelectFiles, bool canSelectFolders,
                                        const char* defaultUri,
                                        VSCodeUri* outUris, size_t maxUris,
                                        size_t* outCount);

        // Save dialog
        VSCodeAPIResult showSaveDialog(const char* title, const char* defaultUri,
                                        VSCodeUri* outUri);

        // Status bar
        VSCodeStatusBarItem* createStatusBarItem(StatusBarAlignment alignment, int priority);
        void disposeStatusBarItem(VSCodeStatusBarItem* item);

        // Output channel
        VSCodeOutputChannel* createOutputChannel(const char* name);
        void disposeOutputChannel(VSCodeOutputChannel* channel);

        // Terminal
        VSCodeTerminal* createTerminal(const VSCodeTerminalOptions* options);
        void getActiveTerminal(VSCodeTerminal** outTerminal);
        void getTerminals(VSCodeTerminal** outTerminals, size_t maxTerminals, size_t* outCount);

        // Progress
        VSCodeAPIResult withProgress(const VSCodeProgressOptions* options,
                                      void (*task)(VSCodeProgress* progress,
                                                   CancellationToken* token, void* ctx),
                                      void* ctx);

        // Webview panel
        VSCodeWebviewPanel* createWebviewPanel(const char* viewType, const char* title,
                                                int viewColumn,
                                                const VSCodeWebviewOptions* options);
        void disposeWebviewPanel(VSCodeWebviewPanel* panel);

        // Tree view
        VSCodeAPIResult registerTreeDataProvider(const char* viewId,
                                                  VSCodeTreeDataProvider* provider);

        // Color theme
        VSCodeColorTheme getColorTheme();

        // Events
        extern VSCodeEventEmitter<VSCodeTextEditor>     onDidChangeActiveTextEditor;
        extern VSCodeEventEmitter<VSCodeTextDocument>    onDidChangeVisibleTextEditors;
        extern VSCodeEventEmitter<VSCodeColorTheme>      onDidChangeActiveColorTheme;
        extern VSCodeEventEmitter<VSCodeTerminal>        onDidOpenTerminal;
        extern VSCodeEventEmitter<VSCodeTerminal>        onDidCloseTerminal;
    }

    // ========================================================================
    // vscode::workspace — File system, configuration, workspace folders
    // ========================================================================
    namespace workspace {

        // Workspace folders
        void getWorkspaceFolders(VSCodeUri* outFolders, size_t maxFolders, size_t* outCount);
        VSCodeUri getWorkspaceFolder(const VSCodeUri* uri);
        const char* getName();       // Workspace name
        const char* getWorkspaceFile();  // .code-workspace path or empty

        // Configuration
        VSCodeConfiguration getConfiguration(const char* section);

        // File system watcher
        VSCodeFileSystemWatcher* createFileSystemWatcher(const char* globPattern,
                                                          bool ignoreCreate,
                                                          bool ignoreChange,
                                                          bool ignoreDelete);
        void disposeFileSystemWatcher(VSCodeFileSystemWatcher* watcher);

        // Text documents
        VSCodeAPIResult openTextDocument(const VSCodeUri* uri, VSCodeTextDocument* outDoc);
        VSCodeAPIResult openTextDocumentByPath(const char* filePath, VSCodeTextDocument* outDoc);
        void getTextDocuments(VSCodeTextDocument** outDocs, size_t maxDocs, size_t* outCount);

        // File operations
        VSCodeAPIResult applyEdit(const VSCodeWorkspaceEdit* edit);
        VSCodeAPIResult saveAll(bool includeUntitled);

        // Find files
        VSCodeAPIResult findFiles(const char* includePattern, const char* excludePattern,
                                   size_t maxResults,
                                   VSCodeUri* outUris, size_t maxUris, size_t* outCount);

        // Events
        extern VSCodeEventEmitter<VSCodeTextDocument>       onDidOpenTextDocument;
        extern VSCodeEventEmitter<VSCodeTextDocument>       onDidCloseTextDocument;
        extern VSCodeEventEmitter<VSCodeTextDocument>       onDidChangeTextDocument;
        extern VSCodeEventEmitter<VSCodeTextDocument>       onDidSaveTextDocument;
        extern VSCodeEventEmitter<VSCodeFileChangeEvent>    onDidChangeWorkspaceFolders;
        extern VSCodeEventEmitter<VSCodeConfiguration>      onDidChangeConfiguration;
    }

    // ========================================================================
    // vscode::languages — Language features (providers, diagnostics)
    // ========================================================================
    namespace languages {

        // Diagnostic collection
        DiagnosticCollection* createDiagnosticCollection(const char* name);
        void disposeDiagnosticCollection(DiagnosticCollection* collection);
        void getDiagnostics(const VSCodeUri* uri,
                             VSCodeDiagnostic* outDiags, size_t maxDiags, size_t* outCount);

        // Language registration
        VSCodeAPIResult registerCompletionItemProvider(const char* languageId,
                                                        VSCodeCompletionProvider* provider,
                                                        Disposable* outDisposable);
        VSCodeAPIResult registerHoverProvider(const char* languageId,
                                               VSCodeHoverProvider* provider,
                                               Disposable* outDisposable);
        VSCodeAPIResult registerDefinitionProvider(const char* languageId,
                                                     VSCodeDefinitionProvider* provider,
                                                     Disposable* outDisposable);
        VSCodeAPIResult registerReferenceProvider(const char* languageId,
                                                    VSCodeReferenceProvider* provider,
                                                    Disposable* outDisposable);
        VSCodeAPIResult registerDocumentSymbolProvider(const char* languageId,
                                                        VSCodeDocumentSymbolProvider* provider,
                                                        Disposable* outDisposable);
        VSCodeAPIResult registerCodeActionProvider(const char* languageId,
                                                     VSCodeCodeActionProvider* provider,
                                                     Disposable* outDisposable);
        VSCodeAPIResult registerDocumentFormattingEditProvider(const char* languageId,
                                                                VSCodeFormattingProvider* provider,
                                                                Disposable* outDisposable);
        VSCodeAPIResult registerFoldingRangeProvider(const char* languageId,
                                                       VSCodeFoldingRangeProvider* provider,
                                                       Disposable* outDisposable);
        VSCodeAPIResult registerRenameProvider(const char* languageId,
                                                VSCodeRenameProvider* provider,
                                                Disposable* outDisposable);
        VSCodeAPIResult registerSignatureHelpProvider(const char* languageId,
                                                       VSCodeSignatureHelpProvider* provider,
                                                       Disposable* outDisposable);
        VSCodeAPIResult registerInlayHintsProvider(const char* languageId,
                                                     VSCodeInlayHintsProvider* provider,
                                                     Disposable* outDisposable);

        // Set language for a document
        VSCodeAPIResult setTextDocumentLanguage(VSCodeTextDocument* doc, const char* languageId);

        // Get registered languages
        VSCodeAPIResult getLanguages(char** outLangs, size_t maxLangs, size_t* outCount);

        // Events
        extern VSCodeEventEmitter<VSCodeTextDocument>   onDidChangeDiagnostics;
    }

    // ========================================================================
    // vscode::debug — Debug sessions and breakpoints
    // ========================================================================
    namespace debug {

        // Start/stop debugging
        VSCodeAPIResult startDebugging(const VSCodeUri* folder,
                                        const VSCodeDebugConfiguration* config);
        VSCodeAPIResult stopDebugging();

        // Breakpoints
        VSCodeAPIResult addBreakpoints(const VSCodeBreakpoint* breakpoints, size_t count);
        VSCodeAPIResult removeBreakpoints(const uint64_t* breakpointIds, size_t count);
        void getBreakpoints(VSCodeBreakpoint* outBreakpoints, size_t maxBps, size_t* outCount);

        // Active debug session info
        const char* activeDebugSessionName();
        bool isDebugging();

        // Events
        extern VSCodeEventEmitter<VSCodeBreakpoint>     onDidChangeBreakpoints;
        extern VSCodeEventEmitter<int>                  onDidStartDebugSession;
        extern VSCodeEventEmitter<int>                  onDidTerminateDebugSession;
    }

    // ========================================================================
    // vscode::tasks — Task providers and execution
    // ========================================================================
    namespace tasks {

        // Register a task provider
        VSCodeAPIResult registerTaskProvider(const char* type,
                                              void (*provideTasks)(VSCodeTaskDefinition* outTasks,
                                                                   size_t maxTasks,
                                                                   size_t* outCount, void* ctx),
                                              void* ctx,
                                              Disposable* outDisposable);

        // Execute a task
        VSCodeAPIResult executeTask(const VSCodeTaskDefinition* task);

        // Fetch defined tasks
        void fetchTasks(VSCodeTaskDefinition* outTasks, size_t maxTasks, size_t* outCount);

        // Events
        extern VSCodeEventEmitter<VSCodeTaskDefinition> onDidStartTask;
        extern VSCodeEventEmitter<VSCodeTaskDefinition> onDidEndTask;
    }

    // ========================================================================
    // vscode::scm — Source Control Manager integration
    // ========================================================================
    namespace scm {

        // Create an SCM provider
        VSCodeSourceControl* createSourceControl(const char* id, const char* label,
                                                   const VSCodeUri* rootUri);
        void disposeSourceControl(VSCodeSourceControl* sc);

        // Active input box
        VSCodeSourceControlInputBox* getInputBox(VSCodeSourceControl* sc);
    }

    // ========================================================================
    // vscode::env — Environment information
    // ========================================================================
    namespace env {

        const char* appName();          // "RawrXD IDE"
        const char* appRoot();          // Installation directory
        const char* language();         // UI language (e.g. "en")
        const char* machineId();        // Anonymous machine identifier
        const char* sessionId();        // Current session ID
        const char* shell();            // Default shell path
        bool isTelemetryEnabled();
        const char* uriScheme();        // Custom URI scheme for this IDE

        // Clipboard
        VSCodeAPIResult clipboardReadText(char* outText, size_t maxLen, size_t* outLen);
        VSCodeAPIResult clipboardWriteText(const char* text);

        // External URI
        VSCodeAPIResult openExternal(const VSCodeUri* uri);
    }

    // ========================================================================
    // vscode::extensions — Extension registry
    // ========================================================================
    namespace extensions {

        // Query installed extensions
        void getAll(VSCodeExtensionManifest* outManifests, size_t maxExts, size_t* outCount);
        VSCodeExtensionManifest* getExtension(const char* extensionId);

        // Activate an extension (triggers its activate() entry point)
        VSCodeAPIResult activateExtension(const char* extensionId);

        // Events
        extern VSCodeEventEmitter<VSCodeExtensionManifest> onDidChange;
    }

    // ========================================================================
    // VSCodeExtensionAPI — Master singleton managing the compatibility layer
    // ========================================================================
    class VSCodeExtensionAPI {
    public:
        // Singleton access
        static VSCodeExtensionAPI& instance();

        // ---- Initialization ----
        // Must be called once at IDE startup with the host Win32IDE pointer.
        // Wires up all function pointer callbacks to real IDE infrastructure.
        VSCodeAPIResult initialize(Win32IDE* host, HWND mainWindow);
        VSCodeAPIResult shutdown();
        bool isInitialized() const;

        // ---- Extension Lifecycle ----
        // Load and activate an extension from its manifest
        VSCodeAPIResult loadExtension(const VSCodeExtensionManifest* manifest);
        VSCodeAPIResult activateExtension(const char* extensionId);
        VSCodeAPIResult deactivateExtension(const char* extensionId);
        VSCodeAPIResult unloadExtension(const char* extensionId);

        // ---- Native DLL Extension Entry Points ----
        // Native extensions export these functions:
        //   void rawr_ext_activate(VSCodeExtensionContext* ctx);
        //   void rawr_ext_deactivate();
        typedef void (*NativeActivateFunc)(VSCodeExtensionContext* ctx);
        typedef void (*NativeDeactivateFunc)();

        VSCodeAPIResult loadNativeExtension(const char* dllPath, const char* extensionId);

        // ---- Command Registry (routed to vscode::commands) ----
        VSCodeAPIResult registerCommand(const char* commandId,
                                         void (*handler)(void* ctx), void* ctx);
        VSCodeAPIResult executeCommand(const char* commandId, const char* argsJson = nullptr);
        const char* getCurrentCommandArgs() const;
        size_t getCommandCount() const;

        // ---- Provider Registry (routed to vscode::languages) ----
        VSCodeAPIResult registerProvider(ProviderType type, const char* languageId,
                                          void* provider);
        size_t getProviderCount(ProviderType type) const;

        // ---- Diagnostic Management ----
        DiagnosticCollection* createDiagnosticCollection(const char* name);
        void publishDiagnostics();

        // ---- Status Bar ----
        VSCodeStatusBarItem* createStatusBarItem(StatusBarAlignment alignment, int priority);
        void updateStatusBar();

        // ---- Output Channels ----
        VSCodeOutputChannel* createOutputChannel(const char* name);
        VSCodeOutputChannel* getOutputChannelById(uint64_t id);
        void flushOutputChannels();

        // ---- Tree Views ----
        VSCodeAPIResult registerTreeDataProvider(const char* viewId,
                                                  VSCodeTreeDataProvider* provider);
        void refreshTreeView(const char* viewId);

        // ---- File System Watchers ----
        VSCodeFileSystemWatcher* createFileSystemWatcher(const char* globPattern);
        void disposeFileSystemWatcher(VSCodeFileSystemWatcher* watcher);

        // ---- Webview Panels ----
        VSCodeWebviewPanel* createWebviewPanel(const char* viewType, const char* title,
                                                int viewColumn,
                                                const VSCodeWebviewOptions* options);

        // ---- Configuration Access ----
        VSCodeConfiguration getConfiguration(const char* section);

        // ---- Statistics ----
        struct APIStats {
            uint64_t    commandsRegistered;
            uint64_t    commandsExecuted;
            uint64_t    providersRegistered;
            uint64_t    diagnosticCollections;
            uint64_t    statusBarItems;
            uint64_t    outputChannels;
            uint64_t    treeProviders;
            uint64_t    fileWatchers;
            uint64_t    webviewPanels;
            uint64_t    extensionsLoaded;
            uint64_t    extensionsActive;
            uint64_t    nativeExtensions;
            uint64_t    totalAPICallCount;
            uint64_t    totalErrorCount;
        };

        APIStats getStats() const;
        void getStatusString(char* buf, size_t maxLen) const;

        // ---- JSON Serialization ----
        void serializeStatsToJson(char* outJson, size_t maxLen) const;

        // ---- Instrumentation (public for namespace-level free functions) ----
        void incrementAPICalls() const { m_totalAPICalls.fetch_add(1, std::memory_order_relaxed); }
        void incrementErrors() const { m_totalErrors.fetch_add(1, std::memory_order_relaxed); }

        // ---- Accessors for vscode:: namespace bridge functions ----
        Win32IDE* getHost() const { return m_host; }
        std::vector<std::unique_ptr<VSCodeTerminal>>& getTerminals() { return m_terminals; }
        std::mutex& getTerminalMutex() { return m_terminalMutex; }
        uint64_t allocTerminalId() { return m_nextTerminalId++; }

    private:
        VSCodeExtensionAPI();
        ~VSCodeExtensionAPI();
        VSCodeExtensionAPI(const VSCodeExtensionAPI&) = delete;
        VSCodeExtensionAPI& operator=(const VSCodeExtensionAPI&) = delete;

        // ---- Internal State ----
        Win32IDE*           m_host;
        HWND                m_mainWindow;
        bool                m_initialized;
        mutable std::mutex  m_mutex;

        // Command registry
        struct CommandEntry {
            std::string         commandId;
            void                (*handler)(void* ctx);
            void*               context;
            bool                isTextEditorCommand;
            void                (*editorHandler)(VSCodeTextEditor* editor, void* ctx);
            uint64_t            executionCount;
        };
        std::unordered_map<std::string, CommandEntry>   m_commands;
        mutable std::mutex                              m_commandsMutex;
        uint64_t                                        m_nextCommandId;

        // Provider registry (per-type, per-language)
        struct ProviderEntry {
            ProviderType        type;
            std::string         languageId;
            void*               provider;       // Typed pointer (cast per ProviderType)
            uint64_t            registrationId;
        };
        std::vector<ProviderEntry>                      m_providers;
        mutable std::mutex                              m_providersMutex;
        uint64_t                                        m_nextProviderId;

        // Diagnostic collections
        std::vector<std::unique_ptr<DiagnosticCollection>>  m_diagnosticCollections;
        mutable std::mutex                                  m_diagnosticsMutex;

        // Status bar items
        std::vector<std::unique_ptr<VSCodeStatusBarItem>>   m_statusBarItems;
        mutable std::mutex                                  m_statusBarMutex;
        uint64_t                                            m_nextStatusBarId;

        // Output channels
        std::vector<std::unique_ptr<VSCodeOutputChannel>>   m_outputChannels;
        mutable std::mutex                                  m_outputMutex;
        uint64_t                                            m_nextOutputId;

        // Tree data providers
        std::unordered_map<std::string, VSCodeTreeDataProvider*>    m_treeProviders;
        mutable std::mutex                                          m_treeMutex;

        // File system watchers
        std::vector<std::unique_ptr<VSCodeFileSystemWatcher>>   m_fileWatchers;
        mutable std::mutex                                      m_watcherMutex;
        uint64_t                                                m_nextWatcherId;

        // Webview panels
        std::vector<std::unique_ptr<VSCodeWebviewPanel>>    m_webviewPanels;
        mutable std::mutex                                  m_webviewMutex;
        uint64_t                                            m_nextWebviewId;

        // Loaded extensions
        struct LoadedExtension {
            VSCodeExtensionManifest     manifest;
            VSCodeExtensionContext      context;
            bool                        activated;
            bool                        isNative;
            HMODULE                     nativeModule;       // For DLL extensions
            NativeActivateFunc          activateFn;
            NativeDeactivateFunc        deactivateFn;
        };
        std::unordered_map<std::string, std::unique_ptr<LoadedExtension>>   m_extensions;
        mutable std::mutex                                                   m_extensionsMutex;

        // Cancellation token sources (pooled)
        std::vector<std::unique_ptr<CancellationTokenSource>>   m_tokenSources;
        mutable std::mutex                                      m_tokenMutex;
        uint64_t                                                m_nextTokenId;

        // Memento persistent state store (key→value, backed by JSON file)
        std::unordered_map<std::string, std::string>    m_mementoStore;
        mutable std::mutex                              m_mementoMutex;
        std::string                                     m_mementoFilePath;
        void loadMementoFromDisk();
        void saveMementoToDisk() const;

        // Terminal instances managed by the extension API
        std::vector<std::unique_ptr<VSCodeTerminal>>    m_terminals;
        mutable std::mutex                              m_terminalMutex;
        uint64_t                                        m_nextTerminalId{1};

        // API call statistics
        mutable std::atomic<uint64_t>   m_totalAPICalls;
        mutable std::atomic<uint64_t>   m_totalErrors;

        // ---- Internal Helpers ----

        // File system watcher thread
        static DWORD WINAPI fileWatcherThread(LPVOID param);

        // Bridge to Win32IDE infrastructure
        void bridgeStatusBarToWin32(const VSCodeStatusBarItem* item);
        void bridgeOutputToWin32(const VSCodeOutputChannel* channel);
        void bridgeTreeViewToWin32(const char* viewId, const VSCodeTreeDataProvider* provider);
        void bridgeDiagnosticsToLSP(const DiagnosticCollection* collection);
        void bridgeCommandToIDM(const char* commandId);
        void bridgeWebviewToWin32(VSCodeWebviewPanel* panel);

        // Configuration bridge
        static const char* configGetString(const char* key, void* ctx);
        static int configGetInt(const char* key, int defaultVal, void* ctx);
        static bool configGetBool(const char* key, bool defaultVal, void* ctx);
        static VSCodeAPIResult configUpdate(const char* key, const char* value,
                                             int target, void* ctx);

        // Memento bridge
        static const char* mementoGet(const char* key, void* ctx);
        static VSCodeAPIResult mementoUpdate(const char* key, const char* value, void* ctx);

        // Extension context factory
        VSCodeExtensionContext createContext(const VSCodeExtensionManifest* manifest);

        // Logging
        void logInfo(const char* fmt, ...) const;
        void logError(const char* fmt, ...) const;
        void logDebug(const char* fmt, ...) const;
    };

} // namespace vscode

// ============================================================================
// Command IDs for VS Code Extension API (Phase 29, 10000 range)
// Relocated from 9400 range to avoid PDB 9400-9500 conflict
// ============================================================================
#define IDM_VSCEXT_API_STATUS           10000
#define IDM_VSCEXT_API_RELOAD           10001
#define IDM_VSCEXT_API_LIST_COMMANDS    10002
#define IDM_VSCEXT_API_LIST_PROVIDERS   10003
#define IDM_VSCEXT_API_DIAGNOSTICS      10004
#define IDM_VSCEXT_API_EXTENSIONS       10005
#define IDM_VSCEXT_API_STATS            10006
#define IDM_VSCEXT_API_LOAD_NATIVE      10007
#define IDM_VSCEXT_API_DEACTIVATE_ALL   10008
#define IDM_VSCEXT_API_EXPORT_CONFIG    10009
