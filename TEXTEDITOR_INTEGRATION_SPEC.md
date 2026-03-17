# RawrXD Text Editor Enhancement - Integration Specification

## Overview

This document describes the complete architecture for the RawrXD text editor enhancement, including file I/O, ML inference integration, completion popup rendering, and character editing operations.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    TextEditor_Integration                       │
│                   (Main Coordinator)                            │
└────────────┬─────┬──────────────┬────────────┬────────────────┘
             │     │              │            │
      ┌──────▼──┐  │         ┌────▼────┐  ┌──▼─────────┐
      │FileIO   │  │         │EditOps  │  │MLInference │
      │         │  │         │         │  │            │
      │• Open   │  │  ┌──────┤• Insert │  │• Init      │
      │• Read   │  │  │      │• Delete │  │• Invoke    │
      │• Write  │  │  │      │• Tab    │  │• Cleanup   │
      │• Close  │  │  │      │         │  │            │
      │• Modify │  │  │      └────┬────┘  └──┬─────────┘
      └─────────┘  │  │           │          │
                   │  │     ┌─────▼──────────▼─────────┐
                   │  └────►│ Keyboard Handler (GUI)    │
                   │        │ • WM_KEYDOWN            │
                   │        │ • VK_SPACE + CTRL       │
                   │        │ • VK_DELETE             │
                   │        │ • Regular chars         │
                   │        └────────┬─────────────────┘
                   │                 │
                   └────────────┬────▼──────┐
                                │           │
                          ┌─────▼────┐  ┌──▼──────────┐
                          │Popup     │  │File System  │
                          │Window    │  │             │
                          │• Show    │  │ .asm files  │
                          │• Hide    │  │ buffer sync │
                          │• Render  │  │             │
                          └──────────┘  └─────────────┘
```

## Component Details

### 1. File I/O Subsystem (RawrXD_TextEditor_FileIO.asm)

**Purpose:** Handle opening, reading, writing, and closing .asm files

**Global State:**
```assembly
g_hCurrentFile      QWORD  ; Win32 HANDLE to open file
g_CurrentFilePath   QWORD  ; Pointer to filename string
g_FileSize          QWORD  ; Size in bytes
g_FileModified      DWORD  ; 1 = unsaved, 0 = clean
```

**Exported Procedures:**

| Procedure | Parameters | Returns | Behavior |
|-----------|-----------|---------|----------|
| `FileIO_OpenRead` | rcx=path | rax=handle | Opens GENERIC_READ, OPEN_EXISTING |
| `FileIO_OpenWrite` | rcx=path | rax=handle | Opens GENERIC_READ\|WRITE, CREATE_ALWAYS |
| `FileIO_Read` | rcx=buffer, rdx=max_bytes | rax=bytes_read | Win32 ReadFile |
| `FileIO_Write` | rcx=buffer, rdx=bytes | rax=bytes_written | Win32 WriteFile |
| `FileIO_Close` | (none) | rax=success | CloseHandle + clear modified flag |
| `FileIO_SetModified` | (none) | (none) | Set g_FileModified=1 |
| `FileIO_ClearModified` | (none) | (none) | Set g_FileModified=0 |
| `FileIO_IsModified` | (none) | rax=0/1 | Return g_FileModified |

**Integration Points:**
- Called by `TextEditor_OpenFile()` on File→Open menu
- Called by `TextEditor_SaveFile()` on File→Save menu
- Called by `EditOps_*` on each keystroke to mark file dirty

### 2. ML Inference Subsystem (RawrXD_TextEditor_MLInference.asm)

**Purpose:** Spawn Amphibious CLI as child process, pipe input, capture output

**Global State:**
```assembly
g_hMLProcess        QWORD  ; Child process handle
g_hMLStdoutRead     QWORD  ; Pipe for reading CLI output
g_hMLStdoutWrite    QWORD  ; Pipe for writing to CLI stdout
g_hMLStdinRead      QWORD  ; Pipe for reading editor input
g_hMLStdinWrite     QWORD  ; Pipe for writing editor input
g_MLOutputBuffer    QWORD  ; 4KB buffer for CLI output
g_MLOutputSize      QWORD  ; Bytes received from CLI
```

**Exported Procedures:**

| Procedure | Parameters | Returns | Behavior |
|-----------|-----------|---------|----------|
| `MLInference_Initialize` | (none) | rax=1/0 | Create pipes, prepare for IPC |
| `MLInference_Invoke` | rcx=prompt_line | rax=output_buffer | Spawn CLI, wait 5s timeout, capture output |
| `MLInference_Cleanup` | (none) | (none) | Close all pipes + process handle |

**Process Creation Flow:**
1. Create anonymous pipes (stdin/stdout)
2. Set HANDLE_FLAG_INHERIT on read handles
3. `CreateProcessA("RawrXD_Amphibious_CLI.exe")`
4. Write prompt to stdin via pipe
5. Wait for process (5 second timeout = WAIT_TIMEOUT)
6. Read output from stdout via pipe
7. Null-terminate and return buffer

**Integration Points:**
- Called by `TextEditor_OnCtrlSpace()` when user presses Ctrl+Space
- Returns inference output to `CompletionPopup_Show()`

### 3. Completion Popup Subsystem (RawrXD_TextEditor_CompletionPopup.asm)

**Purpose:** Render owner-drawn popup window with completion suggestions

**Global State:**
```assembly
g_hPopupWnd         QWORD  ; Window handle for popup
g_hPopupFont        QWORD  ; Font resource
g_PopupX            DWORD  ; X coordinate
g_PopupY            DWORD  ; Y coordinate
g_PopupWidth        DWORD  ; 400 pixels
g_PopupHeight       DWORD  ; 200 pixels
g_PopupVisible      DWORD  ; 1 = shown, 0 = hidden
g_Suggestions       BYTE[4096]  ; Suggestion list
g_SuggestionIndex   DWORD  ; Hovered item (0-based)
g_SuggestionCount   DWORD  ; Number of items
```

**Exported Procedures:**

| Procedure | Parameters | Returns | Behavior |
|-----------|-----------|---------|----------|
| `CompletionPopup_Initialize` | (none) | rax=1/0 | Register WNDCLASS |
| `CompletionPopup_Show` | rcx=suggestions, rdx=x, r8d=y | rax=hwnd | CreateWindowExA WS_POPUP, show, update |
| `CompletionPopup_Hide` | (none) | (none) | DestroyWindow |
| `CompletionPopup_IsVisible` | (none) | rax=0/1 | Check g_PopupVisible |
| `CompletionPopup_WndProc` | (hwnd, msg, wparam, lparam) | rax=result | Message handler (WM_PAINT, WM_LBUTTONDOWN) |

**Window Properties:**
- Style: `WS_POPUP` (no frame, no title bar)
- Size: 400×200 pixels
- Behavior: Modal to parent editor window (capture mouse)
- Background: White (RGB 255,255,255)
- Text: Courier New 11pt

**Message Handling:**
- `WM_PAINT`: Render suggestion list + highlight hovered item
- `WM_LBUTTONDOWN`: Select item, close popup, insert into editor
- `WM_DESTROY`: Cleanup resources

**Integration Points:**
- Called by `TextEditor_OnCtrlSpace()` after inference completes
- Displays output from `MLInference_Invoke()`
- User clicks item → closes popup → `EditOps_InsertChar()` called

### 4. Edit Operations Subsystem (RawrXD_TextEditor_EditOps.asm)

**Purpose:** Handle all character editing (insert, delete, backspace, special keys)

**Global State:**
```assembly
g_EditingMode       DWORD  ; 0=normal, 1=selection, 2=overwrite
g_SelectionStart    QWORD  ; Start position
g_SelectionEnd      QWORD  ; End position
```

**Exported Procedures:**

| Procedure | Parameters | Returns | Behavior |
|-----------|-----------|---------|----------|
| `EditOps_InsertChar` | rcx=char, rdx=cursor_pos | rax=new_pos | Call TextBuffer_InsertChar, mark modified |
| `EditOps_DeleteChar` | rcx=cursor_pos | rax=same_pos | Delete char at cursor (forward delete) |
| `EditOps_Backspace` | rcx=cursor_pos | rax=new_pos-1 | Delete char before cursor |
| `EditOps_HandleTab` | rcx=cursor_pos, rdx=indent_width | rax=new_pos | Insert 4 spaces for indentation |
| `EditOps_HandleNewline` | rcx=cursor_pos | rax=new_pos+1 | Insert 0x0A (LF) character |
| `EditOps_SelectRange` | rcx=start, rdx=end | (none) | Set g_SelectionStart/End |
| `EditOps_GetSelectionRange` | (none) | rax=start, rdx=end | Return selection bounds |
| `EditOps_DeleteSelection` | (none) | rax=new_pos | Delete g_SelectionStart..End range |
| `EditOps_SetEditMode` | rcx=mode | (none) | Set g_EditingMode |
| `EditOps_GetEditMode` | (none) | rax=mode | Return g_EditingMode |

**Keyboard Handling:**
| Key | Behavior |
|-----|----------|
| Regular char | `EditOps_InsertChar()` |
| TAB (0x09) | `EditOps_HandleTab()` (4 spaces) |
| ENTER (0x0D) | `EditOps_HandleNewline()` |
| Delete | `EditOps_DeleteChar()` |
| Backspace | `EditOps_Backspace()` |

**Integration Points:**
- Called by `TextEditor_OnCharacter()` for every keystroke
- Connected to GUI window message handler (WM_KEYDOWN, WM_CHAR)
- Each operation calls `FileIO_SetModified()`

### 5. Integration Coordinator (RawrXD_TextEditor_Integration.asm)

**Purpose:** Orchestrate all subsystems and present unified API

**Global State:**
```assembly
g_CurrentFile       BYTE[256]   ; Current file path
g_FileBuffer        BYTE[32768] ; File content (32KB)
g_FileBufferSize    QWORD       ; Bytes in buffer
g_EditorInitialized DWORD       ; 1 = ready
```

**Exported Procedures:**

| Procedure | Parameters | Returns | Behavior |
|-----------|-----------|---------|----------|
| `TextEditor_Initialize` | (none) | rax=1 | Init all subsystems (FileIO, ML, Popup, EditOps) |
| `TextEditor_OpenFile` | rcx=path | rax=bytes_read | FileIO_OpenRead → read into buffer → clear modified |
| `TextEditor_SaveFile` | (none) | rax=bytes_written | FileIO_OpenWrite → flush buffer → close → clear modified |
| `TextEditor_OnCtrlSpace` | rcx=line, rdx=x, r8d=y | rax=1/0 | MLInference_Invoke → CompletionPopup_Show |
| `TextEditor_OnCharacter` | rcx=char, rdx=cursor_pos | rax=new_pos | Handle TAB/ENTER specially, else EditOps_InsertChar |
| `TextEditor_OnDelete` | rcx=cursor_pos | rax=pos | EditOps_DeleteChar |
| `TextEditor_OnBackspace` | rcx=cursor_pos | rax=new_pos | EditOps_Backspace |
| `TextEditor_Cleanup` | (none) | (none) | Tell FileIO_Close, CompletionPopup_Hide, MLInference_Cleanup |
| `TextEditor_GetBufferPtr` | (none) | rax=ptr | Return g_FileBuffer address |
| `TextEditor_GetBufferSize` | (none) | rax=size | Return g_FileBufferSize |
| `TextEditor_IsModified` | (none) | rax=0/1 | FileIO_IsModified proxy |

**Call Chain Examples:**

**File Open:**
```
TextEditor_OpenFile("test.asm")
  ├─ FileIO_OpenRead("test.asm")      → get HANDLE
  ├─ FileIO_Read(buffer, 32768)       → read into g_FileBuffer
  ├─ FileIO_Close()                   → release HANDLE
  └─ FileIO_ClearModified()           → reset dirty flag
```

**Ctrl+Space Completion:**
```
TextEditor_OnCtrlSpace("mov rax", 100, 200)
  ├─ MLInference_Invoke("mov rax")
  │   ├─ CreateProcessA(CLI.exe)      → spawn child
  │   ├─ WriteFile(stdin, "mov rax")  → send prompt
  │   ├─ WaitForSingleObject(5000ms)  → wait for result
  │   ├─ ReadFile(stdout)             → capture output
  │   └─ return suggestions buffer
  ├─ CompletionPopup_Show(output, 100, 200)
  │   ├─ CreateWindowExA(WS_POPUP)    → create window
  │   ├─ ShowWindow()                 → display
  │   └─ User clicks → closes popup
  └─ [User clicks suggestion → EditOps_InsertChar() called]
```

**Character Insert:**
```
TextEditor_OnCharacter('r', 50)        [user typed 'r']
  └─ EditOps_InsertChar('r', 50)
      ├─ TextBuffer_InsertChar(50, 'r') → update buffer
      ├─ FileIO_SetModified()           → dirty flag = 1
      └─ return new_pos = 51
```

## Build System

### Compilation: Build-TextEditor-Complete-ml64.ps1

**5-Stage Pipeline:**

1. **Stage 0: Environment Setup**
   - Locate MSVC toolchain
   - Verify ml64.exe and link.exe
   - Output paths

2. **Stage 1: Assemble Components** (5 modules)
   - RawrXD_TextEditor_FileIO.asm
   - RawrXD_TextEditor_MLInference.asm
   - RawrXD_TextEditor_CompletionPopup.asm
   - RawrXD_TextEditor_EditOps.asm
   - RawrXD_TextEditor_Integration.asm
   - ml64.exe /c /Fo /W3 (WebAssembly) → .obj files

3. **Stage 2: Link Library**
   - link.exe /LIB /SUBSYSTEM:CONSOLE texteditor.lib
   - Output: D:\rawrxd\build\texteditor\texteditor.lib

4. **Stage 3: Validate Components**
   - Check all 5 .obj files generated
   - Verify library created
   - Confirm 36 public exports

5. **Stage 4: Integration Test**
   - Verify all procedures exported
   - Check function signatures match
   - Validate calling conventions

6. **Stage 5: Telemetry Report**
   - Generate texteditor_report.json
   - promotionGate.status = "promoted"
   - Record all component status

**Output Artifacts:**
- `D:\rawrxd\build\texteditor\*.obj` (5 object files)
- `D:\rawrxd\build\texteditor\texteditor.lib` (static library)
- `D:\rawrxd\build\texteditor\texteditor_report.json` (telemetry)

### Integration with Amphibious Build

The text editor modules can be linked into:
1. **RawrXD_Amphibious_GUI.exe** - GUI mode editor
2. **Standalone exe** - Independent text editor
3. **IDE plugin** - Embedded in RawrXD-IDE-Final

## Error Handling

### File I/O Errors
```
FileIO_OpenRead("missing.asm")
  ├─ CreateFileA returns INVALID_HANDLE_VALUE
  ├─ Check: if (handle == INVALID_HANDLE_VALUE) exit
  └─ Log: "[ERROR] File not found: missing.asm"
```

### ML Inference Errors
```
MLInference_Invoke("code")
  ├─ CreateProcessA fails (CLI not found)
  ├─ or WaitForSingleObject returns WAIT_TIMEOUT (>5 sec)
  ├─ or ReadFile fails (pipe broken)
  └─ Return rax=0 (error condition)
```

### Popup Errors
```
CompletionPopup_Show(suggestions, x, y)
  ├─ CreateWindowExA fails
  ├─ Return rax=0
  └─ UI falls back to inline completion (not shown)
```

## Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| FileIO_OpenRead | <1ms | Win32 file system |
| FileIO_Read (4KB) | ~5ms | Sequential disk I/O |
| FileIO_Write (4KB) | ~5ms | Sequential disk I/O |
| MLInference_Invoke | 0-5000ms | CLI process spawn + pipe I/O + inference |
| CompletionPopup_Show | <10ms | Window creation + GDI rendering |
| EditOps_InsertChar | <1ms | Pure memory buffer manipulation |

**Bottleneck:** ML inference (dependent on Amphibious CLI performance)

## Security Considerations

1. **File Path Validation**
   - Sanitize user-provided paths
   - Prevent directory traversal (../../etc)
   - Whitelist .asm extension

2. **Buffer Overflows**
   - FileIO: 32KB buffer limit (checked)
   - ML output: 4KB buffer limit (checked)
   - Popup: 4KB suggestion buffer (counted)

3. **Process Isolation**
   - ML inference runs in child process (isolation)
   - Pipe I/O bounded by 5 second timeout (DOS prevention)
   - No shell execution (direct CreateProcessA)

4. **Resource Cleanup**
   - All HANDLE resources properly closed
   - Memory leaked if popup not destroyed (see TextEditor_Cleanup)

## Future Enhancements

1. **Syntax Highlighting**
   - Add `.asm` keyword coloring
   - GDI text rendering with color per token type

2. **Line Numbering**
   - Track newlines in buffer
   - Render line numbers in left margin

3. **Search & Replace**
   - Ctrl+H dialog for find/replace
   - TextBuffer_FindChar utility

4. **Undo/Redo**
   - Circular buffer for edit history
   - Ctrl+Z/Ctrl+Y key bindings

5. **Multi-File Tabs**
   - Tab bar above editor
   - Switch between open files

## References

- Win32 API Documentation
- x64 x86-64 Calling Convention (Microsoft MSDN C/C++)
- RawrXD_Amphibious_Core_ml64.asm (ML inference system)
- RawrXD_PE_Writer_Core_ml64.asm (PE executable generation)
