; ============================================================================
; File 26: text_renderer.asm - Double-buffered GDI rendering with token colors
; ============================================================================
; Purpose: Draw syntax-highlighted text with DirectWrite + GDI
; Uses: Offscreen DC/bitmap for double-buffering, token colors, custom overlays
; Functions: Init, BeginDraw, DrawLine, DrawGutter, EndDraw, GetCharMetrics, GetTokenColor
; ============================================================================

.code

; CONSTANTS
; ============================================================================

CHAR_WIDTH              equ 8
CHAR_HEIGHT             equ 16
GUTTER_WIDTH            equ 40

COLOR_KEYWORD           equ 0xFF9D76B6  ; Purple
COLOR_STRING            equ 0xFFFFA500  ; Orange
COLOR_COMMENT           equ 0xFF6A9955  ; Green
COLOR_IDENTIFIER        equ 0xFFD4D4D4  ; White
COLOR_NUMBER            equ 0xFFB5CEA8  ; Light green
COLOR_OPERATOR          equ 0xFF9CDCFE  ; Light blue
COLOR_BACKGROUND        equ 0xFF1E1E1E  ; Dark background
COLOR_SELECTION         equ 0xFF264F78  ; Selection blue
COLOR_ERROR             equ 0xFFFF6B6B  ; Red

; INITIALIZATION
; ============================================================================

Renderer_Init PROC USES rbx rcx rdx rsi rdi r8 r9 hwnd:QWORD, width:QWORD, height:QWORD
    ; hwnd = window handle
    ; width, height = client area dimensions
    ; Returns: Renderer* in rax
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    
    ; Allocate main struct (96 bytes)
    mov rdx, 0
    mov r8, 96
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = Renderer*
    
    ; Store hwnd and dimensions
    mov [rbx + 0], hwnd
    mov [rbx + 8], width
    mov [rbx + 16], height
    
    ; Create offscreen DC
    sub rsp, 40
    mov rcx, hwnd
    call GetDC
    add rsp, 40
    mov r8, rax  ; r8 = screen DC
    
    ; Create compatible DC
    sub rsp, 40
    mov rcx, r8
    call CreateCompatibleDC
    add rsp, 40
    mov [rbx + 24], rax  ; offscreenDC
    
    ; Create compatible bitmap
    mov r9, [rbx + 8]   ; width
    mov r10, [rbx + 16] ; height
    sub rsp, 40
    mov rcx, r8
    mov rdx, r9
    mov r8, r10
    call CreateCompatibleBitmap
    add rsp, 40
    mov [rbx + 32], rax  ; offscreenBitmap
    
    ; Select bitmap into DC
    mov rcx, [rbx + 24]  ; offscreenDC
    mov rdx, [rbx + 32]  ; offscreenBitmap
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    call SelectObject
    add rsp, 40
    
    ; Create font (monospace)
    mov r8, 0
    mov r9, 0
    mov r10, -12  ; font height (negative = pixels)
    mov r11, 0
    sub rsp, 80
    mov rcx, r8
    mov rdx, r9
    mov r8, r10
    mov r9, r11
    mov [rsp + 32], r10
    mov [rsp + 40], r11
    mov [rsp + 48], 400  ; weight (normal)
    mov [rsp + 56], 0    ; italic
    mov [rsp + 64], 0    ; underline
    mov [rsp + 72], 0    ; strikeout
    call CreateFontA
    add rsp, 80
    
    mov [rbx + 40], rax  ; hFont
    
    ; Clear offscreen buffer
    mov rcx, [rbx + 24]
    mov rdx, 0xFFFFFFFF  ; all white
    mov r8, 0
    mov r9, [rbx + 8]
    mov r10, [rbx + 16]
    
    ; PatBlt to clear
    sub rsp, 40
    mov rcx, rcx
    mov rdx, 0
    mov r8, 0
    mov r9, r9
    mov r10, r10
    call PatBlt
    add rsp, 40
    
    mov rax, rbx
    ret
Renderer_Init ENDP

; BEGIN DRAW FRAME
; ============================================================================

Renderer_BeginDraw PROC USES rbx rcx rdx rsi rdi renderer:PTR DWORD
    ; Clear offscreen buffer for new frame
    
    mov rcx, renderer
    mov rax, [rcx + 24]  ; offscreenDC
    
    ; Clear to background color
    mov r8, COLOR_BACKGROUND
    
    sub rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 0
    mov r9, [renderer + 8]
    mov r10, [renderer + 16]
    call PatBlt
    add rsp, 40
    
    ret
Renderer_BeginDraw ENDP

; DRAW LINE WITH TOKENS
; ============================================================================

Renderer_DrawLine PROC USES rbx rcx rdx rsi rdi r8 r9 renderer:PTR DWORD, lineNum:QWORD, tokenArray:PTR DWORD
    ; renderer = Renderer*
    ; lineNum = line number
    ; tokenArray = Token* array
    ; Tokens: { type, position, length, color }
    
    mov rcx, renderer
    mov rsi, tokenArray
    mov rax, 0  ; token index
    
    ; Calculate Y position
    mov r8, lineNum
    mov r9, CHAR_HEIGHT
    imul r8, r9
    
@token_loop:
    cmp qword ptr [rsi + rax*32], 0  ; check if token.type == 0 (end of tokens)
    je @tokens_done
    
    ; Get token properties
    mov r10d, dword ptr [rsi + rax*32 + 0]   ; type
    mov r11, qword ptr [rsi + rax*32 + 8]    ; position
    mov r12, qword ptr [rsi + rax*32 + 16]   ; length
    
    ; Get color based on type
    mov edx, r10d
    call Renderer_GetTokenColor
    mov r13, rax  ; r13 = color
    
    ; Calculate X position
    mov rdi, r11
    mov r9, CHAR_WIDTH
    imul rdi, r9
    add rdi, GUTTER_WIDTH
    
    ; Set text color
    mov rcx, [renderer + 24]  ; offscreenDC
    mov edx, r13d
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    call SetTextColor
    add rsp, 40
    
    ; Draw text (stub - would need actual text from buffer)
    mov rcx, [renderer + 24]
    mov rdx, rdi
    mov r8, r8
    mov r9, 0     ; text pointer
    mov r10, r12  ; length
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    mov r8, r8
    mov r9, r9
    mov r10, r10
    call TextOutA
    add rsp, 40
    
    inc rax
    jmp @token_loop
    
@tokens_done:
    ret
Renderer_DrawLine ENDP

; DRAW GUTTER (Line Numbers)
; ============================================================================

Renderer_DrawGutter PROC USES rbx rcx rdx rsi rdi r8 renderer:PTR DWORD, startLine:QWORD, endLine:QWORD
    ; renderer = Renderer*
    ; startLine, endLine = range of line numbers to draw
    
    mov rcx, renderer
    mov rax, startLine
    
@gutter_loop:
    cmp rax, endLine
    jg @gutter_done
    
    ; Calculate Y position
    mov rdx, rax
    mov r8, CHAR_HEIGHT
    imul rdx, r8
    
    ; Format line number as string (wsprintf)
    mov r9, 4096  ; temp buffer size
    sub rsp, 96
    lea rdi, [rsp + 0]  ; buffer
    lea rsi, [rel line_num_format]
    
    ; wsprintf(buffer, "%4d", line)
    mov rcx, rdi
    mov rdx, rsi
    mov r8, rax
    call wsprintf
    add rsp, 96
    
    ; Draw text
    mov rcx, [renderer + 24]  ; offscreenDC
    mov rdx, 2
    mov r8, [rsp]
    mov r9, rax  ; string length
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    mov r8, rdx
    mov r9, r9
    call TextOutA
    add rsp, 40
    
    inc rax
    jmp @gutter_loop
    
@gutter_done:
    ret
Renderer_DrawGutter ENDP

; DRAW SELECTIONS
; ============================================================================

Renderer_DrawSelections PROC USES rbx rcx rdx rsi rdi renderer:PTR DWORD, selStart:QWORD, selEnd:QWORD
    ; renderer = Renderer*
    ; selStart, selEnd = selection range
    ; Uses PatBlt PATINVERT to highlight
    
    mov rcx, renderer
    mov rax, selStart
    mov rdx = selEnd
    
    ; Convert positions to screen coordinates
    ; Rough calculation: x = (pos % 256) * CHAR_WIDTH
    ;                    y = (pos / 256) * CHAR_HEIGHT
    
    mov r8, rax
    mov r9, 256
    xor edx, edx
    div r9
    mov r10, rax  ; r10 = y line
    mov r11, rdx  ; r11 = x in line
    
    ; Draw selection rectangle
    mov r8 = [renderer + 24]  ; offscreenDC
    mov r9 = r11
    mov r10 = CHAR_WIDTH
    imul r9, r10
    
    mov r10 = r10
    mov r11 = CHAR_HEIGHT
    imul r10, r11
    
    sub rsp, 40
    mov rcx, r8
    mov rdx, r9
    mov r8, r10
    mov r9, CHAR_WIDTH
    mov r10, CHAR_HEIGHT
    call PatBlt
    add rsp, 40
    
    ret
Renderer_DrawSelections ENDP

; DRAW DIAGNOSTICS (Error Squiggles)
; ============================================================================

Renderer_DrawDiagnostics PROC USES rbx rcx rdx rsi rdi renderer:PTR DWORD, diagArray:PTR DWORD
    ; renderer = Renderer*
    ; diagArray = Diagnostic array: { severity, position, length }
    ; severity: 1=Error, 2=Warning, 3=Info
    
    mov rcx, renderer
    mov rsi, diagArray
    mov rax, 0  ; diag index
    
@diag_loop:
    cmp qword ptr [rsi + rax*24], 0
    je @diag_done
    
    ; Get severity
    mov edx, dword ptr [rsi + rax*24 + 0]
    
    ; Choose color based on severity
    cmp edx, 1
    je @diag_error
    cmp edx, 2
    je @diag_warning
    
    mov r8d, 0xFF4EC9B0  ; info = cyan
    jmp @diag_draw
    
@diag_error:
    mov r8d, COLOR_ERROR
    jmp @diag_draw
    
@diag_warning:
    mov r8d, 0xFFC89620  ; warning = orange
    
@diag_draw:
    ; Draw wavy line under error
    mov r9, qword ptr [rsi + rax*24 + 8]   ; position
    mov r10, qword ptr [rsi + rax*24 + 16]  ; length
    
    ; Create pen with color
    mov edx, 1  ; line width
    sub rsp, 40
    mov rcx, edx
    mov rdx, 0
    mov r8, r8d
    call CreatePen
    add rsp, 40
    
    ; Draw with pen (stub)
    
    inc rax
    jmp @diag_loop
    
@diag_done:
    ret
Renderer_DrawDiagnostics ENDP

; END DRAW FRAME (Blit to Screen)
; ============================================================================

Renderer_EndDraw PROC USES rbx rcx rdx rsi rdi renderer:PTR DWORD
    ; BitBlt offscreen buffer to window
    
    mov rcx, renderer
    
    ; Get screen DC
    mov rax, [rcx + 0]  ; hwnd
    sub rsp, 40
    mov rcx, rax
    call GetDC
    add rsp, 40
    mov rsi, rax  ; rsi = screenDC
    
    ; BitBlt from offscreen to screen
    mov r8, [renderer + 24]  ; offscreenDC
    mov r9, [renderer + 8]   ; width
    mov r10, [renderer + 16] ; height
    
    sub rsp, 40
    mov rcx, rsi
    mov rdx, 0
    mov r8, 0
    mov r9, r9
    mov r10, r10
    mov [rsp + 32], r8
    mov [rsp + 40], 0
    mov [rsp + 48], 0
    call BitBlt
    add rsp, 40
    
    ; Release screen DC
    mov rcx, [renderer + 0]
    mov rdx, rsi
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    call ReleaseDC
    add rsp, 40
    
    ret
Renderer_EndDraw ENDP

; GET CHARACTER METRICS
; ============================================================================

Renderer_GetCharMetrics PROC USES rbx rcx
    ; Returns: { width, height } in rax:rdx
    ; Fixed monospace 8×16
    
    mov rax, CHAR_WIDTH
    mov rdx, CHAR_HEIGHT
    ret
Renderer_GetCharMetrics ENDP

; GET TOKEN COLOR
; ============================================================================

Renderer_GetTokenColor PROC USES rbx rcx tokenType:DWORD
    ; tokenType = 1-6 (keyword, string, comment, ident, number, operator)
    ; Returns: BGRA color in rax
    
    mov eax, [tokenType]
    
    cmp eax, 1
    je @color_keyword
    cmp eax, 2
    je @color_string
    cmp eax, 3
    je @color_comment
    cmp eax, 4
    je @color_ident
    cmp eax, 5
    je @color_number
    cmp eax, 6
    je @color_operator
    
    mov eax, 0xFFFFFFFF  ; default white
    ret
    
@color_keyword:
    mov eax, COLOR_KEYWORD
    ret
@color_string:
    mov eax, COLOR_STRING
    ret
@color_comment:
    mov eax, COLOR_COMMENT
    ret
@color_ident:
    mov eax, COLOR_IDENTIFIER
    ret
@color_number:
    mov eax, COLOR_NUMBER
    ret
@color_operator:
    mov eax, COLOR_OPERATOR
    ret
Renderer_GetTokenColor ENDP

; CONVERT POSITION TO SCREEN COORDINATES
; ============================================================================

Renderer_PosToCoords PROC USES rbx rcx bufferOffset:QWORD
    ; bufferOffset = position in buffer
    ; Returns: { x, y } in rax:rdx (screen coordinates)
    
    mov rax, bufferOffset
    
    ; Rough calculation: assume ~256 chars per line
    mov rcx, 256
    xor edx, edx
    div rcx
    
    ; rdx = x offset, rax = y line
    mov r8, rdx
    mov r9, CHAR_WIDTH
    imul r8, r9  ; screen X = char x * CHAR_WIDTH
    add r8, GUTTER_WIDTH
    
    mov r9, CHAR_HEIGHT
    imul rax, r9  ; screen Y = line * CHAR_HEIGHT
    
    mov rdx, rax
    mov rax, r8
    
    ret
Renderer_PosToCoords ENDP

; FORMAT STRING FOR GUTTER
; ============================================================================

line_num_format BYTE "%4d", 0

end
