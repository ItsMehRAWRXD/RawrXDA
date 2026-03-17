; RXUC-TerraForm v4.0 - Smart Native Compiler
; Assemble: ml64 /c /Zi terraform.asm
; Link:     link /SUBSYSTEM:CONSOLE /ENTRY:_start terraform.obj kernel32.lib
; Usage:    terraform.exe source.tf

option casemap:none
option frame:noauto

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
externdef CreateFileW:qword
externdef MultiByteToWideChar:qword

; Constants
STD_OUTPUT_HANDLE equ -11
STD_ERROR_HANDLE equ -12
MAX_SRC equ 1048576
MAX_TOK equ 16384
MAX_SYM equ 4096
MAX_CODE equ 1048576
MAX_DATA equ 65536
INVALID_HANDLE_VALUE equ -1
PAGE_READWRITE equ 4
MEM_COMMIT equ 1000h
MEM_RELEASE equ 8000h
GENERIC_READ equ 80000000h
GENERIC_WRITE equ 40000000h
CREATE_ALWAYS equ 2
OPEN_EXISTING equ 3
FILE_ATTRIBUTE_NORMAL equ 80h

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
T_LE equ 20
T_GE equ 21
T_NE equ 22

; Keyword IDs
K_FN equ 1
K_LET equ 2
K_IF equ 3
K_ELSE equ 4
K_WHILE equ 5
K_RETURN equ 6
K_STRUCT equ 7
K_TRUE equ 8
K_FALSE equ 9
K_EXTERN equ 10

; AST Node Types
AST_PROGRAM equ 1
AST_FN equ 2
AST_LET equ 3
AST_IF equ 4
AST_WHILE equ 5
AST_RETURN equ 6
AST_BINOP equ 7
AST_UNARY equ 8
AST_LIT_INT equ 9
AST_LIT_STR equ 10
AST_LIT_BOOL equ 11
AST_IDENT equ 12
AST_CALL equ 13
AST_BLOCK equ 14
AST_ASSIGN equ 15

; PE constants
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC equ 20Bh
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3
IMAGE_REL_BASED_DIR64 equ 10
IMAGE_REL_BASED_HIGHLOW equ 3

; Data section
.data
align 8

sz_usage db "RXUC-TerraForm v4.0",10,"Usage: terraform <source.tf>",10,0
sz_err_file db "[!] File error",10,0
sz_err_syntax db "[!] Syntax error at line ",0
sz_err_undef db "[!] Undefined: ",0
sz_err_redef db "[!] Redefinition: ",0
sz_ok db "[+] Compiled: ",0
sz_scan db "[*] Parsing...",10,0
sz_codegen db "[*] Generating code...",10,0
sz_link db "[*] Writing PE...",10,0
sz_crlf db 10,0
sz_colon db ":",0

; Instruction templates
asm_push_rax db 50h
asm_push_rcx db 51h
asm_push_rdx db 52h
asm_push_rbx db 53h
asm_push_rbp db 55h
asm_push_rsi db 56h
asm_push_rdi db 57h
asm_pop_rax db 58h
asm_pop_rcx db 59h
asm_pop_rdx db 5Ah
asm_pop_rbx db 5Bh
asm_pop_rbp db 5Dh
asm_pop_rsi db 5Eh
asm_pop_rdi db 5Fh
asm_ret db 0C3h
asm_nop db 90h

; Keyword table
kw_list:
kw_fn db 'fn',0,0,0,0,0,0,0,0,0,0,0,0,0
kw_let db 'let',0,0,0,0,0,0,0,0,0,0,0
kw_if db 'if',0,0,0,0,0,0,0,0,0,0,0,0
kw_else db 'else',0,0,0,0,0,0,0,0,0,0
kw_while db 'while',0,0,0,0,0,0,0,0,0
kw_return db 'return',0,0,0,0,0,0,0,0
kw_struct db 'struct',0,0,0,0,0,0,0,0
kw_true db 'true',0,0,0,0,0,0,0,0,0,0
kw_false db 'false',0,0,0,0,0,0,0,0,0
kw_extern db 'extern',0,0,0,0,0,0,0,0

kw_ids:
db K_FN, K_LET, K_IF, K_ELSE, K_WHILE, K_RETURN, K_STRUCT, K_TRUE, K_FALSE, K_EXTERN

; External API imports for compiled programs
imp_ExitProcess db 'ExitProcess',0
imp_GetStdHandle db 'GetStdHandle',0
imp_WriteConsoleA db 'WriteConsoleA',0
imp_ReadFile db 'ReadFile',0
imp_CreateFileA db 'CreateFileA',0
imp_CloseHandle db 'CloseHandle',0

; PE templates
dos_header:
dw 5A4Dh
dw 30 dup(0)
dd 80

pe_sig db 'PE',0,0

file_header:
dw IMAGE_FILE_MACHINE_AMD64
dw 3                          ; Sections: .text, .data, .idata
dd 0                          ; Timestamp
dd 0                          ; Symbol table
dd 0                          ; Symbol count
dw 240                        ; Optional header size
dw 22h                        ; Characteristics

optional_header:
dw IMAGE_NT_OPTIONAL_HDR64_MAGIC
db 0,1
dd 0,0,0
dd 1000h                      ; Entry point RVA
dd 1000h                      ; Code base
dq 140000000h                 ; Image base
dd 1000h                      ; Section alignment
dd 200h                       ; File alignment
dw 6,0
dw 0,0
dw 6,0
dd 0
dd 4000h                      ; Image size
dd 400h                       ; Headers size
dd 0
dw IMAGE_SUBSYSTEM_WINDOWS_CUI
dw 0
dq 100000h
dq 1000h
dq 100000h
dq 1000h
dd 0
dd 6                          ; Data directories (Import table needed)

; Data directories (Import directory at index 1)
dd 0,0                        ; Export
dd 2000h, 1000h               ; Import RVA and size
dd 0,0                        ; Resource
dd 0,0                        ; Exception
dd 0,0                        ; Certificate
dd 0,0                        ; Base reloc (simplified: fixed image)

section_text:
db '.text',0,0,0
dd 0                          ; Virtual size
dd 1000h                      ; Virtual address
dd 0                          ; Raw size (filled at runtime)
dd 400h                       ; Raw address
dd 0,0
dw 0,0
dd 60000020h                  ; Characteristics: CODE, EXECUTE, READ

section_data:
db '.data',0,0,0
dd 0
dd 2000h
dd 0
dd 600h
dd 0,0
dw 0,0
dd C0000040h                  ; INITIALIZED_DATA, READ, WRITE

section_idata:
db '.idata',0,0
dd 0
dd 3000h
dd 0
dd 800h
dd 0,0
dw 0,0
dd C0000040h

; BSS section
.bss
align 8

hInput dq ?
hOutput dq ?
hSourceFile dq ?
hOutFile dq ?
lpSource dq ?
cbSource dq ?
iSource dq ?
iLine dd ?

lpTokens dq ?
nTokens dd ?
iToken dd ?

lpSymbols dq ?
nSymbols dd ?

lpAST dq ?
nASTNodes dd ?

lpCode dq ?
cbCode dq ?
lpData dq ?
cbData dq ?
lpImport dq ?
cbImport dq ?

temp_str db 512 dup(?)
current_func dd ?
stack_offset dd ?
label_count dd ?

; Code section
.code
align 16

; ============================================
; Utility Functions
; ============================================
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

print_line proc
    call print
    lea rcx, sz_crlf
    call print
    ret
print_line endp

print_num proc
    push rbx
    push rsi
    mov rax, rcx
    lea rsi, temp_str+63
    mov byte ptr [rsi], 0
    mov ebx, 10
L2: xor rdx, rdx
    div rbx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test rax, rax
    jnz L2
    mov rcx, rsi
    call print
    pop rsi
    pop rbx
    ret
print_num endp

memcpy proc
    mov rsi, rdx
    mov rdi, rcx
    mov rcx, r8
    rep movsb
    ret
memcpy endp

memset proc
    mov rdi, rcx
    mov al, dl
    mov rcx, r8
    rep stosb
    ret
memcmp endp

atoi proc
    xor rax, rax
    mov rsi, rcx
    mov r8, 10
L3: movzx rdx, byte ptr [rsi]
    test dl, dl
    jz D3
    cmp dl, '0'
    jb D3
    cmp dl, '9'
    ja D3
    sub dl, '0'
    mul r8
    add rax, rdx
    inc rsi
    jmp L3
D3: ret
atoi endp

strcpy proc
    mov rsi, rdx
    mov rdi, rcx
L4: mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz L4
    ret
strcpy endp

strcmp proc
    mov rsi, rcx
    mov rdi, rdx
L5: mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne D5
    test al, al
    jz E5
    inc rsi
    inc rdi
    jmp L5
D5: sub al, bl
    movsx rax, al
    ret
E5: xor rax, rax
    ret
strcmp endp

; ============================================
; Lexer (Enhanced)
; ============================================
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

peek_next proc
    mov rax, iSource
    inc rax
    cmp rax, cbSource
    jae Eof2
    mov rcx, lpSource
    movzx eax, byte ptr [rcx+rax]
    ret
Eof2: mov eax, -1
    ret
peek_next endp

next proc
    inc iSource
    ret
next endp

skip_ws proc
L6: call peek
    cmp eax, ' '
    je W1
    cmp eax, 9
    je W1
    cmp eax, 13
    je W1
    ret
W1: call next
    jmp L6
skip_ws endp

add_token proc
    push rbx
    push rsi
    push rdi
    mov ebx, nTokens
    shl ebx, 5
    mov rdi, lpTokens
    add rdi, rbx
    mov [rdi], ecx
    mov [rdi+4], iLine
    mov [rdi+8], rdx
    test r8, r8
    jz N1
    lea rdi, [rdi+16]
    mov rsi, r8
    mov ecx, 16
    rep movsb
N1: inc nTokens
    pop rdi
    pop rsi
    pop rbx
    ret
add_token endp

check_kw proc
    push rbx
    push rsi
    push rdi
    push r12
    mov rsi, rcx
    lea rbx, kw_list
    xor r12, r12
    mov r12, 10
    
C1: mov rdi, rbx
    mov rcx, 14
    call strcmp
    test rax, rax
    jz Found
    add rbx, 14
    inc r12
    cmp r12, 10
    jb C1
    
    xor eax, eax
    jmp KwD
    
Found:
    lea rax, kw_ids
    add rax, r12
    movzx eax, byte ptr [rax]
    
KwD:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
check_kw endp

lex proc
    mov nTokens, 0
    mov iLine, 1
    
L7: call skip_ws
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
    jmp L7

StrT:
    call next
    mov r12, iSource
L8: call peek
    cmp eax, '"'
    je StrE
    cmp eax, -1
    je StrE
    cmp eax, '\'
    jne S1
    call next
    call peek
    cmp eax, 'n'
    jne S2
    mov eax, 10
    jmp S3
S2: cmp eax, 't'
    jne S3
    mov eax, 9
S3: mov rbx, lpSource
    add rbx, iSource
    mov byte ptr [rbx], al
S1: call next
    jmp L8
StrE:
    mov rcx, lpSource
    add rcx, r12
    mov rdx, iSource
    sub rdx, r12
    mov byte ptr [rcx+rdx], 0
    mov ecx, T_STR
    xor edx, edx
    mov r8, rcx
    call add_token
    call next
    jmp L7

IdT:
    mov r12, iSource
L9: call peek
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
    call next
    jmp L9
IdE:
    mov rcx, lpSource
    add rcx, r12
    mov rbx, iSource
    sub rbx, r12
    mov byte ptr [rcx+rbx], 0
    call check_kw
    test eax, eax
    jnz IsKw
    mov ecx, T_ID
    mov rdx, r12
    mov r8, lpSource
    add r8, r12
    call add_token
    jmp L7
IsKw:
    add eax, 100
    mov ecx, eax
    xor edx, edx
    mov r8, lpSource
    add r8, r12
    call add_token
    jmp L7

NumT:
    mov r12, iSource
L10: call peek
    cmp eax, '0'
    jb NumE
    cmp eax, '9'
    ja NumE
    call next
    jmp L10
NumE:
    mov rcx, lpSource
    add rcx, r12
    mov rbx, iSource
    sub rbx, r12
    mov byte ptr [rcx+rbx], 0
    call atoi
    mov ecx, T_NUM
    mov rdx, rax
    xor r8, r8
    call add_token
    jmp L7

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
    cmp eax, '!'
    je Tne
    call next
    jmp L7

Tlp: mov ecx, T_LP
    jmp Taddn
Trp: mov ecx, T_RP
    jmp Taddn
Tlb: mov ecx, T_LB
    jmp Taddn
Trb: mov ecx, T_RB
    jmp Taddn
Tpl: mov ecx, T_PLUS
    jmp Taddn
Tmu: mov ecx, T_MUL
    jmp Taddn
Tco: mov ecx, T_COLON
    jmp Taddn
Tcm: mov ecx, T_COMMA
    jmp Taddn
Tgt: 
    call next
    call peek
    cmp eax, '='
    je Tge
    mov ecx, T_GT
    jmp Tadd
Tge:
    call next
    mov ecx, T_GE
    jmp Tadd
Tlt:
    call next
    call peek
    cmp eax, '='
    je Tle
    mov ecx, T_LT
    jmp Tadd
Tle:
    call next
    mov ecx, T_LE
    jmp Tadd
Tne:
    call next
    call peek
    cmp eax, '='
    je Tneq
    jmp L7
Tneq:
    call next
    mov ecx, T_NE
    jmp Tadd
Tmi:
    call next
    call peek
    cmp eax, '>'
    je Tar
    mov ecx, T_MINUS
    jmp Tadd
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
    jmp Tadd
Cmt:
    call next
CmtL:
    call peek
    cmp eax, 10
    je L7
    cmp eax, -1
    je L7
    call next
    jmp CmtL
Tas:
    call next
    call peek
    cmp eax, '='
    je Teq
    mov ecx, T_ASSIGN
    jmp Tadd
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
    jmp L7

EofT:
    mov ecx, T_EOF
    xor edx, edx
    xor r8, r8
    call add_token
    ret
lex endp

; ============================================
; Parser & AST
; ============================================
cur_tok proc
    mov eax, iToken
    shl eax, 5
    mov r10, lpTokens
    add rax, r10
    ret
cur_tok endp

cur_type proc
    call cur_tok
    mov eax, [rax]
    ret
cur_type endp

cur_line proc
    call cur_tok
    mov eax, [rax+4]
    ret
cur_line endp

cur_val proc
    call cur_tok
    mov rax, [rax+8]
    ret
cur_val endp

advance proc
    mov eax, iToken
    inc eax
    cmp eax, nTokens
    jae D7
    mov iToken, eax
D7: ret
advance endp

expect proc
    call cur_type
    cmp eax, ecx
    je E1
    push rcx
    lea rcx, sz_err_syntax
    call print
    call cur_line
    call print_num
    lea rcx, sz_crlf
    call print
    mov ecx, 1
    call ExitProcess
E1: jmp advance
expect endp

add_ast_node proc
    push rbx
    mov ebx, nASTNodes
    shl ebx, 5
    mov rax, lpAST
    add rax, rbx
    mov [rax], ecx        ; Type
    mov [rax+4], edx      ; Line
    mov [rax+8], r8       ; Data1
    mov [rax+16], r9      ; Data2
    mov eax, nASTNodes
    inc nASTNodes
    pop rbx
    ret
add_ast_node endp

parse_expr_precedence proto :word

parse_primary proc
    call cur_type
    cmp eax, T_NUM
    je Pnum
    cmp eax, T_STR
    je Pstr
    cmp eax, 100 + K_TRUE
    je Ptrue
    cmp eax, 100 + K_FALSE
    je Pfalse
    cmp eax, T_ID
    je Pid
    cmp eax, T_LP
    je Pparen
    cmp eax, T_MINUS
    je Punary
    
    lea rcx, sz_err_syntax
    call print
    call cur_line
    call print_num
    lea rcx, sz_crlf
    call print
    mov ecx, 1
    call ExitProcess
    
Pnum:
    call cur_val
    mov r8, rax
    mov ecx, AST_LIT_INT
    call cur_line
    mov edx, eax
    xor r9, r9
    call add_ast_node
    call advance
    ret
    
Pstr:
    call cur_tok
    lea r8, [rax+16]
    mov ecx, AST_LIT_STR
    call cur_line
    mov edx, eax
    xor r9, r9
    call add_ast_node
    call advance
    ret
    
Ptrue:
    mov ecx, AST_LIT_BOOL
    call cur_line
    mov edx, eax
    mov r8, 1
    xor r9, r9
    call add_ast_node
    call advance
    ret
    
Pfalse:
    mov ecx, AST_LIT_BOOL
    call cur_line
    mov edx, eax
    xor r8, r9
    xor r9, r9
    call add_ast_node
    call advance
    ret
    
Pid:
    call cur_tok
    lea r8, [rax+16]
    mov ecx, AST_IDENT
    call cur_line
    mov edx, eax
    xor r9, r9
    call add_ast_node
    call advance
    call cur_type
    cmp eax, T_LP
    je Pcall
    ret
    
Pcall:
    call advance
    mov r12, rax
    mov r13, 0
Pargs:
    call cur_type
    cmp eax, T_RP
    je Pcall_end
    cmp r13, 0
    je Parg1
    mov ecx, T_COMMA
    call expect
Parg1:
    inc r13
    push r12
    push r13
    mov ecx, 0
    call parse_expr_precedence
    pop r13
    pop r12
    jmp Pargs
    
Pcall_end:
    mov ecx, T_RP
    call expect
    mov ecx, AST_CALL
    mov edx, r12d
    mov r8, r12
    mov r9, r13
    call add_ast_node
    ret
    
Pparen:
    mov ecx, T_LP
    call expect
    mov ecx, 0
    call parse_expr_precedence
    mov ecx, T_RP
    call expect
    ret
    
Punary:
    mov ecx, T_MINUS
    call expect
    mov ecx, AST_UNARY
    call cur_line
    mov edx, eax
    mov r8b, '-'
    xor r9, r9
    call add_ast_node
    push rax
    mov ecx, 0
    call parse_expr_precedence
    pop rcx
    mov rdx, lpAST
    shl rcx, 5
    add rdx, rcx
    mov [rdx+16], rax
    mov rax, rcx
    shr rax, 5
    ret
parse_primary endp

parse_expr_precedence proc min_prec:word
    local left:dword
    call parse_primary
    mov left, eax
    
L11:
    call cur_type
    cmp eax, T_EOF
    je D11
    cmp eax, T_NL
    je D11
    cmp eax, T_RP
    je D11
    cmp eax, T_COMMA
    je D11
    
    mov ecx, eax
    cmp eax, T_PLUS
    je Bin
    cmp eax, T_MINUS
    je Bin
    cmp eax, T_MUL
    je Bin
    cmp eax, T_DIV
    je Bin
    cmp eax, T_EQ
    je Bin
    cmp eax, T_LT
    je Bin
    cmp eax, T_GT
    je Bin
    cmp eax, T_LE
    je Bin
    cmp eax, T_GE
    je Bin
    cmp eax, T_NE
    je Bin
    jmp D11
    
Bin:
    call advance
    push rcx
    mov ecx, 0
    call parse_expr_precedence
    pop rdx
    
    push rax
    mov ecx, AST_BINOP
    call cur_line
    mov edx, eax
    mov r8, rdx
    mov r9, rax
    call add_ast_node
    mov rbx, lpAST
    shl rax, 5
    add rbx, rax
    mov ecx, left
    mov [rbx+8], ecx
    pop rcx
    mov [rbx+16], ecx
    mov left, eax
    shr rax, 5
    jmp L11
    
D11:
    mov eax, left
    ret
parse_expr_precedence endp

parse_expr proc
    mov ecx, 0
    call parse_expr_precedence
    ret
parse_expr endp

parse_block proto

parse_stmt proc
    call cur_type
    cmp eax, 100 + K_LET
    je Slet
    cmp eax, 100 + K_IF
    je Sif
    cmp eax, 100 + K_WHILE
    je Swhile
    cmp eax, 100 + K_RETURN
    je Sreturn
    cmp eax, T_LB
    je Sblock
    
    call parse_expr
    push rax
    call cur_type
    cmp eax, T_ASSIGN
    je Sassign
    pop rax
    mov ecx, AST_RETURN
    call cur_line
    mov edx, eax
    mov r8, rax
    xor r9, r9
    call add_ast_node
    ret
    
Sassign:
    call advance
    call parse_expr
    pop rbx
    mov ecx, AST_ASSIGN
    call cur_line
    mov edx, eax
    mov r8, rbx
    mov r9, rax
    call add_ast_node
    ret
    
Slet:
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err
    call cur_tok
    lea r8, [rax+16]
    push r8
    call advance
    call cur_type
    cmp eax, T_COLON
    je Stype
    pop r8
    jmp Slet2
    
Stype:
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err
    call advance
    
Slet2:
    call cur_type
    cmp eax, T_ASSIGN
    jne ErrInit
    call advance
    call parse_expr
    pop r8
    push rax
    mov ecx, AST_LET
    call cur_line
    mov edx, eax
    mov r9, rax
    call add_ast_node
    pop rax
    mov rbx, lpAST
    shl rax, 5
    add rbx, rax
    mov [rbx+16], rax
    shr rax, 5
    ret
    
ErrInit:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    
Sif:
    call advance
    call parse_expr
    push rax
    call cur_type
    cmp eax, T_LB
    je Sif_block
    call parse_stmt
    jmp Sif_else
Sif_block:
    call parse_block
Sif_else:
    push rax
    call cur_type
    cmp eax, 100 + K_ELSE
    jne Sif_done
    call advance
    call parse_stmt
    pop rbx
    push rax
    mov r9, rax
    jmp Sif_build
Sif_done:
    xor r9, r9
Sif_build:
    pop r8
    pop rdx
    mov ecx, AST_IF
    call cur_line
    mov edx, eax
    call add_ast_node
    ret
    
Swhile:
    call advance
    call parse_expr
    push rax
    call parse_stmt
    pop r8
    mov ecx, AST_WHILE
    call cur_line
    mov edx, eax
    mov r9, rax
    call add_ast_node
    ret
    
Sreturn:
    call advance
    call cur_type
    cmp eax, T_NL
    je Sret_void
    cmp eax, T_RB
    je Sret_void
    call parse_expr
    mov r8, rax
    jmp Sret_build
Sret_void:
    mov r8, -1
Sret_build:
    mov ecx, AST_RETURN
    call cur_line
    mov edx, eax
    xor r9, r9
    call add_ast_node
    ret
    
Sblock:
    call parse_block
    ret
    
Err:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
parse_stmt endp

parse_block proc
    mov ecx, T_LB
    call expect
    mov r12, nASTNodes
    
L12:
    call cur_type
    cmp eax, T_RB
    je Bdone
    cmp eax, T_EOF
    je Bdone
    call parse_stmt
    call cur_type
    cmp eax, T_NL
    je Bnl
    cmp eax, T_RB
    je Bdone
    jmp L12
Bnl:
    call advance
    jmp L12
    
Bdone:
    mov ecx, T_RB
    call expect
    
    mov ecx, AST_BLOCK
    mov edx, r12d
    mov r8, r12
    mov r9, nASTNodes
    sub r9d, r12d
    call add_ast_node
    ret
parse_block endp

parse_fn proto

parse_top proc
    call cur_type
    cmp eax, T_EOF
    je Done
    cmp eax, 100 + K_FN
    je Pfn
    cmp eax, 100 + K_EXTERN
    je Pext
    cmp eax, T_NL
    je Pnl
    jmp Err2
    
Pnl:
    call advance
    jmp parse_top
    
Pfn:
    call parse_fn
    jmp parse_top
    
Pext:
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err2
    call cur_tok
    lea r8, [rax+16]
    call advance
    mov ecx, T_NL
    call expect
    jmp parse_top
    
Err2:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
    
Done:
    ret
parse_top endp

parse_fn proc
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err3
    call cur_tok
    lea r12, [rax+16]
    mov r13d, iLine
    call advance
    
    mov ecx, T_LP
    call expect
    
    xor r14, r14
Pparams:
    call cur_type
    cmp eax, T_RP
    je Pparams_done
    cmp r14, 0
    je Pp1
    mov ecx, T_COMMA
    call expect
Pp1:
    call cur_type
    cmp eax, T_ID
    jne Err3
    call advance
    call cur_type
    cmp eax, T_COLON
    je Ptype
    jmp Pnext
Ptype:
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err3
    call advance
Pnext:
    inc r14
    jmp Pparams
    
Pparams_done:
    mov ecx, T_RP
    call expect
    
    call cur_type
    cmp eax, T_ARROW
    jne Pno_ret
    call advance
    call cur_type
    cmp eax, T_ID
    jne Err3
    call advance
Pno_ret:

    call parse_stmt
    
    mov ecx, AST_FN
    mov edx, r13d
    mov r8, r12
    mov r9, rax
    call add_ast_node
    ret
    
Err3:
    lea rcx, sz_err_syntax
    call print
    mov ecx, 1
    call ExitProcess
parse_fn endp

; ============================================
; Code Generator (x64 Machine Code)
; ============================================
emit_byte proc
    mov rax, lpCode
    add rax, cbCode
    mov [rax], cl
    inc cbCode
    ret
emit_byte endp

emit_word proc
    mov rax, lpCode
    add rax, cbCode
    mov [rax], cx
    add cbCode, 2
    ret
emit_word endp

emit_dword proc
    mov rax, lpCode
    add rax, cbCode
    mov [rax], ecx
    add cbCode, 4
    ret
emit_dword endp

emit_qword proc
    mov rax, lpCode
    add rax, cbCode
    mov [rax], rcx
    add cbCode, 8
    ret
emit_qword endp

emit_push_imm32 proc
    mov al, 68h
    mov cl, al
    call emit_byte
    mov ecx, ecx
    call emit_dword
    ret
emit_push_imm32 endp

emit_mov_reg_imm32 proc
    ; rcx=reg(0-7), edx=imm32
    mov al, 0B8h
    add al, cl
    mov cl, al
    call emit_byte
    mov ecx, edx
    call emit_dword
    ret
emit_mov_reg_imm32 endp

emit_mov_rax_imm64 proc
    ; rdx=imm64
    mov cl, 48h
    call emit_byte
    mov cl, 0B8h
    call emit_byte
    mov rax, rdx
    call emit_qword
    ret
emit_mov_rax_imm64 endp

emit_call_rax proc
    mov cl, 0FFh
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    ret
emit_call_rax endp

emit_sub_rsp proc
    ; ecx=amount (aligned to 16)
    add ecx, 15
    and ecx, 0FFFFFFF0h
    cmp ecx, 128
    ja big_sub
    
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0ECh
    call emit_byte
    mov cl, byte ptr [rsp+28]
    call emit_byte
    ret
    
big_sub:
    mov cl, 48h
    call emit_byte
    mov cl, 81h
    call emit_byte
    mov cl, 0ECh
    call emit_byte
    call emit_dword
    ret
emit_sub_rsp endp

emit_add_rsp proc
    add ecx, 15
    and ecx, 0FFFFFFF0h
    cmp ecx, 128
    ja big_add
    
    mov cl, 48h
    call emit_byte
    mov cl, 83h
    call emit_byte
    mov cl, 0C4h
    call emit_byte
    mov cl, byte ptr [rsp+28]
    call emit_byte
    ret
    
big_add:
    mov cl, 48h
    call emit_byte
    mov cl, 81h
    call emit_byte
    mov cl, 0C4h
    call emit_byte
    call emit_dword
    ret
emit_add_rsp endp

emit_prolog proc
    mov cl, 55h                 ; push rbp
    call emit_byte
    mov cl, 48h                 ; mov rbp, rsp
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 0E5h
    call emit_byte
    ret
emit_prolog endp

emit_epilog proc
    mov cl, 5Dh                 ; pop rbp
    call emit_byte
    mov cl, 0C3h                ; ret
    call emit_byte
    ret
emit_epilog endp

gen_new_label proc
    mov eax, label_count
    inc label_count
    ret
gen_new_label endp

find_symbol proc
    ; rcx=name ptr, returns index or -1
    push rsi
    push rdi
    push rbx
    mov rsi, rcx
    xor rbx, rbx
L13:
    cmp ebx, nSymbols
    jae NotFound
    
    mov rax, lpSymbols
    shl rbx, 5
    add rax, rbx
    shr rbx, 5
    
    lea rdi, [rax+8]
    push rsi
    push rdi
    mov rcx, rsi
    mov rdx, rdi
    call strcmp
    test rax, rax
    pop rdi
    pop rsi
    jz Found2
    
    inc rbx
    jmp L13
    
Found2:
    mov eax, ebx
    pop rbx
    pop rdi
    pop rsi
    ret
    
NotFound:
    mov eax, -1
    pop rbx
    pop rdi
    pop rsi
    ret
find_symbol endp

add_symbol proc
    ; rcx=name, edx=type, r8=value/offset
    push rbx
    mov ebx, nSymbols
    shl ebx, 5
    mov rax, lpSymbols
    add rax, rbx
    
    mov [rax], edx
    mov [rax+4], r8d
    lea rdi, [rax+8]
    mov rsi, rcx
    call strcpy
    
    mov eax, nSymbols
    inc nSymbols
    pop rbx
    ret
add_symbol endp

codegen_node proto :dword

codegen_block proc start_idx:dword, count:dword
    local i:dword
    mov i, 0
    
L14:
    mov eax, i
    cmp eax, count
    jge D14
    
    mov ebx, start_idx
    add ebx, eax
    
    mov rax, lpAST
    shl rbx, 5
    add rax, rbx
    mov ecx, [rax]
    cmp ecx, AST_FN
    je Skip
    
    push count
    push start_idx
    mov ecx, ebx
    shr rbx, 5
    call codegen_node
    pop start_idx
    pop count
    
Skip:
    inc i
    jmp L14
    
D14:
    ret
codegen_block endp

codegen_node proc node_idx:dword
    local node:qword, type:dword, line:dword
    local left:dword, right:dword, value:qword
    
    mov eax, node_idx
    shl eax, 5
    add rax, lpAST
    mov node, rax
    
    mov ecx, [rax]
    mov type, ecx
    mov ecx, [rax+4]
    mov line, ecx
    
    cmp type, AST_LIT_INT
    je G_int
    cmp type, AST_LIT_BOOL
    je G_bool
    cmp type, AST_LIT_STR
    je G_str
    cmp type, AST_IDENT
    je G_ident
    cmp type, AST_BINOP
    je G_binop
    cmp type, AST_CALL
    je G_call
    cmp type, AST_LET
    je G_let
    cmp type, AST_ASSIGN
    je G_assign
    cmp type, AST_IF
    je G_if
    cmp type, AST_WHILE
    je G_while
    cmp type, AST_RETURN
    je G_return
    cmp type, AST_BLOCK
    je G_block
    cmp type, AST_FN
    je G_fn
    ret
    
G_int:
    mov rax, node
    mov rcx, [rax+8]
    mov rdx, rcx
    cmp rdx, 0
    jge G_int_pos
    mov cl, 48h
    call emit_byte
    mov cl, 0C7h
    call emit_byte
    mov cl, 0C0h
    call emit_byte
    mov ecx, edx
    call emit_dword
    ret
G_int_pos:
    mov cl, 0
    call emit_mov_reg_imm32
    ret
    
G_bool:
    mov rax, node
    mov rcx, [rax+8]
    mov cl, 0
    mov edx, ecx
    call emit_mov_reg_imm32
    ret
    
G_str:
    ; Store string in data section, load address
    mov rax, node
    mov rsi, [rax+8]
    call strlen
    inc rax
    
    mov rbx, lpData
    add rbx, cbData
    
    mov rdi, rbx
    mov rcx, rsi
    mov rdx, rax
    call memcpy
    
    mov rax, node
    mov rsi, [rax+8]
    call strlen
    inc rax
    add cbData, rax
    
    mov rdx, 140000000h + 2000h
    add rdx, rbx
    sub rdx, lpData
    mov cl, 0
    call emit_mov_rax_imm64
    ret
    
G_ident:
    mov rax, node
    mov rcx, [rax+8]
    call find_symbol
    cmp eax, -1
    jne G_ident_found
    
    lea rcx, sz_err_undef
    call print
    mov rax, node
    mov rcx, [rax+8]
    call print_line
    mov ecx, 1
    call ExitProcess
    
G_ident_found:
    shl eax, 5
    add rax, lpSymbols
    mov ecx, [rax+4]
    neg ecx
    mov cl, 48h
    call emit_byte
    mov cl, 8Bh
    call emit_byte
    mov cl, 85h
    call emit_byte
    call emit_dword
    ret
    
G_binop:
    mov rax, node
    mov ecx, [rax+8]
    push node
    call codegen_node
    pop node
    
    mov cl, 50h                 ; push rax
    call emit_byte
    
    mov rax, node
    mov ecx, [rax+16]
    push node
    call codegen_node
    pop node
    
    mov cl, 5Ah                 ; pop rdx
    call emit_byte
    
    mov rax, node
    mov eax, [rax+8]
    cmp eax, T_PLUS
    je G_add
    cmp eax, T_MINUS
    je G_sub
    cmp eax, T_MUL
    je G_mul
    cmp eax, T_DIV
    je G_div
    ret
    
G_add:
    mov cl, 48h
    call emit_byte
    mov cl, 01h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    ret
G_sub:
    mov cl, 48h
    call emit_byte
    mov cl, 29h
    call emit_byte
    mov cl, 0D0h
    call emit_byte
    ret
G_mul:
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0E2h
    call emit_byte
    ret
G_div:
    mov cl, 48h
    call emit_byte
    mov cl, 99h
    call emit_byte
    mov cl, 48h
    call emit_byte
    mov cl, 0F7h
    call emit_byte
    mov cl, 0F8h
    call emit_byte
    ret
    
G_call:
    mov rax, node
    mov r12, [rax+16]           ; Arg count
    mov r13, [rax+8]            ; Name
    
    ; Generate args right-to-left
    mov r14, r12
    dec r14
G_args_loop:
    cmp r14, 0
    jl G_args_done
    
    mov rax, node
    mov ebx, [rax+8]
    shl rbx, 5
    add rbx, lpAST
    
    ; Find arg node in block...
    
    dec r14
    jmp G_args_loop
    
G_args_done:
    ; Call
    mov rcx, r13
    call find_symbol
    
    ; For now, assume external
    mov rdx, r13
    call emit_mov_rax_imm64
    call emit_call_rax
    
    ret
    
G_let:
    mov rax, node
    mov rcx, [rax+8]            ; Name
    mov r12, rcx
    
    mov ecx, [rax+16]
    push r12
    call codegen_node
    pop r12
    
    ; Allocate stack space
    sub stack_offset, 8
    
    ; Store rax to stack
    mov cl, 48h
    call emit_byte
    mov cl, 89h
    call emit_byte
    mov cl, 85h
    call emit_byte
    mov ecx, stack_offset
    call emit_dword
    
    ; Add symbol
    mov rcx, r12
    mov edx, 1
    mov r8d, stack_offset
    call add_symbol
    
    ret
    
G_assign:
    ret
    
G_if:
    ret
    
G_while:
    ret
    
G_return:
    mov rax, node
    mov ecx, [rax+8]
    cmp ecx, -1
    je G_ret_void
    
    call codegen_node
    
G_ret_void:
    mov ecx, stack_offset
    neg ecx
    call emit_add_rsp
    call emit_epilog
    ret
    
G_block:
    mov rax, node
    mov ecx, [rax+8]            ; Start
    mov edx, [rax+16]           ; Count
    call codegen_block
    ret
    
G_fn:
    mov rax, node
    mov r12, [rax+8]            ; Name
    mov r13d, [rax+16]          ; Body node
    
    ; Reset stack for function
    mov stack_offset, 0
    
    call emit_prolog
    
    mov ecx, 32
    call emit_sub_rsp
    
    mov ecx, r13d
    call codegen_node
    
    ; Ensure return
    mov ecx, stack_offset
    neg ecx
    call emit_add_rsp
    call emit_epilog
    
    ret
codegen_node endp

; ============================================
; PE Writer
; ============================================
write_pe proc
    local headers[1024]:byte, written:qword
    
    ; Calculate sizes
    mov rax, cbCode
    add rax, 511
    and rax, 0FFFFFFFFFFFFFE00h
    mov text_size, eax
    
    mov rax, cbData
    add rax, cbImport
    add rax, 511
    and rax, 0FFFFFFFFFFFFFE00h
    mov data_size, eax
    
    ; Build headers
    lea rdi, headers
    mov rsi, offset dos_header
    mov rcx, 64
    rep movsb
    
    add rdi, 16
    mov rsi, offset pe_sig
    movsq
    
    mov rsi, offset file_header
    mov rcx, 20
    rep movsb
    
    mov rsi, offset optional_header
    mov rcx, 240
    rep movsb
    
    ; Patch section sizes
    lea rdi, headers
    add rdi, 512              ; .text header location
    mov eax, text_size
    mov [rdi+8], eax          ; Virtual size
    mov [rdi+16], eax         ; Raw size
    
    add rdi, 40               ; .data header
    mov eax, data_size
    mov [rdi+8], eax
    
    add rdi, 40               ; .idata header
    mov [rdi+8], eax
    
    ; Create file
    lea rcx, out_path
    mov edx, GENERIC_WRITE
    xor r8, r8
    mov r9, CREATE_ALWAYS
    push 0
    push FILE_ATTRIBUTE_NORMAL
    call CreateFileW
    mov hOutFile, rax
    
    ; Write headers
    lea rcx, headers
    mov edx, 1024
    call write_file
    
    ; Write .text
    mov rcx, lpCode
    mov edx, text_size
    call write_file
    
    ; Write .data + .idata
    mov rcx, lpData
    mov edx, data_size
    call write_file
    
    ; Close
    mov rcx, hOutFile
    call CloseHandle
    
    ret
write_pe endp

write_file proc buf:qword, size:dword
    local written:qword
    sub rsp, 40
    mov rcx, hOutFile
    mov rdx, buf
    mov r8d, size
    lea r9, written
    push 0
    call WriteFile
    add rsp, 48
    ret
write_file endp

; ============================================
; Command Line & Main
; ============================================
get_cmdline_file proc
    call GetCommandLineA
    
    ; Skip program name
    mov rsi, rax
    mov al, [rsi]
    cmp al, '"'
    je SkipQuoted
    
SkipSpace:
    mov al, [rsi]
    test al, al
    jz NoFile
    cmp al, ' '
    je FoundSpace
    inc rsi
    jmp SkipSpace
    
FoundSpace:
    inc rsi
    mov al, [rsi]
    cmp al, ' '
    je FoundSpace
    test al, al
    jz NoFile
    mov rax, rsi
    ret
    
SkipQuoted:
    inc rsi
    mov al, [rsi]
    test al, al
    jz NoFile
    cmp al, '"'
    jne SkipQuoted
    inc rsi
    jmp SkipSpace
    
NoFile:
    xor rax, rax
    ret
get_cmdline_file endp

read_source_file proc fname:qword
    local size:qword
    
    mov rcx, fname
    mov edx, GENERIC_READ
    xor r8, r8
    mov r9, OPEN_EXISTING
    push 0
    push FILE_ATTRIBUTE_NORMAL
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je Rf_err
    mov hSourceFile, rax
    
    lea r9, size
    call GetFileSizeEx
    
    mov rcx, 0
    mov rdx, size
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpSource, rax
    mov cbSource, size
    
    mov rcx, hSourceFile
    mov rdx, lpSource
    mov r8, size
    lea r9, temp_str
    push 0
    call ReadFile
    
    mov rcx, hSourceFile
    call CloseHandle
    
    mov rax, lpSource
    ret
    
Rf_err:
    xor rax, rax
    ret
read_source_file endp

_start proc
    ; Allocate buffers
    mov rcx, 0
    mov rdx, MAX_SRC
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpSource, rax
    
    mov rcx, 0
    mov rdx, MAX_TOK * 32
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpTokens, rax
    
    mov rcx, 0
    mov rdx, MAX_SYM * 32
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpSymbols, rax
    
    mov rcx, 0
    mov rdx, MAX_TOK * 32
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpAST, rax
    
    mov rcx, 0
    mov rdx, MAX_CODE
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpCode, rax
    
    mov rcx, 0
    mov rdx, MAX_DATA
    mov r8, MEM_COMMIT
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov lpData, rax
    
    ; Get input file
    call get_cmdline_file
    test rax, rax
    jnz HasFile
    
    lea rcx, sz_usage
    call print_line
    mov ecx, 1
    call ExitProcess
    
HasFile:
    ; Read file
    mov rcx, rax
    call read_source_file
    test rax, rax
    jnz HasSource
    
    lea rcx, sz_err_file
    call print_line
    mov ecx, 1
    call ExitProcess
    
HasSource:
    ; Lex
    lea rcx, sz_scan
    call print_line
    call lex
    
    ; Parse
    call parse_top
    
    ; Codegen
    lea rcx, sz_codegen
    call print_line
    mov nASTNodes, 0
    mov label_count, 0
    
    ; Find main and generate
    mov iToken, 0
L20:
    call cur_type
    cmp eax, T_EOF
    je DoneGen
    cmp eax, 100 + K_FN
    jne NextTok
    
    call advance
    call cur_type
    cmp eax, T_ID
    jne NextTok
    
    call cur_tok
    lea r8, [rax+16]
    lea rcx, temp_str
    mov rsi, r8
    call strcpy
    
    lea rcx, temp_str
    lea rdx, main_str
    call strcmp
    test rax, rax
    jne NextTok
    
    ; Generate this function
    mov ecx, iToken
    dec ecx
    call codegen_node
    
NextTok:
    call advance
    jmp L20
    
DoneGen:
    ; Write output
    lea rcx, sz_link
    call print_line
    call write_pe
    
    lea rcx, sz_ok
    call print
    lea rcx, out_path
    call print_line
    
    xor ecx, ecx
    call ExitProcess
    
_start endp

main_str db 'main',0
out_path dw 'o','u','t','.','e','x','e',0
text_size dd ?
data_size dd ?

end
    align 8
    dos_header      db 4Dh, 5Ah, 90h, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0FFh, 0FFh, 0, 0
                    db 0B8h, 0, 0, 0, 0, 0, 0, 0, 40h, 0, 0, 0, 0, 0, 0, 0
                    times 32 db 0
                    db 80h, 0, 0, 0 ; Offset to PE Signature
    
    nt_header       db "PE", 0, 0, 0 ; Signature
                    dw 8664h         ; Machine (x64)
                    dw 2             ; Number of sections
                    dd 0             ; Timestamp
                    dd 0             ; Symbol table offset
                    dd 0             ; Number of symbols
                    dw 0F0h          ; Optional header size
                    dw 22h           ; Characteristics
    
    opt_header      dw 20Bh          ; Magic (PE32+)
                    db 0, 1          ; Linker version
                    dd 0             ; Code size
                    dd 0             ; Data size
                    dd 0             ; BSS size
                    dd 1000h         ; Entry point RVA
                    dd 1000h         ; Code base
                    dq 400000h       ; Image base
                    dd 1000h         ; Section alignment
                    dd 200h          ; File alignment
                    dw 6, 0          ; OS version
                    dw 0, 0          ; Image version
                    dw 6, 0          ; Subsystem version
                    dd 0             ; Win32 version
                    dd 3000h         ; Image size
                    dd 400h          ; Headers size
                    dd 0             ; Checksum
                    dw 3             ; Subsystem (console)
                    dw 0             ; DLL characteristics
                    dq 100000h       ; Stack reserve
                    dq 1000h         ; Stack commit
                    dq 100000h       ; Heap reserve
                    dq 1000h         ; Heap commit
                    dd 0             ; Loader flags
                    dd 16            ; Data directories
                    times 128 db 0   ; Data directories
    
    text_section    db ".text", 0, 0, 0
                    dd 0             ; Virtual size
                    dd 2000h         ; Virtual address
                    dd 0             ; Raw size
                    dd 400h          ; Raw address
                    dd 0             ; Relocations
                    dd 0             ; Line numbers
                    dw 0, 0          ; Counts
                    dd 60000020h     ; Characteristics
    
    data_section    db ".data", 0, 0, 0
                    dd 0             ; Virtual size
                    dd 3000h         ; Virtual address
                    dd 0             ; Raw size
                    dd 600h          ; Raw address
                    dd 0             ; Relocations
                    dd 0             ; Line numbers
                    dw 0, 0          ; Counts
                    dd 0C0000040h    ; Characteristics

.bss
    hFile           dq ?
    bytesWritten    dd ?
    textBuffer      db 4096 dup(?)
    dataBuffer      db 4096 dup(?)
    textSize        dd ?
    dataSize        dd ?

.code
; --- Utility: Write Byte to Buffer ---
; RCX: Buffer ptr, RDX: Offset, R8: Byte
write_byte proc
    mov rax, rcx
    add rax, rdx
    mov [rax], r8b
    ret
write_byte endp

; --- Emit x64 Opcode ---
; RCX: Opcode, RDX: ModRM (if any)
emit_x64 proc
    ; Simple emitter for basic ops
    mov rax, textSize
    lea rcx, textBuffer
    call write_byte
    inc textSize
    test rdx, rdx
    jz @@done
    mov r8, rdx
    mov rdx, rax
    inc rdx
    call write_byte
    inc textSize
@@done:
    ret
emit_x64 endp

; --- Parse TerraForm Syntax (Simplified) ---
; This is a basic parser for demonstration
parse_terraform proc
    ; Placeholder: Emit a simple "ret" instruction
    mov rcx, 0C3h  ; RET opcode
    xor rdx, rdx
    call emit_x64
    ret
parse_terraform endp

; --- Write PE to File ---
write_pe proc
    sub rsp, 40
    
    ; Create file
    lea rcx, file_name
    mov edx, 40000000h  ; GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 2  ; CREATE_ALWAYS
    call CreateFileA
    mov hFile, rax
    cmp rax, -1
    je @@error
    
    ; Write DOS header
    mov rcx, hFile
    lea rdx, dos_header
    mov r8d, 64
    lea r9, bytesWritten
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write NT header
    mov rcx, hFile
    lea rdx, nt_header
    mov r8d, 24 + 240 + 80  ; NT + Opt + Sections
    lea r9, bytesWritten
    call WriteFile
    
    ; Pad to 0x400
    mov rcx, hFile
    mov edx, 400h
    xor r8, r8
    xor r9, r9
    call SetFilePointer
    
    ; Write .text
    mov rcx, hFile
    lea rdx, textBuffer
    mov r8d, textSize
    lea r9, bytesWritten
    call WriteFile
    
    ; Pad to 0x600
    mov rcx, hFile
    mov edx, 600h
    xor r8, r8
    xor r9, r9
    call SetFilePointer
    
    ; Write .data
    mov rcx, hFile
    lea rdx, dataBuffer
    mov r8d, dataSize
    lea r9, bytesWritten
    call WriteFile
    
    ; Close
    mov rcx, hFile
    call CloseHandle
    
    ; Success message
    lea rcx, msg_success
    call print
    jmp @@exit
    
@@error:
    lea rcx, msg_error
    call print
    
@@exit:
    add rsp, 40
    ret
write_pe endp

; --- Print String ---
print proc
    mov r12, rcx
    call strlen
    mov rdx, r12
    mov r8, rax
    mov ecx, -11
    call GetStdHandle
    mov rcx, rax
    mov r9, 0
    push 0
    call WriteConsoleA
    add rsp, 8
    ret
print endp

strlen proc
    xor rax, rax
@@l: cmp byte ptr [rcx+rax], 0
    je @@d
    inc rax
    jmp @@l
@@d: ret
strlen endp

; --- Main Entry ---
main proc
    sub rsp, 40
    
    ; Parse TerraForm (placeholder)
    call parse_terraform
    
    ; Write PE
    call write_pe
    
    ; Exit
    xor ecx, ecx
    call ExitProcess
main endp

end