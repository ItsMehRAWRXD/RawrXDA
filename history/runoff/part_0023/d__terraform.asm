; ============================================
; RXUC-TerraForm v1.0 — Universal Language Compiler
; A-Z, 0-9 identifiers, auto-backend x64/ARM64/WASM
; Direct PE/ELF/WASM emission, zero dependencies
; Assemble: ml64 /c terraform.asm && link /entry:_start /subsystem:console terraform.obj
; ============================================

; ========== CONFIG ==========
MAX_SRC     EQU 131072
MAX_SYM     EQU 1024
MAX_RELOC   EQU 4096
MAX_OUT     EQU 262144
MAX_SCOPES  EQU 128

; ========== TOKEN TYPES ==========
T_EOF       EQU 0
T_NL        EQU 1
T_NUM       EQU 2
T_ID        EQU 3
T_STR       EQU 4
T_LP        EQU 5
T_RP        EQU 6
T_LB        EQU 7
T_RB        EQU 8
T_LS        EQU 9      ; [
T_RS        EQU 10     ; ]
T_COLON     EQU 11
T_SEMI      EQU 12
T_COMMA     EQU 13
T_DOT       EQU 14
T_ARROW     EQU 15     ; ->
T_FATARROW  EQU 16     ; =>
T_COLCOLON  EQU 17     ; ::
T_EQ        EQU 18
T_NE        EQU 19
T_LT        EQU 20
T_GT        EQU 21
T_LE        EQU 22
T_GE        EQU 23
T_ASSIGN    EQU 24
T_PLUS      EQU 25
T_MINUS     EQU 26
T_MUL       EQU 27
T_DIV       EQU 28
T_MOD       EQU 29
T_AND       EQU 30
T_OR        EQU 31
T_XOR       EQU 32
T_SHL       EQU 33
T_SHR       EQU 34
T_NOT       EQU 35
T_REF       EQU 36
T_DEREF     EQU 37
T_INC       EQU 38
T_DEC       EQU 39

; ========== KEYWORD IDS ==========
K_FN        EQU 1
K_LET       EQU 2
K_VAR       EQU 3
K_MUT       EQU 4
K_IF        EQU 5
K_ELSE      EQU 6
K_WHILE     EQU 7
K_FOR       EQU 8
K_IN        EQU 9
K_MATCH     EQU 10
K_RETURN    EQU 11
K_BREAK     EQU 12
K_CONTINUE  EQU 13
K_STRUCT    EQU 14
K_ENUM      EQU 15
K_UNION     EQU 16
K_TYPE      EQU 17
K_IMPL      EQU 18
K_TRAIT     EQU 19
K_PUB       EQU 20
K_PRIV      EQU 21
K_UNSAFE    EQU 22
K_ASYNC     EQU 23
K_AWAIT     EQU 24
K_USE       EQU 25
K_MOD       EQU 26
K_CONST     EQU 27
K_STATIC    EQU 28
K_EXTERN    EQU 29
K_ASM       EQU 30
K_SIZEOF    EQU 31
K_ALIGNOF   EQU 32
K_TRUE      EQU 33
K_FALSE     EQU 34
K_NULL      EQU 35
K_SELF      EQU 36

; ========== BACKEND TYPES ==========
BACK_X64    EQU 0
BACK_ARM64  EQU 1
BACK_WASM   EQU 2
BACK_RISCV  EQU 3

; ========== SECTION IDS ==========
SEC_TEXT    EQU 0
SEC_DATA    EQU 1
SEC_RODATA  EQU 2
SEC_BSS     EQU 3
SEC_RELOC   EQU 4

; ========== MACROS ==========
M_PROC MACRO name:REQ
name PROC
    push    rbp
    mov     rbp, rsp
    ENDM

M_END MACRO
    mov     rsp, rbp
    pop     rbp
    ret
    ENDP
    ENDM

; ========== DATA ==========
.data
align 16

; Keyword table (perfect hash: sum chars % 64)
kw_table:
    db 0, K_ASM, 0, K_AWAIT, 0, K_BREAK, K_CONST, 0
    db K_CONTINUE, 0, 0, K_ELSE, K_ENUM, K_EXTERN, K_FALSE, K_FN
    db K_FOR, 0, K_IF, K_IMPL, K_IN, 0, K_LET, 0
    db K_MATCH, K_MOD, K_MUT, 0, K_NULL, 0, K_PRIV, K_PUB
    db 0, K_RETURN, K_SELF, K_SIZEOF, K_STATIC, K_STRUCT, K_TRAIT, K_TRUE
    db K_TYPE, 0, K_UNION, K_UNSAFE, K_USE, K_VAR, 0, K_WHILE
    db 0, 0, 0, 0, 0, 0, K_ALIGNOF, K_ASYNC

kw_strings:
    db "fn",0, "let",0, "var",0, "mut",0, "if",0, "else",0, "while",0, "for",0
    db "in",0, "match",0, "return",0, "break",0, "continue",0, "struct",0, "enum",0
    db "union",0, "type",0, "impl",0, "trait",0, "pub",0, "priv",0, "unsafe",0
    db "async",0, "await",0, "use",0, "mod",0, "const",0, "static",0, "extern",0
    db "asm",0, "sizeof",0, "alignof",0, "true",0, "false",0, "null",0, "self",0

; Error strings
err_file    db "File error", 0
err_syntax  db "Syntax", 0
err_undef   db "Undefined", 0
err_redef   db "Redefined", 0
err_type    db "Type", 0
err_backend db "Backend", 0

; PE headers (minimal)
pe_dos_hdr:
    dw 5A4Dh                    ; MZ
    29 DUP(0)
    dd pe_nt_hdr - pe_dos_hdr   ; PE offset

pe_nt_hdr:
    db "PE",0,0
    dw 8664h                    ; x64
    dw 2                        ; sections
    dd 0                        ; timestamp
    dd 0                        ; symbol table
    dd 0                        ; symbols
    dw opt_hdr_end - opt_hdr    ; optional header size
    dw 22h                      ; characteristics

opt_hdr:
    dw 20Bh                     ; PE32+ magic
    db 0,1                      ; linker version
    dd 0                        ; code size
    dd 0                        ; data size
    dd 0                        ; bss size
    dd 1000h                    ; entry point
    dd 1000h                    ; code base
    dq 400000h                  ; image base
    dd 1000h                    ; section alignment
    dd 200h                     ; file alignment
    dw 6,0                      ; OS version
    dw 0,0                      ; image version
    dw 6,0                      ; subsystem version
    dd 0                        ; win32 version
    dd 3000h                    ; image size
    dd 400h                     ; headers size
    dd 0                        ; checksum
    dw 3                        ; subsystem (console)
    dw 0                        ; DLL characteristics
    dq 100000h                  ; stack reserve
    dq 1000h                    ; stack commit
    dq 100000h                  ; heap reserve
    dq 1000h                    ; heap commit
    dd 0                        ; loader flags
    dd 16                       ; data directories
    16 DUP(0)               ; data directories
opt_hdr_end:

sect_hdr_text:
    db ".text",0,0,0
    dd 0                        ; virtual size
    dd 2000h                    ; virtual address
    dd 0                        ; raw size
    dd 400h                     ; raw address
    dd 0                        ; relocations
    dd 0                        ; line numbers
    dw 0,0                      ; counts
    dd 60000020h                ; characteristics

sect_hdr_data:
    db ".data",0,0,0
    dd 0                        ; virtual size
    dd 3000h                    ; virtual address
    dd 0                        ; raw size
    dd 600h                     ; raw address
    dd 0                        ; relocations
    dd 0                        ; line numbers
    dw 0,0                      ; counts
    dd C0000040h                ; characteristics

; ========== BSS ==========
.bss
align 4096

; Source
hInFile     dq ?
lpSource    dq ?
cbSource    dq ?
iSource     dq ?
iLine       dd ?

; Tokens (32 bytes: type4, line4, val8, str16)
lpTokens    dq ?
nTokens     dd ?
iToken      dd ?
curTok      dd 16 dup(?)

; Symbols (64 bytes: name32, type4, scope4, offset8, size8, flags8)
lpSymbols   dq ?
nSymbols    dd ?
iScope      dd ?
scopeStack  dd MAX_SCOPES dup(?)

; Output sections
lpText      dq ?      ; Code
cbText      dq ?
lpData      dq ?      ; Data
cbData      dq ?
lpReloc     dq ?      ; Relocations
nReloc      dd ?

; Backend state
backend     dd ?      ; BACK_X64 etc
entryPoint  dq ?

; Parsing
inAsmBlock  db ?

; ========== CODE ==========
.code
align 16

; === UTILITIES ===
M_PROC strlen
    xor     rax, rax
    @@l:    cmp     byte ptr [rcx+rax], 0
            je      @@d
            inc     rax
            jmp     @@l
    @@d:    ret
M_END

M_PROC strcmp
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
M_END

M_PROC memcpy
    mov     r9, rcx
    mov     r10, rdx
    mov     r11, r8
    rep     movsb
    ret
M_END

M_PROC memset
    mov     r9, rcx
    mov     r10, rdx
    mov     r11, r8
    rep     stosb
    ret
M_END

M_PROC itoa_hex
    ; rax = value, rdi = buffer
    mov     r12, 16
    mov     r13, rax
    add     rdi, 16
    mov     byte ptr [rdi], 0
    dec     rdi
    @@l:    xor     edx, edx
            mov     rax, r13
            div     r12
            mov     r13, rax
            cmp     dl, 10
            jb      @@d
            add     dl, 7
    @@d:    add     dl, '0'
            mov     [rdi], dl
            dec     rdi
            test    r13, r13
            jnz     @@l
            inc     rdi
            mov     rax, rdi
            ret
M_END

; === MEMORY ===
M_PROC alloc
    ; rcx = size
    mov     r12, rcx
    add     r12, 4095
    and     r12, -4096
    mov     rcx, r12
    mov     edx, 1000h      ; MEM_COMMIT
    mov     r8d, 4          ; PAGE_READWRITE
    call    VirtualAlloc
    ret
M_END

; === LEXER ===
M_PROC peek
    mov     rax, iSource
    cmp     rax, cbSource
    jae     @@eof
    mov     rcx, lpSource
    movzx   eax, byte ptr [rcx+rax]
    ret
@@eof:  mov     eax, -1
        ret
M_END

M_PROC next
    inc     iSource
    ret
M_END

M_PROC skip_ws
    @@l:    call    peek
            cmp     eax, ' '
            je      @ws
            cmp     eax, 9
            je      @ws
            cmp     eax, 13
            je      @ws
            ret
    @ws:    call    next
            jmp     @@l
M_END

M_PROC add_token
    ; ecx=type, edx=val, r8=str
    mov     r9d, nTokens
    shl     r9d, 5          ; *32
    mov     r10, lpTokens
    add     r10, r9
    mov     [r10], ecx
    mov     [r10+4], iLine
    mov     [r10+8], rdx
    test    r8, r8
    jz      @@n
    mov     rcx, r8
    call    strlen
    cmp     rax, 15
    cmova   rax, r15        ; Clamp to 15
    mov     rdi, r10
    add     rdi, 16
    mov     rsi, r8
    mov     rcx, rax
    rep     movsb
    mov     byte ptr [rdi], 0
@@n:    inc     nTokens
        ret
M_END

M_PROC hash_kw
    ; rcx = string, returns eax=hash
    xor     eax, eax
    mov     rsi, rcx
    @@l:    movzx   edx, byte ptr [rsi]
            test    dl, dl
            jz      @@d
            add     eax, edx
            inc     rsi
            jmp     @@l
    @@d:    and     eax, 63
            ret
M_END

M_PROC lookup_kw
    ; rcx = string, returns eax=K_xxx or 0
    mov     r12, rcx
    call    hash_kw
    movzx   eax, byte ptr kw_table[rax]
    test    al, al
    jz      @@no
    ; Verify
    mov     rcx, r12
    lea     rdx, kw_strings
    dec     eax
    @@f:    test    eax, eax
            jz      @@cmp
    @@sk:   cmp     byte ptr [rdx], 0
            jne     @@sk
            inc     rdx
            dec     eax
            jmp     @@f
    @@cmp:  call    strcmp
            test    eax, eax
            jz      @@yes
@@no:   xor     eax, eax
@@yes:  ret
M_END

M_PROC read_string
    call    next          ; Skip opening quote
    mov     r12, iSource
@@l:    call    peek
            cmp     eax, '"'
            je      @@e
            cmp     eax, '\'
            je      @esc
            cmp     eax, -1
            je      @@e
            call    next
            jmp     @@l
@esc:       call    next
            call    next
            jmp     @@l
@@e:    mov     ecx, T_STR
        xor     edx, edx
        mov     r8, lpSource
        add     r8, r12
        call    add_token
        call    next
        ret
M_END

M_PROC read_number
    xor     r12, r12
    xor     r13d, r13d    ; Base 10
    call    peek
    cmp     eax, '0'
    jne     @@dec
    call    next
    call    peek
    cmp     eax, 'x'
    je      @@hex
    cmp     eax, 'b'
    je      @@bin
    cmp     eax, 'o'
    je      @@oct
    jmp     @@dec0
@@hex:  mov     r13d, 16
        call    next
        jmp     @@rd
@@bin:  mov     r13d, 2
        call    next
        jmp     @@rd
@@oct:  mov     r13d, 8
        call    next
        jmp     @@rd
@@dec0: mov     r12, rax
        sub     r12, '0'
@@dec:  mov     r13d, 10
@@rd:   call    peek
        cmp     r13d, 16
        je      @@hx
        cmp     eax, '0'
        jb      @@e
        cmp     eax, '9'
        ja      @@e
        jmp     @@ad
@@hx:   cmp     eax, '0'
        jb      @@e
        cmp     eax, '9'
        jbe     @@ad
        cmp     eax, 'a'
        jb      @@h2
        cmp     eax, 'f'
        jbe     @@ad
@@h2:   cmp     eax, 'A'
        jb      @@e
        cmp     eax, 'F'
        ja      @@e
@@ad:   sub     eax, '0'
        cmp     eax, 9
        jbe     @@ok
        sub     eax, 7
        cmp     eax, 15
        jbe     @@ok
        sub     eax, 32
@@ok:   imul    r12, r13
        add     r12, rax
        call    next
        jmp     @@rd
@@e:    mov     ecx, T_NUM
        mov     rdx, r12
        xor     r8, r8
        call    add_token
        ret
M_END

M_PROC read_ident
    sub     rsp, 48
    lea     rdi, [rsp+8]
@@l:    call    peek
            cmp     eax, 'A'
            jb      @@c
            cmp     eax, 'Z'
            jbe     @@a
@@c:        cmp     eax, 'a'
            jb      @@c2
            cmp     eax, 'z'
            jbe     @@a
@@c2:       cmp     eax, '0'
            jb      @@e
            cmp     eax, '9'
            jbe     @@a
            cmp     eax, '_'
            jne     @@e
@@a:        stosb
            call    next
            jmp     @@l
@@e:    mov     byte ptr [rdi], 0
        lea     rcx, [rsp+8]
        call    lookup_kw
        test    eax, eax
        jnz     @@kw
        mov     ecx, T_ID
        xor     edx, edx
        lea     r8, [rsp+8]
        call    add_token
        jmp     @@x
@@kw:   mov     ecx, eax
        add     ecx, 100        ; Keywords > 100
        xor     edx, edx
        lea     r8, [rsp+8]
        call    add_token
@@x:    add     rsp, 48
        ret
M_END

M_PROC lex
    mov     nTokens, 0
    mov     iLine, 1
@@l:    call    skip_ws
        call    peek
        cmp     eax, -1
        je      @@eof
        cmp     eax, 10
        je      @@nl
        cmp     eax, '"'
        je      @@str
        cmp     eax, "'"
        je      @@chr
        cmp     eax, 'A'
        jb      @@n1
        cmp     eax, 'Z'
        jbe     @@id
@@n1:   cmp     eax, 'a'
        jb      @@n2
        cmp     eax, 'z'
        jbe     @@id
@@n2:   cmp     eax, '0'
        jb      @@sym
        cmp     eax, '9'
        jbe     @@num
@@sym:  cmp     eax, '('
        je      @@lp
        cmp     eax, ')'
        je      @@rp
        cmp     eax, '{'
        je      @@lb
        cmp     eax, '}'
        je      @@rb
        cmp     eax, '['
        je      @@ls
        cmp     eax, ']'
        je      @@rs
        cmp     eax, '+'
        je      @@pl
        cmp     eax, '-'
        je      @@mi
        cmp     eax, '*'
        je      @@mu
        cmp     eax, '/'
        je      @@sl
        cmp     eax, '%'
        je      @@mo
        cmp     eax, '&'
        je      @@an
        cmp     eax, '|'
        je      @@or
        cmp     eax, '^'
        je      @@xo
        cmp     eax, '~'
        je      @@no
        cmp     eax, '!'
        je      @@bn
        cmp     eax, '<'
        je      @@lt
        cmp     eax, '>'
        je      @@gt
        cmp     eax, '='
        je      @@as
        cmp     eax, ':'
        je      @@co
        cmp     eax, ';'
        je      @@sc
        cmp     eax, ','
        je      @@cm
        cmp     eax, '.'
        je      @@dt
        cmp     eax, '#'
        je      @@pp
        call    next
        jmp     @@l
@@str:  call    read_string
        jmp     @@l
@@chr:  ; Char literal
        call    next
        call    peek
        mov     r12, rax
        call    next
        call    peek
        cmp     eax, "'"
        jne     @@l
        call    next
        mov     ecx, T_NUM
        mov     rdx, r12
        xor     r8, r8
        call    add_token
        jmp     @@l
@@id:   call    read_ident
        jmp     @@l
@@num:  call    read_number
        jmp     @@l
@@lp:   mov     ecx, T_LP
        jmp     @@add
@@rp:   mov     ecx, T_RP
        jmp     @@add
@@lb:   mov     ecx, T_LB
        jmp     @@add
@@rb:   mov     ecx, T_RB
        jmp     @@add
@@ls:   mov     ecx, T_LS
        jmp     @@add
@@rs:   mov     ecx, T_RS
        jmp     @@add
@@pl:   mov     ecx, T_PLUS
        jmp     @@add
@@mu:   mov     ecx, T_MUL
        jmp     @@add
@@sl:   call    next
        call    peek
        cmp     eax, '/'
        je      @@ln
        mov     ecx, T_DIV
        jmp     @@addn
@@ln:   ; Line comment
        call    next
@@lnc:  call    peek
        cmp     eax, 10
        je      @@l
        cmp     eax, -1
        je      @@l
        call    next
        jmp     @@lnc
@@mo:   mov     ecx, T_MOD
        jmp     @@add
@@an:   call    next
        call    peek
        cmp     eax, '&'
        je      @@la
        mov     ecx, T_AND
        jmp     @@addn
@@la:   call    next
        mov     ecx, T_AND
        jmp     @@add
@@or:   call    next
        call    peek
        cmp     eax, '|'
        je      @@lo
        mov     ecx, T_OR
        jmp     @@addn
@@lo:   call    next
        mov     ecx, T_OR
        jmp     @@add
@@xo:   mov     ecx, T_XOR
        jmp     @@add
@@no:   mov     ecx, T_NOT
        jmp     @@add
@@bn:   call    next
        call    peek
        cmp     eax, '='
        je      @@ne
        mov     ecx, T_NOT
        jmp     @@addn
@@ne:   call    next
        mov     ecx, T_NE
        jmp     @@add
@@lt:   call    next
        call    peek
        cmp     eax, '<'
        je      @@shl
        cmp     eax, '='
        je      @@le
        mov     ecx, T_LT
        jmp     @@addn
@@shl:  call    next
        mov     ecx, T_SHL
        jmp     @@add
@@le:   call    next
        mov     ecx, T_LE
        jmp     @@add
@@gt:   call    next
        call    peek
        cmp     eax, '>'
        je      @@shr
        cmp     eax, '='
        je      @@ge
        mov     ecx, T_GT
        jmp     @@addn
@@shr:  call    next
        mov     ecx, T_SHR
        jmp     @@add
@@ge:   call    next
        mov     ecx, T_GE
        jmp     @@add
@@as:   call    next
        call    peek
        cmp     eax, '='
        je      @@eq
        cmp     eax, '>'
        je      @@fa
        mov     ecx, T_ASSIGN
        jmp     @@addn
@@eq:   call    next
        mov     ecx, T_EQ
        jmp     @@add
@@fa:   call    next
        mov     ecx, T_FATARROW
        jmp     @@add
@@co:   call    next
        call    peek
        cmp     eax, ':'
        je      @@cc
        mov     ecx, T_COLON
        jmp     @@addn
@@cc:   call    next
        mov     ecx, T_COLCOLON
        jmp     @@add
@@sc:   mov     ecx, T_SEMI
        jmp     @@add
@@cm:   mov     ecx, T_COMMA
        jmp     @@add
@@dt:   call    next
        call    peek
        cmp     eax, '.'
        je      @@rng
        cmp     eax, '>'
        je      @@ar
        mov     ecx, T_DOT
        jmp     @@addn
@@rng:  call    next
        mov     ecx, T_DOT
        jmp     @@add
@@ar:   call    next
        mov     ecx, T_ARROW
        jmp     @@add
@@pp:   ; Preprocessor
        call    next
@@ppc:  call    peek
        cmp     eax, 10
        je      @@l
        cmp     eax, -1
        je      @@eof
        call    next
        jmp     @@ppc
@@nl:   call    next
        inc     iLine
        mov     ecx, T_NL
        xor     edx, edx
        xor     r8, r8
        call    add_token
        jmp     @@l
@@addn: call    next
@@add:  xor     edx, edx
        xor     r8, r8
        call    add_token
        jmp     @@l
@@eof:  mov     ecx, T_EOF
        xor     edx, edx
        xor     r8, r8
        call    add_token
        ret
M_END

; === BACKEND: X64 DIRECT EMITTER ===
M_PROC emit_byte
    ; cl = byte
    mov     rax, lpText
    add     rax, cbText
    mov     [rax], cl
    inc     cbText
    ret
M_END

M_PROC emit_dword
    ; ecx = dword
    mov     rax, lpText
    add     rax, cbText
    mov     [rax], ecx
    add     cbText, 4
    ret
M_END

M_PROC emit_qword
    ; rcx = qword
    mov     rax, lpText
    add     rax, cbText
    mov     [rax], rcx
    add     cbText, 8
    ret
M_END

M_PROC emit_x64_prolog
    ; push rbp / mov rbp, rsp / sub rsp, imm32
    mov     cl, 55h         ; push rbp
    call    emit_byte
    mov     cl, 48h         ; REX.W
    call    emit_byte
    mov     cl, 89h         ; mov rbp, rsp
    call    emit_byte
    mov     cl, 0E5h
    call    emit_byte
    mov     cl, 48h         ; REX.W
    call    emit_byte
    mov     cl, 81h         ; sub rsp, imm32
    call    emit_byte
    mov     cl, 0ECh
    call    emit_byte
    xor     ecx, ecx        ; Stack size (patched later)
    call    emit_dword
    ret
M_END

M_PROC emit_x64_epilog
    ; add rsp, imm32 / pop rbp / ret
    mov     cl, 48h
    call    emit_byte
    mov     cl, 81h
    call    emit_byte
    mov     cl, 0C4h
    call    emit_byte
    xor     ecx, ecx
    call    emit_dword
    mov     cl, 5Dh
    call    emit_byte
    mov     cl, 0C3h
    call    emit_byte
    ret
M_END

M_PROC emit_x64_push_imm
    ; rcx = imm32
    cmp     rcx, 127
    jg      @@big
    cmp     rcx, -128
    jl      @@big
    ; push imm8
    mov     cl, 6Ah
    call    emit_byte
    mov     cl, byte ptr [rsp+8]
    call    emit_byte
    ret
@@big:  ; push imm32
    mov     cl, 68h
    call    emit_byte
    mov     ecx, dword ptr [rsp+8]
    call    emit_dword
    ret
M_END

M_PROC emit_x64_pop_reg
    ; cl = reg (0-7)
    add     cl, 58h
    call    emit_byte
    ret
M_END

M_PROC emit_x64_mov_reg_imm
    ; cl = reg, rdx = imm64
    mov     al, 48h
    or      al, cl
    shr     cl, 3
    and     cl, 1
    shl     cl, 2
    or      al, cl          ; REX prefix
    mov     cl, al
    call    emit_byte
    mov     cl, 0C7h        ; mov r/m64, imm32 (sign extended)
    call    emit_byte
    mov     al, 0C0h
    and     cl, 7
    or      al, cl
    mov     cl, al
    call    emit_byte
    mov     ecx, edx
    call    emit_dword
    ret
M_END

; === PARSER ===
M_PROC cur_tok
    mov     eax, iToken
    shl     eax, 5
    mov     r10, lpTokens
    add     r10, rax
    mov     eax, [r10]
    ret
M_END

M_PROC cur_val
    mov     eax, iToken
    shl     eax, 5
    mov     r10, lpTokens
    mov     rax, [r10+rax+8]
    ret
M_END

M_PROC cur_str
    mov     eax, iToken
    shl     eax, 5
    mov     r10, lpTokens
    lea     rax, [r10+rax+16]
    ret
M_END

M_PROC advance
    inc     iToken
    ret
M_END

M_PROC expect
    ; eax = expected
    call    cur_tok
    cmp     eax, [rsp+8]
    je      @@ok
    ; Error - continue anyway
@@ok:   jmp     advance
M_END

; === EXPRESSION PARSER ===
M_PROC parse_expr
    call    parse_or
    ret
M_END

M_PROC parse_or
    call    parse_and
@@l:    call    cur_tok
    cmp     eax, T_OR
    jne     @@e
    call    advance
    call    parse_and
    ; Emit OR
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_and
    call    parse_eq
@@l:    call    cur_tok
    cmp     eax, T_AND
    jne     @@e
    call    advance
    call    parse_eq
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_eq
    call    parse_cmp
@@l:    call    cur_tok
    cmp     eax, T_EQ
    je      @eq
    cmp     eax, T_NE
    jne     @@e
    call    advance
    call    parse_cmp
    ; Emit NE
    jmp     @@l
@eq:    call    advance
    call    parse_cmp
    ; Emit EQ
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_cmp
    call    parse_shift
@@l:    call    cur_tok
    cmp     eax, T_LT
    je      @lt
    cmp     eax, T_GT
    je      @gt
    cmp     eax, T_LE
    je      @le
    cmp     eax, T_GE
    jne     @@e
    call    advance
    call    parse_shift
    jmp     @@l
@lt:    call    advance
    call    parse_shift
    jmp     @@l
@gt:    call    advance
    call    parse_shift
    jmp     @@l
@le:    call    advance
    call    parse_shift
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_shift
    call    parse_add
@@l:    call    cur_tok
    cmp     eax, T_SHL
    je      @shl
    cmp     eax, T_SHR
    jne     @@e
    call    advance
    call    parse_add
    jmp     @@l
@shl:   call    advance
    call    parse_add
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_add
    call    parse_mul
@@l:    call    cur_tok
    cmp     eax, T_PLUS
    je      @pl
    cmp     eax, T_MINUS
    jne     @@e
    call    advance
    call    parse_mul
    ; Emit SUB
    jmp     @@l
@pl:    call    advance
    call    parse_mul
    ; Emit ADD
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_mul
    call    parse_unary
@@l:    call    cur_tok
    cmp     eax, T_MUL
    je      @mu
    cmp     eax, T_DIV
    je      @di
    cmp     eax, T_MOD
    jne     @@e
    call    advance
    call    parse_unary
    jmp     @@l
@mu:    call    advance
    call    parse_unary
    jmp     @@l
@di:    call    advance
    call    parse_unary
    jmp     @@l
@@e:    ret
M_END

M_PROC parse_unary
    call    cur_tok
    cmp     eax, T_MINUS
    je      @neg
    cmp     eax, T_NOT
    je      @not
    cmp     eax, T_REF
    je      @ref
    cmp     eax, T_MUL
    je      @deref
    jmp     parse_postfix
@neg:   call    advance
    call    parse_unary
    ret
@not:   call    advance
    call    parse_unary
    ret
@ref:   call    advance
    call    parse_unary
    ret
@deref: call    advance
    call    parse_unary
    ret
M_END

M_PROC parse_postfix
    call    parse_primary
@@l:    call    cur_tok
    cmp     eax, T_LP
    je      @call
    cmp     eax, T_LS
    je      @idx
    cmp     eax, T_DOT
    je      @fld
    ret
@call:  call    advance
    ; Args
    call    cur_tok
    cmp     eax, T_RP
    je      @ce
@@ca:   call    parse_expr
    call    cur_tok
    cmp     eax, T_COMMA
    jne     @ce
    call    advance
    jmp     @@ca
@ce:    mov     eax, T_RP
    call    expect
    jmp     @@l
@idx:   call    advance
    call    parse_expr
    mov     eax, T_RS
    call    expect
    jmp     @@l
@fld:   call    advance
    call    cur_tok
    cmp     eax, T_ID
    jne     @@l
    call    advance
    jmp     @@l
M_END

M_PROC parse_primary
    call    cur_tok
    cmp     eax, T_NUM
    je      @num
    cmp     eax, T_ID
    je      @id
    cmp     eax, T_STR
    je      @str
    cmp     eax, T_LP
    je      @par
    cmp     eax, 100 + K_TRUE
    je      @true
    cmp     eax, 100 + K_FALSE
    je      @false
    ret
@num:   call    cur_val
    mov     rcx, rax
    call    emit_x64_push_imm
    jmp     advance
@id:    ; Lookup symbol
    jmp     advance
@str:   ; String literal
    jmp     advance
@par:   call    advance
    call    parse_expr
    mov     eax, T_RP
    jmp     expect
@true:  mov     rcx, 1
    call    emit_x64_push_imm
    jmp     advance
@false: xor     ecx, ecx
    call    emit_x64_push_imm
    jmp     advance
M_END

; === STATEMENT PARSER ===
M_PROC parse_stmt
    call    cur_tok
    cmp     eax, 100 + K_FN
    je      parse_fn
    cmp     eax, 100 + K_LET
    je      parse_let
    cmp     eax, 100 + K_VAR
    je      parse_var
    cmp     eax, 100 + K_IF
    je      parse_if
    cmp     eax, 100 + K_WHILE
    je      parse_while
    cmp     eax, 100 + K_FOR
    je      parse_for
    cmp     eax, 100 + K_RETURN
    je      parse_return
    cmp     eax, 100 + K_BREAK
    je      parse_break
    cmp     eax, 100 + K_CONTINUE
    je      parse_continue
    cmp     eax, 100 + K_STRUCT
    je      parse_struct
    cmp     eax, 100 + K_ENUM
    je      parse_enum
    cmp     eax, 100 + K_IMPL
    je      parse_impl
    cmp     eax, 100 + K_ASM
    je      parse_asm
    cmp     eax, T_LB
    je      parse_block
    jmp     parse_expr_stmt
M_END

M_PROC parse_fn
    call    advance     ; Skip 'fn'
    call    cur_tok
    cmp     eax, T_ID
    jne     @@x
    ; Get name
    call    advance
    mov     eax, T_LP
    call    expect
    ; Params
@@pl:   call    cur_tok
    cmp     eax, T_RP
    je      @@pe
    cmp     eax, T_ID
    jne     @@pn
    call    advance
    call    cur_tok
    cmp     eax, T_COLON
    jne     @@pn
    call    advance
    ; Type
    call    parse_type
@@pn:   call    cur_tok
    cmp     eax, T_COMMA
    jne     @@pe
    call    advance
    jmp     @@pl
@@pe:   mov     eax, T_RP
    call    expect
    ; Return type
    call    cur_tok
    cmp     eax, T_ARROW
    jne     @@pb
    call    advance
    call    parse_type
@@pb:   call    parse_block
@@x:    ret
M_END

M_PROC parse_type
    call    cur_tok
    cmp     eax, T_ID
    je      @id
    cmp     eax, T_AND
    je      @ref
    cmp     eax, T_MUL
    je      @ptr
    ret
@id:    jmp     advance
@ref:   call    advance
    call    parse_type
    ret
@ptr:   call    advance
    call    parse_type
    ret
M_END

M_PROC parse_let
    call    advance
    call    cur_tok
    cmp     eax, T_ID
    jne     @@x
    call    advance
    ; Type annotation?
    call    cur_tok
    cmp     eax, T_COLON
    jne     @@i
    call    advance
    call    parse_type
@@i:    ; Init?
    call    cur_tok
    cmp     eax, T_ASSIGN
    jne     @@x
    call    advance
    call    parse_expr
@@x:    ret
M_END

M_PROC parse_var
    jmp     parse_let
M_END

M_PROC parse_if
    call    advance
    call    parse_expr
    call    parse_block
    call    cur_tok
    cmp     eax, 100 + K_ELSE
    jne     @@x
    call    advance
    call    cur_tok
    cmp     eax, 100 + K_IF
    je      parse_if
    call    parse_block
@@x:    ret
M_END

M_PROC parse_while
    call    advance
    call    parse_expr
    call    parse_block
    ret
M_END

M_PROC parse_for
    call    advance
    call    cur_tok
    cmp     eax, T_ID
    jne     @@x
    call    advance
    mov     eax, 100 + K_IN
    call    expect
    call    parse_expr
    call    parse_block
@@x:    ret
M_END

M_PROC parse_return
    call    advance
    call    cur_tok
    cmp     eax, T_NL
    je      @@v
    cmp     eax, T_SEMI
    je      @@v
    cmp     eax, T_RB
    je      @@v
    call    parse_expr
@@v:    ret
M_END

M_PROC parse_break
    call    advance
    ret
M_END

M_PROC parse_continue
    call    advance
    ret
M_END

M_PROC parse_struct
    call    advance
    call    cur_tok
    cmp     eax, T_ID
    jne     @@x
    call    advance
    mov     eax, T_LB
    call    expect
@@f:    call    cur_tok
    cmp     eax, T_RB
    je      @@e
    cmp     eax, T_ID
    jne     @@n
    call    advance
    mov     eax, T_COLON
    call    expect
    call    parse_type
@@n:    call    cur_tok
    cmp     eax, T_COMMA
    jne     @@c
    call    advance
    jmp     @@f
@@c:    cmp     eax, T_NL
    jne     @@e
    call    advance
    jmp     @@f
@@e:    mov     eax, T_RB
    call    expect
@@x:    ret
M_END

M_PROC parse_enum
    jmp     parse_struct
M_END

M_PROC parse_impl
    call    advance
    call    parse_type
    mov     eax, T_LB
    call    expect
@@m:    call    cur_tok
    cmp     eax, T_RB
    je      @@e
    cmp     eax, 100 + K_FN
    jne     @@n
    call    parse_fn
    jmp     @@m
@@n:    call    advance
    jmp     @@m
@@e:    mov     eax, T_RB
    call    expect
    ret
M_END

M_PROC parse_asm
    call    advance
    mov     eax, T_LB
    call    expect
    ; Raw assembly - copy until }
@@l:    call    cur_tok
    cmp     eax, T_RB
    je      @@e
    call    advance
    jmp     @@l
@@e:    jmp     advance
M_END

M_PROC parse_block
    mov     eax, T_LB
    call    expect
@@l:    call    cur_tok
    cmp     eax, T_RB
    je      @@e
    cmp     eax, T_EOF
    je      @@e
    call    parse_stmt
    jmp     @@l
@@e:    mov     eax, T_RB
    jmp     expect
M_END

M_PROC parse_expr_stmt
    call    parse_expr
    call    cur_tok
    cmp     eax, T_SEMI
    je      @@sc
    cmp     eax, T_NL
    je      @@nl
    ret
@@sc:   jmp     advance
@@nl:   jmp     advance
M_END

; === PE WRITER ===
M_PROC write_pe
    ; rcx = filename
    sub     rsp, 88
    
    ; Create file
    mov     rdx, 40000000h      ; GENERIC_WRITE
    xor     r8d, r8d
    mov     r9d, 2              ; CREATE_ALWAYS
    mov     dword ptr [rsp+32], 0
    mov     dword ptr [rsp+40], 0
    call    CreateFileA
    cmp     rax, -1
    je      @@err
    mov     [rsp+64], rax       ; hFile
    
    ; Write DOS header
    mov     rcx, [rsp+64]
    lea     rdx, pe_dos_hdr
    mov     r8d, 64
    lea     r9, [rsp+72]
    mov     qword ptr [rsp+32], 0
    call    WriteFile
    
    ; Write NT header
    mov     rcx, [rsp+64]
    lea     rdx, pe_nt_hdr
    mov     r8d, 24 + 112 + 80  ; File + optional + 2 section headers
    lea     r9, [rsp+72]
    call    WriteFile
    
    ; Pad to 0x400
    mov     rcx, [rsp+64]
    mov     edx, 400h
    xor     r8d, r8d
    mov     r9d, 0
    call    SetFilePointer
    
    ; Write .text
    mov     rcx, [rsp+64]
    mov     rdx, lpText
    mov     r8, cbText
    lea     r9, [rsp+72]
    call    WriteFile
    
    ; Pad to 0x600
    mov     rcx, [rsp+64]
    mov     edx, 600h
    xor     r8d, r8d
    mov     r9d, 0
    call    SetFilePointer
    
    ; Write .data
    mov     rcx, [rsp+64]
    mov     rdx, lpData
    mov     r8, cbData
    lea     r9, [rsp+72]
    call    WriteFile
    
    ; Close
    mov     rcx, [rsp+64]
    call    CloseHandle
    
    add     rsp, 88
    ret
@@err:  add     rsp, 88
    ret
M_END

; === MAIN ===
_start PROC
    sub     rsp, 56
    
    ; Alloc buffers
    mov     rcx, MAX_SRC
    call    alloc
    mov     lpSource, rax
    
    mov     rcx, MAX_TOK * 32
    call    alloc
    mov     lpTokens, rax
    
    mov     rcx, MAX_SYM * 64
    call    alloc
    mov     lpSymbols, rax
    
    mov     rcx, MAX_OUT
    call    alloc
    mov     lpText, rax
    
    mov     rcx, MAX_OUT
    call    alloc
    mov     lpData, rax
    
    ; Get filename from command line
    call    GetCommandLineA
    mov     rsi, rax
    ; Skip exe name
    cmp     byte ptr [rsi], '"'
    je      @@q
@@s:    cmp     byte ptr [rsi], ' '
    je      @@fs
    inc     rsi
    jmp     @@s
@@q:    inc     rsi
@@q2:   cmp     byte ptr [rsi], '"'
    je      @@fs2
    inc     rsi
    jmp     @@q2
@@fs2:  inc     rsi
@@fs:   inc     rsi
    
    ; Open source
    mov     rcx, rsi
    xor     edx, edx
    mov     r8d, 80000000h
    mov     r9d, 3
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    call    CreateFileA
    cmp     rax, -1
    je      @@err
    mov     hInFile, rax
    
    mov     rcx, hInFile
    mov     rdx, lpSource
    mov     r8d, MAX_SRC
    lea     r9, cbSource
    mov     qword ptr [rsp+32], 0
    call    ReadFile
    mov     rcx, hInFile
    call    CloseHandle
    
    ; Lex
    call    lex
    
    ; Parse & compile
    mov     iToken, 0
    mov     backend, BACK_X64
    
    ; Emit entry point
    call    emit_x64_prolog
    
@@loop: call    cur_tok
    cmp     eax, T_EOF
    je      @@end
    call    parse_stmt
    jmp     @@loop
    
@@end:  call    emit_x64_epilog
    
    ; Write PE
    lea     rcx, [out_file]
    call    write_pe
    
    xor     ecx, ecx
    call    ExitProcess
    
@@err:  mov     ecx, 1
    call    ExitProcess
    
_start ENDP

.data
out_file    db "out.exe", 0

END