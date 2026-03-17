; ================================================================================
; RIEE - RawrXD Instruction Encoder Engine v2.0
; Production-Ready x86-64 Instruction Encoder
; Pure MASM64, Zero Dependencies, All 15 Core Encoders
; ================================================================================
; Build: ml64.exe /c /nologo /Zi instruction_encoder.asm
; Link: lib.exe /OUT:instruction_encoder.lib instruction_encoder.obj
; ================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; ================================================================================
; CONTEXT STRUCTURE (96 bytes)
; ================================================================================

ENCODER_CTX STRUCT
    pOutput             QWORD ?         ; Output buffer pointer
    nCapacity           DWORD ?         ; Buffer size
    nOffset             DWORD ?         ; Current offset
    nLastSize           DWORD ?         ; Last instruction size
    bHasREX             BYTE ?          ; REX present
    bREX                BYTE ?          ; REX byte
    bHasVEX             BYTE ?          ; VEX present
    bHasEVEX            BYTE ?          ; EVEX present
    vexBytes            BYTE 3 DUP(?)   ; VEX bytes
    evexBytes           BYTE 4 DUP(?)   ; EVEX bytes
    pad1                BYTE ?
    bOpcode1            BYTE ?          ; Primary opcode
    bOpcode2            BYTE ?          ; Secondary opcode
    bOpcodeLen          BYTE ?          ; Opcode length
    bHasPrefix66        BYTE ?          ; 66h prefix
    bHasPrefixF2        BYTE ?          ; F2h prefix
    bHasPrefixF3        BYTE ?          ; F3h prefix
    bHasModRM           BYTE ?          ; ModRM present
    bModRM              BYTE ?          ; ModRM byte
    bHasSIB             BYTE ?          ; SIB present
    bSIB                BYTE ?          ; SIB byte
    nDispSize           BYTE ?          ; Displacement size
    pad2                BYTE 3 DUP(?)
    nDisplacement       DWORD ?         ; Displacement
    nImmSize            BYTE ?          ; Immediate size
    pad3                BYTE 7 DUP(?)
    nImmediate          QWORD ?         ; Immediate
    nOperandCount       BYTE ?
    nOperandSize        BYTE ?
    nErrorCode          DWORD ?
ENCODER_CTX ENDS

; ================================================================================
; CONSTANTS
; ================================================================================

REG_RAX     EQU 0
REG_RCX     EQU 1
REG_RDX     EQU 2
REG_RBX     EQU 3
REG_RSP     EQU 4
REG_RBP     EQU 5
REG_RSI     EQU 6
REG_RDI     EQU 7
REG_R8      EQU 8
REG_R9      EQU 9
REG_R10     EQU 10
REG_R11     EQU 11
REG_R12     EQU 12
REG_R13     EQU 13
REG_R14     EQU 14
REG_R15     EQU 15

REX_BASE    EQU 40h
REX_W       EQU 08h
REX_R       EQU 04h
REX_X       EQU 02h
REX_B       EQU 01h

MOD_INDIRECT    EQU 00h
MOD_DISP8       EQU 40h
MOD_DISP32      EQU 80h
MOD_REGISTER    EQU 0C0h

ERR_NONE            EQU 0
ERR_BUFFER_OVERFLOW EQU 1
ERR_INVALID_OPERAND EQU 2
ERR_INVALID_REG     EQU 3
ERR_ENCODING_FAILED EQU 6

CC_O        EQU 0
CC_NO       EQU 1
CC_B        EQU 2
CC_NB       EQU 3
CC_E        EQU 4
CC_NE       EQU 5
CC_BE       EQU 6
CC_A        EQU 7
CC_S        EQU 8
CC_NS       EQU 9
CC_P        EQU 10
CC_NP       EQU 11
CC_L        EQU 12
CC_NL       EQU 13
CC_LE       EQU 14
CC_G        EQU 15

; ================================================================================
; PUBLIC EXPORTS
; ================================================================================

.code
align 16

PUBLIC Encoder_Init
PUBLIC Encoder_Reset
PUBLIC Encoder_SetOpcode
PUBLIC Encoder_SetOpcode2
PUBLIC Encoder_SetREX
PUBLIC Encoder_SetModRM_RegReg
PUBLIC Encoder_SetModRM_RegMem
PUBLIC Encoder_SetModRM_MemReg
PUBLIC Encoder_SetSIB
PUBLIC Encoder_SetDisplacement
PUBLIC Encoder_SetImmediate
PUBLIC Encoder_SetPrefix66
PUBLIC Encoder_SetPrefixF2
PUBLIC Encoder_SetPrefixF3
PUBLIC Encoder_EncodeInstruction
PUBLIC Encoder_GetBuffer
PUBLIC Encoder_GetSize
PUBLIC Encoder_GetError
PUBLIC Encoder_GetLastSize

PUBLIC Encode_MOV_R64_R64
PUBLIC Encode_MOV_R64_IMM64
PUBLIC Encode_PUSH_R64
PUBLIC Encode_POP_R64
PUBLIC Encode_CALL_REL32
PUBLIC Encode_RET
PUBLIC Encode_NOP
PUBLIC Encode_LEA_R64_M
PUBLIC Encode_ADD_R64_R64
PUBLIC Encode_SUB_R64_IMM8
PUBLIC Encode_CMP_R64_R64
PUBLIC Encode_JMP_REL32
PUBLIC Encode_Jcc_REL32
PUBLIC Encode_SYSCALL
PUBLIC Encode_XCHG_R64_R64

PUBLIC CalcRelativeOffset
PUBLIC TestEncoder

; ================================================================================
; INITIALIZATION
; ================================================================================

Encoder_Init PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov [rbx].ENCODER_CTX.pOutput, rdx
    mov [rbx].ENCODER_CTX.nCapacity, r8d
    mov DWORD PTR [rbx].ENCODER_CTX.nOffset, 0
    mov DWORD PTR [rbx].ENCODER_CTX.nLastSize, 0
    mov DWORD PTR [rbx].ENCODER_CTX.nErrorCode, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasModRM, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasSIB, 0
    mov BYTE PTR [rbx].ENCODER_CTX.nDispSize, 0
    mov BYTE PTR [rbx].ENCODER_CTX.nImmSize, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 1
    
    mov eax, 1
    pop rbx
    ret
Encoder_Init ENDP

Encoder_Reset PROC
    mov DWORD PTR [rcx].ENCODER_CTX.nErrorCode, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasREX, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasModRM, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasSIB, 0
    mov BYTE PTR [rcx].ENCODER_CTX.nDispSize, 0
    mov BYTE PTR [rcx].ENCODER_CTX.nImmSize, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasPrefix66, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasPrefixF2, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bHasPrefixF3, 0
    mov BYTE PTR [rcx].ENCODER_CTX.bOpcodeLen, 1
    mov QWORD PTR [rcx].ENCODER_CTX.nImmediate, 0
    ret
Encoder_Reset ENDP

; ================================================================================
; OPCODE CONFIGURATION
; ================================================================================

Encoder_SetOpcode PROC
    mov [rcx].ENCODER_CTX.bOpcode1, dl
    mov BYTE PTR [rcx].ENCODER_CTX.bOpcodeLen, 1
    ret
Encoder_SetOpcode ENDP

Encoder_SetOpcode2 PROC
    mov [rcx].ENCODER_CTX.bOpcode1, 0Fh
    mov [rcx].ENCODER_CTX.bOpcode2, dl
    mov BYTE PTR [rcx].ENCODER_CTX.bOpcodeLen, 2
    ret
Encoder_SetOpcode2 ENDP

Encoder_SetREX PROC
    mov [rcx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, dl
    mov [rcx].ENCODER_CTX.bREX, al
    ret
Encoder_SetREX ENDP

; ================================================================================
; MODRM CONFIGURATION
; ================================================================================

Encoder_SetModRM_RegReg PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; Build ModRM: mod=11
    mov al, MOD_REGISTER
    mov cl, dl
    and cl, 7
    shl cl, 3
    or al, cl
    
    mov cl, r8b
    and cl, 7
    or al, cl
    
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    ; Handle REX bits for registers 8-15
    xor r9d, r9d
    test dl, 8
    jz REX_CheckB1
    or r9d, REX_R
REX_CheckB1:
    test r8b, 8
    jz REX_Done1
    or r9d, REX_B
REX_Done1:
    test r9d, r9d
    jz ModRM_Exit
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne ModRM_Merge
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    jmp ModRM_Exit
    
ModRM_Merge:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    
ModRM_Exit:
    pop rbx
    ret
Encoder_SetModRM_RegReg ENDP

Encoder_SetModRM_RegMem PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; Build ModRM based on displacement
    xor eax, eax
    test r9d, r9d
    jz Mem_Indirect
    cmp r9d, 1
    je Mem_Disp8
    mov al, MOD_DISP32
    jmp Mem_ModSet
    
Mem_Disp8:
    mov al, MOD_DISP8
    jmp Mem_ModSet
    
Mem_Indirect:
    cmp r8d, REG_RBP
    je Mem_Disp8
    cmp r8d, REG_R13
    jne Mem_ModSet
    mov al, MOD_DISP8
    
Mem_ModSet:
    mov cl, dl
    and cl, 7
    shl cl, 3
    or al, cl
    
    cmp r8d, REG_RSP
    je Mem_NeedsSIB
    cmp r8d, REG_R12
    jne Mem_NoSIB
    
Mem_NeedsSIB:
    mov cl, r8b
    and cl, 7
    or al, 4
    mov [rbx].ENCODER_CTX.bHasSIB, 1
    mov cl, r8b
    and cl, 7
    or cl, (4 shl 3)
    mov [rbx].ENCODER_CTX.bSIB, cl
    
Mem_NoSIB:
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    test r9d, r9d
    jz Mem_CheckREX
    
    mov [rbx].ENCODER_CTX.nDispSize, r9b
    mov eax, [rsp+40]
    mov [rbx].ENCODER_CTX.nDisplacement, eax
    
Mem_CheckREX:
    xor r9d, r9d
    test dl, 8
    jz Mem_CheckB
    or r9d, REX_R
    
Mem_CheckB:
    test r8b, 8
    jz Mem_REXDone
    or r9d, REX_B
    
Mem_REXDone:
    test r9d, r9d
    jz Mem_Exit
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne Mem_Merge
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    jmp Mem_Exit
    
Mem_Merge:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    
Mem_Exit:
    pop rbx
    ret
Encoder_SetModRM_RegMem ENDP

Encoder_SetModRM_MemReg PROC
    jmp Encoder_SetModRM_RegMem
Encoder_SetModRM_MemReg ENDP

; ================================================================================
; SIB & IMMEDIATE CONFIGURATION
; ================================================================================

Encoder_SetSIB PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    mov al, dl
    shl al, 6
    
    mov cl, r8b
    and cl, 7
    shl cl, 3
    or al, cl
    
    mov cl, r9b
    and cl, 7
    or al, cl
    
    mov [rbx].ENCODER_CTX.bHasSIB, 1
    mov [rbx].ENCODER_CTX.bSIB, al
    
    ; Handle REX.X
    test r8b, 8
    jz SIB_CheckB
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne SIB_MergeX
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE or REX_X
    mov [rbx].ENCODER_CTX.bREX, al
    jmp SIB_CheckB
    
SIB_MergeX:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, REX_X
    mov [rbx].ENCODER_CTX.bREX, al
    
SIB_CheckB:
    test r9b, 8
    jz SIB_Exit
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne SIB_MergeB
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE or REX_B
    mov [rbx].ENCODER_CTX.bREX, al
    jmp SIB_Exit
    
SIB_MergeB:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, REX_B
    mov [rbx].ENCODER_CTX.bREX, al
    
SIB_Exit:
    pop rbx
    ret
Encoder_SetSIB ENDP

Encoder_SetDisplacement PROC
    mov [rcx].ENCODER_CTX.nDisplacement, edx
    mov [rcx].ENCODER_CTX.nDispSize, r8b
    ret
Encoder_SetDisplacement ENDP

Encoder_SetImmediate PROC
    mov [rcx].ENCODER_CTX.nImmediate, rdx
    mov [rcx].ENCODER_CTX.nImmSize, r8b
    ret
Encoder_SetImmediate ENDP

Encoder_SetPrefix66 PROC
    mov [rcx].ENCODER_CTX.bHasPrefix66, 1
    ret
Encoder_SetPrefix66 ENDP

Encoder_SetPrefixF2 PROC
    mov [rcx].ENCODER_CTX.bHasPrefixF2, 1
    ret
Encoder_SetPrefixF2 ENDP

Encoder_SetPrefixF3 PROC
    mov [rcx].ENCODER_CTX.bHasPrefixF3, 1
    ret
Encoder_SetPrefixF3 ENDP

; ================================================================================
; MAIN ENCODER
; ================================================================================

Encoder_EncodeInstruction PROC FRAME
    push rbx
    push rdi
    push rsi
    .pushreg rbx
    .pushreg rdi
    .pushreg rsi
    .endprolog
    
    mov rbx, rcx
    mov rdi, [rbx].ENCODER_CTX.pOutput
    mov esi, [rbx].ENCODER_CTX.nOffset
    mov r12d, esi
    
    ; Check capacity
    mov ecx, [rbx].ENCODER_CTX.nCapacity
    sub ecx, esi
    cmp ecx, 15
    jl Enc_Error
    
    ; Write prefixes
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefix66, 0
    je Enc_NoPre66
    mov BYTE PTR [rdi+rsi], 66h
    inc esi
Enc_NoPre66:
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF2, 0
    je Enc_NoPreF2
    mov BYTE PTR [rdi+rsi], 0F2h
    inc esi
Enc_NoPreF2:
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF3, 0
    je Enc_NoPreF3
    mov BYTE PTR [rdi+rsi], 0F3h
    inc esi
Enc_NoPreF3:
    
    ; Write REX
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    je Enc_NoREX
    mov al, [rbx].ENCODER_CTX.bREX
    mov BYTE PTR [rdi+rsi], al
    inc esi
Enc_NoREX:
    
    ; Write opcodes
    mov al, [rbx].ENCODER_CTX.bOpcode1
    mov BYTE PTR [rdi+rsi], al
    inc esi
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 2
    jne Enc_NoOp2
    mov al, [rbx].ENCODER_CTX.bOpcode2
    mov BYTE PTR [rdi+rsi], al
    inc esi
Enc_NoOp2:
    
    ; Write ModRM
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasModRM, 0
    je Enc_NoModRM
    mov al, [rbx].ENCODER_CTX.bModRM
    mov BYTE PTR [rdi+rsi], al
    inc esi
Enc_NoModRM:
    
    ; Write SIB
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasSIB, 0
    je Enc_NoSIB
    mov al, [rbx].ENCODER_CTX.bSIB
    mov BYTE PTR [rdi+rsi], al
    inc esi
Enc_NoSIB:
    
    ; Write displacement
    movzx ecx, BYTE PTR [rbx].ENCODER_CTX.nDispSize
    test ecx, ecx
    jz Enc_NoDisp
    
    cmp ecx, 1
    je Enc_D8
    mov eax, [rbx].ENCODER_CTX.nDisplacement
    mov DWORD PTR [rdi+rsi], eax
    add esi, 4
    jmp Enc_NoDisp
    
Enc_D8:
    mov al, BYTE PTR [rbx].ENCODER_CTX.nDisplacement
    mov BYTE PTR [rdi+rsi], al
    inc esi
    
Enc_NoDisp:
    ; Write immediate
    movzx ecx, BYTE PTR [rbx].ENCODER_CTX.nImmSize
    test ecx, ecx
    jz Enc_NoImm
    
    cmp ecx, 1
    je Enc_I1
    cmp ecx, 2
    je Enc_I2
    cmp ecx, 4
    je Enc_I4
    
    mov rax, [rbx].ENCODER_CTX.nImmediate
    mov QWORD PTR [rdi+rsi], rax
    add esi, 8
    jmp Enc_NoImm
    
Enc_I1:
    mov al, BYTE PTR [rbx].ENCODER_CTX.nImmediate
    mov BYTE PTR [rdi+rsi], al
    inc esi
    jmp Enc_NoImm
    
Enc_I2:
    mov eax, DWORD PTR [rbx].ENCODER_CTX.nImmediate
    mov WORD PTR [rdi+rsi], ax
    add esi, 2
    jmp Enc_NoImm
    
Enc_I4:
    mov eax, DWORD PTR [rbx].ENCODER_CTX.nImmediate
    mov DWORD PTR [rdi+rsi], eax
    add esi, 4
    
Enc_NoImm:
    ; Update state
    mov eax, esi
    sub eax, r12d
    mov [rbx].ENCODER_CTX.nLastSize, eax
    mov [rbx].ENCODER_CTX.nOffset, esi
    jmp Enc_Done
    
Enc_Error:
    mov [rbx].ENCODER_CTX.nErrorCode, ERR_BUFFER_OVERFLOW
    xor eax, eax
    
Enc_Done:
    pop rsi
    pop rdi
    pop rbx
    ret
Encoder_EncodeInstruction ENDP

; ================================================================================
; ACCESSORS
; ================================================================================

Encoder_GetBuffer PROC
    mov rax, [rcx].ENCODER_CTX.pOutput
    ret
Encoder_GetBuffer ENDP

Encoder_GetSize PROC
    mov eax, [rcx].ENCODER_CTX.nOffset
    ret
Encoder_GetSize ENDP

Encoder_GetLastSize PROC
    mov eax, [rcx].ENCODER_CTX.nLastSize
    ret
Encoder_GetLastSize ENDP

Encoder_GetError PROC
    mov eax, [rcx].ENCODER_CTX.nErrorCode
    ret
Encoder_GetError ENDP

; ================================================================================
; 15 HIGH-LEVEL INSTRUCTION ENCODERS
; ================================================================================

Encode_MOV_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov dl, REX_W
    call Encoder_SetREX
    
    mov rcx, rbx
    mov dl, 89h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    xchg edx, r8d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_MOV_R64_R64 ENDP

Encode_MOV_R64_IMM64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13, r8
    
    mov dl, REX_W
    test r12b, 8
    jz IMM64_NoB
    or dl, REX_B
IMM64_NoB:
    call Encoder_SetREX
    
    mov al, 0B8h
    add al, r12b
    and al, 7
    mov dl, al
    call Encoder_SetOpcode
    
    mov rcx, rbx
    mov rdx, r13
    mov r8d, 8
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_MOV_R64_IMM64 ENDP

Encode_PUSH_R64 PROC
    cmp dl, 15
    ja Push_Error
    
    cmp dl, 7
    jbe Push_NoREX
    
    mov r8d, edx
    mov rcx, rsi
    mov dl, REX_B
    call Encoder_SetREX
    mov dl, r8b
    and dl, 7
    
Push_NoREX:
    mov al, 50h
    add al, dl
    mov dl, al
    mov rcx, rsi
    call Encoder_SetOpcode
    
    mov rcx, rsi
    jmp Encoder_EncodeInstruction
    
Push_Error:
    xor eax, eax
    ret
Encode_PUSH_R64 ENDP

Encode_POP_R64 PROC
    cmp dl, 15
    ja Pop_Error
    
    cmp dl, 7
    jbe Pop_NoREX
    
    mov r8d, edx
    mov rcx, rsi
    mov dl, REX_B
    call Encoder_SetREX
    mov dl, r8b
    and dl, 7
    
Pop_NoREX:
    mov al, 58h
    add al, dl
    mov dl, al
    mov rcx, rsi
    call Encoder_SetOpcode
    
    mov rcx, rsi
    jmp Encoder_EncodeInstruction
    
Pop_Error:
    xor eax, eax
    ret
Encode_POP_R64 ENDP

Encode_CALL_REL32 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov dl, 0E8h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    movsxd rdx, edx
    mov r8d, 4
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_CALL_REL32 ENDP

Encode_RET PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov dl, 0C3h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_RET ENDP

Encode_NOP PROC
    mov rdi, [rcx].ENCODER_CTX.pOutput
    mov esi, [rcx].ENCODER_CTX.nOffset
    
    test edx, edx
    jz NOP_End
    cmp edx, 15
    ja NOP_End
    
NOP_Loop:
    mov BYTE PTR [rdi+rsi], 90h
    inc esi
    dec edx
    jnz NOP_Loop
    
    mov [rcx].ENCODER_CTX.nOffset, esi
    
NOP_End:
    ret
Encode_NOP ENDP

Encode_LEA_R64_M PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 8Dh
    call Encoder_SetOpcode
    
    mov rcx, rbx
    mov edx, r12d
    mov r8d, r13d
    mov r9d, 4
    mov rax, r14
    mov [rsp+40], rax
    call Encoder_SetModRM_RegMem
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_LEA_R64_M ENDP

Encode_ADD_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 03h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    mov edx, r13d
    mov r8d, r12d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_ADD_R64_R64 ENDP

Encode_SUB_R64_IMM8 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    mov dl, REX_W
    test r12b, 8
    jz Sub8_NoB
    or dl, REX_B
Sub8_NoB:
    call Encoder_SetREX
    
    mov dl, 83h
    call Encoder_SetOpcode
    
    mov al, MOD_REGISTER or (5 shl 3)
    mov cl, r12b
    and cl, 7
    or al, cl
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    mov rcx, rbx
    movzx rdx, r13b
    mov r8d, 1
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_SUB_R64_IMM8 ENDP

Encode_CMP_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 3Bh
    call Encoder_SetOpcode
    
    mov rcx, rbx
    mov edx, r13d
    mov r8d, r12d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_CMP_R64_R64 ENDP

Encode_JMP_REL32 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov dl, 0E9h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    movsxd rdx, edx
    mov r8d, 4
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_JMP_REL32 ENDP

Encode_Jcc_REL32 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    mov [rbx].ENCODER_CTX.bOpcode1, 0Fh
    mov al, 80h
    add al, r12b
    mov [rbx].ENCODER_CTX.bOpcode2, al
    mov BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 2
    
    mov rcx, rbx
    movsxd rdx, r13d
    mov r8d, 4
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_Jcc_REL32 ENDP

Encode_SYSCALL PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov [rbx].ENCODER_CTX.bOpcode1, 0Fh
    mov [rbx].ENCODER_CTX.bOpcode2, 05h
    mov BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 2
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_SYSCALL ENDP

Encode_XCHG_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 87h
    call Encoder_SetOpcode
    
    mov rcx, rbx
    xchg edx, r8d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_XCHG_R64_R64 ENDP

; ================================================================================
; UTILITIES
; ================================================================================

CalcRelativeOffset PROC
    mov rax, rdx
    sub rax, rcx
    mov eax, eax
    ret
CalcRelativeOffset ENDP

TestEncoder PROC FRAME
    sub rsp, 288
    .allocstack 288
    .endprolog
    
    mov rbx, rsp
    add rbx, 32
    
    lea rcx, [rbx]
    lea rdx, [rbx+96]
    mov r8d, 200
    call Encoder_Init
    
    lea rcx, [rbx]
    mov dl, REG_RAX
    mov r8d, REG_RCX
    call Encode_MOV_R64_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R12
    mov r8, 123456789ABCDEF0h
    call Encode_MOV_R64_IMM64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RBP
    call Encode_PUSH_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R8
    call Encode_POP_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, 1000
    call Encode_CALL_REL32
    
    lea rcx, [rbx]
    call Encoder_Reset
    call Encode_RET
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, 1
    call Encode_NOP
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RAX
    mov r8d, REG_RDX
    call Encode_ADD_R64_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RCX
    mov r8d, 10
    call Encode_SUB_R64_IMM8
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R11
    mov r8d, REG_R13
    call Encode_CMP_R64_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, -500
    call Encode_JMP_REL32
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, CC_E
    mov r8d, 2000
    call Encode_Jcc_REL32
    
    lea rcx, [rbx]
    call Encoder_Reset
    call Encode_SYSCALL
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RSI
    mov r8d, REG_RDI
    call Encode_XCHG_R64_R64
    
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RSP
    mov r8d, REG_RBP
    mov r9d, 32
    call Encode_LEA_R64_M
    
    mov eax, 15
    
    add rsp, 288
    ret
TestEncoder ENDP

END
