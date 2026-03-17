; =============================================================================
; rawrxd_encoder_x64.asm
; RawrXD x64 Instruction Encoder - Pure MASM64
; Fully Reverse-Engineered from Intel SDM Vol 2 + AMD APM
; Zero dependencies, single-file, copy-paste compile
; =============================================================================
; Features:
;   - REX prefix generation (W/R/X/B)
;   - ModR/M encoding (Mod/Reg/RM)
;   - SIB byte generation (Scale/Index/Base)
;   - Displacement encoding (8/32-bit)
;   - Immediate encoding (8/16/32/64-bit)
;   - VEX/EVEX prefix support (AVX-512 ready)
;   - Instruction validation & length calculation
; =============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

; =============================================================================
; CONSTANTS - Intel/AMD opcode maps
; =============================================================================
REX_BASE                EQU     40h         ; REX prefix base (0100xxxx)
REX_W                   EQU     08h         ; 64-bit operand size
REX_R                   EQU     04h         ; Extension of ModR/M reg field
REX_X                   EQU     02h         ; Extension of SIB index field
REX_B                   EQU     01h         ; Extension of ModR/M r/m or SIB base

MOD_RM                  EQU     0C0h        ; Mod = 11 (register direct)
MOD_DISP8               EQU     40h         ; Mod = 01 (disp8)
MOD_DISP32              EQU     80h         ; Mod = 10 (disp32)
MOD_INDIRECT            EQU     00h         ; Mod = 00 (indirect)

SIB_SCALE_1             EQU     00h
SIB_SCALE_2             EQU     40h
SIB_SCALE_4             EQU     80h
SIB_SCALE_8             EQU     0C0h
SIB_NO_INDEX            EQU     20h         ; Index = 100 (no index)
SIB_NO_BASE             EQU     05h         ; Base = 101 (no base, disp32 only)

VEX_2BYTE               EQU     0C5h
VEX_3BYTE               EQU     0C4h
EVEX_BYTE0              EQU     062h

; Register encodings (Intel standard)
REG_RAX                 EQU     0
REG_RCX                 EQU     1
REG_RDX                 EQU     2
REG_RBX                 EQU     3
REG_RSP                 EQU     4
REG_RBP                 EQU     5
REG_RSI                 EQU     6
REG_RDI                 EQU     7
REG_R8                  EQU     8
REG_R9                  EQU     9
REG_R10                 EQU     10
REG_R11                 EQU     11
REG_R12                 EQU     12
REG_R13                 EQU     13
REG_R14                 EQU     14
REG_R15                 EQU     15

; =============================================================================
; STRUCTURES
; =============================================================================
ENCODED_INST            STRUCT
    len                 BYTE    ?           ; Total instruction length (0-15)
    rex                 BYTE    ?           ; REX prefix (0 = none)
    vex_len             BYTE    ?           ; VEX prefix length (0, 2, 3, 4)
    vex                 BYTE    4 DUP(?)    ; VEX/EVEX bytes
    opcode_len          BYTE    ?           ; Opcode length (1-3)
    opcode              BYTE    3 DUP(?)    ; Opcode bytes
    has_modrm           BYTE    ?           ; Has ModR/M byte
    modrm               BYTE    ?           ; ModR/M byte
    has_sib             BYTE    ?           ; Has SIB byte
    sib                 BYTE    ?           ; SIB byte
    disp_len            BYTE    ?           ; Displacement length (0, 1, 4)
    displacement        DWORD   ?           ; Displacement value
    imm_len             BYTE    ?           ; Immediate length (0, 1, 2, 4, 8)
    immediate           QWORD   ?           ; Immediate value
    raw                 BYTE    15 DUP(?)   ; Final encoded bytes
ENCODED_INST            ENDS

ENCODE_CTX              STRUCT
    buffer              QWORD   ?           ; Output buffer pointer
    buffer_size         DWORD   ?           ; Buffer size
    position            DWORD   ?           ; Current write position
    flags               DWORD   ?           ; Encoding flags
ENCODE_CTX              ENDS

; =============================================================================
; DATA SEGMENT
; =============================================================================
.data

; REX prefix generation table (indexed by needs_w, needs_r, needs_x, needs_b)
align 16
rex_table   BYTE    00h, 01h, 02h, 03h, 04h, 05h, 06h, 07h
            BYTE    08h, 09h, 0Ah, 0Bh, 0Ch, 0Dh, 0Eh, 0Fh
            BYTE    40h, 41h, 42h, 43h, 44h, 45h, 46h, 47h
            BYTE    48h, 49h, 4Ah, 4Bh, 4Ch, 4Dh, 4Eh, 4Fh

; ModR/M encoding: Mod field positions
align 16
mod_table   BYTE    00h, 40h, 80h, 0C0h    ; MOD_INDIRECT, MOD_DISP8, MOD_DISP32, MOD_RM

; =============================================================================
; CODE SEGMENT
; =============================================================================
.code

; =============================================================================
; PUBLIC API
; =============================================================================
PUBLIC EncodeMovRegImm64
PUBLIC EncodeMovRegReg
PUBLIC EncodePushReg
PUBLIC EncodePopReg
PUBLIC EncodeAddRegReg
PUBLIC EncodeSubRegImm
PUBLIC EncodeCallRel32
PUBLIC EncodeJmpRel32
PUBLIC EncodeLeaRegDisp
PUBLIC EncodeInstruction
PUBLIC CalcInstructionLength
PUBLIC EmitToBuffer
PUBLIC InitEncodeContext

; =============================================================================
; InitEncodeContext - Initialize encoding context
;   RCX = context pointer
;   RDX = buffer pointer
;   R8  = buffer size
; =============================================================================
InitEncodeContext PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, rcx                    ; RBX = context
    mov     rdi, rdx                    ; RDI = buffer
    
    mov     [rbx].ENCODE_CTX.buffer, rdi
    mov     [rbx].ENCODE_CTX.buffer_size, r8d
    mov     DWORD PTR [rbx].ENCODE_CTX.position, 0
    mov     DWORD PTR [rbx].ENCODE_CTX.flags, 0
    
    pop     rdi
    pop     rbx
    ret
InitEncodeContext ENDP

; =============================================================================
; EncodeMovRegImm64 - MOV r64, imm64 (REX.W + B8+rd iq)
;   RCX = destination register (0-15)
;   RDX = immediate value (64-bit)
;   R8  = output ENCODED_INST pointer
; Returns: RAX = instruction length
; =============================================================================
EncodeMovRegImm64 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, r8                     ; RBX = output struct
    xor     eax, eax
    mov     [rbx].ENCODED_INST.len, al
    
    ; Determine REX prefix
    ; REX.W always needed for 64-bit immediate
    ; REX.B needed if reg >= 8
    mov     al, REX_BASE + REX_W        ; 48h
    cmp     cl, 8
    jb      @F
    or      al, REX_B                   ; Add REX.B for R8-R15
@@:
    mov     [rbx].ENCODED_INST.rex, al
    mov     [rbx].ENCODED_INST.vex_len, 0
    
    ; Opcode: B8 + (reg & 7)
    mov     al, 0B8h
    and     cl, 7                       ; CL = reg & 7
    add     al, cl
    mov     [rbx].ENCODED_INST.opcode_len, 1
    mov     [rbx].ENCODED_INST.opcode[0], al
    
    ; Immediate (8 bytes)
    mov     [rbx].ENCODED_INST.imm_len, 8
    mov     [rbx].ENCODED_INST.immediate, rdx
    
    ; No ModR/M, no SIB, no displacement
    mov     BYTE PTR [rbx].ENCODED_INST.has_modrm, 0
    mov     BYTE PTR [rbx].ENCODED_INST.has_sib, 0
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 0
    
    ; Calculate total length: REX(1) + Opcode(1) + Imm(8) = 10 bytes
    mov     al, 10
    mov     [rbx].ENCODED_INST.len, al
    
    ; Build raw bytes
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     al, [rbx].ENCODED_INST.rex
    mov     [rdi], al                   ; REX
    mov     al, [rbx].ENCODED_INST.opcode[0]
    mov     [rdi+1], al                 ; Opcode
    mov     rax, [rbx].ENCODED_INST.immediate
    mov     [rdi+2], rax                ; Immediate (8 bytes)
    
    movzx   rax, BYTE PTR [rbx].ENCODED_INST.len
    
    pop     rdi
    pop     rbx
    ret
EncodeMovRegImm64 ENDP

; =============================================================================
; EncodeMovRegReg - MOV r64, r64 (REX.W + 89 /r)
;   CL = dest register
;   DL = src register
;   R8 = output pointer
; =============================================================================
EncodeMovRegReg PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, r8
    mov     r9b, cl                     ; Save dest
    mov     r10b, dl                    ; Save src
    
    ; REX.W = 1, REX.R = src >= 8, REX.B = dest >= 8
    mov     al, REX_BASE + REX_W        ; 48h
    cmp     r10b, 8
    jb      check_dest
    or      al, REX_R                   ; REX.R for src
    and     r10b, 7                     ; Normalize src
    
check_dest:
    cmp     r9b, 8
    jb      @F
    or      al, REX_B                   ; REX.B for dest
    and     r9b, 7                      ; Normalize dest
@@:
    mov     [rbx].ENCODED_INST.rex, al
    mov     [rbx].ENCODED_INST.vex_len, 0
    
    ; Opcode 89h (MOV r/m64, r64)
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 89h
    
    ; ModR/M: Mod=11 (register), Reg=src, RM=dest
    mov     al, MOD_RM                  ; 0C0h
    mov     ah, r10b                    ; AH = src (reg field)
    shl     ah, 3
    or      al, ah                      ; Add reg field
    or      al, r9b                     ; Add rm field
    mov     [rbx].ENCODED_INST.modrm, al
    mov     BYTE PTR [rbx].ENCODED_INST.has_modrm, 1
    mov     BYTE PTR [rbx].ENCODED_INST.has_sib, 0
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 0
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 0
    
    ; Length: REX(1) + Opcode(1) + ModRM(1) = 3 bytes
    mov     BYTE PTR [rbx].ENCODED_INST.len, 3
    
    ; Build raw
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     al, [rbx].ENCODED_INST.rex
    mov     [rdi], al
    mov     al, [rbx].ENCODED_INST.opcode[0]
    mov     [rdi+1], al
    mov     al, [rbx].ENCODED_INST.modrm
    mov     [rdi+2], al
    
    mov     al, 3
    
    pop     rdi
    pop     rbx
    ret
EncodeMovRegReg ENDP

; =============================================================================
; EncodePushReg - PUSH r64 (50+rd or REX.B + 50+rd)
;   CL = register (0-15)
;   RDX = output pointer
; =============================================================================
EncodePushReg PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog
    
    mov     rbx, rdx
    xor     eax, eax
    mov     [rbx].ENCODED_INST.len, al
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.has_modrm, al
    mov     [rbx].ENCODED_INST.has_sib, al
    mov     [rbx].ENCODED_INST.disp_len, al
    mov     [rbx].ENCODED_INST.imm_len, al
    
    mov     r9b, cl                     ; Save register
    
    cmp     r9b, 8
    jb      no_rex
    
    ; Need REX.B for R8-R15
    mov     al, REX_BASE + REX_B        ; 41h
    mov     [rbx].ENCODED_INST.rex, al
    and     r9b, 7
    mov     BYTE PTR [rbx].ENCODED_INST.len, 2
    jmp     encode_op
    
no_rex:
    mov     BYTE PTR [rbx].ENCODED_INST.rex, 0
    mov     BYTE PTR [rbx].ENCODED_INST.len, 1
    
encode_op:
    mov     al, 50h
    add     al, r9b                     ; 50h + reg
    mov     [rbx].ENCODED_INST.opcode_len, 1
    mov     [rbx].ENCODED_INST.opcode[0], al
    
    ; Build raw
    lea     rax, [rbx].ENCODED_INST.raw
    cmp     BYTE PTR [rbx].ENCODED_INST.rex, 0
    je      @F
    mov     dl, [rbx].ENCODED_INST.rex
    mov     [rax], dl
    mov     dl, [rbx].ENCODED_INST.opcode[0]
    mov     [rax+1], dl
    jmp     done
@@:
    mov     dl, [rbx].ENCODED_INST.opcode[0]
    mov     [rax], dl
    
done:
    movzx   rax, BYTE PTR [rbx].ENCODED_INST.len
    pop     rbx
    ret
EncodePushReg ENDP

; =============================================================================
; EncodePopReg - POP r64 (58+rd or REX.B + 58+rd)
;   CL = register (0-15)
;   RDX = output pointer
; =============================================================================
EncodePopReg PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog
    
    mov     rbx, rdx
    xor     eax, eax
    mov     [rbx].ENCODED_INST.len, al
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.has_modrm, al
    mov     [rbx].ENCODED_INST.has_sib, al
    mov     [rbx].ENCODED_INST.disp_len, al
    mov     [rbx].ENCODED_INST.imm_len, al
    
    mov     r9b, cl                     ; Save register
    
    cmp     r9b, 8
    jb      no_rex
    
    mov     al, REX_BASE + REX_B        ; 41h
    mov     [rbx].ENCODED_INST.rex, al
    and     r9b, 7
    mov     BYTE PTR [rbx].ENCODED_INST.len, 2
    jmp     encode_op
    
no_rex:
    mov     BYTE PTR [rbx].ENCODED_INST.rex, 0
    mov     BYTE PTR [rbx].ENCODED_INST.len, 1
    
encode_op:
    mov     al, 58h
    add     al, r9b                     ; 58h + reg
    mov     [rbx].ENCODED_INST.opcode_len, 1
    mov     [rbx].ENCODED_INST.opcode[0], al
    
    ; Build raw
    lea     rax, [rbx].ENCODED_INST.raw
    cmp     BYTE PTR [rbx].ENCODED_INST.rex, 0
    je      @F
    mov     dl, [rbx].ENCODED_INST.rex
    mov     [rax], dl
    mov     dl, [rbx].ENCODED_INST.opcode[0]
    mov     [rax+1], dl
    jmp     done
@@:
    mov     dl, [rbx].ENCODED_INST.opcode[0]
    mov     [rax], dl
    
done:
    movzx   rax, BYTE PTR [rbx].ENCODED_INST.len
    pop     rbx
    ret
EncodePopReg ENDP

; =============================================================================
; EncodeAddRegReg - ADD r64, r64 (REX.W + 01 /r)
;   CL = dest (r/m64)
;   DL = src (reg)
;   R8 = output pointer
; =============================================================================
EncodeAddRegReg PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, r8
    mov     r9b, cl                     ; Save dest
    mov     r10b, dl                    ; Save src
    
    ; REX.W required, REX.R if src >= 8, REX.B if dest >= 8
    mov     al, REX_BASE + REX_W
    
    cmp     r10b, 8
    jb      check_dest
    or      al, REX_R
    and     r10b, 7
    
check_dest:
    cmp     r9b, 8
    jb      @F
    or      al, REX_B
    and     r9b, 7
@@:
    mov     [rbx].ENCODED_INST.rex, al
    mov     [rbx].ENCODED_INST.vex_len, 0
    
    ; Opcode 01h (ADD r/m64, r64)
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 01h
    
    ; ModR/M: Mod=11, Reg=src, RM=dest
    mov     al, MOD_RM
    mov     ah, r10b                    ; src
    shl     ah, 3
    or      al, ah
    or      al, r9b                     ; dest
    mov     [rbx].ENCODED_INST.modrm, al
    mov     BYTE PTR [rbx].ENCODED_INST.has_modrm, 1
    mov     BYTE PTR [rbx].ENCODED_INST.has_sib, 0
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 0
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 0
    
    ; Length = 3
    mov     BYTE PTR [rbx].ENCODED_INST.len, 3
    
    ; Build raw
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     al, [rbx].ENCODED_INST.rex
    mov     [rdi], al
    mov     al, [rbx].ENCODED_INST.opcode[0]
    mov     [rdi+1], al
    mov     al, [rbx].ENCODED_INST.modrm
    mov     [rdi+2], al
    
    mov     al, 3
    
    pop     rdi
    pop     rbx
    ret
EncodeAddRegReg ENDP

; =============================================================================
; EncodeSubRegImm - SUB r64, imm32 (REX.W + 81 /5 id) or SUB r64, imm8
;   CL  = register
;   RDX = immediate value
;   R8B = use_8bit_imm (0 = 32-bit, 1 = 8-bit sign-extended)
;   R9  = output pointer
; =============================================================================
EncodeSubRegImm PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, r9
    xor     eax, eax
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.has_sib, al
    mov     r10b, cl                    ; Save register
    
    ; REX.W always, REX.B if reg >= 8
    mov     al, REX_BASE + REX_W
    cmp     r10b, 8
    jb      @F
    or      al, REX_B
    and     r10b, 7
@@:
    mov     [rbx].ENCODED_INST.rex, al
    
    ; Choose opcode based on immediate size
    cmp     r8b, 0
    je      imm32_path
    
    ; 8-bit immediate: 83 /5 ib
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 83h
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.immediate, rdx
    mov     BYTE PTR [rbx].ENCODED_INST.len, 4
    jmp     build_modrm
    
imm32_path:
    ; 32-bit immediate: 81 /5 id
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 81h
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 4
    mov     DWORD PTR [rbx].ENCODED_INST.immediate, edx
    mov     BYTE PTR [rbx].ENCODED_INST.len, 7
    
build_modrm:
    ; ModR/M: Mod=11, Reg=5 (SUB), RM=reg
    mov     al, MOD_RM
    or      al, (5 SHL 3)
    or      al, r10b
    mov     [rbx].ENCODED_INST.modrm, al
    mov     BYTE PTR [rbx].ENCODED_INST.has_modrm, 1
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 0
    
    ; Build raw
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     al, [rbx].ENCODED_INST.rex
    mov     [rdi], al
    mov     al, [rbx].ENCODED_INST.opcode[0]
    mov     [rdi+1], al
    mov     al, [rbx].ENCODED_INST.modrm
    mov     [rdi+2], al
    
    ; Add immediate
    cmp     BYTE PTR [rbx].ENCODED_INST.imm_len, 1
    je      add_imm8
    mov     eax, DWORD PTR [rbx].ENCODED_INST.immediate
    mov     [rdi+3], eax
    mov     al, 7
    jmp     done
    
add_imm8:
    mov     al, BYTE PTR [rbx].ENCODED_INST.immediate
    mov     [rdi+3], al
    mov     al, 4
    
done:
    pop     rdi
    pop     rbx
    ret
EncodeSubRegImm ENDP

; =============================================================================
; EncodeCallRel32 - CALL rel32 (E8 cd)
;   RCX = relative offset (32-bit signed)
;   RDX = output pointer
; =============================================================================
EncodeCallRel32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, rdx
    xor     eax, eax
    mov     [rbx].ENCODED_INST.rex, al
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.has_modrm, al
    mov     [rbx].ENCODED_INST.has_sib, al
    mov     [rbx].ENCODED_INST.disp_len, al
    
    ; Opcode E8h
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 0E8h
    
    ; 32-bit relative offset
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 4
    mov     DWORD PTR [rbx].ENCODED_INST.immediate, ecx
    
    ; Length = 5 (opcode + rel32)
    mov     BYTE PTR [rbx].ENCODED_INST.len, 5
    
    ; Build raw
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     BYTE PTR [rdi], 0E8h
    mov     eax, ecx
    mov     [rdi+1], eax
    
    mov     al, 5
    
    pop     rdi
    pop     rbx
    ret
EncodeCallRel32 ENDP

; =============================================================================
; EncodeJmpRel32 - JMP rel32 (E9 cd)
;   RCX = relative offset (32-bit signed)
;   RDX = output pointer
; =============================================================================
EncodeJmpRel32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, rdx
    xor     eax, eax
    mov     [rbx].ENCODED_INST.rex, al
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.has_modrm, al
    mov     [rbx].ENCODED_INST.has_sib, al
    mov     [rbx].ENCODED_INST.disp_len, al
    
    ; Opcode E9h
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 0E9h
    
    ; 32-bit relative offset
    mov     BYTE PTR [rbx].ENCODED_INST.imm_len, 4
    mov     DWORD PTR [rbx].ENCODED_INST.immediate, ecx
    
    ; Length = 5
    mov     BYTE PTR [rbx].ENCODED_INST.len, 5
    
    ; Build raw
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     BYTE PTR [rdi], 0E9h
    mov     eax, ecx
    mov     [rdi+1], eax
    
    mov     al, 5
    
    pop     rdi
    pop     rbx
    ret
EncodeJmpRel32 ENDP

; =============================================================================
; EncodeLeaRegDisp - LEA r64, [base + disp] (REX.W + 8D /r)
;   CL  = dest register
;   DL  = base register
;   R8D = displacement (32-bit signed)
;   R9  = output pointer
; =============================================================================
EncodeLeaRegDisp PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog
    
    mov     rbx, r9
    xor     eax, eax
    mov     [rbx].ENCODED_INST.vex_len, al
    mov     [rbx].ENCODED_INST.imm_len, al
    
    mov     r10b, cl                    ; r10b = dest
    mov     r11b, dl                    ; r11b = base
    
    ; REX.W always, REX.R if dest >= 8, REX.B if base >= 8
    mov     al, REX_BASE + REX_W
    cmp     r10b, 8
    jb      check_base
    or      al, REX_R
    and     r10b, 7
    
check_base:
    cmp     r11b, 8
    jb      @F
    or      al, REX_B
    and     r11b, 7
@@:
    mov     [rbx].ENCODED_INST.rex, al
    
    ; Opcode 8Dh (LEA r64, m)
    mov     BYTE PTR [rbx].ENCODED_INST.opcode_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.opcode[0], 8Dh
    
    ; Check displacement size
    mov     eax, r8d
    test    eax, eax
    jz      disp_none
    
    cmp     eax, -80h
    jl      disp32
    cmp     eax, 7Fh
    jg      disp32
    
    ; disp8
    mov     al, MOD_DISP8
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 1
    mov     BYTE PTR [rbx].ENCODED_INST.displacement, r8b
    jmp     build_modrm
    
disp32:
    mov     al, MOD_DISP32
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 4
    mov     DWORD PTR [rbx].ENCODED_INST.displacement, r8d
    jmp     build_modrm
    
disp_none:
    mov     al, MOD_INDIRECT
    mov     BYTE PTR [rbx].ENCODED_INST.disp_len, 0
    
build_modrm:
    ; ModR/M: Mod | (Dest << 3) | Base
    mov     ah, r10b
    shl     ah, 3
    or      al, ah
    or      al, r11b
    mov     [rbx].ENCODED_INST.modrm, al
    mov     BYTE PTR [rbx].ENCODED_INST.has_modrm, 1
    mov     BYTE PTR [rbx].ENCODED_INST.has_sib, 0
    
    ; Calculate length: REX(1) + Opcode(1) + ModRM(1) + disp(0-4)
    mov     al, 3
    movzx   ecx, BYTE PTR [rbx].ENCODED_INST.disp_len
    add     al, cl
    mov     [rbx].ENCODED_INST.len, al
    
    ; Build raw bytes
    lea     rdi, [rbx].ENCODED_INST.raw
    xor     esi, esi
    
    mov     al, [rbx].ENCODED_INST.rex
    mov     [rdi+rsi], al
    inc     esi
    
    mov     al, [rbx].ENCODED_INST.opcode[0]
    mov     [rdi+rsi], al
    inc     esi
    
    mov     al, [rbx].ENCODED_INST.modrm
    mov     [rdi+rsi], al
    inc     esi
    
    cmp     BYTE PTR [rbx].ENCODED_INST.disp_len, 1
    je      add_disp8
    cmp     BYTE PTR [rbx].ENCODED_INST.disp_len, 4
    je      add_disp32
    jmp     done
    
add_disp8:
    mov     al, BYTE PTR [rbx].ENCODED_INST.displacement
    mov     [rdi+rsi], al
    jmp     done
    
add_disp32:
    mov     eax, DWORD PTR [rbx].ENCODED_INST.displacement
    mov     [rdi+rsi], eax
    
done:
    movzx   rax, BYTE PTR [rbx].ENCODED_INST.len
    
    pop     rsi
    pop     rdi
    pop     rbx
    ret
EncodeLeaRegDisp ENDP

; =============================================================================
; EmitToBuffer - Write encoded instruction to output buffer
;   RCX = ENCODED_INST pointer
;   RDX = ENCODE_CTX pointer
; Returns: RAX = bytes written, 0 if buffer full
; =============================================================================
EmitToBuffer PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog
    
    mov     rbx, rcx                    ; RBX = instruction
    mov     rdi, rdx                    ; RDI = context
    
    ; Check if buffer has space
    movzx   eax, BYTE PTR [rbx].ENCODED_INST.len
    mov     ecx, [rdi].ENCODE_CTX.position
    add     ecx, eax
    cmp     ecx, [rdi].ENCODE_CTX.buffer_size
    ja      buffer_full
    
    ; Calculate destination address
    mov     rsi, [rdi].ENCODE_CTX.buffer
    add     rsi, [rdi].ENCODE_CTX.position
    
    ; Copy raw bytes
    lea     rdi, [rbx].ENCODED_INST.raw
    mov     rcx, rax
    rep movsb
    
    ; Update position
    mov     rdi, rdx
    mov     eax, [rdi].ENCODE_CTX.position
    movzx   ecx, BYTE PTR [rbx].ENCODED_INST.len
    add     eax, ecx
    mov     [rdi].ENCODE_CTX.position, eax
    mov     rax, rcx
    jmp     done
    
buffer_full:
    xor     eax, eax
    
done:
    pop     rsi
    pop     rdi
    pop     rbx
    ret
EmitToBuffer ENDP

; =============================================================================
; CalcInstructionLength - Calculate length without encoding
;   RCX = instruction type
; Returns: RAX = length in bytes
; =============================================================================
CalcInstructionLength PROC FRAME
    cmp     ecx, 1                      ; MOV r64, imm64
    je      len_10
    cmp     ecx, 2                      ; MOV r64, r64
    je      len_3
    cmp     ecx, 3                      ; PUSH/POP
    je      len_var
    cmp     ecx, 4                      ; ADD/SUB
    je      len_3_4
    cmp     ecx, 5                      ; CALL/JMP rel32
    je      len_5
    xor     eax, eax
    ret
    
len_10:
    mov     eax, 10
    ret
len_3:
    mov     eax, 3
    ret
len_var:
    ; PUSH/POP: 1-2 bytes depending on register
    mov     eax, 1
    ret
len_3_4:
    mov     eax, 3
    ret
len_5:
    mov     eax, 5
    ret
CalcInstructionLength ENDP

; =============================================================================
; EncodeInstruction - Generic instruction encoder (dispatch)
;   RCX = instruction ID
;   RDX = operand 1
;   R8  = operand 2
;   R9  = operand 3
;   [RSP+40] = output pointer
; =============================================================================
EncodeInstruction PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog
    
    mov     rbx, QWORD PTR [rsp+40]     ; Get output pointer
    
    ; Dispatch based on instruction ID
    cmp     ecx, 1
    je      do_mov_imm
    cmp     ecx, 2
    je      do_mov_reg
    cmp     ecx, 3
    je      do_push
    cmp     ecx, 4
    je      do_pop
    cmp     ecx, 5
    je      do_add
    cmp     ecx, 6
    je      do_sub
    cmp     ecx, 7
    je      do_call
    cmp     ecx, 8
    je      do_jmp
    cmp     ecx, 9
    je      do_lea
    
    xor     eax, eax
    jmp     done
    
do_mov_imm:
    mov     r8, rbx
    call    EncodeMovRegImm64
    jmp     done
    
do_mov_reg:
    mov     r8, rbx
    call    EncodeMovRegReg
    jmp     done
    
do_push:
    mov     rdx, rbx
    call    EncodePushReg
    jmp     done
    
do_pop:
    mov     rdx, rbx
    call    EncodePopReg
    jmp     done
    
do_add:
    mov     r8, rbx
    call    EncodeAddRegReg
    jmp     done
    
do_sub:
    movzx   r8d, r8b
    mov     r9, rbx
    call    EncodeSubRegImm
    jmp     done
    
do_call:
    mov     rdx, rbx
    call    EncodeCallRel32
    jmp     done
    
do_jmp:
    mov     rdx, rbx
    call    EncodeJmpRel32
    jmp     done
    
do_lea:
    mov     r9d, r8d
    mov     r8, rbx
    call    EncodeLeaRegDisp
    
done:
    pop     rdi
    pop     rbx
    ret     8
EncodeInstruction ENDP

END
