; ================================================
; RXUC-TerraForm v5.0 - Complete Compiler
; Full Parser + x64 CodeGen + PE Writer
; Zero Dependencies, Drag-and-Drop Folder Build
; ================================================

option casemap:none

; External imports
externdef GetCommandLineA:qword
externdef CreateFileA:qword
externdef ReadFile:qword
externdef WriteFile:qword
externdef CloseHandle:qword
externdef GetFileSizeEx:qword
externdef VirtualAlloc:qword
externdef VirtualFree:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword
externdef ExitProcess:qword
externdef FindFirstFileA:qword
externdef FindNextFileA:qword
externdef FindClose:qword
externdef SetFilePointer:qword

; Constants
STD_OUTPUT_HANDLE equ -11
MAX_SRC equ 65536
MAX_TOK equ 4096
MAX_OUT equ 131072
INVALID_HANDLE_VALUE equ -1

; Token types
T_EOF equ 0
T_NL equ 1
T_NUM equ 2
T_ID equ 3
T_STR equ 4
T_LP equ 5
T_RP equ 6
T_LB equ 7
T_RB equ 8
T_PLUS equ 9
T_MINUS equ 10
T_MUL equ 11
T_DIV equ 12
T_ASSIGN equ 13
T_COLON equ 14
T_COMMA equ 15
T_ARROW equ 16
T_EQ equ 17
T_LT equ 18
T_GT equ 19

; Keyword IDs (add 100 to token type)
K_FN equ 1
K_LET equ 2
K_IF equ 3
K_ELSE equ 4
K_WHILE equ 5
K_RETURN equ 6
K_STRUCT equ 7

; PE constants
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC equ 20Bh
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3

; Data section
.data
align 8

sz_usage db "Usage: terraform <source.tf>",10,0
sz_err_file db "[!] Cannot open file",10,0
sz_err_syntax db "[!] Syntax error",10,0
sz_ok db "[+] Compiled: out.exe",10,0
sz_scan db "[*] Compiling...",10,0

; PE Header templates
pe_dos_header:
dw 5A4Dh
dw 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dd 80

pe_sig db 'PE',0,0

pe_file_header:
dw IMAGE_FILE_MACHINE_AMD64
dw 2                          ; Sections
dd 0                          ; Timestamp
dd 0                          ; Symbol table
dd 0                          ; Symbol count
dw 240                        ; Optional header size
dw 22h                        ; Characteristics

pe_optional_header:
dw IMAGE_NT_OPTIONAL_HDR64_MAGIC
db 0,1                        ; Linker version
dd 0,0,0                      ; Code/Data/BSS sizes
dd 1000h                      ; Entry point
dd 1000h                      ; Code base
dq 140000000h                 ; Image base
dd 1000h                      ; Section alignment
dd 200h                       ; File alignment
dw 6,0                        ; OS version
dw 0,0                        ; Image version
dw 6,0                        ; Subsystem version
dd 0                          ; Win32 version
dd 3000h                      ; Image size
dd 400h                       ; Headers size
dd 0                          ; Checksum
dw IMAGE_SUBSYSTEM_WINDOWS_CUI
dw 0                          ; DLL characteristics
dq 100000h                    ; Stack reserve
dq 1000h                      ; Stack commit
dq 100000h                    ; Heap reserve
dq 1000h                      ; Heap commit
dd 0                          ; Loader flags
dd 16                         ; Data directories

dq 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  ; Data directory entries

sect_text_name db '.text',0,0,0
sect_text_misc dd 0, 1000h, 0, 200h, 0, 0
dw 0,0
dd 60000020h

sect_data_name db '.data',0,0,0
sect_data_misc dd 0, 2000h, 0, 400h, 0, 0
dw 0,0
dd 0C0000040h

; Keyword table
kw_fn db 'fn',0
kw_let db 'let',0
kw_if db 'if',0
kw_else db 'else',0
kw_while db 'while',0
kw_return db 'return',0
kw_struct db 'struct',0

; BSS section (uninitialized)
.data
align 8

hInput dq ?
hOutput dq ?
lpSource dq ?
cbSource dq ?
iSource dq ?
iLine dd ?

lpTokens dq ?
nTokens dd ?
iToken dd ?

lpOutput dq ?
cbOutput dq ?

temp_path db 260 dup(?)
source_buffer db MAX_SRC dup(?)
token_buffer db MAX_TOK * 32 dup(?)
output_buffer db MAX_OUT dup(?)

; Symbol table
sym_count dd ?
sym_table db 256 * 48 dup(?)

; Parser state
cur_val dq ?
stack_offset dd ?
func_count dd ?
literal_count dd ?

; Error handling
syntax_err:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    ret

; Code section
.code
align 16

; Utility functions
strlen proc
    xor rax, rax
    mov rsi, rcx
L1: cmp byte ptr [rsi+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

print proc
    push rsi
    push rdi
    mov rsi, rcx
    call strlen
    mov rdx, rsi
    mov r8, rax
    mov ecx, STD_OUTPUT_HANDLE
    call qword ptr [GetStdHandle]
    mov rcx, rax
    mov r9, 0
    push 0
    sub rsp, 32
    call qword ptr [WriteConsoleA]
    add rsp, 40
    pop rdi
    pop rsi
    ret
print endp

memcpy proc
    mov rsi, rdx
    mov rdi, rcx
    mov rcx, r8
    rep movsb
    ret
memcpy endp

atoi proc
    xor rax, rax
    mov rsi, rcx
L2: movzx rdx, byte ptr [rsi]
    test dl, dl
    jz D2
    cmp dl, '0'
    jb D2
    cmp dl, '9'
    ja D2
    sub dl, '0'
    imul rax, 10
    add rax, rdx
    inc rsi
    jmp L2
D2: ret
atoi endp

; Lexer
peek proc
    mov rax, iSource
    cmp rax, cbSource
    jae Eof
    mov rcx, lpSource
    movzx eax, byte ptr [rcx+rax]
    ret
Eof: mov eax, -1
    ret
peek endp

next proc
    inc iSource
    ret
next endp

skip_ws proc
L3: call peek
    cmp eax, ' '
    je W1
    cmp eax, 9
    je W1
    cmp eax, 13
    je W1
    ret
W1: call next
    jmp L3
skip_ws endp

add_token proc
    ; rcx=type, rdx=val, r8=str
    mov r9d, nTokens
    shl r9d, 5
    mov r10, lpTokens
    add r10, r9
    mov [r10], ecx
    mov dword ptr [r10+4], iLine
    mov [r10+8], rdx
    test r8, r8
    jz N1
    mov rdi, r10
    add rdi, 16
    mov rsi, r8
    mov rcx, 16
    rep movsb
N1: inc nTokens
    ret
add_token endp

check_kw proc
    ; rcx=str, returns eax=K_xxx or 0
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    
    ; Check 'fn'
    lea rdi, kw_fn
    mov al, [rsi]
    cmp al, 'f'
    jne C1
    mov al, [rsi+1]
    cmp al, 'n'
    jne C1
    cmp byte ptr [rsi+2], 0
    jne C1
    mov eax, K_FN
    jmp KwD
C1:
    ; Check 'let'
    mov al, [rsi]
    cmp al, 'l'
    jne C2
    mov al, [rsi+1]
    cmp al, 'e'
    jne C2
    mov al, [rsi+2]
    cmp al, 't'
    jne C2
    cmp byte ptr [rsi+3], 0
    jne C2
    mov eax, K_LET
    jmp KwD
C2:
    ; Check 'if'
    mov al, [rsi]
    cmp al, 'i'
    jne C3
    mov al, [rsi+1]
    cmp al, 'f'
    jne C3
    cmp byte ptr [rsi+2], 0
    jne C3
    mov eax, K_IF
    jmp KwD
C3:
    ; Check 'else'
    mov al, [rsi]
    cmp al, 'e'
    jne C4
    mov al, [rsi+1]
    cmp al, 'l'
    jne C4
    mov al, [rsi+2]
    cmp al, 's'
    jne C4
    mov al, [rsi+3]
    cmp al, 'e'
    jne C4
    cmp byte ptr [rsi+4], 0
    jne C4
    mov eax, K_ELSE
    jmp KwD
C4:
    ; Check 'while'
    mov al, [rsi]
    cmp al, 'w'
    jne C5
    mov al, [rsi+1]
    cmp al, 'h'
    jne C5
    mov al, [rsi+2]
    cmp al, 'i'
    jne C5
    mov al, [rsi+3]
    cmp al, 'l'
    jne C5
    mov al, [rsi+4]
    cmp al, 'e'
    jne C5
    cmp byte ptr [rsi+5], 0
    jne C5
    mov eax, K_WHILE
    jmp KwD
C5:
    ; Check 'return'
    mov al, [rsi]
    cmp al, 'r'
    jne C6
    mov al, [rsi+1]
    cmp al, 'e'
    jne C6
    mov al, [rsi+2]
    cmp al, 't'
    jne C6
    mov al, [rsi+3]
    cmp al, 'u'
    jne C6
    mov al, [rsi+4]
    cmp al, 'r'
    jne C6
    mov al, [rsi+5]
    cmp al, 'n'
    jne C6
    cmp byte ptr [rsi+6], 0
    jne C6
    mov eax, K_RETURN
    jmp KwD
C6:
    xor eax, eax
KwD:
    pop rdi
    pop rsi
    pop rbx
    ret
check_kw endp

lex proc
    mov nTokens, 0
    mov iLine, 1
    
L4: call skip_ws
    call peek
    cmp eax, -1
    je EofT
    cmp eax, 10
    je NewL
    cmp eax, '"'
    je StrT
    cmp eax, 'A'
    jb N1a
    cmp eax, 'Z'
    jbe IdT
N1a:
    cmp eax, 'a'
    jb N2a
    cmp eax, 'z'
    jbe IdT
N2a:
    cmp eax, '0'
    jb SymT
    cmp eax, '9'
    jbe NumT
    jmp SymT

NewL:
    call next
    inc iLine
    mov ecx, T_NL
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L4

StrT:
    call next
    mov r12, iSource
L5: call peek
    cmp eax, '"'
    je StrE
    cmp eax, -1
    je StrE
    cmp eax, '\'
    jne S1
    call next
S1: call next
    jmp L5
StrE:
    mov ecx, T_STR
    xor edx, edx
    mov r8, lpSource
    add r8, r12
    call add_token
    call next
    jmp L4

IdT:
    sub rsp, 48
    lea rdi, [rsp+8]
L6: call peek
    cmp eax, 'A'
    jb IdC
    cmp eax, 'Z'
    jbe IdA
IdC:
    cmp eax, 'a'
    jb IdC2
    cmp eax, 'z'
    jbe IdA
IdC2:
    cmp eax, '0'
    jb IdE
    cmp eax, '9'
    jbe IdA
    cmp eax, '_'
    jne IdE
IdA:
    stosb
    call next
    jmp L6
IdE:
    mov byte ptr [rdi], 0
    lea rcx, [rsp+8]
    call check_kw
    test eax, eax
    jnz IsKw
    mov ecx, T_ID
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
    jmp IdD
IsKw:
    add eax, 100
    mov ecx, eax
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
IdD:
    add rsp, 48
    jmp L4

NumT:
    mov r12, iSource
L7: call peek
    cmp eax, '0'
    jb NumE
    cmp eax, '9'
    ja NumE
    call next
    jmp L7
NumE:
    mov byte ptr [temp_path], 0
    lea rdi, temp_path
    mov rsi, lpSource
    add rsi, r12
    mov rcx, iSource
    sub rcx, r12
    rep movsb
    mov byte ptr [rdi], 0
    lea rcx, temp_path
    call atoi
    mov ecx, T_NUM
    mov rdx, rax
    xor r8, r8
    call add_token
    jmp L4

SymT:
    cmp eax, '('
    je Tlp
    cmp eax, ')'
    je Trp
    cmp eax, '{'
    je Tlb
    cmp eax, '}'
    je Trb
    cmp eax, '+'
    je Tpl
    cmp eax, '-'
    je Tmi
    cmp eax, '*'
    je Tmu
    cmp eax, '/'
    je Tdi
    cmp eax, '='
    je Tas
    cmp eax, ':'
    je Tco
    cmp eax, ','
    je Tcm
    cmp eax, '<'
    je Tlt
    cmp eax, '>'
    je Tgt
    call next
    jmp L4

Tlp: mov ecx, T_LP
    jmp Tadd
Trp: mov ecx, T_RP
    jmp Tadd
Tlb: mov ecx, T_LB
    jmp Tadd
Trb: mov ecx, T_RB
    jmp Tadd
Tpl: mov ecx, T_PLUS
    jmp Tadd
Tmu: mov ecx, T_MUL
    jmp Tadd
Tco: mov ecx, T_COLON
    jmp Tadd
Tcm: mov ecx, T_COMMA
    jmp Tadd
Tlt: mov ecx, T_LT
    jmp Tadd
Tgt: mov ecx, T_GT
    jmp Tadd

Tmi:
    call next
    call peek
    cmp eax, '>'
    je Tar
    mov ecx, T_MINUS
    jmp Taddn
Tar:
    call next
    mov ecx, T_ARROW
    jmp Tadd

Tdi:
    call next
    call peek
    cmp eax, '/'
    je Cmt
    mov ecx, T_DIV
    jmp Taddn
Cmt:
    call next
CmtL:
    call peek
    cmp eax, 10
    je L4
    cmp eax, -1
    je L4
    call next
    jmp CmtL

Tas:
    call next
    call peek
    cmp eax, '='
    je Teq
    mov ecx, T_ASSIGN
    jmp Taddn
Teq:
    call next
    mov ecx, T_EQ
    jmp Tadd

Taddn:
    call next
Tadd:
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L4

EofT:
    mov ecx, T_EOF
    xor edx, edx
    xor r8, r8
    call add_token
    ret
lex endp

; Parser/Emitter
emit_byte proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], cl
    inc cbOutput
    ret
emit_byte endp

emit_dword proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], ecx
    add cbOutput, 4
    ret
emit_dword endp

emit_qword proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], rcx
    add cbOutput, 8
    ret
emit_qword endp

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
    ; sub rsp, 32
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
    ; add rsp, 32
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
    ; push imm32 (sign extended)
    mov al, 68h
    mov r12d, ecx
    mov cl, al
    call emit_byte
    mov ecx, r12d
    call emit_dword
    ret
emit_push_imm endp

cur_tok proc
    mov eax, iToken
    shl eax, 5
    mov r10, lpTokens
    mov eax, [r10+rax]
    ret
cur_tok endp

advance proc
    inc iToken
    ret
advance endp

expect proc
    call cur_tok
    cmp eax, ecx
    je ok
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
ok:
    call advance
    ret
expect endp

match proc
    call cur_tok
    cmp eax, ecx
    ret
match endp

sym_find proc
    mov rsi, rcx
    xor ecx, ecx
    mov r8d, sym_count
L0: cmp ecx, r8d
    jae not_found
    mov eax, ecx
    imul eax, 48
    lea rdx, sym_table
    add rdx, rax
    lea rdi, [rdx+8]
    mov rsi, [rsp+8]
    push rcx
    mov rcx, 32
    repe cmpsb
    pop rcx
    je found
    inc ecx
    jmp L0
not_found:
    xor eax, eax
    xor edx, edx
    ret
found:
    mov eax, ecx
    ret
sym_find endp

sym_add proc
    mov r11d, sym_count
    cmp r11d, 256
    jae overflow
    mov eax, r11d
    imul eax, 48
    lea rdx, sym_table
    add rdx, rax
    mov [rdx], r8d
    mov [rdx+4], r9d
    lea rdi, [rdx+8]
    mov rsi, rcx
    mov rcx, 32
    rep movsb
    inc sym_count
    mov eax, r11d
    ret
overflow:
    xor eax, eax
    ret
sym_add endp

; Expression Parser (Recursive Descent)
parse_expr proc
    call parse_addition
    ret
parse_expr endp

parse_addition proc
    call parse_multiplication
L1: call match
    cmp ecx, T_PLUS
    je do_add
    cmp ecx, T_MINUS
    je do_sub
    ret
do_add:
    call advance
    push rax
    call parse_multiplication
    pop rdx
    ; add rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0C2h
    call emit_byte
    jmp L1
do_sub:
    call advance
    push rax
    call parse_multiplication
    pop rdx
    ; sub rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0C2h
    call emit_byte
    jmp L1
parse_addition endp

parse_multiplication proc
    call parse_unary
L1: call match
    cmp ecx, T_MUL
    je do_mul
    cmp ecx, T_DIV
    je do_div
    ret
do_mul:
    call advance
    push rax
    call parse_unary
    pop rdx
    ; imul rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0EAh
    call emit_byte
    jmp L1
do_div:
    call advance
    push rax
    call parse_unary
    pop rdx
    ; xchg rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 87h
    call emit_byte
    mov cl, 0C2h
    call emit_byte
    ; xor rdx, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 31h
    call emit_byte
    mov cl, 0D2h
    call emit_byte
    ; div rdx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0F2h
    call emit_byte
    jmp L1
parse_multiplication endp

parse_unary proc
    call match
    cmp ecx, T_MINUS
    je do_neg
    jmp parse_primary
do_neg:
    call advance
    call parse_unary
    ; neg rax
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret
parse_unary endp

parse_primary proc
    call match
    cmp ecx, T_NUM
    je number
    cmp ecx, T_ID
    je identifier
    cmp ecx, T_LP
    je grouping
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
number:
    mov rcx, cur_val
    call emit_push_imm
    call advance
    ret
identifier:
    ; Variable load (mov rax, [rbp-offset])
    mov rcx, cur_val
    call sym_find
    test rdx, rdx
    jz undefined_var
    mov eax, [rdx+4]
    neg eax
    ; mov rax, [rbp+disp]
    mov cl, 48h
    call emit_byte
    mov cl, 8Bh
    call emit_byte
    mov cl, 85h
    call emit_byte
    mov ecx, eax
    call emit_dword
    call advance
    ret
grouping:
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
    call expect
    ret
undefined_var:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    ret
parse_primary endp

; Statement Parser
parse_stmt proc
    call match
    cmp ecx, 100 + K_LET
    je parse_let
    cmp ecx, 100 + K_IF
    je parse_if
    cmp ecx, 100 + K_WHILE
    je parse_while
    cmp ecx, 100 + K_RETURN
    je parse_return
    cmp ecx, T_LB
    je parse_block
    jmp parse_expr_stmt
parse_stmt endp

parse_let proc
    mov ecx, 100 + K_LET
    call expect
    call match
    cmp ecx, T_ID
    jne syntax_err
    mov r12, cur_val
    call advance
    call match
    cmp ecx, T_ASSIGN
    jne no_init
    call advance
    call parse_expr
    mov ecx, r12d
    mov edx, 1
    mov r8, rax
    mov r9d, stack_offset
    call sym_add
    ; mov [rbp-offset], rax
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 85h
    call emit_byte
    mov ecx, 0FFFFFFF0h
    call emit_dword
    add stack_offset, 8
no_init:
    mov ecx, T_NL
    call expect
    ret
    ret
parse_let endp

parse_if proc
    mov ecx, 100 + K_IF
    call expect
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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
    ; jz else_label
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r12d, dword ptr cbOutput
    xor ecx, ecx
    call emit_dword
    call parse_stmt
    ; jmp end_label
    mov cl, 0E9h
    call emit_byte
    mov r13d, dword ptr cbOutput
    xor ecx, ecx
    call emit_dword
    ; patch else
    mov eax, dword ptr cbOutput
    sub eax, r12d
    sub eax, 4
    mov r10, lpOutput
    add r10d, r12d
    mov [r10], eax
    call match
    cmp ecx, 100 + K_ELSE
    jne no_else
    call advance
    call parse_stmt
no_else:
    ; patch end
    mov eax, dword ptr cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, lpOutput
    add r10d, r13d
    mov [r10], eax
    ret
parse_if endp

parse_while proc
    mov r12d, dword ptr cbOutput
    mov ecx, 100 + K_WHILE
    call expect
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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
    ; jz end
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r13d, dword ptr cbOutput
    xor ecx, ecx
    call emit_dword
    call parse_stmt
    ; jmp start
    mov cl, 0E9h
    call emit_byte
    mov eax, r12d
    sub eax, dword ptr cbOutput
    sub eax, 4
    mov ecx, eax
    call emit_dword
    ; patch exit
    mov eax, dword ptr cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, lpOutput
    add r10d, r13d
    mov [r10], eax
    ret
parse_while endp

parse_return proc
    mov ecx, 100 + K_RETURN
    call expect
    call match
    cmp ecx, T_NL
    je ret_void
    call parse_expr
ret_void:
    mov ecx, T_NL
    call expect
    call emit_epilog
    ret
parse_return endp

parse_expr_stmt proc
    call parse_expr
    mov ecx, T_NL
    call expect
    ret
parse_expr_stmt endp

parse_block proc
    mov ecx, T_LB
    call expect
L1: call match
    cmp ecx, T_RB
    je done
    cmp ecx, T_EOF
    je done
    call parse_stmt
    jmp L1
done:
    mov ecx, T_RB
    call expect
    ret
parse_block endp

; Function Parser
parse_function proc
    mov ecx, 100 + K_FN
    call expect
    call match
    cmp ecx, T_ID
    jne syntax_err
    mov r12, cur_val
    call advance
    mov ecx, T_LP
    call expect
    mov ecx, T_RP
    call expect
    call parse_block
    ret
parse_function endp

; PE Writer
write_pe_file proc
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov [rbp+16], rcx
    
    ; Create file
    mov rcx, [rbp+16]
    xor edx, edx
    mov r8d, 2
    xor r9d, r9d
    mov dword ptr [rsp+32], 2
    mov dword ptr [rsp+40], 80h
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je file_err
    mov hOutput, rax
    
    ; Calculate sizes
    mov eax, dword ptr cbOutput
    add eax, 511
    and eax, -512
    mov [rbp+32], eax   ; raw_size
    
    mov eax, dword ptr cbOutput
    add eax, 4095
    and eax, -4096
    mov [rbp+36], eax   ; virt_size
    
    ; Write DOS header
    mov rcx, hOutput
    lea rdx, pe_dos_header
    mov r8d, 64
    xor r9d, r9d
    xor rax, rax
    push rax
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Write PE sig
    mov rcx, hOutput
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    xor rax, rax
    push rax
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Write file header
    mov rcx, hOutput
    lea rdx, pe_file_header
    mov r8d, 20
    xor r9d, r9d
    xor rax, rax
    push rax
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Update optional header
    mov rax, pe_optional_header
    mov [rax+4], eax
    mov eax, [rbp+36]
    mov [rax+56], eax
    
    ; Write optional header
    mov rcx, hOutput
    lea rdx, pe_optional_header
    mov r8d, 240
    xor r9d, r9d
    xor rax, rax
    push rax
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Write section headers
    mov rcx, hOutput
    lea rdx, sect_text_name
    mov r8d, 40
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Pad to section start
    mov rcx, hOutput
    xor edx, edx
    mov r8d, 400h
    xor r9d, r9d
    call SetFilePointer
    
    ; Write code
    mov rcx, hOutput
    mov rdx, lpOutput
    mov r8d, dword ptr cbOutput
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    ; Pad section
    mov eax, dword ptr cbOutput
    and eax, 511
    jz no_pad
    neg eax
    add eax, 512
    mov [rbp+40], eax
    
    mov rcx, hOutput
    lea rdx, temp_path
    mov r8d, [rbp+40]
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
no_pad:
    ; Close file
    mov rcx, hOutput
    call CloseHandle
    
    lea rcx, sz_ok
    call print
    xor eax, eax
    leave
    ret
file_err:
    lea rcx, sz_err_file
    call print
    mov eax, 1
    leave
    ret
write_pe_file endp

; Main Entry Point
_start proc
    sub rsp, 40
    
    ; Get command line
    call GetCommandLineA
    mov rsi, rax
    
    ; Skip program name
    cmp byte ptr [rsi], '"'
    je quoted
    cmp byte ptr [rsi], 0
    je usage
L1: lodsb
    cmp al, ' '
    jne L1
    jmp skip_space
quoted:
    lodsb
    cmp al, '"'
    jne quoted
skip_space:
    lodsb
    cmp al, ' '
    je skip_space
    dec rsi
    
    cmp al, 0
    je usage
    
    ; rsi now points to source filename
    mov r12, rsi
    
    ; Open source file
    mov rcx, rsi
    xor edx, edx
    mov r8d, 3
    xor r9d, r9d
    mov qword ptr [rsp+32], 3
    mov qword ptr [rsp+40], 80h
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je file_err
    mov hInput, rax
    
    ; Get file size
    mov rcx, hInput
    lea rdx, cbSource
    call GetFileSizeEx
    
    ; Allocate buffer
    mov rcx, cbSource
    add rcx, 4096
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpSource, rax
    
    ; Read file
    mov rcx, hInput
    mov rdx, lpSource
    mov r8, cbSource
    lea r9, temp_path
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    ; Close input
    mov rcx, hInput
    call CloseHandle
    
    ; Initialize token buffer
    mov rcx, MAX_TOK * 32
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpTokens, rax
    
    ; Initialize output buffer
    mov rcx, MAX_OUT
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpOutput, rax
    
    ; Lexical analysis
    call lex
    
    ; Initialize
    mov cbOutput, 0
    mov sym_count, 0
    mov func_count, 0
    mov literal_count, 0
    mov stack_offset, 32
    
    ; Parse and compile
    lea rcx, sz_scan
    call print
    
L2: call cur_tok
    cmp eax, T_EOF
    je compile_done
    cmp eax, 100 + K_FN
    je do_func
    cmp eax, T_NL
    je skip_nl
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
skip_nl:
    call advance
    jmp L2
do_func:
    call parse_function
    jmp L2
    
compile_done:
    ; Write output
    mov rcx, r12
    lea rdi, temp_path
    mov rsi, r12
L3: lodsb
    stosb
    test al, al
    jnz L3
    dec rdi
    mov dword ptr [rdi], '.exe'
    mov byte ptr [rdi+4], 0
    lea rcx, temp_path
    call write_pe_file
    
    ; Cleanup
    mov rcx, lpSource
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
    mov rcx, lpTokens
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
    mov rcx, lpOutput
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
    
    xor ecx, ecx
    call ExitProcess
    
usage:
    lea rcx, sz_usage
    call print
    mov ecx, 1
    call ExitProcess
    
file_err:
    lea rcx, sz_err_file
    call print
    mov ecx, 1
    call ExitProcess
_start endp

end
T_ID equ 3
T_STR equ 4
T_LP equ 5
T_RP equ 6
T_LB equ 7
T_RB equ 8
T_PLUS equ 9
T_MINUS equ 10
T_MUL equ 11
T_DIV equ 12
T_ASSIGN equ 13
T_COLON equ 14
T_COMMA equ 15
T_ARROW equ 16
T_EQ equ 17
T_LT equ 18
T_GT equ 19
T_LTE equ 20
T_GTE equ 21
T_NEQ equ 22
T_AND equ 23
T_OR equ 24
T_NOT equ 25
T_DOT equ 26
T_LBRACKET equ 27
T_RBRACKET equ 28

; Keywords (100 + K_xxx)
K_FN equ 1
K_LET equ 2
K_IF equ 3
K_ELSE equ 4
K_WHILE equ 5
K_RETURN equ 6
K_STRUCT equ 7
K_FOR equ 8

; AST Node Types
AST_PROGRAM equ 1
AST_FN_DECL equ 2
AST_VAR_DECL equ 3
AST_BLOCK equ 4
AST_IF equ 5
AST_WHILE equ 6
AST_RETURN equ 7
AST_EXPR_STMT equ 8
AST_BINARY equ 9
AST_UNARY equ 10
AST_LITERAL equ 11
AST_IDENT equ 12
AST_CALL equ 13
AST_ASSIGN equ 14

; Binary Operators
OP_ADD equ 1
OP_SUB equ 2
OP_MUL equ 3
OP_DIV equ 4
OP_EQ equ 5
OP_NE equ 6
OP_LT equ 7
OP_GT equ 8
OP_LE equ 9
OP_GE equ 10
OP_AND equ 11
OP_OR equ 12

; Unary Operators
UOP_NEG equ 1
UOP_NOT equ 2

; Types
TYPE_VOID equ 0
TYPE_I64 equ 1
TYPE_PTR equ 2

; Precedence Levels
PREC_NONE equ 0
PREC_ASSIGN equ 1
PREC_OR equ 2
PREC_AND equ 3
PREC_EQUALITY equ 4
PREC_COMPARISON equ 5
PREC_TERM equ 6
PREC_FACTOR equ 7
PREC_UNARY equ 8
PREC_CALL equ 9
PREC_PRIMARY equ 10

; Symbol Table
SYMBOL_SIZE equ 48
MAX_SYMS equ 256
MAX_TOK equ 4096
MAX_SRC equ 65536
MAX_OUT equ 131072

; ================================================
; BSS Section
; ================================================

.data
align 8

; File I/O
hInput dq ?
hOutput dq ?
lpSource dq ?
cbSource dq ?
iSource dd ?

; Lexer State
lpTokens dq ?
nTokens dd ?
iToken dd ?
cur_val dq ?
cur_str db 64 dup(?)

; Parser State
ast_pool dq ?
ast_used dd ?

; Symbol Table

; Code Generation
lpOutput dq ?
cbOutput dd ?
stack_offset dd ?
literal_pool db 4096 dup(?)
literal_used dd ?

; Temp buffers
temp_path db MAX_PATH dup(?)
source_buffer db MAX_SRC dup(?)
token_buffer db MAX_TOK * 32 dup(?)
output_buffer db MAX_OUT dup(?)

; ================================================
; Code Section
; ================================================

.code
align 16

; ================================================
; Utility Functions
; ================================================

strlen proc
    xor rax, rax
L1: cmp byte ptr [rcx+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

print proc
    push rsi
    push rdi
    mov rsi, rcx
    call strlen
    mov rdx, rsi
    mov r8, rax
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    mov r9, 0
    push 0
    sub rsp, 32
    call WriteConsoleA
    add rsp, 40
    pop rdi
    pop rsi
    ret
print endp

memcpy proc
    mov rsi, rdx
    mov rdi, rcx
    mov rcx, r8
    rep movsb
    ret
memcpy endp

atoi proc
    xor rax, rax
    mov rsi, rcx
L2: movzx rdx, byte ptr [rsi]
    test dl, dl
    jz D2
    cmp dl, '0'
    jb D2
    cmp dl, '9'
    ja D2
    sub dl, '0'
    imul rax, 10
    add rax, rdx
    inc rsi
    jmp L2
D2: ret
atoi endp

; ================================================
; Symbol Table Management
; ================================================

sym_find proc
    ; rcx = name ptr, returns rax = symbol index or -1
    mov rsi, rcx
    xor ecx, ecx
    mov r8d, sym_count
L0: cmp ecx, r8d
    jae not_found
    mov eax, ecx
    imul eax, SYMBOL_SIZE
    lea rdx, sym_table
    add rdx, rax
    lea rdi, [rdx+8]
    mov rsi, [rsp+8]  ; name ptr from argument
    push rcx
    mov rcx, 32
    repe cmpsb
    pop rcx
    je found
    inc ecx
    jmp L0
not_found:
    mov rax, -1
    ret
found:
    mov eax, ecx
    ret
sym_find endp

sym_add proc
    ; rcx=name, edx=type, r8=value, r9=stack_offset
    mov r11d, sym_count
    cmp r11d, MAX_SYMS
    jae overflow
    mov eax, r11d
    imul eax, SYMBOL_SIZE
    lea rdx, sym_table
    add rdx, rax
    mov [rdx], r8d      ; type
    mov [rdx+4], r9d    ; stack offset
    lea rdi, [rdx+8]
    mov rsi, rcx
    mov rcx, 32
    rep movsb
    inc sym_count
    mov eax, r11d
    ret
overflow:
    xor eax, eax
    ret
sym_add endp

; ================================================
; Lexer
; ================================================

lex proc
    mov nTokens, 0
    mov iSource, 0

L4: call skip_ws
    call peek
    cmp eax, -1
    je EofT
    cmp eax, 10
    je NewL
    cmp eax, '"'
    je StrT
    cmp eax, 'A'
    jb N1a
    cmp eax, 'Z'
    jbe IdT
N1a:
    cmp eax, 'a'
    jb N2a
    cmp eax, 'z'
    jbe IdT
N2a:
    cmp eax, '0'
    jb SymT
    cmp eax, '9'
    jbe NumT
    jmp SymT

NewL:
    call next
    mov ecx, T_NL
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L4

StrT:
    call next
    mov r12, iSource
L5: call peek
    cmp eax, '"'
    je StrE
    cmp eax, -1
    je StrE
    call next
    jmp L5
StrE:
    mov ecx, T_STR
    xor edx, edx
    mov r8, lpSource
    add r8, r12
    call add_token
    call next
    jmp L4

IdT:
    sub rsp, 48
    lea rdi, [rsp+8]
L6: call peek
    cmp eax, 'A'
    jb IdC
    cmp eax, 'Z'
    jbe IdA
IdC:
    cmp eax, 'a'
    jb IdC2
    cmp eax, 'z'
    jbe IdA
IdC2:
    cmp eax, '0'
    jb IdE
    cmp eax, '9'
    jbe IdA
    cmp eax, '_'
    jne IdE
IdA:
    stosb
    call next
    jmp L6
IdE:
    mov byte ptr [rdi], 0
    lea rcx, [rsp+8]
    call check_kw
    test eax, eax
    jnz IsKw
    mov ecx, T_ID
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
    jmp IdD
IsKw:
    add eax, 100
    mov ecx, eax
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
IdD:
    add rsp, 48
    jmp L4

NumT:
    mov r12, iSource
L7: call peek
    cmp eax, '0'
    jb NumE
    cmp eax, '9'
    ja NumE
    call next
    jmp L7
NumE:
    mov byte ptr [temp_path], 0
    lea rdi, temp_path
    mov rsi, lpSource
    add rsi, r12
    mov rcx, iSource
    sub rcx, r12
    rep movsb
    mov byte ptr [rdi], 0
    lea rcx, temp_path
    call atoi
    mov ecx, T_NUM
    mov rdx, rax
    xor r8, r8
    call add_token
    jmp L4

SymT:
    cmp eax, '('
    je Tlp
    cmp eax, ')'
    je Trp
    cmp eax, '{'
    je Tlb
    cmp eax, '}'
    je Trb
    cmp eax, '+'
    je Tpl
    cmp eax, '-'
    je Tmi
    cmp eax, '*'
    je Tmu
    cmp eax, '/'
    je Tdi
    cmp eax, '='
    je Tas
    cmp eax, ':'
    je Tco
    cmp eax, ','
    je Tcm
    cmp eax, '<'
    je Tlt
    cmp eax, '>'
    je Tgt
    cmp eax, '&'
    je Tam
    cmp eax, '|'
    je Tor
    cmp eax, '!'
    je Tno
    cmp eax, '.'
    je Tdt
    cmp eax, '['
    je Tlbk
    cmp eax, ']'
    je Trbk
    call next
    jmp L4

Tlp: mov ecx, T_LP
    jmp Tadd
Trp: mov ecx, T_RP
    jmp Tadd
Tlb: mov ecx, T_LB
    jmp Tadd
Trb: mov ecx, T_RB
    jmp Tadd
Tpl: mov ecx, T_PLUS
    jmp Tadd
Tmu: mov ecx, T_MUL
    jmp Tadd
Tco: mov ecx, T_COLON
    jmp Tadd
Tcm: mov ecx, T_COMMA
    jmp Tadd
Tlt: mov ecx, T_LT
    jmp Tadd
Tgt: mov ecx, T_GT
    jmp Tadd
Tam: mov ecx, T_AND
    jmp Tadd
Tor: mov ecx, T_OR
    jmp Tadd
Tno: mov ecx, T_NOT
    jmp Tadd
Tdt: mov ecx, T_DOT
    jmp Tadd
Tlbk: mov ecx, T_LBRACKET
    jmp Tadd
Trbk: mov ecx, T_RBRACKET
    jmp Tadd

Tmi:
    call next
    call peek
    cmp eax, '>'
    je Tar
    mov ecx, T_MINUS
    jmp Taddn
Tar:
    call next
    mov ecx, T_ARROW
    jmp Tadd

Tdi:
    call next
    call peek
    cmp eax, '/'
    je Cmt
    mov ecx, T_DIV
    jmp Taddn
Cmt:
    call next
CmtL:
    call peek
    cmp eax, 10
    je L4
    cmp eax, -1
    je L4
    call next
    jmp CmtL

Tas:
    call next
    call peek
    cmp eax, '='
    je Teq
    mov ecx, T_ASSIGN
    jmp Taddn
Teq:
    call next
    mov ecx, T_EQ
    jmp Tadd

Taddn:
    call next
Tadd:
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L4

EofT:
    mov ecx, T_EOF
    xor edx, edx
    xor r8, r8
    call add_token
    ret
lex endp

peek proc
    mov eax, iSource
    cmp eax, cbSource
    jae Eof
    mov rcx, lpSource
    movzx eax, byte ptr [rcx+rax]
    ret
Eof: mov eax, -1
    ret
peek endp

next proc
    inc iSource
    ret
next endp

skip_ws proc
L3: call peek
    cmp eax, ' '
    je W1
    cmp eax, 9
    je W1
    cmp eax, 13
    je W1
    ret
W1: call next
    jmp L3
skip_ws endp

add_token proc
    ; rcx=type, rdx=val, r8=str
    mov r9d, nTokens
    shl r9d, 5
    mov r10, lpTokens
    add r10, r9
    mov [r10], ecx
    mov [r10+8], rdx
    test r8, r8
    jz N1
    lea rdi, [r10+16]
    mov rsi, r8
    mov rcx, 16
    rep movsb
N1: inc nTokens
    ret
add_token endp

check_kw proc
    ; rcx=str, returns eax=K_xxx or 0
    push rbx
    push rsi
    push rdi
    mov rsi, rcx

    ; Check 'fn'
    lea rdi, kw_fn
    mov al, [rsi]
    cmp al, 'f'
    jne C1
    mov al, [rsi+1]
    cmp al, 'n'
    jne C1
    cmp byte ptr [rsi+2], 0
    jne C1
    mov eax, K_FN
    jmp KwD
C1:
    ; Check 'let'
    mov al, [rsi]
    cmp al, 'l'
    jne C2
    mov al, [rsi+1]
    cmp al, 'e'
    jne C2
    mov al, [rsi+2]
    cmp al, 't'
    jne C2
    cmp byte ptr [rsi+3], 0
    jne C2
    mov eax, K_LET
    jmp KwD
C2:
    ; Check 'if'
    mov al, [rsi]
    cmp al, 'i'
    jne C3
    mov al, [rsi+1]
    cmp al, 'f'
    jne C3
    cmp byte ptr [rsi+2], 0
    jne C3
    mov eax, K_IF
    jmp KwD
C3:
    ; Check 'else'
    mov al, [rsi]
    cmp al, 'e'
    jne C4
    mov al, [rsi+1]
    cmp al, 'l'
    jne C4
    mov al, [rsi+2]
    cmp al, 's'
    jne C4
    mov al, [rsi+3]
    cmp al, 'e'
    jne C4
    cmp byte ptr [rsi+4], 0
    jne C4
    mov eax, K_ELSE
    jmp KwD
C4:
    ; Check 'while'
    mov al, [rsi]
    cmp al, 'w'
    jne C5
    mov al, [rsi+1]
    cmp al, 'h'
    jne C5
    mov al, [rsi+2]
    cmp al, 'i'
    jne C5
    mov al, [rsi+3]
    cmp al, 'l'
    jne C5
    mov al, [rsi+4]
    cmp al, 'e'
    jne C5
    cmp byte ptr [rsi+5], 0
    jne C5
    mov eax, K_WHILE
    jmp KwD
C5:
    ; Check 'return'
    mov al, [rsi]
    cmp al, 'r'
    jne C6
    mov al, [rsi+1]
    cmp al, 'e'
    jne C6
    mov al, [rsi+2]
    cmp al, 't'
    jne C6
    mov al, [rsi+3]
    cmp al, 'u'
    jne C6
    mov al, [rsi+4]
    cmp al, 'r'
    jne C6
    mov al, [rsi+5]
    cmp al, 'n'
    jne C6
    cmp byte ptr [rsi+6], 0
    jne C6
    mov eax, K_RETURN
    jmp KwD
C6:
    ; Check 'for'
    mov al, [rsi]
    cmp al, 'f'
    jne C7
    mov al, [rsi+1]
    cmp al, 'o'
    jne C7
    mov al, [rsi+2]
    cmp al, 'r'
    jne C7
    cmp byte ptr [rsi+3], 0
    jne C7
    mov eax, K_FOR
    jmp KwD
C7:
    xor eax, eax
KwD:
    pop rdi
    pop rsi
    pop rbx
    ret
check_kw endp

; ================================================
; Parser Core
; ================================================

advance proc
    mov eax, iToken
    inc eax
    mov iToken, eax
    call cur_tok
    mov ecx, iToken
    shl ecx, 5
    mov r10, lpTokens
    lea r10, [r10+rcx]
    mov edx, [r10+8]
    mov cur_val, rdx
    lea rdi, cur_str
    lea rsi, [r10+16]
    mov rcx, 16
    rep movsb
    ret
advance endp

cur_tok proc
    mov eax, iToken
    shl eax, 5
    mov r10, lpTokens
    mov eax, [r10+rax]
    ret
cur_tok endp

expect proc
    ; ecx = expected token
    call cur_tok
    cmp eax, ecx
    je ok
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
ok:
    jmp advance
expect endp

match proc
    ; ecx = token to match, returns ZF=1 if matched
    call cur_tok
    cmp eax, ecx
    ret
match endp

; ================================================
; Expression Parser (Precedence Climbing)
; ================================================

parse_expr proc
    mov ecx, PREC_ASSIGN
    call parse_precedence
    ret
parse_expr endp

parse_precedence proc
    ; ecx = minimum precedence
    push rbp
    mov rbp, rsp
    sub rsp, 32

    mov [rbp+16], ecx

    ; Parse prefix
    call parse_prefix
    mov [rbp+24], rax

L1: ; While next token has higher precedence
    call get_infix_precedence
    cmp eax, [rbp+16]
    jb done

    call advance

    ; Get infix parser
    call get_infix_parser
    mov rcx, [rbp+24]   ; left
    call rax            ; returns new left in rax
    mov [rbp+24], rax
    jmp L1

done:
    mov rax, [rbp+24]
    leave
    ret
parse_precedence endp

parse_prefix proc
    call cur_tok
    mov edx, eax

    cmp edx, T_NUM
    je prefix_number
    cmp edx, T_STR
    je prefix_string
    cmp edx, T_ID
    je prefix_identifier
    cmp edx, T_LP
    je prefix_grouping
    cmp edx, T_MINUS
    je prefix_unary
    cmp edx, T_NOT
    je prefix_unary_not

    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    ret

prefix_number:
    mov rcx, cur_val
    call emit_push_imm
    call advance
    ret

prefix_string:
    ; Add to literal pool
    mov rsi, cur_str
    lea rdi, literal_pool
    add rdi, literal_used
    mov r12, rdi
L_str: lodsb
    test al, al
    jz str_done
    stosb
    inc literal_used
    jmp L_str
str_done:
    mov byte ptr [rdi], 0
    inc literal_used

    ; Emit lea rax, [rip+offset]
    mov cl, 48h
    call emit_byte
    mov cl, 8Dh
    call emit_byte
    mov cl, 05h
    call emit_byte
    mov rcx, r12
    sub rcx, offset literal_pool
    call emit_dword

    call advance
    ret

prefix_identifier:
    ; Variable load
    lea rcx, cur_str
    call sym_find
    cmp rax, -1
    je undef_var

    ; mov rax, [rbp-offset]
    mov ecx, SYMBOL_SIZE
    mul ecx
    lea rdx, sym_table
    add rdx, rax
    mov eax, [rdx+4]    ; stack offset
    neg eax

    mov cl, 48h
    call emit_byte
    mov cl, 8Bh
    call emit_byte
    mov cl, 85h
    call emit_byte
    mov ecx, eax
    call emit_dword

    call advance
    ret

undef_var:
    lea rcx, sz_err_undef
    call print
    mov ecx, 1
    call ExitProcess
    ret

prefix_grouping:
    mov ecx, T_LP
    call expect
    mov ecx, PREC_ASSIGN
    call parse_precedence
    mov ecx, T_RP
    call expect
    ret

prefix_unary:
    call advance
    mov ecx, PREC_UNARY
    call parse_precedence
    ; neg rax
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret

prefix_unary_not:
    call advance
    mov ecx, PREC_UNARY
    call parse_precedence
    ; not rax
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    ret
parse_prefix endp

parse_infix_binary proc
    ; rcx = left side
    push rcx
    call cur_tok
    mov r12d, eax       ; operator token
    call get_precedence
    inc eax             ; right associative
    mov ecx, eax
    call parse_precedence
    mov r13, rax        ; right side

    ; Map token to operator and emit code
    mov ecx, r12d
    call token_to_op
    mov r14d, eax

    pop rdx             ; left (already in stack/register)

    ; For now, assume left is in rax, right in rbx
    ; mov rbx, rax (save left)
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 0C3h
    call emit_byte

    ; right is in rax, emit operation
    cmp r14d, OP_ADD
    je do_add
    cmp r14d, OP_SUB
    je do_sub
    cmp r14d, OP_MUL
    je do_mul
    cmp r14d, OP_DIV
    je do_div
    cmp r14d, OP_EQ
    je do_eq

do_add:
    ; add rax, rbx
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret

do_sub:
    ; sub rbx, rax
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0C3h
    call emit_byte
    ; mov rax, rbx
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret

do_mul:
    ; imul rax, rbx
    mov cl, 48h
    call emit_byte
    mov cl, 0FAFh
    call emit_byte
    mov cl, 0C3h
    call emit_byte
    ret

do_div:
    ; xchg rax, rbx
    mov cl, 48h
    call emit_byte
    mov cl, 87h
    call emit_byte
    mov cl, 0C3h
    call emit_byte
    ; xor rdx, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 31h
    call emit_byte
    mov cl, 0D2h
    call emit_byte
    ; idiv rbx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0FBh
    call emit_byte
    ret

do_eq:
    ; cmp rax, rbx
    mov cl, 48h
    call emit_byte
    mov cl, 39h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ; sete al
    mov cl, 0Fh
    call emit_byte
    mov cl, 94h
    call emit_byte
    mov cl, 0C0h
    call emit_byte
    ; movzx rax, al
    mov cl, 48h
    call emit_byte
    mov cl, 0Fh
    call emit_byte
    mov cl, 0B6h
    call emit_byte
    mov cl, 0C0h
    call emit_byte
    ret
parse_infix_binary endp

get_infix_precedence proc
    call cur_tok
    mov edx, eax

    cmp edx, T_ASSIGN
    je prec_assign
    cmp edx, T_EQ
    je prec_equality
    cmp edx, T_LT
    je prec_comparison
    cmp edx, T_GT
    je prec_comparison
    cmp edx, T_PLUS
    je prec_term
    cmp edx, T_MINUS
    je prec_term
    cmp edx, T_MUL
    je prec_factor
    cmp edx, T_DIV
    je prec_factor
    cmp edx, T_AND
    je prec_and
    cmp edx, T_OR
    je prec_or

    mov eax, PREC_NONE
    ret

prec_assign:    mov eax, PREC_ASSIGN
    ret
prec_or:        mov eax, PREC_OR
    ret
prec_and:       mov eax, PREC_AND
    ret
prec_equality:  mov eax, PREC_EQUALITY
    ret
prec_comparison: mov eax, PREC_COMPARISON
    ret
prec_term:      mov eax, PREC_TERM
    ret
prec_factor:    mov eax, PREC_FACTOR
    ret
get_infix_precedence endp

get_infix_parser proc
    call cur_tok
    mov edx, eax

    cmp edx, T_LP
    je infix_call

    lea rax, parse_infix_binary
    ret

infix_call:
    lea rax, parse_call
    ret
get_infix_parser endp

parse_call proc
    ; rcx = function name (identifier already parsed)
    push rcx
    mov ecx, T_LP
    call expect

    ; Parse args (simplified - no args for now)
    call match
    cmp ecx, T_RP
    je no_args
    ; Parse expressions...
no_args:
    mov ecx, T_RP
    call expect

    ; Emit call (simplified - assume function at fixed address)
    mov cl, 0E8h
    call emit_byte
    xor ecx, ecx        ; placeholder offset
    call emit_dword

    pop rcx
    ret
parse_call endp

token_to_op proc
    ; ecx = token, returns eax = operator
    cmp ecx, T_PLUS
    je op_add
    cmp ecx, T_MINUS
    je op_sub
    cmp ecx, T_MUL
    je op_mul
    cmp ecx, T_DIV
    je op_div
    cmp ecx, T_EQ
    je op_eq

    xor eax, eax
    ret

op_add: mov eax, OP_ADD
    ret
op_sub: mov eax, OP_SUB
    ret
op_mul: mov eax, OP_MUL
    ret
op_div: mov eax, OP_DIV
    ret
op_eq:  mov eax, OP_EQ
    ret
token_to_op endp

get_precedence proc
    call cur_tok
    mov edx, eax

    cmp edx, T_ASSIGN
    je prec_assign
    cmp edx, T_EQ
    je prec_equality
    cmp edx, T_LT
    je prec_comparison
    cmp edx, T_GT
    je prec_comparison
    cmp edx, T_PLUS
    je prec_term
    cmp edx, T_MINUS
    je prec_term
    cmp edx, T_MUL
    je prec_factor
    cmp edx, T_DIV
    je prec_factor

    mov eax, PREC_NONE
    ret

prec_assign:    mov eax, PREC_ASSIGN
    ret
prec_equality:  mov eax, PREC_EQUALITY
    ret
prec_comparison: mov eax, PREC_COMPARISON
    ret
prec_term:      mov eax, PREC_TERM
    ret
prec_factor:    mov eax, PREC_FACTOR
    ret
get_precedence endp

; ================================================
; Statement Parser
; ================================================

parse_stmt proc
    call cur_tok

    cmp eax, 100 + K_LET
    je parse_let_stmt
    cmp eax, 100 + K_IF
    je parse_if_stmt
    cmp eax, 100 + K_WHILE
    je parse_while_stmt
    cmp eax, 100 + K_RETURN
    je parse_return_stmt
    cmp eax, T_LB
    je parse_block_stmt

    ; Expression statement
    jmp parse_expr_stmt
parse_stmt endp

parse_let_stmt proc
    mov ecx, 100 + K_LET
    call expect

    ; Variable name
    call cur_tok
    cmp eax, T_ID
    jne syntax_err
    lea r12, cur_str
    call advance

    ; Optional type (skip)
    call match
    cmp ecx, T_COLON
    jne no_type
    call advance
    call advance        ; skip type name

no_type:
    ; Initializer
    call match
    cmp ecx, T_ASSIGN
    jne no_init
    call advance
    call parse_expr
    jmp has_init

no_init:
    ; Default to 0
    xor ecx, ecx
    call emit_push_imm

has_init:
    ; Allocate stack space
    mov ecx, r12
    mov edx, TYPE_I64
    mov r8, rax
    mov r9d, stack_offset
    call sym_add

    ; mov [rbp-offset], rax
    mov eax, stack_offset
    neg eax
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 85h
    call emit_byte
    mov ecx, eax
    call emit_dword

    add stack_offset, 8

    mov ecx, T_NL
    call expect
    ret

syntax_err:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    ret
parse_let_stmt endp

parse_if_stmt proc
    mov ecx, 100 + K_IF
    call expect

    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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

    ; jz else_label
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r12d, cbOutput
    xor ecx, ecx
    call emit_dword

    call parse_stmt

    ; jmp end_label
    mov cl, 0E9h
    call emit_byte
    mov r13d, cbOutput
    xor ecx, ecx
    call emit_dword

    ; Patch else jump
    mov eax, cbOutput
    sub eax, r12d
    sub eax, 4
    mov r10, lpOutput
    add r10, r12d
    mov [r10], eax

    ; Else?
    call match
    cmp ecx, 100 + K_ELSE
    jne no_else
    call advance
    call parse_stmt

no_else:
    ; Patch end jump
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, lpOutput
    add r10, r13d
    mov [r10], eax

    ret
parse_if_stmt endp

parse_while_stmt proc
    mov r12d, cbOutput  ; loop start

    mov ecx, 100 + K_WHILE
    call expect

    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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

    ; jz end
    mov cl, 0Fh
    call emit_byte
    mov cl, 84h
    call emit_byte
    mov r13d, cbOutput
    xor ecx, ecx
    call emit_dword

    call parse_stmt

    ; jmp start
    mov cl, 0E9h
    call emit_byte
    mov eax, r12d
    sub eax, cbOutput
    sub eax, 4
    mov ecx, eax
    call emit_dword

    ; Patch exit
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, lpOutput
    add r10, r13d
    mov [r10], eax

    ret
parse_while_stmt endp

parse_return_stmt proc
    mov ecx, 100 + K_RETURN
    call expect

    call match
    cmp ecx, T_NL
    je void_ret

    call parse_expr

void_ret:
    mov ecx, T_NL
    call expect

    call emit_epilog
    ret
parse_return_stmt endp

parse_expr_stmt proc
    call parse_expr
    mov ecx, T_NL
    call expect
    ret
parse_expr_stmt endp

parse_block_stmt proc
    mov ecx, T_LB
    call expect

stmt_loop:
    call match
    cmp ecx, T_RB
    je block_done
    cmp ecx, T_EOF
    je block_done

    call parse_stmt
    jmp stmt_loop

block_done:
    mov ecx, T_RB
    call expect
    ret
parse_block_stmt endp

; ================================================
; Function Parser
; ================================================

parse_function proc
    mov ecx, 100 + K_FN
    call expect

    ; Function name
    call cur_tok
    cmp eax, T_ID
    jne syntax_err
    lea r12, cur_str
    call advance

    ; Params (simplified)
    mov ecx, T_LP
    call expect
    call match
    cmp ecx, T_RP
    je no_params
    ; Parse params...
no_params:
    mov ecx, T_RP
    call expect

    ; Return type (skip)
    call match
    cmp ecx, T_ARROW
    jne no_ret
    call advance
    call advance

no_ret:
    ; Prolog
    call emit_prolog

    ; Body
    call parse_block

    ; Epilog
    call emit_epilog

    ret
parse_function endp

; ================================================
; Code Emission
; ================================================

emit_byte proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], cl
    inc cbOutput
    ret
emit_byte endp

emit_dword proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], ecx
    add cbOutput, 4
    ret
emit_dword endp

emit_qword proc
    mov rax, lpOutput
    add rax, cbOutput
    mov [rax], rcx
    add cbOutput, 8
    ret
emit_qword endp

emit_push_imm proc
    ; rcx = value
    cmp rcx, 127
    jg big_imm
    cmp rcx, -128
    jl big_imm

    ; push imm8
    mov cl, 6Ah
    call emit_byte
    mov cl, cl      ; rcx low byte
    call emit_byte
    ret

big_imm:
    ; push imm64
    mov cl, 68h
    call emit_byte
    mov rcx, [rsp+8]
    call emit_qword
    ret
emit_push_imm endp

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
    ; sub rsp, 32
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
    ; add rsp, 32
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

; ================================================
; Main Entry Point
; ================================================

_start proc
    sub rsp, 88

    ; Initialize
    mov sym_count, 0
    mov literal_used, 0
    mov stack_offset, 32

    ; Allocate buffers
    mov rcx, MAX_SRC
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpSource, rax

    mov rcx, MAX_TOK * 32
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpTokens, rax

    mov rcx, MAX_OUT
    mov edx, 1000h
    mov r8d, 4
    call VirtualAlloc
    mov lpOutput, rax

    ; Get command line
    call GetCommandLineA
    mov rsi, rax

    ; Skip exe name
    cmp byte ptr [rsi], '"'
    je quoted
    cmp byte ptr [rsi], 0
    je usage
find_end:
    lodsb
    cmp al, ' '
    jne find_end
    jmp skip_space
quoted:
    lodsb
    cmp al, '"'
    jne quoted
skip_space:
    lodsb
    cmp al, ' '
    je skip_space
    cmp al, 0
    je usage

    ; rsi points to input
    mov r12, rsi

    ; Check if file or folder
    call strlen
    cmp rax, 0
    je usage

    ; Check extension
    mov rdi, r12
    add rdi, rax
    sub rdi, 3
    cmp dword ptr [rdi], '.tf'
    je compile_file

    ; Assume folder
    mov rcx, r12
    call compile_folder
    jmp cleanup

compile_file:
    mov rcx, r12
    call compile_file_proc
    jmp cleanup

usage:
    lea rcx, sz_usage
    call print
    mov ecx, 1
    call ExitProcess

cleanup:
    ; Free buffers
    mov rcx, lpSource
    test rcx, rcx
    jz no_src
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
no_src:
    mov rcx, lpTokens
    test rcx, rcx
    jz no_tok
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
no_tok:
    mov rcx, lpOutput
    test rcx, rcx
    jz no_out
    xor edx, edx
    mov r8d, 8000h
    call VirtualFree
no_out:

    xor ecx, ecx
    call ExitProcess
_start endp

compile_file_proc proc
    ; rcx = filename
    push rbp
    mov rbp, rsp
    sub rsp, 64

    mov [rbp+16], rcx

    ; Print compiling
    lea rcx, sz_compiling
    call print
    mov rcx, [rbp+16]
    call print
    mov ecx, 10
    call putc

    ; Read source
    mov rcx, [rbp+16]
    call read_source_file
    test rax, rax
    jz read_fail

    ; Lex
    call lex

    ; Parse
    mov iToken, 0
    call advance

parse_loop:
    call cur_tok
    cmp eax, T_EOF
    je gen_code
    cmp eax, 100 + K_FN
    je do_fn
    cmp eax, T_NL
    je skip_nl

    lea rcx, sz_err_toplevel
    call print
    mov eax, 1
    leave
    ret

skip_nl:
    call advance
    jmp parse_loop

do_fn:
    call parse_function
    jmp parse_loop

gen_code:
    ; Write exe
    mov rcx, [rbp+16]
    lea rdx, temp_path
    call make_exe_name

    lea rcx, temp_path
    call write_pe_file

    lea rcx, sz_success
    call print

    xor eax, eax
    leave
    ret

read_fail:
    lea rcx, sz_err_read
    call print
    mov eax, 1
    leave
    ret
compile_file_proc endp

read_source_file proc
    ; rcx = filename
    push rbp
    mov rbp, rsp

    xor edx, edx
    mov r8d, 3
    xor r9d, r9d
    mov dword ptr [rsp+32], 3
    mov dword ptr [rsp+40], 80h
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je fail

    mov r12, rax

    lea rdx, cbSource
    mov rcx, r12
    call GetFileSizeEx

    mov rcx, r12
    mov rdx, lpSource
    mov r8, cbSource
    lea r9, temp_path
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40

    mov rcx, r12
    call CloseHandle

    mov rax, lpSource
    leave
    ret

fail:
    xor eax, eax
    leave
    ret
read_source_file endp

make_exe_name proc
    ; rcx = input, rdx = output
    mov rsi, rcx
    mov rdi, rdx

copy_loop:
    lodsb
    cmp al, '.'
    je found_dot
    test al, al
    jz add_exe
    stosb
    jmp copy_loop

found_dot:
    mov dword ptr [rdi], '.exe'
    mov byte ptr [rdi+4], 0
    ret

add_exe:
    mov dword ptr [rdi], '.exe'
    mov byte ptr [rdi+4], 0
    ret
make_exe_name endp

compile_folder proc
    ; rcx = folder path
    push rbp
    mov rbp, rsp
    sub rsp, 320

    ; Setup find pattern
    mov rsi, rcx
    lea rdi, [rbp-256]
    mov rcx, 240
    rep movsb
    mov byte ptr [rdi], '\'
    mov dword ptr [rdi+1], '*.tf'
    mov byte ptr [rdi+5], 0

    lea rcx, [rbp-256]
    lea rdx, [rbp-304]
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je folder_done
    mov [rbp-8], rax

file_loop:
    lea rdx, [rbp-304]
    add rdx, 44         ; cFileName
    mov rcx, [rbp-256]
    lea r8, [rbp-272]
    call path_join

    lea rcx, [rbp-272]
    call compile_file_proc

    mov rcx, [rbp-8]
    lea rdx, [rbp-304]
    call FindNextFileA
    test eax, eax
    jnz file_loop

    mov rcx, [rbp-8]
    call FindClose

folder_done:
    leave
    ret
compile_folder endp

path_join proc
    ; rcx = dir, rdx = file, r8 = out
    mov rsi, rcx
    mov rdi, r8

copy_dir:
    lodsb
    test al, al
    jz dir_end
    stosb
    jmp copy_dir

dir_end:
    cmp byte ptr [rdi-1], '\'
    je has_slash
    mov byte ptr [rdi], '\'
    inc rdi

has_slash:
    mov rsi, rdx
copy_file:
    lodsb
    stosb
    test al, al
    jnz copy_file

    ret
path_join endp

putc proc
    ; ecx = char
    sub rsp, 40
    mov [rsp+32], cl
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    lea rdx, [rsp+32]
    mov r8d, 1
    lea r9, [rsp+24]
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    ret
putc endp

; ================================================
; PE Writer
; ================================================

write_pe_file proc
    push rbp
    mov rbp, rsp
    sub rsp, 96

    mov [rbp+16], rcx

    ; Create file
    mov rcx, [rbp+16]
    mov edx, 40000000h
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+48], 2
    mov dword ptr [rsp+56], 80h
    xor eax, eax
    mov qword ptr [rsp+64], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je pe_fail
    mov [rbp+24], rax

    ; Calculate sizes
    mov eax, cbOutput
    add eax, 511
    and eax, -512
    mov [rbp+32], eax   ; raw_size

    mov eax, cbOutput
    add eax, 4095
    and eax, -4096
    mov [rbp+36], eax   ; virt_size

    ; Write DOS header
    mov rcx, [rbp+24]
    lea rdx, pe_dos_header
    mov r8d, 64
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Write PE sig
    mov rcx, [rbp+24]
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Write COFF header
    mov rcx, [rbp+24]
    lea rdx, pe_file_header
    mov r8d, 20
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Write optional header
    mov rcx, [rbp+24]
    lea rdx, pe_optional_header
    mov r8d, 240
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Write section headers
    mov rcx, [rbp+24]
    lea rdx, sect_text_name
    mov r8d, 40
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Pad to 0x400
    mov rcx, [rbp+24]
    xor edx, edx
    mov r8d, 400h
    xor r9d, r9d
    call SetFilePointer

    ; Write code
    mov rcx, [rbp+24]
    mov rdx, lpOutput
    mov r8d, cbOutput
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

    ; Pad to alignment
    mov eax, cbOutput
    and eax, 511
    jz no_pad
    neg eax
    add eax, 512
    mov [rbp+40], eax

    mov rcx, [rbp+24]
    lea rdx, temp_path    ; zero buffer
    mov r8d, [rbp+40]
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40

no_pad:
    mov rcx, [rbp+24]
    call CloseHandle

    xor eax, eax
    leave
    ret

pe_fail:
    mov eax, 1
    leave
    ret
write_pe_file endp

end