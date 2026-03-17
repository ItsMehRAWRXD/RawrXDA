# Phase 2: Critical Integration Implementation Guide

**Objective**: Wire up LSP client, session management, and agent bridge (5,850 LOC total)  
**Timeline**: 5-7 days  
**Priority**: Files 28-32 (plus Files 29-30 expansion)

---

## File 28: session_management.asm (350 LOC)

### Purpose
- Periodic auto-save (30s interval)
- Crash recovery via backup
- Restore session on startup (last 20 tabs + cursor positions)

### Key Functions

```masm
Session_Initialize          ; Load previous session.json
Session_AutoSave           ; 30s timer callback
Session_SaveSnapshot       ; Atomic write to session.json
Session_CrashRecover       ; Restore from backup if IDE crashed
Session_ExportJSON         ; Serialize tab list to JSON string

; Data structures
SessionTab STRUCT
    filePath        QWORD ?     ; C:\path\to\file.cpp
    cursorPos       QWORD ?     ; Byte offset
    scrollLine      DWORD ?     ; Top visible line
    isDirty         BYTE ?      ; Modified?
SessionTab ENDS

SessionState STRUCT
    version         DWORD ?     ; Format version (1)
    tabCount        DWORD ?     ; Number of tabs
    activeTabs      QWORD ?     ; Array of SessionTab
    lastSaveTime    QWORD ?     ; Timestamp
    autoSaveEnabled BYTE ?      ; 30s saves?
SessionState ENDS
```

### Implementation Strategy
1. **Load phase** (IDE startup):
   - Check for `session.json` in config directory
   - Parse JSON (minimal parser for [filePath, cursorPos, scrollLine])
   - Restore each tab via `TabBuffer_OpenFile`
   - Re-activate previous active tab

2. **Save phase** (every 30s via timer):
   - Lock [sessionMutex]
   - Iterate active tabs, collect metadata
   - Serialize to JSON string in temp buffer
   - AtomicWrite: temp file → session.json (via rename)
   - Unlock

3. **Crash recovery**:
   - On startup, check timestamp of session.json vs session.bak
   - If session.json is fresh but IDE crashed, use session.bak
   - Clean up backups older than 7 days

### JSON Format (Simple)
```json
{
  "version": 1,
  "tabs": [
    {"file": "C:\\code\\main.cpp", "cursor": 1024, "scroll": 10},
    {"file": "C:\\code\\lib.h", "cursor": 512, "scroll": 5}
  ],
  "activeTab": 0
}
```

### Minimal JSON Parser (MASM)
```masm
; Pseudo-code structure
JSON_Parse PROC lpJsonString:QWORD, jsonLen:QWORD
    mov i, 0
.loop:
    .if BYTE PTR [lpJsonString + i] == '{'
        call JSON_ParseObject
    .endif
    inc i
    .continue
.endloop
    ret
JSON_Parse ENDP
```

**Complexity**: O(n) single-pass parse, no tree structure (sequential fields only)

---

## File 29: lsp_client.asm (800 LOC)

### Purpose
- Connect to language server (clangd, pyright, etc.)
- Send/receive JSON-RPC over TCP
- Fetch diagnostics, completions, hover info

### Architecture

```
IDE Editor (MASM)
    ↓ [Named Pipe / TCP Socket]
    ↓
Language Server Process (clangd --listen 127.0.0.1:6009)
    ↓ [JSON-RPC 2.0 Protocol]
    ↓
Responses: Diagnostics, Completions, Definitions
```

### Key Functions

```masm
LSP_Connect              ; TCP connect to localhost:6009
LSP_Initialize           ; Send initialize request (version handshake)
LSP_DidOpen             ; Notify LSP of opened file + content
LSP_DidChange           ; Send incremental edit changes
LSP_GetDiagnostics      ; Request file diagnostics (errors/warnings)
LSP_GetCompletions      ; Request completions at cursor position
LSP_GetHover            ; Request hover info
LSP_GetDefinition       ; Request "go to definition"
LSP_ProcessMessage      ; Parse incoming JSON-RPC response
LSP_Disconnect          ; Clean shutdown

; Data structures
LSPMessage STRUCT
    id              DWORD ?     ; JSON-RPC request ID (incremental)
    method          QWORD ?     ; "textDocument/didOpen", etc.
    params          QWORD ?     ; JSON params object
    result          QWORD ?     ; Response data (JSON)
LSPMessage ENDS

LSPDiagnostic STRUCT
    line            DWORD ?     ; 0-indexed line
    column          DWORD ?     ; 0-indexed column
    endLine         DWORD ?     ; Range end
    endColumn       DWORD ?     ; Range end
    message         QWORD ?     ; Error/warning text
    severity        DWORD ?     ; 1=Error, 2=Warning, 3=Info
LSPDiagnostic ENDS

LSPCompletion STRUCT
    label           QWORD ?     ; "myFunction"
    kind            DWORD ?     ; 1=Function, 2=Variable, 3=Class, etc.
    detail          QWORD ?     ; Type signature
    documentation   QWORD ?     ; Help text
    sortText        QWORD ?     ; Fuzzy match score
LSPCompletion ENDS
```

### JSON-RPC Protocol (Simplified)

**Request (IDE → Server)**:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "processId": 12345,
    "rootPath": "C:\\code"
  }
}
```

**Response (Server → IDE)**:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "capabilities": {
      "diagnosticProvider": true,
      "completionProvider": true,
      "hoverProvider": true
    }
  }
}
```

### MASM JSON Parser (Minimal)

```masm
JSON_ExtractString PROC lpJson:QWORD, lpKey:QWORD, lpOutBuffer:QWORD
    ; Find "key": "value"
    ; Returns value string in lpOutBuffer
    ret
JSON_ExtractString ENDP

JSON_ExtractNumber PROC lpJson:QWORD, lpKey:QWORD
    ; Find "key": 123
    ; Returns 123 in EAX
    ret
JSON_ExtractNumber ENDP

JSON_ExtractArray PROC lpJson:QWORD, lpKey:QWORD, lpOutArray:QWORD
    ; Find "key": [item1, item2, ...]
    ; Returns array count in EAX
    ret
JSON_ExtractArray ENDP
```

**Approach**: No JSON tree parsing; direct string searches for expected keys (protocol is deterministic)

### Implementation Phases

1. **Phase 2a (Days 1-2)**: TCP socket + message framing
   - `CreateSocketA`, `connect`, `send`, `recv`
   - Message length prefixing (LSP uses `Content-Length` header)
   - Thread-safe queue for incoming messages

2. **Phase 2b (Days 3-4)**: Initialize + file tracking
   - Send `initialize` request on startup
   - Track opened files via `didOpen`
   - Send `didChange` on every edit

3. **Phase 2c (Days 5)**: Feature requests
   - `textDocument/diagnostics` → fetch errors
   - `textDocument/completion` → get completions
   - `textDocument/hover` → get hover info

### Thread Model
- **Main thread**: Renders UI, responds to keyboard
- **LSP thread**: Background worker, blocks on socket recv
- **Queue**: Async message arrival → queued for main thread to process

---

## File 30: completions_popup.asm (500 LOC)

### Purpose
- Display autocomplete list
- Fuzzy filter as user types (Ctrl+Space)
- Insert selected completion + handle snippets

### Architecture

```
Trigger: Ctrl+Space or Auto-trigger on "."
    ↓
LSP_GetCompletions (async)
    ↓
Completions_FilterList (fuzzy match)
    ↓
Completions_ShowPopup (window overlay)
    ↓
User selects item (↑↓ navigation, Enter to insert)
    ↓
Completions_InsertCompletion (edit buffer + snippet processing)
```

### Key Functions

```masm
Completions_Trigger        ; Show popup (Ctrl+Space)
Completions_Update         ; Refresh list as user types
Completions_Filter         ; Fuzzy match against pattern
Completions_ShowPopup      ; Create overlay window
Completions_OnKeyDown      ; Handle arrow keys, enter, escape
Completions_InsertSelected ; Insert selected item
Completions_ProcessSnippet ; Handle ${1:param} style snippets

; Data structures
CompletionItem STRUCT
    label           QWORD ?     ; Display text
    kind            DWORD ?     ; 1=Function, 2=Variable, 3=Class
    detail          QWORD ?     ; Type info
    documentation   QWORD ?     ; Help text
    insertText      QWORD ?     ; Text to insert (may have snippets)
    score           DWORD ?     ; Fuzzy match score (0-100)
CompletionItem ENDS

CompletionList STRUCT
    items           QWORD ?     ; Array of CompletionItem
    count           DWORD ?     ; Number of items
    selectedIndex   DWORD ?     ; Currently highlighted
    filterPattern   QWORD ?     ; User's prefix (for fuzzy match)
CompletionList ENDS
```

### Fuzzy Matching Algorithm

```
Input: "myFunc", items: ["myFunction", "myField", "otherFunc"]
Output: [myFunction (score 95), myField (score 60), otherFunc (score 40)]

Algorithm:
  For each item:
    1. Count consecutive character matches from start
    2. Bonus for camelCase boundaries (m-y-F vs myF)
    3. Penalty for gaps between matches
    4. Score = (matches * 100) / itemLength - penalty
```

### Popup Window

```
┌─ Completions ─────────────────┐
│ 🔷 myFunction       func() → int
│ 🔷 myField          const int
│ 🔷 otherFunc        func() → void
│ 📝 Type to filter   Ctrl+Space to dismiss
└────────────────────────────────┘
```

**Implementation**: Custom window (not native ListBox, to avoid focus issues)

---

## File 31: agent_ipc_bridge.asm (400 LOC)

### Purpose
- IPC to existing agent process (co-process model)
- Send code generation requests
- Receive completions + apply hotpatches

### Architecture

```
IDE Process (MASM)
    ↓ [Named Pipe: \\.\pipe\rawrxd-agent]
    ↓
Agent Process (C++ hotpatcher + failure detector)
    ↓ [Response: JSON with generated code + metadata]
    ↓
Render + Apply Hotpatches
```

### Key Functions

```masm
AgentBridge_Connect         ; Open named pipe to agent process
AgentBridge_RequestCompletion  ; Send code completion request
AgentBridge_RequestFix      ; Send "fix this error" request
AgentBridge_RequestTest     ; Send "write test for this" request
AgentBridge_RequestDocs     ; Send "document this function" request
AgentBridge_ProcessResponse ; Parse response JSON
AgentBridge_ApplyHotpatch   ; Call ProxyHotpatcher or ByteLevelHotpatcher
AgentBridge_Disconnect      ; Close named pipe

; IPC Message Format (JSON)
; Request:
{
  "request_id": 1,
  "type": "code_completion",
  "context": {
    "file": "C:\\code\\main.cpp",
    "line": 42,
    "column": 10,
    "prefix": "my"
  },
  "model": "gpt-4",
  "temperature": 0.3
}

; Response:
{
  "request_id": 1,
  "success": true,
  "completions": [
    {"text": "myFunction()", "score": 0.95},
    {"text": "myVariable", "score": 0.82}
  ],
  "generated_code": "int myFunction() { return 42; }",
  "hotpatch_applied": true
}
```

### Thread Model
- **Main thread**: Sends requests, doesn't block
- **IPC thread**: Background reader, async notification via PostMessage
- **Callback**: WM_APP + 1 message when response arrives

### Hotpatcher Integration
```masm
; After agent generates code, apply corrections
IF response.success:
    1. Call AgenticPuppeteer_Correct (failure detection)
    2. Call ProxyHotpatcher_Apply (live correction)
    3. Insert code into buffer
    4. Update tab as dirty
    5. Invalidate token block
```

---

## File 32: theme_system.asm (400 LOC)

### Purpose
- Load JSON theme files
- Map token type → color
- Support dark/light mode switching + dynamic reload

### Key Functions

```masm
Theme_Load              ; Load theme from JSON file
Theme_Apply             ; Apply theme to current editor
Theme_SetDarkMode       ; Switch to dark theme
Theme_SetLightMode      ; Switch to light theme
Theme_GetColor          ; Lookup color for token type
Theme_GetStyle          ; Get font style (bold, italic, etc.)
Theme_RegisterCustom    ; Load user theme from plugins/themes/

; Data structures
ThemeColor STRUCT
    tokenType       DWORD ?     ; 1=Keyword, 2=String, etc.
    foreground      DWORD ?     ; BGRA color
    background      DWORD ?     ; Background color
    bold            BYTE ?      ; Font style
    italic          BYTE ?      ; Font style
    underline       BYTE ?      ; Font style
ThemeColor ENDS

Theme STRUCT
    name            QWORD ?     ; "default-dark"
    colors          QWORD ?     ; Array of ThemeColor
    colorCount      DWORD ?     ; Number of colors
    editorBg        DWORD ?     ; Editor background
    gutterBg        DWORD ?     ; Gutter background
    selectionColor  DWORD ?     ; Selection highlight
    errorColor      DWORD ?     ; Diagnostic error
    warningColor    DWORD ?     ; Diagnostic warning
Theme ENDS
```

### JSON Theme Format
```json
{
  "name": "default-dark",
  "editor": {
    "background": "#1E1E1E",
    "foreground": "#D4D4D4",
    "gutter": "#252526",
    "selection": "#264F78",
    "error": "#F48771",
    "warning": "#FFD700"
  },
  "tokens": {
    "keyword": {"color": "#569CD6", "bold": true},
    "string": {"color": "#CE9178"},
    "comment": {"color": "#6A9955", "italic": true},
    "number": {"color": "#B5CEA8"}
  }
}
```

### Built-in Themes
1. **default-dark** (Monokai-like)
2. **default-light** (Classic white background)
3. **solarized-dark**
4. **solarized-light**

### Dynamic Reload
```masm
; On Ctrl+Shift+T (example):
Theme_Load, OFFSET szThemeName
Theme_Apply
InvalidateRect(hMainWnd, NULL, FALSE)  ; Redraw everything
```

---

## Integration Sequence

### Day 1: Session Management
```
1. Implement Session_Initialize (load session.json)
2. Create 30s timer in main window
3. Implement Session_SaveSnapshot (serialize tabs)
4. Test: Open file, restart IDE, verify tab reopens
```

### Days 2-3: LSP Client
```
1. TCP socket creation + connection
2. Message framing (Content-Length header)
3. JSON-RPC initialization handshake
4. File tracking (didOpen, didChange)
5. Test: Connect to clangd, send/receive messages
```

### Days 4-5: Completions + Theme
```
1. Completions_Trigger on Ctrl+Space
2. Fuzzy filtering algorithm
3. Popup window + keyboard navigation
4. Insert selected completion
5. Theme loading + dynamic switching
```

### Days 6-7: Agent Bridge
```
1. Named pipe connection to agent process
2. Request/response marshalling
3. Hotpatcher integration
4. Error handling + reconnection logic
5. End-to-end testing: Generate code → apply patch
```

---

## Testing Checklist

### Session Management ✅
- [ ] Open 3 files, close IDE, reopen → files restore
- [ ] Edit file, close IDE without saving, reopen → dirty flag preserved
- [ ] Auto-save every 30s → session.json timestamps advance
- [ ] Crash scenario → session.bak recovery works

### LSP Client ✅
- [ ] Connect to clangd (must be running: `clangd --listen 127.0.0.1:6009`)
- [ ] didOpen message received by clangd
- [ ] didChange on every keystroke
- [ ] Diagnostics returned → displayed as error squiggles
- [ ] Hover request → tooltip displayed

### Completions ✅
- [ ] Ctrl+Space shows popup
- [ ] Type "my" → filter to "myFunction", "myVariable"
- [ ] Arrow keys navigate list
- [ ] Enter inserts selected item
- [ ] Snippets with ${1:param} processed correctly

### Theme ✅
- [ ] Load dark theme → colors correct
- [ ] Switch to light theme → colors change
- [ ] Custom theme from plugins/themes/ loads
- [ ] Dynamic reload (Ctrl+Shift+T) re-renders instantly

### Agent Bridge ✅
- [ ] Named pipe connection opens
- [ ] Request JSON formatted correctly
- [ ] Response parsed
- [ ] Generated code inserted into buffer
- [ ] Hotpatches applied to running model

---

## Estimated Effort

| File | LOC | Days | Difficulty |
|------|-----|------|------------|
| 28: Session | 350 | 1 | Easy (JSON parsing) |
| 29: LSP Client | 800 | 2 | Medium (network I/O, JSON-RPC) |
| 30: Completions | 500 | 1.5 | Medium (fuzzy matching) |
| 31: Agent Bridge | 400 | 1 | Medium (IPC, hotpatcher API) |
| 32: Theme | 400 | 1.5 | Easy (file I/O, color lookup) |
| **Total** | **2,450** | **7** | **Medium** |

---

## Success Criteria

After Phase 2, the IDE should have:

✅ **Session persistence** - Restart and restore work  
✅ **LSP integration** - Diagnostics appear in red squiggles  
✅ **Autocomplete** - Ctrl+Space shows fuzzy-filtered list  
✅ **Code generation** - Agent requests work, code inserted  
✅ **Theming** - Dark/light modes + custom themes  
✅ **Production-ready** - No stubs, error handling, thread-safe

**At that point**, the pure MASM IDE is feature-complete and matches Cursor/Copilot capability.

