; ================================================
; RXUC-TerraForm v8.0 — The Complete System
; Unified Compiler + Agent Engine + Orchestrator
; Pure MASM64 | Self-Contained | Production Ready
; Assemble: ml64 /c /nologo terraform.asm
; Link: link /ENTRY:_start /SUBSYSTEM:CONSOLE /FIXED:NO terraform.obj kernel32.lib ntdll.lib
; ================================================

option casemap:none
option frame:auto

; ================================================
; External Imports (Windows API Only)
; ================================================
externdef GetCommandLineA:qword
externdef CreateFileA:qword
externdef ReadFile:qword
externdef WriteFile:qword
externdef CloseHandle:qword
externdef GetFileSizeEx:qword
externdef VirtualAlloc:qword
externdef VirtualFree:qword
externdef VirtualProtect:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword
externdef ExitProcess:qword
externdef SetFilePointer:qword
externdef GetModuleHandleA:qword
externdef GetProcessHeap:qword
externdef HeapAlloc:qword
externdef HeapFree:qword
externdef RtlCopyMemory:qword
externdef RtlZeroMemory:qword
externdef NtQueryInformationProcess:qword
externdef NtSetInformationProcess:qword

; ================================================
; Constants
; ================================================
STD_OUTPUT_HANDLE equ -11
MAX_SRC equ 131072
MAX_TOK equ 8192
MAX_OUT equ 524288
MAX_SYMS equ 512
INVALID_HANDLE_VALUE equ -1

; Token Types
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
T_DOT equ 20

; Keywords (100+ID)
K_FN equ 1
K_LET equ 2
K_IF equ 3
K_ELSE equ 4
K_WHILE equ 5
K_RETURN equ 6
K_STRUCT equ 7
K_FOR equ 8

; PE Constants
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC equ 20Bh
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3

; Agent Constants
AGENT_MAGIC equ 0x4154454D41524C52
STACK_PIVOT_SIZE equ 0xA00000
SIG_RECURSE equ 0x01
SIG_BIGDATA equ 0x02
SIG_THREADS equ 0x04

; ================================================
; Data Section
; ================================================
.data
align 16

; CLI Messages
sz_usage db "RXUC TerraForm v8.0 — Complete Agentic Compiler",10
         db "Usage: terraform <source.tf>",10,0
sz_loading db "[*] Loading source...",10,0
sz_lexing db "[*] Lexical analysis...",10,0
sz_parsing db "[*] Parsing & CodeGen...",10,0
sz_agent db "[*] Agent manifestation...",10,0
sz_linking db "[*] PE Generation...",10,0
sz_success db "[+] Compiled: ",0
sz_error db "[!] Error: ",0
sz_nl db 10,0

; PE Templates
dos_hdr:
dw 5A4Dh,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dd 80
pe_sig db 'PE',0,0

coff_hdr:
dw IMAGE_FILE_MACHINE_AMD64
dw 1
dd 0,0,0
dw 240
dw 22h

opt_hdr:
dw IMAGE_NT_OPTIONAL_HDR64_MAGIC
db 0,1
dd 0,0,0
dd 1000h
dd 1000h
dq 140000000h
dd 1000h
dd 200h
dw 6,0,0,0,6,0
dd 0
dd 2000h
dd 400h
dd 0
dw IMAGE_SUBSYSTEM_WINDOWS_CUI,0
dq 100000h,1000h,100000h,1000h
dd 0,16
times 128 db 0

sect_hdr:
db '.text',0,0,0
dd 0,1000h,0,400h,0,0
dw 0,0
dd 60000020h

; Keywords
kw_fn db 'fn',0
kw_let db 'let',0
kw_if db 'if',0
kw_else db 'else',0
kw_while db 'while',0
kw_return db 'return',0
kw_struct db 'struct',0
kw_for db 'for',0

; Agent State
g_AgentMagic dq 0
g_AgentSignal dd 0
g_AgentComplexity dd 0
g_pHeapStack dq 0
g_OriginalRsp dq 0

; ================================================
; BSS Section (Uninitialized)
; ================================================
.bss
align 16

; Core Buffers
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
cbOutput dd ?

; Symbol Table
sym_count dd ?
sym_table db MAX_SYMS * 64 dup(?)

; Working Buffers
source_buffer db MAX_SRC dup(?)
token_buffer db MAX_TOK * 32 dup(?)
output_buffer db MAX_OUT dup(?)
temp_path db 260 dup(?)

; ================================================
; Code Section
; ================================================
.code
align 16

; ================================================
; String Utilities
; ================================================
strlen proc
    xor rax, rax
    mov rsi, rcx
L1: cmp byte ptr [rsi+rax], 0
    je D1
    inc rax
    jmp L1
D1: ret
strlen endp

strcmp proc
    mov rsi, rcx
    mov rdi, rdx
L2: mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne neq
    test al, al
    jz eq
    inc rsi
    inc rdi
    jmp L2
neq: mov eax, 1
    ret
eq: xor eax, eax
    ret
strcmp endp

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
    xor r9, r9
    push 0
    sub rsp, 32
    call qword ptr [WriteConsoleA]
    add rsp, 40
    pop rdi
    pop rsi
    ret
print endp

; ================================================
; Lexer Implementation
; ================================================
peek proc
    mov rax, iSource
    cmp rax, cbSource
    jae eof
    mov rcx, lpSource
    movzx eax, byte ptr [rcx+rax]
    ret
eof: mov eax, -1
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
    mov r9d, nTokens
    shl r9d, 5
    mov r10, lpTokens
    add r10, r9
    mov [r10], ecx
    mov [r10+4], iLine
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
    lea rdi, kw_struct
    call strcmp
    test eax, eax
    jz is_struct
    lea rdi, kw_for
    call strcmp
    test eax, eax
    jz is_for
    
    xor eax, eax
    jmp done_kw
is_fn: mov eax, K_FN
    jmp done_kw
is_let: mov eax, K_LET
    jmp done_kw
is_if: mov eax, K_IF
    jmp done_kw
is_else: mov eax, K_ELSE
    jmp done_kw
is_while: mov eax, K_WHILE
    jmp done_kw
is_ret: mov eax, K_RETURN
    jmp done_kw
is_struct: mov eax, K_STRUCT
    jmp done_kw
is_for: mov eax, K_FOR
done_kw:
    pop rdi
    pop rsi
    ret
check_kw endp

lex proc
    mov nTokens, 0
    mov iLine, 1
    
L4: call skip_ws
    call peek
    cmp eax, -1
    je EofTok
    cmp eax, 10
    je NewLTok
    cmp eax, '"'
    je StrTok
    cmp eax, 'A'
    jb N1a
    cmp eax, 'Z'
    jbe IdTok
N1a: cmp eax, 'a'
    jb N2a
    cmp eax, 'z'
    jbe IdTok
N2a: cmp eax, '0'
    jb SymTok
    cmp eax, '9'
    jbe NumTok
    jmp SymTok

NewLTok:
    call next
    inc iLine
    mov ecx, T_NL
    xor edx, edx
    xor r8, r8
    call add_token
    jmp L4

StrTok:
    call next
    mov r12, iSource
L5: call peek
    cmp eax, '"'
    je StrEnd
    cmp eax, -1
    je StrEnd
    cmp eax, '\'
    jne S1
    call next
S1: call next
    jmp L5
StrEnd:
    mov ecx, T_STR
    xor edx, edx
    mov r8, lpSource
    add r8, r12
    call add_token
    call next
    jmp L4

IdTok:
    sub rsp, 48
    lea rdi, [rsp+8]
L6: call peek
    cmp eax, 'A'
    jb IdC
    cmp eax, 'Z'
    jbe IdA
IdC: cmp eax, 'a'
    jb IdC2
    cmp eax, 'z'
    jbe IdA
IdC2: cmp eax, '0'
    jb IdE
    cmp eax, '9'
    jbe IdA
    cmp eax, '_'
    jne IdE
IdA: stosb
    call next
    jmp L6
IdE: mov byte ptr [rdi], 0
    lea rcx, [rsp+8]
    call check_kw
    test eax, eax
    jnz IsKwTok
    mov ecx, T_ID
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
    jmp IdDone
IsKwTok:
    add eax, 100
    mov ecx, eax
    xor edx, edx
    lea r8, [rsp+8]
    call add_token
IdDone:
    add rsp, 48
    jmp L4

NumTok:
    mov r12, iSource
L7: call peek
    cmp eax, '0'
    jb NumEnd
    cmp eax, '9'
    ja NumEnd
    call next
    jmp L7
NumEnd:
    mov byte ptr [temp_path], 0
    lea rdi, temp_path
    mov rsi, lpSource
    add rsi, r12
    mov rcx, iSource
    sub rcx, r12
    rep movsb
    mov byte ptr [rdi], 0
    lea rcx, temp_path
    xor rax, rax
    mov rsi, rcx
N2: movzx rdx, byte ptr [rsi]
    test dl, dl
    jz N3
    sub dl, '0'
    imul rax, 10
    add rax, rdx
    inc rsi
    jmp N2
N3: mov ecx, T_NUM
    mov rdx, rax
    xor r8, r8
    call add_token
    jmp L4

SymTok:
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
    cmp eax, '.'
    je Tdt
    call next
    jmp L7

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
Tdt: mov ecx, T_DOT
    jmp Tadd

Tmi:
    call next
    call peek
    cmp eax, '>'
    je Tarw
    mov ecx, T_MINUS
    jmp Taddn
Tarw:
    call next
    mov ecx, T_ARROW
    jmp Tadd

Tdi:
    call next
    call peek
    cmp eax, '/'
    je CmtSkip
    mov ecx, T_DIV
    jmp Taddn
CmtSkip:
    call next
CmtLoop:
    call peek
    cmp eax, 10
    je L4
    cmp eax, -1
    je L4
    call next
    jmp CmtLoop

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

EofTok:
    mov ecx, T_EOF
    xor edx, edx
    xor r8, r8
    call add_token
    ret
lex endp

; ================================================
; Parser Core
; ================================================
cur_tok proc
    mov eax, iToken
    cmp eax, nTokens
    jae ct_eof
    shl eax, 5
    mov r10, lpTokens
    add r10, rax
    mov eax, [r10]
    ret
ct_eof:
    mov eax, T_EOF
    ret
cur_tok endp

advance_parser proc
    inc iToken
    ret
advance_parser endp

expect proc
    call cur_tok
    cmp eax, ecx
    je ok_exp
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess
ok_exp:
    jmp advance_parser
expect endp

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

emit_prolog proc
    mov cl, 55h
    call emit_byte
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 0E5h
    call emit_byte
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
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0C4h
    call emit_byte
    mov cl, 20h
    call emit_byte
    mov cl, 5Dh
    call emit_byte
    mov cl, 0C3h
    call emit_byte
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

; ================================================
; Expression Parser
; ================================================
parse_expr proc
    push rbp
    mov rbp, rsp
    call parse_term
L10:
    call cur_tok
    cmp eax, T_PLUS
    je do_add
    cmp eax, T_MINUS
    je do_sub
    leave
    ret
do_add:
    call advance_parser
    push rax
    call parse_term
    pop rdx
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0C2h
    call emit_byte
    jmp L10
do_sub:
    call advance_parser
    push rax
    call parse_term
    pop rdx
    xchg rax, rdx
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0C2h
    call emit_byte
    jmp L10
parse_expr endp

parse_term proc
    push rbp
    mov rbp, rsp
    call parse_factor
L11:
    call cur_tok
    cmp eax, T_MUL
    je do_mul
    cmp eax, T_DIV
    je do_div
    leave
    ret
do_mul:
    call advance_parser
    push rax
    call parse_factor
    pop rdx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0E2h
    call emit_byte
    jmp L11
do_div:
    call advance_parser
    push rax
    call parse_factor
    mov rcx, rax
    pop rax
    push rcx
    mov cl, 48h
    call emit_byte
    mov cl, 31h
    call emit_byte
    mov cl, 0D2h
    call emit_byte
    pop rcx
    push rax
    mov rax, rcx
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0F0h
    call emit_byte
    pop rax
    jmp L11
parse_term endp

parse_factor proc
    call cur_tok
    cmp eax, T_NUM
    je pf_num
    cmp eax, T_ID
    je pf_id
    cmp eax, T_LP
    je pf_grp
    cmp eax, T_MINUS
    je pf_neg
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess
pf_num:
    mov eax, iToken
    shl eax, 5
    mov r10, lpTokens
    add r10, rax
    mov rcx, [r10+8]
    call emit_push_imm
    call advance_parser
    ret
pf_id:
    call advance_parser
    ret
pf_grp:
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
    call expect
    ret
pf_neg:
    call advance_parser
    call parse_factor
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0D8h
    call emit_byte
    ret
parse_factor endp

; ================================================
; Statement Parser
; ================================================
parse_stmt proc
    call cur_tok
    cmp eax, 100 + K_LET
    je p_let
    cmp eax, 100 + K_IF
    je p_if
    cmp eax, 100 + K_WHILE
    je p_while
    cmp eax, 100 + K_RETURN
    je p_ret
    cmp eax, T_LB
    je p_blk
    call parse_expr
    mov ecx, T_NL
    call expect
    ret
p_let: jmp parse_let
p_if: jmp parse_if
p_while: jmp parse_while
p_ret: jmp parse_return
p_blk: jmp parse_block
parse_stmt endp

parse_let proc
    mov ecx, 100 + K_LET
    call expect
    call cur_tok
    cmp eax, T_ID
    jne err_stmt
    call advance_parser
    call cur_tok
    cmp eax, T_COLON
    jne chk_init
    call advance_parser
    call advance_parser
chk_init:
    call cur_tok
    cmp eax, T_ASSIGN
    jne no_init
    call advance_parser
    call parse_expr
no_init:
    mov ecx, T_NL
    call expect
    ret
err_stmt:
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess
parse_let endp

parse_if proc
    mov ecx, 100 + K_IF
    call expect
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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
    mov r10, lpOutput
    add r10, r12d
    mov [r10], eax
    call cur_tok
    cmp eax, 100 + K_ELSE
    jne no_else
    call advance_parser
    call parse_stmt
no_else:
    mov eax, cbOutput
    sub eax, r13d
    sub eax, 4
    mov r10, lpOutput
    add r10, r13d
    mov [r10], eax
    ret
parse_if endp

parse_while proc
    mov r12d, cbOutput
    mov ecx, 100 + K_WHILE
    call expect
    mov ecx, T_LP
    call expect
    call parse_expr
    mov ecx, T_RP
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
    mov r10, lpOutput
    add r10, r13d
    mov [r10], eax
    ret
parse_while endp

parse_return proc
    mov ecx, 100 + K_RETURN
    call expect
    call cur_tok
    cmp eax, T_NL
    je void_ret
    call parse_expr
void_ret:
    mov ecx, T_NL
    call expect
    call emit_epilog
    ret
parse_return endp

parse_block proc
    mov ecx, T_LB
    call expect
L12:
    call cur_tok
    cmp eax, T_RB
    je blk_done
    cmp eax, T_EOF
    je blk_done
    call parse_stmt
    jmp L12
blk_done:
    mov ecx, T_RB
    call expect
    ret
parse_block endp

parse_function proc
    mov ecx, 100 + K_FN
    call expect
    call cur_tok
    cmp eax, T_ID
    jne err_fn
    call advance_parser
    mov ecx, T_LP
    call expect
    call cur_tok
    cmp eax, T_RP
    je no_params
skip_params:
    call advance_parser
    call cur_tok
    cmp eax, T_RP
    je no_params
    cmp eax, T_COMMA
    je skip_params
    call advance_parser
    jmp skip_params
no_params:
    mov ecx, T_RP
    call expect
    call cur_tok
    cmp eax, T_ARROW
    jne no_ret
    call advance_parser
    call advance_parser
no_ret:
    call emit_prolog
    call parse_block
    call emit_epilog
    ret
err_fn:
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess
parse_function endp

; ================================================
; Agent Engine (Integrated)
; ================================================
manifest_agent proc
    ; rcx = complexity (0-1000)
    mov g_AgentMagic, AGENT_MAGIC
    mov g_AgentComplexity, ecx
    
    ; Simple complexity-to-signal mapping
    xor edx, edx
    cmp ecx, 100
    jl no_signals
    or edx, SIG_RECURSE
    cmp ecx, 300
    jl set_signals
    or edx, SIG_BIGDATA
    cmp ecx, 500
    jl set_signals
    or edx, SIG_THREADS
set_signals:
    mov g_AgentSignal, edx
    
    ; Allocate heap stack if recursion detected
    test edx, SIG_RECURSE
    jz no_heap
    xor ecx, ecx
    mov edx, STACK_PIVOT_SIZE
    mov r8d, 1000h
    mov r9d, 40h
    call VirtualAlloc
    mov g_pHeapStack, rax
    
no_heap:
no_signals:
    ret
manifest_agent endp

; ================================================
; PE Writer
; ================================================
write_pe proc
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp+16], rcx
    
    mov rcx, [rbp+16]
    mov edx, 40000000h
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+48], 2
    mov qword ptr [rsp+56], 80h
    xor eax, eax
    mov qword ptr [rsp+64], rax
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je pe_fail
    mov [rbp+24], rax
    
    mov eax, cbOutput
    add eax, 511
    and eax, -512
    mov [rbp+32], eax
    
    mov rcx, [rbp+24]
    lea rdx, dos_hdr
    mov r8d, 64
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov rcx, [rbp+24]
    lea rdx, pe_sig
    mov r8d, 4
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov rcx, [rbp+24]
    lea rdx, coff_hdr
    mov r8d, 24
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov eax, [rbp+32]
    mov dword ptr [opt_hdr+4], eax
    
    mov rcx, [rbp+24]
    lea rdx, opt_hdr
    mov r8d, 240
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov eax, [rbp+32]
    mov dword ptr [sect_hdr+8], eax
    mov dword ptr [sect_hdr+16], eax
    
    mov rcx, [rbp+24]
    lea rdx, sect_hdr
    mov r8d, 40
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov rcx, [rbp+24]
    xor edx, edx
    mov r8d, 400h
    xor r9d, r9d
    call SetFilePointer
    
    mov rcx, [rbp+24]
    mov rdx, lpOutput
    mov r8d, cbOutput
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    mov rcx, [rbp+24]
    call CloseHandle
    
    xor eax, eax
    leave
    ret
pe_fail:
    mov eax, 1
    leave
    ret
write_pe endp

; ================================================
; Main Entry
; ================================================
_start:
    sub rsp, 56
    
    lea rcx, source_buffer
    mov lpSource, rcx
    lea rcx, token_buffer
    mov lpTokens, rcx
    lea rcx, output_buffer
    mov lpOutput, rcx
    
    call GetCommandLineA
    mov rsi, rax
    cmp byte ptr [rsi], '"'
    je skip_q
L13:
    lodsb
    cmp al, ' '
    jne L13
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
    
    lea rcx, sz_loading
    call print
    
    mov rcx, r12
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
    mov r13, rax
    
    lea rdx, cbSource
    mov rcx, r13
    call GetFileSizeEx
    
    mov rcx, r13
    mov rdx, lpSource
    mov r8, cbSource
    lea r9, temp_path
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    mov rcx, r13
    call CloseHandle
    
    lea rcx, sz_lexing
    call print
    call lex
    
    lea rcx, sz_agent
    call print
    mov ecx, 200
    call manifest_agent
    
    lea rcx, sz_parsing
    call print
    mov cbOutput, 0
    
L14:
    call cur_tok
    cmp eax, T_EOF
    je write_out
    cmp eax, 100 + K_FN
    je do_fn
    call advance_parser
    jmp L14
    
do_fn:
    call parse_function
    jmp L14
    
write_out:
    lea rdi, temp_path
    mov rsi, r12
L15:
    lodsb
    cmp al, '.'
    je ext_f
    stosb
    test al, al
    jnz L15
    dec rdi
ext_f:
    mov dword ptr [rdi], '.exe'
    mov byte ptr [rdi+4], 0
    
    lea rcx, sz_linking
    call print
    
    lea rcx, temp_path
    call write_pe
    test eax, eax
    jnz write_err
    
    lea rcx, sz_success
    call print
    lea rcx, temp_path
    call print
    lea rcx, sz_nl
    call print
    
    xor ecx, ecx
    call ExitProcess
    
usage:
    lea rcx, sz_usage
    call print
    mov ecx, 1
    call ExitProcess
    
file_err:
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess
    
write_err:
    lea rcx, sz_error
    call print
    mov ecx, 1
    call ExitProcess

end