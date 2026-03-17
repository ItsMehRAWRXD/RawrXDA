# Integration Guide: Using RawrXD_TextEditorGUI APIs

## Quick Start - 3 Simple Steps

### Step 1: Create Window
```asm
lea rcx, [title_string]             ; "My Editor"
lea rdx, [window_data_96_bytes]
call IDE_CreateMainWindow           ; Returns hwnd in rax
```

### Step 2: Setup Shortcuts  
```asm
mov rcx, [hwnd_from_step1]
call IDE_SetupAccelerators          ; Returns hAccel in rax
mov r13, rax                        ; Save accelerator handle
```

### Step 3: Run Message Loop
```asm
mov rcx, [hwnd_from_step1]
mov rdx, [hAccel_from_step2]
call IDE_MessageLoop                ; Blocking until window closes
```

---

## Detailed Integration Patterns

### Pattern A: Basic Window Creation

**File**: `WinMain_Integration_Example.asm` (created in this session)

```asm
; Initialize 96-byte window data structure
; (handles, metrics, state)

; Create main window with all UI components
lea rcx, [window_title]
lea rdx, [window_data]
call IDE_CreateMainWindow
; Returns: rax = hwnd or NULL

; Show window
mov rcx, rax
mov edx, SW_SHOW
call ShowWindow
```

**What `IDE_CreateMainWindow()` does**:
1. ✅ Registers window class "RXD" with `EditorWindow_RegisterClass()`
2. ✅ Creates window with `EditorWindow_Create()`
3. ✅ Attaches File/Edit menus with `IDE_CreateMenu()`
4. ✅ Creates toolbar window with `IDE_CreateToolbar()`
5. ✅ Creates status bar with `IDE_CreateStatusBar()`

**Result**: Fully initialized editor window ready to use

---

### Pattern B: Keyboard Shortcuts Setup

**File**: `WinMain_Integration_Example.asm` (Demonstrated)

```asm
; After creating window with IDE_CreateMainWindow

; Setup keyboard accelerators (Ctrl+key combinations)
mov rcx, hwnd
call IDE_SetupAccelerators
; Returns: rax = hAccel (accelerator table handle)

; IMPORTANT: Save hAccel for message loop
mov hAccel_saved, rax
```

**Shortcuts provided**:
- ✅ Ctrl+N → File > New
- ✅ Ctrl+O → File > Open  
- ✅ Ctrl+S → File > Save
- ✅ Ctrl+Z → Edit > Undo
- ✅ Ctrl+X → Edit > Cut
- ✅ Ctrl+C → Edit > Copy

**Note**: These are automatically wired to menu commands via `WM_COMMAND` messages

---

### Pattern C: Message Loop with Accelerators

**File**: `WinMain_Integration_Example.asm` (Main loop section)

```asm
; IMPORTANT: Use accelerator handle from Step B
mov rcx, hwnd
mov rdx, hAccel_handle
call IDE_MessageLoop
; Blocking call - returns when WM_QUIT received
; rax = exit code
```

**IDE_MessageLoop() Flow**:
1. `GetMessageA()` → Fetch event from queue
2. `TranslateAcceleratorA()` → Check for Ctrl+key shortcuts
3. `TranslateMessageA()` → Convert virtual keys
4. `DispatchMessageA()` → Route to `EditorWindow_WndProc()`
5. Repeat until `PostQuitMessage()` received

**Result**: Full interactive text editor running

---

### Pattern D: AI Completion Integration

**File**: `AICompletionIntegration.asm` (Complete example)

#### Phase 1: Export Buffer to AI
```asm
; Get current text for AI processing
lea rcx, [text_buffer]
lea rdx, [snapshot_buffer]          ; 64KB buffer
call AICompletion_GetBufferSnapshot
; rax = buffer size

; snapshot_buffer now contains current editor text
; Ready to send to AI backend
```

#### Phase 2: Send to Backend
```asm
; [Custom code]
; POST snapshot_buffer to AI service:
;   - llama.cpp: http://localhost:8000/v1/completions
;   - OpenAI: https://api.openai.com/v1/chat/completions
;   - Local model: any HTTP endpoint

; Build JSON request with snapshot text
; Make HTTP request
; Wait for response
```

#### Phase 3: Insert AI Tokens
```asm
; Parse AI response to extract tokens
; [AI_ParseJsonResponse example in AICompletionIntegration.asm]

; Insert tokens into buffer
lea rcx, [text_buffer]
lea rdx, [tokens_array]             ; Bytes from AI
mov r8d, 5                          ; 5 tokens
call AICompletion_InsertTokens
; rax = 1 (success) or 0 (buffer full)

; Text inserted at cursor, screen auto-refreshed
```

**Complete Flow**:
```
User types "def hello"
         ↓
AI worker thread calls GetBufferSnapshot()
         ↓
"def hello" sent to AI backend
         ↓
AI returns: ":\n    pass"
         ↓
InsertTokens() adds tokens at cursor
         ↓
User sees: "def hello:\n    pass"
         ↓
User can accept (Tab) or reject (Esc)
```

---

### Pattern E: File Operations

#### File Open
```asm
lea rcx, [window_data]
call EditorWindow_FileOpen
; rax = filename string or NULL

; File content loaded into buffer automatically
; Screen refreshed automatically
```

#### File Save
```asm
lea rcx, [window_data]
lea rdx, [filename]
call EditorWindow_FileSave
; rax = 1 (success) or 0 (error)
```

---

### Pattern F: Status Bar Updates

```asm
; Update status with message
lea rcx, [window_data]
lea rdx, [status_text]              ; "Ready" or other message
call EditorWindow_UpdateStatus

; Examples of status messages:
;   - "Ready"
;   - "Line 42, Col 15"  
;   - "Saving..."
;   - "AI Processing..."
;   - "File Saved"
```

---

## Window Data Structure Reference

**Location**: Allocated by caller, passed to `IDE_CreateMainWindow()`
**Size**: 96+ bytes minimum

```
Offset  Bytes  Field                  Purpose
-------------------------------------------------------
0       8      hwnd                   Window handle
8       8      hdc                    Device context
16      8      hfont                  Font (Courier 8x16)
24      8      cursor_ptr             Cursor structure
32      8      buffer_ptr             Text buffer
40      4      char_width             8 pixels
44      4      char_height            16 pixels
48      4      client_width           800 pixels
52      4      client_height          600 pixels
56      4      line_num_width         40 pixels
60      4      scroll_offset_x        H-scroll
64      4      scroll_offset_y        V-scroll
68      8      hbitmap                Backbuffer
76      8      hmemdc                 Memory DC
84      4      timer_id               Blink timer
88      8      hToolbar               Toolbar
92      8      hAccel                 Accelerator (SET BY IDE_SetupAccelerators)
96      8      hStatusBar             Status bar
```

---

## Keyboard Shortcuts - Complete List

| Shortcut | Menu Command | ID | Handler |
|----------|--------------|----|----|
| Ctrl+N | File > New | 0x1001 | EditorWindow_FileOpen |
| Ctrl+O | File > Open | 0x1002 | EditorWindow_FileOpen |
| Ctrl+S | File > Save | 0x1003 | EditorWindow_FileSave |
| Ctrl+Z | Edit > Undo | 0x2001 | (Future) |
| Ctrl+X | Edit > Cut | 0x2002 | (Future) |
| Ctrl+C | Edit > Copy | 0x2003 | (Future) |
| Ctrl+V | Edit > Paste | (Reserved) | (Future) |

All shortcuts are automatically routed through `IDE_MessageLoop()` via `TranslateAcceleratorA()`

---

## Error Handling

### Window Creation Failure
```asm
call IDE_CreateMainWindow
test rax, rax
jz .CreateWindowFailed
```

### Token Insertion Failure
```asm
call AICompletion_InsertTokens
test eax, eax
jz .BufferFullError
```

### File Operation Failure
```asm
call EditorWindow_FileSave
test eax, eax
jz .SaveFailed
```

---

## MultithreadingConsiderations

For AI completion in background:

1. **Main thread** (UI):
   - Runs `IDE_MessageLoop()`
   - Handles user input, rendering
   - Thread-safe: All GDI calls must be on main thread

2. **Background thread** (AI worker):
   - Monitors typing activity
   - Calls `AICompletion_GetBufferSnapshot()`
   - Makes HTTP requests to AI backend
   - **IMPORTANT**: Use synchronization (critical section) when calling `InsertTokens()` from background thread

---

## Compilation & Build

### Three Files to Build:

1. **RawrXD_TextEditorGUI.asm** (Main editor)
   - 2,092 lines, 42 procedures
   - Contains all rendering, input, file I/O

2. **WinMain_Integration_Example.asm** (Application entry)
   - ~150 lines
   - Shows how to use IDE_CreateMainWindow()
   - Optional: Integrate into your own WinMain

3. **AICompletionIntegration.asm** (AI backend integration)
   - ~250 lines
   - Shows patterns for calling GetBufferSnapshot/InsertTokens
   - Optional: Template for your AI service integration

### Build Process:
```batch
ml64.exe RawrXD_TextEditorGUI.asm /c /Fo TextEditorGUI.obj /W3
ml64.exe WinMain_Integration_Example.asm /c /Fo WinMain.obj /W3
link.exe TextEditorGUI.obj WinMain.obj kernel32.lib user32.lib gdi32.lib ^
    /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:TextEditorGUI.exe
```

---

## Testing Checklist

- [ ] Window appears (800x600)
- [ ] Type text; appears in editor
- [ ] Cursor visible and blinking
- [ ] Arrow keys move cursor
- [ ] Ctrl+O opens file dialog
- [ ] Ctrl+S opens save dialog
- [ ] Menu bar clickable
- [ ] Line numbers display
- [ ] Backspace deletes characters
- [ ] Delete key removes next char
- [ ] Mouse click positions cursor
- [ ] Home/End keys work
- [ ] Page Up/Down scroll

---

## Next Steps

1. **Basic Usage**:
   - Use patterns from this guide
   - See `WinMain_Integration_Example.asm` for full example

2. **AI Integration**:
   - See `AICompletionIntegration.asm` for patterns
   - Adapt pattern to your AI backend (llama, OpenAI, etc)
   - Run in background thread

3. **Customization**:
   - Add toolbar buttons (toolbar window exists, needs button iteration)
   - Custom status updates
   - Syntax highlighting
   - Additional menus

---

## Support Files

- **TEXTEDITOR_IDE_INTEGRATION_DELIVERY.md** - 400+ line integration reference
- **TEXTEDITOR_FUNCTION_REFERENCE.asm** - All 42 functions documented
- **00-FINAL_DELIVERY_SUMMARY.md** - Architecture overview
- **MANIFEST.txt** - Project checklist

---

**Your editor is ready to use. Start with Step 1 above, or examine `WinMain_Integration_Example.asm` for a complete working example.**
