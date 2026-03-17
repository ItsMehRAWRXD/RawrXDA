; ============================================================================
; RawrXD_TextEditor_DisplayIntegration.asm
; Integration of rendering and syntax highlighting with editor display
; ============================================================================

OPTION CASEMAP:NONE

; Import renderer and syntax modules
EXTERN Renderer_Initialize:PROC
EXTERN Renderer_DrawLine:PROC
EXTERN Renderer_DrawCursor:PROC
EXTERN Renderer_ClearRect:PROC
EXTERN Renderer_RenderWindow:PROC
EXTERN Renderer_DrawTextWithSyntax:PROC

EXTERN SyntaxHighlighter_IsKeyword:PROC
EXTERN SyntaxHighlighter_IsRegister:PROC
EXTERN SyntaxHighlighter_AnalyzeLine:PROC
EXTERN SyntaxHighlighter_GetColorForToken:PROC

; Import text buffer and cursor APIs
EXTERN TextBuffer_GetLineCount:PROC
EXTERN TextBuffer_GetLine:PROC
EXTERN TextBuffer_GetLineLength:PROC
EXTERN TextBuffer_GetCharAt:PROC

EXTERN Cursor_GetLine:PROC
EXTERN Cursor_GetColumn:PROC

; Win32 APIs
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN InvalidateRect:PROC
EXTERN GetClientRect:PROC

.data
    ; Display constants
    COLOR_BACKGROUND    equ 0xFFFFFF    ; White background
    COLOR_TEXT          equ 0x000000    ; Black text
    COLOR_KEYWORD       equ 0x0000FF    ; Blue keywords
    COLOR_REGISTER      equ 0xFF0000    ; Red registers
    COLOR_COMMENT       equ 0x008000    ; Green comments
    COLOR_STRING        equ 0xFF00FF    ; Magenta strings
    COLOR_NUMBER        equ 0x00FFFF    ; Yellow numbers
    COLOR_CURSOR        equ 0x000000    ; Black cursor
    COLOR_LINENUM       equ 0x808080    ; Gray line numbers
    
    CHAR_HEIGHT         equ 16          ; Pixels per line
    CHAR_WIDTH          equ 8           ; Pixels per character (monospace)
    LINE_MARGIN         equ 40          ; Left margin for line numbers
    
    ; Global display state
    ALIGN 16
    g_hwndEditor        dq 0            ; Editor window handle
    g_hdcEditor         dq 0            ; Device context
    g_buffer_ptr        dq 0            ; Text buffer pointer
    g_cursor_ptr        dq 0            ; Cursor pointer
    g_client_width      dd 800
    g_client_height     dd 600
    g_scroll_x          dd 0            ; Horizontal scroll offset
    g_scroll_y          dd 0            ; Vertical scroll offset
    
    ; Debug strings
    szRenderStart       db "[DISPLAY] Rendering window...", 0
    szRenderComplete    db "[DISPLAY] Render complete", 0
    szRenderError       db "[DISPLAY] Render failed - no DC", 0

.code

; ============================================================================
; DisplayIntegration_UpdateWindow(rcx = hwnd)
;
; Trigger a complete window redraw
; ============================================================================
DisplayIntegration_UpdateWindow PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                        ; rbx = hwnd
    
    ; Invalidate entire window to force WM_PAINT
    lea rcx, [rsp]                      ; NULL rect
    mov qword [rcx], 0
    mov rdx, rbx
    mov r8d, 0
    call InvalidateRect
    
    add rsp, 32
    pop rbx
    ret
    
DisplayIntegration_UpdateWindow ENDP


; ============================================================================
; DisplayIntegration_OnPaint(rcx = hwnd, rdx = hdc)
;
; Main paint handler - orchestrates all rendering
; ============================================================================
DisplayIntegration_OnPaint PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 64
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov rbx, rcx                        ; rbx = hwnd
    mov r12, rdx                        ; r12 = hdc
    mov [g_hwndEditor], rbx
    mov [g_hdcEditor], r12
    
    test r12, r12
    jz .RenderFail
    
    ; Get window client area
    lea rcx, [rsp]
    mov rdx, rbx
    call GetClientRect
    
    mov eax, [rsp + 8]
    mov [g_client_width], eax
    mov eax, [rsp + 12]
    mov [g_client_height], eax
    
    ; Clear background
    mov rcx, r12                        ; hdc
    xor edx, edx                        ; left = 0
    xor r8d, r8d                        ; top = 0
    mov r9d, [g_client_width]           ; right
    mov rax, [g_client_height]
    mov qword [rsp + 32], rax           ; bottom
    mov qword [rsp + 40], COLOR_BACKGROUND
    
    call Renderer_ClearRect
    
    ; Render all visible lines
    mov rcx, [g_buffer_ptr]
    mov rdx, [g_cursor_ptr]
    
    test rcx, rcx
    jz .RenderComplete
    
    ; Get line count
    call TextBuffer_GetLineCount
    mov r13d, eax                       ; r13d = line count
    
    ; Setup rendering loop
    xor r14d, r14d                      ; r14d = line counter
    mov r15d, 0                         ; r15d = y position
    
.LineRenderLoop:
    ; Check if within visible area
    cmp r15d, [g_client_height]
    jge .RenderComplete
    
    cmp r14d, r13d
    jge .RenderComplete
    
    ; Get line from buffer
    mov rcx, [g_buffer_ptr]
    mov edx, r14d
    call TextBuffer_GetLine
    
    ; Get line length
    mov rcx, [g_buffer_ptr]
    mov edx, r14d
    call TextBuffer_GetLineLength
    mov r10d, eax                       ; r10d = line length
    
    ; Draw line number
    mov rcx, r12                        ; hdc
    mov edx, r14d
    inc edx                             ; 1-based
    
    ; Format number and draw (simplified - would convert to string)
    
    ; Draw line content with syntax highlighting
    mov rcx, r12                        ; hdc
    mov rdx, rax                        ; line text
    mov r8d, r10d                       ; line length
    mov r9d, LINE_MARGIN                ; x start position
    mov r10d, r15d                      ; y position (current line's Y)
    mov r11d, [g_client_width]          ; max width
    
    ; Call renderer to draw line with syntax
    call DisplayIntegration_RenderLineWithSyntax
    
    ; Draw cursor if on this line
    mov ecx, [r13]                      ; Get cursor line (simplified)
    cmp ecx, r14d
    jne .SkipCursorRender
    
    ; Get cursor column
    mov rcx, [g_cursor_ptr]
    call Cursor_GetColumn
    mov r8d, eax                        ; r8d = column
    
    ; Draw cursor
    mov rcx, r12                        ; hdc
    mov edx, LINE_MARGIN
    mov eax, r8d
    imul eax, CHAR_WIDTH
    add edx, eax                        ; x = margin + column * width
    mov r8d, r15d                       ; y position
    mov r9d, CHAR_HEIGHT                ; height
    
    call Renderer_DrawCursor
    
.SkipCursorRender:
    ; Next line
    add r15d, CHAR_HEIGHT
    inc r14d
    jmp .LineRenderLoop
    
.RenderComplete:
    mov rax, 1
    jmp .RenderDone
    
.RenderFail:
    lea rcx, [rel szRenderError]
    call OutputDebugStringA
    xor eax, eax
    
.RenderDone:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret
    
DisplayIntegration_OnPaint ENDP


; ============================================================================
; DisplayIntegration_RenderLineWithSyntax(rcx = hdc, rdx = line_text,
;                                         r8d = line_length, r9d = x_pos,
;                                         r10d = y_pos, r11d = max_width)
;
; Render a single line with syntax highlighting
; ============================================================================
DisplayIntegration_RenderLineWithSyntax PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 96
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 96
    
    mov rbx, rcx                        ; rbx = hdc
    mov r12, rdx                        ; r12 = line_text
    mov r13d, r8d                       ; r13d = line_length
    mov r14d, r9d                       ; r14d = x_pos
    mov r15d, r10d                      ; r15d = y_pos
    
    xor ecx, ecx                        ; ecx = current x position
    xor edx, edx                        ; edx = current character index
    
.CharLoop:
    cmp edx, r13d
    jge .LineRenderDone
    
    cmp ecx, r11d
    jge .LineRenderDone                 ; Past right edge
    
    movzx eax, byte [r12 + rdx]
    
    ; Skip null terminator and newline
    test al, al
    jz .LineRenderDone
    cmp al, 10
    je .LineRenderDone
    cmp al, 13
    je .LineRenderDone
    
    ; Get token type for this character
    lea rcx, [r12 + rdx]
    mov r8d, r13d
    sub r8d, edx
    call SyntaxHighlighter_IsKeyword
    
    mov r9d, eax                        ; r9d = color
    
    ; Draw character
    mov rcx, rbx                        ; hdc
    mov rdx, r14d
    add rdx, rcx                        ; x position
    mov r8d, r15d                       ; y position
    mov r9, [rsp]                       ; character pointer
    mov r10d, 1                         ; length = 1
    
    ; Call Renderer_DrawLine to output character
    mov qword [rsp + 32], r8            ; y_pos
    mov qword [rsp + 40], r9d           ; color
    mov qword [rsp + 56], 1             ; text_length = 1
    
    call Renderer_DrawLine
    
    ; Move to next character
    add ecx, CHAR_WIDTH
    inc edx
    jmp .CharLoop
    
.LineRenderDone:
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    ret
    
DisplayIntegration_RenderLineWithSyntax ENDP


; ============================================================================
; DisplayIntegration_SetBufferAndCursor(rcx = buffer_ptr, rdx = cursor_ptr)
;
; Update display references to buffer and cursor
; ============================================================================
DisplayIntegration_SetBufferAndCursor PROC FRAME
    
    mov [g_buffer_ptr], rcx
    mov [g_cursor_ptr], rdx
    ret
    
DisplayIntegration_SetBufferAndCursor ENDP


; ============================================================================
; DisplayIntegration_GetWindowHandle()
;
; Get current editor window handle
; Returns: rax = hwnd
; ============================================================================
DisplayIntegration_GetWindowHandle PROC FRAME
    
    mov rax, [g_hwndEditor]
    ret
    
DisplayIntegration_GetWindowHandle ENDP


; ============================================================================
; DisplayIntegration_SetScrollOffset(rcx = x_offset, edx = y_offset)
;
; Set viewport scroll position
; ============================================================================
DisplayIntegration_SetScrollOffset PROC FRAME
    
    mov [g_scroll_x], ecx
    mov [g_scroll_y], edx
    ret
    
DisplayIntegration_SetScrollOffset ENDP


; ============================================================================
; DisplayIntegration_GetScrollOffset()
;
; Get current viewport scroll position
; Returns: rax = x_offset, rdx = y_offset
; ============================================================================
DisplayIntegration_GetScrollOffset PROC FRAME
    
    mov eax, [g_scroll_x]
    mov edx, [g_scroll_y]
    ret
    
DisplayIntegration_GetScrollOffset ENDP


; ============================================================================
; Debug/Info functions
; ============================================================================
EXTERN OutputDebugStringA:PROC


END
