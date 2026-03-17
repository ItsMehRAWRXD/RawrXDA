; =============================================================================
; RawrXD_MASM_SyntaxHighlighter.asm
; Syntax highlighting engine for x64 MASM in RawrXD IDE
; Tokenizes MASM source → color tokens for display
; =============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; COLOR CONSTANTS (24-bit RGB)
; ============================================================================
COLOR_KEYWORD       EQU 0569CD6h    ; Blue (keywords: MOV, CALL, PROC)
COLOR_REGISTER      EQU 04EC9B0h    ; Cyan (RAX, RBX, RSI, etc.)
COLOR_NUMBER        EQU 0B5CEA8h    ; Green (0x1000, 42, 3Fh)
COLOR_STRING        EQU 0CE9178h    ; Orange (quoted text)
COLOR_COMMENT       EQU 06A9955h    ; Gray (semicolon comments)
COLOR_DIRECTIVE     EQU 0C586C0h    ; Purple (.data, .code, PROC, ENDP)
COLOR_INSTRUCTION   EQU 0569CD6h    ; Blue (ADD, SUB, CMP, etc.)
COLOR_LABEL         EQU 0D7BA7Dh    ; Pink (labels ending in :)
COLOR_DEFAULT       EQU 0D4D4D4h    ; Default text

; Token type IDs
TOKEN_KEYWORD       EQU 1
TOKEN_REGISTER      EQU 2
TOKEN_NUMBER        EQU 3
TOKEN_STRING        EQU 4
TOKEN_COMMENT       EQU 5
TOKEN_DIRECTIVE     EQU 6
TOKEN_INSTRUCTION   EQU 7
TOKEN_LABEL         EQU 8
TOKEN_OPERATOR      EQU 9
TOKEN_UNKNOWN       EQU 0

; ============================================================================
; KEYWORD LOOKUP TABLE
; ============================================================================

.data
    ALIGN 8
    
    ; MASM Keywords (procedural)
    g_Keywords          DB "PROC", 0
                        DB "ENDP", 0
                        DB "FRAME", 0
                        DB "PUSHREG", 0
                        DB "ALLOCSTACK", 0
                        DB "ENDPROLOG", 0
                        DB "SAVEREG", 0
                        DB "SETFRAME", 0
                        DB "STRUCT", 0
                        DB "ENDS", 0
                        DB "UNION", 0
                        DB "RECORD", 0
                        DB "EQU", 0
                        DB "DB", 0
                        DB "DW", 0
                        DB "DD", 0
                        DB "DQ", 0
                        DB 0
    
    g_KeywordCount      EQU ($ - OFFSET g_Keywords) / 8
    
    ; x64 MASM Registers (case-insensitive)
    g_Registers         DB "RAX", 0
                        DB "RBX", 0
                        DB "RCX", 0
                        DB "RDX", 0
                        DB "RSI", 0
                        DB "RDI", 0
                        DB "RBP", 0
                        DB "RSP", 0
                        DB "R8", 0
                        DB "R9", 0
                        DB "R10", 0
                        DB "R11", 0
                        DB "R12", 0
                        DB "R13", 0
                        DB "R14", 0
                        DB "R15", 0
                        DB "EAX", 0
                        DB "EBX", 0
                        DB "ECX", 0
                        DB "EDX", 0
                        DB "ESI", 0
                        DB "EDI", 0
                        DB "EBP", 0
                        DB "ESP", 0
                        DB "AX", 0
                        DB "BX", 0
                        DB "CX", 0
                        DB "DX", 0
                        DB "SI", 0
                        DB "DI", 0
                        DB "BP", 0
                        DB "SP", 0
                        DB "AL", 0
                        DB "BL", 0
                        DB "CL", 0
                        DB "DL", 0
                        DB 0
    
    ; x64 MASM Instructions (mnemonics)
    g_Instructions      DB "MOV", 0
                        DB "CALL", 0
                        DB "RET", 0
                        DB "PUSH", 0
                        DB "POP", 0
                        DB "ADD", 0
                        DB "SUB", 0
                        DB "MUL", 0
                        DB "DIV", 0
                        DB "CMP", 0
                        DB "TEST", 0
                        DB "JMP", 0
                        DB "JZ", 0
                        DB "JNZ", 0
                        DB "JE", 0
                        DB "JNE", 0
                        DB "JLE", 0
                        DB "JGE", 0
                        DB "JL", 0
                        DB "JG", 0
                        DB "LEA", 0
                        DB "XOR", 0
                        DB "AND", 0
                        DB "OR", 0
                        DB "NOT", 0
                        DB "NEG", 0
                        DB "SHL", 0
                        DB "SHR", 0
                        DB "IMUL", 0
                        DB "IDIV", 0
                        DB "NOP", 0
                        DB "HLT", 0
                        DB "LOOP", 0
                        DB "LOOPZ", 0
                        DB "LOOPNZ", 0
                        DB "CALL", 0
                        DB "RET", 0
                        DB 0
    
    ; Assembler Directives
    g_Directives        DB ".data", 0
                        DB ".code", 0
                        DB ".text", 0
                        DB "ALIGN", 0
                        DB "OPTION", 0
                        DB "EXTERN", 0
                        DB "PUBLIC", 0
                        DB "INCLUDE", 0
                        DB "MACRO", 0
                        DB "ENDM", 0
                        DB 0

; ============================================================================
; TOKEN STRUCTURE
; ============================================================================
; typedef struct {
;   DWORD start_offset;
;   DWORD length;
;   BYTE  token_type;
;   DWORD color;
; } TOKEN;

TOKEN_START     EQU 0
TOKEN_LENGTH    EQU 4
TOKEN_TYPE      EQU 8
TOKEN_COLOR     EQU 12
TOKEN_SIZE      EQU 16

; ============================================================================
; HELPER: String comparison (case-insensitive for registers)
; ============================================================================
STRCMP_CASE_INSENSITIVE PROC FRAME
    ; RCX = string 1 (from source)
    ; RDX = string 2 (keyword/register list)
    ; Returns: ZF set if equal
    
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .endprolog
    
.cmp_loop:
    mov al, [rcx]
    mov bl, [rdx]
    
    ; Convert to uppercase
    cmp al, 'a'
    jl .al_ok
    cmp al, 'z'
    jg .al_ok
    sub al, 20h
    
.al_ok:
    cmp bl, 'a'
    jl .bl_ok
    cmp bl, 'z'
    jg .bl_ok
    sub bl, 20h
    
.bl_ok:
    cmp al, bl
    jne .cmp_fail
    
    test al, al
    jz .cmp_ok
    
    inc rcx
    inc rdx
    jmp .cmp_loop
    
.cmp_ok:
    xor al, al          ; Set ZF
    jmp .done
    
.cmp_fail:
    mov al, 1           ; Clear ZF
    
.done:
    pop rbp
    ret
STRCMP_CASE_INSENSITIVE ENDP

; ============================================================================
; TOKENIZE_MASM_LINE
; Scans a line of MASM source and returns array of tokens
; RCX = source line (null-terminated)
; RDX = output token array (TOKEN structures)
; R8 = max tokens
; Returns: RAX = number of tokens produced
; ============================================================================
TOKENIZE_MASM_LINE PROC FRAME
    .pushreg rbp
    .pushreg rdi
    .pushreg rsi
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    push rbp
    push rdi
    push rsi
    push rbx
    push r12
    push r13
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rsi, rcx            ; rsi = source
    mov rdi, rdx            ; rdi = output tokens
    mov r12, r8             ; r12 = max tokens
    xor r13, r13            ; r13 = token count
    xor rbx, rbx            ; rbx = current offset
    
.scan_loop:
    mov al, [rsi + rbx]
    test al, al
    jz .done
    
    ; Skip whitespace
    cmp al, ' '
    je .skip_ws
    cmp al, 9               ; Tab
    je .skip_ws
    
    ; Check for comment
    cmp al, ';'
    je .handle_comment
    
    ; Check for label (word followed by ':')
    mov r10, rbx
    mov r9, 0
    
.find_label_end:
    mov al, [rsi + r10]
    cmp al, ':'
    je .handle_label
    cmp al, ' '
    je .not_label
    cmp al, 9
    je .not_label
    cmp al, 0
    je .not_label
    
    inc r10
    inc r9
    cmp r9, 32              ; Max identifier length
    jl .find_label_end
    
.not_label:
    ; Check for quoted string
    cmp al, '"'
    je .handle_string
    cmp al, "'"
    je .handle_string
    
    ; Check for number (0-9 or 0x)
    cmp al, '0'
    jl .check_operator
    cmp al, '9'
    jg .check_operator
    
    mov r9, rbx
    jmp .scan_number
    
.check_operator:
    ; Single-char operators: comma, bracket, etc.
    mov r9, rbx
    mov r10, 1
    mov al, [rsi + rbx]
    cmp al, ','
    je .add_token_op
    cmp al, '['
    je .add_token_op
    cmp al, ']'
    je .add_token_op
    cmp al, '('
    je .add_token_op
    cmp al, ')'
    je .add_token_op
    
    ; Identifier (keyword, register, instruction, or label)
    mov r9, rbx
    mov r10, 0
    
.scan_identifier:
    mov al, [rsi + r9 + r10]
    cmp al, ' '
    je .id_end
    cmp al, 9
    je .id_end
    cmp al, ','
    je .id_end
    cmp al, '['
    je .id_end
    cmp al, ']'
    je .id_end
    cmp al, 0
    je .id_end
    
    inc r10
    cmp r10, 32
    jl .scan_identifier
    
.id_end:
    ; r9 = start, r10 = length
    ; Classify identifier as keyword/register/instruction
    ; TODO: Full keyword matching
    
    mov r11, TOKEN_INSTRUCTION
    mov r14d, COLOR_INSTRUCTION
    jmp .add_token_type
    
.handle_comment:
    ; Rest of line is comment
    mov r10, 0
.find_comment_end:
    cmp [rsi + rbx + r10], byte ptr 0
    je .comment_end
    inc r10
    jmp .find_comment_end
    
.comment_end:
    mov r9, rbx
    mov r11, TOKEN_COMMENT
    mov r14d, COLOR_COMMENT
    jmp .add_token_type
    
.handle_label:
    mov r10, 0
.find_label_end_2:
    cmp [rsi + rbx + r10], byte ptr ':'
    je .label_found
    inc r10
    jmp .find_label_end_2
    
.label_found:
    inc r10              ; Include the ':'
    mov r9, rbx
    mov r11, TOKEN_LABEL
    mov r14d, COLOR_LABEL
    jmp .add_token_type
    
.handle_string:
    mov r11, al         ; Save quote char
    mov r9, rbx
    short mov r10, 1
.scan_string:
    mov al, [rsi + rbx + r10]
    cmp al, r11b
    je .string_end
    cmp al, 0
    je .string_end
    inc r10
    jmp .scan_string
    
.string_end:
    inc r10              ; Include closing quote
    mov r11, TOKEN_STRING
    mov r14d, COLOR_STRING
    jmp .add_token_type
    
.scan_number:
    mov r10, 1
    mov al, [rsi + rbx]
    cmp al, '0'
    jne .hex_or_dec
    
    mov al, [rsi + rbx + 1]
    cmp al, 'x'
    jne .decimal_number
    
    ; Hex number
    add r10, 2
.scan_hex:
    mov al, [rsi + rbx + r10]
    cmp al, '0'
    jl .hex_end
    cmp al, '9'
    jle .hex_digit
    cmp al, 'A'
    jl .hex_end
    cmp al, 'F'
    jle .hex_digit
    cmp al, 'a'
    jl .hex_end
    cmp al, 'f'
    jle .hex_digit
    
.hex_end:
    mov r9, rbx
    mov r11, TOKEN_NUMBER
    mov r14d, COLOR_NUMBER
    jmp .add_token_type
    
.hex_digit:
    inc r10
    jmp .scan_hex
    
.decimal_number:
.scan_decimal:
    mov al, [rsi + rbx + r10]
    cmp al, '0'
    jl .decimal_end
    cmp al, '9'
    jg .decimal_end
    inc r10
    jmp .scan_decimal
    
.decimal_end:
    ; Check for 'h' suffix (hex in MASM)
    mov al, [rsi + rbx + r10]
    cmp al, 'h'
    jne .dec_no_h
    inc r10
.dec_no_h:
    mov r9, rbx
    mov r11, TOKEN_NUMBER
    mov r14d, COLOR_NUMBER
    jmp .add_token_type
    
.hex_or_dec:
    mov r10, 1
.scan_remaining:
    mov al, [rsi + rbx + r10]
    cmp al, '0'
    jl .rem_end
    cmp al, '9'
    jg .rem_end
    inc r10
    jmp .scan_remaining
    
.rem_end:
    mov al, [rsi + rbx + r10]
    cmp al, 'h'
    jne .rem_no_h
    inc r10
.rem_no_h:
    mov r9, rbx
    mov r11, TOKEN_NUMBER
    mov r14d, COLOR_NUMBER
    jmp .add_token_type
    
.add_token_op:
    mov r11, TOKEN_OPERATOR
    mov r14d, COLOR_DEFAULT
    
.add_token_type:
    ; r9 = start offset, r10 = length, r11 = token type, r14d = color
    cmp r13, r12
    jge .done
    
    mov eax, r9d
    mov [rdi + r13*TOKEN_SIZE + TOKEN_START], eax
    mov eax, r10d
    mov [rdi + r13*TOKEN_SIZE + TOKEN_LENGTH], eax
    mov [rdi + r13*TOKEN_SIZE + TOKEN_TYPE], r11b
    mov [rdi + r13*TOKEN_SIZE + TOKEN_COLOR], r14d
    
    inc r13
    add rbx, r10
    jmp .scan_loop
    
.skip_ws:
    inc rbx
    jmp .scan_loop
    
.done:
    mov rax, r13        ; Return token count
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    pop rsi
    pop rdi
    pop rbp
    ret
TOKENIZE_MASM_LINE ENDP

; ============================================================================
; RENDER_TOKENS_TO_DISPLAY
; Takes token array and renders colored text to editor surface
; RCX = token array
; RDX = source line
; R8 = display surface
; R9 = line number
; ============================================================================
RENDER_TOKENS_TO_DISPLAY PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .endprolog
    
    ; TODO: Implement display rendering
    ; Loop through tokens, extract spans, apply colors, render to display
    
    pop rbp
    ret
RENDER_TOKENS_TO_DISPLAY ENDP

END
