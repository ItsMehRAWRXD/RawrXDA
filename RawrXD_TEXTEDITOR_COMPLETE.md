# RawrXD Text Editor - Complete Implementation Guide

## Overview

The RawrXD Text Editor is a **production-grade text editing engine** written in **x64 MASM**, providing:

- **Efficient text buffer management** with dynamic allocation
- **Accurate cursor position tracking** (line, column, byte offset)
- **Complete keyboard navigation** (arrows, Home, End, Page Up/Down)
- **Text selection & highlighting** with multi-line support
- **Cursor blinking animation** with configurable rate
- **Win32 GUI rendering** with GDI double-buffering
- **Mouse input handling** for click-to-position
- **Line number display** with proper formatting

## Architecture

### Component Stack

```
┌─────────────────────────────────────────┐
│    RawrXD_TextEditor_Main.asm (API)    │  Main entry point & high-level orchestration
├─────────────────────────────────────────┤
│  RawrXD_TextEditorGUI.asm (GUI Layer)   │  Win32 window, rendering, input handling
├─────────────────────────────────────────┤
│ RawrXD_CursorTracker.asm (Navigation)   │  Cursor position & selection management
├─────────────────────────────────────────┤
│   RawrXD_TextBuffer.asm (Data Layer)    │  Text storage & manipulation
└─────────────────────────────────────────┘
            ↓
      Win32 API (kernel32, user32, gdi32, comctl32)
```

### Memory Layout

Each editor instance consists of three main structures:

#### 1. Text Buffer (2080 bytes)
```
Offset  Size    Field
0       8       data_ptr        → allocated text buffer (64KB initial)
8       8       capacity        → buffer capacity in bytes
16      8       length          → current text length
24      8       line_count      → total number of lines
32      2048    line_table[256] → byte offset of each line start
```

#### 2. Cursor State (96 bytes)
```
Offset  Size    Field
0       8       byte_offset     → current position in text
8       8       line            → current line number
16      8       column          → current column in line
24      8       sel_start       → selection start (-1 = no selection)
32      8       sel_end         → selection end (-1 = no selection)
40      8       last_line       → last line index (for bounds)
48      4       blink_state     → 0=off, 1=on (blinking)
52      4       blink_timer     → milliseconds elapsed (0-500)
56      8       buffer_ptr      → reference to buffer
```

#### 3. Editor Window (96 bytes)
```
Offset  Size    Field
0       8       hwnd            → window handle
8       8       hdc             → device context
16      8       hfont           → font handle
24      8       cursor_ptr      → reference to cursor
32      8       buffer_ptr      → reference to buffer
40      4       char_width      → width per character (pixels)
44      4       char_height     → height per character (pixels)
48      4       client_width    → window width
52      4       client_height   → window height
56      4       line_num_width  → reserved for line numbers
60      4       scroll_offset_x → horizontal scroll
64      4       scroll_offset_y → vertical scroll
68      8       hbitmap         → backbuffer bitmap
76      8       hmemdc          → backbuffer device context
84      4       timer_id        → blink timer ID
```

## Core APIs

### Text Buffer Functions

#### `TextBuffer_Initialize(rcx = buffer_ptr) → rax`
Initialize text buffer structure with 64KB capacity.

**Parameters:**
- `rcx` = buffer structure pointer

**Returns:**
- `rax` = 1 (success), 0 (failure)

**Example:**
```asm
lea rcx, [buffer_struct]
call TextBuffer_Initialize
```

---

#### `TextBuffer_InsertChar(rcx = buffer, rdx = position, r8b = char) → rax`
Insert single character at byte offset.

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = insertion byte offset
- `r8b` = character code

**Returns:**
- `rax` = 1 (success), 0 (failure)

---

#### `TextBuffer_InsertString(rcx = buffer, rdx = position, r8 = src_ptr, r9 = length) → rax`
Insert string of bytes at position.

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = insertion byte offset
- `r8` = source string pointer
- `r9` = bytes to insert

**Returns:**
- `rax` = 1 (success), 0 (failure)

---

#### `TextBuffer_DeleteChar(rcx = buffer, rdx = position) → rax`
Delete character at byte offset.

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = deletion byte offset

**Returns:**
- `rax` = 1 (success), 0 (failure)

---

#### `TextBuffer_GetLine(rcx = buffer, rdx = line_no, r8 = dest_ptr, r9 = dest_size) → rax`
Extract entire line (without newline).

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = line number (0-based)
- `r8` = destination buffer pointer
- `r9` = destination buffer size

**Returns:**
- `rax` = bytes copied

---

#### `TextBuffer_GetLineColumn(rcx = buffer, rdx = byte_offset) → rax, rdx`
Convert byte offset to (line, column).

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = byte offset in text

**Returns:**
- `rax` = line number
- `rdx` = column in line

---

#### `TextBuffer_GetOffsetFromLineColumn(rcx = buffer, rdx = line, r8 = column) → rax`
Convert (line, column) to byte offset.

**Parameters:**
- `rcx` = buffer pointer
- `rdx` = line number
- `r8` = column in line

**Returns:**
- `rax` = byte offset (or -1 if out of bounds)

---

### Cursor Navigation Functions

#### `Cursor_Initialize(rcx = cursor_ptr, rdx = buffer_ptr) → rax`
Initialize cursor at position (0, 0).

---

#### `Cursor_MoveLeft(rcx = cursor) → rax`
Move cursor left by one position. Bounds-checked.

**Returns:**
- `rax` = 1 (moved), 0 (already at start)

---

#### `Cursor_MoveRight(rcx = cursor, rdx = buffer_length) → rax`
Move cursor right by one position. Bounds-checked.

---

#### `Cursor_MoveUp(rcx = cursor, rdx = buffer_ptr) → rax`
Move up one line, maintaining column if possible.

---

#### `Cursor_MoveDown(rcx = cursor, rdx = buffer_ptr) → rax`
Move down one line, maintaining column.

---

#### `Cursor_MoveHome(rcx = cursor) → rax`
Move to beginning of current line.

---

#### `Cursor_MoveEnd(rcx = cursor) → rax`
Move to end of current line.

---

#### `Cursor_MoveStartOfDocument(rcx = cursor) → rax`
Jump to start of document (0, 0).

---

#### `Cursor_MoveEndOfDocument(rcx = cursor, rdx = buffer_ptr) → rax`
Jump to end of document.

---

#### `Cursor_PageUp(rcx = cursor, rdx = buffer, r8 = lines_per_page) → rax`
Scroll up by lines_per_page lines.

---

#### `Cursor_PageDown(rcx = cursor, rdx = buffer, r8 = lines_per_page) → rax`
Scroll down by lines_per_page lines.

---

### Selection Functions

#### `Cursor_SelectTo(rcx = cursor, rdx = end_offset) → rax`
Set selection from current position to end_offset.

**Returns:**
- `rax` = 1

---

#### `Cursor_ClearSelection(rcx = cursor) → rax`
Deselect (clear active selection).

---

#### `Cursor_IsSelected(rcx = cursor) → rax`
Check if text is selected.

**Returns:**
- `rax` = 1 (selected), 0 (no selection)

---

#### `Cursor_GetSelection(rcx = cursor) → rax, rdx`
Get selection start and end offsets.

**Returns:**
- `rax` = selection start (-1 if no selection)
- `rdx` = selection end (-1 if no selection)

---

### Cursor Blinking Functions

#### `Cursor_SetBlink(rcx = cursor, rdx = state) → rax`
Set cursor blink state (0=off, 1=on).

---

#### `Cursor_UpdateBlink(rcx = cursor, rdx = delta_ms) → rax`
Update blink animation. Call every frame.

**Parameters:**
- `rdx` = milliseconds elapsed since last update

---

#### `Cursor_GetBlink(rcx = cursor) → rax`
Get current blink state.

**Returns:**
- `rax` = 0 (cursor off), 1 (cursor on)

---

### GUI Functions

#### `EditorWindow_Create(rcx = editor_ptr, rdx = title_ptr) → rax`
Create editor window with default metrics.

---

#### `EditorWindow_HandlePaint(rcx = editor) → rax`
Redraw window (called on WM_PAINT).

Renders:
1. Line numbers
2. Text content
3. Selection highlight
4. Cursor

---

#### `EditorWindow_HandleKeyDown(rcx = editor, rdx = vkCode) → rax`
Process keyboard input.

**Supported Keys:**
- Arrow keys (navigation)
- Home/End (line boundaries)
- Page Up/Page Down (scroll)
- Backspace/Delete (text removal)

---

#### `EditorWindow_HandleChar(rcx = editor, rdx = char_code) → rax`
Insert character at cursor (from WM_CHAR).

---

#### `EditorWindow_HandleMouseClick(rcx = editor, rdx = x, r8 = y) → rax`
Position cursor at clicked location.

**Parameters:**
- `rdx` = screen X coordinate
- `r8` = screen Y coordinate

---

#### `EditorWindow_ScrollToCursor(rcx = editor) → rax`
Ensure cursor is visible by scrolling if needed.

---

### High-Level API (Main Entry Point)

#### `TextEditor_Create() → rax`
Create and initialize a complete editor instance.

**Returns:**
- `rax` = editor handle

---

#### `TextEditor_Destroy(rcx = editor_handle) → rax`
Clean up and free editor instance.

---

#### `TextEditor_InsertText(rcx = editor, rdx = text_ptr, r8 = length) → rax`
Insert text at current cursor position.

---

#### `TextEditor_GetCursorPosition(rcx = editor) → rax, rdx`
Get current cursor line and column.

**Returns:**
- `rax` = line number
- `rdx` = column

---

#### `TextEditor_SetCursorPosition(rcx = editor, rdx = line, r8 = column) → rax`
Move cursor to specific location.

---

#### `TextEditor_GetTextContent(rcx = editor) → rax, rdx`
Get entire text buffer.

**Returns:**
- `rax` = text pointer
- `rdx` = text length

---

#### `TextEditor_Clear(rcx = editor) → rax`
Clear all text and reset cursor.

---

## Keyboard Navigation Map

| Key | Action |
|-----|--------|
| ← | Move cursor left |
| → | Move cursor right |
| ↑ | Move cursor up (maintain column) |
| ↓ | Move cursor down (maintain column) |
| Home | Start of current line |
| End | End of current line |
| Ctrl+Home | Start of document |
| Ctrl+End | End of document |
| Page Up | Scroll up 10 lines |
| Page Down | Scroll down 10 lines |
| Backspace | Delete previous character |
| Delete | Delete character at cursor |
| Shift+Arrow | Select text |
| Printable chars | Insert character |

---

## Building

### Prerequisites
- ml64.exe (MASM x64 assembler)
- link.exe (Microsoft linker)
- Windows SDK (kernel32.lib, user32.lib, gdi32.lib, comctl32.lib)

### Compilation

```powershell
# From d:\rawrxd\
.\Build_TextEditor.ps1

# Output: .\build\text-editor\RawrXD_TextEditor.exe
```

### Manual Build
```bash
# Assemble modules
ml64 /c /D_AMD64_ RawrXD_TextBuffer.asm
ml64 /c /D_AMD64_ RawrXD_CursorTracker.asm
ml64 /c /D_AMD64_ RawrXD_TextEditorGUI.asm
ml64 /c /D_AMD64_ RawrXD_TextEditor_Main.asm

# Link
link /SUBSYSTEM:WINDOWS /ENTRY:main \
   RawrXD_TextBuffer.obj \
   RawrXD_CursorTracker.obj \
   RawrXD_TextEditorGUI.obj \
   RawrXD_TextEditor_Main.obj \
   kernel32.lib user32.lib gdi32.lib comctl32.lib
```

---

## Implementation Details

### Efficient Text Storage

The buffer uses **linear byte-addressed storage** with a **line offset table** for O(1) line lookup:

- **Insert/Delete**: O(n) where n = buffer size (shifted region only)
- **Line Lookup**: O(1) via line table (cached)
- **Position Queries**: O(log n) binary search on line table

### Cursor Blinking

Implemented as **hardware-independent timer**:
- 500ms on, 500ms off cycle
- Blink state toggled every 500 calls to `Cursor_UpdateBlink`
- Call with `delta_ms = 16` (60 FPS) for smooth animation

### Line Tracking

Line offsets cached in buffer header (supports up to 256 lines efficiently):

```asm
line_table[0] = 0
line_table[1] = (offset of first \n) + 1
line_table[2] = (offset of second \n) + 1
...
```

Automatic update on insert/delete of newline characters.

### Selection Highlighting

Selection stored as **[start_offset, end_offset]** pair:

- `sel_start = -1` → no selection
- `sel_start == sel_end` → zero-width selection (just cursor)
- `sel_start < sel_end` → forward selection
- `sel_start > sel_end` → backward selection (user dragging leftward)

Renderer draws semi-transparent blue box over selected region.

### Double-Buffered Rendering

GUI module maintains backbuffer (memory DC + bitmap) for flicker-free rendering:

1. Clear back-to-white
2. Render all content to memory DC
3. BitBlt to screen DC
4. Swap buffers

---

## Integration Examples

### Embedding in RawrXD IDE

```asm
; Create editor for REPL/chat output
call TextEditor_Create              ; rax = editor handle
mov [g_EditorHandle], rax

; Insert model inference output
mov rcx, [g_EditorHandle]
lea rdx, [szModelOutput]
mov r8, szModelOutput_len
call TextEditor_InsertText

; Query cursor (to highlight agentic steps)
mov rcx, [g_EditorHandle]
call TextEditor_GetCursorPosition   ; rax = line, rdx = column

; Position at error (from telemetry)
mov rcx, [g_EditorHandle]
mov rdx, [error_line]
mov r8, [error_column]
call TextEditor_SetCursorPosition
```

### Real-Time Telemetry Display

```asm
; Clear old output
mov rcx, [g_EditorHandle]
call TextEditor_Clear

; Insert telemetry JSON with live cursor
mov rcx, [g_EditorHandle]
call TextEditor_InsertText          ; Insert phase info
...
call EditorWindow_HandlePaint       ; Update display
call Cursor_UpdateBlink             ; Blink cursor
```

---

## Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|-----------------|-------|
| Insert char | O(n) | n = text size (shift operation) |
| Delete char | O(n) | n = text size (shift operation) |
| Navigate (arrow) | O(1) | Direct cursor update |
| Goto line/col | O(1) | Via line table |
| Select region | O(1) | Just store offsets |
| Render frame | O(v) | v = visible lines (typically 30-50) |
| Blink update | O(1) | Just increment counter |

**Typical Usage:**
- 64KB text buffer
- 256 lines max
- 60 FPS rendering
- 16ms/frame budget → comfortable for single-threaded UI

---

## Testing Checklist

- [x] Text insertion at various positions
- [x] Text deletion forward/backward
- [x] Multi-line navigation
- [x] Cursor bounds checking
- [x] Selection creation/clearing
- [x] Selection display in rendering
- [x] Blink animation
- [x] Line number rendering
- [x] Horizontal/vertical scrolling
- [x] Mouse click positioning
- [x] Keyboard combinations (Shift+Arrow, Ctrl+Home, etc.)
- [x] Edge cases (empty buffer, single line, cursor at EOF)

---

## Future Enhancements

- [ ] Syntax highlighting (tokenizer + color map)
- [ ] Code folding (collapse/expand regions)
- [ ] Search & replace (regex support)
- [ ] Undo/redo stack (transaction log)
- [ ] Multi-cursor selection (Ctrl+D)
- [ ] Bracket matching and auto-pairing
- [ ] Copy/paste integration (clipboard)
- [ ] Smooth scrolling animation
- [ ] Minimap (zoomed view of entire document)
- [ ] Plugin API for extensions

---

## File Structure

```
d:\rawrxd\
├── RawrXD_TextBuffer.asm         (Core text storage - 1200 LOC)
├── RawrXD_CursorTracker.asm      (Cursor navigation - 800 LOC)
├── RawrXD_TextEditorGUI.asm      (Win32 GUI rendering - 900 LOC)
├── RawrXD_TextEditor_Main.asm    (Entry point + API - 500 LOC)
├── Build_TextEditor.ps1          (Build orchestration)
├── RawrXD_TEXTEDITOR_COMPLETE.md (This document)
└── build/
    └── text-editor/
        ├── RawrXD_TextBuffer.obj
        ├── RawrXD_CursorTracker.obj
        ├── RawrXD_TextEditorGUI.obj
        ├── RawrXD_TextEditor_Main.obj
        └── RawrXD_TextEditor.exe        (Final executable)
```

---

## License & Attribution

RawrXD Text Editor - x64 MASM Implementation  
© 2026 - Pure native code, zero managed dependencies

