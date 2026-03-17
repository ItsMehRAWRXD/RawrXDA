; ============================================================================
; WinMain.asm - RawrXD TextEditorGUI Integration Example
; ============================================================================
; Demonstrates proper usage of IDE integration APIs from RawrXD_TextEditorGUI.asm
; ============================================================================
; Integration Patterns:
;   1. IDE_CreateMainWindow() - Single-call window creation
;   2. IDE_SetupAccelerators() - Wire keyboard shortcuts
;   3. IDE_MessageLoop() - Main event loop with accelerators
;   4. AI integration hooks - Buffer snapshot & token insertion
; ============================================================================

.DATA
    ; Window data structure (96+ bytes)
    window_data QWORD 0                ; hwnd (filled by Create)
    QWORD 0                            ; hdc
    QWORD 0                            ; hfont
    QWORD 0                            ; cursor_ptr
    QWORD 0                            ; buffer_ptr
    DWORD 8                            ; char_width
    DWORD 16                           ; char_height
    DWORD 800                          ; client_width
    DWORD 600                          ; client_height
    DWORD 40                           ; line_num_width
    DWORD 0                            ; scroll_offset_x
    DWORD 0                            ; scroll_offset_y
    QWORD 0                            ; hbitmap
    QWORD 0                            ; hmemdc
    DWORD 0                            ; timer_id
    QWORD 0                            ; hToolbar
    QWORD 0                            ; hAccel (filled by SetupAccelerators)
    QWORD 0                            ; hStatusBar
    
    window_title db "RawrXD Text Editor", 0
    
    ; AI Completion Example Data
    ai_snapshot_buffer db 65536 dup(0)  ; 64KB text snapshot for AI
    ai_tokens BYTE 256 dup(0)           ; Token buffer from AI model
    ai_token_count DWORD 0              ; Number of tokens
    
.CODE

; ============================================================================
; WinMain(rcx=hInstance, rdx=hPrevInstance, r8=lpCmdLine, r9d=nCmdShow)
; Entry point for Windows GUI application
; ============================================================================
WinMain PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 32
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    ; rcx = hInstance, rdx = hPrevInstance, r8 = lpCmdLine, r9d = nCmdShow
    ; (Note: This procedure receives parameters in x64 calling convention)
    
    ; ========================================================================
    ; STEP 1: CREATE MAIN EDITOR WINDOW
    ; ========================================================================
    
    lea rcx, [rel window_title]        ; rcx = "RawrXD Text Editor"
    lea rdx, [rel window_data]         ; rdx = window_data_ptr (96+ bytes)
    call IDE_CreateMainWindow          ; Creates window, menus, toolbar, status bar
    
    test rax, rax
    jz .WindowCreateFailed             ; If NULL, window creation failed
    
    mov r12, rax                       ; r12 = hwnd (save for later use)
    
    ; ========================================================================
    ; STEP 2: SETUP KEYBOARD ACCELERATORS (Ctrl+N/O/S/Z/C/X)
    ; ========================================================================
    
    mov rcx, r12                       ; rcx = hwnd
    call IDE_SetupAccelerators         ; Sets up Ctrl+key shortcuts
    
    ; IMPORTANT: Store accelerator handle in window_data for message loop
    mov r13, rax                       ; r13 = hAccel
    lea rbx, [rel window_data]
    mov [rbx + 92], r13                ; Save at window_data[92]
    
    ; ========================================================================
    ; STEP 3: SHOW WINDOW
    ; ========================================================================
    
    mov rcx, r12                       ; hwnd
    mov edx, [r9d]                     ; nCmdShow (from parameter r9d)
    call ShowWindow
    
    ; Update window (repaint)
    mov rcx, r12
    call UpdateWindow
    
    ; ========================================================================
    ; STEP 4: DEMONSTRATE AI COMPLETION INTEGRATION (Optional Example)
    ; ========================================================================
    
    ; EXAMPLE: How to integrate AI completion into message loop
    ; This would typically be done in a separate thread or on user request
    
    ; ; Get current buffer snapshot for AI processing
    ; lea rcx, [rel text_buffer]        ; source: editor text buffer
    ; lea rdx, [rel ai_snapshot_buffer] ; dest: AI input
    ; call AICompletion_GetBufferSnapshot
    ; ; rax = buffer size
    ; 
    ; [... send ai_snapshot_buffer to AI backend API ...]
    ; [... receive tokens back ...]
    ; 
    ; ; Insert AI-generated tokens into buffer
    ; lea rcx, [rel text_buffer]
    ; lea rdx, [rel ai_tokens]          ; token array
    ; mov r8d, [ai_token_count]         ; number of tokens
    ; call AICompletion_InsertTokens
    ; ; Text now inserted at cursor, screen updated
    
    ; ========================================================================
    ; STEP 5: MAIN MESSAGE LOOP (Blocking until WM_QUIT)
    ; ========================================================================
    
    mov rcx, r12                       ; rcx = hwnd
    mov rdx, r13                       ; rdx = hAccel (accelerator table)
    call IDE_MessageLoop               ; BLOCKING: runs main event loop
    
    ; Returns when user closes window (WM_QUIT received)
    ; rax = exit code from PostQuitMessage
    
    ; ========================================================================
    ; CLEANUP (if needed)
    ; ========================================================================
    
    ; At this point, window is destroyed and all resources cleaned up
    ; by WndProc handlers (DestroyWindow sends WM_DESTROY)
    
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
    
.WindowCreateFailed:
    ; Window creation failed
    mov rcx, 0
    call MessageBoxA
    xor eax, eax
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

WinMain ENDP


; ============================================================================
; INTEGRATION PATTERN GLOSSARY
; ============================================================================

; ============================================================================
; Pattern 1: Basic Window Creation
; ============================================================================
; 
;   lea rcx, [title_string]
;   lea rdx, [window_data_buffer]    ; 96+ bytes
;   call IDE_CreateMainWindow
;   ; rax = hwnd or NULL
;   ; Window now created with menus, toolbar, status bar
;   
; Returns: hwnd in rax (or NULL on failure)
; Called: Once during app initialization
; ============================================================================

; ============================================================================
; Pattern 2: Setup Accelerators (Keyboard Shortcuts)
; ============================================================================
;
;   mov rcx, hwnd                     ; From IDE_CreateMainWindow
;   call IDE_SetupAccelerators
;   ; rax = hAccel (accelerator table handle)
;   ; Store for message loop
;   
; Returns: hAccel in rax
; Called: After window creation, before message loop
; IMPORTANT: Must pass hAccel to IDE_MessageLoop!
; ============================================================================

; ============================================================================
; Pattern 3: Main Message Loop (with Accelerators)
; ============================================================================
;
;   mov rcx, hwnd                     ; hwnd from window creation
;   mov rdx, hAccel                   ; hAccel from SetupAccelerators
;   call IDE_MessageLoop
;   ; Blocking call - returns when WM_QUIT received
;   ; rax = exit code
;
; Returns: Exit code in rax
; Called: Main application loop (blocking)
; Flow: GetMessage → TranslateAccelerator → DispatchMessage → repeat
; ============================================================================

; ============================================================================
; Pattern 4: AI Completion Integration
; ============================================================================
;
; 4a. Export buffer for AI processing:
;   lea rcx, [text_buffer_ptr]
;   lea rdx, [ai_input_buffer]        ; 64KB snapshot
;   call AICompletion_GetBufferSnapshot
;   ; rax = buffer size
;   ; Now ai_input_buffer contains current editor text
;
; 4b. Send to AI backend:
;   ; [Custom code to send ai_input_buffer to AI service]
;   ; [Wait for response with tokens]
;   ; [Parse tokens into array]
;
; 4c. Insert AI tokens into editor:
;   lea rcx, [text_buffer_ptr]
;   lea rdx, [token_array]            ; Byte array from AI
;   mov r8d, token_count              ; Number of tokens
;   call AICompletion_InsertTokens
;   ; rax = 1 (success) or 0 (buffer full)
;   ; Text inserted at cursor, screen auto-refreshed
;
; Total Flow: GetSnapshot → SendToAI → ParseResponse → InsertTokens
; ============================================================================

; ============================================================================
; KEYBOARD ACCELERATORS PROVIDED
; ============================================================================
; 
; Ctrl+N   → File > New (ID 0x1001)
; Ctrl+O   → File > Open (ID 0x1002)
; Ctrl+S   → File > Save (ID 0x1003)
; Ctrl+Z   → Edit > Undo (ID 0x2001)
; Ctrl+X   → Edit > Cut (ID 0x2002)
; Ctrl+C   → Edit > Copy (ID 0x2003)
; 
; These are automatically wired via IDE_SetupAccelerators()
; They route to EditorWindow_WndProc via WM_COMMAND messages
; ============================================================================

; ============================================================================
; WINDOW DATA STRUCTURE LAYOUT (96 bytes minimum)
; ============================================================================
;
; Offset  Size  Field
; 0       8     hwnd              - Window handle
; 8       8     hdc               - Device context
; 16      8     hfont             - Font handle
; 24      8     cursor_ptr        - Cursor structure pointer
; 32      8     buffer_ptr        - Text buffer pointer
; 40      4     char_width        - 8 pixels
; 44      4     char_height       - 16 pixels
; 48      4     client_width      - 800 pixels
; 52      4     client_height     - 600 pixels
; 56      4     line_num_width    - 40 pixels
; 60      4     scroll_offset_x   - Horizontal scroll
; 64      4     scroll_offset_y   - Vertical scroll
; 68      8     hbitmap           - Backbuffer bitmap
; 76      8     hmemdc            - Memory device context
; 84      4     timer_id          - Cursor blink timer
; 88      8     hToolbar          - Toolbar window
; 92      8     hAccel            - Accelerator table (SET BY SetupAccelerators)
; 96      8     hStatusBar        - Status bar window
;
; ============================================================================

; ============================================================================
; AI COMPLETION WORKFLOW EXAMPLE
; ============================================================================
;
; User types: "def hello"
;   ↓
; Background thread calls AICompletion_GetBufferSnapshot()
;   ↓ (gets: "def hello")
; Send to AI backend (e.g., llama.cpp, OpenAI API)
;   ↓
; AI returns tokens: [':','\n','    ','p','a','s','s']
;   ↓
; Main thread calls AICompletion_InsertTokens()
;   ↓
; Editor displays: "def hello:\n    pass"
;   ↓
; User can accept or edit further
;
; ============================================================================

; ============================================================================
; ERROR HANDLING EXAMPLES
; ============================================================================
;
; Check window creation:
;   call IDE_CreateMainWindow
;   test rax, rax              ; Returns NULL if failed
;   jz .CreateFailed
;
; Check token insertion:
;   call AICompletion_InsertTokens
;   test eax, eax              ; Returns 0 if buffer full
;   jz .InsertFailed
;
; Check buffer snapshot:
;   call AICompletion_GetBufferSnapshot
;   mov buffer_size, rax       ; Size in rax
;
; ============================================================================

; ============================================================================
; MENU COMMAND HANDLING (Future Enhancement)
; ============================================================================
;
; When user clicks File > Open (Ctrl+O):
;   1. EditorWindow_WndProc receives WM_COMMAND
;   2. wparam = 0x1002 (ID_FILE_OPEN)
;   3. Route to EditorWindow_FileOpen()
;   4. GetOpenFileNameA dialog appears
;   5. File loaded into text buffer
;   6. Screen auto-refreshed
;
; Current status: Menu items created, command routing in WndProc
; Future: Custom handlers for each menu item ID
;
; ============================================================================

; ============================================================================
; TOOLBAR BUTTON EXPANSION (Future Enhancement)
; ============================================================================
;
; Current: Toolbar window created at (x=0, y=0, width=800, height=28)
; Future: Add button loop to create:
;   - Open button (x=0)
;   - Save button (x=30)
;   - Cut button (x=60)
;   - Copy button (x=90)
;   - Paste button (x=120)
;   - Undo button (x=150)
;   - Redo button (x=180)
;
; Each button: CreateWindowExA with "BUTTON" class, BS_PUSHBUTTON style
;
; ============================================================================

; ============================================================================
; STATUS BAR UPDATE EXAMPLES
; ============================================================================
;
; Update to "Ready":
;   lea rcx, [window_data]
;   lea rdx, [status_text_ready]      ; "Ready"
;   call EditorWindow_UpdateStatus
;
; Update to line/column info:
;   lea rcx, [window_data]
;   lea rdx, [status_line_col]        ; "Line 42, Col 15"
;   call EditorWindow_UpdateStatus
;
; Update to file saved:
;   lea rcx, [window_data]
;   lea rdx, [status_saved]           ; "Saved"
;   call EditorWindow_UpdateStatus
;
; ============================================================================

END WinMain
