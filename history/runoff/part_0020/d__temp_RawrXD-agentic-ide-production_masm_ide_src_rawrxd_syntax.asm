;======================================================================
; RawrXD IDE - Syntax Highlighter
; Token detection, color mapping, real-time highlighting for ASM/C
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Token types
TOKEN_KEYWORD           EQU 0
TOKEN_IDENTIFIER        EQU 1
TOKEN_NUMBER            EQU 2
TOKEN_STRING            EQU 3
TOKEN_COMMENT           EQU 4
TOKEN_OPERATOR          EQU 5
TOKEN_REGISTER          EQU 6
TOKEN_PREPROCESSOR      EQU 7

; Color table (BGR format for Windows)
g_colorTable[8*3]:
    DD 0FF0000h             ; Keyword (blue)
    DD 000000h              ; Identifier (black)
    DD 0FF6000h             ; Number (orange)
    DD 0008000h             ; String (green)
    DD 0808080h             ; Comment (gray)
    DD 0FF0000h             ; Operator (red)
    DD 0FF6000h             ; Register (orange)
    DD 0FF0000h             ; Preprocessor (purple)

; ASM keywords
g_asmKeywords:
    DB "MOV", 0
    DB "LEA", 0
    DB "ADD", 0
    DB "SUB", 0
    DB "MUL", 0
    DB "DIV", 0
    DB "JMP", 0
    DB "JNE", 0
    DB "JGE", 0
    DB "JLE", 0
    DB "CALL", 0
    DB "RET", 0
    DB "PUSH", 0
    DB "POP", 0
    DB "XOR", 0
    DB "AND", 0
    DB "OR", 0
    DB "NOT", 0
    DB "TEST", 0
    DB "CMP", 0
    DB "LOOP", 0
    DB "INVOKE", 0
    DB "PROC", 0
    DB "ENDP", 0
    DB "END", 0
    DB "DB", 0
    DB "DQ", 0
    DB "INCLUDE", 0
    DB "EXTERN", 0

; ASM registers
g_asmRegisters:
    DB "RAX", 0
    DB "RBX", 0
    DB "RCX", 0
    DB "RDX", 0
    DB "RSI", 0
    DB "RDI", 0
    DB "RBP", 0
    DB "RSP", 0
    DB "EAX", 0
    DB "EBX", 0
    DB "ECX", 0
    DB "EDX", 0
    DB "ESI", 0
    DB "EDI", 0
    DB "EBP", 0
    DB "ESP", 0

g_highlightEnabled      DQ 1
g_lastHighlightTime     DQ 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Syntax_Init - Initialize syntax highlighter
;----------------------------------------------------------------------
RawrXD_Syntax_Init PROC
    ; Load keyword and register tables
    ; Tables already loaded in data section
    
    xor eax, eax
    ret
RawrXD_Syntax_Init ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_GetTokenType - Identify token type from string
;----------------------------------------------------------------------
RawrXD_Syntax_GetTokenType PROC pszToken:QWORD
    LOCAL tokenLen:QWORD
    LOCAL charCode:QWORD
    
    ; Get token length
    INVOKE lstrlenA, pszToken
    mov tokenLen, rax
    
    ; Check first character
    mov al, [pszToken]
    mov charCode, rax
    
    ; Check if comment (line starting with ;)
    cmp al, ';'
    jne @@not_comment
    mov eax, TOKEN_COMMENT
    ret
    
@@not_comment:
    ; Check if preprocessor (#)
    cmp al, '#'
    jne @@not_prep
    mov eax, TOKEN_PREPROCESSOR
    ret
    
@@not_prep:
    ; Check if number (digit or negative sign)
    cmp al, '0'
    jb @@not_digit
    cmp al, '9'
    ja @@not_digit
    mov eax, TOKEN_NUMBER
    ret
    
@@not_digit:
    ; Check if string (quote)
    cmp al, '"'
    je @@is_string
    cmp al, "'"
    je @@is_string
    jne @@not_string
    
@@is_string:
    mov eax, TOKEN_STRING
    ret
    
@@not_string:
    ; Check if keyword (uppercase alphanumeric)
    INVOKE RawrXD_Syntax_IsKeyword, pszToken
    test eax, eax
    jz @@not_keyword
    mov eax, TOKEN_KEYWORD
    ret
    
@@not_keyword:
    ; Check if register
    INVOKE RawrXD_Syntax_IsRegister, pszToken
    test eax, eax
    jz @@not_register
    mov eax, TOKEN_REGISTER
    ret
    
@@not_register:
    ; Check if operator
    INVOKE RawrXD_Syntax_IsOperator, al
    test eax, eax
    jz @@default
    mov eax, TOKEN_OPERATOR
    ret
    
@@default:
    mov eax, TOKEN_IDENTIFIER
    ret
    
RawrXD_Syntax_GetTokenType ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_IsKeyword - Check if token is ASM keyword
;----------------------------------------------------------------------
RawrXD_Syntax_IsKeyword PROC pszToken:QWORD
    LOCAL pKeyword:QWORD
    LOCAL offset:QWORD
    
    ; Linear search through keyword table
    mov pKeyword, OFFSET g_asmKeywords
    
@@loop:
    ; Check for end of table (double null)
    mov al, [pKeyword]
    test al, al
    jz @@not_found
    
    ; Compare keywords
    INVOKE lstrcmpA, pszToken, pKeyword
    test eax, eax
    jz @@found
    
    ; Skip to next keyword
    INVOKE lstrlenA, pKeyword
    mov rcx, rax
    add pKeyword, rcx
    inc pKeyword  ; Skip null terminator
    
    jmp @@loop
    
@@found:
    mov eax, 1
    ret
    
@@not_found:
    xor eax, eax
    ret
    
RawrXD_Syntax_IsKeyword ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_IsRegister - Check if token is ASM register
;----------------------------------------------------------------------
RawrXD_Syntax_IsRegister PROC pszToken:QWORD
    LOCAL pRegister:QWORD
    
    ; Linear search through register table
    mov pRegister, OFFSET g_asmRegisters
    
@@loop:
    ; Check for end of table
    mov al, [pRegister]
    test al, al
    jz @@not_found
    
    ; Compare registers
    INVOKE lstrcmpA, pszToken, pRegister
    test eax, eax
    jz @@found
    
    ; Skip to next register
    INVOKE lstrlenA, pRegister
    mov rcx, rax
    add pRegister, rcx
    inc pRegister
    
    jmp @@loop
    
@@found:
    mov eax, 1
    ret
    
@@not_found:
    xor eax, eax
    ret
    
RawrXD_Syntax_IsRegister ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_IsOperator - Check if character is operator
;----------------------------------------------------------------------
RawrXD_Syntax_IsOperator PROC charCode:QWORD
    mov al, charCode
    
    ; Check common operators
    cmp al, '+'
    je @@yes
    cmp al, '-'
    je @@yes
    cmp al, '*'
    je @@yes
    cmp al, '/'
    je @@yes
    cmp al, '%'
    je @@yes
    cmp al, '='
    je @@yes
    cmp al, '<'
    je @@yes
    cmp al, '>'
    je @@yes
    cmp al, '!'
    je @@yes
    cmp al, '&'
    je @@yes
    cmp al, '|'
    je @@yes
    cmp al, '^'
    je @@yes
    cmp al, '~'
    je @@yes
    cmp al, '('
    je @@yes
    cmp al, ')'
    je @@yes
    cmp al, '['
    je @@yes
    cmp al, ']'
    je @@yes
    cmp al, '{'
    je @@yes
    cmp al, '}'
    je @@yes
    cmp al, ','
    je @@yes
    cmp al, '.'
    je @@yes
    cmp al, ':'
    je @@yes
    cmp al, ';'
    je @@yes
    
    xor eax, eax
    ret
    
@@yes:
    mov eax, 1
    ret
    
RawrXD_Syntax_IsOperator ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_GetColor - Get color for token type
;----------------------------------------------------------------------
RawrXD_Syntax_GetColor PROC tokenType:QWORD
    ; Bounds check
    cmp tokenType, 8
    jae @@default
    
    ; Index into color table
    mov rax, tokenType
    imul rax, 3
    mov rcx, OFFSET g_colorTable
    add rcx, rax
    mov eax, [rcx]
    
    ret
    
@@default:
    mov eax, 0
    ret
    
RawrXD_Syntax_GetColor ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_HighlightLine - Highlight single line
;----------------------------------------------------------------------
RawrXD_Syntax_HighlightLine PROC pszLine:QWORD, hdc:QWORD, y:QWORD
    LOCAL pos:QWORD
    LOCAL tokenStart:QWORD
    LOCAL tokenType:QWORD
    LOCAL color:QWORD
    LOCAL token[128]:BYTE
    LOCAL charCode:QWORD
    
    xor pos, pos
    mov tokenStart, 0
    
@@scan_loop:
    ; Get character
    mov al, [pszLine + pos]
    test al, al
    jz @@process_token  ; End of line
    
    mov charCode, rax
    
    ; Check if whitespace or operator (token boundary)
    INVOKE RawrXD_Syntax_IsOperator, charCode
    test eax, eax
    jnz @@process_token
    
    cmp al, ' '
    je @@process_token
    cmp al, 09h  ; Tab
    je @@process_token
    cmp al, 13  ; CR
    je @@process_token
    cmp al, 10  ; LF
    je @@process_token
    
    ; Continue building token
    inc pos
    jmp @@scan_loop
    
@@process_token:
    ; Extract token
    mov rax, pos
    sub rax, tokenStart
    cmp rax, 0
    je @@next_pos
    
    ; Copy token substring
    mov rcx, rax
    mov rsi, pszLine
    add rsi, tokenStart
    mov rdi, OFFSET token
    rep movsb
    mov byte [rdi], 0
    
    ; Get token type and color
    INVOKE RawrXD_Syntax_GetTokenType, OFFSET token
    mov tokenType, rax
    
    INVOKE RawrXD_Syntax_GetColor, tokenType
    mov color, rax
    
    ; Draw token with color
    INVOKE SetTextColor, hdc, color
    INVOKE TextOutA, hdc, tokenStart * 8, y, OFFSET token, lstrlenA(OFFSET token)
    
@@next_pos:
    mov tokenStart, pos
    cmp al, 0
    je @@done
    inc pos
    jmp @@scan_loop
    
@@done:
    ret
    
RawrXD_Syntax_HighlightLine ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_Enable - Enable/disable syntax highlighting
;----------------------------------------------------------------------
RawrXD_Syntax_Enable PROC enabled:QWORD
    mov g_highlightEnabled, enabled
    ret
RawrXD_Syntax_Enable ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_IsEnabled - Check if highlighting is enabled
;----------------------------------------------------------------------
RawrXD_Syntax_IsEnabled PROC
    mov eax, g_highlightEnabled
    ret
RawrXD_Syntax_IsEnabled ENDP

;----------------------------------------------------------------------
; RawrXD_Syntax_UpdateColor - Dynamically update color for token type
;----------------------------------------------------------------------
RawrXD_Syntax_UpdateColor PROC tokenType:QWORD, newColor:QWORD
    ; Bounds check
    cmp tokenType, 8
    jae @@fail
    
    ; Update color table
    mov rax, tokenType
    imul rax, 3
    mov rcx, OFFSET g_colorTable
    add rcx, rax
    mov eax, newColor
    mov [rcx], eax
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Syntax_UpdateColor ENDP

END
