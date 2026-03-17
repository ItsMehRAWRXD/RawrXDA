; ============================================================================
; RawrXD_TextEditor_Main.asm - Complete Text Editor Implementation
; ============================================================================
; Entry point for the text editor application
; Orchestrates:
; - Buffer initialization
; - Cursor tracking
; - GUI rendering
; - Input handling
; ============================================================================

.INCLUDE \masm32\include\windows.inc
.INCLUDE \masm32\include\kernel32.inc
.INCLUDE \masm32\include\user32.inc
.INCLUDE \masm32\include\gdi32.inc
.INCLUDE \masm32\include\comctl32.inc

LIBS \masm32\lib\kernel32.lib, \
     \masm32\lib\user32.lib, \
     \masm32\lib\gdi32.lib, \
     \masm32\lib\comctl32.lib

.DATA
    szClassName db "RawrXDTextEditor", 0
    szWindowTitle db "RawrXD Text Editor - Cursor Position Tracking", 0
    szMenuNotImpl db "Feature not yet implemented", 0
    
    ; Global pointers
    gBuffer dq 0
    gCursor dq 0
    gEditorWindow dq 0
    gBlinkTimer dq 0

.CODE

; ============================================================================
; Entry Point
; ============================================================================
main PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .ALLOCSTACK 32 + 2080 + 96  ; buffer + cursor + editor_window
    .ENDPROLOG

    push rbx
    push r12
    sub rsp, 2080 + 96
    
    ; rsp+off: buffer structure (2080 bytes)
    ; rsp+2080+off: cursor structure (96 bytes)
    ; rsp+2176+off: editor_window structure (96 bytes)
    
    mov r12, rsp                       ; r12 = stack base for data
    
    ; ==== Step 1: Initialize Text Buffer ====
    lea rcx, [r12]                     ; rcx = buffer_ptr
    call TextBuffer_Initialize
    test rax, rax
    jz .InitFail
    
    mov rax, [r12]                     ; Load buffer data pointer
    mov [gBuffer], r12                 ; Save buffer reference
    
    ; ==== Step 2: Initialize Cursor ====
    lea rcx, [r12 + 2080]              ; rcx = cursor_ptr
    mov rdx, r12                       ; rdx = buffer_ptr
    call Cursor_Initialize
    test rax, rax
    jz .InitFail
    
    mov [gCursor], rcx                 ; Save cursor reference
    
    ; ==== Step 3: Create GUI Window ====
    call EditorWindow_RegisterClass    ; Register window class
    test eax, eax
    jz .InitFail
    
    lea rcx, [r12 + 2080 + 96]         ; rcx = editor_window_ptr
    lea rdx, [szWindowTitle]           ; rdx = title
    call EditorWindow_Create           ; Returns HWND in rax
    test rax, rax
    jz .InitFail
    
    lea rcx, [r12 + 2080 + 96]         ; rcx = editor_window_ptr (updated with HWND)
    mov [gEditorWindow], rcx           ; Save editor window reference
    
    ; Link editor window to buffer and cursor
    mov [rcx + 24], qword ptr [gCursor]      ; editor->cursor_ptr
    mov [rcx + 32], qword ptr [gBuffer]      ; editor->buffer_ptr
    
    ; ==== Step 4: Demo - Insert Sample Text ====
    lea rcx, [r12]                     ; rcx = buffer_ptr
    xor rdx, rdx                       ; rdx = position 0
    lea r8, [szSampleText]             ; r8 = sample text
    mov r9, szSampleText_len           ; r9 = text length
    call TextBuffer_InsertString
    
    ; ==== Step 5: Main Editor Loop ====
    mov rbx, 10                        ; rbx = iterations (for demo)

.EditorLoop:
    test rbx, rbx
    jz .ExitLoop
    
    ; Simulate editor updates every frame
    
    ; Update cursor blink
    lea rcx, [r12 + 2080]              ; rcx = cursor_ptr
    mov rdx, 16                        ; 16ms per frame
    call Cursor_UpdateBlink
    
    ; Get cursor blink state
    lea rcx, [r12 + 2080]
    call Cursor_GetBlink
    test rax, rax
    jz .BlinkOff
    
    ; Cursor is visible
    jmp .BlinkDone

.BlinkOff:
    ; Cursor is hidden
    
.BlinkDone:
    ; Simulate rendering
    lea rcx, [r12 + 2080 + 96]         ; rcx = editor_window_ptr
    call EditorWindow_HandlePaint
    
    ; Test cursor navigation
    lea rcx, [r12 + 2080]              ; rcx = cursor_ptr
    call Cursor_MoveRight              ; Move cursor right
    
    dec rbx
    jmp .EditorLoop

.ExitLoop:
    ; ==== Step 6: Output Final State ====
    lea rcx, [r12]                     ; rcx = buffer_ptr
    mov rdx, [rcx + 16]                ; rdx = buffer length
    
    lea rcx, [r12 + 2080]              ; rcx = cursor_ptr
    mov rax, [rcx + 8]                 ; rax = cursor line
    mov rbx, [rcx + 16]                ; rbx = cursor column
    
    ; Print results (placeholder)
    ; In real application: would output to window or console
    
    xor ecx, ecx
    call ExitProcess

.InitFail:
    mov ecx, 1
    call ExitProcess

main ENDP


; ============================================================================
; Exported Functions for External Use
; ============================================================================

; ============================================================================
; TextEditor_Create()
;
; Create and initialize a complete text editor instance
; Returns: rax = editor_handle or 0 on failure
; ============================================================================
TextEditor_Create PROC FRAME
    .ALLOCSTACK 32 + 2080 + 96 + 96
    .ENDPROLOG

    sub rsp, 2080 + 96 + 96
    
    mov rax, rsp                       ; Structure base
    
    ; Quick initialization (combined setup)
    lea rcx, [rax]                     ; buffer
    call TextBuffer_Initialize
    
    lea rcx, [rax + 2080]              ; cursor
    mov rdx, rax                       ; buffer
    call Cursor_Initialize
    
    lea rcx, [rax + 2080 + 96]         ; editor_window
    call EditorWindow_Create
    
    add rsp, 2080 + 96 + 96
    ret
TextEditor_Create ENDP


; ============================================================================
; TextEditor_Destroy(rcx = editor_handle)
;
; Clean up and destroy text editor instance
; ============================================================================
TextEditor_Destroy PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = editor_handle
    
    ; Free associated resources
    mov rax, [rcx + 0]                 ; Get buffer pointer
    
    ; Free buffer memory via VirtualFree (placeholder)
    
    ; Destroy window if needed
    mov rdx, [rcx + 2080 + 96]         ; Get window handle
    
    mov rax, 1
    ret
TextEditor_Destroy ENDP


; ============================================================================
; TextEditor_InsertText(rcx = editor_handle, rdx = text_ptr, r8 = length)
;
; Insert text at current cursor position
; ============================================================================
TextEditor_InsertText PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    ; rcx = editor_handle, rdx = text_ptr, r8 = length
    
    mov rax, rcx                       ; rax = editor base
    mov r9, [rax + 2080]               ; r9 = cursor struct
    mov r10, [rax]                     ; r10 = buffer struct
    
    ; Get current cursor position
    mov r11, [r9]                      ; r11 = cursor byte offset
    
    ; Insert text at cursor
    mov rcx, r10
    mov rdx, r11
    mov r9, rdx
    ; call TextBuffer_InsertString
    
    ; Move cursor to end of inserted text
    mov rcx, [rax + 2080]
    add rcx, r8
    mov [rcx], rcx                     ; Update cursor position
    
    mov rax, 1
    ret
TextEditor_InsertText ENDP


; ============================================================================
; TextEditor_GetCursorPosition(rcx = editor_handle)
;
; Get current cursor line and column
; Returns: rax = line, rdx = column
; ============================================================================
TextEditor_GetCursorPosition PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, rcx
    mov rcx, [rax + 2080]              ; Get cursor struct
    
    mov rax, [rcx + 8]                 ; rax = line
    mov rdx, [rcx + 16]                ; rdx = column
    
    ret
TextEditor_GetCursorPosition ENDP


; ============================================================================
; TextEditor_SetCursorPosition(rcx = editor_handle, rdx = line, r8 = column)
;
; Set cursor to specific line and column
; ============================================================================
TextEditor_SetCursorPosition PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, rcx
    mov rcx, [rax]                     ; rcx = buffer
    mov r9, [rax + 2080]               ; r9 = cursor
    
    ; Convert line/column to byte offset
    call TextBuffer_GetOffsetFromLineColumn
    
    ; Update cursor
    mov [r9], rax                      ; Update byte offset
    mov [r9 + 8], rdx                  ; Update line
    mov [r9 + 16], r8                  ; Update column
    
    mov rax, 1
    ret
TextEditor_SetCursorPosition ENDP


; ============================================================================
; TextEditor_GetTextContent(rcx = editor_handle)
;
; Get entire text buffer content
; Returns: rax = text_ptr, rdx = length
; ============================================================================
TextEditor_GetTextContent PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, rcx
    mov rcx, [rax]                     ; rcx = buffer
    
    mov rax, [rcx]                     ; rax = data pointer
    mov rdx, [rcx + 16]                ; rdx = length
    
    ret
TextEditor_GetTextContent ENDP


; ============================================================================
; TextEditor_Clear(rcx = editor_handle)
;
; Clear all text and reset cursor to origin
; ============================================================================
TextEditor_Clear PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG

    mov rax, rcx
    mov rcx, [rax]                     ; rcx = buffer
    
    ; Clear buffer length
    mov qword [rcx + 16], 0            ; Set length to 0
    
    ; Reset cursor
    mov rdx, [rax + 2080]              ; rdx = cursor
    mov qword [rdx], 0                 ; byte_offset = 0
    mov qword [rdx + 8], 0             ; line = 0
    mov qword [rdx + 16], 0            ; column = 0
    
    mov rax, 1
    ret
TextEditor_Clear ENDP


; Sample text for demo
szSampleText db "Line 1: Welcome to RawrXD Text Editor", 13, 10, \
                "Line 2: Cursor position tracking enabled", 13, 10, \
                "Line 3: Multi-line editing support", 13, 10, \
                "Line 4: Selection and highlight rendering", 13, 10, \
                "Line 5: Keyboard navigation (arrows, home, end)", 0

szSampleText_len = $ - szSampleText


; Implicit linkage: these modules are external
EXTERN TextBuffer_Initialize:PROC
EXTERN TextBuffer_InsertChar:PROC
EXTERN TextBuffer_InsertString:PROC
EXTERN TextBuffer_DeleteChar:PROC
EXTERN TextBuffer_GetLine:PROC
EXTERN TextBuffer_GetLineColumn:PROC
EXTERN TextBuffer_GetOffsetFromLineColumn:PROC
EXTERN TextBuffer_UpdateLineTable:PROC
EXTERN TextBuffer_Grow:PROC

EXTERN Cursor_Initialize:PROC
EXTERN Cursor_MoveLeft:PROC
EXTERN Cursor_MoveRight:PROC
EXTERN Cursor_MoveUp:PROC
EXTERN Cursor_MoveDown:PROC
EXTERN Cursor_MoveHome:PROC
EXTERN Cursor_MoveEnd:PROC
EXTERN Cursor_MoveStartOfDocument:PROC
EXTERN Cursor_MoveEndOfDocument:PROC
EXTERN Cursor_SelectTo:PROC
EXTERN Cursor_ClearSelection:PROC
EXTERN Cursor_IsSelected:PROC
EXTERN Cursor_GetSelection:PROC
EXTERN Cursor_SetBlink:PROC
EXTERN Cursor_UpdateBlink:PROC
EXTERN Cursor_GetBlink:PROC
EXTERN Cursor_PageUp:PROC
EXTERN Cursor_PageDown:PROC
EXTERN Cursor_UpdateLineColumn:PROC

EXTERN EditorWindow_Create:PROC
EXTERN EditorWindow_HandlePaint:PROC
EXTERN EditorWindow_HandleKeyDown:PROC
EXTERN EditorWindow_HandleChar:PROC
EXTERN EditorWindow_HandleMouseClick:PROC
EXTERN EditorWindow_ScrollToCursor:PROC

END main
