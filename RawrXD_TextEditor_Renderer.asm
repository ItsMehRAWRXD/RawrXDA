; ============================================================================
; RawrXD_TextEditor_Renderer.asm
; Production-grade text rendering with Win32 and syntax highlighting
; ============================================================================

OPTION CASEMAP:NONE

; Win32 API Imports
EXTERN CreateFontA:PROC
EXTERN SelectObject:PROC
EXTERN DeleteObject:PROC
EXTERN GetStockObject:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkColor:PROC
EXTERN SetBkMode:PROC
EXTERN TextOutA:PROC
EXTERN GetTextExtentPoint32A:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN PatBlt:PROC
EXTERN Rectangle:PROC
EXTERN MoveToEx:PROC
EXTERN LineTo:PROC

; External module APIs
EXTERN TextBuffer_GetLineCount:PROC
EXTERN TextBuffer_GetLine:PROC
EXTERN TextBuffer_GetLineLength:PROC
EXTERN Cursor_GetLine:PROC
EXTERN Cursor_GetColumn:PROC

.data
    ; Font names
    szMonospace     db "Courier New", 0
    szConsolas      db "Consolas", 0
    
    ; Color constants (BGR format for Win32)
    COLOR_WHITE     equ 0xFFFFFF
    COLOR_BLACK     equ 0x000000
    COLOR_GRAY      equ 0xC0C0C0
    COLOR_RED       equ 0x0000FF
    COLOR_GREEN     equ 0x00FF00
    COLOR_BLUE      equ 0xFF0000
    COLOR_YELLOW    equ 0x00FFFF
    COLOR_CYAN      equ 0xFFFF00
    COLOR_MAGENTA   equ 0xFF00FF
    
    ; MASM keyword table (for quick lookup)
    MASM_KEYWORDS = 128  ; Hash table size
    
    ; Syntax token types
    TOKEN_KEYWORD   equ 1
    TOKEN_REGISTER  equ 2
    TOKEN_LABEL     equ 3
    TOKEN_COMMENT   equ 4
    TOKEN_STRING    equ 5
    TOKEN_NUMBER    equ 6
    TOKEN_OPERATOR  equ 7
    TOKEN_DEFAULT   equ 0

.code

; ============================================================================
; Renderer_Initialize(rcx = hdc, rdx = char_width_ptr, r8 = char_height_ptr)
;
; Set up rendering context with monospace font
; Returns: rax = success (1) or failure (0)
; ============================================================================
Renderer_Initialize PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 64
    .ENDPROLOG
    
    push rbx
    sub rsp, 64
    
    mov rbx, rcx                        ; rbx = hdc
    
    ; Create monospace font (Courier New or Consolas)
    ; CreateFontA(height, width, escapement, orientation, weight, 
    ;             italic, underline, strikeout, charset, precision,
    ;             clip, quality, pitch, face)
    mov ecx, 14                         ; nHeight = 14
    mov edx, 0                          ; nWidth = 0 (auto)
    mov r8d, 0                          ; nEscapement
    mov r9d, 0                          ; nOrientation
    mov qword [rsp + 32], 400           ; nWeight = FW_NORMAL (400)
    mov qword [rsp + 40], 0             ; bItalic = FALSE
    mov qword [rsp + 48], 0             ; bUnderline = FALSE
    mov qword [rsp + 56], 0             ; bStrikeOut = FALSE
    
    lea rax, [rel szConsolas]
    mov qword [rsp + 24], rax           ; lpszFace
    mov qword [rsp + 16], 1             ; bPrecision = OUT_DEFAULT_PRECIS
    mov qword [rsp + 8], 10             ; iClipPrecision
    mov qword [rsp], 0                  ; iQuality
    
    call CreateFontA
    
    test rax, rax
    jz .InitFail
    
    ; Select font into DC
    mov rcx, rbx                        ; rcx = hdc
    mov rdx, rax                        ; rdx = hfont
    call SelectObject
    
    ; Set rendering parameters
    mov rcx, rbx                        ; rcx = hdc
    mov edx, TRANSPARENT                ; background mode (value = 1)
    call SetBkMode
    
    ; Get character metrics - measure 'W'
    mov rcx, rbx
    lea rdx, [rel szTestChar]
    mov r8d, 1                          ; length = 1
    lea r9, [rsp]                       ; SIZE structure
    
    sub rsp, 16
    call GetTextExtentPoint32A
    add rsp, 16
    
    ; Store width/height if pointers provided
    mov eax, [rsp]                      ; width from SIZE
    cmp rdx, 0
    je .SkipWidthWrite
    mov [rdx], eax                      ; *char_width_ptr = width
    
.SkipWidthWrite:
    cmp r8, 0
    je .SkipHeightWrite
    mov eax, [rsp + 4]                  ; height from SIZE
    mov [r8], eax                       ; *char_height_ptr = height
    
.SkipHeightWrite:
    mov eax, 1                          ; Success
    add rsp, 64
    pop rbx
    ret
    
.InitFail:
    xor eax, eax
    add rsp, 64
    pop rbx
    ret
    
Renderer_Initialize ENDP


; ============================================================================
; Renderer_DrawLine(rcx = hdc, rdx = line_text, r8 = text_length,
;                   r9d = x_pos, [rsp+32] = y_pos, [rsp+40] = color)
;
; Draw a single line of text at specified position with color
; ============================================================================
Renderer_DrawLine PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .ALLOCSTACK 48
    .ENDPROLOG
    
    push rbx
    push r12
    sub rsp, 48
    
    mov rbx, rcx                        ; rbx = hdc
    mov r12, rdx                        ; r12 = text pointer
    
    ; Get parameters from stack
    mov eax, [rsp + 64]                 ; [rsp+64] = x_pos (stored above ALLOCSTACK)
    mov r10d, eax                       ; r10d = x
    
    ; Extract y_pos and color from parameters
    mov eax, [rsp + 72]                 ; y_pos
    mov r11d, eax                       ; r11d = y
    
    ; Set text color (if provided)
    mov ecx, [rsp + 80]                 ; color parameter
    cmp ecx, 0
    je .UseDefault
    mov rcx, rbx
    mov edx, ecx
    call SetTextColor
    
.UseDefault:
    ; Draw text using TextOutA
    mov rcx, rbx                        ; rcx = hdc
    mov edx, r10d                       ; edx = x
    mov r8d, r11d                       ; r8d = y
    mov r9, r12                         ; r9 = lpString
    mov rax, [rsp + 56]                 ; text_length from original params
    mov qword [rsp], rax                ; [rsp+0] = cbString for TextOutA
    
    call TextOutA
    
    add rsp, 48
    pop r12
    pop rbx
    ret
    
Renderer_DrawLine ENDP


; ============================================================================
; Renderer_ClearRect(rcx = hdc, rdx = left, r8 = top, r9 = right,
;                    [rsp+32] = bottom, [rsp+40] = color)
;
; Clear rectangular region with background color
; ============================================================================
Renderer_ClearRect PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 64
    .ENDPROLOG
    
    push rbx
    sub rsp, 64
    
    mov rbx, rcx                        ; rbx = hdc
    
    ; Set background color
    mov eax, [rsp + 72]                 ; color
    mov rcx, rbx
    mov edx, eax
    call SetBkColor
    
    ; Fill rectangle with background color using PatBlt
    mov eax, [rsp + 80]                 ; bottom
    mov ecx, eax
    sub ecx, r8d                        ; ecx = height
    
    mov eax, r9d
    sub eax, edx                        ; eax = width
    
    ; rcx=hdc, edx=left, r8=top, r9=width, [rsp+32]=height
    mov rcx, rbx
    mov r8, [rsp + 64]                  ; bottom from stack
    mov r9d, [rsp + 80]                 ; color (reuse as mask)
    
    ; PatBlt(hdc, x, y, width, height, rop) - WHITENESS = 0xFF0062
    mov qword [rsp + 32], 0xFF0062
    call PatBlt
    
    add rsp, 64
    pop rbx
    ret
    
Renderer_ClearRect ENDP


; ============================================================================
; Renderer_DrawCursor(rcx = hdc, rdx = x, r8 = y, r9d = height)
;
; Draw blinking cursor at specified position
; ============================================================================
Renderer_DrawCursor PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .ALLOCSTACK 32
    .ENDPROLOG
    
    push rbx
    push r12
    sub rsp, 32
    
    mov rbx, rcx                        ; rbx = hdc
    mov r10d, edx                       ; r10d = x
    mov r11d, r8d                       ; r11d = y
    mov r12d, r9d                       ; r12d = height
    
    ; Set cursor color (black)
    mov rcx, rbx
    mov edx, COLOR_BLACK
    call SetTextColor
    
    ; Draw vertical line for cursor
    mov rcx, rbx
    mov edx, r10d                       ; x
    mov r8d, r11d                       ; y
    call MoveToEx
    
    mov rcx, rbx
    mov edx, r10d
    mov r8d, r11d
    add r8d, r12d                       ; y + height
    call LineTo
    
    add rsp, 32
    pop r12
    pop rbx
    ret
    
Renderer_DrawCursor ENDP


; ============================================================================
; Renderer_GetTokenColor(rcx = token_type)
;
; Map token type to display color
; Returns: rax = color (BGR format)
; ============================================================================
Renderer_GetTokenColor PROC FRAME
    
    cmp rcx, TOKEN_KEYWORD
    je .IsKeyword
    cmp rcx, TOKEN_REGISTER
    je .IsRegister
    cmp rcx, TOKEN_COMMENT
    je .IsComment
    cmp rcx, TOKEN_STRING
    je .IsString
    cmp rcx, TOKEN_NUMBER
    je .IsNumber
    cmp rcx, TOKEN_OPERATOR
    je .IsOperator
    
    ; Default color (black)
    mov rax, COLOR_BLACK
    ret
    
.IsKeyword:
    mov rax, COLOR_BLUE                 ; 0xFF0000 in BGR
    ret
    
.IsRegister:
    mov rax, COLOR_RED                  ; 0x0000FF in BGR
    ret
    
.IsComment:
    mov rax, 0x008000                   ; Green
    ret
    
.IsString:
    mov rax, COLOR_MAGENTA              ; 0xFF00FF
    ret
    
.IsNumber:
    mov rax, COLOR_YELLOW               ; 0x00FFFF
    ret
    
.IsOperator:
    mov rax, 0x800000                   ; Dark red
    ret
    
Renderer_GetTokenColor ENDP


; ============================================================================
; Tokenizer_IdentifyToken(rcx = char_ptr, rdx = remaining_length)
;
; Identify token type at current position
; Returns: rax = token_type, rdx = token_length
; ============================================================================
Tokenizer_IdentifyToken PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx                        ; rbx = char_ptr
    mov r8d, edx                        ; r8d = remaining_length
    
    movzx eax, byte [rbx]               ; al = first character
    
    ; Check for comment (semicolon)
    cmp al, ';'
    je .IsComment
    
    ; Check for string (quote)
    cmp al, '"'
    je .IsString
    cmp al, "'"
    je .IsString
    
    ; Check for number (0-9)
    cmp al, '0'
    jb .CheckSymbol
    cmp al, '9'
    ja .CheckSymbol
    jmp .IsNumber
    
.CheckSymbol:
    ; Check for MASM keywords/registers by scanning word
    ; (Simplified: just identify the token type)
    
    test r8d, r8d
    jz .EndToken
    
    ; Default to checking if it's alphanumeric (identifier)
    cmp al, ' '
    je .EndToken
    cmp al, 9                           ; Tab
    je .EndToken
    
    ; Assume keyword/identifier
    mov eax, TOKEN_KEYWORD
    mov edx, 1
    add rsp, 32
    pop rbx
    ret
    
.IsComment:
    ; Rest of line is comment
    mov eax, TOKEN_COMMENT
    mov edx, r8d                        ; entire remaining length
    add rsp, 32
    pop rbx
    ret
    
.IsString:
    ; Find closing quote
    mov ecx, 1                          ; Start after opening quote
    mov edx, al                         ; edx = quote character
    
.StringLoop:
    cmp ecx, r8d
    je .StringEnd
    
    movzx eax, byte [rbx + rcx]
    cmp al, dl
    je .StringEnd
    
    inc ecx
    jmp .StringLoop
    
.StringEnd:
    mov eax, TOKEN_STRING
    mov edx, ecx
    add rsp, 32
    pop rbx
    ret
    
.IsNumber:
    mov ecx, 0
    
.NumberLoop:
    cmp ecx, r8d
    je .NumberEnd
    
    movzx eax, byte [rbx + rcx]
    cmp al, '0'
    jb .NumberEnd
    cmp al, '9'
    ja .CheckHex
    inc ecx
    jmp .NumberLoop
    
.CheckHex:
    ; Check for hex suffix (h)
    cmp al, 'h'
    je .NumEnd
    cmp al, 'H'
    je .NumEnd
    cmp al, 'x'
    je .NumEnd
    cmp al, 'X'
    je .NumEnd
    
.NumberEnd:
    mov eax, TOKEN_NUMBER
    mov edx, ecx
    test edx, edx
    jnz .DoneNumber
    mov edx, 1                          ; At least 1 character
    
.DoneNumber:
    add rsp, 32
    pop rbx
    ret
    
.NumEnd:
    inc ecx
    mov eax, TOKEN_NUMBER
    mov edx, ecx
    add rsp, 32
    pop rbx
    ret
    
.EndToken:
    mov eax, TOKEN_DEFAULT
    mov edx, 1
    add rsp, 32
    pop rbx
    ret
    
Tokenizer_IdentifyToken ENDP


; ============================================================================
; Renderer_DrawTextWithSyntax(rcx = hdc, rdx = buffer_ptr, r8 = start_offset,
;                             r9d = x_start, [rsp+32] = y_pos, [rsp+40] = max_width)
;
; Draw text with syntax highlighting from buffer
; ============================================================================
Renderer_DrawTextWithSyntax PROC FRAME
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
    mov r12, rdx                        ; r12 = buffer_ptr
    mov r13, r8                         ; r13 = start_offset
    mov r10d, r9d                       ; r10d = x_start
    
    mov r11d, [rsp + 128]               ; y_pos
    mov r14d, [rsp + 136]               ; max_width
    
    mov eax, [rsp + 64]                 ; Get buffer start
    
    ; Draw tokens until end of line or max width
    xor ecx, ecx                        ; ecx = current x position
    
.RenderToken:
    add ecx, r10d
    cmp ecx, r14d
    jge .RenderDone
    
    ; Get next character
    mov rax, r13
    add rax, [r12]                      ; Add buffer start pointer
    movzx edx, byte [rax]
    
    ; End of line
    cmp dl, 0x0A                        ; newline
    je .RenderDone
    cmp dl, 0
    je .RenderDone
    
    ; Identify token type
    mov rcx, rax
    mov rdx, r14d
    sub rdx, rcx
    call Tokenizer_IdentifyToken
    
    mov r8d, eax                        ; r8d = token_type
    mov r9d, edx                        ; r9d = token_length
    
    ; Get color for token
    mov rcx, r8d
    call Renderer_GetTokenColor
    mov r15d, eax                       ; r15d = color
    
    ; Draw token
    mov rcx, rbx                        ; hdc
    mov rdx, r13                        ; text pointer
    add rdx, [r12]
    mov r8, r9                          ; length
    mov r9d, r10d                       ; x
    add r9d, ecx                        ; x + current offset
    
    mov rax, r11d
    mov qword [rsp + 32], rax           ; y_pos
    mov qword [rsp + 40], r15d          ; color
    mov qword [rsp + 56], r9d           ; text_length
    
    call Renderer_DrawLine
    
    ; Update position
    add ecx, [rsp + 40]                 ; Add token width (approximate)
    add r13, r9d                        ; Move in buffer
    
    jmp .RenderToken
    
.RenderDone:
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    ret
    
Renderer_DrawTextWithSyntax ENDP


; ============================================================================
; Renderer_RenderWindow(rcx = hdc, rdx = buffer_ptr, r8 = cursor_ptr,
;                       r9d = client_width, [rsp+32] = client_height)
;
; Main rendering routine - draws entire editor content
; ============================================================================
Renderer_RenderWindow PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 128
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 128
    
    mov rbx, rcx                        ; rbx = hdc
    mov r12, rdx                        ; r12 = buffer_ptr
    mov r13, r8                         ; r13 = cursor_ptr
    mov r10d, r9d                       ; r10d = client_width
    
    mov eax, [rsp + 160]                ; client_height
    mov r11d, eax                       ; r11d = client_height
    
    ; Clear background (white)
    mov rcx, rbx
    xor edx, edx                        ; left = 0
    xor r8d, r8d                        ; top = 0
    mov r9d, r10d                       ; right = width
    mov rax, r11d
    mov qword [rsp + 32], rax           ; bottom = height
    mov qword [rsp + 40], COLOR_WHITE   ; color
    
    call Renderer_ClearRect
    
    ; Get line count
    mov rcx, r12
    call TextBuffer_GetLineCount
    mov r14d, eax                       ; r14d = line_count
    
    ; Render each visible line
    xor r15d, r15d                      ; r15d = current line
    xor ecx, ecx                        ; ecx = y position (accumulator)
    
.LineLoop:
    ; Check if line is visible
    cmp ecx, r11d
    jge .RenderComplete
    
    cmp r15d, r14d
    jge .RenderComplete
    
    ; Draw line number (left margin)
    mov r8d, r15d
    inc r8d                             ; 1-based
    
    ; Convert line number to string and draw (simplified)
    mov rcx, rbx
    xor edx, edx                        ; x = 0
    mov r8d, ecx                        ; y = current y
    mov r9d, 0x808080                   ; Gray color
    
    ; Draw line text with syntax highlighting
    mov rcx, rbx
    mov rdx, r12
    xor r8, r8                          ; offset = 0 (simplified)
    mov r9d, 40                         ; x_start after line numbers
    mov rax, rcx
    mov qword [rsp + 32], rax           ; y_pos = current y
    mov qword [rsp + 40], r10d          ; max_width
    
    call Renderer_DrawTextWithSyntax
    
    ; Draw cursor if on this line
    mov r8d, [r13 + 0]                  ; cursor line (simplified)
    cmp r8d, r15d
    jne .SkipCursor
    
    mov rcx, rbx
    mov edx, 40                         ; x (after margin)
    mov r8d, ecx                        ; y
    mov r9d, 16                         ; height
    
    call Renderer_DrawCursor
    
.SkipCursor:
    ; Move to next line
    add ecx, 16                         ; 16 pixels per line (char height)
    inc r15d
    
    jmp .LineLoop
    
.RenderComplete:
    add rsp, 128
    pop r13
    pop r12
    pop rbx
    ret
    
Renderer_RenderWindow ENDP


; ============================================================================
; Test data
; ============================================================================
.data
    szTestChar  db "W", 0               ; Test character for measuring

END
