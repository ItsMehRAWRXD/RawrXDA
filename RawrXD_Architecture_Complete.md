# RawrXD Text Editor - Complete System Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    User Application Layer                        │
│  (IDE Frame / Main Window / User Input)                          │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                 WinProc Dispatch
                       │
        ┌──────────────┼──────────────┐
        │              │              │
    WM_KEYDOWN    WM_PAINT      WM_COMMAND
        │              │              │
        └──────────────┴──────────────┘
                       │
        ┌──────────────┴──────────────────────────────┐
        │   EditorWindow Message Handler (WNDPROC)    │
        └──────────────┬──────────────────────────────┘
                       │
        ┌──────────────┴──────────────┬──────────────┐
        │                             │              │
    RenderPipeline          CursorNavigation   Input Handling
    ─────────────────────────────────────────────────────────
    │                        │                    │
    ├─ClearBackground        ├─MoveLeft           ├─HandleKeyDown
    ├─RenderLineNumbers      ├─MoveRight          ├─HandleChar
    ├─RenderText             ├─MoveUp             ├─HandleMouseClick
    ├─RenderSelection        ├─MoveDown           │
    └─RenderCursor           ├─MoveHome           TextBuffer Ops
                             ├─MoveEnd            │
                             ├─PageUp             ├─InsertChar
                             └─PageDown           └─DeleteChar
                             
                Buffer Structure (2080 bytes)
                ┌─────────────────────────────┐
                │ Text Data (2000 bytes)      │
                ├─────────────────────────────┤
                │ Metadata (24 bytes)         │
                ├─────────────────────────────┤
                │ Line Offset Table (56 bytes)│
                └─────────────────────────────┘
                             │
                Cursor Tracking (96 bytes)
                ┌─────────────────────────────┐
                │ Position (0)                │
                │ Line (8)                    │
                │ Column (16)                 │
                │ Selection Start (24)        │
                │ Selection End (32)          │
                │ Blink Counter (40)          │
                └─────────────────────────────┘
                             │
                AI Token Stream
                ├─ Completion_InsertToken
                ├─ Completion_InsertTokenString
                ├─ Completion_Stream
                └─ Clipboard Integration
                   (Cut/Copy/Paste)
```

## Component Interaction Map

### 1. Initialization Sequence

```
START
  │
  ├─ EditorWindow_RegisterClass()
  │   └─ Creates WNDCLASSA struct
  │   └─ Registers "RawrXDEditor" class
  │
  ├─ EditorWindow_Create(window_ptr, title)
  │   │
  │   ├─ CreateWindowExA(class, title, style, x, y, width, height, ...)
  │   │   └─ Stores hwnd at [window_ptr + 0]
  │   │
  │   ├─ GetDC(hwnd)
  │   │   └─ Stores hdc at [window_ptr + 8]
  │   │
  │   ├─ EditorWindow_CreateMenuBar(window_ptr)
  │   │   ├─ CreateMenu()
  │   │   ├─ CreateMenu() for File submenu
  │   │   ├─ AppendMenuA(hFileMenu, ..., 1001, "Open")
  │   │   ├─ AppendMenuA(hFileMenu, ..., 1002, "Save")
  │   │   ├─ AppendMenuA(hMenuBar, ..., hFileMenu, "File")
  │   │   └─ SetMenu(hwnd, hMenuBar)
  │   │
  │   ├─ EditorWindow_CreateToolbar(window_ptr)
  │   │   └─ CreateWindowExA for button controls
  │   │
  │   ├─ EditorWindow_CreateStatusBar(window_ptr)
  │   │   └─ CreateWindowExA(WC_STATIC, ..., "Ready")
  │   │
  │   └─ SetTimer(hwnd, timer_id, 500, NULL)
  │       └─ Triggers WM_TIMER every 500ms for cursor blink
  │
  ├─ Message Loop
  │   └─ GetMessageA / DispatchMessageA
  │
  └─ END
```

### 2. Paint/Render Sequence

```
WM_PAINT (triggered by InvalidateRect or Timer)
  │
  ├─ BeginPaint()
  │   └─ Get hdc + PAINTSTRUCT
  │
  ├─ CreateCompatibleBitmap(hdc, width, height)
  │   └─ Double-buffer bitmap
  │
  ├─ CreateCompatibleDC(hdc)
  │   └─ Memory DC for off-screen drawing
  │
  ├─ SelectObject(hmemdc, hbitmap)
  │   └─ Attach bitmap to memory DC
  │
  ├─ EditorWindow_ClearBackground(window_ptr)
  │   │
  │   └─ FillRect(hmemdc, &rect, GetStockObject(WHITE_BRUSH))
  │       └─ Clear to white
  │
  ├─ EditorWindow_RenderLineNumbers(window_ptr)
  │   │
  │   ├─ for line = 0 to line_count
  │   │   ├─ TextBuffer_IntToAscii(line + 1, buf, 10)
  │   │   └─ TextOutA(hmemdc, x=5, y=line*16, buf, strlen(buf))
  │   │
  │   └─ Draw line numbers in left margin
  │
  ├─ EditorWindow_RenderText(window_ptr)
  │   │
  │   ├─ SelectObject(hmemdc, hfont)
  │   ├─ SetTextColor(hmemdc, 0x000000)      ; black text
  │   │
  │   ├─ for line = scroll_start to scroll_start + visible_lines
  │   │   │
  │   │   ├─ Get line_offset from [buffer_ptr + 28 + line*8]
  │   │   ├─ Get line_length
  │   │   │
  │   │   └─ for i = 0 to line_length
  │   │       └─ TextOutA(hmemdc, x, y, &text[line_offset + i], 1)
  │   │           Draw each character
  │   │
  │   └─ Draws all visible text
  │
  ├─ EditorWindow_RenderSelection(window_ptr)
  │   │
  │   ├─ if [cursor_ptr + 24] != -1  (selection exists)
  │   │   │
  │   │   ├─ Get selection_start = [cursor_ptr + 24]
  │   │   ├─ Get selection_end = [cursor_ptr + 32]
  │   │   │
  │   │   ├─ Convert offsets to (line, column) pairs
  │   │   │
  │   │   └─ PatBlt(hmemdc, x, y, width, height, SRCAND with blue)
  │   │       Highlight selected region
  │   │
  │   └─ Draw selection highlight if exists
  │
  ├─ EditorWindow_RenderCursor(window_ptr)
  │   │
  │   ├─ if Cursor_GetBlink(cursor_ptr) == ON
  │   │   │
  │   │   ├─ Get cursor_pos = [cursor_ptr + 0]
  │   │   ├─ Convert to (line, column)
  │   │   │
  │   │   └─ MoveToEx(hmemdc, x, y)
  │   │       LineTo(hmemdc, x, y + 16)
  │   │           Draw vertical line at cursor
  │   │
  │   └─ Draw blinking cursor
  │
  ├─ BitBlt(hdc, 0, 0, width, height, hmemdc, 0, 0, SRCCOPY)
  │   └─ Copy double-buffer to screen
  │
  ├─ DeleteObject(hbitmap)
  ├─ DeleteDC(hmemdc)
  │
  └─ EndPaint()
```

### 3. Keyboard Input Sequence

```
WM_KEYDOWN (user presses key)
  │
  ├─ Get vk_code from wParam
  │
  ├─ EditorWindow_HandleKeyDown(window_ptr, vk_code)
  │   │
  │   ├─ if vk_code == VK_LEFT
  │   │   │
  │   │   └─ Cursor_MoveLeft(cursor_ptr, buffer_ptr)
  │   │       │
  │   │       ├─ Get cursor_offset = [cursor_ptr + 0]
  │   │       ├─ if cursor_offset > 0
  │   │       │   └─ cursor_offset--
  │   │       ├─ Store updated offset: [cursor_ptr + 0] = cursor_offset
  │   │       │
  │   │       └─ Update line/column:
  │   │           ├─ [cursor_ptr + 8] = get_line_from_offset(offset)
  │   │           └─ [cursor_ptr + 16] = get_column_in_line(line, offset)
  │   │
  │   ├─ if vk_code == VK_RIGHT
  │   │   └─ Cursor_MoveRight(cursor_ptr, buffer_ptr)
  │   │
  │   ├─ ... (similar for VK_UP, VK_DOWN, VK_HOME, VK_END, VK_PRIOR, VK_NEXT)
  │   │
  │   ├─ if vk_code == VK_BACK
  │   │   │
  │   │   └─ TextBuffer_DeleteChar(buffer_ptr, cursor_offset - 1)
  │   │       │
  │   │       ├─ Get text = [buffer_ptr + 0]
  │   │       ├─ Get length = [buffer_ptr + 16]
  │   │       │
  │   │       ├─ for i = position to length-1
  │   │       │   └─ text[i] = text[i+1]
  │   │       │       Shift content left
  │   │       │
  │   │       ├─ length--
  │   │       └─ [buffer_ptr + 16] = length
  │   │
  │   └─ Calls appropriate navigation or deletion
  │
  ├─ InvalidateRect(hwnd, NULL, FALSE)
  │   └─ Trigger repaint
  │
  └─ end

WM_CHAR (character after key translation)
  │
  ├─ Get char_code from wParam
  │
  ├─ EditorWindow_HandleChar(window_ptr, char_code)
  │   │
  │   └─ TextBuffer_InsertChar(buffer_ptr, cursor_offset, char_value)
  │       │
  │       ├─ Get text = [buffer_ptr + 0]
  │       ├─ Get length = [buffer_ptr + 16]
  │       │
  │       ├─ for i = length to position
  │       │   └─ text[i+1] = text[i]
  │       │       Shift content right
  │       │
  │       ├─ text[position] = char_value
  │       ├─ length++
  │       ├─ [buffer_ptr + 16] = length
  │       │
  │       └─ Cursor_MoveRight(cursor_ptr, buffer_ptr)
  │           Advance cursor
  │
  ├─ InvalidateRect(hwnd, NULL, FALSE)
  │
  └─ end
```

### 4. File Open Sequence

```
User: File > Open (menu command 1001)
  │
  ├─ EditorWindow_FileOpen(window_ptr)
  │   │
  │   ├─ Initialize OPENFILENAMEA struct (120 bytes)
  │   │   ├─ szFile = 256-byte buffer (empty)
  │   │   ├─ nMaxFile = 256
  │   │   ├─ lpstrFilter = "All Files\0*.*\0\0"
  │   │   ├─ hwndOwner = hwnd from [window_ptr + 0]
  │   │   └─ Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
  │   │
  │   └─ GetOpenFileNameA(&ofn)
  │       │
  │       ├─ Shows file picker dialog (blocks until user selects or cancels)
  │       │
  │       └─ Returns TRUE if file selected, FALSE if cancelled
  │
  ├─ if file selected
  │   │
  │   ├─ CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, ...)
  │   │   └─ Returns file handle for reading
  │   │
  │   ├─ GetFileSize(hFile, NULL)
  │   │   └─ Get file size in bytes
  │   │
  │   ├─ ReadFile(hFile, buffer_ptr + 0, file_size, &bytes_read, NULL)
  │   │   └─ Read entire file into text buffer
  │   │
  │   ├─ CloseHandle(hFile)
  │   │
  │   ├─ [buffer_ptr + 16] = bytes_read
  │   │   (Update content length)
  │   │
  │   ├─ Cursor_Initialize(cursor_ptr, buffer_ptr)
  │   │   └─ Set cursor to (0, 0, 0)
  │   │
  │   ├─ InvalidateRect(hwnd, NULL, FALSE)
  │   │
  │   └─ File displayed in editor
  │
  └─ else (cancelled)
      └─ Return NULL
```

### 5. AI Token Streaming Sequence

```
AI Model: Generates "cat"
  │
  ├─ Completion_Stream(buffer_ptr, "cat", 3, cursor_ptr)
  │   │
  │   ├─ token_ptr = "cat"
  │   ├─ for i = 0 to 2  (3 tokens)
  │   │   │
  │   │   ├─ token = token_ptr[i]      ; 'c', then 'a', then 't'
  │   │   │
  │   │   ├─ Completion_InsertToken(buffer_ptr, token, cursor_ptr)
  │   │   │   │
  │   │   │   ├─ get cursor_offset = [cursor_ptr + 0]
  │   │   │   │
  │   │   │   ├─ TextBuffer_InsertChar(buffer_ptr, cursor_offset, token)
  │   │   │   │   │
  │   │   │   │   ├─ Get text = [buffer_ptr + 0]
  │   │   │   │   ├─ Get length = [buffer_ptr + 16]
  │   │   │   │   ├─ Shift right from cursor_offset
  │   │   │   │   ├─ Insert token at cursor_offset
  │   │   │   │   └─ Update length
  │   │   │   │
  │   │   │   └─ Cursor_MoveRight(cursor_ptr, buffer_ptr)
  │   │   │       └─ Increment [cursor_ptr + 0]
  │   │   │
  │   │   └─ (WM_TIMER fires during this loop)
  │   │
  │   └─ All 3 tokens inserted
  │
  ├─ WM_TIMER (500ms cursor blink)
  │   ├─ InvalidateRect(hwnd, NULL, FALSE)
  │   └─ Trigger repaint → user sees "cat" appear character by character
  │
  └─ end
```

## Memory Layout

### EditorWindow (96 bytes)
```
Offset  Size  Field               Purpose
0       8     hwnd                Window handle returned by CreateWindowExA
8       8     hdc                 Device context from GetDC
16      8     hfont               Font handle from GetStockObject
24      8     cursor_ptr          Pointer to cursor structure
32      8     buffer_ptr          Pointer to text buffer structure
40      4     char_width          Width of character in pixels (8)
44      4     char_height         Height of character in pixels (16)
48      4     client_width        Visible area width
52      4     client_height       Visible area height
56      4     line_num_width      Left margin for line numbers (40)
60      4     scroll_offset_x     Horizontal scroll position
64      4     scroll_offset_y     Vertical scroll position
68      8     hbitmap             Double-buffer bitmap handle
76      8     hmemdc              Memory DC for double-buffering
84      4     timer_id            Timer ID from SetTimer
88-95   8     (reserved)          Future use
```

### Cursor (96 bytes)
```
Offset  Size  Field               Purpose
0       8     byte_offset         Absolute position in buffer (0-based)
8       4     line_number         Current line (0-based)
12      4     column_number       Column within line (0-based)
20      8     selection_start     Start offset if selecting (-1 = no selection)
28      8     selection_end       End offset of selection
36      8     blink_counter       Tracks 500ms on/off cycles
44-95   52    (reserved)          Future use (undo history, etc.)
```

### TextBuffer (2080 bytes)
```
Offset  Size  Field               Purpose
0       2000  text_data           Raw text content (up to 2000 bytes)
2000    8     capacity            Allocation size
2008    8     used_length         Current content length
2016    4     line_count          Number of lines
2020    4     (padding)
2024    56    line_offsets[7]     Top 7 line offset table for quick lookup
2080    END
```

## State Machine: Input Processing

```
         ┌─────────────────────────┐
         │   Editing Normal Mode   │
         └────────┬────────────────┘
                  │
          ◄───────┼─────────┐
          │       │         │
      BackSpace  Key    Char
      DeleteKey  Down   Input
          │       │         │
          ▼       ▼         ▼
      ┌──────┐ ┌─────────────┐ ┌──────────────┐
      │Delete│ │Navigation   │ │TextBuffer_   │
      │Char │ │Cursor       │ │InsertChar    │
      └──────┘ └─────────────┘ └──────────────┘
          │       │         │
          └───────┼─────────┘
                  │
         ┌─────────┴────────┐
         │ InvalidateRect   │
         │ (Trigger Paint)  │
         └────────┬─────────┘
                  │
         ┌────────▼─────────┐
         │   WM_PAINT       │
         │  Render All      │
         └──────────────────┘
```

## Error Handling

### File Open Failures
```
GetOpenFileNameA returns FALSE
  → User cancelled dialog
  → Return NULL
  → Display status "Cancelled"

CreateFileA returns INVALID_HANDLE_VALUE
  → File doesn't exist or permission denied
  → Return NULL
  → Display status "Error: Cannot open file"

ReadFile returns 0 bytes
  → File is empty or read failed
  → Proceed with empty buffer
  → Display status "File loaded (empty)"
```

### Buffer Overflow Protection
```
TextBuffer_InsertChar checks:
  if (used_length + 1 > 2000)
      return ERROR_BUFFER_FULL
      
Prevents memory corruption
```

### Navigation Bounds Checking
```
Cursor_MoveLeft checks:
  if (byte_offset == 0)
      Stay at position 0
      
Cursor_MoveRight checks:
  if (byte_offset >= used_length)
      Clamp to end
```

## Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| CreateWindowExA | ~10ms | One-time initialization |
| GetOpenFileNameA | 500ms+ | Blocked UI (modal dialog) |
| ReadFile (1MB) | ~5ms | Sequential disk I/O |
| InsertChar | <0.1ms | O(n) buffer shift, n=2000 |
| RenderFrame (500 lines) | ~16ms | For 60fps target |
| Completion_InsertToken (1 char) | <0.1ms | TextBuffer_InsertChar + move |
| Completion_Stream (100 tokens) | ~10ms | 100 × 0.1ms |
| GetCursorBlink | <0.01ms | GetTickCount modulo |

## Deployment Checklist

### Pre-Compilation
- [ ] All .asm files in same directory
- [ ] No undefined symbols (verify all PROTO declarations)
- [ ] No circular dependencies between files
- [ ] All procedure names match PROTO declarations

### Compilation (ml64.exe)
- [ ] `ml64 /c /Zi /Fo RawrXD_TextEditorGUI.obj RawrXD_TextEditorGUI.asm`
- [ ] `ml64 /c /Zi /Fo RawrXD_TextEditor_Main.obj RawrXD_TextEditor_Main.asm`
- [ ] `ml64 /c /Zi /Fo RawrXD_TextEditor_Completion.obj RawrXD_TextEditor_Completion.asm`
- [ ] All three .obj files created successfully
- [ ] No assembly errors reported

### Linking (link.exe)
- [ ] `link /subsystem:windows /entry:main /debug RawrXD_TextEditorGUI.obj RawrXD_TextEditor_Main.obj RawrXD_TextEditor_Completion.obj kernel32.lib user32.lib gdi32.lib /out:RawrXDEditor.exe`
- [ ] RawrXDEditor.exe created
- [ ] RawrXDEditor.pdb created (debugging symbols)
- [ ] No linker warnings about missing symbols

### Runtime Testing
- [ ] Window appears with correct title
- [ ] Menu bar shows (File/Edit)
- [ ] Toolbar buttons visible
- [ ] Status bar shows at bottom
- [ ] Text input works (appears on screen)
- [ ] Arrow keys navigate
- [ ] Home/End keys work
- [ ] Page Up/Down scroll
- [ ] Backspace/Delete work
- [ ] Ctrl+O opens file dialog
- [ ] Ctrl+S saves to file
- [ ] Ctrl+C/X/V work with clipboard
- [ ] Cursor blinks (500ms on/off)
- [ ] No memory leaks after 1000 keystrokes
- [ ] No crashes on large files (>1MB)

### AI Integration Testing
- [ ] Completion_InsertToken receives tokens
- [ ] Tokens appear in buffer in order
- [ ] Cursor advances with each token
- [ ] Screen updates during token stream
- [ ] 100 tokens insert in <20ms
- [ ] Clipboard paste works with pasted text

## Known Limitations

1. **Buffer Size**: Fixed 2000 bytes max (upgrade to dynamic allocation for production)
2. **No Undo/Redo**: Operations cannot be undone
3. **No Syntax Highlighting**: All text same color
4. **Single File**: Only one document at a time
5. **No Multi-line Selection**: Selection tracking simplified
6. **No Search/Replace**: Find functionality not implemented
7. **Fixed Font**: Cannot change font/size
8. **ASCII Only**: No Unicode support
9. **Memory DC Leak**: Bitmap not cleaned up on window close (add WM_DESTROY handler)
10. **No Scrollbars**: Manual scroll via Page Up/Down only

## Future Enhancements

1. **Dynamic Buffer**: Replace fixed 2000-byte buffer with heap allocation
2. **Undo Stack**: Store operations for reversal
3. **Syntax Coloring**: Token-based colors from AI
4. **Multi-Document Tabs**: Switch between open files
5. **Find/Replace Dialog**: Search implementation
6. **Font Selection**: Configurable font/size
7. **Unicode Support**: UTF-8 text handling
8. **Scrollbars**: Standard Windows scrollbar controls
9. **Line Wrapping**: Wrap long lines to window width
10. **Code Folding**: Collapse/expand regions
