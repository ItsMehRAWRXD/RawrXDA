# RawrXD Text Editor - Quick Reference Card

## Files & Modules

| Module | Size | Purpose |
|--------|------|---------|
| `RawrXD_TextBuffer.asm` | 1200 LOC | Text storage, line indexing |
| `RawrXD_CursorTracker.asm` | 800 LOC | Cursor position, navigation, selection |
| `RawrXD_TextEditorGUI.asm` | 900 LOC | Win32 window, rendering, input |
| `RawrXD_TextEditor_Main.asm` | 500 LOC | High-level API, orchestration |
| `Build_TextEditor.ps1` | 120 LOC | Automated build script |

## Data Structures

### Text Buffer (2080 bytes)
```
[0-8]   data_ptr        → 64KB text buffer
[8-16]  capacity        → buffer size limit
[16-24] length          → bytes used
[24-32] line_count      → number of lines
[32+]   line_table[256] → byte offset per line
```

### Cursor (96 bytes)
```
[0-8]   byte_offset     → position in text
[8-16]  line            → line number
[16-24] column          → column in line
[24-32] sel_start       → selection start
[32-40] sel_end         → selection end
[40-48] last_line       → bounds
[48-52] blink_state     → 0/1
[52-56] blink_timer     → 0-500ms
[56-64] buffer_ptr      → reference
```

### Editor Window (96 bytes)
```
[0-8]    hwnd            → window handle
[8-16]   hdc             → device context
[16-24]  hfont           → font handle
[24-32]  cursor_ptr      → reference
[32-40]  buffer_ptr      → reference
[40-44]  char_width      → pixels per char
[44-48]  char_height     → pixels per line
[48-52]  client_width    → window width
[52-56]  client_height   → window height
[56-60]  line_num_width  → reserved
[60-64]  scroll_offset_x → horizontal scroll
[64-68]  scroll_offset_y → vertical scroll
[68+]    rest            → device contexts
```

## Essential APIs

### Initialization
```asm
; Create editor (all-in-one)
call TextEditor_Create              ; → rax = handle

; Or manual initialization
lea rcx, [buffer]
call TextBuffer_Initialize          ; Initialize text storage

lea rcx, [cursor]
mov rdx, buffer_ptr
call Cursor_Initialize              ; Initialize cursor at (0,0)

lea rcx, [editor_window]
call EditorWindow_Create            ; Create GUI
```

### Text Operations
```asm
; Insert string at cursor
mov rcx, [editor_handle]
lea rdx, [text]
mov r8, length
call TextEditor_InsertText

; Insert at specific position (raw)
mov rcx, [buffer_ptr]
mov rdx, byte_offset
lea r8, [string]
mov r9, length
call TextBuffer_InsertString

; Delete character
mov rcx, [buffer_ptr]
mov rdx, position
call TextBuffer_DeleteChar
```

### Navigation
```asm
mov rcx, [cursor_ptr]               ; Load cursor
mov rdx, [buffer_ptr]               ; And buffer

call Cursor_MoveLeft                ; ← (if not at start)
call Cursor_MoveRight               ; → (if not at end)
call Cursor_MoveUp                  ; ↑ (if not at line 0)
call Cursor_MoveDown                ; ↓ (if not at last line)
call Cursor_MoveHome                ; Home key
call Cursor_MoveEnd                 ; End key
call Cursor_MoveStartOfDocument     ; Ctrl+Home
call Cursor_MoveEndOfDocument       ; Ctrl+End
call Cursor_PageUp                  ; Page Up
call Cursor_PageDown                ; Page Down
```

### Cursor Position Queries
```asm
; Get current position
mov rcx, [editor_handle]
call TextEditor_GetCursorPosition   ; → rax=line, rdx=column

; Set specific position
mov rcx, [editor_handle]
mov rdx, target_line
mov r8, target_column
call TextEditor_SetCursorPosition
```

### Selection
```asm
mov rcx, [cursor_ptr]

; Start selection
mov rdx, end_offset
call Cursor_SelectTo                ; Select from current to rdx

; Check if selected
call Cursor_IsSelected              ; → rax = 1/0

; Get selection range
call Cursor_GetSelection            ; → rax=start, rdx=end

; Clear selection
call Cursor_ClearSelection
```

### Blinking
```asm
mov rcx, [cursor_ptr]

; Enable/disable
mov edx, 1                          ; or 0
call Cursor_SetBlink

; Update animation (call every frame)
mov edx, 16                         ; 16ms per frame
call Cursor_UpdateBlink

; Check if cursor should be drawn
call Cursor_GetBlink                ; → rax = 0 (off) or 1 (on)
```

### Rendering & Input
```asm
mov rcx, [editor_window]

; Redraw entire window
call EditorWindow_HandlePaint

; Process keyboard
mov edx, vkCode                     ; VK_* constant
call EditorWindow_HandleKeyDown

; Process character input
mov edx, char_code
call EditorWindow_HandleChar

; Process mouse click
mov edx, screen_x
mov r8, screen_y
call EditorWindow_HandleMouseClick
```

## Keyboard Map

| Key | vkCode | Function |
|-----|--------|----------|
| ← | 37 | Move left |
| → | 39 | Move right |
| ↑ | 38 | Move up |
| ↓ | 40 | Move down |
| Home | 36 | Line start |
| End | 35 | Line end |
| Page Up | 33 | Scroll up 10 |
| Page Down | 34 | Scroll down 10 |
| Backspace | 8 | Delete prev |
| Delete | 46 | Delete curr |
| Printable | 32-126 | Insert char |

## Common Patterns

### Full Lifecycle
```asm
; Create
call TextEditor_Create
mov [hEditor], rax

; Use (loop)
mov rcx, [hEditor]
lea rdx, [text]
mov r8, length
call TextEditor_InsertText

call EditorWindow_HandlePaint

; Blink
mov rcx, [cursor_ptr]
mov edx, 16
call Cursor_UpdateBlink

; Query
call TextEditor_GetCursorPosition
; rax = line, rdx = column

; Cleanup
mov rcx, [hEditor]
call TextEditor_Destroy
```

### Real-Time Output
```asm
; In token-per-frame loop:
.Output:
    ; Get token
    lea rdx, [token]
    mov r8, 1
    mov rcx, [editor_handle]
    call TextEditor_InsertText      ; Add to display
    
    ; Render
    lea rcx, [editor_window]
    call EditorWindow_HandlePaint
    
    ; Animate
    mov rcx, [cursor_ptr]
    mov edx, 16
    call Cursor_UpdateBlink
    
    ; Next frame
    jmp .Output
```

### Error Highlighting
```asm
; Position at error
mov rcx, [editor_handle]
mov rdx, error_line
mov r8, error_column
call TextEditor_SetCursorPosition

; Blink to highlight
mov rcx, [cursor_ptr]
mov edx, 1
call Cursor_SetBlink

; Render
lea rcx, [editor_window]
call EditorWindow_HandlePaint
```

## Return values

| Function | Success | Failure |
|----------|---------|---------|
| TextBuffer_Initialize | rax=1 | rax=0 |
| TextBuffer_InsertChar | rax=1 | rax=0 |
| TextBuffer_InsertString | rax=1 | rax=0 |
| TextBuffer_DeleteChar | rax=1 | rax=0 |
| Cursor_MoveLeft | rax=1 | rax=0 (at start) |
| Cursor_MoveRight | rax=1 | rax=0 (at end) |
| Cursor_MoveUp | rax=1 | rax=0 (at top) |
| Cursor_MoveDown | rax=1 | rax=0 (at bottom) |
| Cursor_IsSelected | rax=1 (yes) | rax=0 (no) |
| Cursor_GetBlink | rax=1 (on) | rax=0 (off) |

## Stack Requirements

Per editor instance:
```
2080 bytes - Text buffer structure
96 bytes   - Cursor structure
96 bytes   - Editor window structure
64 KB      - Text data (allocated separately)
─────────────────────────────────────
~2,272 bytes stack + heap
```

Total per instance: ~66 KB (mostly text buffer allocation)

## Performance Tips

1. **Don't redraw every insert** - batch text updates before rendering
2. **Use line table** - gives O(1) access to line starts
3. **Cache cursor position** - don't recalculate after every keystroke
4. **Double-buffer rendering** - included automatically
5. **Call blink update at 60 FPS** - not faster

## Common Mistakes

❌ Don't forget to update cursor after insert
```asm
call TextBuffer_InsertChar
; Missing: call Cursor_MoveRight
```

❌ Don't check bounds manually (API does it)
```asm
; WRONG: checking manually
cmp [cursor + 0], buffer_length
jge .CantMove

; RIGHT: let API check
call Cursor_MoveRight           ; Returns 0 if at end
```

❌ Don't invalidate window after every keystroke
```asm
; BETTER: group updates
call TextBuffer_InsertChar
call TextBuffer_InsertChar
call TextBuffer_InsertChar
call EditorWindow_HandlePaint   ; One refresh for 3 chars
```

## Building

```powershell
cd d:\rawrxd
.\Build_TextEditor.ps1

# Output: .\build\text-editor\RawrXD_TextEditor.exe
```

## File Locations

- Modules: `d:\rawrxd\RawrXD_*.asm`
- Build script: `d:\rawrxd\Build_TextEditor.ps1`
- Documentation: `d:\rawrxd\RawrXD_*.md`
- Output: `d:\rawrxd\build\text-editor\*`

---

**For full documentation, see:**
- [RawrXD_TEXTEDITOR_COMPLETE.md](RawrXD_TEXTEDITOR_COMPLETE.md) - Complete API reference
- [RawrXD_TextEditor_INTEGRATION_GUIDE.md](RawrXD_TextEditor_INTEGRATION_GUIDE.md) - Integration examples

