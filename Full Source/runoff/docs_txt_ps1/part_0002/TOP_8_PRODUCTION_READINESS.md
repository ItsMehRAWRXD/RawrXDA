# 🚀 RawrXD IDE: Top 8 Production-Readiness Checklist
## Critical Path to VS Code/Cursor Parity (15-18 Weeks)

**Current State:** 30% complete — AI features ✅ | Core IDE features ❌  
**Goal:** Close the gap to enable professional daily use  
**Strategy:** Focus on **non-negotiable blockers** that prevent real-world adoption

---

## ⚡ The 8 Critical Gaps (Ordered by Impact)

### **#1: Full File CRUD & Project Explorer** 
**Status:** 🔴 CRITICAL BLOCKER  
**Phase:** 2 (Weeks 3-4)  
**Impact:** Without this, users cannot save work or manage multi-file projects  
**Effort:** 1,500 LOC | 38 hours

#### What's Missing:
- ❌ No way to create new files/folders from IDE
- ❌ No file deletion/rename operations
- ❌ No drag-and-drop file organization
- ❌ No `.gitignore` support (users see clutter)
- ❌ Project explorer shows stub data, not real filesystem

#### Implementation Checklist:
```cpp
// File: src/qtapp/widgets/project_explorer.cpp (replace stub)
class ProjectExplorer : public QWidget {
    ✓ QTreeWidget showing actual filesystem (lazy loading)
    ✓ Right-click context menu:
      - New File / New Folder
      - Rename / Delete  
      - Copy / Cut / Paste
      - Reveal in File Explorer
    ✓ Double-click to open file in editor
    ✓ Drag-and-drop rearrangement
    ✓ .gitignore parsing (gray out ignored files)
    ✓ Breadcrumb path at top
};

// File: src/qtapp/utils/file_operations.cpp (NEW)
class FileManager {
    bool createFile(QString path);
    bool deleteFile(QString path);  
    bool renameFile(QString oldPath, QString newPath);
    bool moveFile(QString from, QString to);
    bool createDirectory(QString path);
    QByteArray readFileSafe(QString path); // encoding detection
    bool writeFileAtomic(QString path, QByteArray data); // .tmp → rename
};

// File: src/qtapp/utils/project_detector.cpp (NEW)
enum ProjectType { Git, CMake, QMake, NodeJS, Python, Rust };
ProjectType detectProjectType(QString rootPath);
QString findProjectRoot(QString anyFilePath);
void saveProjectMetadata(QString path, ProjectMeta meta);
```

#### Acceptance Criteria:
- ✅ Can create `main.asm` from context menu
- ✅ Can delete/rename files safely (with confirmation)
- ✅ Can drag `helper.cpp` into `src/` folder
- ✅ Can open multi-file C++ project without confusion
- ✅ `.git/` folder is hidden by default

#### Dependencies:
- QFileSystemModel, QDir, QFileInfo
- Qt's MIME type detection for file icons

#### Risk:
🟡 **MEDIUM** — File operations need atomic writes + error recovery

---

### **#2: Find/Replace in Files**
**Status:** 🔴 CRITICAL BLOCKER  
**Phase:** 3 (Weeks 5-6)  
**Impact:** Code navigation impossible without project-wide search  
**Effort:** 800 LOC | 22 hours

#### What's Missing:
- ❌ No Ctrl+F search in current file
- ❌ No Ctrl+Shift+F search across project
- ❌ No find/replace functionality
- ❌ No regex support for complex searches

#### Implementation Checklist:
```cpp
// File: src/qtapp/widgets/find_widget.cpp (NEW)
class FindWidget : public QWidget {
    ✓ Search bar (Ctrl+F) with:
      - Match count: "3 of 15"
      - Case-sensitive toggle (Aa)
      - Whole-word toggle (ab|)
      - Regex toggle (.*)
      - Previous/Next buttons (↑↓)
    ✓ Replace bar (toggle visibility):
      - Replace current / Replace all
      - Support capture groups: \1, \2
      - Undo entire replace operation
    ✓ Search history (last 10 searches)
};

// File: src/qtapp/widgets/multi_file_search.cpp (NEW)
class MultiFileSearch : public QWidget {
    ✓ Ctrl+Shift+F panel showing:
      - Search input + file filter (*.cpp, *.h)
      - Results tree grouped by file
      - Line preview with highlighted match
    ✓ Async search (QThreadPool):
      - Respects .gitignore
      - Cancellable mid-search
      - Progress bar for large projects
    ✓ Click result → jump to file+line
    ✓ Export results to CSV/text
};

// Key Methods:
QList<SearchResult> findInFile(QString path, QRegularExpression pattern);
QList<SearchResult> findInProject(QString root, QString pattern, QStringList filters);
void highlightMatches(QPlainTextEdit* editor, QList<int> positions);
```

#### Acceptance Criteria:
- ✅ Ctrl+F finds "vulkan" in current file, highlights all 15 matches
- ✅ Ctrl+Shift+F finds "QByteArray" across entire Qt project (100+ files)
- ✅ Replace "old_function" with "new_function" in 42 files (single undo)
- ✅ Regex search: `#include <(.+)>` finds all includes
- ✅ Search completes in <2 seconds for 100K-line codebase

#### Dependencies:
- QRegularExpression, QThreadPool
- QTextDocument::find() for highlighting

#### Risk:
🟡 **MEDIUM** — Performance on large projects (need caching/indexing)

---

### **#3: Persistent Settings & Shortcuts UI**
**Status:** 🔴 CRITICAL BLOCKER  
**Phase:** 7 (Week 15)  
**Impact:** Ends "hardcoded" state, enables professional customization  
**Effort:** 600 LOC | 18 hours

#### What's Missing:
- ❌ All settings are hardcoded in C++ (no runtime config)
- ❌ Can't change font, theme, or tab size without recompiling
- ❌ No keyboard shortcut customization (users forced to use defaults)
- ❌ No workspace-specific settings

#### Implementation Checklist:
```cpp
// File: src/qtapp/widgets/settings_dialog.cpp (replace stub)
class SettingsDialog : public QDialog {
    ✓ Tabs:
      1. General:
         - Auto-save interval (0 = off, 1-60 seconds)
         - Startup behavior (restore last project / welcome screen)
      
      2. Appearance:
         - Theme: Dark, Light, Custom
         - Font family + size (monospace list)
         - Color scheme picker (syntax highlighting)
      
      3. Editor:
         - Tab size (2, 4, 8)
         - Insert spaces / Use tabs
         - Trim trailing whitespace
         - Format on save
      
      4. Keyboard:
         - Rebindable shortcuts table
         - Conflict detection
         - Import keymap (VS Code, Vim, Emacs presets)
      
      5. Extensions:
         - List installed plugins
         - Enable/disable toggles
};

// File: src/qtapp/utils/settings_manager.cpp (NEW)
class SettingsManager : public QObject {
    static SettingsManager& instance();
    
    // Storage: ~/.rawrxd/settings.json
    QVariant get(QString key, QVariant defaultValue);
    void set(QString key, QVariant value);
    void save(); // Write to disk
    void load(); // Read from disk
    void resetToDefaults();
    
    // Workspace overrides: .rawrxd/workspace.json
    void setWorkspace(QString key, QVariant value);
};

// File: src/qtapp/utils/shortcut_manager.cpp (NEW)
class ShortcutManager : public QObject {
    void registerShortcut(QString action, QKeySequence seq);
    void rebindShortcut(QString action, QKeySequence newSeq);
    bool hasConflict(QKeySequence seq);
    void importKeymap(QString preset); // "vscode", "vim", "emacs"
    void exportKeymap(QString path);
};
```

#### Acceptance Criteria:
- ✅ Change font to "Consolas 14pt", IDE updates immediately
- ✅ Switch theme from Dark to Light, all panels update
- ✅ Rebind Ctrl+S to Ctrl+Shift+S, no conflicts detected
- ✅ Enable "format on save", all files auto-format
- ✅ Settings persist across IDE restarts
- ✅ Import VS Code keybindings, all shortcuts work correctly

#### Dependencies:
- QSettings or QJsonDocument for storage
- QKeySequenceEdit for shortcut editor

#### Risk:
🟢 **LOW** — Standard Qt functionality, well-documented

---

### **#4: Basic Build/Task Runner**
**Status:** 🟠 HIGH PRIORITY  
**Phase:** 4 (Weeks 7-9)  
**Impact:** Users can compile/run code without switching to terminal  
**Effort:** 800 LOC | 23 hours (subset of Phase 4)

#### What's Missing:
- ❌ No integrated build system (users manually run NASM/GCC)
- ❌ No output panel showing compiler errors
- ❌ No task automation (e.g., "build + run")
- ❌ No error parsing (can't click error to jump to line)

#### Implementation Checklist:
```cpp
// File: src/qtapp/utils/task_runner.cpp (NEW - simplified Phase 4)
class TaskRunner : public QObject {
    Q_OBJECT
signals:
    void taskOutput(QString line, TaskOutputType type);
    void taskFinished(bool success, int exitCode);
    
public:
    // Execute from .rawrxd/tasks.json
    void executeTask(const Task& task);
    void cancelTask();
    
    // Built-in tasks for common scenarios
    Task createBuildTask(ProjectType type);
    Task createRunTask(QString executable);
};

struct Task {
    QString label;        // "Build Debug"
    QString command;      // "cmake --build build"
    QString workingDir;   // Project root
    QStringList args;     
    QString group;        // "build" | "run" | "test"
};

// File: src/qtapp/widgets/build_output_panel.cpp (NEW)
class BuildOutputPanel : public QPlainTextEdit {
    ✓ Color-coded output:
      - Errors: red
      - Warnings: yellow
      - Info: white
    ✓ Clickable error lines:
      - Parse: "file.cpp:123: error: ..."
      - Click → jump to file+line
    ✓ Clear/Export buttons
};

// File: src/qtapp/utils/compiler_output_parser.cpp (NEW)
struct CompilationError {
    QString file;
    int line, column;
    QString message;
    enum Type { Error, Warning, Info } type;
};

class CompilerOutputParser {
    QList<CompilationError> parse(QString output, CompilerType type);
};

enum CompilerType { MSVC, GCC, Clang, NASM, Link };
```

#### Minimal Feature Set (Focus on MASM first):
1. **NASM Build Task:**
   - Detect `.asm` files
   - Run: `nasm -f win64 main.asm -o main.obj`
   - Parse errors: `main.asm:42: error: invalid instruction`
   - Show in output panel with clickable links

2. **Generic Task Runner:**
   - Execute any shell command
   - Capture stdout/stderr
   - Show in real-time output panel

3. **Task Configuration:**
   - Create `.rawrxd/tasks.json`:
     ```json
     {
       "tasks": [
         {
           "label": "Build MASM",
           "command": "nasm",
           "args": ["-f", "win64", "${file}", "-o", "${fileDirname}/${fileBasenameNoExtension}.obj"]
         }
       ]
     }
     ```

#### Acceptance Criteria:
- ✅ Ctrl+Shift+B builds current MASM file, shows errors in panel
- ✅ Click error line in output → jumps to `main.asm:42`
- ✅ Can define custom tasks (e.g., "Run Python script")
- ✅ Output panel shows real-time compilation progress
- ✅ Build completes in <5 seconds for typical MASM project

#### Dependencies:
- QProcess for running external commands
- QRegularExpression for parsing compiler output

#### Risk:
🟡 **MEDIUM** — Parsing different compiler output formats is complex

---

### **#5: Core Code Editor Features**
**Status:** 🟠 HIGH PRIORITY  
**Phase:** 1 (Weeks 1-2)  
**Impact:** Makes editing experience productive and VS Code-like  
**Effort:** 800 LOC | 23 hours (subset of Phase 1)

#### What's Missing:
- ❌ No line numbers (users can't reference specific lines)
- ❌ No multi-cursor editing (slow for bulk changes)
- ❌ No code folding (large files are overwhelming)
- ❌ No minimap (hard to navigate long files)

#### Implementation Checklist (Focused Subset):
```cpp
// File: src/qtapp/widgets/advanced_editor.cpp (enhance existing)
class AdvancedTextEditor : public QPlainTextEdit {
    ✓ Line numbers:
      - Auto-width based on line count
      - Clickable to set breakpoints
      - Highlight current line number
    
    ✓ Multi-cursor:
      - Ctrl+Alt+Up/Down: add cursor above/below
      - Ctrl+D: select next occurrence
      - Ctrl+Shift+L: select all occurrences
      - Each cursor edits independently
    
    ✓ Code folding:
      - Detect { } blocks
      - Show ►/▼ icons in gutter
      - Click to collapse/expand
      - Show "... (N lines hidden)"
    
    ✓ Minimap (optional, can defer):
      - 10% width sidebar on right
      - Shows shrunken file overview
      - Click to jump to section
};

// Key Methods:
class LineNumberArea : public QWidget {
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override; // Breakpoints
};

class MultiCursorManager {
    QList<QTextCursor> cursors;
    void addCursorAbove();
    void addCursorBelow();
    void selectNextOccurrence();
    void typeAtAllCursors(QString text);
};

class FoldingManager {
    struct FoldRegion { int startLine, endLine; bool folded; };
    QList<FoldRegion> detectFoldingRegions();
    void foldRegion(int line);
    void unfoldRegion(int line);
};
```

#### Acceptance Criteria:
- ✅ Line numbers visible on left, update as file grows
- ✅ Ctrl+D selects next "vulkan", can edit 5 occurrences simultaneously
- ✅ Click ► to collapse 50-line function, see "... (50 lines hidden)"
- ✅ Minimap shows 500-line file overview (optional)
- ✅ Current line highlighted with subtle gray background

#### Dependencies:
- QPlainTextEdit's viewport painting
- QPainter for custom gutter rendering

#### Risk:
🟡 **MEDIUM** — Multi-cursor is complex to implement correctly

---

### **#6: Git Status/Commit UI**
**Status:** 🟠 HIGH PRIORITY  
**Phase:** 5 (Weeks 10-11)  
**Impact:** Professional teams need source control integration  
**Effort:** 450 LOC | 14 hours (subset of Phase 5)

#### What's Missing:
- ❌ No visibility into git status (modified/staged files)
- ❌ No ability to stage/commit from IDE
- ❌ No branch switching
- ❌ No diff viewer

#### Implementation Checklist (Minimal Git Support):
```cpp
// File: src/qtapp/utils/git_manager.cpp (NEW)
class GitManager : public QObject {
    Q_OBJECT
signals:
    void statusChanged(GitStatus status);
    
public:
    // Core operations (using libgit2 or QProcess)
    GitStatus getStatus(QString repoPath);
    bool stageFile(QString path);
    bool unstageFile(QString path);
    bool commit(QString message);
    bool push();
    bool pull();
    QStringList getBranches();
    bool switchBranch(QString name);
};

struct GitStatus {
    QString currentBranch;
    QStringList staged;
    QStringList modified;
    QStringList untracked;
    bool hasConflicts;
};

// File: src/qtapp/widgets/source_control_panel.cpp (NEW)
class SourceControlPanel : public QDockWidget {
    ✓ Show current branch + status
    ✓ File tree grouped by:
      - Staged Changes (checkboxes to unstage)
      - Modified Files (checkboxes to stage)
      - Untracked Files
    ✓ Commit message input
    ✓ Commit button (validates message not empty)
    ✓ Refresh button
};

// File: src/qtapp/widgets/simple_diff_viewer.cpp (NEW)
class SimpleDiffViewer : public QPlainTextEdit {
    ✓ Unified diff format:
      - Green lines: + additions
      - Red lines: - deletions
    ✓ Syntax highlighting preserved
    ✓ Side-by-side view (optional, can defer)
};
```

#### Minimal Feature Set (Focus on Core Workflow):
1. **Status Display:**
   - Show current branch in status bar: `[main]`
   - Show dirty indicator: ● if uncommitted changes

2. **Source Control Panel:**
   - List modified files
   - Checkboxes to stage/unstage
   - Commit message input
   - Commit button

3. **Basic Diff:**
   - Show unified diff for selected file
   - Syntax highlight diff content

#### Acceptance Criteria:
- ✅ Modify `main.cpp`, see it in "Modified Files" list
- ✅ Check box to stage, enter commit message, click Commit
- ✅ See green/red diff when selecting file
- ✅ Status bar shows `[main] ●` when dirty
- ✅ Can switch branches via dropdown menu

#### Dependencies:
- libgit2 (C library) OR QProcess + git CLI
- Qt's text rendering for diffs

#### Risk:
🟢 **LOW** — Basic git operations are well-documented

---

### **#7: Simple Debugger (Port Existing)**
**Status:** 🟠 HIGH PRIORITY  
**Phase:** 4 (Weeks 7-9)  
**Impact:** Critical for low-level MASM debugging  
**Effort:** 400 LOC | 12 hours (integration of existing code)

#### What's Missing:
- ❌ Win32 debugger exists but not integrated into Qt IDE
- ❌ No visual breakpoint markers in editor
- ❌ No watch expressions panel
- ❌ No call stack visualization

#### Implementation Checklist (Port Existing Debugger):
```cpp
// EXISTING: src/agent/debugger.cpp (Win32 API debugger)
// - Already supports breakpoints, stepping, memory inspection
// - Needs Qt UI wrapper

// File: src/qtapp/widgets/debugger_panel.cpp (NEW)
class DebuggerPanel : public QDockWidget {
    ✓ Toolbar:
      - Start/Stop debugging (F5)
      - Step Over (F10)
      - Step Into (F11)
      - Step Out (Shift+F11)
      - Continue (F5)
    
    ✓ Breakpoints panel:
      - List all breakpoints (file:line)
      - Enable/disable checkboxes
      - Delete button
    
    ✓ Watch expressions:
      - Text input to add variable
      - Tree view showing values
      - Auto-update on step
    
    ✓ Call stack:
      - Show function call chain
      - Click to jump to frame
    
    ✓ Locals panel:
      - Auto-show function parameters
      - Show local variables
};

// File: src/qtapp/widgets/advanced_editor.cpp (modifications)
class AdvancedTextEditor : public QPlainTextEdit {
    ✓ Breakpoint gutter:
      - Click line number to add/remove breakpoint
      - Show red dot for active breakpoints
      - Yellow arrow for current execution line
    
    ✓ Integration with debugger backend:
      - Send breakpoint add/remove events
      - Receive execution position updates
};

// Integration Layer:
class DebuggerBridge : public QObject {
    Q_OBJECT
signals:
    void breakpointHit(QString file, int line);
    void variableChanged(QString name, QString value);
    void callStackUpdated(QStringList frames);
    
public slots:
    void addBreakpoint(QString file, int line);
    void removeBreakpoint(QString file, int line);
    void stepOver();
    void stepInto();
    void continue_();
    void stop();
};
```

#### Minimal Feature Set (Leverage Existing Code):
1. **Breakpoint Management:**
   - Click gutter to add/remove
   - Visual markers (red dots)
   - Persist across sessions

2. **Stepping Controls:**
   - Step Over, Step Into, Step Out
   - Continue to next breakpoint
   - Stop debugging

3. **Basic Inspection:**
   - Show register values (RAX, RBX, etc.)
   - Show memory at address
   - Watch simple expressions

#### Acceptance Criteria:
- ✅ Click line 42 in `main.asm`, see red breakpoint dot
- ✅ Press F5, debugger stops at breakpoint
- ✅ Press F10, execution advances to next line (yellow arrow)
- ✅ See RAX register value update in panel: `0x00000042`
- ✅ Add watch expression `[rbp+8]`, see stack value

#### Dependencies:
- Existing Win32 debugger code (`src/agent/debugger.cpp`)
- Qt signal/slot for UI updates

#### Risk:
🟡 **MEDIUM** — Integration complexity (Win32 API ↔ Qt event loop)

---

### **#8: Language Service/Autocomplete (MASM + 1 Other)**
**Status:** 🟡 MEDIUM PRIORITY  
**Phase:** 6 (Weeks 12-14)  
**Impact:** Improves productivity, but not a hard blocker  
**Effort:** 550 LOC | 16 hours (subset of Phase 6)

#### What's Missing:
- ❌ No autocomplete (must manually type all instructions/registers)
- ❌ No hover tooltips (no quick reference for opcodes)
- ❌ No go-to-definition (can't jump to label definitions)

#### Implementation Checklist (Focused on MASM):
```cpp
// File: src/qtapp/widgets/masm_autocomplete.cpp (NEW)
class MASMAutocompleteProvider : public QObject {
    ✓ Trigger conditions:
      - After typing 3+ characters
      - After typing space (for opcodes)
    
    ✓ Suggestions:
      - Instructions: mov, add, sub, push, pop, call, ret (x86-64 set)
      - Registers: rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15
      - Directives: db, dw, dd, dq, section, global, extern
      - Labels: scan current file for "label_name:"
    
    ✓ Completion popup:
      - QCompleter-based dropdown
      - Filter by prefix as user types
      - Show instruction description in tooltip
    
    ✓ Snippet support:
      - Type "proc" → expand to:
        ```asm
        procedure_name:
            push rbp
            mov rbp, rsp
            
            ; body
            
            mov rsp, rbp
            pop rbp
            ret
        ```
};

// File: src/qtapp/widgets/symbol_navigator.cpp (NEW)
class SymbolNavigator : public QObject {
    ✓ Parse current file for symbols:
      - Labels: "main:", "loop_start:", etc.
      - Procedures: detect standard prologue/epilogue
    
    ✓ Ctrl+Click or F12:
      - Find label definition
      - Jump to line
    
    ✓ Navigation history:
      - Ctrl+Alt+Left: back
      - Ctrl+Alt+Right: forward
};

// File: src/qtapp/widgets/hover_provider.cpp (NEW)
class HoverProvider : public QObject {
    ✓ On mouse hover (500ms delay):
      - Show tooltip for instruction:
        "MOV: Copies data from source to destination"
      - Show register size/purpose:
        "RAX: 64-bit accumulator register"
      - Show label definition:
        "main: (line 15)"
};
```

#### Minimal Feature Set (MASM Only):
1. **Instruction Autocomplete:**
   - Type "mo" → suggest "mov", "movzx", "movsx"
   - Type "r" → suggest all registers

2. **Label Navigation:**
   - Ctrl+Click on `call my_func` → jump to `my_func:` definition

3. **Hover Tooltips:**
   - Hover over "add" → show "ADD: Adds source to destination"

#### Acceptance Criteria:
- ✅ Type "mo", see autocomplete dropdown with "mov", "movzx"
- ✅ Select "mov", press Tab, autocomplete inserts "mov "
- ✅ Ctrl+Click on `call helper` → jump to `helper:` definition
- ✅ Hover over "rax" → see "64-bit accumulator register"
- ✅ Type "proc", expand to full procedure template

#### Dependencies:
- QCompleter for popup
- QToolTip for hover
- Regex for label detection

#### Risk:
🟢 **LOW** — MASM instruction set is fixed and well-documented

---

## 📊 Implementation Priority Matrix

| Feature | Impact | Effort | Priority | Weeks |
|---------|--------|--------|----------|-------|
| #1: File CRUD & Explorer | 🔴 CRITICAL | 38h | **DO FIRST** | 3-4 |
| #2: Find/Replace | 🔴 CRITICAL | 22h | **DO FIRST** | 5-6 |
| #3: Settings UI | 🔴 CRITICAL | 18h | **DO EARLY** | 15 |
| #4: Build/Task Runner | 🟠 HIGH | 23h | **DO SECOND** | 7-9 |
| #5: Editor Features | 🟠 HIGH | 23h | **DO FIRST** | 1-2 |
| #6: Git UI | 🟠 HIGH | 14h | **DO SECOND** | 10-11 |
| #7: Debugger Port | 🟠 HIGH | 12h | **DO SECOND** | 7-9 |
| #8: Autocomplete | 🟡 MEDIUM | 16h | **DO LAST** | 12-14 |
| **TOTAL** | — | **166h** | **~21 days** | **15 weeks** |

---

## 🎯 Suggested Implementation Order

### **Sprint 1 (Weeks 1-2): Make Editing Usable**
- ✅ #5: Line numbers, multi-cursor, code folding
- **Result:** Editor feels professional, not a toy

### **Sprint 2 (Weeks 3-4): Make File Management Work**
- ✅ #1: Full file CRUD + project explorer
- **Result:** Users can actually manage projects

### **Sprint 3 (Weeks 5-6): Make Navigation Possible**
- ✅ #2: Find/replace in files
- **Result:** Users can find code without external tools

### **Sprint 4 (Weeks 7-9): Make Building Work**
- ✅ #4: Build/task runner (MASM focus)
- ✅ #7: Integrate existing debugger
- **Result:** Users can compile + debug MASM code

### **Sprint 5 (Weeks 10-11): Add Source Control**
- ✅ #6: Git status/commit UI
- **Result:** Teams can collaborate

### **Sprint 6 (Weeks 12-14): Add Intelligence**
- ✅ #8: MASM autocomplete + navigation
- **Result:** Productivity boost

### **Sprint 7 (Week 15): Polish & Settings**
- ✅ #3: Settings UI + shortcuts
- ✅ Bug fixes, testing, documentation
- **Result:** 100% production-ready

---

## 🚨 Current Showstoppers (Must Fix First)

1. **File CRUD (#1):**
   - Without this, users lose work or resort to external file managers
   - **Blocks:** All professional use cases

2. **Find/Replace (#2):**
   - Without this, navigating >1000 line projects is impossible
   - **Blocks:** Medium/large project work

3. **Settings (#3):**
   - Without this, IDE feels like a prototype (hardcoded values)
   - **Blocks:** User customization and adoption

**All other features can wait until these 3 are fixed.**

---

## 💡 Quick Wins (Low Effort, High Impact)

1. **Line Numbers (#5):** ~8 hours, makes IDE instantly more credible
2. **Git Status Bar (#6):** ~4 hours, shows current branch (huge UX improvement)
3. **Hover Tooltips (#8):** ~4 hours, instant documentation lookup

**Total:** ~16 hours for 3 visible improvements

---

## 🔧 Technical Debt to Address

1. **Replace All Stubs:**
   - `ProjectExplorerWidget` (currently fake data)
   - `SettingsDialog` (currently empty)
   - `OutputPanel` (needs real build output)

2. **Add Missing Infrastructure:**
   - Settings persistence (JSON-based)
   - Task execution framework (QProcess wrapper)
   - Symbol indexing (for autocomplete/navigation)

3. **Performance Optimization:**
   - Large file handling (>10K lines)
   - Multi-file search (needs indexing)
   - Syntax highlighting (currently re-parses entire file)

---

## 📈 Success Metrics

After completing all 8 items, RawrXD should:

- ✅ Open and edit multi-file MASM projects without friction
- ✅ Find/replace across 100+ files in <2 seconds
- ✅ Build MASM code with one keypress (Ctrl+Shift+B)
- ✅ Debug with breakpoints, stepping, and watches
- ✅ Commit code changes to Git without leaving IDE
- ✅ Customize keybindings, fonts, and themes via UI
- ✅ Autocomplete MASM instructions and navigate to labels
- ✅ Handle 10K+ line files without lag

**If all 8 are complete, RawrXD is a viable VS Code alternative for MASM development.**

---

## 🎓 Learning Resources

### Qt Documentation:
- QFileSystemModel: https://doc.qt.io/qt-6/qfilesystemmodel.html
- QPlainTextEdit: https://doc.qt.io/qt-6/qplaintextedit.html
- QProcess: https://doc.qt.io/qt-6/qprocess.html
- QCompleter: https://doc.qt.io/qt-6/qcompleter.html

### Implementation Examples:
- Qt Creator source code (similar IDE, Qt-based)
- KDE Kate editor (advanced QPlainTextEdit usage)
- Visual Studio Code extension API (feature parity reference)

### Git Integration:
- libgit2: https://libgit2.org/
- QGit (Qt-based Git client, open source)

---

## ✅ Acceptance Test Scenarios

### **Scenario 1: New MASM Project**
1. Open RawrXD
2. File → New Project → MASM
3. Create `main.asm`, `helper.asm`, `data.asm`
4. Write code with autocomplete
5. Ctrl+Shift+F to find all uses of "my_label"
6. Ctrl+Shift+B to build
7. F5 to debug with breakpoints
8. Modify code, stage changes, commit to Git
9. Close IDE, reopen → all settings/files restored

**Result:** Complete workflow without leaving RawrXD ✅

### **Scenario 2: Import Existing C++ Project**
1. Open folder with 50+ `.cpp/.h` files
2. Project type auto-detected as CMake
3. Ctrl+Shift+F finds all includes
4. Build with CMake task
5. Navigate to symbol definition with Ctrl+Click
6. Change theme to Light, font to Consolas 12pt
7. Settings persist across restart

**Result:** Professional C++ development workflow ✅

---

## 🏁 Definition of Done

Each of the 8 features is "done" when:

- ✅ Code is written and compiles without warnings
- ✅ Unit tests pass (where applicable)
- ✅ Manual testing confirms acceptance criteria
- ✅ Performance targets met (no lag on typical projects)
- ✅ Error handling implemented (no crashes on invalid input)
- ✅ Documentation updated (user guide + developer docs)
- ✅ Code reviewed by second developer (if team size = 2)

**IDE is "production-ready" when all 8 features are done.**

---

## 🚀 Next Steps

1. **Week 1:** Start with #5 (Line numbers, multi-cursor, code folding)
2. **Week 3:** Implement #1 (File CRUD + project explorer)
3. **Week 5:** Add #2 (Find/replace)
4. **Week 7:** Build #4 (Task runner) + #7 (Debugger)
5. **Week 10:** Integrate #6 (Git UI)
6. **Week 12:** Enhance with #8 (Autocomplete)
7. **Week 15:** Polish with #3 (Settings) + testing

**Timeline:** 15 weeks to 100% production-ready ✅

---

**Questions? Start with the "Quick Wins" section for immediate visible progress!**
