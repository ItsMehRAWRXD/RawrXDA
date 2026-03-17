; =============================================================================
; RawrXD x64 Instruction Encoder (Pure MASM64)
; Reverse-engineered from Intel SDM Vol 2 + AMD APM Vol 3
; Zero external dependencies, 1.92KB resident footprint target
; =============================================================================
; Build: ml64.exe /c /nologo /Zi /W3 /errorReport:none RawrXD_x64_Encoder.asm
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; -----------------------------------------------------------------------------
; Instruction Encoding Structures
; -----------------------------------------------------------------------------

REX_PREFIX      STRUCT
    W           BYTE    ?       ; 64-bit operand size (bit 3)
    R           BYTE    ?       ; ModRM.reg extension (bit 2)
    X           BYTE    ?       ; SIB.index extension (bit 1)
    B           BYTE    ?       ; ModRM.rm/SIB.base extension (bit 0)
REX_PREFIX      ENDS

MODRM_BYTE      STRUCT
    mod_field   BYTE    ?       ; 2 bits: addressing mode
    reg_field   BYTE    ?       ; 3 bits: register or opcode ext
    rm_field    BYTE    ?       ; 3 bits: register or memory ref
MODRM_BYTE      ENDS

SIB_BYTE        STRUCT
    scale       BYTE    ?       ; 2 bits: scale factor
    index       BYTE    ?       ; 3 bits: index register
    base        BYTE    ?       ; 3 bits: base register
SIB_BYTE        ENDS

INSTRUCTION     STRUCT
    ; Prefixes
    lock        BYTE    ?       ; F0h
    rep         BYTE    ?       ; F2h/F3h
    seg         BYTE    ?       ; 2Eh-65h segment override
    opsize      BYTE    ?       ; 66h operand size
    addrsize    BYTE    ?       ; 67h address size
    rex         REX_PREFIX <>
    
    ; Opcode
    opcode_len  BYTE    ?       ; 1-3 bytes
    opcode      BYTE    3 DUP(?)
    
    ; ModRM/SIB
    has_modrm   BYTE    ?
    modrm       MODRM_BYTE <>
    has_sib     BYTE    ?
    sib         SIB_BYTE <>
    
    ; Displacement
    disp_len    BYTE    ?       ; 0, 1, 4 bytes
    disp        DWORD   ?
    
    ; Immediate
    imm_len     BYTE    ?       ; 0, 1, 2, 4, 8 bytes
    imm         QWORD   ?
    
    ; Output
    encoded_len BYTE    ?
    encoded     BYTE    15 DUP(?) ; Max x64 instruction length
INSTRUCTION     ENDS

; -----------------------------------------------------------------------------
; Register Constants (Intel encoding)
; -----------------------------------------------------------------------------

; 8-bit registers
REG_AL          EQU     0
REG_CL          EQU     1
REG_DL          EQU     2
REG_BL          EQU     3
REG_AH          EQU     4
REG_CH          EQU     5
REG_DH          EQU     6
REG_BH          EQU     7

; 16-bit registers
REG_AX          EQU     0
REG_CX          EQU     1
REG_DX          EQU     2
REG_BX          EQU     3
REG_SP          EQU     4
REG_BP          EQU     5
REG_SI          EQU     6
REG_DI          EQU     7

; 32-bit registers
REG_EAX         EQU     0
REG_ECX         EQU     1
REG_EDX         EQU     2
REG_EBX         EQU     3
REG_ESP         EQU     4
REG_EBP         EQU     5
REG_ESI         EQU     6
REG_EDI         EQU     7

; 64-bit registers
REG_RAX         EQU     0
REG_RCX         EQU     1
REG_RDX         EQU     2
REG_RBX         EQU     3
REG_RSP         EQU     4
REG_RBP         EQU     5
REG_RSI         EQU     6
REG_RDI         EQU     7
REG_R8          EQU     8
REG_R9          EQU     9
REG_R10         EQU     10
REG_R11         EQU     11
REG_R12         EQU     12
REG_R13         EQU     13
REG_R14         EQU     14
REG_R15         EQU     15

; -----------------------------------------------------------------------------
; ModRM Mode Constants
; -----------------------------------------------------------------------------

MOD_DISP0       EQU     0       ; [reg] or [disp32] (RIP-relative)
MOD_DISP8       EQU     1       ; [reg+disp8]
MOD_DISP32      EQU     2       ; [reg+disp32]
MOD_REG         EQU     3       ; register direct

; -----------------------------------------------------------------------------
; SIB Scale Constants
; -----------------------------------------------------------------------------

SCALE_1         EQU     0
SCALE_2         EQU     1
SCALE_4         EQU     2
SCALE_8         EQU     3

; -----------------------------------------------------------------------------
; Opcode Constants (Common instructions)
; -----------------------------------------------------------------------------

OP_NOP          EQU     090h
OP_RET          EQU     0C3h
OP_RET_IMM16    EQU     0C2h
OP_CALL_REL32   EQU     0E8h
OP_JMP_REL32    EQU     0E9h
OP_JMP_REL8     EQU     0EBh
OP_PUSH_RAX     EQU     050h
OP_POP_RAX      EQU     058h
OP_MOV_R8_IMM8  EQU     0B0h    ; +reg
OP_MOV_R32_IMM32 EQU    0B8h    ; +reg
OP_MOV_R64_IMM64 EQU    0B8h    ; +reg (with REX.W)
OP_LEA_R32_M    EQU     08Dh
OP_ADD_RM8_R8   EQU     000h
OP_ADD_RM32_R32 EQU     001h
OP_ADD_R8_RM8   EQU     002h
OP_ADD_R32_RM32 EQU     003h
OP_SUB_RM32_R32 EQU     029h
OP_XOR_RM32_R32 EQU     031h
OP_CMP_RM32_R32 EQU     039h
OP_TEST_RM32_R32 EQU    085h
OP_MOV_RM32_R32 EQU     089h
OP_MOV_R32_RM32 EQU     08Bh
OP_MOV_RM8_R8   EQU     088h
OP_MOV_R8_RM8   EQU     08Ah
OP_INT3         EQU     0CCh
OP_SYSCALL      EQU     050Fh   ; 2-byte

; Two-byte opcode prefix
OP2_0F          EQU     0Fh

; =============================================================================
; .DATA Section
; =============================================================================

.DATA
    ALIGN 8
    
    ; Lookup table: register needs REX.R (bit 2) or REX.B (bit 0)
    ; Index 0-15, value = 1 if needs extension bit
    reg_needs_rex   BYTE    0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1
    
    ; Lookup table: register encoding (3-bit, 0-7)
    reg_encoding    BYTE    0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7

; =============================================================================
; .CODE Section
; =============================================================================

.CODE

ALIGN 16

; =============================================================================
; Instruction Encoder Core Functions
; =============================================================================

; -----------------------------------------------------------------------------
; EncodeREX - Build and emit REX prefix if needed
; RCX = pointer to INSTRUCTION
; RDX = dest register (0-15)
; R8  = src register (0-15) or -1 for none
; R9  = needs 64-bit operand size (0/1)
; Returns: AL = 1 if REX emitted
; -----------------------------------------------------------------------------
EncodeREX PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    
    mov     rsi, rcx                    ; RSI = INSTRUCTION ptr
    xor     ebx, ebx                    ; REX bits accumulator
    
    ; Check if we need REX.W (64-bit operand)
    test    r9, r9
    jz      @F
    or      bl, 08h                     ; Set W bit
@@:
    
    ; Check destination register extension (REX.R)
    cmp     rdx, 8
    jb      @F
    or      bl, 04h                     ; Set R bit
@@:
    
    ; Check source register extension (REX.B or REX.X)
    cmp     r8, 0
    jl      check_emit                  ; No source reg
    
    cmp     r8, 8
    jb      check_emit
    or      bl, 01h                     ; Set B bit (for rm field)
    
check_emit:
    ; If any REX bit set or we need to access R8-R15, emit REX
    test    bl, bl
    jnz     emit_rex
    
    ; Check if either register needs REX (R8-R15)
    cmp     rdx, 8
    jae     emit_rex
    cmp     r8, 8
    jae     emit_rex
    
    ; No REX needed
    xor     al, al
    jmp     done
    
emit_rex:
    or      bl, 40h                     ; REX prefix base
    mov     [rsi].INSTRUCTION.rex.W, r9b
    mov     [rsi].INSTRUCTION.encoded_len, 1
    mov     [rsi].INSTRUCTION.encoded[0], bl
    mov     al, 1
    
done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
EncodeREX ENDP

; -----------------------------------------------------------------------------
; EncodeModRM - Build ModRM byte
; RCX = pointer to INSTRUCTION  
; DL = mod (2 bits)
; R8B = reg/opcode (3 bits)
; R9B = rm (3 bits)
; -----------------------------------------------------------------------------
EncodeModRM PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Build ModRM: mod(2) | reg(3) | rm(3)
    mov     al, dl
    shl     al, 3
    or      al, r8b
    shl     al, 3
    or      al, r9b
    
    mov     [rsi].INSTRUCTION.modrm.mod_field, dl
    mov     [rsi].INSTRUCTION.modrm.reg_field, r8b
    mov     [rsi].INSTRUCTION.modrm.rm_field, r9b
    mov     [rsi].INSTRUCTION.has_modrm, 1
    
    ; Append to encoded buffer
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodeModRM ENDP

; -----------------------------------------------------------------------------
; EncodeSIB - Build SIB byte for complex addressing
; RCX = pointer to INSTRUCTION
; DL = scale (0-3, representing 1,2,4,8)
; R8B = index (0-15, 4=none)
; R9B = base (0-15)
; -----------------------------------------------------------------------------
EncodeSIB PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Build SIB: scale(2) | index(3) | base(3)
    mov     al, dl
    shl     al, 3
    or      al, r8b
    shl     al, 3
    or      al, r9b
    
    mov     [rsi].INSTRUCTION.sib.scale, dl
    mov     [rsi].INSTRUCTION.sib.index, r8b
    mov     [rsi].INSTRUCTION.sib.base, r9b
    mov     [rsi].INSTRUCTION.has_sib, 1
    
    ; Append to encoded buffer
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodeSIB ENDP

; -----------------------------------------------------------------------------
; EncodeDisplacement - Add displacement bytes
; RCX = pointer to INSTRUCTION
; EDX = displacement value
; R8B = length (1 or 4)
; -----------------------------------------------------------------------------
EncodeDisplacement PROC FRAME
    push    rsi
    push    rdi
    mov     rsi, rcx
    
    mov     [rsi].INSTRUCTION.disp_len, r8b
    mov     [rsi].INSTRUCTION.disp, edx
    
    movzx   edi, [rsi].INSTRUCTION.encoded_len
    
    cmp     r8b, 1
    jne     disp32
    
    ; 8-bit displacement
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    inc     [rsi].INSTRUCTION.encoded_len
    jmp     done
    
disp32:
    ; 32-bit displacement (little-endian)
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    mov     [rsi].INSTRUCTION.encoded[rdi+1], dh
    shr     edx, 16
    mov     [rsi].INSTRUCTION.encoded[rdi+2], dl
    mov     [rsi].INSTRUCTION.encoded[rdi+3], dh
    add     [rsi].INSTRUCTION.encoded_len, 4
    
done:
    pop     rdi
    pop     rsi
    ret
EncodeDisplacement ENDP

; -----------------------------------------------------------------------------
; EncodeImmediate - Add immediate value
; RCX = pointer to INSTRUCTION
; RDX = immediate value
; R8B = length (1, 2, 4, or 8)
; -----------------------------------------------------------------------------
EncodeImmediate PROC FRAME
    push    rsi
    push    rdi
    mov     rsi, rcx
    
    mov     [rsi].INSTRUCTION.imm_len, r8b
    mov     [rsi].INSTRUCTION.imm, rdx
    
    movzx   edi, [rsi].INSTRUCTION.encoded_len
    
    cmp     r8b, 1
    je      imm8
    cmp     r8b, 2
    je      imm16
    cmp     r8b, 4
    je      imm32
    cmp     r8b, 8
    je      imm64
    
    jmp     done
    
imm8:
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    inc     [rsi].INSTRUCTION.encoded_len
    jmp     done
    
imm16:
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    mov     [rsi].INSTRUCTION.encoded[rdi+1], dh
    add     [rsi].INSTRUCTION.encoded_len, 2
    jmp     done
    
imm32:
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    mov     [rsi].INSTRUCTION.encoded[rdi+1], dh
    shr     rdx, 16
    mov     [rsi].INSTRUCTION.encoded[rdi+2], dl
    mov     [rsi].INSTRUCTION.encoded[rdi+3], dh
    add     [rsi].INSTRUCTION.encoded_len, 4
    jmp     done
    
imm64:
    ; 64-bit immediate (mov r64, imm64 only)
    mov     rax, rdx
    mov     [rsi].INSTRUCTION.encoded[rdi], al
    mov     [rsi].INSTRUCTION.encoded[rdi+1], ah
    shr     rax, 16
    mov     [rsi].INSTRUCTION.encoded[rdi+2], al
    mov     [rsi].INSTRUCTION.encoded[rdi+3], ah
    shr     rax, 16
    mov     [rsi].INSTRUCTION.encoded[rdi+4], al
    mov     [rsi].INSTRUCTION.encoded[rdi+5], ah
    shr     rax, 16
    mov     [rsi].INSTRUCTION.encoded[rdi+6], al
    mov     [rsi].INSTRUCTION.encoded[rdi+7], ah
    add     [rsi].INSTRUCTION.encoded_len, 8
    
done:
    pop     rdi
    pop     rsi
    ret
EncodeImmediate ENDP

; =============================================================================
; High-Level Instruction Encoders
; =============================================================================

; -----------------------------------------------------------------------------
; EncodeMovRegImm64 - MOV r64, imm64 (REX.W + B8+rd + 8 bytes)
; RCX = pointer to INSTRUCTION
; DL = dest register (0-15)
; R8 = immediate value (64-bit)
; -----------------------------------------------------------------------------
EncodeMovRegImm64 PROC FRAME
    push    rbx
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Emit REX.W with register extension if needed
    mov     rcx, rsi
    mov     rdx, rdx                      ; Dest reg
    mov     r9, -1                        ; No src reg
    mov     r8, 1                         ; REX.W = 1
    call    EncodeREX
    
    ; Build opcode: B8 + (reg & 7)
    movzx   eax, dl
    and     al, 7
    add     al, OP_MOV_R64_IMM64
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    ; Emit 64-bit immediate
    mov     rcx, rsi
    mov     rdx, r8
    mov     r8b, 8
    call    EncodeImmediate
    
    pop     rsi
    pop     rbx
    ret
EncodeMovRegImm64 ENDP

; -----------------------------------------------------------------------------
; EncodeMovRegReg - MOV r64, r64 (REX.W + 89 /r)
; RCX = pointer to INSTRUCTION
; DL = dest register (0-15)  
; R8B = src register (0-15)
; -----------------------------------------------------------------------------
EncodeMovRegReg PROC FRAME
    push    rbx
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Emit REX.W with extensions
    mov     rcx, rsi
    mov     rdx, rdx                      ; Dest (rm field)
    mov     r9, r8                        ; Src (reg field)
    mov     r8, 1                         ; REX.W = 1
    call    EncodeREX
    
    ; Opcode 89h (MOV r/m64, r64)
    mov     al, OP_MOV_RM32_R32
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    ; ModRM: mod=11 (register), reg=src, rm=dest
    mov     rcx, rsi
    mov     dl, MOD_REG                   ; 11b = register mode
    movzx   r8, r8b                       ; src in reg field
    and     r8b, 7
    movzx   r9, [rsi].INSTRUCTION.modrm.rm_field  ; dest in rm field  
    and     r9b, 7
    mov     r9b, [rsp+8]                  ; Get original dest register
    and     r9b, 7
    call    EncodeModRM
    
    pop     rsi
    pop     rbx
    ret
EncodeMovRegReg ENDP

; -----------------------------------------------------------------------------
; EncodePushReg - PUSH r64 (50+rd or REX.B + 50+rd)
; RCX = pointer to INSTRUCTION
; DL = register (0-15)
; -----------------------------------------------------------------------------
EncodePushReg PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Check if we need REX.B for R8-R15
    cmp     dl, 8
    jb      no_rex
    
    ; Emit REX.B
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], 41h  ; REX.B
    mov     [rsi].INSTRUCTION.encoded_len, 1
    and     dl, 7                                       ; Mask to 3 bits
    
no_rex:
    ; Opcode: 50h + (reg & 7)
    movzx   eax, dl
    add     al, OP_PUSH_RAX
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodePushReg ENDP

; -----------------------------------------------------------------------------
; EncodePopReg - POP r64 (58+rd or REX.B + 58+rd)
; RCX = pointer to INSTRUCTION
; DL = register (0-15)
; -----------------------------------------------------------------------------
EncodePopReg PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Check if we need REX.B for R8-R15
    cmp     dl, 8
    jb      no_rex
    
    ; Emit REX.B
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], 41h  ; REX.B
    mov     [rsi].INSTRUCTION.encoded_len, 1
    and     dl, 7
    
no_rex:
    ; Opcode: 58h + (reg & 7)
    movzx   eax, dl
    add     al, OP_POP_RAX
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodePopReg ENDP

; -----------------------------------------------------------------------------
; EncodeRet - RET (C3h) or RET imm16 (C2h + 2 bytes)
; RCX = pointer to INSTRUCTION
; DX = immediate (0 = plain RET, nonzero = RET imm16)
; -----------------------------------------------------------------------------
EncodeRet PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    test    dx, dx
    jnz     ret_imm
    
    ; Plain RET
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_RET
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_RET
    mov     [rsi].INSTRUCTION.encoded_len, 1
    jmp     done
    
ret_imm:
    ; RET imm16
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_RET_IMM16
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_RET_IMM16
    
    ; Little-endian immediate
    mov     [rsi].INSTRUCTION.encoded[1], dl
    mov     [rsi].INSTRUCTION.encoded[2], dh
    mov     [rsi].INSTRUCTION.encoded_len, 3
    
done:
    pop     rsi
    ret
EncodeRet ENDP

; -----------------------------------------------------------------------------
; EncodeCallRel32 - CALL rel32 (E8h + 4 bytes displacement)
; RCX = pointer to INSTRUCTION
; EDX = relative offset (target - next_instruction)
; -----------------------------------------------------------------------------
EncodeCallRel32 PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Opcode E8h
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_CALL_REL32
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_CALL_REL32
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    ; 32-bit relative displacement
    mov     rcx, rsi
    mov     rdx, [rsp+24]                  ; Get original EDX from stack
    mov     r8b, 4
    call    EncodeDisplacement
    
    pop     rsi
    ret
EncodeCallRel32 ENDP

; -----------------------------------------------------------------------------
; EncodeJmpRel32 - JMP rel32 (E9h + 4 bytes)
; RCX = pointer to INSTRUCTION
; EDX = relative offset
; -----------------------------------------------------------------------------
EncodeJmpRel32 PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_JMP_REL32
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_JMP_REL32
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    mov     rcx, rsi
    mov     rdx, [rsp+24]
    mov     r8b, 4
    call    EncodeDisplacement
    
    pop     rsi
    ret
EncodeJmpRel32 ENDP

; -----------------------------------------------------------------------------
; EncodeNop - NOP (90h) or multi-byte NOPs
; RCX = pointer to INSTRUCTION
; DL = length (1-15, uses standard NOP sequences)
; -----------------------------------------------------------------------------
EncodeNop PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    mov     rsi, rcx
    movzx   edi, dl
    mov     r12b, dl
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Multi-byte NOP sequences (Intel recommended)
    ; 1: 90
    ; 2: 66 90
    ; 3: 0F 1F 00
    ; 4: 0F 1F 40 00
    ; 5: 0F 1F 44 00 00
    ; 6: 66 0F 1F 44 00 00
    ; 7: 0F 1F 80 00 00 00 00
    ; 8: 0F 1F 84 00 00 00 00 00
    ; 9+: repeat 8-byte + remainder
    
    xor     r8d, r8d                    ; Encoded length accumulator
    mov     r9d, edi                    ; Remaining length
    
@@big_loop:
    cmp     r9d, 9
    jb      small_nop
    
    ; Use 8-byte NOP: 0F 1F 84 00 00 00 00 00
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 84h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+4], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+5], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+6], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+7], 00h
    add     r8d, 8
    sub     r9d, 8
    jmp     @@big_loop
    
small_nop:
    ; Handle 1-8 byte NOPs via jump table
    lea     rax, nop_table
    jmp     qword ptr [rax + r9*8]
    
    ALIGN 8
nop_table:
    DQ      nop_0, nop_1, nop_2, nop_3, nop_4, nop_5, nop_6, nop_7, nop_8
    
nop_0:
    jmp     nop_done
nop_1:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 90h
    inc     r8d
    jmp     nop_done
nop_2:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 66h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 90h
    add     r8d, 2
    jmp     nop_done
nop_3:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 00h
    add     r8d, 3
    jmp     nop_done
nop_4:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 40h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 00h
    add     r8d, 4
    jmp     nop_done
nop_5:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 44h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+4], 00h
    add     r8d, 5
    jmp     nop_done
nop_6:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 66h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 44h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+4], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+5], 00h
    add     r8d, 6
    jmp     nop_done
nop_7:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 80h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+4], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+5], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+6], 00h
    add     r8d, 7
    jmp     nop_done
nop_8:
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+1], 1Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+2], 84h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+3], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+4], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+5], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+6], 00h
    mov     byte ptr [rsi].INSTRUCTION.encoded[r8+7], 00h
    add     r8d, 8
    
nop_done:
    mov     [rsi].INSTRUCTION.encoded_len, r8b
    
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
EncodeNop ENDP

; -----------------------------------------------------------------------------
; EncodeSyscall - SYSCALL (0F 05)
; RCX = pointer to INSTRUCTION
; -----------------------------------------------------------------------------
EncodeSyscall PROC FRAME
    push    rsi
    mov     rsi, rcx
    
    ; Clear instruction
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    xor     al, al
    push    rdi
    mov     rdi, rcx
    rep     stosb
    pop     rdi
    
    ; Two-byte opcode: 0F 05
    mov     [rsi].INSTRUCTION.opcode[0], OP2_0F
    mov     [rsi].INSTRUCTION.opcode[1], 05h
    mov     [rsi].INSTRUCTION.opcode_len, 2
    
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP2_0F
    mov     byte ptr [rsi].INSTRUCTION.encoded[1], 05h
    mov     [rsi].INSTRUCTION.encoded_len, 2
    
    pop     rsi
    ret
EncodeSyscall ENDP

; =============================================================================
; Utility Functions
; =============================================================================

; -----------------------------------------------------------------------------
; GetInstructionLength - Returns encoded length
; RCX = pointer to INSTRUCTION
; Returns: EAX = length in bytes
; -----------------------------------------------------------------------------
GetInstructionLength PROC FRAME
    movzx   eax, [rcx].INSTRUCTION.encoded_len
    ret
GetInstructionLength ENDP

; -----------------------------------------------------------------------------
; CopyInstructionBytes - Copy encoded bytes to buffer
; RCX = pointer to INSTRUCTION
; RDX = destination buffer
; R8B = max bytes to copy
; Returns: EAX = bytes copied
; -----------------------------------------------------------------------------
CopyInstructionBytes PROC FRAME
    push    rsi
    push    rdi
    push    rcx
    
    mov     rsi, rcx
    mov     rdi, rdx
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    cmp     cl, r8b
    jbe     @F
    mov     cl, r8b
@@:
    movzx   eax, cl
    rep     movsb
    
    pop     rcx
    pop     rdi
    pop     rsi
    ret
CopyInstructionBytes ENDP

; =============================================================================
; End of encoder
; =============================================================================

END
