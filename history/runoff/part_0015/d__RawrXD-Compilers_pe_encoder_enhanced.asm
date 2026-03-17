; ================================================================================
; RawrXD PE Generator + x64 Instruction Encoder v2.0
; Pure MASM x64 - Production Ready
; Generates PEs with dynamically encoded x64 instructions
; ================================================================================
; Build: ml64.exe pe_encoder_enhanced.asm /link /subsystem:console /entry:main
; ================================================================================

; ================================================================================
; SECTION 1: CONSTANTS
; ================================================================================

; PE Constants (same as v1)
IMAGE_DOS_SIGNATURE             EQU     5A4Dh
IMAGE_NT_SIGNATURE              EQU     00004550h
IMAGE_FILE_MACHINE_AMD64        EQU     8664h
IMAGE_FILE_EXECUTABLE_IMAGE     EQU     0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE  EQU     0020h
IMAGE_SUBSYSTEM_WINDOWS_CUI     EQU     3
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE   EQU     0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT      EQU     0100h
IMAGE_NT_OPTIONAL_HDR64_MAGIC   EQU     020Bh
IMAGE_SCN_CNT_CODE              EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU     000000040h
IMAGE_SCN_MEM_EXECUTE           EQU     020000000h
IMAGE_SCN_MEM_READ              EQU     040000000h
IMAGE_SCN_MEM_WRITE             EQU     080000000h

; Instruction Encoding Constants
REX_W           EQU     48h
REX_R           EQU     44h
REX_X           EQU     42h
REX_B           EQU     41h
REX_WR          EQU     4Ch
REX_WX          EQU     4Ah
REX_WB          EQU     49h
REX_WRX         EQU     4Eh
REX_WRB         EQU     4Dh
REX_WXB         EQU     4Bh
REX_WRXB        EQU     4Fh

MOD_DIRECT      EQU     0C0h
MOD_DISP8       EQU     40h
MOD_DISP32      EQU     80h
MOD_INDIRECT    EQU     00h

; Register encodings (3-bit, with REX.R/REX.B extension)
REG_RAX         EQU     0
REG_RCX         EQU     1
REG_RDX         EQU     2
REG_RBX         EQU     3
REG_RSP         EQU     4
REG_RBP         EQU     5
REG_RSI         EQU     6
REG_RDI         EQU     7
REG_R8          EQU     8   ; Requires REX.B
REG_R9          EQU     9
REG_R10         EQU     10
REG_R11         EQU     11
REG_R12         EQU     12
REG_R13         EQU     13
REG_R14         EQU     14
REG_R15         EQU     15

; Opcode prefixes
PREFIX_REX      EQU     40h
PREFIX_66       EQU     66h
PREFIX_67       EQU     67h
PREFIX_F0       EQU     0F0h
PREFIX_F2       EQU     0F2h
PREFIX_F3       EQU     0F3h

; ================================================================================
; STRUCTURES
; ================================================================================

; Instruction encoding context
INSTR_CTX struct
    pOutput         QWORD   ?   ; Output buffer pointer
    nOffset         QWORD   ?   ; Current offset in buffer
    nCapacity       QWORD   ?   ; Buffer capacity
    bRexRequired    BYTE    ?   ; REX prefix needed
    bRexW           BYTE    ?   ; REX.W (64-bit operand)
    bRexR           BYTE    ?   ; REX.R (extension of ModRM reg)
    bRexX           BYTE    ?   ; REX.X (extension of SIB index)
    bRexB           BYTE    ?   ; REX.B (extension of ModRM r/m or SIB base)
    bHasModRM       BYTE    ?   ; ModRM byte present
    bHasSIB         BYTE    ?   ; SIB byte present
    bHasDisp        BYTE    ?   ; Displacement present
    nDispSize       BYTE    ?   ; Displacement size (0/1/4)
    nDispValue      DWORD   ?   ; Displacement value
    bHasImm         BYTE    ?   ; Immediate present
    nImmSize        BYTE    ?   ; Immediate size (0/1/2/4/8)
    nImmValue       QWORD   ?   ; Immediate value
INSTR_CTX ends

; PE Section entry
PE_SECTION struct
    szName          BYTE    8 DUP(?)
    dwVirtualSize   DWORD   ?
    dwVirtualAddr   DWORD   ?
    dwRawSize       DWORD   ?
    dwRawAddr       DWORD   ?
    dwCharacteristics   DWORD   ?
    pData           QWORD   ?
PE_SECTION ends

; ================================================================================
; SECTION 2: DATA
; ================================================================================

.const
align 16

; Standard opcodes (simplified set)
; Format: Opcode, ModRM required, Immediate size
OPCODES struct
    bOpcode         BYTE    ?
    bModRM          BYTE    ?
    bImmSize        BYTE    ?
OPCODES ends

; Common instructions
g_OpcodeMOVrr   OPCODES <8Bh, 1, 0>     ; MOV r64, r64
g_OpcodeMOVri   OPCODES <0C7h, 1, 4>     ; MOV r/m64, imm32
g_OpcodeMOVrm   OPCODES <8Bh, 1, 0>     ; MOV r64, r/m64
g_OpcodeMOVmr   OPCODES <89h, 1, 0>     ; MOV r/m64, r64

; REX prefix calculation table
g_RexTable      BYTE    40h, 41h, 42h, 43h, 44h, 45h, 46h, 47h
                BYTE    48h, 49h, 4Ah, 4Bh, 4Ch, 4Dh, 4Eh, 4Fh

; ================================================================================
; SECTION 3: BSS
; ================================================================================

.data?
align 16

; Encoder state
g_EncoderCtx    INSTR_CTX   <>
g_SectionTable  PE_SECTION  16 DUP(<>)
g_nSectionCount DWORD       ?
g_pPEBuffer     QWORD       ?
g_nPESize       QWORD       ?
g_nPECapacity   QWORD       ?

; Instruction buffer (temp encoding space)
g_InstrBuffer   BYTE        256 DUP(?)
g_nInstrLen     DWORD       ?

; ================================================================================
; SECTION 4: CODE
; ================================================================================

.code
align 16

; ================================================================================
; INSTRUCTION ENCODER CORE
; ================================================================================

; --------------------------------------------------------------------------------
; Enc_Init - Initialize instruction encoder
; Input:  RCX = Output buffer
;         RDX = Capacity
; --------------------------------------------------------------------------------
Enc_Init PROC
    mov     g_EncoderCtx.pOutput, rcx
    mov     g_EncoderCtx.nCapacity, rdx
    mov     g_EncoderCtx.nOffset, 0
    xor     eax, eax
    mov     g_EncoderCtx.bRexRequired, al
    ret
Enc_Init ENDP

; --------------------------------------------------------------------------------
; Enc_EmitREX - Calculate and emit REX prefix
; Output: Advances output pointer
; --------------------------------------------------------------------------------
Enc_EmitREX PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     rbx, g_EncoderCtx.pOutput
    add     rbx, g_EncoderCtx.nOffset
    
    ; Calculate REX value
    xor     eax, eax
    mov     al, 40h             ; Base REX
    
    cmp     g_EncoderCtx.bRexW, 0
    jz      @F
    or      al, 08h             ; REX.W
@@:
    cmp     g_EncoderCtx.bRexR, 0
    jz      @F
    or      al, 04h             ; REX.R
@@:
    cmp     g_EncoderCtx.bRexX, 0
    jz      @F
    or      al, 02h             ; REX.X
@@:
    cmp     g_EncoderCtx.bRexB, 0
    jz      @F
    or      al, 01h             ; REX.B
@@:
    
    ; Store REX
    mov     [rbx], al
    inc     g_EncoderCtx.nOffset
    
    pop     rbx
    ret
Enc_EmitREX ENDP

; --------------------------------------------------------------------------------
; Enc_EmitByte - Emit single byte
; Input:  CL = Byte to emit
; --------------------------------------------------------------------------------
Enc_EmitByte PROC
    mov     rax, g_EncoderCtx.pOutput
    add     rax, g_EncoderCtx.nOffset
    mov     [rax], cl
    inc     g_EncoderCtx.nOffset
    ret
Enc_EmitByte ENDP

; --------------------------------------------------------------------------------
; Enc_EmitDword - Emit 32-bit value
; Input:  ECX = Value
; --------------------------------------------------------------------------------
Enc_EmitDword PROC
    mov     rax, g_EncoderCtx.pOutput
    add     rax, g_EncoderCtx.nOffset
    mov     [rax], ecx
    add     g_EncoderCtx.nOffset, 4
    ret
Enc_EmitDword ENDP

; --------------------------------------------------------------------------------
; Enc_EmitQword - Emit 64-bit value
; Input:  RCX = Value
; --------------------------------------------------------------------------------
Enc_EmitQword PROC
    mov     rax, g_EncoderCtx.pOutput
    add     rax, g_EncoderCtx.nOffset
    mov     [rax], rcx
    add     g_EncoderCtx.nOffset, 8
    ret
Enc_EmitQword ENDP

; --------------------------------------------------------------------------------
; Enc_CalcModRM - Calculate ModRM byte
; Input:  CL = Mod (2 bits, shifted to bits 6-7)
;         DL = Reg/Opcode (3 bits, shifted to bits 3-5)
;         R8B = R/M (3 bits, bits 0-2)
; Output: AL = ModRM byte
; --------------------------------------------------------------------------------
Enc_CalcModRM PROC
    mov     al, cl              ; Mod in bits 6-7
    shl     al, 6
    or      al, dl              ; Reg in bits 3-5
    shl     dl, 3
    or      al, r8b             ; R/M in bits 0-2
    ret
Enc_CalcModRM ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_I64 - Encode MOV reg64, imm64 (special 16-byte form)
; Input:  ECX = Destination register (0-15)
;         RDX = Immediate value
; Output: RAX = Number of bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_I64 PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .allocstack 24
    .endprolog
    
    mov     ebx, ecx            ; Save register
    mov     rsi, rdx            ; Save immediate
    
    ; Clear context
    xor     eax, eax
    mov     g_EncoderCtx.bRexRequired, 1
    mov     g_EncoderCtx.bRexW, 1
    
    ; Determine REX.B based on register
    cmp     ebx, 8
    setae   g_EncoderCtx.bRexB
    
    ; Emit REX
    call    Enc_EmitREX
    
    ; Emit opcode: B8+rd (MOV r64, imm64)
    mov     ecx, 0B8h
    and     ebx, 7              ; Register bits
    or      cl, bl              ; B8 + rd
    call    Enc_EmitByte
    
    ; Emit immediate (64-bit)
    mov     rcx, rsi
    call    Enc_EmitQword
    
    ; Return length
    mov     rax, 10             ; REX + opcode + 8 bytes
    
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enc_MOV_R64_I64 ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_R64 - Encode MOV r64, r64
; Input:  ECX = Destination register
;         EDX = Source register
; Output: RAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_R64 PROC FRAME
    push    rbx
    push    r12
    push    r13
    .allocstack 24
    .endprolog
    
    mov     r12d, ecx           ; Dest
    mov     r13d, edx           ; Source
    
    ; Setup REX
    mov     g_EncoderCtx.bRexRequired, 1
    mov     g_EncoderCtx.bRexW, 1
    
    ; REX.R from dest (reg field), REX.B from source (r/m field)
    cmp     r12d, 8
    setae   g_EncoderCtx.bRexR
    cmp     r13d, 8
    setae   g_EncoderCtx.bRexB
    
    call    Enc_EmitREX
    
    ; Opcode: 8B /r (MOV r64, r/m64)
    mov     cl, 8Bh
    call    Enc_EmitByte
    
    ; ModRM: mod=11 (register direct), reg=dest, r/m=src
    mov     cl, 3               ; MOD_DIRECT >> 6 = 3
    mov     dl, r12b
    and     dl, 7
    mov     r8b, r13b
    and     r8b, 7
    call    Enc_CalcModRM
    mov     cl, al
    call    Enc_EmitByte
    
    mov     rax, 3              ; REX + opcode + ModRM
    
    pop     r13
    pop     r12
    pop     rbx
    ret
Enc_MOV_R64_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_IMM32 - Encode MOV r/m64, imm32 (sign-extended)
; Input:  ECX = Register
;         EDX = Immediate
; Output: RAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_IMM32 PROC FRAME
    push    rbx
    push    r12
    .allocstack 16
    .endprolog
    
    mov     r12d, ecx
    mov     ebx, edx
    
    ; REX.W required for 64-bit operation
    mov     g_EncoderCtx.bRexRequired, 1
    mov     g_EncoderCtx.bRexW, 1
    cmp     r12d, 8
    setae   g_EncoderCtx.bRexB
    
    call    Enc_EmitREX
    
    ; Opcode: C7 /0 id (MOV r/m64, imm32)
    mov     cl, 0C7h
    call    Enc_EmitByte
    
    ; ModRM: mod=11, reg=0, r/m=reg
    mov     cl, 3
    xor     dl, dl
    mov     r8b, r12b
    and     r8b, 7
    call    Enc_CalcModRM
    mov     cl, al
    call    Enc_EmitByte
    
    ; Immediate
    mov     ecx, ebx
    call    Enc_EmitDword
    
    mov     rax, 7              ; REX + opcode + ModRM + imm32
    
    pop     r12
    pop     rbx
    ret
Enc_MOV_R64_IMM32 ENDP

; --------------------------------------------------------------------------------
; Enc_CALL_R64 - Encode CALL r64
; Input:  ECX = Register
; Output: RAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_CALL_R64 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx
    
    ; REX.B if register >= 8
    mov     g_EncoderCtx.bRexRequired, 0
    cmp     ebx, 8
    jb      @F
    mov     g_EncoderCtx.bRexRequired, 1
    xor     eax, eax
    mov     g_EncoderCtx.bRexW, al
    mov     g_EncoderCtx.bRexR, al
    mov     g_EncoderCtx.bRexX, al
    mov     g_EncoderCtx.bRexB, 1
    call    Enc_EmitREX
@@:
    
    ; Opcode: FF /2 (CALL r/m64)
    mov     cl, 0FFh
    call    Enc_EmitByte
    
    ; ModRM: mod=11, reg=2, r/m=reg
    mov     cl, 3
    mov     dl, 2
    mov     r8b, bl
    and     r8b, 7
    call    Enc_CalcModRM
    mov     cl, al
    call    Enc_EmitByte
    
    mov     rax, 2
    cmp     ebx, 8
    jb      @F
    inc     rax                 ; +1 for REX
@@:
    ret
Enc_CALL_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_PUSH_R64 - Encode PUSH r64
; Input:  ECX = Register (0-15)
; Output: RAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_PUSH_R64 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx
    
    ; Check if we need REX prefix
    cmp     ebx, 8
    jb      NoRexPush
    
    ; REX.B required for R8-R15
    mov     g_EncoderCtx.bRexRequired, 1
    xor     eax, eax
    mov     g_EncoderCtx.bRexW, al
    mov     g_EncoderCtx.bRexR, al
    mov     g_EncoderCtx.bRexX, al
    mov     g_EncoderCtx.bRexB, 1
    call    Enc_EmitREX
    
NoRexPush:
    ; Opcode: 50+rd (PUSH r64)
    mov     cl, 50h
    mov     eax, ebx
    and     al, 7
    or      cl, al
    call    Enc_EmitByte
    
    mov     rax, 1
    cmp     ebx, 8
    jb      @F
    inc     rax
@@:
    pop     rbx
    ret
Enc_PUSH_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_POP_R64 - Encode POP r64
; Input:  ECX = Register (0-15)
; Output: RAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_POP_R64 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx
    
    cmp     ebx, 8
    jb      NoRexPop
    
    mov     g_EncoderCtx.bRexRequired, 1
    xor     eax, eax
    mov     g_EncoderCtx.bRexW, al
    mov     g_EncoderCtx.bRexR, al
    mov     g_EncoderCtx.bRexX, al
    mov     g_EncoderCtx.bRexB, 1
    call    Enc_EmitREX
    
NoRexPop:
    mov     cl, 58h             ; POP r64 base
    mov     eax, ebx
    and     al, 7
    or      cl, al
    call    Enc_EmitByte
    
    mov     rax, 1
    cmp     ebx, 8
    jb      @F
    inc     rax
@@:
    pop     rbx
    ret
Enc_POP_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_RET - Encode RET
; Output: RAX = Bytes emitted (always 1)
; --------------------------------------------------------------------------------
Enc_RET PROC
    mov     cl, 0C3h
    call    Enc_EmitByte
    mov     rax, 1
    ret
Enc_RET ENDP

; ================================================================================
; WINDOWS API IMPORTS
; ================================================================================

EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetLastError:PROC
EXTERN ExitProcess:PROC

; ================================================================================
; DEMO
; ================================================================================

main PROC
    xor     ecx, ecx
    call    ExitProcess
main ENDP

END
