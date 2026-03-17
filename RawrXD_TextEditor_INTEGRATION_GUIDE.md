# RawrXD Text Editor - Integration Guide

## Quick Start

### 1. Basic Usage

```asm
; Create an editor instance
call TextEditor_Create              ; Returns editor handle in rax
mov [hEditor], rax

; Initialize components
lea rcx, [buffer_struct]
call TextBuffer_Initialize

lea rcx, [cursor_struct]
mov rdx, [buffer_ptr]
call Cursor_Initialize

; Link them together
mov rcx, [hEditor]
mov [rcx + 24], cursor_link         ; Store cursor reference
mov [rcx + 32], buffer_link         ; Store buffer reference
```

### 2. Inserting Text

```asm
; Insert text at cursor
mov rcx, [hEditor]                  ; editor handle
lea rdx, [szText]                   ; text pointer
mov r8, strlen                      ; text length
call TextEditor_InsertText

; Or directly:
mov rcx, [buffer_ptr]
mov rdx, [cursor_offset]            ; current byte offset
lea r8, [szNewText]
mov r9, text_len
call TextBuffer_InsertString
```

### 3. Navigation

```asm
mov rcx, [cursor_ptr]

; Move left
call Cursor_MoveLeft

; Move right
mov rcx, [cursor_ptr]
mov rdx, [buffer_length]
call Cursor_MoveRight

; Jump to line/column
mov rcx, [cursor_ptr]
mov rdx, target_line
mov r8, target_column
call TextEditor_SetCursorPosition
```

### 4. Getting Cursor Position

```asm
mov rcx, [editor_handle]
call TextEditor_GetCursorPosition
; Returns: rax = line, rdx = column
```

---

## Integration with RawrXD IDE

### Pattern: Live Token Display in Editor

```asm
; Main loop
.MainLoop:
    ; Get inference token from ML system
    lea rcx, [szNewToken]
    mov rdx, token_len
    
    ; Insert at cursor
    mov rcx, [editor_handle]
    lea rdx, [szNewToken]
    mov r8, token_len
    call TextEditor_InsertText
    
    ; Update display
    lea rcx, [editor_window]
    call EditorWindow_HandlePaint
    
    ; Update blink
    mov rcx, [cursor_ptr]
    mov rdx, 16                      ; 16ms tick (60 FPS)
    call Cursor_UpdateBlink
    
    ; Check for more tokens
    jmp .MainLoop
```

### Pattern: Displaying Telemetry with Cursor Tracking

```asm
; Clear previous output
mov rcx, [editor_handle]
call TextEditor_Clear

; Insert phase information (with cursor at key lines)
lea rdx, [szPhase1Text]
mov r8, phase1_len
call TextEditor_InsertText          ; Cursor now after phase 1

; Display agent outputs
mov rbx, 0
.AgentLoop:
    cmp rbx, agent_count
    jge .AgentsDone
    
    ; Get agent result
    mov rcx, [agents + rbx*8]       ; Get agent handle
    
    ; Insert output
    lea rdx, [agent_output]
    mov r8, output_len
    call TextEditor_InsertText
    
    ; Highlight cursor at successful agent
    mov rcx, [editor_handle]
    call TextEditor_GetCursorPosition
    ; (rax = line with agent output)
    
    inc rbx
    jmp .AgentLoop

.AgentsDone:
    ; Redraw
    lea rcx, [editor_window]
    call EditorWindow_HandlePaint
```

### Pattern: Error Highlighting

```asm
; Find error location from telemetry
mov rdx, [error_line]
mov r8, [error_column]

; Position cursor at error
mov rcx, [editor_handle]
call TextEditor_SetCursorPosition

; Visually blink cursor to draw attention
mov rcx, [cursor_ptr]
mov edx, 1                          ; Force blink on
call Cursor_SetBlink

; Redraw with cursor visible
lea rcx, [editor_window]
call EditorWindow_HandlePaint
```

---

## Data Flow: From Inference to Display

```
AI Model (Ollama)
    ↓ (NDJSON tokens)
RawrXD_InferenceAPI.asm
    ↓ (extracted tokens)
TextEditor_InsertText()
    ↓
TextBuffer_InsertString() ← Updates buffer length
    ↓
Cursor updates line/column
    ↓
EditorWindow_HandlePaint() ← Renders current view
    ↓
Screen Display (Real-time token streaming)
```

---

## Memory Management Strategy

### Stack-Based Instance Storage (Recommended for Single Instance)

```asm
main PROC FRAME
    .ALLOCSTACK 32 + 2080 + 96 + 96  ; buffer + cursor + editor_window
    .ENDPROLOG
    
    sub rsp, 2080 + 96 + 96
    
    ; Initialize at stack locations
    lea rcx, [rsp + 0]               ; buffer at rsp+0
    call TextBuffer_Initialize
    
    lea rcx, [rsp + 2080]            ; cursor at rsp+2080
    mov rdx, rsp
    call Cursor_Initialize
    
    lea rcx, [rsp + 2080 + 96]       ; editor at rsp+2176
    call EditorWindow_Create
    
    ; ... editor operations ...
    
    add rsp, 2080 + 96 + 96
    ret
main ENDP
```

### Heap-Based Instance Storage (Multiple Instances)

```asm
; Create instance
mov rcx, 2080 + 96 + 96 + sys_overhead
call HeapAlloc                      ; Windows heap allocation
mov [pEditor], rax

; Initialize
mov rcx, rax
add rcx, 0
call TextBuffer_Initialize          ; buffer at offset 0

add rcx, rax, 2080
call Cursor_Initialize              ; cursor at offset 2080

; Cleanup
mov rcx, [pEditor]
call HeapFree
```

---

## Real Example: Displaying Autonomy Phase Updates

```asm
; During autonomy stack execution
phase_loop PROC
    ; Phase 1: Decomposition
    lea rdx, [szPhase1Start]
    mov r8, strlen_phase1
    call TextEditor_InsertText
    
    ; Display decomposed goals
    mov rbx, 0
.GoalLoop:
    cmp rbx, goal_count
    jge .GoalsDone
    
    ; Insert goal text
    lea rcx, [editor_handle]
    lea rdx, [goals + rbx*64]
    mov r8, 64
    call TextEditor_InsertText
    
    inc rbx
    jmp .GoalLoop

.GoalsDone:
    ; Phase 2: Swarm initialization
    lea rdx, [szPhase2Start]
    call TextEditor_InsertText
    
    ; Display agent count
    mov rbx, [agent_count]
    ; (Convert to string and insert)
    
    ; Phase 3-6: Similar for each phase
    
    ; Final cursor position at success/error
    mov rcx, [editor_handle]
    mov rdx, final_line
    mov r8, 0
    call TextEditor_SetCursorPosition
    
    ; Blink to highlight result
    mov rcx, [cursor_ptr]
    mov edx, 1
    call Cursor_SetBlink
    
    ret
phase_loop ENDP
```

---

## Rendering Pipeline

### Single Frame Update Sequence

```
1. EditorWindow_HandlePaint()
   ├─ Clear background (white)
   ├─ EditorWindow_RenderLineNumbers()
   │  └─ Draw "1", "2", etc. on left margin
   ├─ EditorWindow_RenderText()
   │  └─ Draw characters with scrolling offset
   ├─ EditorWindow_RenderSelection()
   │  └─ If sel_start != -1: highlight region
   └─ EditorWindow_RenderCursor()
      └─ If blink_state == 1: draw caret line

2. Cursor_UpdateBlink() [called from main loop]
   └─ Increment blink_timer
   └─ If timer > 500ms: toggle blink_state, reset timer

3. Display refreshed (60 FPS recommended)
```

---

## Input Handling Flow

### Keyboard Input (WM_KEYDOWN Message)

```
EditorWindow_HandleKeyDown
├─ VK_LEFT → Cursor_MoveLeft()
├─ VK_RIGHT → Cursor_MoveRight()
├─ VK_UP → Cursor_MoveUp()
├─ VK_DOWN → Cursor_MoveDown()
├─ VK_HOME → Cursor_MoveHome()
├─ VK_END → Cursor_MoveEnd()
├─ VK_BACK → TextBuffer_DeleteChar() + Cursor_MoveLeft()
├─ VK_DELETE → TextBuffer_DeleteChar()
└─ Invalidate window
```

### Character Input (WM_CHAR Message)

```
EditorWindow_HandleChar
├─ Filter control characters
├─ TextBuffer_InsertChar()
├─ Cursor_MoveRight()
└─ Invalidate window
```

### Mouse Input (WM_LBUTTONDOWN Message)

```
EditorWindow_HandleMouseClick
├─ Convert screen (x,y) to character grid position
├─ Calculate line = y / char_height
├─ Calculate column = x / char_width
├─ TextBuffer_GetOffsetFromLineColumn()
└─ Update cursor position and invalidate
```

---

## Performance Tips

### 1. Minimize Redraws

Store dirty rectangle and only redraw changed region:

```asm
; Instead of:
call EditorWindow_HandlePaint       ; Redraws entire window

; Do:
; Track insertion location
mov [dirty_line], current_line
mov [dirty_column], current_column

; Only redraw affected region
call EditorWindow_RenderLineRange(dirty_line, dirty_line+10)
```

### 2. Buffer Large Insertions

Instead of inserting character-by-character:

```asm
; Bad: 100 chars = 100 insert calls
lea rsi, [szText]
mov ecx, 100
.CharLoop:
    lodsb
    mov rcx, [cursor_ptr]
    mov rdx, [rcx]
    mov r8b, al
    call TextBuffer_InsertChar
    call Cursor_MoveRight
    loop .CharLoop

; Good: batch insertion
mov rcx, [buffer_ptr]
mov rdx, [cursor_ptr + 0]
lea r8, [szText]
mov r9, 100
call TextBuffer_InsertString
```

### 3. Cache Line Table

The buffer maintains line offsets automatically. Use for fast jumps:

```asm
; Jump to line 50
mov rcx, [buffer_ptr]
mov rax, [rcx + 32 + 50*8]          ; Direct table lookup O(1)
mov [cursor_ptr + 0], rax            ; Set byte offset
mov [cursor_ptr + 8], 50             ; Set line
mov [cursor_ptr + 16], 0             ; Set column
```

### 4. Update Cursor Blinking Only When Visible

```asm
; Don't update blink if editor window is minimized/hidden
cmp [hwnd_visible], 0
je .SkipBlink

mov rcx, [cursor_ptr]
mov rdx, 16
call Cursor_UpdateBlink

.SkipBlink:
```

---

## Common Pitfalls & Solutions

### Issue: Cursor Moves Beyond EOF

**Solution:** Always bounds-check before calling navigate functions:

```asm
mov rcx, [cursor_ptr]
mov rdx, [buffer_ptr]
mov rdx, [rdx + 16]                 ; Get buffer length
call Cursor_MoveRight               ; SAFE: internally checks vs length
```

### Issue: Selection Highlighting Doesn't Appear

**Solution:** Ensure selection is properly set and renderer checks it:

```asm
; Set selection
mov rcx, [cursor_ptr]
mov rdx, end_offset
call Cursor_SelectTo                ; Sets sel_start and sel_end

; Verify in renderer
mov rax, [rcx + 24]                 ; Load sel_start
cmp rax, -1
je .NoSelection                     ; Should NOT jump if selection is active
```

### Issue: Line Numbers Don't Align with Text

**Solution:** Ensure char_height matches actual font height:

```asm
; In EditorWindow_Create:
mov dword [rcx + 44], 16            ; char_height in pixels

; Must match what GDI renders for your font
; Test: render character and measure bounding box
```

### Issue: Text Doesn't Update Immediately After Insert

**Solution:** Invalidate window to trigger WM_PAINT:

```asm
; After insert:
mov rcx, [buffer_ptr]
mov rdx, position
move r8, char
call TextBuffer_InsertChar

; REQUIRED: Signal GUI to redraw
call InvalidateRect(hwnd, NULL, FALSE)
```

---

## Testing Template

```asm
; Test: Insert, navigate, get position
test_insert_navigate PROC
    ; Create editor
    call TextEditor_Create
    mov [test_editor], rax
    
    ; Insert text
    lea rdx, [szHelloWorld]
    mov r8, 11
    call TextEditor_InsertText
    
    ; Move cursor right 5 times
    mov rcx, [test_editor]
    mov rx_edx, [buffer_length]
    
    mov rbx, 5
.NavLoop:
    call TextEditor_GetCursorPosition
    ; rax = line, rdx = column
    push rdx
    
    call Cursor_MoveRight
    loop .NavLoop
    
    ; Verify position == (0, 5)
    call TextEditor_GetCursorPosition
    cmp rax, 0                       ; Line should be 0
    jne .TestFail
    cmp rdx, 5                       ; Column should be 5
    jne .TestFail
    
    mov rax, 1                       ; TEST PASS
    ret

.TestFail:
    xor rax, rax
    ret
test_insert_navigate ENDP
```

---

## API Cheat Sheet

```asm
; Buffer Operations
TextBuffer_Initialize(rcx=buf_ptr)
TextBuffer_InsertChar(rcx=buf, rdx=pos, r8b=char)
TextBuffer_InsertString(rcx=buf, rdx=pos, r8=str, r9=len)
TextBuffer_DeleteChar(rcx=buf, rdx=pos)
TextBuffer_GetLine(rcx=buf, rdx=line, r8=dest, r9=size) → len
TextBuffer_GetLineColumn(rcx=buf, rdx=offset) → line, col
TextBuffer_GetOffsetFromLineColumn(rcx=buf, rdx=line, r8=col) → offset

; Cursor Navigation
Cursor_Initialize(rcx=cur, rdx=buf)
Cursor_MoveLeft(rcx=cur) → 1/0
Cursor_MoveRight(rcx=cur, rdx=len) → 1/0
Cursor_MoveUp(rcx=cur, rdx=buf) → 1/0
Cursor_MoveDown(rcx=cur, rdx=buf) → 1/0
Cursor_MoveHome(rcx=cur)
Cursor_MoveEnd(rcx=cur)
Cursor_MoveStartOfDocument(rcx=cur)
Cursor_MoveEndOfDocument(rcx=cur, rdx=buf)
Cursor_PageUp(rcx=cur, rdx=buf, r8=lines)
Cursor_PageDown(rcx=cur, rdx=buf, r8=lines)

; Selection
Cursor_SelectTo(rcx=cur, rdx=end_offset)
Cursor_ClearSelection(rcx=cur)
Cursor_IsSelected(rcx=cur) → 1/0
Cursor_GetSelection(rcx=cur) → start, end

; Blinking
Cursor_SetBlink(rcx=cur, rdx=state)
Cursor_UpdateBlink(rcx=cur, rdx=delta_ms)
Cursor_GetBlink(rcx=cur) → 0/1

; GUI
EditorWindow_HandlePaint(rcx=editor)
EditorWindow_HandleKeyDown(rcx=editor, rdx=vkCode)
EditorWindow_HandleChar(rcx=editor, rdx=char)
EditorWindow_HandleMouseClick(rcx=editor, rdx=x, r8=y)

; High-Level
TextEditor_Create() → handle
TextEditor_Destroy(rcx=handle)
TextEditor_InsertText(rcx=handle, rdx=text, r8=len)
TextEditor_GetCursorPosition(rcx=handle) → line, col
TextEditor_SetCursorPosition(rcx=handle, rdx=line, r8=col)
TextEditor_GetTextContent(rcx=handle) → ptr, len
TextEditor_Clear(rcx=handle)
```

---

## Next Steps

1. **Build the project:** `.\Build_TextEditor.ps1`
2. **Integration:** Add editor to RawrXD-IDE for real-time model output
3. **Extensions:** Add syntax highlighting, search/replace, undo/redo
4. **Optimization:** Profile and optimize hot paths (rendering, navigation)

