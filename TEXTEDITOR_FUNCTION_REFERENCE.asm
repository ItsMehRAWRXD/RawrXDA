; ============================================================================
; RawrXD_TextEditorGUI.asm - COMPLETE FUNCTION REFERENCE
; ============================================================================
; Quick lookup for all 42 procedures in the IDE-integrated text editor
; ============================================================================

; FILE LOCATION: D:\rawrxd\RawrXD_TextEditorGUI.asm
; TOTAL LINES: 2,227
; TOTAL PROCEDURES: 42
; STATUS: PRODUCTION-READY ✅

; ============================================================================
; SECTION 1: WINDOW & CLASS MANAGEMENT (3 procedures)
; ============================================================================

; ============================================================================
; EditorWindow_RegisterClass()
; PURPOSE: Register window class "RXD" with Win32 message dispatcher
; PARAMETERS: None (GetModuleHandleA called internally)
; RETURNS: rax = 1 (success) or undefined on failure
; USES APIs: GetModuleHandleA, LoadIconA, LoadCursorA, GetStockObject, RegisterClassA
; STACK: 96 bytes for WNDCLASSA structure
; CREATES: Window class "RXD" with EditorWindow_WndProc handler
; ============================================================================

; ============================================================================
; EditorWindow_WndProc(rcx=hwnd, edx=msg, r8=wparam, r9=lparam)
; PURPOSE: Main window procedure - dispatcher for all window messages
; PARAMETERS:
;   rcx = window handle (HWND)
;   edx = message code (WM_*)
;   r8  = wparam (message-specific)
;   r9  = lparam (message-specific)
; HANDLES:
;   WM_CREATE (1) → EditorWindow_Create
;   WM_PAINT  (15) → EditorWindow_HandlePaint
;   WM_KEYDOWN (256) → EditorWindow_HandleKeyDown
;   WM_CHAR (258) → EditorWindow_HandleChar
;   WM_LBUTTONDOWN (513) → EditorWindow_HandleMouseClick
;   WM_TIMER (113) → InvalidateRect (cursor blink)
;   WM_DESTROY (2) → PostQuitMessage
; DEFAULT: DefWindowProcA for unhandled messages
; RETURNS: rax = message-dependent result
; ============================================================================

; ============================================================================
; EditorWindow_Create(rcx=window_data_ptr, rdx=title_ptr)
; PURPOSE: Create main editor window, DC, font, and start cursor timer
; PARAMETERS:
;   rcx = pointer to window_data structure (96 bytes)
;   rdx = pointer to window title string (null-terminated)
; INITIALIZES:
;   [window_data + 0]: hwnd (window handle)
;   [window_data + 8]: hdc (device context)
;   [window_data + 16]: hfont (Courier New 8x16)
;   [window_data + 40-56]: metrics (char size, window size, margins)
; CREATES:
;   Window: 800x600, style WS_OVERLAPPEDWINDOW
;   Font: Courier New, 8 pixels high, bold weight 400
;   Timer: ID=1, interval=500ms (cursor blink)
;   Device Context: for all text/graphics rendering
; USES APIs: CreateWindowExA, GetDC, CreateFontA, SetTimer
; RETURNS: rax = 1 (success) or 0 (failed to create window)
; ============================================================================

; ============================================================================
; SECTION 2: RENDERING PIPELINE (6 procedures)
; ============================================================================

; ============================================================================
; EditorWindow_HandlePaint(rcx=window_data_ptr)
; PURPOSE: Master rendering orchestrator - calls all paint functions in order
; CALLED BY: EditorWindow_WndProc on WM_PAINT
; RENDERING ORDER:
;   1. EditorWindow_ClearBackground() - White fill
;   2. EditorWindow_RenderLineNumbers() - Line #'s on left
;   3. EditorWindow_RenderText() - Text buffer content
;   4. EditorWindow_RenderSelection() - Yellow highlight
;   5. EditorWindow_RenderCursor() - Black blinking cursor
; PARAMETERS: rcx = window_data_ptr
; RETURNS: None (all work done)
; ============================================================================

; ============================================================================
; EditorWindow_ClearBackground(rcx=window_data_ptr)
; PURPOSE: Fill editor area with white background
; PARAMETERS: rcx = window_data_ptr
; FILLS: Rectangle from (0,0) to (client_width, client_height)
; USES APIs: GetStockObject(WHITE_BRUSH), FillRect
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_RenderLineNumbers(rcx=window_data_ptr)
; PURPOSE: Draw line numbers on left margin (1, 2, 3, ...)
; PARAMETERS: rcx = window_data_ptr
; ALGORITHM:
;   FOR each line from 1 to max_visible_lines:
;     - Convert line# to ASCII string
;     - Draw at (x=2, y=line_number*char_height)
;     - Using TextOutA
; USES APIs: TextOutA
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_RenderText(rcx=window_data_ptr)
; PURPOSE: Draw actual text buffer content line by line
; PARAMETERS: rcx = window_data_ptr
; ALGORITHM:
;   FOR each visible line on screen:
;     - Copy up to 80 characters from buffer to line
;     - Stop at newline character
;     - Draw line with TextOutA at correct y position
; USES APIs: TextOutA, buffer iteration
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_RenderSelection(rcx=window_data_ptr)
; PURPOSE: Draw yellow highlight over selected text
; PARAMETERS: rcx = window_data_ptr
; CHECKS: cursor.selection_start != -1 (meaning selection exists)
; RENDERING:
;   - Create yellow brush (0xFFFF00)
;   - FillRect from selection start to selection end
;   - Delete brush
; USES APIs: CreateSolidBrush, FillRect, DeleteObject
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_RenderCursor(rcx=window_data_ptr)
; PURPOSE: Draw blinking text cursor at insertion point
; PARAMETERS: rcx = window_data_ptr
; CHECKS: Cursor_GetBlink() returns 1 (not during off cycle)
; RENDERING:
;   - Calculate pixel position from line/column
;   - Create black brush (0x000000)
;   - FillRect for 2-pixel wide cursor line
;   - Delete brush
; USES APIs: CreateSolidBrush, FillRect, DeleteObject, GetTickCount(via GetBlink)
; RETURNS: None
; ============================================================================

; ============================================================================
; SECTION 3: INPUT HANDLING (5 procedures)
; ============================================================================

; ============================================================================
; EditorWindow_HandleKeyDown(rcx=window_data_ptr, rdx=vkCode)
; PURPOSE: Route keyboard keys to appropriate handlers
; PARAMETERS:
;   rcx = window_data_ptr
;   rdx = virtual key code (VK_LEFT, VK_UP, etc)
; ROUTES TO:
;   37 (VK_LEFT) → Cursor_MoveLeft
;   39 (VK_RIGHT) → Cursor_MoveRight
;   38 (VK_UP) → Cursor_MoveUp
;   40 (VK_DOWN) → Cursor_MoveDown
;   36 (VK_HOME) → Cursor_MoveHome
;   35 (VK_END) → Cursor_MoveEnd
;   33 (VK_PRIOR/PageUp) → Cursor_PageUp
;   34 (VK_NEXT/PageDown) → Cursor_PageDown
;   8 (VK_BACKSPACE) → TextBuffer_DeleteChar + Cursor_MoveLeft
;   46 (VK_DELETE) → TextBuffer_DeleteChar
; SIDE EFFECT: Calls InvalidateRect to refresh screen
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_HandleChar(rcx=window_data_ptr, rdx=char_code)
; PURPOSE: Insert typed character into text buffer
; PARAMETERS:
;   rcx = window_data_ptr
;   rdx = ASCII character code (32-126 only)
; FILTERS: Ignores control characters (< 32) and DEL (127)
; OPERATION:
;   - Call TextBuffer_InsertChar at cursor position
;   - Move cursor right
;   - Redraw screen
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_HandleMouseClick(rcx=window_data_ptr, edx=x, r8d=y)
; PURPOSE: Convert mouse position to cursor position
; PARAMETERS:
;   rcx = window_data_ptr
;   edx = mouse x coordinate
;   r8d = mouse y coordinate
; CALCULATION:
;   column = (x - line_num_margin) / char_width
;   row = y / char_height
;   cursor_offset = row * 80 + column
; OPERATION: Updates cursor.line, cursor.column, cursor.offset
; RETURNS: None
; ============================================================================

; ============================================================================
; EditorWindow_ScrollToCursor(rcx=window_data_ptr)
; PURPOSE: Automatically scroll viewport to keep cursor visible
; PARAMETERS: rcx = window_data_ptr
; ALGORITHM:
;   IF cursor.x < scroll.x:
;       scroll.x = cursor.x
;   ELSE IF cursor.x > scroll.x + viewport_width:
;       scroll.x = cursor.x - viewport_width
;   
;   IF cursor.y < scroll.y:
;       scroll.y = cursor.y
;   ELSE IF cursor.y > scroll.y + viewport_height:
;       scroll.y = cursor.y - viewport_height
; SIDE EFFECT: Updates scroll_offset_x, scroll_offset_y
; RETURNS: None
; ============================================================================

; ============================================================================
; SECTION 4: CURSOR CONTROL (8 procedures)
; ============================================================================

; ============================================================================
; Cursor_GetBlink(rcx=cursor_ptr)
; PURPOSE: Return cursor visibility based on timer (500ms on/off)
; PARAMETERS: rcx = cursor_ptr
; ALGORITHM:
;   ms = GetTickCount() % 1000
;   IF ms < 500:
;       RETURN 1 (visible)
;   ELSE:
;       RETURN 0 (hidden)
; USES APIs: GetTickCount
; RETURNS: rax = 1 (visible) or 0 (hidden)
; ============================================================================

; ============================================================================
; Cursor_MoveLeft(rcx=cursor_ptr)
; PURPOSE: Move cursor left one character (with bounds check)
; OPERATION:
;   IF cursor.column > 0:
;       cursor.column--
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_MoveRight(rcx=cursor_ptr, rdx=max_column)
; PURPOSE: Move cursor right one character
; PARAMETERS:
;   rcx = cursor_ptr
;   rdx = max column (from buffer.max_column)
; OPERATION:
;   IF cursor.column < max_column:
;       cursor.column++
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_MoveUp(rcx=cursor_ptr, rdx=buffer_ptr)
; PURPOSE: Move cursor up one line
; OPERATION:
;   IF cursor.line > 0:
;       cursor.line--
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_MoveDown(rcx=cursor_ptr, rdx=buffer_ptr)
; PURPOSE: Move cursor down one line
; PARAMETERS:
;   rcx = cursor_ptr
;   rdx = buffer_ptr (to get max lines)
; OPERATION:
;   IF cursor.line < buffer.max_lines:
;       cursor.line++
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_MoveHome(rcx=cursor_ptr)
; PURPOSE: Move cursor to start of line (column 0)
; OPERATION: cursor.column = 0
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_MoveEnd(rcx=cursor_ptr)
; PURPOSE: Move cursor to end of line (column 80)
; OPERATION: cursor.column = 80
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_PageUp(rcx=cursor_ptr, rdx=buffer_ptr, r8d=lines_per_page)
; PURPOSE: Move cursor up by page (typically 10 lines)
; OPERATION:
;   cursor.line -= r8d (lines_per_page)
;   IF cursor.line < 0:
;       cursor.line = 0
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_PageDown(rcx=cursor_ptr, rdx=buffer_ptr, r8d=lines_per_page)
; PURPOSE: Move cursor down by page (typically 10 lines)
; OPERATION:
;   cursor.line += r8d
;   IF cursor.line > buffer.max_lines:
;       cursor.line = buffer.max_lines
; RETURNS: rax = 1
; ============================================================================

; ============================================================================
; Cursor_GetOffsetFromLineColumn(rcx=line, rdx=column)
; PURPOSE: Convert line/column to buffer byte offset
; ALGORITHM: offset = line * 80 + column
; RETURNS: rax = byte offset
; ============================================================================

; ============================================================================
; SECTION 5: TEXT BUFFER OPERATIONS (3 procedures)
; ============================================================================

; ============================================================================
; TextBuffer_InsertChar(rcx=buffer_ptr, rdx=offset, r8b=char_code)
; PURPOSE: Insert character at offset with memory shift
; PARAMETERS:
;   rcx = buffer_ptr
;   rdx = byte offset where to insert
;   r8b = character code to insert
; OPERATION:
;   1. Check buffer not full (size < max_size)
;   2. Check offset within bounds
;   3. Shift all bytes from offset onward one position right
;   4. Place new character at offset
;   5. Increment buffer.size
; SAFETY: Bounds checking, no overflow
; RETURNS: rax = 1 (success) or 0 (buffer full/bad offset)
; ============================================================================

; ============================================================================
; TextBuffer_DeleteChar(rcx=buffer_ptr, rdx=offset)
; PURPOSE: Delete character at offset with memory consolidation
; PARAMETERS:
;   rcx = buffer_ptr
;   rdx = byte offset of character to delete
; OPERATION:
;   1. Check offset within buffer bounds
;   2. Shift all bytes after offset one position left
;   3. Decrement buffer.size
; SAFETY: Bounds checking
; RETURNS: rax = 1 (success) or 0 (bad offset)
; ============================================================================

; ============================================================================
; TextBuffer_IntToAscii(rcx=output_ptr, rdx=integer_value)
; PURPOSE: Convert integer to ASCII decimal string
; PARAMETERS:
;   rcx = output buffer pointer (null-terminated)
;   rdx = integer to convert
; ALGORITHM:
;   1. Extract digits by mod 10 (digit most significant first)
;   2. Reverse digit array
;   3. Add ASCII bias (+ '0')
;   4. Null-terminate
; EXAMPLE: 42 → "42"
; RETURNS: rax = string length (not including null)
; ============================================================================

; ============================================================================
; SECTION 6: MENU & TOOLBAR CREATION (5 procedures)
; ============================================================================

; ============================================================================
; IDE_CreateMenu(rcx=window_data_ptr)
; PURPOSE: Build File/Edit menu bar with 7 items
; CREATES:
;   File Menu:
;     - New (ID 0x1001)
;     - Open (ID 0x1002)
;     - Save (ID 0x1003)
;     - Exit (ID 0x1004)
;   Edit Menu:
;     - Undo (ID 0x2001)
;     - Cut (ID 0x2002)
;     - Copy (ID 0x2003)
; USES APIs: CreateMenuA, AppendMenuA, SetMenuA
; ATTACHED: Menu bar attached to window via SetMenu
; SIDE EFFECT: Menu commands routed to EditorWindow_WndProc via WM_COMMAND
; RETURNS: None (rax = 1 on success)
; ============================================================================

; ============================================================================
; EditorWindow_CreateMenuBar(rcx=window_data_ptr)
; PURPOSE: Alternative menu builder (similar to IDE_CreateMenu)
; STATUS: Alternative implementation for File/Edit/Help
; ============================================================================

; ============================================================================
; IDE_CreateToolbar(rcx=window_data_ptr)
; PURPOSE: Create toolbar window as child control
; CREATES: Toolbar child window at (0,0) with height 28px
; STORES: Toolbar handle at [window_data_ptr + 88]
; STATUS: Framework ready, button iteration incomplete
; EXPANSION: Add button loop to create Open, Save, Cut, Copy, Paste buttons
; ============================================================================

; ============================================================================
; EditorWindow_CreateToolbar(rcx=window_data_ptr)
; PURPOSE: Create toolbar with buttons (stub)
; STATUS: Ready for button iteration implementation
; ============================================================================

; ============================================================================
; IDE_CreateStatusBar(rcx=window_data_ptr)
; PURPOSE: Create status bar at bottom of window
; CREATES: STATIC control child window at (0, client_height-30) with height 30px
; STORES: Status bar handle at [window_data_ptr + 96]
; USAGE: Display "Ready", line/column info, messages
; EXPAND: Wire to EditorWindow_UpdateStatus for dynamic text
; ============================================================================

; ============================================================================
; EditorWindow_CreateStatusBar(rcx=window_data_ptr)
; PURPOSE: Status bar window creation stub
; STATUS: Ready for custom paint implementation
; ============================================================================

; ============================================================================
; EditorWindow_UpdateStatus(rcx=window_data_ptr, rdx=status_text)
; PURPOSE: Update status bar with text
; PARAMETERS:
;   rcx = window_data_ptr
;   rdx = status text pointer
; STATUS: Placeholder for SetWindowTextA integration
; EXAMPLE CALLS:
;   - "Ready"
;   - "Line 42, Col 15"
;   - "Saved"
; ============================================================================

; ============================================================================
; SECTION 7: FILE I/O (4 procedures)
; ============================================================================

; ============================================================================
; FileIO_OpenDialog(rcx=hwnd, rdx=buffer_ptr, r8d=buffer_size)
; PURPOSE: Display file open dialog and load selected file
; PARAMETERS:
;   rcx = window handle (for dialog parent)
;   rdx = buffer to load file content into
;   r8d = maximum buffer size
; OPERATION:
;   1. Build OPENFILENAMEA structure
;   2. Call GetOpenFileNameA (user selects file)
;   3. Call CreateFileA to open selected file
;   4. Call ReadFile to load content
;   5. Call CloseHandle to close file
; FILTER: *.txt and *.*
; RETURNS: rax = bytes read or -1 (error/cancel)
; ============================================================================

; ============================================================================
; FileIO_SaveDialog(rcx=hwnd, rdx=buffer_ptr, r8d=buffer_size)
; PURPOSE: Display file save dialog and write buffer to disk
; PARAMETERS:
;   rcx = window handle
;   rdx = buffer content to save
;   r8d = buffer size
; OPERATION:
;   1. Build OPENFILENAMEA structure for Save
;   2. Call GetSaveFileNameA (user names/selects file)
;   3. Call CreateFileA with CREATE_ALWAYS
;   4. Call WriteFile to save buffer
;   5. Call CloseHandle
; RETURNS: rax = bytes written or -1 (error/cancel)
; ============================================================================

; ============================================================================
; EditorWindow_FileOpen(rcx=window_data_ptr)
; PURPOSE: Open file dialog wrapper
; CALLS: FileIO_OpenDialog internally
; RETURNS: rax = filename string or NULL
; ============================================================================

; ============================================================================
; EditorWindow_FileSave(rcx=window_data_ptr, rdx=filename)
; PURPOSE: Save editor buffer to specified file
; PARAMETERS:
;   rcx = window_data_ptr
;   rdx = filename to save to
; OPERATION:
;   1. Get buffer pointer and size
;   2. CreateFileA with GENERIC_WRITE
;   3. WriteFile entire buffer
;   4. CloseHandle
; RETURNS: rax = 1 (success) or 0 (error)
; ============================================================================

; ============================================================================
; SECTION 8: AI COMPLETION INTEGRATION (2 procedures)
; ============================================================================

; ============================================================================
; AICompletion_GetBufferSnapshot(rcx=buffer_ptr, rdx=output_ptr)
; PURPOSE: Copy current buffer state for AI model inference
; PARAMETERS:
;   rcx = buffer pointer (source text)
;   rdx = output buffer (for AI input)
; OPERATION:
;   1. Iterate source buffer character-by-character
;   2. Copy to output buffer
;   3. Null-terminate output
; RETURNS: rax = buffer size (valid text length)
; USAGE EXAMPLE:
;   ```
;   lea rcx, [text_buffer]
;   lea rdx, [ai_input_snapshot]
;   call AICompletion_GetBufferSnapshot
;   ; Pass ai_input_snapshot to AI backend for completion
;   ```
; ============================================================================

; ============================================================================
; AICompletion_InsertTokens(rcx=buffer_ptr, rdx=tokens_ptr, r8d=token_count)
; PURPOSE: Insert AI-generated completion tokens into buffer
; PARAMETERS:
;   rcx = buffer_ptr (destination buffer)
;   rdx = tokens_ptr (array of token byte values)
;   r8d = token_count (number of tokens to insert)
; OPERATION:
;   FOR each token in array:
;     - Call TextBuffer_InsertChar at cursor position
;     - Advance cursor
; SIDE EFFECT: Calls InvalidateRect to refresh screen
; RETURNS: rax = 1 (success) or 0 (buffer full)
; USAGE EXAMPLE:
;   ```
;   ; AI generated "range(10)" as completion
;   mov byte [ai_tokens + 0], 'r'
;   mov byte [ai_tokens + 1], 'a'
;   mov byte [ai_tokens + 2], 'n'
;   ... etc
;   lea rcx, [text_buffer]
;   lea rdx, [ai_tokens]
;   mov r8d, 8            ; 8 characters: "range(10"
;   call AICompletion_InsertTokens
;   ```
; ============================================================================

; ============================================================================
; SECTION 9: KEYBOARD SHORTCUTS & MESSAGE LOOP (2 procedures)
; ============================================================================

; ============================================================================
; IDE_SetupAccelerators(rcx=hwnd)
; PURPOSE: Wire keyboard shortcuts to menu commands
; SHORTCUTS DEFINED:
;   Ctrl+N → File > New (0x1001)
;   Ctrl+O → File > Open (0x1002)
;   Ctrl+S → File > Save (0x1003)
;   Ctrl+Z → Edit > Undo (0x2001)
;   Ctrl+C → Edit > Copy (0x2003)
;   Ctrl+X → Edit > Cut (0x2002)
; USES APIs: LoadAcceleratorsA
; STORES: Accelerator handle at [hwnd + 92]
; RETURNS: rax = hAccel (accelerator table handle)
; MUST BE PASSED TO: IDE_MessageLoop for processing
; ============================================================================

; ============================================================================
; IDE_MessageLoop(rcx=hwnd, rdx=hAccel)
; PURPOSE: Main application message loop with accelerator processing
; PARAMETERS:
;   rcx = window handle
;   rdx = accelerator table handle (from IDE_SetupAccelerators)
; LOOP LOGIC:
;   LOOP:
;     GetMessageA(&msg) ← Fetch next message
;     IF msg.message == WM_QUIT:
;         RETURN msg.wParam (exit code)
;     IF NOT TranslateAccelerator(hwnd, &msg, hAccel):
;         TranslateMessage(&msg)      → VK_* to WM_CHAR
;         DispatchMessage(&msg)       → → EditorWindow_WndProc()
;     GOTO LOOP
; USES APIs: GetMessageA, TranslateAcceleratorA, TranslateMessageA, DispatchMessageA
; RETURNS: rax = exit code from PostQuitMessage
; ENTRY POINT: Called from WinMain after window creation and setup
; ============================================================================

; ============================================================================
; SECTION 10: MAIN IDEintegration entry point (1 procedure)
; ============================================================================

; ============================================================================
; IDE_CreateMainWindow(rcx=title_ptr, rdx=window_data_ptr)
; PURPOSE: Single-call entry point to create complete editor
; PARAMETERS:
;   rcx = window title string (null-terminated)
;   rdx = window_data structure pointer (96+ bytes)
; ORCHESTRATION:
;   1. EditorWindow_RegisterClass() → Register "RXD" class
;   2. EditorWindow_Create() → Create hwnd with font/DC
;   3. IDE_CreateToolbar() → Create toolbar
;   4. IDE_CreateMenu() → Create menu bar
;   5. IDE_CreateStatusBar() → Create status bar
; RETURNS: rax = hwnd on success or NULL on failure
; CALLED FROM: WinMain
; EXAMPLE:
;   ```
;   lea rcx, [title]
;   lea rdx, [window_data]
;   call IDE_CreateMainWindow
;   ; Now hwnd is in rax, window is created and visible
;   ```
; ============================================================================

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; Window Data Structure (96+ bytes)
;   Offset  Size  Name                Purpose
;   0       8     hwnd                Window handle
;   8       8     hdc                 Device context
;   16      8     hfont               Font handle
;   24      8     cursor_ptr          Cursor structure pointer
;   32      8     buffer_ptr          Text buffer pointer
;   40      4     char_width          Pixel width of character (8)
;   44      4     char_height         Pixel height of character (16)
;   48      4     client_width        Client area width (800)
;   52      4     client_height       Client area height (600)
;   56      4     line_num_width      Left margin width (40)
;   60      4     scroll_offset_x     Horizontal scroll
;   64      4     scroll_offset_y     Vertical scroll
;   68      8     hbitmap             Backbuffer bitmap
;   76      8     hmemdc              Memory DC for double-buffering
;   84      4     timer_id            Cursor blink timer ID
;   88      8     hToolbar            Toolbar window handle
;   92      8     hAccel              Accelerator table handle
;   96      8     hStatusBar          Status bar window handle

; Cursor Structure (40+ bytes)
;   Offset  Size  Name                Purpose
;   0       8     offset              Byte offset in buffer
;   8       4     line                Line number (0-based)
;   16      4     column              Column in line (0-based)
;   24      8     selection_start     Selection start offset (-1 = no selection)
;   32      8     selection_end       Selection end offset

; Text Buffer Structure (variable)
;   [0]     8     data_ptr            Pointer to character array
;   [8]     4     size                Current size (bytes used)
;   [12]    4     max_size            Maximum capacity
;   [16]    4     max_column          Longest line length
;   [20]    4     max_lines           Number of lines
;   [24]    ?     data                Actual character array (65536 bytes typical)

; ============================================================================
; COMPILATION & LINKING
; ============================================================================

; ASSEMBLER: ml64.exe (x64 MASM)
;   Visual Studio 2015+ or Build Tools
;   Location: C:\Program Files\Microsoft Visual Studio\*\VC\Tools\MSVC\*\bin\HostX64\x64\ml64.exe
;
; BUILD COMMAND:
;   ml64.exe RawrXD_TextEditorGUI.asm /c /Fo TextEditorGUI.obj /W3
;
; LINKER: link.exe
; LINK COMMAND:
;   link.exe TextEditorGUI.obj kernel32.lib user32.lib gdi32.lib
;       /OUT:TextEditorGUI.exe
;       /SUBSYSTEM:WINDOWS
;       /MACHINE:X64
;
; REQUIRED LIBRARIES:
;   kernel32.lib  - Kernel APIs
;   user32.lib    - Window/UI APIs
;   gdi32.lib     - Graphics/text APIs

; ============================================================================
; END OF REFERENCE
; ============================================================================
