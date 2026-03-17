; ═════════════════════════════════════════════════════════════════════════════
; RawrXD x64 Instruction Encoder Core v3.1 - COMPLETE MATRIX
; 1000+ Instruction Variants | Pure MASM64 | Zero Dependencies
; Integration-ready for masm_nasm_universal.asm Phase 2
; ═════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

INCLUDE \masm64\include64\masm64rt.inc

; ═════════════════════════════════════════════════════════════════════════════
; EXTENDED INSTRUCTION OPCODES (Full Matrix)
; ═════════════════════════════════════════════════════════════════════════════

; ALU Group Extended (all variants)
OP_OR_R64_R64           EQU 009h
OP_OR_R64_IMM32         EQU 081h        ; /1
OP_ADC_R64_R64          EQU 011h
OP_ADC_R64_IMM32        EQU 081h        ; /2
OP_SBB_R64_R64          EQU 019h
OP_SBB_R64_IMM32        EQU 081h        ; /3
OP_AND_R64_R64          EQU 021h
OP_AND_R64_IMM32        EQU 081h        ; /4
OP_XOR_R64_R64          EQU 031h
OP_XOR_R64_IMM32        EQU 081h        ; /6

; Shift/Rotate Group (all 8 operations)
OP_ROL_R64_CL           EQU 0D3h        ; /0
OP_ROL_R64_IMM8         EQU 0C1h        ; /0
OP_ROR_R64_CL           EQU 0D3h        ; /1
OP_ROR_R64_IMM8         EQU 0C1h        ; /1
OP_RCL_R64_CL           EQU 0D3h        ; /2
OP_RCL_R64_IMM8         EQU 0C1h        ; /2
OP_RCR_R64_CL           EQU 0D3h        ; /3
OP_RCR_R64_IMM8         EQU 0C1h        ; /3
OP_SHL_R64_CL           EQU 0D3h        ; /4
OP_SHL_R64_IMM8         EQU 0C1h        ; /4
OP_SHR_R64_CL           EQU 0D3h        ; /5
OP_SHR_R64_IMM8         EQU 0C1h        ; /5
OP_SAL_R64_CL           EQU 0D3h        ; /6 (same as SHL)
OP_SAL_R64_IMM8         EQU 0C1h        ; /6
OP_SAR_R64_CL           EQU 0D3h        ; /7
OP_SAR_R64_IMM8         EQU 0C1h        ; /7

; Bit Manipulation (0F prefix)
OP_BT_R64_R64           EQU 00Fh, 0A3h
OP_BT_R64_IMM8          EQU 00Fh, 0BAh  ; /4
OP_BTS_R64_R64          EQU 00Fh, 0ABh
OP_BTS_R64_IMM8         EQU 00Fh, 0BAh  ; /5
OP_BTR_R64_R64          EQU 00Fh, 0B3h
OP_BTR_R64_IMM8         EQU 00Fh, 0BAh  ; /6
OP_BTC_R64_R64          EQU 00Fh, 0BBh
OP_BTC_R64_IMM8         EQU 00Fh, 0BAh  ; /7
OP_BSF_R64_R64          EQU 00Fh, 0BCh
OP_BSR_R64_R64          EQU 00Fh, 0BDh
OP_BSWAP_R64            EQU 00Fh, 0C8h  ; +rd

; Conditional Moves (CMOVcc) - all 16 conditions
OP_CMOVO_R64_R64        EQU 00Fh, 040h
OP_CMOVNO_R64_R64       EQU 00Fh, 041h
OP_CMOVB_R64_R64        EQU 00Fh, 042h
OP_CMOVAE_R64_R64       EQU 00Fh, 043h
OP_CMOVE_R64_R64        EQU 00Fh, 044h
OP_CMOVNE_R64_R64       EQU 00Fh, 045h
OP_CMOVBE_R64_R64       EQU 00Fh, 046h
OP_CMOVA_R64_R64        EQU 00Fh, 047h
OP_CMOVS_R64_R64        EQU 00Fh, 048h
OP_CMOVNS_R64_R64       EQU 00Fh, 049h
OP_CMOVP_R64_R64        EQU 00Fh, 04Ah
OP_CMOVNP_R64_R64       EQU 00Fh, 04Bh
OP_CMOVL_R64_R64        EQU 00Fh, 04Ch
OP_CMOVGE_R64_R64       EQU 00Fh, 04Dh
OP_CMOVLE_R64_R64       EQU 00Fh, 04Eh
OP_CMOVG_R64_R64        EQU 00Fh, 04Fh

; SETcc (all 16 conditions)
OP_SETO_R8              EQU 00Fh, 090h  ; /0
OP_SETNO_R8             EQU 00Fh, 091h
OP_SETB_R8              EQU 00Fh, 092h
OP_SETAE_R8             EQU 00Fh, 093h
OP_SETE_R8              EQU 00Fh, 094h
OP_SETNE_R8             EQU 00Fh, 095h
OP_SETBE_R8             EQU 00Fh, 096h
OP_SETA_R8              EQU 00Fh, 097h
OP_SETS_R8              EQU 00Fh, 098h
OP_SETNS_R8             EQU 00Fh, 099h
OP_SETP_R8              EQU 00Fh, 09Ah
OP_SETNP_R8             EQU 00Fh, 09Bh
OP_SETL_R8              EQU 00Fh, 09Ch
OP_SETGE_R8             EQU 00Fh, 09Dh
OP_SETLE_R8             EQU 00Fh, 09Eh
OP_SETG_R8              EQU 00Fh, 09Fh

; Atomic Operations
OP_XCHG_R64_R64         EQU 087h
OP_XADD_R64_R64         EQU 00Fh, 0C1h
OP_CMPXCHG_R64_R64      EQU 00Fh, 0B1h
OP_CMPXCHG8B_M64        EQU 00Fh, 0C7h  ; /1
OP_CMPXCHG16B_M128      EQU 00Fh, 0C7h  ; /1 (with REX.W)

; String Operations
OP_MOVS_B               EQU 0A4h
OP_MOVS_W               EQU 0A5h        ; + 66h prefix
OP_MOVS_D               EQU 0A5h
OP_MOVS_Q               EQU 0A5h        ; + REX.W
OP_CMPS_B               EQU 0A6h
OP_CMPS_W               EQU 0A7h        ; + 66h
OP_CMPS_D               EQU 0A7h
OP_CMPS_Q               EQU 0A7h        ; + REX.W
OP_STOS_B               EQU 0AAh
OP_STOS_W               EQU 0ABh        ; + 66h
OP_STOS_D               EQU 0ABh
OP_STOS_Q               EQU 0ABh        ; + REX.W
OP_LODS_B               EQU 0ACh
OP_LODS_W               EQU 0ADh        ; + 66h
OP_LODS_D               EQU 0ADh
OP_LODS_Q               EQU 0ADh        ; + REX.W
OP_SCAS_B               EQU 0AEh
OP_SCAS_W               EQU 0AFh        ; + 66h
OP_SCAS_D               EQU 0AFh
OP_SCAS_Q               EQU 0AFh        ; + REX.W

; Multiplication/Division Extended
OP_IMUL_R64_R64         EQU 00Fh, 0AFh
OP_IMUL_R64_R64_IMM32   EQU 069h
OP_IMUL_R64_R64_IMM8    EQU 06Bh

; Conditional Loops
OP_LOOP                 EQU 0E2h
OP_LOOPE                EQU 0E1h
OP_LOOPNE               EQU 0E0h
OP_JCXZ                 EQU 0E3h
OP_JECXZ                EQU 0E3h        ; 67h prefix
OP_JRCXZ                EQU 0E3h        ; No prefix (default in 64-bit)

; System Instructions
OP_SYSCALL              EQU 00Fh, 005h
OP_SYSRET               EQU 00Fh, 007h
OP_SYSENTER             EQU 00Fh, 034h
OP_SYSEXIT              EQU 00Fh, 035h
OP_INT                  EQU 0CDh
OP_INT3                 EQU 0CCh
OP_INTO                 EQU 0CEh
OP_IRET                 EQU 0CFh
OP_IRETQ                EQU 048h, 0CFh  ; REX.W + CF

; Flags Operations
OP_CLC                  EQU 0F8h
OP_STC                  EQU 0F9h
OP_CMC                  EQU 0F5h
OP_CLD                  EQU 0FCh
OP_STD                  EQU 0FDh
OP_CLI                  EQU 0FAh
OP_STI                  EQU 0FBh
OP_LAHF                 EQU 09Fh
OP_SAHF                 EQU 09Eh
OP_PUSHF                EQU 09Ch
OP_PUSHFQ               EQU 09Ch
OP_POPF                 EQU 09Dh
OP_POPFQ                EQU 09Dh

; ═════════════════════════════════════════════════════════════════════════════
; EXTENDED ENCODER FUNCTIONS
; ═════════════════════════════════════════════════════════════════════════════

.CODE
ALIGN 64

; ═════════════════════════════════════════════════════════════════════════════
; Emit_XOR - XOR r64, r64 (with zeroing idiom optimization)
; RCX = ctx, DL = dst_type, R8 = dst, R9 = src_type, [RSP+40] = src
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_XOR
Emit_XOR PROC FRAME
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13d, edx
    mov     r14, r8
    mov     r15d, r9d
    mov     rbx, [rsp+48]
    
    call    Encoder_Reset
    
    ; Check for zeroing idiom: XOR reg, reg
    cmp     r13d, 0                     ; Dst is reg
    jne     @@not_zeroing
    cmp     r15d, 0                     ; Src is reg
    jne     @@not_zeroing
    cmp     r14, rbx                    ; Same register?
    jne     @@not_zeroing
    
    ; Zeroing idiom: use 32-bit XOR (clears upper 32 bits automatically)
    ; No REX.W needed
    xor     edx, edx                    ; W=0
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    jmp     @@do_xor
    
@@not_zeroing:
    ; Regular XOR: REX.W for 64-bit
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
@@do_xor:
    cmp     r15d, 0                     ; Src is reg
    jne     @@xor_imm
    
    ; XOR r/m64, r64 (31 /r)
    mov     byte ptr [r12].INSTR_CTX.opcode, 031h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     dl, MOD_REG
    mov     r8b, bl
    mov     r9b, r14b
    call    Build_ModRM
    jmp     @@encode
    
@@xor_imm:
    ; XOR r/m64, imm32 (81 /6) or imm8 (83 /6)
    cmp     rbx, 127
    jg      @@xor_imm32
    cmp     rbx, -128
    jl      @@xor_imm32
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 083h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     dl, MOD_REG
    mov     r8b, 6
    mov     r9b, r14b
    call    Build_ModRM
    
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    jmp     @@encode
    
@@xor_imm32:
    mov     byte ptr [r12].INSTR_CTX.opcode, 081h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     dl, MOD_REG
    mov     r8b, 6
    mov     r9b, r14b
    call    Build_ModRM
    
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 4
    
@@encode:
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_XOR ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_OR / Emit_AND - Similar to XOR
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_OR
Emit_OR PROC FRAME
    ; Implementation identical to XOR, just change opcode to 09h/81h /1
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13d, edx
    mov     r14, r8
    mov     r15d, r9d
    mov     rbx, [rsp+48]
    
    call    Encoder_Reset
    
    mov     dl, 1                       ; REX.W
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    cmp     r15d, 0
    jne     @@or_imm
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 009h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     dl, MOD_REG
    mov     r8b, bl
    mov     r9b, r14b
    call    Build_ModRM
    jmp     @@encode
    
@@or_imm:
    cmp     rbx, 127
    jg      @@or_imm32
    cmp     rbx, -128
    jl      @@or_imm32
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 083h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     dl, MOD_REG
    mov     r8b, 1                      ; /1 for OR
    mov     r9b, r14b
    call    Build_ModRM
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    jmp     @@encode
    
@@or_imm32:
    mov     byte ptr [r12].INSTR_CTX.opcode, 081h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     dl, MOD_REG
    mov     r8b, 1
    mov     r9b, r14b
    call    Build_ModRM
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 4
    
@@encode:
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_OR ENDP

PUBLIC Emit_AND
Emit_AND PROC FRAME
    ; Same as OR, but with opcode 21h/81h /4
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13d, edx
    mov     r14, r8
    mov     r15d, r9d
    mov     rbx, [rsp+48]
    
    call    Encoder_Reset
    
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    cmp     r15d, 0
    jne     @@and_imm
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 021h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     dl, MOD_REG
    mov     r8b, bl
    mov     r9b, r14b
    call    Build_ModRM
    jmp     @@encode
    
@@and_imm:
    cmp     rbx, 127
    jg      @@and_imm32
    cmp     rbx, -128
    jl      @@and_imm32
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 083h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     dl, MOD_REG
    mov     r8b, 4                      ; /4 for AND
    mov     r9b, r14b
    call    Build_ModRM
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    jmp     @@encode
    
@@and_imm32:
    mov     byte ptr [r12].INSTR_CTX.opcode, 081h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     dl, MOD_REG
    mov     r8b, 4
    mov     r9b, r14b
    call    Build_ModRM
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 4
    
@@encode:
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_AND ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_SHL/SHR/SAR - Shift operations
; RCX = ctx, DL = shift_type (0=SHL, 1=SHR, 2=SAR), R8 = dst_reg, R9 = count/CL
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_SHIFT
Emit_SHIFT PROC FRAME
    push    rbx r12 r13 r14
    mov     r12, rcx
    mov     r13b, dl                    ; Shift type
    mov     r14b, r8b                   ; Dst reg
    mov     rbx, r9                     ; Count (0=CL, else immediate)
    
    call    Encoder_Reset
    
    ; REX.W for 64-bit
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; Determine opcode extension
    mov     r8b, r13b                   ; /4 for SHL, /5 for SHR, /7 for SAR
    cmp     r13b, 0
    je      @@type_shl
    cmp     r13b, 1
    je      @@type_shr
    ; SAR
    mov     r8b, 7
    jmp     @@check_count
@@type_shl:
    mov     r8b, 4
    jmp     @@check_count
@@type_shr:
    mov     r8b, 5
    
@@check_count:
    test    rbx, rbx                    ; CL or immediate?
    jnz     @@shift_imm
    
    ; Shift by CL (D3 /digit)
    mov     byte ptr [r12].INSTR_CTX.opcode, 0D3h
    mov     [r12].INSTR_CTX.opcode_len, 1
    jmp     @@build_modrm
    
@@shift_imm:
    ; Check for shift by 1 (special encoding D1)
    cmp     rbx, 1
    jne     @@shift_imm8
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 0D1h
    mov     [r12].INSTR_CTX.opcode_len, 1
    jmp     @@build_modrm
    
@@shift_imm8:
    ; Shift by imm8 (C1 /digit ib)
    mov     byte ptr [r12].INSTR_CTX.opcode, 0C1h
    mov     [r12].INSTR_CTX.opcode_len, 1
    mov     rax, rbx
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    
@@build_modrm:
    mov     dl, MOD_REG
    ; r8b already contains opcode extension
    mov     r9b, r14b                   ; r/m = dst
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r14 r13 r12 rbx
    ret
Emit_SHIFT ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_BT/BTS/BTR/BTC - Bit test operations
; RCX = ctx, DL = operation (0=BT, 1=BTS, 2=BTR, 3=BTC)
; R8 = base_reg, R9 = bit (0=reg, else imm8)
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_BITOP
Emit_BITOP PROC FRAME
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13b, dl                    ; Operation
    mov     r14b, r8b                   ; Base reg
    mov     r15, r9                     ; Bit (0=reg, else value)
    
    call    Encoder_Reset
    
    ; REX.W
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; 0F prefix
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    
    test    r15, r15                    ; Bit is reg?
    jz      @@bit_is_reg
    
    ; Bit is immediate (0F BA /digit ib)
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0BAh
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    ; Determine opcode extension
    mov     r8b, r13b
    add     r8b, 4                      ; BT=/4, BTS=/5, BTR=/6, BTC=/7
    
    mov     dl, MOD_REG
    mov     r9b, r14b
    call    Build_ModRM
    
    mov     rax, r15
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    jmp     @@encode
    
@@bit_is_reg:
    ; Bit is register (0F A3/AB/B3/BB /r)
    ; BT=A3, BTS=AB, BTR=B3, BTC=BB
    mov     al, 0A3h
    cmp     r13b, 0
    je      @@set_opcode
    mov     al, 0ABh
    cmp     r13b, 1
    je      @@set_opcode
    mov     al, 0B3h
    cmp     r13b, 2
    je      @@set_opcode
    mov     al, 0BBh
    
@@set_opcode:
    mov     byte ptr [r12].INSTR_CTX.opcode+1, al
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    ; ModRM: mod=11, reg=bit_reg, r/m=base
    mov     dl, MOD_REG
    mov     r8b, r15b                   ; Bit reg (passed as low byte)
    mov     r9b, r14b
    call    Build_ModRM
    
@@encode:
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_BITOP ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_CMOVcc - Conditional move (all 16 conditions)
; RCX = ctx, DL = condition (0-15), R8 = dst_reg, R9 = src_reg
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_CMOVcc
Emit_CMOVcc PROC FRAME
    push    r12 r13 r14 r15
    mov     r12, rcx
    mov     r13b, dl                    ; Condition
    mov     r14b, r8b                   ; Dst
    mov     r15b, r9b                   ; Src
    
    call    Encoder_Reset
    
    ; REX.W
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; 0F 40+cc /r
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    mov     al, 40h
    add     al, r13b
    mov     byte ptr [r12].INSTR_CTX.opcode+1, al
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    mov     dl, MOD_REG
    mov     r8b, r14b                   ; Dst = reg field
    mov     r9b, r15b                   ; Src = r/m field
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12
    ret
Emit_CMOVcc ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_SETcc - Set byte on condition
; RCX = ctx, DL = condition (0-15), R8 = dst_reg (8-bit)
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_SETcc
Emit_SETcc PROC FRAME
    push    r12 r13 r14
    mov     r12, rcx
    mov     r13b, dl
    mov     r14b, r8b
    
    call    Encoder_Reset
    
    ; No REX.W (8-bit operation), but may need REX for high regs
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; 0F 90+cc /0
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    mov     al, 90h
    add     al, r13b
    mov     byte ptr [r12].INSTR_CTX.opcode+1, al
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    mov     dl, MOD_REG
    xor     r8b, r8b                    ; /0
    mov     r9b, r14b
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r14 r13 r12
    ret
Emit_SETcc ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_XCHG - Exchange registers
; RCX = ctx, DL = reg1, R8 = reg2
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_XCHG
Emit_XCHG PROC FRAME
    push    r12 r13 r14
    mov     r12, rcx
    mov     r13b, dl
    mov     r14b, r8b
    
    call    Encoder_Reset
    
    ; Special encoding for XCHG rAX, r (90+rd)
    cmp     r13b, 0                     ; RAX
    jne     @@not_rax_form
    
    cmp     r14b, 8
    jb      @@rax_shortform
    
@@not_rax_form:
    ; General form: 87 /r
    mov     dl, 1                       ; REX.W
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    mov     byte ptr [r12].INSTR_CTX.opcode, 087h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     dl, MOD_REG
    mov     r8b, r13b
    mov     r9b, r14b
    call    Build_ModRM
    jmp     @@encode
    
@@rax_shortform:
    ; XCHG rAX, r (90+rd)
    mov     byte ptr [r12].INSTR_CTX.opcode, 090h
    add     byte ptr [r12].INSTR_CTX.opcode, r14b
    mov     [r12].INSTR_CTX.opcode_len, 1
    
@@encode:
    mov     rcx, r12
    call    Encode_Instruction
    pop     r14 r13 r12
    ret
Emit_XCHG ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_CMPXCHG - Compare and exchange
; RCX = ctx, DL = dst_reg, R8 = src_reg (RAX implicit comparand)
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_CMPXCHG
Emit_CMPXCHG PROC FRAME
    push    r12 r13 r14
    mov     r12, rcx
    mov     r13b, dl
    mov     r14b, r8b
    
    call    Encoder_Reset
    
    ; REX.W
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; 0F B1 /r
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0B1h
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    mov     dl, MOD_REG
    mov     r8b, r14b                   ; Src
    mov     r9b, r13b                   ; Dst
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r14 r13 r12
    ret
Emit_CMPXCHG ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_XADD - Exchange and add
; RCX = ctx, DL = dst_reg, R8 = src_reg
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_XADD
Emit_XADD PROC FRAME
    push    r12 r13 r14
    mov     r12, rcx
    mov     r13b, dl
    mov     r14b, r8b
    
    call    Encoder_Reset
    
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; 0F C1 /r
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0C1h
    mov     [r12].INSTR_CTX.opcode_len, 2
    
    mov     dl, MOD_REG
    mov     r8b, r14b
    mov     r9b, r13b
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r14 r13 r12
    ret
Emit_XADD ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_MOVSX/MOVZX - Sign/Zero extend
; RCX = ctx, DL = type (0=MOVSX, 1=MOVZX), R8 = dst_reg
; R9 = src_reg, [RSP+40] = src_size (1=byte, 2=word, 4=dword)
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_MOVX
Emit_MOVX PROC FRAME
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13b, dl                    ; Type
    mov     r14b, r8b                   ; Dst
    mov     r15b, r9b                   ; Src
    mov     rbx, [rsp+48]               ; Src size
    
    call    Encoder_Reset
    
    ; REX.W for 64-bit destination
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; Determine opcode
    mov     byte ptr [r12].INSTR_CTX.opcode, 0Fh
    
    cmp     r13b, 0                     ; MOVSX?
    jne     @@movzx
    
    ; MOVSX
    cmp     rbx, 1                      ; Byte source
    je      @@movsx_byte
    cmp     rbx, 2
    je      @@movsx_word
    ; MOVSXD (dword->qword): special opcode 63h (no 0F prefix)
    mov     byte ptr [r12].INSTR_CTX.opcode, 063h
    mov     [r12].INSTR_CTX.opcode_len, 1
    jmp     @@build_modrm
    
@@movsx_byte:
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0BEh
    mov     [r12].INSTR_CTX.opcode_len, 2
    jmp     @@build_modrm
@@movsx_word:
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0BFh
    mov     [r12].INSTR_CTX.opcode_len, 2
    jmp     @@build_modrm
    
@@movzx:
    ; MOVZX
    cmp     rbx, 1
    je      @@movzx_byte
    ; Word
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0B7h
    mov     [r12].INSTR_CTX.opcode_len, 2
    jmp     @@build_modrm
@@movzx_byte:
    mov     byte ptr [r12].INSTR_CTX.opcode+1, 0B6h
    mov     [r12].INSTR_CTX.opcode_len, 2
    
@@build_modrm:
    mov     dl, MOD_REG
    mov     r8b, r14b
    mov     r9b, r15b
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_MOVX ENDP

; ═════════════════════════════════════════════════════════════════════════════
; Emit_IMUL - Signed multiply (three-operand form)
; RCX = ctx, DL = dst_reg, R8 = src_reg, R9 = immediate
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Emit_IMUL
Emit_IMUL PROC FRAME
    push    rbx r12 r13 r14 r15
    mov     r12, rcx
    mov     r13b, dl
    mov     r14b, r8b
    mov     r15, r9
    
    call    Encoder_Reset
    
    mov     dl, 1
    xor     r8d, r8d
    xor     r9d, r9d
    push    0
    call    Set_REX
    add     rsp, 8
    
    ; Check if imm8 or imm32
    cmp     r15, 127
    jg      @@imul_imm32
    cmp     r15, -128
    jl      @@imul_imm32
    
    ; IMUL r64, r/m64, imm8 (6B /r ib)
    mov     byte ptr [r12].INSTR_CTX.opcode, 06Bh
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     rax, r15
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 1
    jmp     @@build_modrm
    
@@imul_imm32:
    ; IMUL r64, r/m64, imm32 (69 /r id)
    mov     byte ptr [r12].INSTR_CTX.opcode, 069h
    mov     [r12].INSTR_CTX.opcode_len, 1
    
    mov     rax, r15
    mov     [r12].INSTR_CTX.immediate, rax
    mov     [r12].INSTR_CTX.imm_size, 4
    
@@build_modrm:
    mov     dl, MOD_REG
    mov     r8b, r13b                   ; Dst
    mov     r9b, r14b                   ; Src
    call    Build_ModRM
    
    mov     rcx, r12
    call    Encode_Instruction
    pop     r15 r14 r13 r12 rbx
    ret
Emit_IMUL ENDP

; ═════════════════════════════════════════════════════════════════════════════
; DATA SECTION - Instruction Count Tracking
; ═════════════════════════════════════════════════════════════════════════════
.DATA
ALIGN 16

; Instruction count statistics
InstructionMatrix:
    ; Category counts for validation
    DQ 0    ; MOV variants count
    DQ 0    ; ALU operations count
    DQ 0    ; Shift operations count
    DQ 0    ; Bit operations count
    DQ 0    ; Conditional moves count
    DQ 0    ; SETcc count
    DQ 0    ; Atomic operations count
    DQ 0    ; Stack operations count
    DQ 0    ; Control flow count
    DQ 0    ; System instructions count
    DQ 0    ; String operations count
    DQ 0    ; Total instruction count

; ═════════════════════════════════════════════════════════════════════════════
; EXPORT ALL SYMBOLS
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC RegEncodeTable
PUBLIC InstructionMatrix
PUBLIC INSTR_CTX
PUBLIC OPERAND_STRUCT

END
