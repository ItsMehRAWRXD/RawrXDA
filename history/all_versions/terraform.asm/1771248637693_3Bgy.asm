; RXUC-WorkingCompiler v10.1 — FIXED OPERANDS
; Pure x64 MASM — Compiles .tf to .exe
; Assemble: & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" /c terraform.asm
; Link: & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /ENTRY:_start /SUBSYSTEM:CONSOLE terraform.obj kernel32.lib

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

pe_sig      db 'PE',0,0

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
    push rdi
    mov rdi, rcx
    mov rcx, rdx
    xor al, al
    rep stosb
    pop rdi
    ret
memzero endp

; ================================================
; Utility: String length
; RCX = string ptr -> RAX = length
; ================================================
strlen proc
    xor rax, rax
L1: cmp byte ptr [rcx+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

; ================================================
; Print string to console
; RCX = string ptr
; ================================================
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

; ================================================
; Fatal error: print and exit
; ================================================
fatal proc
    lea rcx, szError
    call print
    mov ecx, 1
    call ExitProcess
fatal endp

; ================================================
; Character classification
; ================================================
is_space proc
    cmp al, ' '
    jbe yes
    ret
yes: mov al, 1
    ret
is_space endp

is_digit proc
    cmp al, '0'
    jb no
    cmp al, '9'
    ja no
    mov al, 1
    ret
no: xor al, al
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

; ================================================
; Lexer helpers
; ================================================
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

; ================================================
; String compare: RSI vs RDI
; Returns: EAX = 0 if equal
; ================================================
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

; ================================================
; Add token: CL=type, EDX=value, R8=name ptr
; ================================================
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

; ================================================
; Keyword check: RCX = identifier string
; Returns: EAX = token type or 0
; ================================================
check_keyword proc
    push rsi
    push rdi
    mov rsi, rcx
    
    lea rdi, kw_fn
    call strcmp
    test eax, eax
    jz is_fn
    lea rdi, kw_let
    call strcmp
    test eax, eax
    jz is_let
    lea rdi, kw_if
    call strcmp
    test eax, eax
    jz is_if
    lea rdi, kw_else
    call strcmp
    test eax, eax
    jz is_else
    lea rdi, kw_while
    call strcmp
    test eax, eax
    jz is_while
    lea rdi, kw_return
    call strcmp
    test eax, eax
    jz is_ret
    
    xor eax, eax
    jmp kw_done
    
is_fn:   mov eax, TOK_FN
    jmp kw_done
is_let:  mov eax, TOK_LET
    jmp kw_done
is_if:   mov eax, TOK_IF
    jmp kw_done
is_else: mov eax, TOK_ELSE
    jmp kw_done
is_while: mov eax, TOK_WHILE
    jmp kw_done
is_ret:  mov eax, TOK_RETURN
    
kw_done:
    pop rdi
    pop rsi
    ret
check_keyword endp

; ================================================
; Lexer: tokenize source buffer
; ================================================
lexer proc
    mov nTokens, 0
    mov iLine, 1
    
L3: call skip_whitespace
    call peek_char
    cmp eax, -1
    je eof_tok
    
    cmp al, '0'
    jb not_num
    cmp al, '9'
    jbe number_tok
    
not_num:
    cmp al, 'a'
    jb not_id
    cmp al, 'z'
    jbe ident_tok
    cmp al, 'A'
    jb not_id
    cmp al, 'Z'
    ja not_id
    
ident_tok:
    mov rax, iSource
    mov tempSpace, rax        ; Save start position
    mov r13, rax
    
L4: call peek_char
    call is_alpha
    test al, al
    jnz is_id
    call is_digit
    test al, al
    jz id_done
is_id:
    call next_char
    jmp L4
    
id_done:
    mov rax, iSource
    mov rcx, pSource
    mov byte ptr [rcx+rax], 0
    
    mov rax, tempSpace
    mov rcx, pSource
    add rcx, rax              ; RCX = pointer to string
    call check_keyword
    test eax, eax
    jnz kw_tok
    
    mov ecx, TOK_ID
    xor edx, edx
    mov rax, tempSpace
    mov r8, pSource
    add r8, rax
    call add_token
    jmp L3
    
kw_tok:
    mov ecx, eax
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L3

number_tok:
    xor eax, eax
    
L5: call peek_char
    cmp al, '0'
    jb num_done
    cmp al, '9'
    ja num_done
    sub al, '0'
    movzx edx, al
    imul eax, 10
    add eax, edx
    call next_char
    jmp L5
    
num_done:
    mov ecx, TOK_NUM
    mov edx, eax
    xor r8, r8
    call add_token
    jmp L3

not_id:
    mov ecx, eax
    call next_char
    
    cmp ecx, '('
    je lp_tok
    cmp ecx, ')'
    je rp_tok
    cmp ecx, '{'
    je lb_tok
    cmp ecx, '}'
    je rb_tok
    cmp ecx, '+'
    je plus_tok
    cmp ecx, '-'
    je minus_tok
    cmp ecx, '*'
    je mul_tok
    cmp ecx, '/'
    je div_tok
    cmp ecx, '='
    je assign_tok
    cmp ecx, ';'
    je semi_tok
    cmp ecx, ','
    je comma_tok
    
    jmp L3
    
lp_tok:  mov ecx, TOK_LPAREN
    jmp add_simple
rp_tok:  mov ecx, TOK_RPAREN
    jmp add_simple
lb_tok:  mov ecx, TOK_LBRACE
    jmp add_simple
rb_tok:  mov ecx, TOK_RBRACE
    jmp add_simple
plus_tok: mov ecx, TOK_PLUS
    jmp add_simple
minus_tok: mov ecx, TOK_MINUS
    jmp add_simple
mul_tok: mov ecx, TOK_MUL
    jmp add_simple
div_tok: mov ecx, TOK_DIV
    jmp add_simple
assign_tok: mov ecx, TOK_ASSIGN
    jmp add_simple
semi_tok: mov ecx, TOK_SEMI
    jmp add_simple
comma_tok: mov ecx, TOK_COMMA
    
add_simple:
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L3
    
eof_tok:
    mov ecx, TOK_EOF
    xor edx, edx
    xor r8, r8
    call add_token
    ret
lexer endp

; ================================================
; Code emitters
; ================================================
emit_byte proc
    mov rax, pOutput
    mov r11d, cbOutput
    mov [rax+r11], cl
    inc cbOutput
    ret
emit_byte endp

emit_dword proc
    mov rax, pOutput
    mov r11d, cbOutput
    mov [rax+r11], ecx
    add cbOutput, 4
    ret
emit_dword endp

emit_prolog proc
    ; push rbp
    mov cl, 55h
    call emit_byte
    ; mov rbp, rsp
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 0E5h
    call emit_byte
    ; sub rsp, 20h
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0ECh
    call emit_byte
    mov cl, 20h
    call emit_byte
    ret
emit_prolog endp

emit_epilog proc
    ; add rsp, 20h
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0C4h
    call emit_byte
    mov cl, 20h
    call emit_byte
    ; pop rbp
    mov cl, 5Dh
    call emit_byte
    ; ret
    mov cl, 0C3h
    call emit_byte
    ret
emit_epilog endp

emit_push_imm proc
    cmp rcx, 127
    jg big_imm
    cmp rcx, -128
    jl big_imm
    
    ; push imm8
    mov al, 6Ah
    mov r12b, cl
    mov cl, al
    call emit_byte
    mov cl, r12b
    call emit_byte
    ret
    
big_imm:
    ; push imm32
    mov al, 68h
    mov r12d, ecx
    mov cl, al
    call emit_byte
    mov ecx, r12d
    call emit_dword
    ret
emit_push_imm endp

; ================================================
; Parser: token access
; ================================================
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

; ================================================
; Parser: expressions
; ================================================
parse_expr proc
    push rbx
    call parse_term
    
L6: call cur_tok
    cmp eax, TOK_PLUS
    je do_add
    cmp eax, TOK_MINUS
    je do_sub
    pop rbx
    ret
    
do_add:
    call advance
    push rax
    call parse_term
    pop rdx
    ; add rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    jmp L6
    
do_sub:
    call advance
    push rax
    call parse_term
    pop rdx
    xchg rax, rdx
    ; sub rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    jmp L6
parse_expr endp

parse_term proc
    push rbx
    call parse_factor
    
L7: call cur_tok
    cmp eax, TOK_MUL
    je do_mul
    cmp eax, TOK_DIV
    je do_div
    pop rbx
    ret
    
do_mul:
    call advance
    push rax
    call parse_factor
    pop rdx
    ; imul rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0E2h
    call emit_byte
    jmp L7
    
do_div:
    call advance
    push rax
    call parse_factor
    mov rcx, rax
    pop rax
    push rcx
    ; xor rdx, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 31h
    call emit_byte
    mov cl, 0D2h
    call emit_byte
    ; div rcx
    pop rcx
    push rax
    mov rax, rcx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0F1h
    call emit_byte
    pop rax
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
    ; neg rax
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret
parse_factor endp

; ================================================
; Parser: statements
; ================================================
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
    mov ecx, TOK_IF
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    
    ; cmp rax, 0
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    
    ; jz <else_label> (6-byte near jump)
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r12d, cbOutput          ; Save fixup position
    xor ecx, ecx
    call emit_dword             ; Placeholder
    
    call parse_stmt
    
    ; jmp <end_label> (5-byte near jump)
    mov cl, 0E9h
    call emit_byte
    mov r13d, cbOutput          ; Save fixup position
    xor ecx, ecx
    call emit_dword             ; Placeholder
    
    ; Fixup jz target
    mov eax, cbOutput
    sub eax, r12d
    sub eax, 4
    mov r10, pOutput
    mov r11d, r12d
    mov [r10+r11], eax
    
    ; Check for else
    call cur_tok
    cmp eax, TOK_ELSE
    jne no_else
    call advance
    call parse_stmt
    
no_else:
    ; Fixup jmp target
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    mov r11d, r13d
    mov [r10+r11], eax
    ret
    
ps_while:
    mov r12d, cbOutput          ; Loop start
    
    mov ecx, TOK_WHILE
    call expect
    mov ecx, TOK_LPAREN
    call expect
    call parse_expr
    mov ecx, TOK_RPAREN
    call expect
    
    ; cmp rax, 0
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    mov cl, 0
    call emit_byte
    
    ; jz <end> (6-byte near jump)
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r13d, cbOutput          ; Save fixup position
    xor ecx, ecx
    call emit_dword             ; Placeholder
    
    call parse_stmt
    
    ; jmp <loop_start> (5-byte near jump)
    mov cl, 0E9h
    call emit_byte
    mov eax, r12d
    sub eax, cbOutput
    sub eax, 4
    mov ecx, eax
    call emit_dword
    
    ; Fixup jz target
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, pOutput
    mov r11d, r13d
    mov [r10+r11], eax
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

; ================================================
; Parser: function definition
; ================================================
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

; ================================================
; PE Writer: generate minimal x64 PE executable
; ================================================
write_pe_file proc
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    ; Create output file
    lea rcx, filename
    mov edx, 40000000h         ; GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+48], 2  ; CREATE_ALWAYS
    mov qword ptr [rsp+56], 80h ; FILE_ATTRIBUTE_NORMAL
    xor eax, eax
    mov qword ptr [rsp+64], rax
    call CreateFileA
    cmp rax, -1
    je pe_fail
    mov hOutput, rax
    
    ; Align code size to 512
    mov eax, cbOutput
    add eax, 511
    and eax, -512
    mov [rbp-8], eax            ; alignedCodeSize
    
    ; --- DOS Header (64 bytes) ---
    sub rsp, 88
    mov rcx, rsp
    mov edx, 64
    call memzero
    
    mov word ptr [rsp], 5A4Dh   ; e_magic "MZ"
    mov dword ptr [rsp+60], 64  ; e_lfanew -> PE header at offset 64
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 64
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    add rsp, 88
    
    ; --- PE Signature (4 bytes) ---
    mov rcx, hOutput
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; --- COFF Header + Optional Header + Section Header ---
    sub rsp, 264
    mov rcx, rsp
    mov edx, 264
    call memzero
    
    ; COFF Header (20 bytes)
    mov word ptr [rsp], 8664h      ; Machine: AMD64
    mov word ptr [rsp+2], 1        ; NumberOfSections: 1
    mov word ptr [rsp+16], 240     ; SizeOfOptionalHeader
    mov word ptr [rsp+18], 22h     ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 20
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Optional Header PE32+ (240 bytes) - write at [rsp]
    mov rcx, rsp
    mov edx, 240
    call memzero
    
    mov word ptr [rsp], 20Bh       ; Magic: PE32+
    mov byte ptr [rsp+2], 1        ; MajorLinkerVersion
    mov eax, [rbp-8]               ; SizeOfCode = alignedCodeSize
    mov dword ptr [rsp+4], eax
    mov dword ptr [rsp+16], 1000h  ; AddressOfEntryPoint
    mov dword ptr [rsp+20], 1000h  ; BaseOfCode
    mov rax, 140000000h            ; ImageBase
    mov qword ptr [rsp+24], rax
    mov dword ptr [rsp+32], 1000h  ; SectionAlignment
    mov dword ptr [rsp+36], 200h   ; FileAlignment
    mov word ptr [rsp+40], 6       ; MajorOperatingSystemVersion
    mov word ptr [rsp+44], 6       ; MajorSubsystemVersion
    mov eax, [rbp-8]
    add eax, 2000h                 ; SizeOfImage = headers + code
    mov dword ptr [rsp+56], eax
    mov dword ptr [rsp+60], 200h   ; SizeOfHeaders
    mov word ptr [rsp+68], 3       ; Subsystem: CONSOLE
    mov word ptr [rsp+70], 8160h   ; DllCharacteristics: NX_COMPAT | DYNAMIC_BASE | TERMINAL_SERVER_AWARE
    mov rax, 100000h               ; SizeOfStackReserve
    mov qword ptr [rsp+72], rax
    mov rax, 1000h                 ; SizeOfStackCommit
    mov qword ptr [rsp+80], rax
    mov rax, 100000h               ; SizeOfHeapReserve
    mov qword ptr [rsp+88], rax
    mov rax, 1000h                 ; SizeOfHeapCommit
    mov qword ptr [rsp+96], rax
    mov dword ptr [rsp+108], 16    ; NumberOfRvaAndSizes
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 240
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Section Header: .text (40 bytes)
    mov rcx, rsp
    mov edx, 40
    call memzero
    
    mov dword ptr [rsp], 'xet.'    ; ".tex"
    mov byte ptr [rsp+4], 't'      ; "t"
    mov eax, cbOutput              ; VirtualSize = actual code size
    mov dword ptr [rsp+8], eax
    mov dword ptr [rsp+12], 1000h  ; VirtualAddress
    mov eax, [rbp-8]               ; SizeOfRawData = aligned
    mov dword ptr [rsp+16], eax
    mov dword ptr [rsp+20], 200h   ; PointerToRawData (after headers)
    mov dword ptr [rsp+36], 60000020h ; CODE | EXECUTE | READ
    
    mov rcx, hOutput
    mov rdx, rsp
    mov r8d, 40
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    add rsp, 264
    
    ; Seek to 0x200 (raw data offset)
    mov rcx, hOutput
    mov edx, 200h
    xor r8d, r8d               ; FILE_BEGIN
    xor r9d, r9d
    call SetFilePointer
    
    ; Write code
    mov rcx, hOutput
    mov rdx, pOutput
    mov r8d, cbOutput
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Close
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

; ================================================
; Entry Point
; ================================================
_start:
    sub rsp, 56
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT
    call GetStdHandle
    mov hStdOut, rax
    
    ; Print banner
    lea rcx, szBanner
    call print
    
    ; Initialize buffer pointers
    lea rax, sourceBuf
    mov pSource, rax
    lea rax, tokens
    mov pTokens, rax
    lea rax, outputBuf
    mov pOutput, rax
    
    ; Parse command line
    call GetCommandLineA
    mov rsi, rax
    
    ; Skip executable name
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
    mov edx, 80000000h         ; GENERIC_READ
    mov r8d, 1                 ; FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+32], 3  ; OPEN_EXISTING
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
    
    ; Close source
    mov rcx, hSource
    call CloseHandle
    
    ; Reset state
    mov iSource, 0
    mov cbOutput, 0
    mov iToken, 0
    
    ; Lex
    call lexer
    
    ; Parse
    mov iToken, 0
    call parse_function
    
    ; Generate output filename (.tf -> .exe)
    lea rsi, filename
L10:
    lodsb
    cmp al, '.'
    je ext_f
    test al, al
    jnz L10
    dec rsi
ext_f:
    dec rsi
    mov byte ptr [rsi], '.'
    mov byte ptr [rsi+1], 'e'
    mov byte ptr [rsi+2], 'x'
    mov byte ptr [rsi+3], 'e'
    mov byte ptr [rsi+4], 0
    
    ; Write PE
    call write_pe_file
    test eax, eax
    jnz file_err
    
    ; Success
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
