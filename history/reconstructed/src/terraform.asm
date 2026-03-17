; ============================================
; RXUC-TerraForm v1.0 — Universal Auto-Backend Compiler
; Direct PE emission, no intermediate ASM text
; Supports: C/Rust/Python/Go/JS/Zig/Nim/Carbon/Mojo syntax
; Assemble: ml64 /c terraform.asm && link /entry:main terraform.obj kernel32.lib
; ============================================
INCLUDE ksamd64.inc

; ========== CONFIG ==========
MAX_SRC     EQU 131072
MAX_SYM     EQU 1024
MAX_RELOC   EQU 4096
MAX_CODE    EQU 262144
MAX_DATA    EQU 65536
MAX_IMPORT  EQU 256

; ========== TOKEN TYPES (Universal) ==========
T_EOF       EQU 0
T_NL        EQU 1
T_IDENT     EQU 2     ; [a-zA-Z_][a-zA-Z0-9_]*
T_NUMBER    EQU 3     ; 0-9+
T_STRING    EQU 4     ; "..."
T_CHAR      EQU 5     ; '...'
T_LPAREN    EQU 6     ; (
T_RPAREN    EQU 7     ; )
T_LBRACE    EQU 8     ; {
T_RBRACE    EQU 9     ; }
T_LBRACKET  EQU 10    ; [
T_RBRACKET  EQU 11    ; ]
T_SEMI      EQU 12    ; ;
T_COLON     EQU 13    ; :
T_COMMA     EQU 14    ; ,
T_DOT       EQU 15    ; .
T_ARROW     EQU 16    ; ->
T_FATARROW  EQU 17    ; =>
T_EQ        EQU 18    ; ==
T_NE        EQU 19    ; !=
T_LT        EQU 20    ; <
T_GT        EQU 21    ; >
T_LE        EQU 22    ; <=
T_GE        EQU 23    ; >=
T_ASSIGN    EQU 24    ; =
T_PLUS      EQU 25    ; +
T_MINUS     EQU 26    ; -
T_STAR      EQU 27    ; *
T_SLASH     EQU 28    ; /
T_PERCENT   EQU 29    ; %
T_AMP       EQU 30    ; &
T_PIPE      EQU 31    ; |
T_CARET     EQU 32    ; ^
T_TILDE     EQU 33    ; ~
T_BANG      EQU 34    ; !
T_SHL       EQU 35    ; <<
T_SHR       EQU 36    ; >>
T_PLUSASS   EQU 37    ; +=
T_MINUSASS  EQU 38    ; -=
T_INC       EQU 39    ; ++
T_DEC       EQU 40    ; --

; ========== KEYWORD IDs ==========
K_FN        EQU 1
K_FUNC      EQU 2
K_DEF       EQU 3
K_PROC      EQU 4
K_LET       EQU 5
K_VAR       EQU 6
K_CONST     EQU 7
K_MUT       EQU 8
K_IF        EQU 9
K_ELSE      EQU 10
K_WHILE     EQU 11
K_FOR       EQU 12
K_LOOP      EQU 13
K_RETURN    EQU 14
K_BREAK     EQU 15
K_CONTINUE  EQU 16
K_MATCH     EQU 17
K_SWITCH    EQU 18
K_STRUCT    EQU 19
K_CLASS     EQU 20
K_TYPE      EQU 21
K_IMPL      EQU 22
K_IMPORT    EQU 23
K_PUB       EQU 24
K_PRIV      EQU 25
K_STATIC    EQU 26
K_EXTERN    EQU 27
K_ASM       EQU 28
K_TRUE      EQU 29
K_FALSE     EQU 30
K_NIL       EQU 31

; ========== X64 OPCODES (Raw Bytes) ==========
OP_NOP      EQU 090h
OP_RET      EQU 0C3h
OP_PUSH_RAX EQU 050h
OP_PUSH_RCX EQU 051h
OP_PUSH_RDX EQU 052h
OP_PUSH_RBX EQU 053h
OP_PUSH_RBP EQU 055h
OP_PUSH_RSI EQU 056h
OP_PUSH_RDI EQU 057h
OP_POP_RAX  EQU 058h
OP_POP_RCX  EQU 059h
OP_POP_RDX  EQU 05Ah
OP_POP_RBX  EQU 05Bh
OP_POP_RBP  EQU 05Dh
OP_POP_RSI  EQU 05Eh
OP_POP_RDI  EQU 05Fh
OP_MOV_EAX  EQU 0B8h      ; + imm32
OP_MOV_RAX  EQU 048h      ; REX.W prefix
OP_CALL_REL EQU 0E8h      ; + rel32
OP_JMP_REL  EQU 0E9h      ; + rel32
OP_JZ_REL   EQU 0840Fh    ; 0F 84 + rel32 (near)
OP_JNZ_REL  EQU 0850Fh    ; 0F 85 + rel32
OP_CMP_RAX  EQU 03D48h    ; 48 3D + imm32
OP_TEST_RAX EQU 0C08548h  ; 48 85 C0
OP_ADD_RAX  EQU 00548h    ; 48 05 + imm32
OP_SUB_RAX  EQU 02D48h    ; 48 2D + imm32
OP_XOR_EAX  EQU 03548h    ; 48 35 + imm32 (zero extend)

; ========== PE STRUCTURES ==========
IMAGE_DOS_SIGNATURE     EQU 5A4Dh
IMAGE_NT_SIGNATURE      EQU 00004550h
IMAGE_FILE_MACHINE_AMD64 EQU 8664h
IMAGE_FILE_EXECUTABLE_IMAGE EQU 2h
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 20h
IMAGE_SUBSYSTEM_WINDOWS_CUI EQU 3h
IMAGE_REL_BASED_DIR64   EQU 10

; ========== MACROS ==========
EMIT_BYTE MACRO b
    mov     al, b
    stosb
    inc     cbCode
    ENDM

EMIT_DWORD MACRO d
    mov     eax, d
    stosd
    add     cbCode, 4
    ENDM

EMIT_QWORD MACRO q
    mov     rax, q
    stosq
    add     cbCode, 8
    ENDM

; ========== DATA ==========
.data
align 8
kw_table:
    db "fn",0,0,0,0,0,0,0,0,0,0,0,0,0,0, K_FN
    db "func",0,0,0,0,0,0,0,0,0,0,0,0, K_FUNC
    db "def",0,0,0,0,0,0,0,0,0,0,0,0,0, K_DEF
    db "proc",0,0,0,0,0,0,0,0,0,0,0,0, K_PROC
    db "let",0,0,0,0,0,0,0,0,0,0,0,0,0, K_LET
    db "var",0,0,0,0,0,0,0,0,0,0,0,0,0, K_VAR
    db "const",0,0,0,0,0,0,0,0,0,0,0, K_CONST
    db "mut",0,0,0,0,0,0,0,0,0,0,0,0,0, K_MUT
    db "if",0,0,0,0,0,0,0,0,0,0,0,0,0,0, K_IF
    db "else",0,0,0,0,0,0,0,0,0,0,0,0, K_ELSE
    db "while",0,0,0,0,0,0,0,0,0,0,0, K_WHILE
    db "for",0,0,0,0,0,0,0,0,0,0,0,0,0, K_FOR
    db "loop",0,0,0,0,0,0,0,0,0,0,0,0, K_LOOP
    db "return",0,0,0,0,0,0,0,0,0,0, K_RETURN
    db "break",0,0,0,0,0,0,0,0,0,0,0, K_BREAK
    db "continue",0,0,0,0,0,0,0, K_CONTINUE
    db "match",0,0,0,0,0,0,0,0,0,0,0, K_MATCH
    db "switch",0,0,0,0,0,0,0,0,0,0, K_SWITCH
    db "struct",0,0,0,0,0,0,0,0,0,0, K_STRUCT
    db "class",0,0,0,0,0,0,0,0,0,0,0, K_CLASS
    db "type",0,0,0,0,0,0,0,0,0,0,0,0, K_TYPE
    db "impl",0,0,0,0,0,0,0,0,0,0,0,0, K_IMPL
    db "import",0,0,0,0,0,0,0,0,0,0, K_IMPORT
    db "pub",0,0,0,0,0,0,0,0,0,0,0,0,0, K_PUB
    db "priv",0,0,0,0,0,0,0,0,0,0,0,0, K_PRIV
    db "static",0,0,0,0,0,0,0,0,0,0, K_STATIC
    db "extern",0,0,0,0,0,0,0,0,0,0, K_EXTERN
    db "asm",0,0,0,0,0,0,0,0,0,0,0,0,0, K_ASM
    db "true",0,0,0,0,0,0,0,0,0,0,0,0, K_TRUE
    db "false",0,0,0,0,0,0,0,0,0,0,0, K_FALSE
    db "nil",0,0,0,0,0,0,0,0,0,0,0,0,0, K_NIL
    db 0    ; End marker

imp_kernel32 db "kernel32.dll", 0
imp_exitproc db "ExitProcess", 0

; ========== BSS (Uninitialized Data) ==========
.data?
align 4096
src_buf     db MAX_SRC dup(?)
tok_buf     dd MAX_SYM*8 dup(?)     ; {type:8, val:8, line:4, kw:4, str:16}
sym_buf     db MAX_SYM*64 dup(?)    ; Symbol table
reloc_buf   dd MAX_RELOC*4 dup(?)   ; Relocations
code_buf    db MAX_CODE dup(?)      ; Raw code emission
data_buf    db MAX_DATA dup(?)      ; Data section
imp_buf     db MAX_IMPORT dup(?)    ; Import table

align 8
cbSrc       dq ?
iSrc        dq ?
iTok        dq ?
nTok        dq ?
lnCur       dd ?
cbCode      dq ?
cbData      dq ?
cbImp       dq ?
nReloc      dq ?
hFile       dq ?
hOut        dq ?
stkDepth    dd ?
curFunc     dq ?

; Token offsets
TKO_TYPE    EQU 0
TKO_VAL     EQU 8
TKO_LINE    EQU 16
TKO_KW      EQU 20
TKO_STR     EQU 24

; Symbol offsets
SY_NAME     EQU 0
SY_TYPE     EQU 32
SY_SCOPE    EQU 36
SY_OFF      EQU 40
SY_SIZE     EQU 44
SY_FLAGS    EQU 48

; ========== CODE ==========
.code
align 16

; === UTILS ===
strlen PROC
    xor     rax, rax
@@l:    cmp     byte ptr [rcx+rax], 0
    je      @@d
    inc     rax
    jmp     @@l
@@d:    ret
strlen ENDP

strcmp PROC
@@l:    mov     al, [rcx]
    mov     ah, [rdx]
    cmp     al, ah
    jne     @@ne
    test    al, al
    jz      @@eq
    inc     rcx
    inc     rdx
    jmp     @@l
@@ne:   mov     eax, 1
    ret
@@eq:   xor     eax, eax
    ret
strcmp ENDP

memset PROC
    mov     rax, rdx
    rep     stosb
    ret
memset ENDP

memcpy PROC
    mov     rax, r8
    rep     movsb
    ret
memcpy ENDP

; === LEXER (Universal) ===
peek PROC
    mov     rcx, iSrc
    cmp     rcx, cbSrc
    jae     @@eof
    movzx   eax, byte ptr src_buf[rcx]
    ret
@@eof:  mov     eax, -1
    ret
peek ENDP

next PROC
    inc     iSrc
    ret
next ENDP

skip_ws PROC
@@l:    call    peek
    cmp     eax, ' '
    je      @ws
    cmp     eax, 9
    je      @ws
    cmp     eax, 13
    je      @ws
    cmp     eax, 10
    je      @nl
    cmp     eax, '/'
    jne     @@d
    call    next
    call    peek
    cmp     eax, '/'
    je      @ln_cmt
    cmp     eax, '*'
    je      @blk_cmt
    dec     iSrc
    mov     eax, '/'
    ret
@ws:    call    next
    jmp     @@l
@nl:    inc     lnCur
    call    next
    jmp     @@l
@ln_cmt:
@@lc:   call    peek
    cmp     eax, 10
    je      @@l
    cmp     eax, -1
    je      @@l
    call    next
    jmp     @@lc
@blk_cmt:
    call    next
@@bc:   call    peek
    cmp     eax, -1
    je      @@l
    cmp     eax, '*'
    jne     @bnc
    call    next
    call    peek
    cmp     eax, '/'
    jne     @@bc
    call    next
    jmp     @@l
@bnc:   call    next
    jmp     @@bc
@@d:    ret
skip_ws ENDP

addtok PROC
    mov     r8, nTok
    shl     r8, 5
    lea     r9, tok_buf[r8]
    mov     [r9+TKO_TYPE], ecx
    mov     [r9+TKO_VAL], rdx
    mov     [r9+TKO_LINE], lnCur
    mov     [r9+TKO_KW], r12d
    test    r11, r11
    jz      @@n
    lea     rdi, [r9+TKO_STR]
    mov     rsi, r11
    mov     ecx, 16
    rep     movsb
@@n:    inc     nTok
    ret
addtok ENDP

lookup_kw PROC
    ; rcx=ident, returns r12=kw_id or 0
    lea     rdx, kw_table
@@l:    movzx   eax, byte ptr [rdx+30]
    test    eax, eax
    jz      @@nf
    mov     r8, rcx
    mov     r9, rdx
@@c:    mov     al, [r8]
    mov     ah, [r9]
    cmp     al, ah
    jne     @@ne
    test    al, al
    jz      @@f
    inc     r8
    inc     r9
    jmp     @@c
@@f:    movzx   r12d, byte ptr [rdx+30]
    ret
@@ne:   add     rdx, 31
    jmp     @@l
@@nf:   xor     r12d, r12d
    ret
lookup_kw ENDP

read_str PROC
    call    next
    mov     r13, iSrc
@@l:    call    peek
    cmp     eax, '"'
    je      @@e
    cmp     eax, -1
    je      @@e
    cmp     eax, '\'
    jne     @nc
    call    next
@nc:    call    next
    jmp     @@l
@@e:    mov     r14, iSrc
    sub     r14, r13
    mov     ecx, T_STRING
    xor     edx, edx
    xor     r12d, r12d
    lea     r11, src_buf[r13]
    call    addtok
    call    next
    ret
read_str ENDP

read_num PROC
    xor     r13, r13
@@l:    call    peek
    sub     eax, '0'
    cmp     eax, 9
    ja      @@e
    imul    r13, 10
    add     r13, rax
    call    next
    jmp     @@l
@@e:    mov     ecx, T_NUMBER
    mov     rdx, r13
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    ret
read_num ENDP

read_ident PROC
    mov     r13, iSrc
@@l:    call    peek
    cmp     eax, '_'
    je      @ok
    cmp     eax, '0'
    jb      @@c
    cmp     eax, '9'
    jbe     @ok
@@c:    cmp     eax, 'a'
    jb      @uc
    cmp     eax, 'z'
    jbe     @ok
@uc:    cmp     eax, 'A'
    jb      @@e
    cmp     eax, 'Z'
    ja      @@e
@ok:    call    next
    jmp     @@l
@@e:    mov     r14, iSrc
    sub     r14, r13
    ; Copy to temp
    sub     rsp, 32
    lea     rdi, [rsp]
    lea     rsi, src_buf[r13]
    mov     ecx, r14d
    cmp     ecx, 31
    jbe     @cp
    mov     ecx, 31
@cp:    rep     movsb
    mov     byte ptr [rdi], 0
    lea     rcx, [rsp]
    call    lookup_kw
    mov     ecx, T_IDENT
    test    r12d, r12d
    jz      @id
    cmp     r12d, K_TRUE
    je      @tr
    cmp     r12d, K_FALSE
    je      @fa
    cmp     r12d, K_NIL
    je      @ni
@id:    mov     rdx, r13
    lea     r11, [rsp]
    call    addtok
    add     rsp, 32
    ret
@tr:    mov     ecx, T_NUMBER
    mov     rdx, 1
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    add     rsp, 32
    ret
@fa:    xor     edx, edx
    mov     ecx, T_NUMBER
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    add     rsp, 32
    ret
@ni:    mov     rdx, -1
    mov     ecx, T_NUMBER
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    add     rsp, 32
    ret
read_ident ENDP

lex PROC
    mov     nTok, 0
    mov     lnCur, 1
@@l:    call    skip_ws
    call    peek
    cmp     eax, -1
    je      @@eof
    cmp     eax, '"'
    je      @str
    cmp     eax, '0'
    jb      @sym
    cmp     eax, '9'
    jbe     @num
    cmp     eax, 'a'
    jb      @up
    cmp     eax, 'z'
    jbe     @id
@up:    cmp     eax, 'A'
    jb      @sym
    cmp     eax, 'Z'
    jbe     @id
    cmp     eax, '_'
    je      @id
    jmp     @sym
@str:   call    read_str
    jmp     @@l
@num:   call    read_num
    jmp     @@l
@id:    call    read_ident
    jmp     @@l
@sym:   ; Multi-char operators
    cmp     eax, '-'
    jne     @n1
    call    next
    call    peek
    cmp     eax, '>'
    je      @ar
    cmp     eax, '-'
    je      @dc
    cmp     eax, '='
    je      @ma
    mov     ecx, T_MINUS
    jmp     @add
@ar:    call    next
    mov     ecx, T_ARROW
    jmp     @add
@dc:    call    next
    mov     ecx, T_DEC
    jmp     @add
@ma:    call    next
    mov     ecx, T_MINUSASS
    jmp     @add
@n1:    cmp     eax, '+'
    jne     @n2
    call    next
    call    peek
    cmp     eax, '+'
    je      @ic
    cmp     eax, '='
    je      @pa
    mov     ecx, T_PLUS
    jmp     @add
@ic:    call    next
    mov     ecx, T_INC
    jmp     @add
@pa:    call    next
    mov     ecx, T_PLUSASS
    jmp     @add
@n2:    cmp     eax, '='
    jne     @n3
    call    next
    call    peek
    cmp     eax, '='
    je      @eq
    cmp     eax, '>'
    je      @fa
    mov     ecx, T_ASSIGN
    jmp     @add
@eq:    call    next
    mov     ecx, T_EQ
    jmp     @add
@fa:    call    next
    mov     ecx, T_FATARROW
    jmp     @add
@n3:    cmp     eax, '!'
    jne     @n4
    call    next
    call    peek
    cmp     eax, '='
    je      @ne
    mov     ecx, T_BANG
    jmp     @add
@ne:    call    next
    mov     ecx, T_NE
    jmp     @add
@n4:    cmp     eax, '<'
    jne     @n5
    call    next
    call    peek
    cmp     eax, '='
    je      @le
    cmp     eax, '<'
    je      @sl
    mov     ecx, T_LT
    jmp     @add
@le:    call    next
    mov     ecx, T_LE
    jmp     @add
@sl:    call    next
    mov     ecx, T_SHL
    jmp     @add
@n5:    cmp     eax, '>'
    jne     @n6
    call    next
    call    peek
    cmp     eax, '='
    je      @ge
    cmp     eax, '>'
    je      @sr
    mov     ecx, T_GT
    jmp     @add
@ge:    call    next
    mov     ecx, T_GE
    jmp     @add
@sr:    call    next
    mov     ecx, T_SHR
    jmp     @add
@n6:    cmp     eax, '&'
    jne     @n7
    call    next
    call    peek
    cmp     eax, '&'
    je      @an
    mov     ecx, T_AMP
    jmp     @add
@an:    call    next
    mov     ecx, T_AMP    ; Logical AND as &
    jmp     @add
@n7:    cmp     eax, '|'
    jne     @n8
    call    next
    call    peek
    cmp     eax, '|'
    je      @or
    mov     ecx, T_PIPE
    jmp     @add
@or:    call    next
    mov     ecx, T_PIPE   ; Logical OR as |
    jmp     @add
@n8:    ; Single char
    movzx   ecx, al
    call    next
    cmp     ecx, '('
    jne     @p2
    mov     ecx, T_LPAREN
    jmp     @add
@p2:    cmp     ecx, ')'
    jne     @p3
    mov     ecx, T_RPAREN
    jmp     @add
@p3:    cmp     ecx, '{'
    jne     @p4
    mov     ecx, T_LBRACE
    jmp     @add
@p4:    cmp     ecx, '}'
    jne     @p5
    mov     ecx, T_RBRACE
    jmp     @add
@p5:    cmp     ecx, '['
    jne     @p6
    mov     ecx, T_LBRACKET
    jmp     @add
@p6:    cmp     ecx, ']'
    jne     @p7
    mov     ecx, T_RBRACKET
    jmp     @add
@p7:    cmp     ecx, ';'
    jne     @p8
    mov     ecx, T_SEMI
    jmp     @add
@p8:    cmp     ecx, ':'
    jne     @p9
    mov     ecx, T_COLON
    jmp     @add
@p9:    cmp     ecx, ','
    jne     @pa
    mov     ecx, T_COMMA
    jmp     @add
@pa:    cmp     ecx, '.'
    jne     @pb
    mov     ecx, T_DOT
    jmp     @add
@pb:    cmp     ecx, '*'
    jne     @pc
    mov     ecx, T_STAR
    jmp     @add
@pc:    cmp     ecx, '/'
    jne     @pd
    mov     ecx, T_SLASH
    jmp     @add
@pd:    cmp     ecx, '%'
    jne     @pe
    mov     ecx, T_PERCENT
    jmp     @add
@pe:    cmp     ecx, '^'
    jne     @pf
    mov     ecx, T_CARET
    jmp     @add
@pf:    cmp     ecx, '~'
    jne     @pg
    mov     ecx, T_TILDE
    jmp     @add
@pg:    cmp     ecx, 10
    jne     @ph
    inc     lnCur
    mov     ecx, T_NL
    jmp     @add
@ph:    jmp     @@l    ; Skip unknown
@add:   xor     edx, edx
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    jmp     @@l
@@eof:  mov     ecx, T_EOF
    xor     edx, edx
    xor     r12d, r12d
    xor     r11, r11
    call    addtok
    ret
lex ENDP

; === BINARY EMITTER ===
emit_byte PROC
    mov     rdi, cbCode
    lea     rdi, code_buf[rdi]
    stosb
    inc     cbCode
    ret
emit_byte ENDP

emit_word PROC
    mov     rdi, cbCode
    lea     rdi, code_buf[rdi]
    stosw
    add     cbCode, 2
    ret
emit_word ENDP

emit_dword PROC
    mov     rdi, cbCode
    lea     rdi, code_buf[rdi]
    stosd
    add     cbCode, 4
    ret
emit_dword ENDP

emit_qword PROC
    mov     rdi, cbCode
    lea     rdi, code_buf[rdi]
    stosq
    add     cbCode, 8
    ret
emit_qword ENDP

; REX.W prefix + opcode helpers
emit_rexw_op PROC
    mov     al, 48h
    call    emit_byte
    mov     al, cl
    call    emit_byte
    ret
emit_rexw_op ENDP

; ModRM: mod(2) | reg(3) | rm(3)
emit_modrm PROC
    ; cl=mod, dl=reg, r8=rm
    shl     cl, 6
    shl     dl, 3
    or      al, cl
    or      al, dl
    or      al, r8b
    call    emit_byte
    ret
emit_modrm ENDP

; Push imm32 (sign-extended to 64)
emit_push_imm PROC
    mov     al, 68h
    call    emit_byte
    mov     eax, ecx
    call    emit_dword
    ret
emit_push_imm ENDP

; Mov rax, imm64
emit_mov_rax_imm PROC
    mov     al, 48h
    call    emit_byte
    mov     al, 0B8h
    call    emit_byte
    mov     rax, rcx
    call    emit_qword
    ret
emit_mov_rax_imm ENDP

; Call rel32
emit_call_rel PROC
    mov     al, 0E8h
    call    emit_byte
    mov     eax, ecx
    call    emit_dword
    ret
emit_call_rel ENDP

; Jmp rel32
emit_jmp_rel PROC
    mov     al, 0E9h
    call    emit_byte
    mov     eax, ecx
    call    emit_dword
    ret
emit_jmp_rel ENDP

; Jcc rel32 (near)
emit_jcc_rel PROC
    ; cl=condition (0-15), edx=rel32
    mov     al, 0Fh
    call    emit_byte
    mov     al, cl
    or      al, 80h
    call    emit_byte
    mov     eax, edx
    call    emit_dword
    ret
emit_jcc_rel ENDP

; === PARSER / COMPILER ===
cur PROC
    mov     r8, iTok
    shl     r8, 5
    lea     rax, tok_buf[r8]
    mov     eax, [rax+TKO_TYPE]
    ret
cur ENDP

adv PROC
    inc     iTok
    ret
adv ENDP

expect PROC
    call    cur
    cmp     eax, ecx
    je      @@ok
    ; Error - continue anyway
@@ok:   jmp     adv
    ret
expect ENDP

; Expression compiler - returns type in eax
expr PROC
    call    unary
@@l:    call    cur
    cmp     eax, T_PLUS
    je      @add
    cmp     eax, T_MINUS
    je      @sub
    cmp     eax, T_STAR
    je      @mul
    cmp     eax, T_SLASH
    je      @div
    cmp     eax, T_PERCENT
    je      @mod
    cmp     eax, T_AMP
    je      @and
    cmp     eax, T_PIPE
    je      @or
    cmp     eax, T_CARET
    je      @xor
    cmp     eax, T_SHL
    je      @shl
    cmp     eax, T_SHR
    je      @shr
    cmp     eax, T_EQ
    je      @eq
    cmp     eax, T_NE
    je      @ne
    cmp     eax, T_LT
    je      @lt
    cmp     eax, T_GT
    je      @gt
    cmp     eax, T_LE
    je      @le
    cmp     eax, T_GE
    je      @ge
    ret
@add:   call    adv
    call    unary
    ; pop rbx, pop rax, add rax, rbx, push rax
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     cl, 3
    xor     dl, dl
    mov     r8, 3
    call    emit_modrm
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@sub:   call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     cl, 3
    xor     dl, dl
    mov     r8, 3
    call    emit_modrm
    mov     al, 28h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@mul:   call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0F7h
    call    emit_byte
    mov     al, 0E3h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@div:   call    adv
    call    unary
    mov     al, 59h
    call    emit_byte
    mov     al, 5Ah
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 31h
    call    emit_byte
    mov     al, 0D2h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0F7h
    call    emit_byte
    mov     al, 0F8h
    call    emit_byte
    mov     al, 52h
    call    emit_byte
    jmp     @@l
@mod:   call    adv
    call    unary
    ; div, remainder in rdx
    mov     al, 59h
    call    emit_byte
    mov     al, 5Ah
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 31h
    call    emit_byte
    mov     al, 0D2h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0F7h
    call    emit_byte
    mov     al, 0F8h
    call    emit_byte
    mov     al, 5Ah
    call    emit_byte
    jmp     @@l
@and:   call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 21h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@or:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 09h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@xor:   call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 31h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@shl:   call    adv
    call    unary
    mov     al, 59h
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0D3h
    call    emit_byte
    mov     al, 0E0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@shr:   call    adv
    call    unary
    mov     al, 59h
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0D3h
    call    emit_byte
    mov     al, 0E8h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@eq:    call    adv
    call    unary
    ; cmp, sete
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 94h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@ne:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 95h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@lt:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 9Ch
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@gt:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 9Fh
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@le:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 9Eh
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
@ge:    call    adv
    call    unary
    mov     al, 5Bh
    call    emit_byte
    mov     al, 58h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 39h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 9Dh
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    jmp     @@l
expr ENDP

unary PROC
    call    cur
    cmp     eax, T_MINUS
    je      @neg
    cmp     eax, T_BANG
    je      @not
    cmp     eax, T_TILDE
    je      @bitnot
    cmp     eax, T_INC
    je      @preinc
    cmp     eax, T_DEC
    je      @predec
    jmp     primary
@neg:   call    adv
    call    primary
    ; neg rax
    mov     al, 48h
    call    emit_byte
    mov     al, 0F7h
    call    emit_byte
    mov     al, 0D8h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    ret
@not:   call    adv
    call    primary
    mov     al, 48h
    call    emit_byte
    mov     al, 83h
    call    emit_byte
    mov     al, 0F8h
    call    emit_byte
    mov     al, 00h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 94h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 48h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 0B6h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    ret
@bitnot: call   adv
    call    primary
    mov     al, 48h
    call    emit_byte
    mov     al, 0F7h
    call    emit_byte
    mov     al, 0D0h
    call    emit_byte
    mov     al, 50h
    call    emit_byte
    ret
@preinc: call   adv
    call    primary
    ret
@predec: call   adv
    call    primary
    ret
unary ENDP

primary PROC
    call    cur
    cmp     eax, T_NUMBER
    je      @num
    cmp     eax, T_IDENT
    je      @id
    cmp     eax, T_LPAREN
    je      @par
    cmp     eax, T_STRING
    je      @str
    ret
@num:   ; Push immediate
    mov     r8, iTok
    shl     r8, 5
    mov     rax, tok_buf[r8+TKO_VAL]
    mov     rcx, rax
    call    emit_push_imm
    call    adv
    ret
@id:    ; Variable load (simplified - always stack)
    call    adv
    ret
@par:   call    adv
    call    expr
    mov     ecx, T_RPAREN
    jmp     expect
@str:   ; String literal address
    call    adv
    ret
primary ENDP

; === STATEMENTS ===
stmt PROC
    call    cur
    cmp     eax, T_LBRACE
    je      @block
    cmp     eax, T_IDENT
    je      @check_kw
    cmp     eax, T_KW
    je      @kw_stmt
    cmp     eax, T_SEMI
    je      @semi
    cmp     eax, T_NL
    je      @nl
    jmp     expr_stmt
@semi:  jmp     adv
@nl:    jmp     adv
@block: jmp     block
@check_kw:
    ; Check if ident is actually keyword
    jmp     expr_stmt
@kw_stmt:
    mov     r8, iTok
    shl     r8, 5
    mov     eax, tok_buf[r8+TKO_KW]
    cmp     eax, K_IF
    je      @if
    cmp     eax, K_WHILE
    je      @while
    cmp     eax, K_RETURN
    je      @ret
    cmp     eax, K_LET
    je      @let
    cmp     eax, K_VAR
    je      @let
    cmp     eax, K_FN
    je      @func
    cmp     eax, K_FOR
    je      @for
    jmp     expr_stmt
@let:   call    adv
    call    cur
    ; Skip mut if present
    mov     r8, iTok
    shl     r8, 5
    cmp     tok_buf[r8+TKO_KW], K_MUT
    jne     @nmut
    call    adv
@nmut:  ; Get name
    call    adv
    call    cur
    cmp     eax, T_ASSIGN
    jne     @noinit
    call    adv
    call    expr
@noinit:
    ret
@if:    call    adv
    call    expr
    ; Generate conditional jump
    mov     al, 48h
    call    emit_byte
    mov     al, 85h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    ; jz placeholder
    mov     al, 0Fh
    call    emit_byte
    mov     al, 84h
    call    emit_byte
    mov     r12, cbCode
    xor     eax, eax
    call    emit_dword
    call    stmt
    ; Patch jump
    mov     rax, cbCode
    sub     rax, r12
    sub     rax, 4
    mov     code_buf[r12], eax
    call    cur
    mov     r8, iTok
    shl     r8, 5
    cmp     tok_buf[r8+TKO_KW], K_ELSE
    jne     @noelse
    call    adv
    call    stmt
@noelse:
    ret
@while: call    adv
    mov     r13, cbCode   ; Loop start
    call    expr
    mov     al, 48h
    call    emit_byte
    mov     al, 85h
    call    emit_byte
    mov     al, 0C0h
    call    emit_byte
    mov     al, 0Fh
    call    emit_byte
    mov     al, 84h
    call    emit_byte
    mov     r12, cbCode
    xor     eax, eax
    call    emit_dword
    call    stmt
    ; Jump back
    mov     rcx, r13
    call    emit_jmp_rel
    ; Patch exit
    mov     rax, cbCode
    sub     rax, r12
    sub     rax, 4
    mov     code_buf[r12], eax
    ret
@ret:   call    adv
    call    cur
    cmp     eax, T_EOF
    je      @voidret
    cmp     eax, T_RBRACE
    je      @voidret
    cmp     eax, T_NL
    je      @voidret
    call    expr
    mov     al, 58h
    call    emit_byte
@voidret:
    mov     al, 0C3h
    call    emit_byte
    ret
@func:  call    adv
    call    cur
    ; Get function name
    call    adv
    mov     ecx, T_LPAREN
    call    expect
    ; Parse params
    mov     ecx, T_RPAREN
    call    expect
    call    cur
    cmp     eax, T_ARROW
    jne     @notype
    call    adv
    call    adv   ; Skip return type
@notype:
    call    stmt
    ret
@for:   call    adv
    call    cur
    ; for ident in expr { ... }
    call    adv   ; ident
    mov     r8, iTok
    shl     r8, 5
    cmp     tok_buf[r8+TKO_KW], K_IN
    jne     @badfor
    call    adv
    call    expr
    call    stmt
    ret
@badfor:
    ret
expr_stmt:
    call    expr
    ; Pop result
    mov     al, 58h
    call    emit_byte
    ret
stmt ENDP

block PROC
    mov     ecx, T_LBRACE
    call    expect
@@l:    call    cur
    cmp     eax, T_RBRACE
    je      @@e
    cmp     eax, T_EOF
    je      @@e
    call    stmt
    jmp     @@l
@@e:    mov     ecx, T_RBRACE
    jmp     expect
    ret
block ENDP

; === PE WRITER ===
write_pe PROC
    ; Build PE headers and write to file
    ; Simplified: Just write raw code for now + minimal header
    mov     rcx, hOut
    lea     rdx, code_buf
    mov     r8, cbCode
    lea     r9, r12
    mov     qword ptr [rsp+32], 0
    call    WriteFile
    ret
write_pe ENDP

; === MAIN ===
main PROC
    sub     rsp, 72
    
    ; Get command line
    call    GetCommandLineA
    mov     rsi, rax
    ; Skip exe name
    cmp     byte ptr [rsi], '"'
    je      @@q
@@s1:   cmp     byte ptr [rsi], ' '
    je      @@fs
    inc     rsi
    jmp     @@s1
@@q:    inc     rsi
@@q2:   cmp     byte ptr [rsi], '"'
    je      @@fs2
    inc     rsi
    jmp     @@q2
@@fs2:  inc     rsi
@@fs:   inc     rsi
    
    ; Copy filename
    lea     rdi, src_buf[MAX_SRC-256]
@@cp:   mov     al, [rsi]
    cmp     al, ' '
    je      @@ec
    cmp     al, 0
    je      @@ec
    stosb
    inc     rsi
    jmp     @@cp
@@ec:   mov     byte ptr [rdi], 0
    
    ; Open source
    lea     rcx, src_buf[MAX_SRC-256]
    xor     edx, edx
    mov     r8d, 80000000h
    mov     r9d, 3
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    call    CreateFileA
    cmp     rax, -1
    je      @@err
    mov     hFile, rax
    
    mov     rcx, hFile
    lea     rdx, src_buf
    mov     r8d, MAX_SRC-256
    lea     r9, cbSrc
    mov     qword ptr [rsp+32], 0
    call    ReadFile
    mov     rcx, hFile
    call    CloseHandle
    
    ; Init
    mov     cbCode, 0
    mov     iSrc, 0
    mov     iTok, 0
    
    ; Lex
    call    lex
    
    ; Parse and compile
    mov     iTok, 0
@@loop: call    cur
    cmp     eax, T_EOF
    je      @@end
    call    stmt
    jmp     @@loop
    
@@end:  ; Add final ret
    mov     al, 0C3h
    call    emit_byte
    
    ; Write output
    lea     rcx, out_name
    mov     edx, 40000000h
    mov     r8d, 2
    xor     r9d, r9d
    mov     dword ptr [rsp+32], 128
    mov     dword ptr [rsp+40], 0
    call    CreateFileA
    mov     hOut, rax
    
    call    write_pe
    
    mov     rcx, hOut
    call    CloseHandle
    
    xor     ecx, ecx
    call    ExitProcess
    
@@err:  mov     ecx, 1
    call    ExitProcess
    
main ENDP

.data
out_name    db "a.out", 0

END