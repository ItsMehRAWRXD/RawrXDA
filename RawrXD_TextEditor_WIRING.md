# RawrXD Text Editor - Complete Wiring Manual

## Quick Start: Integrating All Components

### 1. IDE Frame Integration (C/C++ Side)

```cpp
// In your IDE frame window:
#include "windows.h"
#include "rawrxd_editor.h"

class IDEFrame {
    HWND hEditorWindow;
    LPVOID pBufferPtr;
    LPVOID pCursorPtr;
    
    void InitializeEditor() {
        // Call assembly to create editor window
        //
        // Parameters:
        // rcx = editor_window structure (96 bytes on stack)
        // rdx = window title string
        //
        // Returns:
        // rax = HWND
        
        HWND hWnd = EditorWindow_Create(pEditorData, L"My Editor");
        
        // Link to buffer/cursor
        *(LPVOID*)(pEditorData + 24) = pCursorPtr;
        *(LPVOID*)(pEditorData + 32) = pBufferPtr;
    }
    
    void OnFileOpen() {
        // Display GetOpenFileNameA dialog
        LPSTR pFilename = EditorWindow_FileOpen(pEditorData);
        if (pFilename) {
            // Load file into buffer
            LoadFileToBuffer(pFilename);
        }
    }
    
    void OnFileSave() {
        EditorWindow_FileSave(pEditorData, L"document.txt");
    }
    
    void OnKeyDown(UINT vkCode, DWORD dwParam) {
        // Route keyboard input to editor
        EditorWindow_HandleKeyDown(pEditorData, vkCode);
    }
    
    void OnChar(UINT charCode) {
        EditorWindow_HandleChar(pEditorData, charCode);
    }
    
    void CreateMenu() {
        HMENU hMenuBar = CreateMenu();
        HMENU hFileMenu = CreateMenu();
        
        // File menu
        AppendMenuA(hFileMenu, MFT_STRING, 1001, "Open");
        AppendMenuA(hFileMenu, MFT_STRING, 1002, "Save");
        AppendMenuA(hFileMenu, MFT_SEPARATOR, 0, NULL);
        AppendMenuA(hFileMenu, MFT_STRING, 2001, "Cut");
        AppendMenuA(hFileMenu, MFT_STRING, 2002, "Copy");
        AppendMenuA(hFileMenu, MFT_STRING, 2003, "Paste");
        
        AppendMenuA(hMenuBar, MFT_POPUP, (UINT_PTR)hFileMenu, "File");
        SetMenu(hEditorWindow, hMenuBar);
    }
};
```

### 2. AI Completion Integration

```cpp
// In your AI inference engine:

void OnTokenGenerated(char cToken) {
    // Insert single token into editor in real-time
    // Parameters:
    // rcx = buffer_ptr
    // rdx = token character
    // r8  = cursor_ptr
    
    BYTE token = cToken;
    Completion_InsertToken(pBufferPtr, token, pCursorPtr);
    
    // UI updates automatically via WM_TIMER → InvalidateRect
}

void OnTokenStreamGenerated(const std::string& tokens) {
    // Insert entire token stream
    // Parameters:
    // rcx = buffer_ptr
    // rdx = token_string (null-terminated)
    // r8  = cursor_ptr
    
    Completion_InsertTokenString(pBufferPtr, (char*)tokens.c_str(), pCursorPtr);
}

void OnCompletionAccepted() {
    // User accepted completion
    Completion_AcceptSelection(pBufferPtr, 
                              *(QWORD*)(pCursorPtr + 24),  // selection_start
                              *(QWORD*)(pCursorPtr + 32)); // selection_end
}
```

### 3. Menu/Toolbar Wiring

```cpp
// Handle menu commands in WinProc:
case WM_COMMAND:
    switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case 1001: OnFileOpen(); break;     // File > Open
        case 1002: OnFileSave(); break;     // File > Save
        case 2001: EditorWindow_Cut(pEditorData); break;      // Edit > Cut
        case 2002: EditorWindow_Copy(pEditorData); break;     // Edit > Copy
        case 2003: EditorWindow_Paste(pEditorData); break;    // Edit > Paste
        case 1003: PostQuitMessage(0); break;           // File > Exit
    }
    break;
```

## Memory Layout Reference

### Buffer Structure (2080 bytes total)
```
Offset  Field               Size    Description
0       data_ptr           8       Pointer to text data
8       capacity           8       Allocation size
16      used_length        8       Current content length
24      line_count         4       Number of lines
28      line_offsets[512]  2048    Offset table for O(1) line lookup
```

### Cursor Structure (96 bytes total)
```
Offset  Field               Size    Description
0       byte_offset        8       Current position in buffer
8       line_number        4       Current line (0-based)
16      column_number      4       Current column in line
24      selection_start    8       "-1" if no selection
32      selection_end      8       End of selection range
40      blink_counter      8       500ms toggle counter
80-95   (reserved)         16      Future expansion
```

### EditorWindow Structure (96 bytes total)
```
Offset  Field               Size    Description
0       hwnd               8       Window handle
8       hdc                8       Device context
16      hfont              8       Font handle
24      cursor_ptr         8       Pointer to cursor structure
32      buffer_ptr         8       Pointer to buffer structure
40      char_width         4       Width in pixels (8)
44      char_height        4       Height in pixels (16)
48      client_width       4       Visible width
52      client_height      4       Visible height
56      line_num_width     4       Line number margin (40px)
60      scroll_offset_x    4       Horizontal scroll
64      scroll_offset_y    4       Vertical scroll
68      hbitmap            8       Double-buffer bitmap
76      hmemdc             8       Memory DC
84      timer_id           4       Cursor blink timer ID
```

## Function Call Sequences

### File Open Flow
```
User clicks File > Open
    ↓
Windows sends WM_COMMAND (1001)
    ↓
OnFileOpen()
    ↓
EditorWindow_FileOpen(pEditorData)
    ├─ GetOpenFileNameA(...) → returns filename
    ├─ CreateFileA(filename, GENERIC_READ)
    ├─ GetFileSize(hFile)
    ├─ ReadFile(hFile, buffer, size)
    └─ CloseHandle(hFile)
    ↓
TextBuffer_Clear(pBufferPtr)
    ↓
TextBuffer_InsertString(pBufferPtr, 0, file_content, file_size)
    ├─ Allocates memory if needed
    ├─ Copies content into buffer
    └─ Updates [buffer_ptr + 16] = file_size
    ↓
Cursor_Initialize(pCursorPtr, pBufferPtr)
    └─ Sets position to (0, 0, 0)
    ↓
InvalidateRect(hwnd, NULL, FALSE)
    ↓
WM_PAINT
    ├─ EditorWindow_ClearBackground
    ├─ EditorWindow_RenderLineNumbers
    ├─ EditorWindow_RenderText
    └─ EditorWindow_RenderCursor
    ↓
File displayed in editor
```

### AI Completion Flow
```
AI model generates token 't'
    ↓
OnTokenGenerated('t')
    ↓
Completion_InsertToken(
    pBufferPtr,
    't',
    pCursorPtr
)
    ├─ Get [pCursorPtr + 0] = current byte offset
    ├─ TextBuffer_InsertChar(pBufferPtr, offset, 't')
    │   ├─ Shift existing content right
    │   ├─ Insert 't' at offset
    │   └─ Update [pBufferPtr + 16] = new length
    ├─ Cursor_MoveRight(pCursorPtr)
    │   └─ Increment [pCursorPtr + 0]
    └─ Return new cursor position
    ↓
WM_TIMER (500ms)
    ├─ InvalidateRect(hwnd, NULL, FALSE)
    └─ Trigger repaint
    ↓
WM_PAINT
    ├─ Clear background
    ├─ Render all text including new 't'
    ├─ Render cursor at new position
    └─ Blit double buffer to screen
    ↓
User sees 't' appear in real-time
```

### Selection & Copy Flow
```
User: Shift+Right Arrow (select character)
    ↓
OnKeyDown(VK_RIGHT with Shift)
    ├─ Cursor_SelectTo(pCursorPtr, start, end)
    │   ├─ Set [pCursorPtr + 24] = start
    │   └─ Set [pCursorPtr + 32] = end
    └─ Cursor_MoveRight(pCursorPtr)
    ↓
WM_PAINT
    ├─ EditorWindow_RenderText (draw selected chars highlighted)
    └─ EditorWindow_RenderSelection (paint blue background)
    ↓
User: Ctrl+C
    ↓
OnCommand(2002)  // Copy
    ├─ EditorWindow_Copy(pEditorData)
    │   ├─ Get selection_start/end from cursor
    │   ├─ Extract text from buffer
    │   ├─ OpenClipboard()
    │   ├─ EmptyClipboard()
    │   ├─ SetClipboardData(CF_TEXT, text)
    │   └─ CloseClipboard()
    └─ Status: "Copied to clipboard"
    ↓
User: Ctrl+V elsewhere
    ├─ EditorWindow_Paste(pEditorData)
    │   ├─ OpenClipboard()
    │   ├─ GetClipboardData(CF_TEXT)
    │   ├─ Completion_InsertTokenString(buffer, clipboard_text, cursor)
    │   └─ CloseClipboard()
    ↓
Clipboard content inserted at cursor
```

## Performance Optimization Tips

### 1. Batch Updates
Instead of:
```asm
; BAD: One repaint per token
for each token:
    Completion_InsertToken(...)
    InvalidateRect()  ; Triggers immediate repaint
```

Do:
```asm
; GOOD: Batch tokens, single repaint
for each token:
    Completion_InsertToken(...)

InvalidateRect() ; Single repaint
```

### 2. Line Offset Cache
Buffer stores line offsets at [buffer + 28] for O(1) line lookup:
```asm
; Get line 42 in O(1)
mov rax, [buffer_ptr + 28]      ; line_offsets array
mov ecx, 42
mov edx, [rax + rcx * 8]        ; Get offset for line 42
```

### 3. Scrolling Optimization
Only redraw visible region:
```asm
; Get scroll position
mov eax, [window_ptr + 60]      ; scroll_offset_x
mov ecx, [window_ptr + 64]      ; scroll_offset_y

; Calculate visible line range
mov edx, [window_ptr + 52]      ; client_height
mov r8d, [window_ptr + 44]      ; char_height
div r8d                         ; lines_visible = height / char_height

; Only render from scroll_start to scroll_start + lines_visible
```

## Testing Checklist

- [ ] Window creates with title in taskbar
- [ ] File Open dialog appears and loads files
- [ ] File Save writes to disk
- [ ] Text appears as user types
- [ ] Arrow keys navigate
- [ ] Ctrl+O opens files
- [ ] Ctrl+S saves files
- [ ] Ctrl+C/X/V work
- [ ] Cursor blinks smoothly
- [ ] Selection highlights correctly
- [ ] Status bar updates position
- [ ] AI tokens insert smoothly
- [ ] No memory leaks on file operations
- [ ] Window resizing keeps content visible

## Future Extensions

1. **Syntax Highlighting**: Color tokens by type
   ```asm
   EditorWindow_RenderTextWithColors(window_ptr, color_table)
   ```

2. **Undo/Redo**: Maintain operation stack
   ```asm
   EditorHistory_Undo(history_ptr)
   EditorHistory_Redo(history_ptr)
   ```

3. **Find/Replace**: Search and replace dialog
   ```asm
   EditorWindow_ShowFindDialog(window_ptr)
   TextBuffer_FindNext(buffer_ptr, search_string)
   ```

4. **Multi-Document**: Tab bar for multiple files
   ```asm
   EditorTabs_Add(tab_manager_ptr, filename)
   EditorTabs_Switch(tab_manager_ptr, tab_index)
   ```

## Debugging Guide

### Check Window Creation:
```asm
lea rax, [window_ptr]
mov rax, [rax]          ; hwnd at offset 0
cmp rax, 0
je .WindowCreationFailed
```

### Verify Buffer Content:
```asm
mov rax, [buffer_ptr]   ; Buffer data pointer
mov rcx, [buffer_ptr + 16]  ; Content length
; rax points to rcx bytes of text data
```

### Trace Cursor Position:
```asm
mov al, [cursor_ptr + 0]    ; Byte offset
mov cl, [cursor_ptr + 8]    ; Line number
mov dl, [cursor_ptr + 16]   ; Column
; Position = (line, column, byte_offset)
```

### Monitor Paint Calls:
Add tracing to WM_PAINT message handler count repaints and measure latency.
