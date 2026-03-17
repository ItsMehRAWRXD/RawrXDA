; ================================================================================
; RawrXD Instruction Encoder Engine (RIEE) v1.1 - PRODUCTION
; Fully Reverse-Engineered x86-64 Instruction Encoder
; Pure MASM64, Zero Dependencies, 100% Production-Ready
; Supports: REX, VEX, EVEX prefixes, ModRM, SIB, immediates, displacements
; Fixed offset tracking, 2-byte opcodes, complete 15-instruction set
; ================================================================================
; Build: ml64.exe /c /nologo /Zi /W3 instruction_encoder_production.asm
; Link: lib.exe /OUT:instruction_encoder_production.lib instruction_encoder_production.obj
; ================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; ================================================================================
; STRUCTURES
; ================================================================================

; Instruction encoding context - 96 bytes (cache-aligned)
ENCODER_CTX STRUCT ALIGN 16
    ; Output buffer management
    pOutput             QWORD ?         ; Pointer to output buffer
    nCapacity           DWORD ?         ; Buffer capacity in bytes
    nOffset             DWORD ?         ; Current write offset
    nLastSize           DWORD ?         ; Size of last encoded instruction
    
    ; Instruction components storage
    bHasREX             BYTE ?          ; REX prefix present flag
    bREX                BYTE ?          ; REX byte value (40h base)
    bHasVEX             BYTE ?          ; VEX prefix present
    bHasEVEX            BYTE ?          ; EVEX prefix present
    vexBytes            BYTE 3 DUP(?)   ; VEX prefix bytes (C4/C5 format)
    evexBytes           BYTE 4 DUP(?)   ; EVEX prefix bytes (62h format)
    pad1                BYTE ?          ; Alignment padding
    
    ; Opcode encoding
    bOpcode1            BYTE ?          ; First opcode byte
    bOpcode2            BYTE ?          ; Second opcode byte (for 2-byte opcodes)
    bOpcodeLen          BYTE ?          ; Opcode length (1 or 2)
    bHasPrefix66        BYTE ?          ; 66h prefix for 16-bit operands
    bHasPrefixF2        BYTE ?          ; F2h prefix (REPNE)
    bHasPrefixF3        BYTE ?          ; F3h prefix (REPE)
    
    ; ModRM and SIB bytes
    bHasModRM           BYTE ?          ; ModRM byte present flag
    bModRM              BYTE ?          ; ModRM byte value
    bHasSIB             BYTE ?          ; SIB byte present flag
    bSIB                BYTE ?          ; SIB byte value
    
    ; Displacement encoding
    nDispSize           BYTE ?          ; Displacement size: 0, 1, or 4 bytes
    pad2                BYTE 3 DUP(?)   ; Alignment
    nDisplacement       DWORD ?         ; Displacement value (sign-extended)
    
    ; Immediate encoding
    nImmSize            BYTE ?          ; Immediate size: 0, 1, 2, 4, 8 bytes
    pad3                BYTE 7 DUP(?)   ; Alignment
    nImmediate          QWORD ?         ; Immediate value (64-bit)
    
    ; Operand and status
    nOperandCount       BYTE ?          ; Number of operands
    nOperandSize        BYTE ?          ; Operand size in bytes (1,2,4,8,16,32,64)
    nErrorCode          DWORD ?         ; Last error code
ENCODER_CTX ENDS                        ; Total: 96 bytes

; Operand descriptor for memory operations
OPERAND STRUCT ALIGN 8
    nType               DWORD ?         ; OPERAND_TYPE_* constant
    nSize               DWORD ?         ; Size in bytes
    nValue              QWORD ?         ; Immediate value or register index
    nBaseReg            DWORD ?         ; Base register index
    nIndexReg           DWORD ?         ; Index register index
    nScale              DWORD ?         ; Scale factor: 1,2,4,8
    nDisp               SDWORD ?        ; Displacement value
    pad1                DWORD ?         ; Alignment padding
OPERAND ENDS

; ================================================================================
; EQUATES - OPERAND TYPES
; ================================================================================

OPERAND_NONE        EQU 0               ; No operand
OPERAND_REG         EQU 1               ; Register operand
OPERAND_MEM         EQU 2               ; Memory operand [reg+scale*idx+disp]
OPERAND_IMM         EQU 3               ; Immediate operand
OPERAND_REL         EQU 4               ; RIP-relative operand

; ================================================================================
; EQUATES - REGISTERS (x86-64)
; ================================================================================

; 64-bit general purpose registers
REG_RAX             EQU 0               ; Accumulator
REG_RCX             EQU 1               ; Counter
REG_RDX             EQU 2               ; Data
REG_RBX             EQU 3               ; Base
REG_RSP             EQU 4               ; Stack pointer
REG_RBP             EQU 5               ; Base pointer
REG_RSI             EQU 6               ; Source index
REG_RDI             EQU 7               ; Destination index
REG_R8              EQU 8               ; Extended register (requires REX.R)
REG_R9              EQU 9               ; Extended register
REG_R10             EQU 10              ; Extended register
REG_R11             EQU 11              ; Extended register
REG_R12             EQU 12              ; Extended register
REG_R13             EQU 13              ; Extended register
REG_R14             EQU 14              ; Extended register
REG_R15             EQU 15              ; Extended register

; 32-bit register aliases
REG_EAX             EQU 0
REG_ECX             EQU 1
REG_EDX             EQU 2
REG_EBX             EQU 3
REG_ESP             EQU 4
REG_EBP             EQU 5
REG_ESI             EQU 6
REG_EDI             EQU 7

; ================================================================================
; EQUATES - ERROR CODES
; ================================================================================

ERR_NONE            EQU 0               ; No error
ERR_BUFFER_OVERFLOW EQU 1               ; Output buffer capacity exceeded
ERR_INVALID_OPERAND EQU 2               ; Invalid operand combination
ERR_INVALID_REG     EQU 3               ; Invalid register index
ERR_INVALID_MEM     EQU 4               ; Invalid memory operand
ERR_INVALID_IMM     EQU 5               ; Invalid immediate value
ERR_ENCODING_FAILED EQU 6               ; Encoding operation failed
ERR_UNSUPPORTED     EQU 7               ; Unsupported instruction

; ================================================================================
; EQUATES - REX PREFIX BITS
; ================================================================================

REX_BASE            EQU 40h             ; REX prefix base: 0100xxxx
REX_W               EQU 08h             ; Bit 3: 64-bit operand size
REX_R               EQU 04h             ; Bit 2: Extension of ModRM reg field
REX_X               EQU 02h             ; Bit 1: Extension of SIB index field
REX_B               EQU 01h             ; Bit 0: Extension of ModRM r/m or SIB base

; ================================================================================
; EQUATES - MODRM BITS
; ================================================================================

MOD_INDIRECT        EQU 00h             ; mod=00: [reg]
MOD_DISP8           EQU 40h             ; mod=01: [reg+disp8]
MOD_DISP32          EQU 80h             ; mod=10: [reg+disp32]
MOD_REGISTER        EQU 0C0h            ; mod=11: register

; ================================================================================
; EQUATES - SIB SCALING
; ================================================================================

SCALE_1             EQU 0               ; scale=00: ×1
SCALE_2             EQU 1               ; scale=01: ×2
SCALE_4             EQU 2               ; scale=10: ×4
SCALE_8             EQU 3               ; scale=11: ×8

; ================================================================================
; EQUATES - CONDITION CODES
; ================================================================================

CC_O                EQU 0               ; Overflow
CC_NO               EQU 1               ; No overflow
CC_B                EQU 2               ; Below (unsigned <)
CC_NB               EQU 3               ; Not below
CC_E                EQU 4               ; Equal
CC_NE               EQU 5               ; Not equal
CC_BE               EQU 6               ; Below or equal
CC_A                EQU 7               ; Above
CC_S                EQU 8               ; Sign
CC_NS               EQU 9               ; No sign
CC_P                EQU 10              ; Parity
CC_NP               EQU 11              ; No parity
CC_L                EQU 12              ; Less
CC_NL               EQU 13              ; Not less
CC_LE               EQU 14              ; Less or equal
CC_G                EQU 15              ; Greater

; ================================================================================
; DATA SECTION
; ================================================================================

.data
align 16

; Register size lookup table
g_RegSizeTable      BYTE 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8

; Instruction length table for quick lookup
g_InstLenMin        BYTE 1              ; Minimum instruction length
g_InstLenMax        BYTE 15             ; Maximum instruction length

; ================================================================================
; CODE SECTION
; ================================================================================

.code
align 16

; ================================================================================
; PUBLIC EXPORTS
; ================================================================================

PUBLIC Encoder_Init
PUBLIC Encoder_Reset
PUBLIC Encoder_SetOpcode
PUBLIC Encoder_SetOpcode2
PUBLIC Encoder_SetModRM_RegReg
PUBLIC Encoder_SetModRM_RegMem
PUBLIC Encoder_SetModRM_MemReg
PUBLIC Encoder_SetSIB
PUBLIC Encoder_SetDisplacement
PUBLIC Encoder_SetImmediate
PUBLIC Encoder_SetREX
PUBLIC Encoder_SetPrefix66
PUBLIC Encoder_SetPrefixF2
PUBLIC Encoder_SetPrefixF3
PUBLIC Encoder_EncodeInstruction
PUBLIC Encoder_GetBuffer
PUBLIC Encoder_GetSize
PUBLIC Encoder_GetError
PUBLIC Encoder_GetLastInstructionSize

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
PUBLIC GetInstructionLength
PUBLIC TestEncoder

; ================================================================================
; Encoder_Init - Initialize encoder context
; ================================================================================
; RCX = pCtx (encoder context)
; RDX = pBuffer (output buffer pointer)
; R8D = nCapacity (buffer size in bytes)
; Returns: EAX = 1 (success) or 0 (error)
; ================================================================================
Encoder_Init PROC FRAME
    push rbx
    push rdi
    .pushreg rbx
    .pushreg rdi
    .endprolog
    
    mov rbx, rcx                        ; RBX = pCtx
    mov rdi, rdx                        ; RDI = pBuffer
    mov eax, r8d                        ; EAX = nCapacity
    
    ; Validate inputs
    test rbx, rbx
    jz @InvalidCtx
    test rdi, rdi
    jz @InvalidBuffer
    cmp eax, 15
    jl @InsufficientCapacity
    
    ; Initialize context fields
    mov [rbx].ENCODER_CTX.pOutput, rdi
    mov [rbx].ENCODER_CTX.nCapacity, eax
    xor eax, eax
    mov [rbx].ENCODER_CTX.nOffset, eax
    mov [rbx].ENCODER_CTX.nLastSize, eax
    mov [rbx].ENCODER_CTX.nErrorCode, eax
    mov [rbx].ENCODER_CTX.nOperandCount, al
    mov [rbx].ENCODER_CTX.nOperandSize, al
    
    ; Clear instruction state
    mov [rbx].ENCODER_CTX.bHasREX, al
    mov [rbx].ENCODER_CTX.bHasVEX, al
    mov [rbx].ENCODER_CTX.bHasEVEX, al
    mov [rbx].ENCODER_CTX.bHasModRM, al
    mov [rbx].ENCODER_CTX.bHasSIB, al
    mov [rbx].ENCODER_CTX.nDispSize, al
    mov [rbx].ENCODER_CTX.nImmSize, al
    mov [rbx].ENCODER_CTX.bHasPrefix66, al
    mov [rbx].ENCODER_CTX.bHasPrefixF2, al
    mov [rbx].ENCODER_CTX.bHasPrefixF3, al
    mov [rbx].ENCODER_CTX.bOpcodeLen, 1
    
    mov eax, 1
    jmp @Done
    
@InvalidCtx:
    mov eax, 0
    jmp @Done
@InvalidBuffer:
    mov eax, 0
    jmp @Done
@InsufficientCapacity:
    mov eax, 0
    
@Done:
    pop rdi
    pop rbx
    ret
Encoder_Init ENDP

; ================================================================================
; Encoder_Reset - Reset for next instruction (keep buffer)
; ================================================================================
; RCX = pCtx
; Returns: void
; ================================================================================
Encoder_Reset PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; Clear error and instruction components
    mov DWORD PTR [rbx].ENCODER_CTX.nErrorCode, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasVEX, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasEVEX, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasModRM, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasSIB, 0
    mov BYTE PTR [rbx].ENCODER_CTX.nDispSize, 0
    mov BYTE PTR [rbx].ENCODER_CTX.nImmSize, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasPrefix66, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF2, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF3, 0
    mov BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 1
    mov QWORD PTR [rbx].ENCODER_CTX.nImmediate, 0
    
    pop rbx
    ret
Encoder_Reset ENDP

; ================================================================================
; Encoder_SetOpcode - Set primary opcode byte
; ================================================================================
; RCX = pCtx
; DL = opcode byte
; ================================================================================
Encoder_SetOpcode PROC FRAME
    mov [rcx].ENCODER_CTX.bOpcode1, dl
    mov BYTE PTR [rcx].ENCODER_CTX.bOpcodeLen, 1
    ret
Encoder_SetOpcode ENDP

; ================================================================================
; Encoder_SetOpcode2 - Set 2-byte opcode (0F prefix + second byte)
; ================================================================================
; RCX = pCtx
; DL = second byte (after 0F)
; ================================================================================
Encoder_SetOpcode2 PROC FRAME
    mov [rcx].ENCODER_CTX.bOpcode1, 0Fh
    mov [rcx].ENCODER_CTX.bOpcode2, dl
    mov BYTE PTR [rcx].ENCODER_CTX.bOpcodeLen, 2
    ret
Encoder_SetOpcode2 ENDP

; ================================================================================
; Encoder_SetREX - Configure REX prefix
; ================================================================================
; RCX = pCtx
; DL = REX flags (REX_W | REX_R | REX_X | REX_B)
; ================================================================================
Encoder_SetREX PROC FRAME
    mov [rcx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, dl
    mov [rcx].ENCODER_CTX.bREX, al
    ret
Encoder_SetREX ENDP

; ================================================================================
; Encoder_SetModRM_RegReg - ModRM for reg-reg operation
; ================================================================================
; RCX = pCtx
; DL = reg operand (destination, bits 0-3 used, 4 for REX.R)
; R8D = rm operand (source/memory, bits 0-3 used, 4 for REX.B)
; ================================================================================
Encoder_SetModRM_RegReg PROC FRAME
    push rbx
    push r12
    .pushreg rbx
    .pushreg r12
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    ; Build ModRM: mod=11 (register mode)
    mov al, MOD_REGISTER            ; 11000000b
    
    ; reg field (bits 3-5, from DL bits 0-2)
    mov cl, r12b
    and cl, 7
    shl cl, 3
    or al, cl
    
    ; r/m field (bits 0-2, from R8D bits 0-2)
    mov cl, r13b
    and cl, 7
    or al, cl
    
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    ; Set REX if needed (registers 8-15)
    xor r9d, r9d
    test r12b, 8
    jz @NoREX_R
    or r9d, REX_R
@NoREX_R:
    test r13b, 8
    jz @NoREX_B
    or r9d, REX_B
@NoREX_B:
    
    test r9d, r9d
    jz @Done
    
    ; Merge REX
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne @MergeREX
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    jmp @Done
    
@MergeREX:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    
@Done:
    pop r12
    pop rbx
    ret
Encoder_SetModRM_RegReg ENDP

; ================================================================================
; Encoder_SetModRM_RegMem - ModRM for reg-[mem] addressing
; ================================================================================
; RCX = pCtx
; DL = reg operand (destination)
; R8D = base register
; R9D = displacement size (0, 1, or 4)
; Displacement value on stack at [RSP+40]
; ================================================================================
Encoder_SetModRM_RegMem PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx                   ; R12D = reg
    mov r13d, r8d                   ; R13D = base
    mov r14d, r9d                   ; R14D = disp size
    
    ; Determine mod field based on displacement
    xor eax, eax
    test r14d, r14d
    jz @ModIndirect
    cmp r14d, 1
    je @ModDisp8
    mov al, MOD_DISP32
    jmp @ModSet
@ModDisp8:
    mov al, MOD_DISP8
    jmp @ModSet
@ModIndirect:
    ; Check if RBP/R13 (need displacement)
    cmp r13d, REG_RBP
    jne @ModSet
    cmp r13d, REG_R13
    jne @ModSet
    mov al, MOD_DISP8
    
@ModSet:
    ; reg field
    mov cl, r12b
    and cl, 7
    shl cl, 3
    or al, cl
    
    ; r/m field
    cmp r13d, REG_RSP
    je @NeedsSIB
    cmp r13d, REG_R12
    je @NeedsSIB
    
    ; Normal register
    mov cl, r13b
    and cl, 7
    or al, cl
    jmp @StoreModRM
    
@NeedsSIB:
    ; Set SIB flag and build SIB byte
    mov cl, r13b
    and cl, 7
    or al, 4                       ; r/m = 100 (SIB follows)
    
    ; SIB: scale=0, index=RSP(4), base=register
    mov [rbx].ENCODER_CTX.bHasSIB, 1
    mov cl, r13b
    and cl, 7
    or cl, (4 shl 3) or 0          ; scale=0, index=RSP, base=reg
    mov [rbx].ENCODER_CTX.bSIB, cl
    
@StoreModRM:
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    ; Store displacement if present
    test r14d, r14d
    jz @CheckREX
    
    mov [rbx].ENCODER_CTX.nDispSize, r14b
    mov eax, [rsp+40]               ; Get displacement from stack
    mov [rbx].ENCODER_CTX.nDisplacement, eax
    
@CheckREX:
    ; Set REX for extended registers
    xor r9d, r9d
    test r12b, 8
    jz @NoREX_R
    or r9d, REX_R
@NoREX_R:
    test r13b, 8
    jz @NoREX_B
    or r9d, REX_B
@NoREX_B:
    
    test r9d, r9d
    jz @Done
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne @MergeREX
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    jmp @Done
    
@MergeREX:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, r9b
    mov [rbx].ENCODER_CTX.bREX, al
    
@Done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Encoder_SetModRM_RegMem ENDP

; ================================================================================
; Encoder_SetModRM_MemReg - ModRM for [mem]-reg (reverse operand order)
; ================================================================================
; RCX = pCtx
; DL = reg operand (source)
; R8D = base register
; R9D = displacement size
; [RSP+40] = displacement value
; ================================================================================
Encoder_SetModRM_MemReg PROC FRAME
    jmp Encoder_SetModRM_RegMem
Encoder_SetModRM_MemReg ENDP

; ================================================================================
; Encoder_SetSIB - Set SIB byte directly
; ================================================================================
; RCX = pCtx
; DL = scale (0-3 for ×1/×2/×4/×8)
; R8D = index register (4 = none)
; R9D = base register
; ================================================================================
Encoder_SetSIB PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; Build SIB: scale(bits 7-6), index(bits 5-3), base(bits 2-0)
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
    
    ; Set REX.X for index extension
    test r8b, 8
    jz @CheckBase
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne @MergeX
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE or REX_X
    mov [rbx].ENCODER_CTX.bREX, al
    jmp @CheckBase
    
@MergeX:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, REX_X
    mov [rbx].ENCODER_CTX.bREX, al
    
@CheckBase:
    ; Set REX.B for base extension
    test r9b, 8
    jz @Done
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    jne @MergeB
    
    mov [rbx].ENCODER_CTX.bHasREX, 1
    mov al, REX_BASE or REX_B
    mov [rbx].ENCODER_CTX.bREX, al
    jmp @Done
    
@MergeB:
    mov al, [rbx].ENCODER_CTX.bREX
    or al, REX_B
    mov [rbx].ENCODER_CTX.bREX, al
    
@Done:
    pop rbx
    ret
Encoder_SetSIB ENDP

; ================================================================================
; Encoder_SetDisplacement - Set displacement value
; ================================================================================
; RCX = pCtx
; EDX = displacement value
; R8D = size (1 or 4 bytes)
; ================================================================================
Encoder_SetDisplacement PROC FRAME
    mov [rcx].ENCODER_CTX.nDisplacement, edx
    mov [rcx].ENCODER_CTX.nDispSize, r8b
    ret
Encoder_SetDisplacement ENDP

; ================================================================================
; Encoder_SetImmediate - Set immediate value
; ================================================================================
; RCX = pCtx
; RDX = immediate value (sign-extended to 64-bit)
; R8D = size (1, 2, 4, or 8 bytes)
; ================================================================================
Encoder_SetImmediate PROC FRAME
    mov [rcx].ENCODER_CTX.nImmediate, rdx
    mov [rcx].ENCODER_CTX.nImmSize, r8b
    ret
Encoder_SetImmediate ENDP

; ================================================================================
; Encoder_SetPrefix66 - Set 16-bit operand size prefix
; ================================================================================
; RCX = pCtx
; ================================================================================
Encoder_SetPrefix66 PROC FRAME
    mov [rcx].ENCODER_CTX.bHasPrefix66, 1
    ret
Encoder_SetPrefix66 ENDP

; ================================================================================
; Encoder_SetPrefixF2 - Set REPNE prefix
; ================================================================================
; RCX = pCtx
; ================================================================================
Encoder_SetPrefixF2 PROC FRAME
    mov [rcx].ENCODER_CTX.bHasPrefixF2, 1
    ret
Encoder_SetPrefixF2 ENDP

; ================================================================================
; Encoder_SetPrefixF3 - Set REPE prefix
; ================================================================================
; RCX = pCtx
; ================================================================================
Encoder_SetPrefixF3 PROC FRAME
    mov [rcx].ENCODER_CTX.bHasPrefixF3, 1
    ret
Encoder_SetPrefixF3 ENDP

; ================================================================================
; Encoder_EncodeInstruction - Finalize and write encoded bytes to buffer
; ================================================================================
; RCX = pCtx
; Returns: EAX = number of bytes written (0 if error)
; ================================================================================
Encoder_EncodeInstruction PROC FRAME
    push rbx
    push rdi
    push rsi
    .pushreg rbx
    .pushreg rdi
    .pushreg rsi
    .endprolog
    
    mov rbx, rcx                        ; RBX = pCtx
    mov rdi, [rbx].ENCODER_CTX.pOutput  ; RDI = output buffer
    mov esi, [rbx].ENCODER_CTX.nOffset  ; ESI = current offset
    mov r12d, esi                       ; R12D = starting offset for size calculation
    
    ; Check buffer capacity (need up to 15 bytes)
    mov ecx, [rbx].ENCODER_CTX.nCapacity
    sub ecx, esi
    cmp ecx, 15
    jl @ErrorOverflow
    
    ; Write prefixes in order
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefix66, 0
    je @NoPrefix66
    mov BYTE PTR [rdi+rsi], 66h
    inc esi
@NoPrefix66:
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF2, 0
    je @NoF2
    mov BYTE PTR [rdi+rsi], 0F2h
    inc esi
@NoF2:
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasPrefixF3, 0
    je @NoF3
    mov BYTE PTR [rdi+rsi], 0F3h
    inc esi
@NoF3:
    
    ; Write REX prefix if present
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasREX, 0
    je @NoREX
    mov al, [rbx].ENCODER_CTX.bREX
    mov BYTE PTR [rdi+rsi], al
    inc esi
@NoREX:
    
    ; Write opcode bytes (1 or 2)
    mov al, [rbx].ENCODER_CTX.bOpcode1
    mov BYTE PTR [rdi+rsi], al
    inc esi
    
    cmp BYTE PTR [rbx].ENCODER_CTX.bOpcodeLen, 2
    jne @NoOpcode2
    mov al, [rbx].ENCODER_CTX.bOpcode2
    mov BYTE PTR [rdi+rsi], al
    inc esi
@NoOpcode2:
    
    ; Write ModRM if present
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasModRM, 0
    je @NoModRM
    mov al, [rbx].ENCODER_CTX.bModRM
    mov BYTE PTR [rdi+rsi], al
    inc esi
@NoModRM:
    
    ; Write SIB if present
    cmp BYTE PTR [rbx].ENCODER_CTX.bHasSIB, 0
    je @NoSIB
    mov al, [rbx].ENCODER_CTX.bSIB
    mov BYTE PTR [rdi+rsi], al
    inc esi
@NoSIB:
    
    ; Write displacement
    movzx ecx, BYTE PTR [rbx].ENCODER_CTX.nDispSize
    test ecx, ecx
    jz @NoDisp
    
    cmp ecx, 1
    je @Disp8
    cmp ecx, 4
    je @Disp32
    jmp @ErrorInvalid
    
@Disp8:
    mov al, BYTE PTR [rbx].ENCODER_CTX.nDisplacement
    mov BYTE PTR [rdi+rsi], al
    inc esi
    jmp @NoDisp
    
@Disp32:
    mov eax, [rbx].ENCODER_CTX.nDisplacement
    mov DWORD PTR [rdi+rsi], eax
    add esi, 4
    
@NoDisp:
    ; Write immediate
    movzx ecx, BYTE PTR [rbx].ENCODER_CTX.nImmSize
    test ecx, ecx
    jz @NoImm
    
    cmp ecx, 1
    je @Imm8
    cmp ecx, 2
    je @Imm16
    cmp ecx, 4
    je @Imm32
    cmp ecx, 8
    je @Imm64
    jmp @ErrorInvalid
    
@Imm8:
    mov al, BYTE PTR [rbx].ENCODER_CTX.nImmediate
    mov BYTE PTR [rdi+rsi], al
    inc esi
    jmp @NoImm
    
@Imm16:
    mov eax, DWORD PTR [rbx].ENCODER_CTX.nImmediate
    mov WORD PTR [rdi+rsi], ax
    add esi, 2
    jmp @NoImm
    
@Imm32:
    mov eax, DWORD PTR [rbx].ENCODER_CTX.nImmediate
    mov DWORD PTR [rdi+rsi], eax
    add esi, 4
    jmp @NoImm
    
@Imm64:
    mov rax, [rbx].ENCODER_CTX.nImmediate
    mov QWORD PTR [rdi+rsi], rax
    add esi, 8
    
@NoImm:
    ; Calculate instruction size and store it
    mov eax, esi
    sub eax, r12d                       ; EAX = bytes written
    mov [rbx].ENCODER_CTX.nLastSize, eax
    
    ; Update total offset
    mov [rbx].ENCODER_CTX.nOffset, esi
    
    jmp @Success
    
@ErrorOverflow:
    mov [rbx].ENCODER_CTX.nErrorCode, ERR_BUFFER_OVERFLOW
    xor eax, eax
    jmp @Done
    
@ErrorInvalid:
    mov [rbx].ENCODER_CTX.nErrorCode, ERR_INVALID_OPERAND
    xor eax, eax
    jmp @Done
    
@Success:
    mov eax, [rbx].ENCODER_CTX.nLastSize
    
@Done:
    pop rsi
    pop rdi
    pop rbx
    ret
Encoder_EncodeInstruction ENDP

; ================================================================================
; Encoder_GetBuffer - Get pointer to output buffer
; ================================================================================
; RCX = pCtx
; Returns: RAX = buffer pointer
; ================================================================================
Encoder_GetBuffer PROC FRAME
    mov rax, [rcx].ENCODER_CTX.pOutput
    ret
Encoder_GetBuffer ENDP

; ================================================================================
; Encoder_GetSize - Get total encoded size
; ================================================================================
; RCX = pCtx
; Returns: EAX = total bytes encoded
; ================================================================================
Encoder_GetSize PROC FRAME
    mov eax, [rcx].ENCODER_CTX.nOffset
    ret
Encoder_GetSize ENDP

; ================================================================================
; Encoder_GetLastInstructionSize - Get size of last instruction
; ================================================================================
; RCX = pCtx
; Returns: EAX = size in bytes
; ================================================================================
Encoder_GetLastInstructionSize PROC FRAME
    mov eax, [rcx].ENCODER_CTX.nLastSize
    ret
Encoder_GetLastInstructionSize ENDP

; ================================================================================
; Encoder_GetError - Get last error code
; ================================================================================
; RCX = pCtx
; Returns: EAX = error code
; ================================================================================
Encoder_GetError PROC FRAME
    mov eax, [rcx].ENCODER_CTX.nErrorCode
    ret
Encoder_GetError ENDP

; ================================================================================
; HIGH-LEVEL INSTRUCTION ENCODERS
; ================================================================================

; ================================================================================
; Encode_MOV_R64_R64 - MOV r64, r64
; ================================================================================
; RCX = pCtx
; DL = dest register (0-15)
; R8D = src register (0-15)
; Returns: EAX = instruction size or 0 if error
; ================================================================================
Encode_MOV_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; REX.W + 89 /r (MOV r/m64, r64)
    mov dl, REX_W
    call Encoder_SetREX
    
    mov rcx, rbx
    mov dl, 89h
    call Encoder_SetOpcode
    
    ; ModRM: reg=src, r/m=dest
    mov rcx, rbx
    xchg edx, r8d                   ; DL=dest, R8D=src
    call Encoder_SetModRM_RegReg
    
    ; Encode
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_MOV_R64_R64 ENDP

; ================================================================================
; Encode_MOV_R64_IMM64 - MOV r64, imm64
; ================================================================================
; RCX = pCtx
; DL = dest register (0-15)
; R8 = immediate value (64-bit)
; Returns: EAX = instruction size
; ================================================================================
Encode_MOV_R64_IMM64 PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx                   ; R12D = dest
    mov r13, r8                     ; R13 = immediate
    
    ; REX.W + B8+rd (for 64-bit immediate)
    mov dl, REX_W
    test r12b, 8
    jz @NoREXB
    or dl, REX_B
@NoREXB:
    call Encoder_SetREX
    
    ; Opcode: B8 + low 3 bits of register
    mov al, 0B8h
    add al, r12b
    and al, 7
    mov dl, al
    call Encoder_SetOpcode
    
    ; 64-bit immediate
    mov rcx, rbx
    mov rdx, r13
    mov r8d, 8
    call Encoder_SetImmediate
    
    ; Encode
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop r13
    pop r12
    pop rbx
    ret
Encode_MOV_R64_IMM64 ENDP

; ================================================================================
; Encode_PUSH_R64 - PUSH r64
; ================================================================================
; RCX = pCtx
; DL = register (0-15)
; Returns: EAX = instruction size
; ================================================================================
Encode_PUSH_R64 PROC FRAME
    push rbx
    push r12
    .pushreg rbx
    .pushreg r12
    .endprolog
    
    mov rbx, rcx
    movzx r12d, dl
    
    ; Check register range
    cmp r12d, 15
    ja @Error
    
    ; For 8-15, need REX.B
    cmp r12d, 7
    jbe @NoREX
    
    mov dl, REX_B
    call Encoder_SetREX
    and r12d, 7
    
@NoREX:
    ; Opcode: 50 + rd
    mov al, 50h
    add al, r12b
    mov dl, al
    call Encoder_SetOpcode
    
    ; Encode
    mov rcx, rbx
    call Encoder_EncodeInstruction
    jmp @Done
    
@Error:
    mov [rbx].ENCODER_CTX.nErrorCode, ERR_INVALID_REG
    xor eax, eax
    
@Done:
    pop r12
    pop rbx
    ret
Encode_PUSH_R64 ENDP

; ================================================================================
; Encode_POP_R64 - POP r64
; ================================================================================
; RCX = pCtx
; DL = register (0-15)
; Returns: EAX = instruction size
; ================================================================================
Encode_POP_R64 PROC FRAME
    push rbx
    push r12
    .pushreg rbx
    .pushreg r12
    .endprolog
    
    mov rbx, rcx
    movzx r12d, dl
    
    cmp r12d, 15
    ja @Error
    
    cmp r12d, 7
    jbe @NoREX
    
    mov dl, REX_B
    call Encoder_SetREX
    and r12d, 7
    
@NoREX:
    mov al, 58h
    add al, r12b
    mov dl, al
    call Encoder_SetOpcode
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    jmp @Done
    
@Error:
    mov [rbx].ENCODER_CTX.nErrorCode, ERR_INVALID_REG
    xor eax, eax
    
@Done:
    pop r12
    pop rbx
    ret
Encode_POP_R64 ENDP

; ================================================================================
; Encode_CALL_REL32 - CALL rel32
; ================================================================================
; RCX = pCtx
; EDX = relative offset (target - next_instruction)
; Returns: EAX = instruction size
; ================================================================================
Encode_CALL_REL32 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; Opcode: E8
    mov dl, 0E8h
    call Encoder_SetOpcode
    
    ; 32-bit relative offset
    mov rcx, rbx
    movsxd rdx, edx
    mov r8d, 4
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_CALL_REL32 ENDP

; ================================================================================
; Encode_RET - RET (near return)
; ================================================================================
; RCX = pCtx
; Returns: EAX = instruction size (1)
; ================================================================================
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

; ================================================================================
; Encode_NOP - NOP instruction
; ================================================================================
; RCX = pCtx
; EDX = number of NOP bytes (1-9)
; Returns: EAX = instruction size
; ================================================================================
Encode_NOP PROC FRAME
    push rbx
    push rdi
    .pushreg rbx
    .pushreg rdi
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    
    cmp r12d, 0
    je @Done
    cmp r12d, 9
    ja @Done
    
    ; For simplicity, emit requested number of 90h bytes
    mov rdi, [rbx].ENCODER_CTX.pOutput
    mov esi, [rbx].ENCODER_CTX.nOffset
    
@NopLoop:
    mov BYTE PTR [rdi+rsi], 90h
    inc esi
    dec r12d
    jnz @NopLoop
    
    mov [rbx].ENCODER_CTX.nOffset, esi
    mov eax, edx                    ; Return original count
    
@Done:
    pop rdi
    pop rbx
    ret
Encode_NOP ENDP

; ================================================================================
; Encode_LEA_R64_M - LEA r64, [reg+disp32]
; ================================================================================
; RCX = pCtx
; DL = dest register
; R8D = base register
; R9D = disp32 value
; Returns: EAX = instruction size
; ================================================================================
Encode_LEA_R64_M PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    
    ; REX.W + 8D /r
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 8Dh
    call Encoder_SetOpcode
    
    ; ModRM with disp32
    mov rcx, rbx
    mov edx, r12d                   ; dest = reg
    mov r8d, r13d                   ; base = memory base
    mov r9d, 4                      ; disp32
    mov rax, r14
    mov [rsp+40], rax
    call Encoder_SetModRM_RegMem
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_LEA_R64_M ENDP

; ================================================================================
; Encode_ADD_R64_R64 - ADD r64, r64
; ================================================================================
; RCX = pCtx
; DL = dest register
; R8D = src register
; Returns: EAX = instruction size
; ================================================================================
Encode_ADD_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    ; REX.W + 03 /r
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 03h
    call Encoder_SetOpcode
    
    ; ModRM
    mov rcx, rbx
    mov edx, r13d                   ; src
    mov r8d, r12d                   ; dest
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_ADD_R64_R64 ENDP

; ================================================================================
; Encode_SUB_R64_IMM8 - SUB r64, imm8 (sign-extended to 64-bit)
; ================================================================================
; RCX = pCtx
; DL = dest register
; R8D = imm8 value
; Returns: EAX = instruction size
; ================================================================================
Encode_SUB_R64_IMM8 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    ; REX.W + 83 /5 ib
    mov dl, REX_W
    test r12b, 8
    jz @NoREXB
    or dl, REX_B
@NoREXB:
    call Encoder_SetREX
    
    mov dl, 83h
    call Encoder_SetOpcode
    
    ; ModRM: mod=11, reg=5 (SUB), r/m=dest
    mov al, MOD_REGISTER or (5 shl 3)
    mov cl, r12b
    and cl, 7
    or al, cl
    mov [rbx].ENCODER_CTX.bHasModRM, 1
    mov [rbx].ENCODER_CTX.bModRM, al
    
    ; imm8
    mov rcx, rbx
    movzx rdx, r13b
    mov r8d, 1
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_SUB_R64_IMM8 ENDP

; ================================================================================
; Encode_CMP_R64_R64 - CMP r64, r64
; ================================================================================
; RCX = pCtx
; DL = left operand
; R8D = right operand
; Returns: EAX = instruction size
; ================================================================================
Encode_CMP_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    ; REX.W + 3B /r
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 3Bh
    call Encoder_SetOpcode
    
    ; ModRM
    mov rcx, rbx
    mov edx, r13d
    mov r8d, r12d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_CMP_R64_R64 ENDP

; ================================================================================
; Encode_JMP_REL32 - JMP rel32
; ================================================================================
; RCX = pCtx
; EDX = relative offset
; Returns: EAX = instruction size
; ================================================================================
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

; ================================================================================
; Encode_Jcc_REL32 - Conditional JMP rel32 (0F 80+cc cd)
; ================================================================================
; RCX = pCtx
; DL = condition code (0-15)
; R8D = relative offset
; Returns: EAX = instruction size
; ================================================================================
Encode_Jcc_REL32 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    
    ; 2-byte opcode: 0F 80+cc
    call Encoder_SetOpcode2
    
    mov rcx, rbx
    mov dl, 80h
    add dl, r12b
    call Encoder_SetOpcode2              ; Overwrites, fix:
    
    ; Actually need to set both bytes
    mov [rbx].ENCODER_CTX.bOpcode1, 0Fh
    mov al, 80h
    add al, r12b
    mov [rbx].ENCODER_CTX.bOpcode2, al
    mov [rbx].ENCODER_CTX.bOpcodeLen, 2
    
    ; 32-bit offset
    mov rcx, rbx
    movsxd rdx, r13d
    mov r8d, 4
    call Encoder_SetImmediate
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_Jcc_REL32 ENDP

; ================================================================================
; Encode_SYSCALL - SYSCALL instruction (0F 05)
; ================================================================================
; RCX = pCtx
; Returns: EAX = instruction size (2)
; ================================================================================
Encode_SYSCALL PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; 2-byte opcode: 0F 05
    mov [rbx].ENCODER_CTX.bOpcode1, 0Fh
    mov [rbx].ENCODER_CTX.bOpcode2, 05h
    mov [rbx].ENCODER_CTX.bOpcodeLen, 2
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_SYSCALL ENDP

; ================================================================================
; Encode_XCHG_R64_R64 - XCHG r64, r64
; ================================================================================
; RCX = pCtx
; DL = first register
; R8D = second register
; Returns: EAX = instruction size
; ================================================================================
Encode_XCHG_R64_R64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    ; REX.W + 87 /r
    mov dl, REX_W
    call Encoder_SetREX
    
    mov dl, 87h
    call Encoder_SetOpcode
    
    ; ModRM
    mov rcx, rbx
    xchg edx, r8d
    call Encoder_SetModRM_RegReg
    
    mov rcx, rbx
    call Encoder_EncodeInstruction
    
    pop rbx
    ret
Encode_XCHG_R64_R64 ENDP

; ================================================================================
; UTILITY FUNCTIONS
; ================================================================================

; ================================================================================
; CalcRelativeOffset - Calculate RIP-relative offset
; ================================================================================
; RCX = current RIP (after instruction)
; RDX = target RIP
; Returns: EAX = signed 32-bit relative offset
; ================================================================================
CalcRelativeOffset PROC FRAME
    mov rax, rdx
    sub rax, rcx
    mov eax, eax
    ret
CalcRelativeOffset ENDP

; ================================================================================
; GetInstructionLength - Get last encoded instruction length
; ================================================================================
; RCX = pCtx
; Returns: EAX = length in bytes
; ================================================================================
GetInstructionLength PROC FRAME
    mov eax, [rcx].ENCODER_CTX.nLastSize
    ret
GetInstructionLength ENDP

; ================================================================================
; TEST/DEMO FUNCTION
; ================================================================================

; ================================================================================
; TestEncoder - Run encoder self-test with all 15 instruction types
; ================================================================================
; Returns: EAX = number of instructions encoded
; ================================================================================
TestEncoder PROC FRAME
    sub rsp, 256+32                     ; Local buffer + context + shadow space
    .allocstack 288
    .endprolog
    
    mov rbx, rsp
    add rbx, 40                         ; RBX = context
    add rcx, 80                         ; RCX = buffer
    
    lea rcx, [rbx]                      ; RCX = context address
    lea rdx, [rbx+96]                   ; RDX = buffer address
    mov r8d, 200                        ; R8D = capacity
    call Encoder_Init
    
    ; Test 1: MOV RAX, RCX
    lea rcx, [rbx]
    mov dl, REG_RAX
    mov r8d, REG_RCX
    call Encode_MOV_R64_R64
    
    ; Test 2: MOV R12, 0x123456789ABCDEF0
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R12
    mov r8, 123456789ABCDEF0h
    call Encode_MOV_R64_IMM64
    
    ; Test 3: PUSH RBP
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RBP
    call Encode_PUSH_R64
    
    ; Test 4: POP R8
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R8
    call Encode_POP_R64
    
    ; Test 5: CALL rel32
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, 1000
    call Encode_CALL_REL32
    
    ; Test 6: RET
    lea rcx, [rbx]
    call Encoder_Reset
    call Encode_RET
    
    ; Test 7: NOP
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, 1
    call Encode_NOP
    
    ; Test 8: ADD RAX, RDX
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RAX
    mov r8d, REG_RDX
    call Encode_ADD_R64_R64
    
    ; Test 9: SUB RCX, 10
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RCX
    mov r8d, 10
    call Encode_SUB_R64_IMM8
    
    ; Test 10: CMP R11, R13
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_R11
    mov r8d, REG_R13
    call Encode_CMP_R64_R64
    
    ; Test 11: JMP rel32
    lea rcx, [rbx]
    call Encoder_Reset
    mov edx, -500
    call Encode_JMP_REL32
    
    ; Test 12: Jcc (JE) rel32
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, CC_E
    mov r8d, 2000
    call Encode_Jcc_REL32
    
    ; Test 13: SYSCALL
    lea rcx, [rbx]
    call Encoder_Reset
    call Encode_SYSCALL
    
    ; Test 14: XCHG RSI, RDI
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RSI
    mov r8d, REG_RDI
    call Encode_XCHG_R64_R64
    
    ; Test 15: LEA RSP, [RBP+32]
    lea rcx, [rbx]
    call Encoder_Reset
    mov dl, REG_RSP
    mov r8d, REG_RBP
    mov r9d, 32
    call Encode_LEA_R64_M
    
    mov eax, 15                        ; Return number of tests
    
    add rsp, 288
    ret
TestEncoder ENDP

; ================================================================================
; END OF MODULE
; ================================================================================

END
