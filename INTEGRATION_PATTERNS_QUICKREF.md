# Integration Patterns - Quick Reference Card

## Three Core Patterns You Asked For

---

## Pattern 1: IDE_CreateMainWindow() - Window Creation

**Purpose**: Initialize the complete GUI from scratch

**Call**:
```asm
lea rcx, [title_string]             ; "My Editor"
lea rdx, [window_data_buffer]       ; 96+ bytes, caller-allocated
call IDE_CreateMainWindow
; Returns: rax = hwnd (window handle), or NULL on failure
```

**What it does internally**:
```
1. Register window class via EditorWindow_RegisterClass()
2. Create main window via EditorWindow_Create()
3. Create menu bar (File, Edit) via IDE_CreateMenu()
4. Create toolbar window via IDE_CreateToolbar()
5. Create status bar window via IDE_CreateStatusBar()
6. Show main window
```

**Window Data Structure** (96 bytes):
- Stores window handles (hwnd, hdc, hAccel, etc)
- Stores metrics (dimensions, scroll, font)
- Stores state (buffer, timer, backbuffer)

**Result**: Full functional text editor window displayed on screen

**Example Usage**:
```asm
    ; Allocate window data (96 bytes minimum)
    sub rsp, 96
    mov rdx, rsp
    
    ; Create window
    lea rcx, [myTitle]              ; "My Text Editor"
    call IDE_CreateMainWindow
    mov [window_hwnd], rax          ; Save handle
    
    ; Continue to Pattern 2...
```

---

## Pattern 2: IDE_SetupAccelerators() → IDE_MessageLoop() - Input Handling

**Purpose**: Wire keyboard shortcuts and run main event loop

**Setup**:
```asm
; After creating window with Pattern 1
mov rcx, [window_hwnd]              ; From Pattern 1
call IDE_SetupAccelerators
; Returns: rax = hAccelHandle
mov [hAccel_saved], rax             ; Must save!
```

**Keyboard Shortcuts Registered**:
- Ctrl+N → New
- Ctrl+O → Open
- Ctrl+S → Save
- Ctrl+Z → Undo
- Ctrl+X → Cut
- Ctrl+C → Copy
- Ctrl+V → Paste

**Run Message Loop**:
```asm
; Required: Use hAccel from IDE_SetupAccelerators()
mov rcx, [window_hwnd]
mov rdx, [hAccel_saved]             ; CRITICAL: Must provide accelerator!
call IDE_MessageLoop                ; BLOCKING until window closed
; Returns: rax = exit code
```

**IDE_MessageLoop() Internal Flow**:
```
LOOP:
  GetMessageA()                     ← Fetch events (blocks if idle)
  TranslateAcceleratorA()           ← Check for Ctrl+key
    └─ If match: Send WM_COMMAND to window
  TranslateMessageA()               ← Convert virtual keys
  DispatchMessageA()                ← Route to EditorWindow_WndProc()
  
  Message Handler (WndProc):
    - WM_COMMAND              → Execute menu/accelerator command
    - WM_KEYDOWN              → Character input
    - WM_PAINT                → Render text buffer
    - WM_DESTROY              → PostQuitMessage()
    - WM_TIMER                → Cursor blink
    - WM_LBUTTONDOWN          → Mouse click positioning
    - etc...
  
  IF PostQuitMessage received: BREAK and return exit code
```

**Result**: Interactive text editor running, responding to user input

**Example Usage**:
```asm
    ; After Pattern 1 created window in [window_hwnd]
    
    mov rcx, [window_hwnd]
    call IDE_SetupAccelerators
    mov [hAccel_handle], rax
    
    ; Now run message loop
    mov rcx, [window_hwnd]
    mov rdx, [hAccel_handle]
    call IDE_MessageLoop
    
    ; When user closes window:
    ; - Returns to here with exit code in rax
    ; - Program can then clean up and exit
```

**CRITICAL**: Must pass hAccel to IDE_MessageLoop()!

---

## Pattern 3: AI Completion - GetBufferSnapshot() → Process → InsertTokens()

**Purpose**: Enable AI-powered code completion

### Phase A: Export Current Buffer to AI

```asm
lea rcx, [text_buffer]              ; Current editor text
lea rdx, [snapshot_buffer]          ; Destination (64KB)
call AICompletion_GetBufferSnapshot
; Returns: rax = number of bytes exported
```

**Snapshot Contains**:
- Complete current editor text
- All characters user has typed so far
- Ready to send to AI backend

### Phase B: Send to AI Backend (Your Code)

```
AI Backend Options:
┌─────────────────────────────────────────┐
│ 1. Llama.cpp (Local)                    │
│    http://localhost:8000/v1/completions│
│    Fast, free, runs on your machine     │
│                                         │
│ 2. OpenAI API                           │
│    https://api.openai.com/v1/...        │
│    Professional, accurate, requires key │
│                                         │
│ 3. Custom HTTP server                   │
│    Your own backend service             │
└─────────────────────────────────────────┘
```

**Example: Llama.cpp**
```
POST http://localhost:8000/v1/completions
Content-Type: application/json

{
  "prompt": "[snapshot from GetBufferSnapshot]",
  "max_tokens": 50,
  "temperature": 0.7
}

Response:
{
  "choices": [
    {"text": ":\n    pass"}  ← Tokens to insert!
  ]
}
```

### Phase C: Insert AI Tokens

```asm
lea rcx, [text_buffer]              ; Where to insert
lea rdx, [tokens_from_ai]           ; Bytes received from AI
mov r8d, token_count                ; Number of bytes
call AICompletion_InsertTokens
; Returns: rax = 1 (success) or 0 (buffer full)
```

**Tokens Inserted At**:
- Current cursor position
- Editor automatically refreshed
- User can see completion immediately

### Complete Workflow Example

```
User types:     "def hello"
                      ↓
[Background thread starts]
                      ↓
GetBufferSnapshot() exports "def hello"
                      ↓
HTTP POST to llama.cpp with prompt "def hello"
                      ↓
AI returns ":\n    pass"
                      ↓
InsertTokens() inserts at cursor
                      ↓
User sees:      "def hello:\n    pass"
                ↓
User accepts (Tab) or rejects (Esc)
```

**Example Code (Pseudo)**:
```asm
; In background thread:
Thread_AIWorker:
    ; Get current buffer
    lea rcx, [text_buffer]
    lea rdx, [snapshot]
    call AICompletion_GetBufferSnapshot
    ; rax = size
    
    ; [Your code: POST snapshot to llama.cpp]
    ; [Your code: Parse JSON response]
    ; [Your code: Extract tokens]
    
    ; Insert tokens at cursor
    lea rcx, [text_buffer]
    lea rdx, [tokens]
    mov r8d, 10                     ; 10 bytes from AI
    call AICompletion_InsertTokens
    ; rax = success indicator
    
    ; Invalidate to refresh screen
    call InvalidateRect
    
    jmp Thread_AIWorker             ; Loop for next completion
```

---

## Side-by-Side Comparison

| Pattern | Purpose | Function | Input | Output | When Called |
|---------|---------|----------|-------|--------|-------------|
| 1 | Create GUI | `IDE_CreateMainWindow()` | title, data struct | hwnd | Startup, once |
| 2 | Input handling | `IDE_SetupAccelerators()` + `IDE_MessageLoop()` | hwnd, hAccel | exit code | After Pattern 1 |
| 3 | AI completion | `GetBufferSnapshot()` + HTTP + `InsertTokens()` | buffer | success/fail | Background loop |

---

## Data Flow Diagram

```
┌─────────────────────────────────┐
│   WinMain() Entry Point         │
└──────────────┬──────────────────┘
               │
               ├─► Pattern 1: IDE_CreateMainWindow()
               │   └─► window displayed
               │
               ├─► Pattern 2: IDE_SetupAccelerators()
               │   └─► keyboard shortcuts wired
               │
               ├─► Pattern 2: IDE_MessageLoop()
               │   └─► ┌─ GetMessageA()
               │       ├─ TranslateAcceleratorA()
               │       ├─ DispatchMessageA()
               │       └─ Loop until WM_QUIT
               │
               ├─► (Optional) Background thread:
               │   └─► Pattern 3: AI Completion Loop
               │       ├─ GetBufferSnapshot()
               │       ├─ HTTP POST to AI
               │       ├─ Parse JSON
               │       └─ InsertTokens()
               │
               └─► Cleanup and exit
```

---

## Common Mistakes to Avoid

❌ **Mistake 1**: Forget to save hAccel from IDE_SetupAccelerators()
```asm
call IDE_SetupAccelerators
; WRONG - hAccel in rax, not saved!
```
✅ **Fix**: 
```asm
call IDE_SetupAccelerators
mov [hAccel_saved], rax             ; Save it!
```

---

❌ **Mistake 2**: Pass wrong parameters to IDE_MessageLoop()
```asm
call IDE_MessageLoop                ; WRONG - no parameters!
```
✅ **Fix**:
```asm
mov rcx, hwnd                       ; Parameter 1
mov rdx, hAccel                     ; Parameter 2
call IDE_MessageLoop
```

---

❌ **Mistake 3**: Call InsertTokens with huge token count
```asm
mov r8d, 1000000                    ; WRONG - buffer overflow!
call AICompletion_InsertTokens
```
✅ **Fix**:
```asm
mov r8d, byte_count_from_ai         ; Actual bytes received
call AICompletion_InsertTokens
```

---

## Integration Checklist

**Startup**:
- [ ] Allocate 96-byte window_data structure
- [ ] Call `IDE_CreateMainWindow()` with title
- [ ] Save returned hwnd
- [ ] Call `IDE_SetupAccelerators(hwnd)`
- [ ] Save returned hAccel
- [ ] Call `IDE_MessageLoop(hwnd, hAccel)` - launches UI

**AI Completion** (background thread):
- [ ] Call `GetBufferSnapshot(buffer, snapshot)`
- [ ] Build AI request with snapshot
- [ ] POST to AI service (llama.cpp or OpenAI)
- [ ] Parse JSON response
- [ ] Extract tokens
- [ ] Call `InsertTokens(buffer, tokens, count)`
- [ ] Loop for next completion

**Shutdown**:
- [ ] `IDE_MessageLoop()` returns on WM_QUIT
- [ ] Clean up handles
- [ ] Exit program

---

## Reference Files

- **RawrXD_TextEditorGUI.asm** - Main editor (42 procedures, 2092 lines)
- **WinMain_Integration_Example.asm** - Full working example
- **AICompletionIntegration.asm** - AI backend examples (llama.cpp, OpenAI)
- **INTEGRATION_USAGE_GUIDE.md** - Detailed patterns with code
- **TEXTEDITOR_FUNCTION_REFERENCE.asm** - All function documentation

---

**Start with Pattern 1 (IDE_CreateMainWindow), then Pattern 2 (IDE_SetupAccelerators/IDE_MessageLoop), then add Pattern 3 (AI) in background thread.**
