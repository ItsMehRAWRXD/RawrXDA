; RXUC-WorkingCompiler v10.1 — FIXED OPERANDS
; Pure x64 MASM — Compiles .tf to .exe
; Assemble: & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" /c terraform.asm
; Link: & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /ENTRY:_start /SUBSYSTEM:CONSOLE terraform.obj kernel32.lib

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


externdef GetCommandLineA:qword
externdef CreateFileA:qword
externdef ReadFile:qword
externdef WriteFile:qword
externdef CloseHandle:qword
externdef GetFileSizeEx:qword
externdef VirtualAlloc:qword
externdef ExitProcess:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword
externdef SetFilePointer:qword

STD_OUTPUT equ -11

TOK_EOF     equ 0
TOK_NUM     equ 1
TOK_ID      equ 2
TOK_STR     equ 3
TOK_LPAREN  equ 4
TOK_RPAREN  equ 5
TOK_LBRACE  equ 6
TOK_RBRACE  equ 7
TOK_PLUS    equ 8
TOK_MINUS   equ 9
TOK_MUL     equ 10
TOK_DIV     equ 11
TOK_ASSIGN  equ 12
TOK_SEMI    equ 13
TOK_COMMA   equ 14
TOK_FN      equ 15
TOK_LET     equ 16
TOK_IF      equ 17
TOK_ELSE    equ 18
TOK_WHILE   equ 19
TOK_RETURN  equ 20

.data
szBanner    db 'RXUC Compiler v10.1',0Dh,0Ah,0
szUsage     db 'Usage: terraform <source.tf>',0Dh,0Ah,0
szError     db '[!] Error',0Dh,0Ah,0
szDone      db '[+] Compiled: output.exe',0Dh,0Ah,0

kw_fn       db 'fn',0
kw_let      db 'let',0
kw_if       db 'if',0
kw_else     db 'else',0
kw_while    db 'while',0
kw_return   db 'return',0

; Character classification lookup table (256 bytes)
CHAR_SPACE  equ 1
CHAR_DIGIT  equ 2
CHAR_ALPHA  equ 4
CHAR_ALNUM  equ 6

charTable   db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  ; 0-15
            db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1  ; 16-31
            db 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; 32-47 (space=1)
            db 2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0  ; 48-63 (digits=2)
            db 0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4  ; 64-79 (A-O=4)
            db 4,4,4,4,4,4,4,4,4,4,4,0,0,0,0,0  ; 80-95 (P-Z=4)
            db 0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4  ; 96-111 (a-o=4)
            db 4,4,4,4,4,4,4,4,4,4,4,0,0,0,0,0  ; 112-127 (p-z=4)
            db 128 dup(0)                        ; 128-255

.data?
align 16

hSource     dq ?
hOutput     dq ?
hStdOut     dq ?
hStdIn      dq ?

pSource     dq ?
cbSource    dq ?
iSource     dq ?
iLine       dd ?

pTokens     dq ?
nTokens     dd ?
iToken      dd ?

pOutput     dq ?
cbOutput    dd ?

MAX_TOKENS  equ 4096
tokens      db MAX_TOKENS * 32 dup(?)

MAX_SYMBOLS equ 256
symTable    db MAX_SYMBOLS * 64 dup(?)
symCount    dd ?
stackOffset dd ?

sourceBuf   db 262144 dup(?)
outputBuf   db 524288 dup(?)
filename    db 260 dup(?)

tempSpace   dq ?

.code

; ================================================
; Utility: Zero memory
; RCX = ptr, RDX = byte count
; ================================================
memzero proc
    ; Optimized memory zeroing using SSE2
    push rdi
    mov rdi, rcx
    mov rax, rdx
    
    ; Use 16-byte aligned stores for large blocks
    cmp rax, 64
    jb small_zero
    
    pxor xmm0, xmm0
    mov rcx, rax
    shr rcx, 4          ; Divide by 16
    
sse_loop:
    movdqu [rdi], xmm0
    add rdi, 16
    dec rcx
    jnz sse_loop
    
    and rax, 15         ; Remainder
    jz zero_done
    
small_zero:
    mov rcx, rax
    xor al, al
    rep stosb
    
zero_done:
    pop rdi
    ret
memzero endp

strlen proc
    ; Optimized strlen using SIMD when possible
    push rsi
    mov rsi, rcx
    xor rax, rax
    
    ; Check alignment
    test rsi, 15
    jnz byte_scan
    
    ; Use SSE2 for aligned strings
    pxor xmm0, xmm0
    
sse_strlen:
    movdqa xmm1, [rsi+rax]
    pcmpeqb xmm1, xmm0
    pmovmskb edx, xmm1
    test edx, edx
    jnz found_zero
    add rax, 16
    jmp sse_strlen
    
found_zero:
    bsf edx, edx
    add rax, rdx
    pop rsi
    ret
    
byte_scan:
    cmp byte ptr [rsi+rax], 0
    je strlen_done
    inc rax
    jmp byte_scan
    
strlen_done:
    pop rsi
    ret
strlen endp

print proc
    push rsi
    push rdi
    mov rsi, rcx
    call strlen
    mov r8, rax
    mov rdx, rsi
    mov rcx, hStdOut
    xor r9, r9
    push 0
    sub rsp, 32
    call WriteConsoleA
    add rsp, 40
    pop rdi
    pop rsi
    ret
print endp

fatal proc
    lea rcx, szError
    call print
    mov ecx, 1
    call ExitProcess
fatal endp

is_space proc
    lea rdx, charTable
    movzx eax, al
    test byte ptr [rdx+rax], CHAR_SPACE
    setnz al
    ret
is_space endp

is_digit proc
    lea rdx, charTable
    movzx eax, al
    test byte ptr [rdx+rax], CHAR_DIGIT
    setnz al
    ret
is_digit endp

is_alpha proc
    lea rdx, charTable
    movzx eax, al
    test byte ptr [rdx+rax], CHAR_ALPHA
    setnz al
    ret
is_alpha endp

peek_char proc
    mov rax, iSource
    cmp rax, cbSource
    jae eof
    mov rcx, pSource
    movzx eax, byte ptr [rcx+rax]
    ret
eof: mov eax, -1
    ret
peek_char endp

next_char proc
    inc iSource
    ret
next_char endp

skip_whitespace proc
L2: call peek_char
    cmp eax, -1
    je D2
    cmp al, ' '
    jbe ws
    ret
ws: call next_char
    cmp al, 0Ah
    jne L2
    inc iLine
    jmp L2
D2: ret
skip_whitespace endp

strcmp proc
    mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne not_eq
    test al, al
    jz is_eq
    inc rsi
    inc rdi
    jmp strcmp
not_eq:
    mov eax, 1
    ret
is_eq:
    xor eax, eax
    ret
strcmp endp

add_token_fast proc
    ; Optimized token addition with fewer memory accesses
    mov r9d, nTokens
    cmp r9d, MAX_TOKENS
    jae overflow_fast
    
    ; Calculate token offset once
    lea r10, tokens
    mov eax, r9d
    shl eax, 5
    add r10, rax
    
    ; Store token data in single operation
    mov [r10], cl        ; type
    mov [r10+4], edx     ; value
    
    test r8, r8
    jz no_name_fast
    
    ; Copy name efficiently
    push rsi
    push rdi
    push rcx
    lea rdi, [r10+8]
    mov rsi, r8
    mov rcx, 4           ; Copy 16 bytes as 4 qwords
    rep movsq
    pop rcx
    pop rdi
    pop rsi
    
no_name_fast:
    inc nTokens
overflow_fast:
    ret
add_token_fast endp

add_token proc
    mov r9d, nTokens
    cmp r9d, MAX_TOKENS
    jae overflow
    shl r9d, 5
    lea r10, tokens
    add r10, r9
    mov [r10], cl
    mov [r10+4], edx
    test r8, r8
    jz no_name
    lea rdi, [r10+8]
    mov rsi, r8
    mov rcx, 16
    rep movsb
no_name:
    inc nTokens
overflow:
    ret
add_token endp

check_keyword proc
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    
    ; Quick length-based dispatch
    call strlen
    cmp rax, 2
    je check_2char
    cmp rax, 3
    je check_3char
    cmp rax, 4
    je check_4char
    cmp rax, 5
    je check_5char
    cmp rax, 6
    je check_6char
    xor eax, eax
    jmp kw_exit
    
check_2char:
    cmp word ptr [rsi], 'nf'  ; "fn"
    je ret_fn
    cmp word ptr [rsi], 'fi'  ; "if"
    je ret_if
    xor eax, eax
    jmp kw_exit
    
check_3char:
    mov eax, [rsi]
    and eax, 0FFFFFFh
    cmp eax, 'tel'  ; "let"
    je ret_let
    xor eax, eax
    jmp kw_exit
    
check_4char:
    cmp dword ptr [rsi], 'esle'  ; "else"
    je ret_else
    xor eax, eax
    jmp kw_exit
    
check_5char:
    cmp dword ptr [rsi], 'lihw'  ; "whil"
    jne kw_fail
    cmp byte ptr [rsi+4], 'e'
    je ret_while
    jmp kw_fail
    
check_6char:
    cmp dword ptr [rsi], 'uter'  ; "retu"
    jne kw_fail
    cmp word ptr [rsi+4], 'nr'
    je ret_return
    
kw_fail:
    xor eax, eax
    jmp kw_exit
    
ret_fn:     mov eax, TOK_FN
    jmp kw_exit
ret_let:    mov eax, TOK_LET
    jmp kw_exit
ret_if:     mov eax, TOK_IF
    jmp kw_exit
ret_else:   mov eax, TOK_ELSE
    jmp kw_exit
ret_while:  mov eax, TOK_WHILE
    jmp kw_exit
ret_return: mov eax, TOK_RETURN
    
kw_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
check_keyword endp

lexer proc
    mov nTokens, 0
    mov iLine, 1
    mov rsi, pSource
    mov rdi, cbSource
    xor r12, r12              ; iSource in register
    
L3: ; Skip whitespace inline
    cmp r12, rdi
    jae eof_tok
    movzx eax, byte ptr [rsi+r12]
    cmp al, ' '
    ja char_ready
    inc r12
    cmp al, 0Ah
    jne L3
    inc iLine
    jmp L3
    
char_ready:
    ; Fast digit check
    sub al, '0'
    cmp al, 9
    jbe number_tok_fast
    add al, '0'
    
    ; Fast alpha check using lookup
    lea rdx, charTable
    movzx ecx, al
    test byte ptr [rdx+rcx], CHAR_ALPHA
    jnz ident_tok_fast
    
    ; Single character tokens
    mov ecx, eax
    inc r12
    
    cmp ecx, '('
    je emit_lparen
    cmp ecx, ')'
    je emit_rparen
    cmp ecx, '{'
    je emit_lbrace
    cmp ecx, '}'
    je emit_rbrace
    cmp ecx, '+'
    je emit_plus
    cmp ecx, '-'
    je emit_minus
    cmp ecx, '*'
    je emit_mul
    cmp ecx, '/'
    je emit_div
    cmp ecx, '='
    je emit_assign
    cmp ecx, ';'
    je emit_semi
    cmp ecx, ','
    je emit_comma
    jmp L3

number_tok_fast:
    xor eax, eax
num_loop:
    cmp r12, rdi
    jae num_done_fast
    movzx edx, byte ptr [rsi+r12]
    sub dl, '0'
    cmp dl, 9
    ja num_done_fast
    imul eax, 10
    add eax, edx
    inc r12
    jmp num_loop
    
num_done_fast:
    mov ecx, TOK_NUM
    mov edx, eax
    xor r8, r8
    call add_token_fast
    jmp L3

ident_tok_fast:
    mov r13, r12              ; Save start
id_loop:
    cmp r12, rdi
    jae id_done_fast
    movzx eax, byte ptr [rsi+r12]
    lea rdx, charTable
    test byte ptr [rdx+rax], CHAR_ALNUM
    jz id_done_fast
    inc r12
    jmp id_loop
    
id_done_fast:
    ; Null terminate temporarily
    mov byte ptr [rsi+r12], 0
    
    ; Check keyword by length
    mov rax, r12
    sub rax, r13              ; Length
    lea rcx, [rsi+r13]        ; String pointer
    
    cmp rax, 2
    je check_2char_fast
    cmp rax, 3
    je check_3char_fast
    cmp rax, 4
    je check_4char_fast
    cmp rax, 5
    je check_5char_fast
    cmp rax, 6
    je check_6char_fast
    
    ; Regular identifier
    mov ecx, TOK_ID
    xor edx, edx
    lea r8, [rsi+r13]
    call add_token_fast
    jmp L3

check_2char_fast:
    cmp word ptr [rcx], 'nf'
    je emit_fn
    cmp word ptr [rcx], 'fi'
    je emit_if
    jmp emit_id
    
check_3char_fast:
    mov eax, [rcx]
    and eax, 0FFFFFFh
    cmp eax, 'tel'
    je emit_let
    jmp emit_id
    
check_4char_fast:
    cmp dword ptr [rcx], 'esle'
    je emit_else
    jmp emit_id
    
check_5char_fast:
    cmp dword ptr [rcx], 'lihw'
    jne emit_id
    cmp byte ptr [rcx+4], 'e'
    je emit_while
    jmp emit_id
    
check_6char_fast:
    cmp dword ptr [rcx], 'uter'
    jne emit_id
    cmp word ptr [rcx+4], 'nr'
    je emit_return
    
emit_id:
    mov ecx, TOK_ID
    xor edx, edx
    lea r8, [rsi+r13]
    call add_token_fast
    jmp L3

emit_fn:     mov eax, TOK_FN
    jmp emit_kw
emit_let:    mov eax, TOK_LET
    jmp emit_kw
emit_if:     mov eax, TOK_IF
    jmp emit_kw
emit_else:   mov eax, TOK_ELSE
    jmp emit_kw
emit_while:  mov eax, TOK_WHILE
    jmp emit_kw
emit_return: mov eax, TOK_RETURN
    
emit_kw:
    mov ecx, eax
    xor edx, edx
    xor r8, r8
    call add_token_fast
    jmp L3

emit_lparen: mov eax, TOK_LPAREN
    jmp emit_simple
emit_rparen: mov eax, TOK_RPAREN
    jmp emit_simple
emit_lbrace: mov eax, TOK_LBRACE
    jmp emit_simple
emit_rbrace: mov eax, TOK_RBRACE
    jmp emit_simple
emit_plus:   mov eax, TOK_PLUS
    jmp emit_simple
emit_minus:  mov eax, TOK_MINUS
    jmp emit_simple
emit_mul:    mov eax, TOK_MUL
    jmp emit_simple
emit_div:    mov eax, TOK_DIV
    jmp emit_simple
emit_assign: mov eax, TOK_ASSIGN
    jmp emit_simple
emit_semi:   mov eax, TOK_SEMI
    jmp emit_simple
emit_comma:  mov eax, TOK_COMMA
    
emit_simple:
    mov ecx, eax
    xor edx, edx
    xor r8, r8
    call add_token_fast
    jmp L3
    
eof_tok:
    mov ecx, TOK_EOF
    xor edx, edx
    xor r8, r8
    call add_token_fast
    
    ; Update global iSource
    mov iSource, r12
    ret
lexer endp

emit_bytes proc
    ; RCX = source buffer, RDX = count
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, pOutput
    add edi, cbOutput
    mov rcx, rdx
    rep movsb
    add cbOutput, edx
    pop rdi
    pop rsi
    ret
emit_bytes endp

emit_byte proc
    mov rax, pOutput
    add eax, cbOutput
    mov [rax], cl
    inc cbOutput
    ret
emit_byte endp

emit_dword proc
    mov rax, pOutput
    add eax, cbOutput
    mov [rax], ecx
    add cbOutput, 4
    ret
emit_dword endp

emit_prolog proc
    ; Emit function prolog as single block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    
    mov qword ptr [rbp-8], 8955489E58348h   ; push rbp; mov rbp,rsp; sub rsp,32
    lea rcx, [rbp-8]
    mov edx, 8
    call emit_bytes
    
    leave
    ret
emit_prolog endp

emit_epilog proc
    ; Emit function epilog as single block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    
    mov qword ptr [rbp-8], 0C35DC48320C483h  ; add rsp,32; pop rbp; ret
    lea rcx, [rbp-8]
    mov edx, 6
    call emit_bytes
    
    leave
    ret
emit_epilog endp

emit_push_imm proc
    cmp rcx, 127
    jg big_imm
    cmp rcx, -128
    jl big_imm
    
    mov al, 6Ah
    mov r12b, cl
    mov cl, al
    call emit_byte
    mov cl, r12b
    call emit_byte
    ret
    
big_imm:
    mov al, 68h
    mov r12d, ecx
    mov cl, al
    call emit_byte
    mov ecx, r12d
    call emit_dword
    ret
emit_push_imm endp

; Inline token access macros for better performance
GET_TOKEN_TYPE MACRO token_idx
    mov eax, token_idx
    shl eax, 5
    lea r10, tokens
    movzx eax, byte ptr [r10+rax]
ENDM

GET_TOKEN_VALUE MACRO token_idx
    mov eax, token_idx
    shl eax, 5
    lea r10, tokens
    mov eax, [r10+rax+4]
ENDM

cur_tok proc
    mov eax, iToken
    cmp eax, nTokens
    jae ct_eof
    GET_TOKEN_TYPE eax
    ret
ct_eof:
    mov eax, TOK_EOF
    ret
cur_tok endp

advance proc
    inc iToken
    ret
advance endp

expect proc
    call cur_tok
    cmp eax, ecx
    je ok_exp
    call fatal
ok_exp:
    jmp advance
expect endp

parse_expr proc
    push rbx
    call parse_term
    
L6: ; Inline token access
    mov eax, iToken
    cmp eax, nTokens
    jae expr_done
    shl eax, 5
    lea r10, tokens
    movzx eax, byte ptr [r10+rax]
    
    cmp eax, TOK_PLUS
    je do_add
    cmp eax, TOK_MINUS
    je do_sub
expr_done:
    pop rbx
    ret
    
do_add:
    inc iToken
    call parse_term
    
    ; Emit add instruction as block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp-4], 48D00148h  ; add rax,rdx (48 01 D0)
    lea rcx, [rbp-4]
    mov edx, 3
    call emit_bytes
    leave
    jmp L6
    
do_sub:
    inc iToken
    call parse_term
    
    ; Emit sub instruction as block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp-4], 48D02948h  ; sub rax,rdx (48 29 D0)
    lea rcx, [rbp-4]
    mov edx, 3
    call emit_bytes
    leave
    jmp L6
parse_expr endp

parse_term proc
    push rbx
    call parse_factor
    
L7: ; Inline token access
    mov eax, iToken
    cmp eax, nTokens
    jae term_done
    shl eax, 5
    lea r10, tokens
    movzx eax, byte ptr [r10+rax]
    
    cmp eax, TOK_MUL
    je do_mul
    cmp eax, TOK_DIV
    je do_div
term_done:
    pop rbx
    ret
    
do_mul:
    inc iToken
    call parse_factor
    
    ; Emit mul instruction as block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp-4], 48E2F748h  ; imul rax,rdx (48 F7 E2)
    lea rcx, [rbp-4]
    mov edx, 3
    call emit_bytes
    leave
    jmp L7
    
do_div:
    inc iToken
    call parse_factor
    
    ; Emit div sequence as block
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov qword ptr [rbp-8], 48F1F74831D248h  ; xor rdx,rdx; idiv rcx
    lea rcx, [rbp-8]
    mov edx, 6
    call emit_bytes
    leave
    jmp L7
parse_term endp

parse_factor proc
    call cur_tok
    cmp eax, TOK_NUM
    je pf_num
    cmp eax, TOK_LPAREN
    je pf_paren
    cmp eax, TOK_MINUS
    je pf_neg
    call fatal
    
pf_num:
    mov eax, iToken
    shl eax, 5
    lea r10, tokens
    add r10, rax
    mov ecx, [r10+4]
    call emit_push_imm
    call advance
    ret
    
pf_paren:
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    ret
    
pf_neg:
    call advance
    call parse_factor
    
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret
parse_factor endp

parse_stmt proc
    call cur_tok
    cmp eax, TOK_LET
    je ps_let
    cmp eax, TOK_IF
    je ps_if
    cmp eax, TOK_WHILE
    je ps_while
    cmp eax, TOK_RETURN
    je ps_return
    cmp eax, TOK_LBRACE
    je ps_block
    
    call parse_expr
    mov ecx, TOK_SEMI
    call expect
    ret
    
ps_let:
    mov ecx, TOK_LET
    call expect
    mov ecx, TOK_ID
    call expect
    mov ecx, TOK_ASSIGN
    call expect
    call parse_expr
    mov ecx, TOK_SEMI
    call expect
    ret
    
ps_if:
    mov ecx, TOK_IF
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r12d, cbOutput
    xor ecx, ecx
    call emit_dword
    
    call parse_stmt
    
    mov cl, 0E9h
    call emit_byte
    mov r13d, cbOutput
    xor ecx, ecx
    call emit_dword
    
    mov eax, cbOutput
    sub eax, r12d
    sub eax, 4
    mov r10, pOutput
    add r10d, r12d
    mov [r10], eax
    
    call cur_tok
    cmp eax, TOK_ELSE
    jne no_else
    call advance
    call parse_stmt
    
no_else:
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    add r10d, r13d
    mov [r10], eax
    ret
    
ps_while:
    mov r12d, cbOutput
    
    mov ecx, TOK_WHILE
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r13d, cbOutput
    xor ecx, ecx
    call emit_dword
    
    call parse_stmt
    
    mov cl, 0E9h
    call emit_byte
    mov eax, r12d
    sub eax, cbOutput
    sub eax, 4
    mov ecx, eax
    call emit_dword
    
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    add r10d, r13d
    mov [r10], eax
    ret
    
ps_return:
    mov ecx, TOK_RETURN
    call expect
    call cur_tok
    cmp eax, TOK_SEMI
    je void_ret
    call parse_expr
void_ret:
    mov ecx, TOK_SEMI
    call expect
    call emit_epilog
    ret
    
ps_block:
    mov ecx, TOK_LBRACE
    call expect
L8: call cur_tok
    cmp eax, TOK_RBRACE
    je blk_done
    cmp eax, TOK_EOF
    je blk_done
    call parse_stmt
    jmp L8
blk_done:
    mov ecx, TOK_RBRACE
    call expect
    ret
parse_stmt endp

parse_function proc
    mov ecx, TOK_FN
    call expect
    mov ecx, TOK_ID
    call expect
    mov ecx, TOK_LPAREN
    call expect
    mov ecx, TOK_RPAREN
    call expect
    
    call emit_prolog
    call parse_stmt
    call emit_epilog
    ret
parse_function endp

write_pe_file proc
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    lea rcx, filename
    mov edx, 40000000h
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+48], 2
    mov qword ptr [rsp+56], 80h
    xor eax, eax
    mov qword ptr [rsp+64], rax
    call CreateFileA
    cmp rax, -1
    je pe_fail
    mov hOutput, rax
    
    mov eax, cbOutput
    add eax, 511
    and eax, -512
    mov [rbp-8], eax
    
    ; Write DOS header (64 bytes zeroed + MZ signature + PE offset)
    sub rsp, 88
    mov rcx, rsp
    mov edx, 64
    call memzero
    
    mov word ptr [rsp], 5A4Dh
    mov dword ptr [rsp+60], 64
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 64
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    add rsp, 88
    
    ; Write PE signature
    mov rcx, hOutput
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Write COFF header + Optional header + Section header
    sub rsp, 264
    mov rcx, rsp
    mov edx, 264
    call memzero
    
    ; COFF header (24 bytes)
    mov word ptr [rsp], 8664h         ; Machine: x64
    mov word ptr [rsp+2], 1           ; NumberOfSections
    mov word ptr [rsp+20], 240        ; SizeOfOptionalHeader
    mov word ptr [rsp+22], 22h        ; Characteristics
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 24
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Optional header (PE32+ = 240 bytes)
    mov word ptr [rsp], 20Bh          ; Magic: PE32+
    mov byte ptr [rsp+2], 1           ; MajorLinkerVersion
    mov eax, [rbp-8]
    mov dword ptr [rsp+4], eax        ; SizeOfCode
    mov dword ptr [rsp+16], 1000h     ; AddressOfEntryPoint
    mov dword ptr [rsp+20], 1000h     ; BaseOfCode
    mov rax, 140000000h
    mov qword ptr [rsp+24], rax       ; ImageBase
    mov dword ptr [rsp+32], 1000h     ; SectionAlignment
    mov dword ptr [rsp+36], 200h      ; FileAlignment
    mov word ptr [rsp+68], 3          ; Subsystem: CONSOLE
    mov rax, 100000h
    mov qword ptr [rsp+72], rax       ; SizeOfStackReserve
    mov rax, 1000h
    mov qword ptr [rsp+80], rax       ; SizeOfStackCommit
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 240
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Section header: .text (40 bytes)
    mov rcx, rsp
    mov edx, 40
    call memzero
    
    mov rax, 'xet.'
    mov qword ptr [rsp], rax          ; Name: .text
    mov eax, [rbp-8]
    mov dword ptr [rsp+8], eax        ; VirtualSize
    mov dword ptr [rsp+12], 1000h     ; VirtualAddress
    mov dword ptr [rsp+16], eax       ; SizeOfRawData
    mov dword ptr [rsp+20], 400h      ; PointerToRawData
    mov dword ptr [rsp+36], 60000020h ; Characteristics: CODE|EXECUTE|READ
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 40
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    add rsp, 264
    
    ; Seek to 0x400 (code section offset)
    mov rcx, hOutput
    xor edx, edx
    mov r8d, 400h
    xor r9d, r9d
    call SetFilePointer
    
    ; Write compiled code
    mov rcx, hOutput
    mov rdx, pOutput
    mov r8d, cbOutput
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov rcx, hOutput
    call CloseHandle
    
    xor eax, eax
    leave
    ret
    
pe_fail:
    mov eax, 1
    leave
    ret
write_pe_file endp

.data
pe_sig db 'PE',0,0

.code

; ================================================
; Entry Point
; ================================================
_start:
    sub rsp, 56
    
    mov ecx, STD_OUTPUT
    call GetStdHandle
    mov hStdOut, rax
    
    lea rcx, szBanner
    call print
    
    lea rax, sourceBuf
    mov pSource, rax
    lea rax, tokens
    mov pTokens, rax
    lea rax, outputBuf
    mov pOutput, rax
    
    ; Parse command line
    call GetCommandLineA
    mov rsi, rax
    
    cmp byte ptr [rsi], '"'
    je skip_q
L9: lodsb
    cmp al, ' '
    jne L9
    jmp skip_sp
skip_q:
    lodsb
    cmp al, '"'
    jne skip_q
skip_sp:
    lodsb
    cmp al, ' '
    je skip_sp
    dec rsi
    
    cmp al, 0
    je usage
    
    mov r12, rsi
    
    ; Copy filename
    lea rdi, filename
    mov rcx, 260
    rep movsb
    
    ; Open source file
    lea rcx, filename
    xor edx, edx
    mov r8d, 3
    xor r9d, r9d
    mov qword ptr [rsp+32], 3
    mov qword ptr [rsp+40], 80h
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    cmp rax, -1
    je file_err
    mov hSource, rax
    
    ; Get file size
    lea rdx, cbSource
    mov rcx, hSource
    call GetFileSizeEx
    
    ; Read source
    mov rcx, hSource
    mov rdx, pSource
    mov r8, cbSource
    lea r9, tempSpace
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    mov rcx, hSource
    call CloseHandle
    
    ; Lex
    call lexer
    
    ; Parse & codegen
    mov cbOutput, 0
    mov iToken, 0
    
    call parse_function
    
    ; Build output filename (.tf -> .exe)
    lea rsi, filename
L10:
    lodsb
    cmp al, '.'
    je ext_f
    test al, al
    jnz L10
    dec rsi
ext_f:
    mov dword ptr [rsi], 'exe.'
    mov byte ptr [rsi+4], 0
    
    ; Write PE
    call write_pe_file
    
    lea rcx, szDone
    call print
    
    xor ecx, ecx
    call ExitProcess
    
usage:
    lea rcx, szUsage
    call print
    mov ecx, 1
    call ExitProcess
    
file_err:
    call fatal

end
