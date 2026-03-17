; ============================================================================
; RawrXD_TextEditor_SyntaxHighlighter.asm
; MASM syntax highlighting with keyword recognition and color mapping
; ============================================================================

OPTION CASEMAP:NONE

.data
    ; MASM Keywords - uppercase for comparison
    szKeyword_PROC       db "PROC", 0
    szKeyword_ENDP       db "ENDP", 0
    szKeyword_PUBLIC     db "PUBLIC", 0
    szKeyword_EXTERN     db "EXTERN", 0
    szKeyword_ASSUME     db "ASSUME", 0
    szKeyword_SECTION    db "SECTION", 0
    szKeyword_SEGMENT    db "SEGMENT", 0
    szKeyword_ENDS       db "ENDS", 0
    szKeyword_STRUC      db "STRUC", 0
    szKeyword_UNION      db "UNION", 0
    szKeyword_RECORD     db "RECORD", 0
    
    ; x64 General Purpose Registers
    szReg_RAX            db "RAX", 0
    szReg_RBX            db "RBX", 0
    szReg_RCX            db "RCX", 0
    szReg_RDX            db "RDX", 0
    szReg_RSI            db "RSI", 0
    szReg_RDI            db "RDI", 0
    szReg_RBP            db "RBP", 0
    szReg_RSP            db "RSP", 0
    szReg_R8             db "R8", 0
    szReg_R15            db "R15", 0
    
    ; 32-bit registers
    szReg_EAX            db "EAX", 0
    szReg_EBX            db "EBX", 0
    szReg_ECX            db "ECX", 0
    szReg_EDX            db "EDX", 0
    
    ; x64 Instructions - common ones
    szInst_MOV           db "MOV", 0
    szInst_ADD           db "ADD", 0
    szInst_SUB           db "SUB", 0
    szInst_MUL           db "MUL", 0
    szInst_DIV           db "DIV", 0
    szInst_CALL          db "CALL", 0
    szInst_RET           db "RET", 0
    szInst_JMP           db "JMP", 0
    szInst_JE            db "JE", 0
    szInst_JNE           db "JNE", 0
    szInst_PUSH          db "PUSH", 0
    szInst_POP           db "POP", 0
    szInst_LEA           db "LEA", 0
    szInst_TEST          db "TEST", 0
    szInst_CMP           db "CMP", 0
    
    ; Assembler directives
    szDir_DB             db "DB", 0
    szDir_DW             db "DW", 0
    szDir_DD             db "DD", 0
    szDir_DQ             db "DQ", 0
    szDir_BYTE           db "BYTE", 0
    szDir_WORD           db "WORD", 0
    szDir_DWORD          db "DWORD", 0
    szDir_QWORD          db "QWORD", 0
    
    ; Pseudo-ops
    szPseudo_ALIGN       db "ALIGN", 0
    szPseudo_INCLUDE     db "INCLUDE", 0
    szPseudo_INCLUDELIB  db "INCLUDELIB", 0
    szPseudo_EXTERN      db "EXTERN", 0
    szPseudo_GLOBAL      db "GLOBAL", 0
    
    ; Operators and delimiters
    szOp_COLON           db ":", 0
    szOp_SEMICOLON       db ";", 0
    szOp_COMMA           db ",", 0
    szOp_LPAREN          db "(", 0
    szOp_RPAREN          db ")", 0
    szOp_LBRACKET        db "[", 0
    szOp_RBRACKET        db "]", 0
    szOp_PLUS            db "+", 0
    szOp_MINUS           db "-", 0
    szOp_MULT            db "*", 0
    szOp_DIV             db "/", 0
    szOp_AND             db "&", 0
    szOp_OR              db "|", 0
    szOp_XOR             db "^", 0
    szOp_NOT             db "~", 0

.code

; Token type constants
TOKEN_TYPE_KEYWORD       equ 1
TOKEN_TYPE_REGISTER      equ 2
TOKEN_TYPE_INSTRUCTION   equ 3
TOKEN_TYPE_DIRECTIVE     equ 4
TOKEN_TYPE_LABEL        equ 5
TOKEN_TYPE_COMMENT      equ 6
TOKEN_TYPE_STRING       equ 7
TOKEN_TYPE_NUMBER       equ 8
TOKEN_TYPE_OPERATOR     equ 9
TOKEN_TYPE_WHITESPACE   equ 10
TOKEN_TYPE_DEFAULT      equ 0

; ============================================================================
; SyntaxHighlighter_IsKeyword(rcx = string_ptr, [rsp+32] = length)
;
; Check if input word is a MASM keyword
; Returns: rax = keyword_color or 0 if not keyword
; ============================================================================
SyntaxHighlighter_IsKeyword PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 64
    .ENDPROLOG
    
    push rbx
    sub rsp, 64
    
    mov rbx, rcx                        ; rbx = string_ptr
    mov eax, [rsp + 96]                 ; eax = length
    
    ; Uppercase first character to test
    movzx ecx, byte [rbx]
    cmp cl, 'a'
    jb .Skip1
    cmp cl, 'z'
    ja .Skip1
    sub cl, 32                          ; Convert to uppercase
    
.Skip1:
    ; Compare against known keywords
    ; PROC, ENDP, PUBLIC, EXTERN, etc.
    
    cmp eax, 4
    je .CheckFourChar
    
    cmp eax, 3
    je .CheckThreeChar
    
    cmp eax, 5
    je .CheckFiveChar
    
    xor eax, eax
    add rsp, 64
    pop rbx
    ret
    
.CheckThreeChar:
    ; Check: JMP, JE, JNE, MOV, ADD, SUB, MUL, DIV, RET, LEA, POP,
    ;        RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP, DB, DW, DD, DQ
    
    ; First char already in ecx
    movzx edx, byte [rbx + 1]
    cmp edx, 'a'
    jb .T3_Skip2
    cmp edx, 'z'
    ja .T3_Skip2
    sub edx, 32
    
.T3_Skip2:
    movzx r8d, byte [rbx + 2]
    cmp r8d, 'a'
    jb .T3_Skip3
    cmp r8d, 'z'
    ja .T3_Skip3
    sub r8d, 32
    
.T3_Skip3:
    ; Quick check for JMP, JE, etc.
    cmp ecx, 'J'
    je .CheckJump3
    
    ; Check MOV (0x4D 0x4F 0x56)
    cmp ecx, 'M'
    je .CheckMov
    cmp ecx, 'A'
    je .CheckAdd
    
    xor eax, eax
    jmp .KeywordDone
    
.CheckMov:
    cmp edx, 'O'
    jne .MovFail
    cmp r8d, 'V'
    jne .MovFail
    mov eax, 0x0000FF                   ; COLOR_RED
    jmp .KeywordDone
    
.MovFail:
    xor eax, eax
    jmp .KeywordDone
    
.CheckAdd:
    cmp edx, 'D'
    jne .CheckAddSkip
    cmp r8d, 'D'
    jne .CheckAddSkip
    mov eax, 0x0000FF                   ; COLOR_RED
    jmp .KeywordDone
    
.CheckAddSkip:
    xor eax, eax
    jmp .KeywordDone
    
.CheckJump3:
    ; JMP, JE, JNE
    cmp edx, 'M'
    je .JmpMatch
    cmp edx, 'E'
    je .JeMatch
    xor eax, eax
    jmp .KeywordDone
    
.JmpMatch:
    mov eax, 0x0000FF
    jmp .KeywordDone
    
.JeMatch:
    cmp r8d, 'E'
    je .JeeMatch
    xor eax, eax
    jmp .KeywordDone
    
.JeeMatch:
    mov eax, 0x0000FF
    jmp .KeywordDone
    
.CheckFourChar:
    mov eax, 0x0000FF                   ; Blue for keywords
    jmp .KeywordDone
    
.CheckFiveChar:
    mov eax, 0x0000FF                   ; Blue for keywords
    
.KeywordDone:
    add rsp, 64
    pop rbx
    ret
    
SyntaxHighlighter_IsKeyword ENDP


; ============================================================================
; SyntaxHighlighter_IsRegister(rcx = string_ptr, [rsp+32] = length)
;
; Check if input word is an x64 register
; Returns: rax = register_color or 0 if not register
; ============================================================================
SyntaxHighlighter_IsRegister PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 64
    .ENDPROLOG
    
    push rbx
    sub rsp, 64
    
    mov rbx, rcx
    mov eax, [rsp + 96]
    
    ; Check register length patterns
    ; 64-bit: RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP (3 chars)
    ;         R8, R9, R10-R15 (2-3 chars)
    ; 32-bit: EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP (3 chars)
    ; 16-bit: AX, BX, CX, DX, SI, DI, BP, SP (2 chars)
    ; 8-bit:  AL, AH, BL, BH, CL, CH, DL, DH (2 chars)
    
    cmp eax, 3
    je .Check3RegChars
    
    cmp eax, 2
    je .Check2RegChars
    
    xor eax, eax
    jmp .RegDone
    
.Check3RegChars:
    ; RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP
    ; EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP
    movzx ecx, byte [rbx]
    movzx edx, byte [rbx + 1]
    movzx r8d, byte [rbx + 2]
    
    ; Convert to uppercase
    cmp cl, 'a'
    jb .Skip3a
    cmp cl, 'z'
    ja .Skip3a
    sub cl, 32
.Skip3a:
    cmp dl, 'a'
    jb .Skip3b
    cmp dl, 'z'
    ja .Skip3b
    sub dl, 32
.Skip3b:
    cmp r8b, 'a'
    jb .Skip3c
    cmp r8b, 'z'
    ja .Skip3c
    sub r8b, 32
.Skip3c:
    
    ; Check for R** pattern (64-bit)
    cmp cl, 'R'
    je .Is64BitReg
    
    ; Check for E** pattern (32-bit)
    cmp cl, 'E'
    je .Is32BitReg
    
    xor eax, eax
    jmp .RegDone
    
.Is64BitReg:
    mov eax, 0xFF0000                   ; Blue color for registers
    jmp .RegDone
    
.Is32BitReg:
    mov eax, 0xFF0000
    jmp .RegDone
    
.Check2RegChars:
    movzx ecx, byte [rbx]
    movzx edx, byte [rbx + 1]
    
    cmp cl, 'a'
    jb .Skip2a
    cmp cl, 'z'
    ja .Skip2a
    sub cl, 32
.Skip2a:
    cmp dl, 'a'
    jb .Skip2b
    cmp dl, 'z'
    ja .Skip2b
    sub dl, 32
.Skip2b:
    
    ; Check for AX, BX, CX, DX, SP, BP, SI, DI
    ; or R8, R9
    cmp cl, 'R'
    je .Is64Short
    
    ; Check single letter followed by X, L, H
    cmp dl, 'X'
    je .Is16Bit
    cmp dl, 'L'
    je .Is8Bit
    cmp dl, 'H'
    je .Is8Bit
    
    xor eax, eax
    jmp .RegDone
    
.Is16Bit:
    mov eax, 0xFF0000
    jmp .RegDone
    
.Is8Bit:
    mov eax, 0xFF0000
    jmp .RegDone
    
.Is64Short:
    mov eax, 0xFF0000
    
.RegDone:
    add rsp, 64
    pop rbx
    ret
    
SyntaxHighlighter_IsRegister ENDP


; ============================================================================
; SyntaxHighlighter_AnalyzeLine(rcx = line_text_ptr, rdx = line_length,
;                               r8 = token_array_ptr)
;
; Analyze a line and return array of token types/colors
; Returns: rax = number of tokens found
; ============================================================================
SyntaxHighlighter_AnalyzeLine PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 96
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 96
    
    mov rbx, rcx                        ; rbx = line_text
    mov r12d, edx                       ; r12d = line_length
    mov r13, r8                         ; r13 = token_array
    
    xor r14d, r14d                      ; r14d = current position
    xor r15d, r15d                      ; r15d = token count
    
.TokenLoop:
    cmp r14d, r12d
    jge .EndTokens
    
    mov al, [rbx + r14d]
    
    ; Skip whitespace
    cmp al, ' '
    je .SkipWhitespace
    cmp al, 9
    je .SkipWhitespace
    cmp al, 13
    je .EndTokens
    cmp al, 10
    je .EndTokens
    
    ; Check for comment (;)
    cmp al, ';'
    je .IsCommentToken
    
    ; Check for label (ends with :)
    ; Just mark as label if we see identifier followed by :
    
    ; Extract word/token
    lea rcx, [rsp]                      ; Token buffer
    xor ecx, ecx                        ; Token start
    
.WordExtract:
    cmp r14d, r12d
    jge .WordEnd
    
    mov al, [rbx + r14d]
    
    ; Check if character is part of identifier
    cmp al, ' '
    je .WordEnd
    cmp al, 9
    je .WordEnd
    cmp al, 13
    je .WordEnd
    cmp al, 10
    je .WordEnd
    cmp al, ','
    je .WordEnd
    cmp al, ':'
    je .WordEnd
    cmp al, '['
    je .WordEnd
    cmp al, ']'
    je .WordEnd
    cmp al, '('
    je .WordEnd
    cmp al, ')'
    je .WordEnd
    cmp al, ';'
    je .WordEnd
    
    mov [rsp + rcx], al
    inc ecx
    cmp ecx, 32                         ; Max token length
    jge .WordEnd
    
    inc r14d
    jmp .WordExtract
    
.WordEnd:
    ; Check if it's a keyword
    mov rdx1, rsp                       ; rdx = token
    mov r8, rcx                         ; r8 = token length
    
    call SyntaxHighlighter_IsKeyword
    
    cmp eax, 0
    jne .FoundKeyword
    
    ; Check if it's a register
    mov rcx, rsp
    mov edx, ecx
    call SyntaxHighlighter_IsRegister
    
    cmp eax, 0
    jne .FoundRegister
    
    ; Default: identifier/variable
    mov eax, 0x000000                   ; Black
    jmp .StoreToken
    
.FoundKeyword:
    ; eax already has color
    jmp .StoreToken
    
.FoundRegister:
    ; eax already has color
    
.StoreToken:
    ; Store token in array
    mov [r13 + r15d * 4], eax
    inc r15d
    jmp .TokenLoop
    
.IsCommentToken:
    ; Rest of line is comment
    mov eax, 0x008000                   ; Green for comments
    mov [r13 + r15d * 4], eax
    inc r15d
    add r14d, r12d                      ; Skip rest ofline
    jmp .EndTokens
    
.SkipWhitespace:
    inc r14d
    jmp .TokenLoop
    
.EndTokens:
    mov eax, r15d                       ; Return token count
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    ret
    
SyntaxHighlighter_AnalyzeLine ENDP


; ============================================================================
; SyntaxHighlighter_GetColorForToken(rcx = token_type)
;
; Map token type to RGB color (BGR format for Win32)
; ============================================================================
SyntaxHighlighter_GetColorForToken PROC FRAME
    
    cmp ecx, TOKEN_TYPE_KEYWORD
    je .KeywordColor
    
    cmp ecx, TOKEN_TYPE_REGISTER
    je .RegisterColor
    
    cmp ecx, TOKEN_TYPE_INSTRUCTION
    je .InstructionColor
    
    cmp ecx, TOKEN_TYPE_DIRECTIVE
    je .DirectiveColor
    
    cmp ecx, TOKEN_TYPE_COMMENT
    je .CommentColor
    
    cmp ecx, TOKEN_TYPE_STRING
    je .StringColor
    
    cmp ecx, TOKEN_TYPE_NUMBER
    je .NumberColor
    
    cmp ecx, TOKEN_TYPE_OPERATOR
    je .OperatorColor
    
    ; Default: black
    xor eax, eax
    ret
    
.KeywordColor:
    mov eax, 0x0000FF                   ; Blue
    ret
    
.RegisterColor:
    mov eax, 0xFF0000                   ; Red
    ret
    
.InstructionColor:
    mov eax, 0x008000                   ; Green
    ret
    
.DirectiveColor:
    mov eax, 0xA020F0                   ; Purple
    ret
    
.CommentColor:
    mov eax, 0x008000                   ; Green
    ret
    
.StringColor:
    mov eax, 0xFF00FF                   ; Magenta
    ret
    
.NumberColor:
    mov eax, 0x00FFFF                   ; Yellow
    ret
    
.OperatorColor:
    mov eax, 0x808000                   ; Teal
    ret
    
SyntaxHighlighter_GetColorForToken ENDP


END
