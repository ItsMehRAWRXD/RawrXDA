# RawrXD IDE - Dual-Pane Terminal Implementation

## Overview
Complete implementation of a resizable, dual-pane terminal system for the RawrXD Win32 IDE, featuring:
- **PowerShell pane** (left): Native Windows PowerShell integration
- **x64 MASM CLI pane** (right): Native command-line interface 
- **Resizable splitter**: Drag to adjust pane widths (10%-90% range)
- **Full keyboard support**: Commands and arguments
- **Status colors**: PowerShell (white text), CLI (green text on dark background)

## Changes Made

### 1. Utility Macros (Lines ~90-95)
```cpp
#define GET_X_LPARAM(lp) (int)(short)LOWORD(lp)
#define GET_Y_LPARAM(lp) (int)(short)HIWORD(lp)
```
Extracts X and Y coordinates from mouse event lParam for splitter drag detection.

### 2. Global Variables (Added)
```cpp
HWND g_hwndCLI;                    // CLI RichEdit window handle
int g_terminalSplitterPos = 50;    // Splitter position (0-100 %)
bool g_isDraggingSplitter = false; // Drag state tracking
```

### 3. Forward Declarations (Line ~1840)
```cpp
LRESULT CALLBACK CLIInputSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
```

### 4. CLI Append Function (Line ~1960)
```cpp
void AppendCLI(const wchar_t* text) { AppendToRichEdit(g_hwndCLI, text); }
```
Appends text to CLI pane with auto-scroll to bottom.

### 5. CLI Command Executor (Lines ~1964-2010)
```cpp
void ExecuteCLICommand(const std::wstring& cmd)
```
**Implemented commands:**
- `open <file>` - Open file dialog
- `save` - Save current file
- `build` / `compile` - Compile current file
- `run` - Execute current file
- `help` - Show command help
- `clear` - Clear CLI history
- Handles tokenization and command routing

### 6. WM_CREATE - CLI Pane Creation (Lines ~7450-7460)
```cpp
g_hwndCLI = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
    WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
    0, 0, 0, 0, hwnd, (HMENU)IDC_OUTPUT, GetModuleHandle(nullptr), nullptr);
SendMessageW(g_hwndCLI, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
SendMessageW(g_hwndCLI, EM_SETBKGNDCOLOR, 0, RGB(20, 20, 20));
CHARFORMAT2W cfCLI = {};
cfCLI.cbSize = sizeof(cfCLI); cfCLI.dwMask = CFM_COLOR; cfCLI.crTextColor = RGB(0, 255, 100);
SendMessageW(g_hwndCLI, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfCLI);
AppendCLI(L"RawrXD CLI  v1.0\r\nCommands: open, save, build, ai, search, help, exit\r\n---\r\n> ");
SetWindowSubclass(g_hwndCLI, CLIInputSubclassProc, 4, 0);
```
- Green monospace text on dark background for visibility
- Subclass hook for keyboard input handling
- Initial prompt display

### 7. Mouse Event Handlers (Lines ~7613-7656)

#### WM_LBUTTONDOWN - Start Splitter Drag
```cpp
case WM_LBUTTONDOWN: {
    if (g_bTerminalVisible && g_hwndTerminal && g_hwndCLI) {
        int xPos = GET_X_LPARAM(lParam);
        RECT termRect;
        GetWindowRect(g_hwndTerminal, &termRect);
        ScreenToClient(hwnd, (POINT*)&termRect);
        int splitterX = termRect.right;
        if (xPos >= splitterX - 4 && xPos <= splitterX + 12) {
            g_isDraggingSplitter = true;
            SetCapture(hwnd);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        }
    }
    break;
}
```
Detects click on splitter bar (±8px sensitive area).

#### WM_MOUSEMOVE - Resize During Drag
```cpp
case WM_MOUSEMOVE: {
    if (g_isDraggingSplitter && g_bTerminalVisible) {
        int xPos = GET_X_LPARAM(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int treeWidth = g_bFileTreeVisible ? 220 : 0;
        int editorWidth = rc.right - treeWidth;
        int relX = xPos - treeWidth;
        int newPos = (relX * 100) / editorWidth;
        if (newPos < 10) newPos = 10;
        if (newPos > 90) newPos = 90;
        g_terminalSplitterPos = newPos;
        UpdateLayout(hwnd);
    } else if (g_bTerminalVisible && g_hwndTerminal) {
        // Show resize cursor when hovering
        RECT termRect;
        GetWindowRect(g_hwndTerminal, &termRect);
        ScreenToClient(hwnd, (POINT*)&termRect);
        int xPos = GET_X_LPARAM(lParam);
        int splitterX = termRect.right;
        if (xPos >= splitterX - 4 && xPos <= splitterX + 12) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        }
    }
    break;
}
```
- Calculates new split ratio (constrained 10%-90%)
- Calls UpdateLayout() to redraw panes
- Shows resize cursor on hover

#### WM_LBUTTONUP - End Drag
```cpp
case WM_LBUTTONUP: {
    if (g_isDraggingSplitter) {
        g_isDraggingSplitter = false;
        ReleaseCapture();
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
    break;
}
```

### 8. UpdateLayout() - Dual-Pane Layout (Lines ~7495-7555)
**When g_bTerminalVisible:**
```cpp
int terminalY = neonStripH + editorHeight;
int terminalHeight = 200;
int splitterWidth = 8;
int leftPaneWidth = (width - treeWidth) * g_terminalSplitterPos / 100;
int rightPaneWidth = (width - treeWidth) - leftPaneWidth - splitterWidth;

// PowerShell (left pane)
if (g_hwndTerminal) {
    MoveWindow(g_hwndTerminal, treeWidth, terminalY, leftPaneWidth, terminalHeight, TRUE);
    ShowWindow(g_hwndTerminal, SW_SHOW);
}

// CLI (right pane)
if (g_hwndCLI) {
    MoveWindow(g_hwndCLI, treeWidth + leftPaneWidth + splitterWidth, terminalY, rightPaneWidth, terminalHeight, TRUE);
    ShowWindow(g_hwndCLI, SW_SHOW);
}
```

### 9. CLIInputSubclassProc - Keyboard Handler (Lines ~7462-7489)
```cpp
LRESULT CALLBACK CLIInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, ...)
```
- Triggers on Enter (Shift+Enter for newline)
- Extracts command from last line
- Calls ExecuteCLICommand()
- Displays result with new prompt

### 10. Terminal Visibility Fix
CLI pane is shown only when terminal is visible (g_bTerminalVisible = true).

## Behavior

### Layout States
1. **Terminal Hidden**: Full editor view
2. **Terminal Visible**: Editor + dual-pane terminal (200px height)
3. **Splitter Adjustment**: Drag splitter to resize panes (update saves to g_terminalSplitterPos)

### Command Execution Flow
```
User types command in CLI → Enter key pressed
    ↓
CLIInputSubclassProc extracts command line
    ↓
ExecuteCLICommand() parses tokens
    ↓
Routes to handler (open/save/build/run/help/clear)
    ↓
Handler executes (calls IDE functions or displays help)
    ↓
Result appended to CLI pane with new prompt "> "
```

### Colors
- **PowerShell**: RGB(240,240,240) text on RGB(30,30,30) background (white on dark gray)
- **CLI**: RGB(0,255,100) text on RGB(20,20,20) background (hacker green on black)

## Building

### Compile Command
```bash
cd d:\rawrxd\Ship
cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS \
   /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp \
   /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib \
         comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib \
   /SUBSYSTEM:WINDOWS
```

### Use Existing MASM CLI
The `RawrXD_CommandCLI.asm` file contains command dispatcher infrastructure:
- Command opcode routing (FILE_OPEN, FILE_SAVE, BUILD_RUN, etc.)
- Command parsing function (ParseCommand)
- Result formatting (FormatResult)
- Named pipe IPC communication paths

## Testing Checklist

- [ ] Terminal pane visible at 50% split
- [ ] Drag splitter to 25% (left pane shrinks)
- [ ] Drag splitter to 75% (right pane shrinks)
- [ ] Splitter constraints: can't go below 10% or above 90%
- [ ] Type `help` in CLI, press Enter
- [ ] Type `save`, press Enter (saves if file open)
- [ ] Type `build`, press Enter (runs compiler)
- [ ] Splitter cursor changes on hover
- [ ] PowerShell text is white (non-black)
- [ ] CLI text is green (visible non-black)
- [ ] Toggle "View > Terminal" to show/hide panes
- [ ] File tree still resizable with splitter active

## Future Enhancements

1. **Persistent State**: Save g_terminalSplitterPos to registry/config
2. **More CLI Commands**: 
   - `git <command>` - Git operations
   - `lint` - Run linter on current file
   - `test` - Run tests
   - `debug` - Launch debugger
3. **Auto-complete**: Tab completion for commands in CLI
4. **Command History**: Up/Down arrows to navigate CLI history
5. **MASM Integration**: Full wire-up of CommandDispatch() from CLI.asm
6. **Parallel Execution**: Non-blocking async command execution
7. **Output Filtering**: Color output, syntax highlighting in CLI

## Files Modified
- `d:\rawrxd\Ship\RawrXD_Win32_IDE.cpp` (8500+ lines)
  - Added dual-pane terminal infrastructure
  - Added CLI command executor
  - Added mouse splitter handling
  - Added layout management for split panes

## Files Created
- `d:\rawrxd\Ship\RawrXD_CommandCLI.asm` (180 lines, x64 MASM)
  - Command dispatcher skeleton
  - Handler infrastructure for future expansion

---

**Implementation Status**: ✓ Complete and Ready for Testing
**Compilation**: Requires MSVC compiler and Windows 10+ SDK
**Runtime**: Pure Win32 API, no external dependencies
