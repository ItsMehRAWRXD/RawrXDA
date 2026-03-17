; RXUC-WorkingCompiler v10.1 — FIXED OPERANDS
; Pure x64 MASM — Compiles .tf to .exe
; Assemble: ml64 /c /nologo terraform.asm
; Link: link /ENTRY:_start /SUBSYSTEM:CONSOLE terraform.obj kernel32.lib

option casemap:none
option frame:none

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

MAX_TOKENS  equ 4096
MAX_SYMBOLS equ 256

.data
szBanner    db 'RXUC Compiler v10.1',0Dh,0Ah,0
szUsage     db 'Usage: terraform <source.tf>',0Dh,0Ah,0
szError     db '[!] Error',0Dh,0Ah,0
szDone      db '[+] Compiled: output.exe',0Dh,0Ah,0
pe_sig      db 'PE',0,0

kw_fn       db 'fn',0
kw_let      db 'let',0
kw_if       db 'if',0
kw_else     db 'else',0
kw_while    db 'while',0
kw_return   db 'return',0

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

tokens      db MAX_TOKENS * 32 dup(?)
symTable    db MAX_SYMBOLS * 64 dup(?)
symCount    dd ?
stackOffset dd ?

sourceBuf   db 262144 dup(?)
outputBuf   db 524288 dup(?)
filename    db 260 dup(?)

tempSpace   dq ?

.code

; =========================================================================
; Utility: strlen — rcx=string, returns length in rax
; =========================================================================
strlen proc
    xor rax, rax
L1: cmp byte ptr [rcx+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

; =========================================================================
; Utility: print — rcx=string
; =========================================================================
print proc
    push rsi
    push rdi
    sub rsp, 48
    mov rsi, rcx
    call strlen
    mov r8, rax
    mov rdx, rsi
    mov rcx, hStdOut
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteConsoleA
    add rsp, 48
    pop rdi
    pop rsi
    ret
print endp

; =========================================================================
; Utility: fatal — print error and exit
; =========================================================================
fatal proc
    lea rcx, szError
    call print
    mov ecx, 1
    call ExitProcess
fatal endp

; =========================================================================
; Utility: memzero — rcx=ptr, rdx=len
; =========================================================================
memzero proc
    push rdi
    mov rdi, rcx
    mov rcx, rdx
    xor al, al
    rep stosb
    pop rdi
    ret
memzero endp

; =========================================================================
; Char classification helpers
; =========================================================================
is_digit proc
    cmp al, '0'
    jb idno
    cmp al, '9'
    ja idno
    mov al, 1
    ret
idno:
    xor al, al
    ret
is_digit endp

is_alpha proc
    cmp al, 'a'
    jb check_upper
    cmp al, 'z'
    ja no_alpha
    mov al, 1
    ret
check_upper:
    cmp al, 'A'
    jb no_alpha
    cmp al, 'Z'
    ja no_alpha
    mov al, 1
    ret
no_alpha:
    xor al, al
    ret
is_alpha endp

; =========================================================================
; Lexer: peek_char — returns char in eax or -1 at EOF
; =========================================================================
peek_char proc
    mov rax, iSource
    cmp rax, cbSource
    jae pc_eof
    mov rcx, pSource
    movzx eax, byte ptr [rcx+rax]
    ret
pc_eof:
    mov eax, -1
    ret
peek_char endp

; =========================================================================
; Lexer: next_char — advance source position
; =========================================================================
next_char proc
    inc iSource
    ret
next_char endp

; =========================================================================
; Lexer: skip_whitespace
; =========================================================================
skip_whitespace proc
sw_lp:
    call peek_char
    cmp eax, -1
    je sw_done
    cmp al, ' '
    je sw_skip
    cmp al, 9
    je sw_skip
    cmp al, 13
    je sw_skip
    cmp al, 10
    je sw_nl
    ret
sw_skip:
    call next_char
    jmp sw_lp
sw_nl:
    call next_char
    inc iLine
    jmp sw_lp
sw_done:
    ret
skip_whitespace endp

; =========================================================================
; Utility: strcmp — rsi=s1, rdi=s2, returns 0 if equal
; =========================================================================
strcmp proc
sc_lp:
    mov al, byte ptr [rsi]
    mov bl, byte ptr [rdi]
    cmp al, bl
    jne sc_neq
    test al, al
    jz sc_eq
    inc rsi
    inc rdi
    jmp sc_lp
sc_neq:
    mov eax, 1
    ret
sc_eq:
    xor eax, eax
    ret
strcmp endp

; =========================================================================
; Lexer: add_token — cl=type, edx=value, r8=name_ptr(or 0)
; =========================================================================
add_token proc
    mov r9d, nTokens
    cmp r9d, MAX_TOKENS
    jae at_done
    shl r9d, 5
    lea r10, tokens
    add r10, r9
    mov byte ptr [r10], cl
    mov dword ptr [r10+4], edx
    test r8, r8
    jz at_noname
    push rcx
    lea rdi, [r10+8]
    mov rsi, r8
    mov rcx, 16
    rep movsb
    pop rcx
at_noname:
    inc nTokens
at_done:
    ret
add_token endp

; =========================================================================
; Lexer: check_keyword — rcx=string, returns token type or 0
; =========================================================================
check_keyword proc
    push rsi
    push rdi
    push rbx
    mov rbx, rcx

    mov rsi, rbx
    lea rdi, kw_fn
    call strcmp
    test eax, eax
    jz ck_fn

    mov rsi, rbx
    lea rdi, kw_let
    call strcmp
    test eax, eax
    jz ck_let

    mov rsi, rbx
    lea rdi, kw_if
    call strcmp
    test eax, eax
    jz ck_if

    mov rsi, rbx
    lea rdi, kw_else
    call strcmp
    test eax, eax
    jz ck_else

    mov rsi, rbx
    lea rdi, kw_while
    call strcmp
    test eax, eax
    jz ck_while

    mov rsi, rbx
    lea rdi, kw_return
    call strcmp
    test eax, eax
    jz ck_ret

    xor eax, eax
    jmp ck_done

ck_fn:    mov eax, TOK_FN
    jmp ck_done
ck_let:   mov eax, TOK_LET
    jmp ck_done
ck_if:    mov eax, TOK_IF
    jmp ck_done
ck_else:  mov eax, TOK_ELSE
    jmp ck_done
ck_while: mov eax, TOK_WHILE
    jmp ck_done
ck_ret:   mov eax, TOK_RETURN

ck_done:
    pop rbx
    pop rdi
    pop rsi
    ret
check_keyword endp

; =========================================================================
; Lexer: main tokenizer
; =========================================================================
lexer proc
    push r12
    push r13
    push r14
    push rbx
    mov nTokens, 0
    mov iLine, 1

lx_loop:
    call skip_whitespace
    call peek_char
    cmp eax, -1
    je lx_eof

    ; Number?
    cmp al, '0'
    jb lx_not_num
    cmp al, '9'
    jbe lx_number
lx_not_num:

    ; Identifier / keyword?
    cmp al, 'a'
    jb lx_chk_upper
    cmp al, 'z'
    jbe lx_ident
lx_chk_upper:
    cmp al, 'A'
    jb lx_chk_under
    cmp al, 'Z'
    jbe lx_ident
lx_chk_under:
    cmp al, '_'
    je lx_ident

    ; Symbols
    jmp lx_symbol

; --- Number literal ---
lx_number:
    xor r12d, r12d              ; accumulator
lx_num_lp:
    call peek_char
    cmp al, '0'
    jb lx_num_done
    cmp al, '9'
    ja lx_num_done
    sub al, '0'
    movzx edx, al
    imul r12d, r12d, 10
    add r12d, edx
    call next_char
    jmp lx_num_lp
lx_num_done:
    mov cl, TOK_NUM
    mov edx, r12d
    xor r8, r8
    call add_token
    jmp lx_loop

; --- Identifier ---
lx_ident:
    mov r13, iSource            ; save start

lx_id_lp:
    call peek_char
    cmp al, '_'
    je lx_id_ch
    call is_alpha
    test al, al
    jnz lx_id_ch
    call peek_char
    call is_digit
    test al, al
    jz lx_id_done
lx_id_ch:
    call next_char
    jmp lx_id_lp

lx_id_done:
    ; Null-terminate in source buffer
    mov rax, iSource
    mov rcx, pSource
    mov byte ptr [rcx+rax], 0

    ; Check keyword
    mov rcx, pSource
    add rcx, r13
    mov r14, rcx               ; save string ptr
    call check_keyword
    test eax, eax
    jnz lx_kw

    ; Regular identifier
    mov cl, TOK_ID
    xor edx, edx
    mov r8, r14
    call add_token
    jmp lx_loop

lx_kw:
    mov cl, al
    xor edx, edx
    xor r8, r8
    call add_token
    jmp lx_loop

; --- Symbol tokens ---
lx_symbol:
    mov r12d, eax
    call next_char

    cmp r12d, '('
    je lx_lp
    cmp r12d, ')'
    je lx_rp
    cmp r12d, '{'
    je lx_lb
    cmp r12d, '}'
    je lx_rb
    cmp r12d, '+'
    je lx_plus
    cmp r12d, '-'
    je lx_minus
    cmp r12d, '*'
    je lx_mul
    cmp r12d, '/'
    je lx_div
    cmp r12d, '='
    je lx_assign
    cmp r12d, ';'
    je lx_semi
    cmp r12d, ','
    je lx_comma
    ; Unknown char — skip
    jmp lx_loop

lx_lp:     mov cl, TOK_LPAREN
    jmp lx_addsym
lx_rp:     mov cl, TOK_RPAREN
    jmp lx_addsym
lx_lb:     mov cl, TOK_LBRACE
    jmp lx_addsym
lx_rb:     mov cl, TOK_RBRACE
    jmp lx_addsym
lx_plus:   mov cl, TOK_PLUS
    jmp lx_addsym
lx_minus:  mov cl, TOK_MINUS
    jmp lx_addsym
lx_mul:    mov cl, TOK_MUL
    jmp lx_addsym
lx_div:    mov cl, TOK_DIV
    jmp lx_addsym
lx_assign: mov cl, TOK_ASSIGN
    jmp lx_addsym
lx_semi:   mov cl, TOK_SEMI
    jmp lx_addsym
lx_comma:  mov cl, TOK_COMMA

lx_addsym:
    xor edx, edx
    xor r8, r8
    call add_token
    jmp lx_loop

lx_eof:
    mov cl, TOK_EOF
    xor edx, edx
    xor r8, r8
    call add_token

    pop rbx
    pop r14
    pop r13
    pop r12
    ret
lexer endp

; =========================================================================
; Emitter: emit_byte — cl=byte
; =========================================================================
emit_byte proc
    mov eax, cbOutput
    mov r10, pOutput
    add r10, rax
    mov byte ptr [r10], cl
    inc cbOutput
    ret
emit_byte endp

; =========================================================================
; Emitter: emit_dword — ecx=dword
; =========================================================================
emit_dword proc
    mov eax, cbOutput
    mov r10, pOutput
    add r10, rax
    mov dword ptr [r10], ecx
    add cbOutput, 4
    ret
emit_dword endp

; =========================================================================
; Emitter: emit_prolog — push rbp; mov rbp,rsp; sub rsp,20h
; =========================================================================
emit_prolog proc
    mov cl, 55h                ; push rbp
    call emit_byte
    mov cl, 48h                ; REX.W
    call emit_byte
    mov cl, 89h                ; mov rbp, rsp
    call emit_byte
    mov cl, 0E5h
    call emit_byte
    mov cl, 48h                ; REX.W
    call emit_byte
    mov cl, 83h                ; sub rsp, imm8
    call emit_byte
    mov cl, 0ECh
    call emit_byte
    mov cl, 20h
    call emit_byte
    ret
emit_prolog endp

; =========================================================================
; Emitter: emit_epilog — add rsp,20h; pop rbp; ret
; =========================================================================
emit_epilog proc
    mov cl, 48h                ; REX.W
    call emit_byte
    mov cl, 83h                ; add rsp, imm8
    call emit_byte
    mov cl, 0C4h
    call emit_byte
    mov cl, 20h
    call emit_byte
    mov cl, 5Dh                ; pop rbp
    call emit_byte
    mov cl, 0C3h               ; ret
    call emit_byte
    ret
emit_epilog endp

; =========================================================================
; Emitter: emit_push_imm — rcx=value
; =========================================================================
emit_push_imm proc
    cmp rcx, 127
    jg epi_big
    cmp rcx, -128
    jl epi_big
    mov r12b, cl
    mov cl, 6Ah                ; push imm8
    call emit_byte
    mov cl, r12b
    call emit_byte
    ret
epi_big:
    mov r12d, ecx
    mov cl, 68h                ; push imm32
    call emit_byte
    mov ecx, r12d
    call emit_dword
    ret
emit_push_imm endp

; =========================================================================
; Parser: cur_tok — returns token type in eax
; =========================================================================
cur_tok proc
    mov eax, iToken
    cmp eax, nTokens
    jae ct_eof
    shl eax, 5
    lea r10, tokens
    add r10, rax
    movzx eax, byte ptr [r10]
    ret
ct_eof:
    mov eax, TOK_EOF
    ret
cur_tok endp

; =========================================================================
; Parser: advance — move to next token
; =========================================================================
advance proc
    inc iToken
    ret
advance endp

; =========================================================================
; Parser: expect — eax must match ecx, else fatal
; =========================================================================
expect proc
    push rcx
    call cur_tok
    pop rcx
    cmp eax, ecx
    je exp_ok
    call fatal
exp_ok:
    jmp advance
expect endp

; =========================================================================
; Parser: parse_expr — additive expressions
; =========================================================================
parse_expr proc
    push rbx
    call parse_term

pe_loop:
    call cur_tok
    cmp eax, TOK_PLUS
    je pe_add
    cmp eax, TOK_MINUS
    je pe_sub
    pop rbx
    ret

pe_add:
    call advance
    push rax
    call parse_term
    pop rdx
    ; emit: add rax, rdx  (48 01 D0)
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    jmp pe_loop

pe_sub:
    call advance
    push rax
    call parse_term
    pop rdx
    xchg rax, rdx
    ; emit: sub rax, rdx  (48 29 D0)
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    jmp pe_loop
parse_expr endp

; =========================================================================
; Parser: parse_term — multiplicative expressions
; =========================================================================
parse_term proc
    push rbx
    call parse_factor

pt_loop:
    call cur_tok
    cmp eax, TOK_MUL
    je pt_mul
    cmp eax, TOK_DIV
    je pt_div
    pop rbx
    ret

pt_mul:
    call advance
    push rax
    call parse_factor
    pop rdx
    ; emit: mul rdx  (48 F7 E2)
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0E2h
    call emit_byte
    jmp pt_loop

pt_div:
    call advance
    push rax
    call parse_factor
    mov rcx, rax
    pop rax
    push rcx
    ; emit: xor rdx, rdx  (48 31 D2)
    mov cl, 48h
    call emit_byte
    mov cl, 31h
    call emit_byte
    mov cl, 0D2h
    call emit_byte
    pop rcx
    push rax
    mov rax, rcx
    ; emit: div rcx  (48 F7 F1)
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0F1h
    call emit_byte
    pop rax
    jmp pt_loop
parse_term endp

; =========================================================================
; Parser: parse_factor — atoms
; =========================================================================
parse_factor proc
    call cur_tok
    cmp eax, TOK_NUM
    je pf_num
    cmp eax, TOK_LPAREN
    je pf_paren
    cmp eax, TOK_MINUS
    je pf_neg
    cmp eax, TOK_ID
    je pf_id
    call fatal

pf_num:
    mov eax, iToken
    shl eax, 5
    lea r10, tokens
    add r10, rax
    mov ecx, dword ptr [r10+4]
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
    ; emit: neg rax  (48 F7 D8)
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret

pf_id:
    call advance
    ret
parse_factor endp

; =========================================================================
; Parser: parse_stmt
; =========================================================================
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

    ; Expression statement
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
    push r12
    push r13
    mov ecx, TOK_IF
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    ; emit: cmp rax, 0  (48 83 F8 00)
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    ; emit: je (0F 84 xx xx xx xx)
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r12d, cbOutput          ; patch point for if-skip
    xor ecx, ecx
    call emit_dword

    call parse_stmt

    ; emit: jmp (E9 xx xx xx xx)
    mov cl, 0E9h
    call emit_byte
    mov r13d, cbOutput          ; patch point for else-skip
    xor ecx, ecx
    call emit_dword

    ; Patch if-skip offset
    mov eax, cbOutput
    sub eax, r12d
    sub eax, 4
    mov r10, pOutput
    movsxd r11, r12d
    add r10, r11
    mov dword ptr [r10], eax

    call cur_tok
    cmp eax, TOK_ELSE
    jne ps_no_else
    call advance
    call parse_stmt
ps_no_else:
    ; Patch else-skip offset
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    movsxd r11, r13d
    add r10, r11
    mov dword ptr [r10], eax

    pop r13
    pop r12
    ret

ps_while:
    push r12
    push r13
    mov r12d, cbOutput          ; loop start

    mov ecx, TOK_WHILE
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    ; emit: cmp rax, 0
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    ; emit: je (exit loop)
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r13d, cbOutput          ; patch point for exit
    xor ecx, ecx
    call emit_dword

    call parse_stmt

    ; emit: jmp back to loop start
    mov cl, 0E9h
    call emit_byte
    mov eax, r12d
    sub eax, cbOutput
    sub eax, 4
    mov ecx, eax
    call emit_dword

    ; Patch exit offset
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    movsxd r11, r13d
    add r10, r11
    mov dword ptr [r10], eax

    pop r13
    pop r12
    ret

ps_return:
    mov ecx, TOK_RETURN
    call expect
    call cur_tok
    cmp eax, TOK_SEMI
    je ps_void_ret
    call parse_expr
ps_void_ret:
    mov ecx, TOK_SEMI
    call expect
    call emit_epilog
    ret

ps_block:
    mov ecx, TOK_LBRACE
    call expect
ps_blk_lp:
    call cur_tok
    cmp eax, TOK_RBRACE
    je ps_blk_done
    cmp eax, TOK_EOF
    je ps_blk_done
    call parse_stmt
    jmp ps_blk_lp
ps_blk_done:
    mov ecx, TOK_RBRACE
    call expect
    ret
parse_stmt endp

; =========================================================================
; Parser: parse_function
; =========================================================================
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

; =========================================================================
; PE Writer: writes minimal PE64 executable
; =========================================================================
write_pe_file proc
    push rbp
    mov rbp, rsp
    sub rsp, 400                ; big local frame

    ; Open output file
    lea rcx, filename
    mov edx, 40000000h         ; GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 2   ; CREATE_ALWAYS
    mov qword ptr [rsp+8], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+16], 0
    call CreateFileA
    add rsp, 32
    cmp rax, -1
    je pe_fail
    mov hOutput, rax

    ; Calculate aligned code size
    mov eax, cbOutput
    add eax, 511
    and eax, -512
    mov dword ptr [rbp-8], eax  ; aligned_size

    ; --- Write DOS Header (64 bytes) ---
    lea rcx, [rbp-128]         ; local buffer
    mov rdx, 64
    call memzero
    lea rcx, [rbp-128]
    mov word ptr [rcx], 5A4Dh  ; e_magic
    mov dword ptr [rcx+60], 64 ; e_lfanew

    mov rcx, hOutput
    lea rdx, [rbp-128]
    mov r8d, 64
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; --- Write PE Signature (4 bytes) ---
    mov rcx, hOutput
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; --- Write COFF Header (20 bytes) ---
    lea rcx, [rbp-128]
    mov rdx, 24
    call memzero
    lea rcx, [rbp-128]
    mov word ptr [rcx], 8664h       ; Machine: AMD64
    mov word ptr [rcx+2], 1         ; NumberOfSections
    mov word ptr [rcx+16], 240      ; SizeOfOptionalHeader
    mov word ptr [rcx+18], 22h      ; Characteristics

    mov rcx, hOutput
    lea rdx, [rbp-128]
    mov r8d, 20
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; --- Write Optional Header (240 bytes) ---
    lea rcx, [rbp-380]
    mov rdx, 240
    call memzero
    lea rcx, [rbp-380]
    mov word ptr [rcx], 20Bh        ; PE32+ magic
    mov byte ptr [rcx+2], 1         ; MajorLinkerVersion
    mov eax, dword ptr [rbp-8]
    mov dword ptr [rcx+4], eax      ; SizeOfCode
    mov dword ptr [rcx+16], 1000h   ; AddressOfEntryPoint
    mov dword ptr [rcx+20], 1000h   ; BaseOfCode
    mov rax, 140000000h
    mov qword ptr [rcx+24], rax     ; ImageBase
    mov dword ptr [rcx+32], 1000h   ; SectionAlignment
    mov dword ptr [rcx+36], 200h    ; FileAlignment
    mov word ptr [rcx+40], 6        ; MajorOSVersion
    mov word ptr [rcx+44], 6        ; MajorSubsystemVersion
    mov eax, dword ptr [rbp-8]
    add eax, 1000h
    mov dword ptr [rcx+56], eax     ; SizeOfImage
    mov dword ptr [rcx+60], 400h    ; SizeOfHeaders
    mov word ptr [rcx+68], 3        ; Subsystem: CONSOLE
    mov rax, 100000h
    mov qword ptr [rcx+72], rax     ; SizeOfStackReserve
    mov rax, 1000h
    mov qword ptr [rcx+80], rax     ; SizeOfStackCommit
    mov rax, 100000h
    mov qword ptr [rcx+88], rax     ; SizeOfHeapReserve
    mov rax, 1000h
    mov qword ptr [rcx+96], rax     ; SizeOfHeapCommit
    mov dword ptr [rcx+108], 16     ; NumberOfRvaAndSizes

    mov rcx, hOutput
    lea rdx, [rbp-380]
    mov r8d, 240
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; --- Write Section Header ".text" (40 bytes) ---
    lea rcx, [rbp-128]
    mov rdx, 40
    call memzero
    lea rcx, [rbp-128]
    mov byte ptr [rcx], '.'
    mov byte ptr [rcx+1], 't'
    mov byte ptr [rcx+2], 'e'
    mov byte ptr [rcx+3], 'x'
    mov byte ptr [rcx+4], 't'
    mov eax, dword ptr [rbp-8]
    mov dword ptr [rcx+8], eax      ; VirtualSize
    mov dword ptr [rcx+12], 1000h   ; VirtualAddress
    mov dword ptr [rcx+16], eax     ; SizeOfRawData
    mov dword ptr [rcx+20], 400h    ; PointerToRawData
    mov dword ptr [rcx+36], 60000020h ; Characteristics: code+exec+read

    mov rcx, hOutput
    lea rdx, [rbp-128]
    mov r8d, 40
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; --- Seek to code section offset (0x400) ---
    mov rcx, hOutput
    xor edx, edx
    mov r8d, 400h
    xor r9d, r9d
    call SetFilePointer

    ; --- Write code ---
    mov rcx, hOutput
    mov rdx, pOutput
    mov r8d, cbOutput
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call WriteFile
    add rsp, 32

    ; Close file
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

; =========================================================================
; Entry Point
; =========================================================================
_start:
    sub rsp, 56

    ; Get stdout handle
    mov ecx, STD_OUTPUT
    call GetStdHandle
    mov hStdOut, rax

    ; Print banner
    lea rcx, szBanner
    call print

    ; Set buffer pointers
    lea rax, sourceBuf
    mov pSource, rax
    lea rax, tokens
    mov pTokens, rax
    lea rax, outputBuf
    mov pOutput, rax

    ; Get command line
    call GetCommandLineA
    mov rsi, rax

    ; Skip past program name
    cmp byte ptr [rsi], '"'
    je main_skip_q
main_skip_exe:
    lodsb
    cmp al, ' '
    jne main_skip_exe
    jmp main_skip_sp
main_skip_q:
    lodsb
    cmp al, '"'
    jne main_skip_q
main_skip_sp:
    lodsb
    cmp al, ' '
    je main_skip_sp
    dec rsi

    ; Check if filename provided
    cmp al, 0
    je main_usage

    mov r12, rsi

    ; Copy filename
    lea rdi, filename
    mov rcx, 260
    rep movsb

    ; Open source file
    lea rcx, filename
    xor edx, edx               ; GENERIC_READ
    mov r8d, 3                  ; FILE_SHARE_READ|WRITE
    xor r9d, r9d
    sub rsp, 32
    mov qword ptr [rsp+0], 3   ; OPEN_EXISTING
    mov qword ptr [rsp+8], 80h
    mov qword ptr [rsp+16], 0
    call CreateFileA
    add rsp, 32
    cmp rax, -1
    je main_file_err
    mov hSource, rax

    ; Get file size
    lea rdx, cbSource
    mov rcx, hSource
    call GetFileSizeEx

    ; Read file
    mov rcx, hSource
    mov rdx, pSource
    mov r8, cbSource
    lea r9, tempSpace
    sub rsp, 32
    mov qword ptr [rsp+0], 0
    call ReadFile
    add rsp, 32

    ; Close source file
    mov rcx, hSource
    call CloseHandle

    ; Tokenize
    mov qword ptr iSource, 0
    call lexer

    ; Parse & codegen
    mov cbOutput, 0
    mov iToken, 0

main_parse_lp:
    call cur_tok
    cmp eax, TOK_EOF
    je main_write
    cmp eax, TOK_FN
    je main_do_fn
    call advance
    jmp main_parse_lp

main_do_fn:
    call parse_function
    jmp main_parse_lp

main_write:
    ; Build output filename (replace .tf with .exe)
    lea rsi, filename
main_ext_lp:
    lodsb
    cmp al, '.'
    je main_ext_found
    test al, al
    jnz main_ext_lp
    dec rsi
main_ext_found:
    mov byte ptr [rsi-1], '.'
    mov byte ptr [rsi],   'e'
    mov byte ptr [rsi+1], 'x'
    mov byte ptr [rsi+2], 'e'
    mov byte ptr [rsi+3], 0

    ; Write PE
    call write_pe_file

    ; Success
    lea rcx, szDone
    call print

    xor ecx, ecx
    call ExitProcess

main_usage:
    lea rcx, szUsage
    call print
    mov ecx, 1
    call ExitProcess

main_file_err:
    call fatal

end
