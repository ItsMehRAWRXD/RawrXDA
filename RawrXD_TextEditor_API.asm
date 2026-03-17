; RawrXD Text Editor - Assembly Function Reference
; x64 MASM Calling Convention (Microsoft x64)
; Parameters: rcx, rdx, r8, r9 (then stack)
; Return: rax (64-bit), rdx:rax (128-bit)
; Caller must preserve: rbx, rbp, rsi, rdi, r12-r15
; Callee can clobber: rax, rcx, rdx, r8-r11

;==============================================================================
; ==================== WINDOW MANAGEMENT API ==================================================
;==============================================================================

; ---- Register Window Class ----
; void EditorWindow_RegisterClass(void)
; No parameters
; Returns: Nothing
; Side effects: Registers "RawrXDEditor" window class
EditorWindow_RegisterClass PROTO

; Example usage:
;   call EditorWindow_RegisterClass

;----

; ---- Create Window with HWND Return ----
; HWND EditorWindow_Create(window_ptr, title_ptr)
; rcx = editor_window structure (96 bytes)
; rdx = window title (null-terminated wide string)
; Returns: rax = HWND
; Side effects: Creates actual window, GetDC, CreateMenu, SetTimer
EditorWindow_Create PROTO :PTR, :PTR

; Example usage:
;   lea rcx, [editor_window]
;   lea rdx, [title_string]
;   call EditorWindow_Create
;   mov hwnd, rax

;----

; ---- Window Message Dispatcher ----
; LRESULT EditorWindow_WndProc(hwnd, msg, wparam, lparam)
; rcx = HWND
; rdx = message (WM_*)
; r8  = wParam
; r9  = lParam
; Returns: rax = message result
; Routes to: OnCreat/Paint/KeyDown/Char/LButtonDown/Timer/Destroy
EditorWindow_WndProc PROTO :QWORD, :QWORD, :QWORD, :QWORD

;==============================================================================
; ==================== RENDERING API ====================================================
;==============================================================================

; ---- Clear Canvas (White Background) ----
; void EditorWindow_ClearBackground(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: Fills entire window with white
EditorWindow_ClearBackground PROTO :PTR

;----

; ---- Render Line Numbers (Left Margin) ----
; void EditorWindow_RenderLineNumbers(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: Draws line numbers in left margin
EditorWindow_RenderLineNumbers PROTO :PTR

;----

; ---- Render Text Content ----
; void EditorWindow_RenderText(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: TextOutA for each visible line
EditorWindow_RenderText PROTO :PTR

;----

; ---- Render Selection Highlight ----
; void EditorWindow_RenderSelection(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: PatBlt with blue background over selected region
EditorWindow_RenderSelection PROTO :PTR

;----

; ---- Render Cursor ----
; void EditorWindow_RenderCursor(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: Draws blinking cursor line at current position
EditorWindow_RenderCursor PROTO :PTR

;----

; ---- Paint Message Handler (Orchestrates All Rendering) ----
; void EditorWindow_HandlePaint(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Calls: ClearBackground → RenderLineNumbers → RenderText → 
;        RenderSelection → RenderCursor (in sequence)
EditorWindow_HandlePaint PROTO :PTR

;==============================================================================
; ==================== INPUT HANDLING API ================================================
;==============================================================================

; ---- Key Down Message Handler ----
; void EditorWindow_HandleKeyDown(window_ptr, vk_code)
; rcx = editor_window structure
; rdx = virtual key code (VK_LEFT, VK_UP, etc.)
; Returns: Nothing
; Routes key: VK_LEFT → Cursor_MoveLeft, VK_RIGHT → Cursor_MoveRight,
;            VK_UP → Cursor_MoveUp, VK_DOWN → Cursor_MoveDown,
;            VK_HOME → Cursor_MoveHome, VK_END → Cursor_MoveEnd,
;            VK_PRIOR → Cursor_PageUp, VK_NEXT → Cursor_PageDown,
;            VK_BACK → TextBuffer_DeleteChar, VK_DELETE → TextBuffer_DeleteChar
EditorWindow_HandleKeyDown PROTO :PTR, :QWORD

;----

; ---- Character Message Handler ----
; void EditorWindow_HandleChar(window_ptr, char_code)
; rcx = editor_window structure
; rdx = character code (ASCII value)
; Returns: Nothing
; Inserts character into buffer at cursor and advances cursor
EditorWindow_HandleChar PROTO :PTR, :QWORD

;----

; ---- Mouse Click Handler ----
; void EditorWindow_HandleMouseClick(window_ptr, x_pos, y_pos)
; rcx = editor_window structure
; rdx = X pixel coordinate
; r8  = Y pixel coordinate
; Returns: Nothing
; Converts screen coordinates to buffer position and sets cursor
EditorWindow_HandleMouseClick PROTO :PTR, :QWORD, :QWORD

;==============================================================================
; ==================== FILE I/O API =====================================================
;==============================================================================

; ---- File Open Dialog (GetOpenFileNameA Wrapper) ----
; LPSTR EditorWindow_FileOpen(window_ptr)
; rcx = editor_window structure
; Returns: rax = filename string (256 bytes max), or NULL if cancelled
; Side effects: Shows GetOpenFileNameA dialog
EditorWindow_FileOpen PROTO :PTR

; Example usage:
;   lea rcx, [editor_window]
;   call EditorWindow_FileOpen
;   test rax, rax
;   jz .UserCancelled

;----

; ---- File Save (CreateFileA + WriteFile) ----
; DWORD EditorWindow_FileSave(window_ptr, filename_ptr)
; rcx = editor_window structure
; rdx = filename (null-terminated string)
; Returns: rax = bytes written, or -1 on error
; Side effects: Creates/overwrites file with buffer content
EditorWindow_FileSave PROTO :PTR, :PTR

;----

; ---- Update Status Bar Text ----
; void EditorWindow_UpdateStatus(window_ptr, status_str)
; rcx = editor_window structure
; rdx = status text (null-terminated string)
; Returns: Nothing
; Side effects: Updates status bar control at bottom
EditorWindow_UpdateStatus PROTO :PTR, :PTR

;==============================================================================
; ==================== MENU & TOOLBAR API ================================================
;==============================================================================

; ---- Create Menu Bar ----
; HMENU EditorWindow_CreateMenuBar(window_ptr)
; rcx = editor_window structure
; Returns: rax = menu handle
; Side effects: Creates File/Edit menus, AppendMenuA commands,
;               1001=Open, 1002=Save, 1003=Exit,
;               2001=Cut, 2002=Copy, 2003=Paste
EditorWindow_CreateMenuBar PROTO :PTR

;----

; ---- Create Toolbar ----
; HWND EditorWindow_CreateToolbar(window_ptr)
; rcx = editor_window structure
; Returns: rax = toolbar window handle
; Side effects: Creates button controls for Open, Save, Cut, Copy, Paste
EditorWindow_CreateToolbar PROTO :PTR

;----

; ---- Create Status Bar ----
; HWND EditorWindow_CreateStatusBar(window_ptr)
; rcx = editor_window structure
; Returns: rax = status bar window handle
; Side effects: Creates static control at bottom of window
EditorWindow_CreateStatusBar PROTO :PTR

;==============================================================================
; ==================== CURSOR NAVIGATION API ========================================
;==============================================================================

; ---- Move Cursor Left One Character ----
; void Cursor_MoveLeft(cursor_ptr, buffer_ptr)
; rcx = cursor structure (96 bytes)
; rdx = buffer structure (2080 bytes)
; Returns: Nothing
; Side effects: Decrements [cursor_ptr] with bounds checking
Cursor_MoveLeft PROTO :PTR, :PTR

;----

; ---- Move Cursor Right One Character ----
; void Cursor_MoveRight(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Side effects: Increments [cursor_ptr] with bounds checking
Cursor_MoveRight PROTO :PTR, :PTR

;----

; ---- Move Cursor Up One Line ----
; void Cursor_MoveUp(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Updates line/column at [cursor_ptr + 8] and [cursor_ptr + 16]
Cursor_MoveUp PROTO :PTR, :PTR

;----

; ---- Move Cursor Down One Line ----
; void Cursor_MoveDown(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Updates line/column
Cursor_MoveDown PROTO :PTR, :PTR

;----

; ---- Move Cursor to Line Start ----
; void Cursor_MoveHome(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Sets column to line start
Cursor_MoveHome PROTO :PTR, :PTR

;----

; ---- Move Cursor to Line End ----
; void Cursor_MoveEnd(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Sets column to line end
Cursor_MoveEnd PROTO :PTR, :PTR

;----

; ---- Move Cursor Up One Page ----
; void Cursor_PageUp(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Moves up 10 lines
Cursor_PageUp PROTO :PTR, :PTR

;----

; ---- Move Cursor Down One Page ----
; void Cursor_PageDown(cursor_ptr, buffer_ptr)
; rcx = cursor structure
; rdx = buffer structure
; Returns: Nothing
; Moves down 10 lines
Cursor_PageDown PROTO :PTR, :PTR

;----

; ---- Get Cursor Blink State ----
; BOOL Cursor_GetBlink(cursor_ptr)
; rcx = cursor structure
; Returns: rax = 0 (off) or 1 (on), 500ms period
; Uses GetTickCount % 1000 for blinking
Cursor_GetBlink PROTO :PTR

;==============================================================================
; ==================== TEXT BUFFER API ================================================
;==============================================================================

; ---- Insert Character at Position ----
; void TextBuffer_InsertChar(buffer_ptr, position, char_value)
; rcx = buffer structure (2080 bytes)
; rdx = byte offset where to insert
; r8b = character value (ASCII)
; Returns: Nothing
; Side effects: Shifts content right, inserts char, updates length
TextBuffer_InsertChar PROTO :PTR, :QWORD, :BYTE

;----

; ---- Delete Character at Position ----
; void TextBuffer_DeleteChar(buffer_ptr, position)
; rcx = buffer structure
; rdx = byte offset to delete
; Returns: Nothing
; Side effects: Shifts content left, removes char, updates length
TextBuffer_DeleteChar PROTO :PTR, :QWORD

;----

; ---- Convert Integer to ASCII String ----
; LPSTR TextBuffer_IntToAscii(integer_value, buffer_ptr, radix)
; rcx = integer to convert
; rdx = output buffer (at least 32 bytes)
; r8d = radix (10 for decimal, 16 for hex)
; Returns: rax = pointer to output buffer
; Useful for line numbers in rendering
TextBuffer_IntToAscii PROTO :QWORD, :PTR, :QWORD

;==============================================================================
; ==================== AI COMPLETION ENGINE API ======================================
;==============================================================================

; ---- Insert Single Token Character ----
; void Completion_InsertToken(buffer_ptr, token_byte, cursor_ptr)
; rcx = buffer structure
; rdx = character value from AI model
; r8  = cursor structure
; Returns: Nothing
; Side effects: Calls TextBuffer_InsertChar + Cursor_MoveRight
; Use for: Real-time character-by-character AI token streaming
Completion_InsertToken PROTO :PTR, :BYTE, :PTR

;----

; ---- Insert Token String ----
; void Completion_InsertTokenString(buffer_ptr, token_string, cursor_ptr)
; rcx = buffer structure
; rdx = null-terminated string from AI model
; r8  = cursor structure
; Returns: Nothing
; Loops: Completion_InsertToken for each character in string
Completion_InsertTokenString PROTO :PTR, :PTR, :PTR

;----

; ---- Accept/Finalize Completion ----
; void Completion_AcceptSelection(buffer_ptr, start_pos, end_pos)
; rcx = buffer structure
; rdx = selection start offset
; r8  = selection end offset
; Returns: Nothing
; Side effects: Clears selection markers, commits completion
Completion_AcceptSelection PROTO :PTR, :QWORD, :QWORD

;----

; ---- Stream Tokens from AI Model (Batch Insert) ----
; void Completion_Stream(buffer_ptr, token_array, token_count, cursor_ptr)
; rcx = buffer structure
; rdx = pointer to token array (byte array)
; r8  = number of tokens
; r9  = cursor structure
; Returns: Nothing
; Batch alternative: Loops Completion_InsertToken for all tokens
; Use for: Processing entire completion at once
Completion_Stream PROTO :PTR, :PTR, :QWORD, :PTR

;==============================================================================
; ==================== CLIPBOARD API ============================================
;==============================================================================

; ---- Cut to Clipboard ----
; void EditorWindow_Cut(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: Gets selection, OpenClipboard, SetClipboardData,
;               deletes selection from buffer
EditorWindow_Cut PROTO :PTR

;----

; ---- Copy to Clipboard ----
; void EditorWindow_Copy(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: Gets selection, OpenClipboard, SetClipboardData
;               (does NOT delete from buffer)
EditorWindow_Copy PROTO :PTR

;----

; ---- Paste from Clipboard ----
; void EditorWindow_Paste(window_ptr)
; rcx = editor_window structure
; Returns: Nothing
; Side effects: OpenClipboard, GetClipboardData, calls
;               Completion_InsertTokenString with clipboard content
EditorWindow_Paste PROTO :PTR

;==============================================================================
; ==================    QUICK CALL EXAMPLES    ======================================
;==============================================================================

; --- EXAMPLE 1: Initialize Editor ---
example_init:
    ; Allocate structures:
    sub rsp, 296                    ; struct sizes: 96+96+2080 = 2272
    
    lea r12, [rsp]                  ; r12 = editor_window
    lea r13, [r12 + 96]             ; r13 = cursor
    lea r14, [r13 + 96]             ; r14 = buffer
    
    ; Register window class
    call EditorWindow_RegisterClass
    
    ; Create window
    lea rcx, [r12]
    lea rdx, [title_string]
    call EditorWindow_Create
    
    ; hwnd now in [r12]
    
    add rsp, 296
    ret

; --- EXAMPLE 2: Handle Keyboard Input ---
example_keydown:
    ; rcx = editor_window ptr (from WM_KEYDOWN handler)
    ; rdx = vk_code (from lParam)
    
    call EditorWindow_HandleKeyDown
    
    ; Gets cursor from [rcx + 24]
    ; Gets buffer from [rcx + 32]
    ; Calls appropriate Cursor_Move* or TextBuffer_Delete*
    
    ; Trigger repaint:
    mov rax, [rcx]                  ; hwnd
    mov rcx, rax
    xor edx, edx                    ; NULL rect (entire window)
    mov r8d, 0
    call InvalidateRect             ; Windows API
    ret

; --- EXAMPLE 3: AI Token Stream ---
example_ai_stream:
    ; rcx = buffer_ptr
    ; rdx = token string from neural network
    ; r8 = cursor_ptr
    
    call Completion_InsertTokenString
    
    ; All tokens inserted and cursor advanced
    ; WM_TIMER will trigger repaint
    ret

; --- EXAMPLE 4: File Open ---
example_file_open:
    ; rcx = editor_window ptr
    
    lea r15, [file_buffer]          ; Save buffer_ptr
    lea r14, [cursor_struct]        ; Save cursor_ptr
    
    call EditorWindow_FileOpen
    ; rax now contains filename or NULL
    
    test rax, rax
    jz .cancelled
    
    ; Load file into buffer...
    
    .cancelled:
    ret

; === END OF EXAMPLES ===

; Key Info for asm21 developers:
; - All structures passed by reference (pointer in rcx/rdx/r8/r9)
; - All calls use FRAME prologue/ENDPROLOG (x64 stack discipline)
; - Window handle (HWND) stored at [window_ptr + 0]
; - Device context (HDC) stored at [window_ptr + 8]
; - Never call these directly from Python - use C wrapper DLL
; - All sizes in bytes: window=96, cursor=96, buffer=2080
; - Text data pointer at [buffer_ptr + 0]
; - Text length at [buffer_ptr + 16]
; - Cursor byte offset at [cursor_ptr + 0]
; - Menu command IDs: 1001=Open, 1002=Save, 1003=Exit,
;   2001=Cut, 2002=Copy, 2003=Paste
