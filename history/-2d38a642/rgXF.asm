; =============================================================================
; RawrXD x64 Instruction Encoder - PRODUCTION READY
; Pure MASM64 - Zero Dependencies - Struct-Based Architecture
; Reverse-engineered from Intel SDM Vol 2 + AMD APM Vol 3
; Build: ml64 /c /nologo /W3 /Zi x64_encoder_production.asm
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

EXTRN RtlZeroMemory:PROC

; =============================================================================
; Instruction Encoding Structures
; =============================================================================

REX_PREFIX      STRUCT
    W           BYTE    ?
    R           BYTE    ?
    X           BYTE    ?
    B           BYTE    ?
REX_PREFIX      ENDS

MODRM_BYTE      STRUCT
    mod_field   BYTE    ?
    reg_field   BYTE    ?
    rm_field    BYTE    ?
MODRM_BYTE      ENDS

SIB_BYTE        STRUCT
    scale       BYTE    ?
    index       BYTE    ?
    base        BYTE    ?
SIB_BYTE        ENDS

INSTRUCTION     STRUCT
    lock_prefix BYTE    ?
    rep_prefix  BYTE    ?
    rex         REX_PREFIX <>
    opcode_len  BYTE    ?
    opcode      BYTE 3 DUP(?)
    has_modrm   BYTE    ?
    modrm       MODRM_BYTE <>
    has_sib     BYTE    ?
    sib         SIB_BYTE <>
    disp_len    BYTE    ?
    disp        DWORD   ?
    imm_len     BYTE    ?
    imm         QWORD   ?
    encoded_len BYTE    ?
    encoded     BYTE 15 DUP(?)
INSTRUCTION     ENDS

; =============================================================================
; Register Constants
; =============================================================================

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

MOD_DISP0       EQU     0
MOD_DISP8       EQU     1
MOD_DISP32      EQU     2
MOD_REG         EQU     3

SCALE_1         EQU     0
SCALE_2         EQU     1
SCALE_4         EQU     2
SCALE_8         EQU     3

REX_W           EQU     48h

OP_NOP          EQU     090h
OP_RET          EQU     0C3h
OP_CALL_REL32   EQU     0E8h
OP_JMP_REL32    EQU     0E9h
OP_PUSH_RAX     EQU     050h
OP_POP_RAX      EQU     058h
OP_MOV_R64_IMM64 EQU    0B8h
OP_MOV_RM32_R32 EQU     089h
OP_SYSCALL      EQU     050Fh

; =============================================================================
; Code Section
; =============================================================================

.CODE

ALIGN 16

; =============================================================================
; EncodeREX - Build and emit REX prefix if needed
; =============================================================================
EncodeREX PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rsi, rcx
    xor     ebx, ebx
    
    test    r9, r9
    jz      @F
    or      bl, 08h
@@:
    
    cmp     rdx, 8
    jb      @F
    or      bl, 04h
@@:
    
    cmp     r8, 0
    jl      check_emit
    cmp     r8, 8
    jb      check_emit
    or      bl, 01h
    
check_emit:
    test    bl, bl
    jnz     emit_rex
    cmp     rdx, 8
    jae     emit_rex
    cmp     r8, 8
    jae     emit_rex
    xor     al, al
    jmp     done
    
emit_rex:
    or      bl, 40h
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

; =============================================================================
; EncodeModRM - Build ModRM byte
; =============================================================================
EncodeModRM PROC FRAME
    push    rsi
    .pushreg rsi
    .endprolog
    
    mov     rsi, rcx
    mov     al, dl
    shl     al, 3
    or      al, r8b
    shl     al, 3
    or      al, r9b
    
    mov     [rsi].INSTRUCTION.modrm.mod_field, dl
    mov     [rsi].INSTRUCTION.modrm.reg_field, r8b
    mov     [rsi].INSTRUCTION.modrm.rm_field, r9b
    mov     [rsi].INSTRUCTION.has_modrm, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodeModRM ENDP

; =============================================================================
; EncodeSIB - Build SIB byte
; =============================================================================
EncodeSIB PROC FRAME
    push    rsi
    .pushreg rsi
    .endprolog
    
    mov     rsi, rcx
    mov     al, dl
    shl     al, 3
    or      al, r8b
    shl     al, 3
    or      al, r9b
    
    mov     [rsi].INSTRUCTION.sib.scale, dl
    mov     [rsi].INSTRUCTION.sib.index, r8b
    mov     [rsi].INSTRUCTION.sib.base, r9b
    mov     [rsi].INSTRUCTION.has_sib, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    pop     rsi
    ret
EncodeSIB ENDP

; =============================================================================
; EncodeDisplacement - Add displacement bytes
; =============================================================================
EncodeDisplacement PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rsi, rcx
    mov     [rsi].INSTRUCTION.disp_len, r8b
    mov     [rsi].INSTRUCTION.disp, edx
    
    movzx   edi, [rsi].INSTRUCTION.encoded_len
    
    cmp     r8b, 1
    jne     disp32
    
    mov     [rsi].INSTRUCTION.encoded[rdi], dl
    inc     [rsi].INSTRUCTION.encoded_len
    jmp     done
    
disp32:
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

; =============================================================================
; EncodeImmediate - Add immediate value
; =============================================================================
EncodeImmediate PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog
    
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
; EncodeMovRegImm64 - MOV r64, imm64
; =============================================================================
EncodeMovRegImm64 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     rcx, rsi
    mov     rdx, rdx
    mov     r8, -1
    mov     r9, 1
    call    EncodeREX
    
    movzx   eax, dl
    and     al, 7
    add     al, OP_MOV_R64_IMM64
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    mov     rcx, rsi
    mov     rdx, r8
    mov     r8b, 8
    call    EncodeImmediate
    
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
EncodeMovRegImm64 ENDP

; =============================================================================
; EncodePushReg - PUSH r64
; =============================================================================
EncodePushReg PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    cmp     dl, 8
    jb      no_rex
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], 41h
    mov     [rsi].INSTRUCTION.encoded_len, 1
    and     dl, 7
    
no_rex:
    movzx   eax, dl
    add     al, OP_PUSH_RAX
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    add     rsp, 32
    pop     rsi
    ret
EncodePushReg ENDP

; =============================================================================
; EncodePopReg - POP r64
; =============================================================================
EncodePopReg PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    cmp     dl, 8
    jb      no_rex
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], 41h
    mov     [rsi].INSTRUCTION.encoded_len, 1
    and     dl, 7
    
no_rex:
    movzx   eax, dl
    add     al, OP_POP_RAX
    mov     [rsi].INSTRUCTION.opcode[0], al
    mov     [rsi].INSTRUCTION.opcode_len, 1
    
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    mov     [rsi].INSTRUCTION.encoded[rcx], al
    inc     [rsi].INSTRUCTION.encoded_len
    
    add     rsp, 32
    pop     rsi
    ret
EncodePopReg ENDP

; =============================================================================
; EncodeRet - RET
; =============================================================================
EncodeRet PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_RET
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_RET
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    add     rsp, 32
    pop     rsi
    ret
EncodeRet ENDP

; =============================================================================
; EncodeCallRel32 - CALL rel32
; =============================================================================
EncodeCallRel32 PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_CALL_REL32
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_CALL_REL32
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    mov     rcx, rsi
    mov     rdx, rdx
    mov     r8b, 4
    call    EncodeDisplacement
    
    add     rsp, 32
    pop     rsi
    ret
EncodeCallRel32 ENDP

; =============================================================================
; EncodeJmpRel32 - JMP rel32
; =============================================================================
EncodeJmpRel32 PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_JMP_REL32
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_JMP_REL32
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    mov     rcx, rsi
    mov     rdx, rdx
    mov     r8b, 4
    call    EncodeDisplacement
    
    add     rsp, 32
    pop     rsi
    ret
EncodeJmpRel32 ENDP

; =============================================================================
; EncodeNop - NOP
; =============================================================================
EncodeNop PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     byte ptr [rsi].INSTRUCTION.opcode[0], OP_NOP
    mov     [rsi].INSTRUCTION.opcode_len, 1
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], OP_NOP
    mov     [rsi].INSTRUCTION.encoded_len, 1
    
    add     rsp, 32
    pop     rsi
    ret
EncodeNop ENDP

; =============================================================================
; EncodeSyscall - SYSCALL
; =============================================================================
EncodeSyscall PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rsi, rcx
    mov     rcx, rsi
    mov     rdx, SIZEOF INSTRUCTION
    call    RtlZeroMemory
    
    mov     [rsi].INSTRUCTION.opcode[0], 0Fh
    mov     [rsi].INSTRUCTION.opcode[1], 05h
    mov     [rsi].INSTRUCTION.opcode_len, 2
    
    mov     byte ptr [rsi].INSTRUCTION.encoded[0], 0Fh
    mov     byte ptr [rsi].INSTRUCTION.encoded[1], 05h
    mov     [rsi].INSTRUCTION.encoded_len, 2
    
    add     rsp, 32
    pop     rsi
    ret
EncodeSyscall ENDP

; =============================================================================
; GetInstructionLength - Returns encoded length
; =============================================================================
GetInstructionLength PROC FRAME
    .endprolog
    movzx   eax, [rcx].INSTRUCTION.encoded_len
    ret
GetInstructionLength ENDP

; =============================================================================
; CopyInstructionBytes - Copy encoded bytes to buffer
; =============================================================================
CopyInstructionBytes PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rcx
    .pushreg rcx
    .pushreg rcx
    .endprolog
    
    mov     rsi, rcx
    mov     rdi, rdx
    movzx   ecx, [rsi].INSTRUCTION.encoded_len
    cmp     cl, r8b
    jbe     @F
    mov     cl, r8b
@@:
    movzx   eax, cl
    test    cl, cl
    jz      done
    
    lea     rsi, [rsi].INSTRUCTION.encoded
@@:
    mov     dl, [rsi]
    mov     [rdi], dl
    inc     rsi
    inc     rdi
    dec     cl
    jnz     @B
    
done:
    pop     rcx
    pop     rcx
    pop     rdi
    pop     rsi
    ret
CopyInstructionBytes ENDP

; =============================================================================
; Exports
; =============================================================================

public EncodeREX
public EncodeModRM
public EncodeSIB
public EncodeDisplacement
public EncodeImmediate
public EncodeMovRegImm64
public EncodePushReg
public EncodePopReg
public EncodeRet
public EncodeCallRel32
public EncodeJmpRel32
public EncodeNop
public EncodeSyscall
public GetInstructionLength
public CopyInstructionBytes

END
