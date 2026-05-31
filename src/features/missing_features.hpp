// ============================================================================
// missing_features.hpp - RawrXD IDE Missing Features Implementation
// ============================================================================
// Zero external dependencies, pure C++17 standard library
// Features: Multi-cursor, Minimap, Breadcrumb, Regex Find, Snippets, Tab Groups,
//           Workspace Symbols, Code Actions, Visual Diff, Inline Chat, DAP, LSP, Terminal
//
// Copyright (c) 2026 RawrXD Sovereign IDE. All rights reserved.
// ============================================================================

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Windows-specific includes
#include <windows.h>

namespace rawrxd {

// ============================================================================
// SECTION 1: MULTI-CURSOR EDITOR SUPPORT
// ============================================================================

struct CursorPosition {
    int line = 0;
    int column = 0;
    int anchor_line = -1;
    int anchor_column = -1;
    
    bool hasSelection() const { return anchor_line >= 0; }
    void clearSelection() { anchor_line = -1; anchor_column = -1; }
    void setSelection(int l, int c) { anchor_line = l; anchor_column = c; }
    
    bool operator==(const CursorPosition& other) const {
        return line == other.line && column == other.column;
    }
    bool operator<(const CursorPosition& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

class MultiCursorManager {
public:
    std::vector<CursorPosition> cursors;
    size_t primaryCursorIndex = 0;
    
    void addCursor(int line, int column);
    void removeCursor(size_t index);
    void clearCursors();
    void setPrimaryCursor(size_t index);
    CursorPosition& primary();
    const CursorPosition& primary() const;
    void moveAll(int deltaLine, int deltaColumn, const std::vector<std::string>& lines);
    std::vector<std::pair<int, int>> insertAtAll(const std::string& text, std::vector<std::string>& lines);
    void deleteAtAll(std::vector<std::string>& lines, bool forward = true);
    
private:
    void updatePrimaryIndex();
    void removeDuplicates();
};

// ============================================================================
// SECTION 2: MINIMAP RENDERER
// ============================================================================

class MinimapRenderer {
public:
    int width = 100;
    int lineHeight = 2;
    int visibleLines = 50;
    int totalLines = 0;
    int scrollOffset = 0;
    
    struct MinimapLine {
        std::vector<std::pair<int, int>> tokens;
        uint32_t backgroundColor = 0x1e1e1e;
    };
    
    std::vector<MinimapLine> lines;
    
    void updateFromDocument(const std::vector<std::string>& docLines,
                            const std::vector<std::vector<std::tuple<int, int, uint32_t>>>& highlights);
    void render(std::vector<uint8_t>& pixels, int viewportStart, int viewportHeight);
    int clickToLine(int mouseY, int minimapHeight);
};

// ============================================================================
// SECTION 3: BREADCRUMB NAVIGATION
// ============================================================================

struct BreadcrumbItem {
    std::string label;
    std::string icon;
    std::string kind;
    int line = 0;
    int column = 0;
    std::vector<BreadcrumbItem> children;
};

class BreadcrumbProvider {
public:
    std::vector<BreadcrumbItem> items;
    
    void updateFromPath(const std::string& filePath);
    void addSymbolBreadcrumb(const std::string& name, const std::string& kind, int line, int column);
    std::string render() const;
};

// ============================================================================
// SECTION 4: REGEX FIND/REPLACE ENGINE
// ============================================================================

class RegexFindReplace {
public:
    struct SearchResult {
        int line;
        int start;
        int end;
        std::string text;
        std::vector<std::string> captures;
    };
    
    struct ReplaceResult {
        int matches;
        int replacements;
        std::vector<SearchResult> unmatched;
    };
    
    enum class SearchScope { CurrentFile, OpenFiles, Project, Selection };
    
    std::string pattern;
    std::string replace;
    bool caseSensitive = true;
    bool wholeWord = false;
    bool multiline = false;
    bool useRegex = true;
    SearchScope scope = SearchScope::CurrentFile;
    
    std::vector<SearchResult> searchResults;
    size_t currentResultIndex = 0;
    
    bool compile(std::string& error);
    std::vector<SearchResult> search(const std::vector<std::string>& lines);
    std::string replaceMatch(const SearchResult& result, const std::string& replacement);
    ReplaceResult replaceAll(std::vector<std::string>& lines);
    SearchResult* nextResult();
    SearchResult* prevResult();
    size_t resultCount() const;
};

// ============================================================================
// SECTION 5: SNIPPETS COMPLETION ENGINE
// ============================================================================

struct SnippetPlaceholder {
    int index;
    std::string defaultValue;
    std::vector<std::pair<int, int>> positions;
};

class Snippet {
public:
    std::string prefix;
    std::string name;
    std::string description;
    std::string body;
    std::vector<std::string> bodyLines;
    std::map<int, SnippetPlaceholder> placeholders;
    
    void parse(const std::string& snippetBody);
    std::vector<std::string> expand() const;
};

class SnippetManager {
public:
    std::map<std::string, std::vector<Snippet>> snippetsByLanguage;
    std::map<std::string, Snippet> snippetsByPrefix;
    
    void loadSnippets(const std::string& language, const std::map<std::string, std::string>& snippetDefs);
    std::vector<Snippet*> getCompletions(const std::string& prefix, const std::string& language);
    void loadDefaults();
};

// ============================================================================
// SECTION 6: TAB GROUPS & SPLIT LAYOUT
// ============================================================================

enum class SplitDirection { Horizontal, Vertical };

struct EditorTab {
    std::string id;
    std::string path;
    std::string name;
    bool isDirty = false;
    bool isPinned = false;
    int order = 0;
    std::string language;
    std::vector<std::string> content;
    CursorPosition cursor;
    std::vector<CursorPosition> multiCursors;
    int scrollX = 0;
    int scrollY = 0;
};

class TabGroup {
public:
    std::string id;
    std::vector<std::shared_ptr<EditorTab>> tabs;
    size_t activeTabIndex = 0;
    bool isPrimary = true;
    
    std::shared_ptr<EditorTab> addTab(const std::string& path, const std::string& name);
    void removeTab(const std::string& tabId);
    std::shared_ptr<EditorTab> activeTab();
    void activateTab(const std::string& tabId);
    void moveTab(size_t from, size_t to);
};

class LayoutNode {
public:
    std::string id;
    SplitDirection direction = SplitDirection::Horizontal;
    float splitRatio = 0.5f;
    std::shared_ptr<TabGroup> tabGroup;
    std::shared_ptr<LayoutNode> first;
    std::shared_ptr<LayoutNode> second;
    LayoutNode* parent = nullptr;
    
    bool isLeaf() const { return tabGroup != nullptr; }
    void split(SplitDirection dir, bool secondGroup = false);
};

class WorkspaceLayoutManager {
public:
    std::shared_ptr<LayoutNode> root;
    std::vector<std::shared_ptr<TabGroup>> allGroups;
    
    void init();
    TabGroup* primaryGroup();
    std::shared_ptr<TabGroup> addGroup(TabGroup* afterGroup, SplitDirection dir);
    void removeGroup(TabGroup* group);
    void setSplitRatio(LayoutNode* node, float ratio);
    
private:
    LayoutNode* findNode(LayoutNode* node, TabGroup* group);
};

// ============================================================================
// SECTION 7: WORKSPACE SYMBOLS SEARCH
// ============================================================================

struct SymbolInfo {
    std::string name;
    std::string kind;
    std::string container;
    std::string file;
    int line = 0;
    int column = 0;
    std::string signature;
    std::string documentation;
};

class WorkspaceSymbolsIndex {
public:
    std::vector<SymbolInfo> symbols;
    std::unordered_map<std::string, std::vector<size_t>> nameIndex;
    std::unordered_map<std::string, std::vector<size_t>> kindIndex;
    std::unordered_map<std::string, std::vector<size_t>> fileIndex;
    
    void addSymbol(const SymbolInfo& symbol);
    void clear() { symbols.clear(); nameIndex.clear(); kindIndex.clear(); fileIndex.clear(); }
    std::vector<SymbolInfo*> search(const std::string& query, int maxResults = 100);
    std::vector<SymbolInfo*> getByKind(const std::string& kind);
    std::vector<SymbolInfo*> getByFile(const std::string& file);
    
private:
    bool fuzzyMatch(const std::string& pattern, const std::string& text);
};

class SymbolParser {
public:
    void parseFile(const std::string& content, const std::string& filePath,
                   WorkspaceSymbolsIndex& index, const std::string& language);
    
private:
    void parseCppFile(const std::vector<std::string>& lines, const std::string& filePath, WorkspaceSymbolsIndex& index);
    void parsePythonFile(const std::vector<std::string>& lines, const std::string& filePath, WorkspaceSymbolsIndex& index);
    void parseJsFile(const std::vector<std::string>& lines, const std::string& filePath, WorkspaceSymbolsIndex& index);
    void parseRustFile(const std::vector<std::string>& lines, const std::string& filePath, WorkspaceSymbolsIndex& index);
    void parseGoFile(const std::vector<std::string>& lines, const std::string& filePath, WorkspaceSymbolsIndex& index);
};

// ============================================================================
// SECTION 8: CODE ACTIONS (QUICK FIXES)
// ============================================================================

struct CodeAction {
    std::string title;
    std::string kind;
    std::string command;
    std::vector<std::string> arguments;
    std::function<void(std::vector<std::string>&, int, int)> apply;
    bool isPreferred = false;
};

class CodeActionProvider {
public:
    std::vector<CodeAction> getActions(const std::vector<std::string>& lines, 
                                        int line, int column, const std::string& language);
    
private:
    std::vector<CodeAction> getCppActions(const std::vector<std::string>& lines, int line, int column);
    std::vector<CodeAction> getPythonActions(const std::vector<std::string>& lines, int line, int column);
    std::vector<CodeAction> getJsActions(const std::vector<std::string>& lines, int line, int column);
    std::vector<CodeAction> getGenericActions(const std::vector<std::string>& lines, int line, int column);
    bool hasSelection(const std::vector<std::string>& lines, int line, int column);
};

// ============================================================================
// SECTION 9: VISUAL DIFF ENGINE
// ============================================================================

enum class DiffOp { Equal, Insert, Delete };

struct DiffHunk {
    DiffOp op;
    int oldStart;
    int oldEnd;
    int newStart;
    int newEnd;
    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;
};

class DiffEngine {
public:
    std::vector<DiffHunk> computeDiff(const std::vector<std::string>& oldLines,
                                       const std::vector<std::string>& newLines);
    std::string generateUnifiedDiff(const std::vector<DiffHunk>& hunks,
                                      const std::string& oldPath,
                                      const std::string& newPath,
                                      int contextLines = 3);
    
private:
    std::vector<std::string> computeLCS(const std::vector<std::string>& a,
                                          const std::vector<std::string>& b);
    std::vector<DiffHunk> mergeHunks(std::vector<DiffHunk> hunks);
};

// ============================================================================
// SECTION 10: INLINE CHAT WIDGET
// ============================================================================

struct InlineChatMessage {
    std::string role;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::string model;
};

class InlineChatWidget {
public:
    int line = -1;
    int column = -1;
    int width = 400;
    int height = 200;
    bool visible = false;
    bool expanded = false;
    std::string input;
    std::vector<InlineChatMessage> messages;
    std::string selection;
    std::string contextCode;
    std::function<void(const std::string&)> onResponse;
    
    enum class Mode { Edit, Generate, Explain, Fix, Refactor };
    Mode mode = Mode::Edit;
    
    void show(int atLine, int atColumn, const std::string& selectedText = "");
    void hide();
    void sendMessage(const std::string& text);
    void acceptSuggestion(const std::string& suggestion);
    void rejectSuggestion();
    
private:
    void processWithAI();
};

// ============================================================================
// SECTION 11: DEBUG ADAPTER PROTOCOL (DAP) CLIENT
// ============================================================================

namespace dap {

struct Source {
    std::string name;
    std::string path;
    int sourceReference = 0;
};

struct Breakpoint {
    int id = 0;
    bool verified = false;
    std::string message;
    Source source;
    int line = 0;
    int column = 0;
};

struct StackFrame {
    int id = 0;
    std::string name;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    int instructionPointerReference = 0;
    std::string module;
    std::string presentationHint;
};

struct Variable {
    std::string name;
    std::string value;
    std::string type;
    int evaluateName = 0;
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;
};

struct Thread {
    int id = 0;
    std::string name;
};

struct Scope {
    std::string name;
    std::string presentationHint;
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;
    bool expensive = false;
    Source source;
    int line = 0;
    int column = 0;
};

class DebugSession {
public:
    std::string sessionId;
    bool initialized = false;
    bool stopped = false;
    std::string stopReason;
    
    std::vector<Thread> threads;
    std::vector<StackFrame> callStack;
    std::vector<Scope> scopes;
    std::vector<Variable> variables;
    std::vector<Breakpoint> breakpoints;
    
    std::function<void(const Breakpoint&)> onBreakpointChanged;
    std::function<void(int)> onThreadStarted;
    std::function<void(int)> onThreadExited;
    std::function<void()> onStopped;
    std::function<void()> onContinued;
    std::function<void()> onTerminated;
    std::function<void(const std::string&)> onOutput;
    
    void initialize();
    void launch(const std::string& program, const std::vector<std::string>& args = {});
    void attach(int pid);
    void disconnect(bool restart = false);
    void terminate();
    void restart();
    void setBreakpoints(const std::string& path, const std::vector<int>& lines);
    void addBreakpoint(const std::string& path, int line, int column = 0);
    void removeBreakpoint(int id);
    void continueExecution();
    void stepOver();
    void stepInto();
    void stepOut();
    void pause();
    void setCallStack(const std::vector<StackFrame>& frames);
    void setScopes(int frameId, const std::vector<Scope>& s);
    void setVariables(int variablesReference, const std::vector<Variable>& vars);
    std::string evaluate(const std::string& expression, int frameId = -1, const std::string& context = "watch");
    void setVariable(int variablesReference, const std::string& name, const std::string& value);
    void completions(const std::string& text, int line, int column);
    void disassemble(const std::string& memoryReference, int offset, int instructionOffset, int instructionCount);
    void readMemory(const std::string& memoryReference, int offset, int count);
};

class DebugManager {
public:
    std::vector<std::shared_ptr<DebugSession>> sessions;
    std::shared_ptr<DebugSession> activeSession;
    
    std::shared_ptr<DebugSession> createSession();
    void setActiveSession(const std::string& sessionId);
    void removeSession(const std::string& sessionId);
    void stopAll();
    std::vector<Breakpoint> getAllBreakpoints();
    void toggleBreakpoint(const std::string& path, int line);
};

} // namespace dap

// ============================================================================
// SECTION 12: LANGUAGE SERVER PROTOCOL (LSP) CLIENT
// ============================================================================

namespace lsp {

struct Position {
    int line = 0;
    int character = 0;
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range range;
};

struct Diagnostic {
    std::string severity;
    Range range;
    std::string message;
    std::string source;
    std::string code;
    std::vector<Location> relatedInformation;
};

struct CompletionItem {
    std::string label;
    std::string kind;
    std::string detail;
    std::string documentation;
    std::string sortText;
    std::string filterText;
    std::string insertText;
    std::string insertTextFormat;
    Range textEdit;
    std::vector<std::string> commitCharacters;
    bool deprecated = false;
    bool preselect = false;
};

struct Hover {
    std::string contents;
    Range range;
};

struct SignatureInformation {
    std::string label;
    std::string documentation;
    std::vector<std::string> parameters;
};

struct SignatureHelp {
    std::vector<SignatureInformation> signatures;
    int activeSignature = 0;
    int activeParameter = 0;
};

struct DocumentSymbol {
    std::string name;
    std::string detail;
    std::string kind;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<TextEdit>> changes;
};

struct ReferenceContext {
    bool includeDeclaration = true;
};

struct RenameResult {
    std::string newUri;
    WorkspaceEdit changes;
};

class LSPClient {
public:
    std::string serverPath;
    std::string languageId;
    bool initialized = false;
    
    bool supportsCompletion = true;
    bool supportsHover = true;
    bool supportsSignatureHelp = true;
    bool supportsDefinition = true;
    bool supportsReferences = true;
    bool supportsDiagnostics = true;
    bool supportsFormatting = true;
    bool supportsRename = true;
    
    std::function<void(const std::vector<Diagnostic>&)> onDiagnostics;
    std::function<void(const std::string&)> onLogMessage;
    std::function<void(const std::string&)> onShowMessage;
    
    std::unordered_map<std::string, int> documentVersions;
    
    void initialize(const std::string& serverExecutable, const std::vector<std::string>& args = {});
    void shutdown();
    void didOpen(const std::string& uri, const std::string& languageId, const std::string& text);
    void didChange(const std::string& uri, const std::vector<TextEdit>& changes);
    void didClose(const std::string& uri);
    void didSave(const std::string& uri, const std::string& text = "");
    std::vector<CompletionItem> completion(const std::string& uri, Position pos);
    Hover hover(const std::string& uri, Position pos);
    SignatureHelp signatureHelp(const std::string& uri, Position pos);
    std::vector<Location> gotoDefinition(const std::string& uri, Position pos);
    std::vector<Location> gotoDeclaration(const std::string& uri, Position pos);
    std::vector<Location> gotoTypeDefinition(const std::string& uri, Position pos);
    std::vector<Location> gotoImplementation(const std::string& uri, Position pos);
    std::vector<Location> findReferences(const std::string& uri, Position pos, bool includeDeclaration = true);
    std::vector<DocumentSymbol> documentSymbols(const std::string& uri);
    std::vector<Location> workspaceSymbols(const std::string& query);
    std::vector<TextEdit> formatting(const std::string& uri, int tabSize = 4, bool insertSpaces = true);
    std::vector<TextEdit> rangeFormatting(const std::string& uri, Range range, int tabSize = 4, bool insertSpaces = true);
    std::vector<TextEdit> onTypeFormatting(const std::string& uri, Position pos, char triggerChar, int tabSize = 4);
    RenameResult rename(const std::string& uri, Position pos, const std::string& newName);
    std::vector<CodeAction> codeActions(const std::string& uri, Range range, const std::vector<Diagnostic>& diagnostics);
    std::vector<std::string> foldingRanges(const std::string& uri);
    std::vector<Location> callHierarchy(const std::string& uri, Position pos);
    void executeCommand(const std::string& command, const std::vector<std::string>& args);
    void applyEdit(const WorkspaceEdit& edit);
    void notifyDiagnostics(const std::vector<Diagnostic>& diags);
    
private:
    std::vector<Diagnostic> diagnostics;
};

class LSPManager {
public:
    std::map<std::string, std::shared_ptr<LSPClient>> clients;
    
    void registerLanguage(const std::string& langId, const std::string& serverPath);
    LSPClient* getClient(const std::string& langId);
    void shutdownAll();
    void didOpen(const std::string& uri, const std::string& langId, const std::string& text);
    void didChange(const std::string& uri, const std::string& langId, const std::vector<TextEdit>& changes);
    std::vector<CompletionItem> completion(const std::string& uri, const std::string& langId, Position pos);
};

} // namespace lsp

// ============================================================================
// SECTION 13: TERMINAL INTEGRATION
// ============================================================================

struct TerminalProfile {
    std::string name;
    std::string shell;
    std::vector<std::string> args;
    std::string cwd;
    std::map<std::string, std::string> env;
    std::string icon;
};

struct TerminalInstance {
    int id = 0;
    std::string name;
    TerminalProfile profile;
    std::string cwd;
    std::vector<std::string> history;
    std::string buffer;
    int scrollPosition = 0;
    bool isRunning = false;
    int exitCode = -1;
    
    std::function<void(const std::string&)> onData;
    std::function<void(int)> onExit;
};

class TerminalManager {
public:
    std::vector<TerminalProfile> profiles;
    std::vector<std::shared_ptr<TerminalInstance>> terminals;
    int nextId = 1;
    
    void loadProfiles();
    std::shared_ptr<TerminalInstance> createTerminal(const std::string& profileName = "", const std::string& cwd = "");
    void closeTerminal(int id);
    void writeToTerminal(int id, const std::string& data);
    void resizeTerminal(int id, int cols, int rows);
    void sendCommand(int id, const std::string& cmd);
    std::shared_ptr<TerminalInstance> getTerminal(int id);
    void setTerminalCwd(int id, const std::string& cwd);
    void clearTerminal(int id);
    void splitTerminal(int id, bool horizontal = true);
};

// ============================================================================
// SECTION 14: MULTI-ROOT WORKSPACE
// ============================================================================

struct WorkspaceFolder {
    std::string uri;
    std::string name;
    std::string path;
    int index = 0;
};

class MultiRootWorkspace {
public:
    std::vector<WorkspaceFolder> folders;
    std::map<std::string, std::string> settings;
    std::string workspaceFile;
    
    void addFolder(const std::string& path, const std::string& name = "");
    void removeFolder(const std::string& path);
    void reorderFolder(const std::string& path, int newIndex);
    bool contains(const std::string& filePath);
    WorkspaceFolder* getContainingFolder(const std::string& filePath);
    void save(const std::string& path);
    bool load(const std::string& path);
    std::vector<std::string> getAllFiles(const std::vector<std::string>& extensions = {});
    std::string relativePath(const std::string& absolutePath);
};

// ============================================================================
// SECTION 15: BRANCH MANAGEMENT (GIT)
// ============================================================================

struct Branch {
    std::string name;
    std::string upstream;
    bool isRemote = false;
    bool isHead = false;
    std::string lastCommit;
    std::string lastCommitMessage;
    std::chrono::system_clock::time_point lastCommitTime;
};

struct Commit {
    std::string hash;
    std::string shortHash;
    std::string message;
    std::string author;
    std::string email;
    std::chrono::system_clock::time_point time;
    std::vector<std::string> parents;
};

struct FileStatus {
    std::string path;
    std::string oldPath;
    char x = ' ';
    char y = ' ';
};

class GitManager {
public:
    std::string repoPath;
    std::vector<Branch> branches;
    std::vector<Commit> commits;
    std::vector<FileStatus> staged;
    std::vector<FileStatus> unstaged;
    Branch currentBranch;
    std::string currentHead;
    bool initialized = false;
    
    std::function<void()> onStatusChanged;
    std::function<void(const std::string&, const std::string&)> onCommand;
    
    bool init(const std::string& path);
    void refresh();
    void refreshBranches();
    void refreshStatus();
    void refreshLog(int limit = 50);
    
    bool checkout(const std::string& branch, bool create = false);
    bool checkoutFile(const std::string& file);
    bool createBranch(const std::string& name, const std::string& from = "");
    bool deleteBranch(const std::string& name, bool force = false);
    bool renameBranch(const std::string& oldName, const std::string& newName);
    bool merge(const std::string& branch, const std::string& strategy = "");
    bool rebase(const std::string& onto);
    bool abortRebase();
    bool continueRebase();
    bool cherryPick(const std::string& commit);
    bool stash(const std::string& message = "");
    std::vector<std::string> stashList();
    bool stashPop(int index = 0);
    bool stashDrop(int index = 0);
    bool pull(const std::string& remote = "", const std::string& branch = "");
    bool push(const std::string& remote = "", const std::string& branch = "");
    bool fetch(const std::string& remote = "");
    bool add(const std::vector<std::string>& files);
    bool addAll();
    bool reset(const std::vector<std::string>& files);
    bool resetHard();
    bool commit(const std::string& message, const std::string& author = "");
    bool amend(const std::string& message = "");
    bool revert(const std::string& commit);
    std::string diff(const std::string& file = "", bool staged = false);
    std::string show(const std::string& commit);
    std::vector<std::string> remotes();
    bool addRemote(const std::string& name, const std::string& url);
    bool removeRemote(const std::string& name);
    std::string remoteUrl(const std::string& name);
    bool setRemoteUrl(const std::string& name, const std::string& url);
    std::vector<std::string> tags();
    bool createTag(const std::string& name, const std::string& commit = "");
    bool deleteTag(const std::string& name);
    std::string blame(const std::string& file, int line);
    std::vector<std::pair<int, std::string>> annotate(const std::string& file);
};

// ============================================================================
// SECTION 16: SETTINGS SYNC (LOCAL)
// ============================================================================

struct SettingsSyncState {
    std::string lastSyncTime;
    std::map<std::string, std::string> content;
    std::map<std::string, std::string> hashes;
};

class SettingsManager {
public:
    std::map<std::string, std::string> settings;
    std::map<std::string, std::string> keybindings;
    std::vector<std::string> recentFiles;
    std::vector<std::string> recentFolders;
    std::vector<std::string> recentSearches;
    std::map<std::string, std::string> snippets;
    
    std::string settingsPath;
    bool autoSync = true;
    
    void setDefaultPath(const std::string& path);
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& key, int defaultValue = 0);
    bool getBool(const std::string& key, bool defaultValue = false);
    void setKeybinding(const std::string& command, const std::string& key);
    std::string getKeybinding(const std::string& command);
    void addRecentFile(const std::string& path);
    void addRecentFolder(const std::string& path);
    void addRecentSearch(const std::string& query);
    bool save();
    bool load();
    void reset();
    void exportToFile(const std::string& path);
    void importFromFile(const std::string& path);
};

// ============================================================================
// SECTION 17: ICON THEMES
// ============================================================================

struct IconThemeIcon {
    std::string name;
    std::string path;
    std::vector<std::string> fileExtensions;
    std::vector<std::string> fileNames;
    std::string languageId;
};

class IconTheme {
public:
    std::string id;
    std::string name;
    std::string path;
    std::vector<IconThemeIcon> icons;
    std::map<std::string, std::string> extensionMap;
    std::map<std::string, std::string> nameMap;
    std::map<std::string, std::string> languageMap;
    std::string defaultIcon;
    std::string folderIcon;
    
    void buildMaps();
    std::string getIconForFile(const std::string& fileName, const std::string& languageId = "");
    std::string getIconForFolder(const std::string& folderName = "");
};

class IconThemeManager {
public:
    std::map<std::string, IconTheme> themes;
    IconTheme* activeTheme = nullptr;
    
    void loadTheme(const std::string& id, const std::string& name, const std::vector<IconThemeIcon>& icons);
    void setActiveTheme(const std::string& id);
    std::string getIconForFile(const std::string& fileName, const std::string& languageId = "");
    std::string getIconForFolder(const std::string& folderName = "");
    void loadDefaultThemes();
};

// ============================================================================
// SECTION 18: EXTENSION HOST STUB
// ============================================================================

struct Extension {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string publisher;
    bool enabled = true;
    bool installed = true;
    std::string path;
    std::map<std::string, std::string> contributes;
};

class ExtensionHost {
public:
    std::map<std::string, Extension> extensions;
    std::vector<std::function<void(const Extension&)>> onLoadCallbacks;
    std::vector<std::function<void(const Extension&)>> onUnloadCallbacks;
    std::map<std::string, std::function<void()>> commands;
    std::vector<std::pair<std::string, std::string>> statusBarItems;
    
    bool loadExtension(const std::string& path);
    void unloadExtension(const std::string& id);
    void enableExtension(const std::string& id);
    void disableExtension(const std::string& id);
    Extension* getExtension(const std::string& id);
    std::vector<Extension*> getAllExtensions();
    std::vector<Extension*> getEnabledExtensions();
    void onLoad(const std::function<void(const Extension&)>& callback);
    void onUnload(const std::function<void(const Extension&)>& callback);
    void registerCommand(const std::string& id, std::function<void()> handler);
    void executeCommand(const std::string& id);
    void addStatusBarItem(const std::string& id, const std::string& text);
    void removeStatusBarItem(const std::string& id);
};

// ============================================================================
// MAIN INTEGRATION CLASS
// ============================================================================

class IDEFeatures {
public:
    MultiCursorManager multiCursor;
    MinimapRenderer minimap;
    BreadcrumbProvider breadcrumb;
    RegexFindReplace findReplace;
    SnippetManager snippets;
    WorkspaceLayoutManager layout;
    WorkspaceSymbolsIndex symbolsIndex;
    SymbolParser symbolParser;
    CodeActionProvider codeActions;
    DiffEngine diffEngine;
    InlineChatWidget inlineChat;
    dap::DebugManager debugger;
    lsp::LSPManager lsp;
    TerminalManager terminals;
    MultiRootWorkspace workspace;
    GitManager git;
    SettingsManager settings;
    IconThemeManager icons;
    ExtensionHost extensions;
    
    void initialize();
    void loadWorkspace(const std::string& path);
    void indexWorkspace();
    void openFile(const std::string& path);
    std::string detectLanguage(const std::string& path);
    void saveSettings();
    void loadSettings();
};

} // namespace rawrxd
