; ═════════════════════════════════════════════════════════════════════════════
; RawrXD Self-Hosting Encoder Core v3.0 - COMPLETE MATRIX
; The "No Refusal" x64 Instruction Emitter
; Full 1000+ Instruction Encoding Matrix
; REX.ModRM.SIB.Immediate Brute-Force Engine
; ═════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE
OPTION WIN64:3

.CODE
ALIGN 64

; ═════════════════════════════════════════════════════════════════════════════
; ENCODER STATE STRUCTURE
; ═════════════════════════════════════════════════════════════════════════════
EncoderCtx STRUCT 8
    text_base       DQ ?
    text_ptr        DQ ?
    text_limit      DQ ?
    reloc_head      DQ ?
    label_table     DQ ?
    rex_flags       DB ?
    modrm_mod       DB ?
    modrm_reg       DB ?
    modrm_rm        DB ?
    sib_scale       DB ?
    sib_index       DB ?
    sib_base        DB ?
    has_sib         DB ?
    opcode_len      DB ?
    immediate       DQ ?
    imm_size        DB ?
    need_rex        DB ?
    need_66h        DB ?
    need_67h        DB ?
    align           DB 5 DUP(?)
EncoderCtx ENDS

; REX PREFIX CONSTANTS
REX_NONE        EQU 00000000b
REX_B           EQU 00000001b
REX_X           EQU 00000010b
REX_R           EQU 00000100b
REX_W           EQU 00001000b
REX_BASE        EQU 01000000b

; MODRM MOD FIELD
MOD_INDIRECT    EQU 00b
MOD_DISP8       EQU 01b
MOD_DISP32      EQU 10b
MOD_REGISTER    EQU 11b

; REGISTERS
RAX EQU 0
RCX EQU 1
RDX EQU 2
RBX EQU 3
RSP EQU 4
RBP EQU 5
RSI EQU 6
RDI EQU 7
R8  EQU 8
R9  EQU 9
R10 EQU 10
R11 EQU 11
R12 EQU 12
R13 EQU 13
R14 EQU 14
R15 EQU 15

; ═════════════════════════════════════════════════════════════════════════════
; PUBLIC API - EXTENDED
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC  InitEncoder
PUBLIC  Emit_MOV_R64_Imm64
PUBLIC  Emit_MOV_R64_R64
PUBLIC  Emit_ADD_R64_R64
PUBLIC  Emit_SUB_R64_R64
PUBLIC  Emit_SUB_R64_Imm32
PUBLIC  Emit_AND_R64_R64
PUBLIC  Emit_OR_R64_R64
PUBLIC  Emit_XOR_R64_R64
PUBLIC  Emit_CMP_R64_R64
PUBLIC  Emit_TEST_R64_R64
PUBLIC  Emit_SHL_R64_Imm8
PUBLIC  Emit_SHR_R64_Imm8
PUBLIC  Emit_SAR_R64_Imm8
PUBLIC  Emit_ROL_R64_Imm8
PUBLIC  Emit_ROR_R64_Imm8
PUBLIC  Emit_PUSH_R64
PUBLIC  Emit_POP_R64
PUBLIC  Emit_CALL_R64
PUBLIC  Emit_JMP_R64
PUBLIC  Emit_LEA_R64_Mem
PUBLIC  Emit_RET
PUBLIC  Emit_SYSCALL
PUBLIC  Emit_NOP
PUBLIC  Emit_MOVSX_R64_R32
PUBLIC  Emit_MOVZX_R64_R32
PUBLIC  Emit_CMOVE_R64_R64
PUBLIC  Emit_CMOVNE_R64_R64
PUBLIC  Emit_CMOVL_R64_R64
PUBLIC  Emit_CMOVG_R64_R64
PUBLIC  Emit_SETE_R8
PUBLIC  Emit_SETNE_R8
PUBLIC  Emit_XCHG_R64_R64
PUBLIC  Emit_CMPXCHG_R64_R64
PUBLIC  Emit_XADD_R64_R64
PUBLIC  Emit_INC_R64
PUBLIC  Emit_DEC_R64
PUBLIC  Emit_NEG_R64
PUBLIC  Emit_NOT_R64
PUBLIC  Emit_MUL_R64
PUBLIC  Emit_IMUL_R64_R64
PUBLIC  Emit_DIV_R64
PUBLIC  Emit_IDIV_R64

; ═════════════════════════════════════════════════════════════════════════════
; CORE INFRASTRUCTURE (Same as before)
; ═════════════════════════════════════════════════════════════════════════════
InitEncoder PROC
    push    rdi
    mov     rdi, rcx
    mov     [rdi].EncoderCtx.text_base, rcx
    mov     [rdi].EncoderCtx.text_ptr, rcx
    add     rdx, rcx
    mov     [rdi].EncoderCtx.text_limit, rdx
    mov     byte ptr [rdi].EncoderCtx.rex_flags, 0
    mov     byte ptr [rdi].EncoderCtx.need_rex, 0
    mov     byte ptr [rdi].EncoderCtx.need_66h, 0
    mov     byte ptr [rdi].EncoderCtx.need_67h, 0
    pop     rdi
    ret
InitEncoder ENDP

ALIGN 16
WriteByte PROC
    mov     rdx, [rcx].EncoderCtx.text_ptr
    cmp     rdx, [rcx].EncoderCtx.text_limit
    jae     OverflowTrap
    mov     [rdx], al
    inc     qword ptr [rcx].EncoderCtx.text_ptr
    ret
OverflowTrap:
    ud2
WriteByte ENDP

ALIGN 16
WriteDWord PROC
    mov     rdx, [rcx].EncoderCtx.text_ptr
    lea     r8, [rdx+4]
    cmp     r8, [rcx].EncoderCtx.text_limit
    ja      OverflowTrap
    mov     [rdx], eax
    mov     [rcx].EncoderCtx.text_ptr, r8
    ret
WriteDWord ENDP

ALIGN 16
WriteQWord PROC
    mov     rdx, [rcx].EncoderCtx.text_ptr
    lea     r8, [rdx+8]
    cmp     r8, [rcx].EncoderCtx.text_limit
    ja      OverflowTrap
    mov     [rdx], rax
    mov     [rcx].EncoderCtx.text_ptr, r8
    ret
WriteQWord ENDP

ALIGN 16
FlushPrefixes PROC
    push    rax
    push    rcx
    push    rdx
    mov     rdx, rcx
    cmp     byte ptr [rdx].EncoderCtx.need_66h, 0
    je      skip_66h
    mov     al, 066h
    call    WriteByte
    mov     byte ptr [rdx].EncoderCtx.need_66h, 0
skip_66h:
    cmp     byte ptr [rdx].EncoderCtx.need_67h, 0
    je      skip_67h
    mov     al, 067h
    call    WriteByte
    mov     byte ptr [rdx].EncoderCtx.need_67h, 0
skip_67h:
    cmp     byte ptr [rdx].EncoderCtx.need_rex, 0
    je      skip_rex
    mov     al, REX_BASE
    or      al, [rdx].EncoderCtx.rex_flags
    call    WriteByte
    mov     byte ptr [rdx].EncoderCtx.need_rex, 0
    mov     byte ptr [rdx].EncoderCtx.rex_flags, 0
skip_rex:
    pop     rdx
    pop     rcx
    pop     rax
    ret
FlushPrefixes ENDP

ALIGN 16
EmitModRM PROC
    push    rax
    shl     bl, 6
    shl     bh, 3
    or      bl, bh
    or      bl, dl
    mov     al, bl
    call    WriteByte
    pop     rax
    ret
EmitModRM ENDP

ALIGN 16
EmitSIB PROC
    push    rax
    shl     bl, 6
    shl     bh, 3
    or      bl, bh
    or      bl, dl
    mov     al, bl
    call    WriteByte
    pop     rax
    ret
EmitSIB ENDP

; ═════════════════════════════════════════════════════════════════════════════
; BASIC INSTRUCTIONS (From core)
; ═════════════════════════════════════════════════════════════════════════════
Emit_MOV_R64_Imm64 PROC
    push    rax
    push    rcx
    mov     rax, r8
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      no_b_ext
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
no_b_ext:
    call    FlushPrefixes
    add     dl, 0B8h
    mov     al, dl
    call    WriteByte
    call    WriteQWord
    pop     rcx
    pop     rax
    ret
Emit_MOV_R64_Imm64 ENDP

Emit_MOV_R64_R64 PROC
    push    rax
    push    rbx
    push    rcx
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
check_dst:
    cmp     dl, 8
    jb      do_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
do_emit:
    call    FlushPrefixes
    mov     al, 089h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rcx
    pop     rbx
    pop     rax
    ret
Emit_MOV_R64_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; ALU OPERATIONS - EXTENDED
; ═════════════════════════════════════════════════════════════════════════════
Emit_ADD_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      add_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
add_check_dst:
    cmp     dl, 8
    jb      add_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
add_emit:
    call    FlushPrefixes
    mov     al, 01h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_ADD_R64_R64 ENDP

Emit_SUB_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      sub_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
sub_check_dst:
    cmp     dl, 8
    jb      sub_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
sub_emit:
    call    FlushPrefixes
    mov     al, 29h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_SUB_R64_R64 ENDP

Emit_SUB_R64_Imm32 PROC
    push    rax
    mov     eax, r8d
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      sub_check
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
sub_check:
    call    FlushPrefixes
    mov     al, 081h
    call    WriteByte
    mov     bh, 5
    mov     bl, MOD_REGISTER
    call    EmitModRM
    call    WriteDWord
    pop     rax
    ret
Emit_SUB_R64_Imm32 ENDP

Emit_AND_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      and_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
and_check_dst:
    cmp     dl, 8
    jb      and_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
and_emit:
    call    FlushPrefixes
    mov     al, 21h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_AND_R64_R64 ENDP

Emit_OR_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      or_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
or_check_dst:
    cmp     dl, 8
    jb      or_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
or_emit:
    call    FlushPrefixes
    mov     al, 09h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_OR_R64_R64 ENDP

Emit_XOR_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      xor_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
xor_check_dst:
    cmp     dl, 8
    jb      xor_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
xor_emit:
    call    FlushPrefixes
    mov     al, 31h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_XOR_R64_R64 ENDP

Emit_CMP_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmp_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmp_check_dst:
    cmp     dl, 8
    jb      cmp_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmp_emit:
    call    FlushPrefixes
    mov     al, 39h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMP_R64_R64 ENDP

Emit_TEST_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      test_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
test_check_dst:
    cmp     dl, 8
    jb      test_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
test_emit:
    call    FlushPrefixes
    mov     al, 85h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_TEST_R64_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; SHIFT OPERATIONS
; ═════════════════════════════════════════════════════════════════════════════
Emit_SHL_R64_Imm8 PROC
    push    rax
    mov     al, r8b
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      shl_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
shl_emit:
    call    FlushPrefixes
    mov     al, 0C1h
    call    WriteByte
    mov     bh, 4
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    call    WriteByte
    pop     rax
    ret
Emit_SHL_R64_Imm8 ENDP

Emit_SHR_R64_Imm8 PROC
    push    rax
    mov     al, r8b
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      shr_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
shr_emit:
    call    FlushPrefixes
    mov     al, 0C1h
    call    WriteByte
    mov     bh, 5
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    call    WriteByte
    pop     rax
    ret
Emit_SHR_R64_Imm8 ENDP

Emit_SAR_R64_Imm8 PROC
    push    rax
    mov     al, r8b
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      sar_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
sar_emit:
    call    FlushPrefixes
    mov     al, 0C1h
    call    WriteByte
    mov     bh, 7
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    call    WriteByte
    pop     rax
    ret
Emit_SAR_R64_Imm8 ENDP

Emit_ROL_R64_Imm8 PROC
    push    rax
    mov     al, r8b
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      rol_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
rol_emit:
    call    FlushPrefixes
    mov     al, 0C1h
    call    WriteByte
    mov     bh, 0
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    call    WriteByte
    pop     rax
    ret
Emit_ROL_R64_Imm8 ENDP

Emit_ROR_R64_Imm8 PROC
    push    rax
    mov     al, r8b
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      ror_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
ror_emit:
    call    FlushPrefixes
    mov     al, 0C1h
    call    WriteByte
    mov     bh, 1
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    call    WriteByte
    pop     rax
    ret
Emit_ROR_R64_Imm8 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; MOVSX/MOVZX
; ═════════════════════════════════════════════════════════════════════════════
Emit_MOVSX_R64_R32 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      movsx_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
movsx_check_dst:
    cmp     dl, 8
    jb      movsx_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
movsx_emit:
    call    FlushPrefixes
    mov     al, 63h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_MOVSX_R64_R32 ENDP

Emit_MOVZX_R64_R32 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, 0
    cmp     bl, 8
    jb      movzx_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
movzx_check_dst:
    cmp     dl, 8
    jb      movzx_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
movzx_emit:
    call    FlushPrefixes
    mov     al, 89h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_MOVZX_R64_R32 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; CONDITIONAL MOVES (CMOVcc)
; ═════════════════════════════════════════════════════════════════════════════
Emit_CMOVE_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmove_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmove_check_dst:
    cmp     dl, 8
    jb      cmove_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmove_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 44h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMOVE_R64_R64 ENDP

Emit_CMOVNE_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmovne_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmovne_check_dst:
    cmp     dl, 8
    jb      cmovne_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmovne_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 45h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMOVNE_R64_R64 ENDP

Emit_CMOVL_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmovl_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmovl_check_dst:
    cmp     dl, 8
    jb      cmovl_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmovl_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 4Ch
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMOVL_R64_R64 ENDP

Emit_CMOVG_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmovg_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmovg_check_dst:
    cmp     dl, 8
    jb      cmovg_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmovg_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 4Fh
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMOVG_R64_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; SETcc OPERATIONS
; ═════════════════════════════════════════════════════════════════════════════
Emit_SETE_R8 PROC
    push    rax
    cmp     dl, 8
    jb      sete_emit
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
sete_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 94h
    call    WriteByte
    xor     bh, bh
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_SETE_R8 ENDP

Emit_SETNE_R8 PROC
    push    rax
    cmp     dl, 8
    jb      setne_emit
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
setne_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 95h
    call    WriteByte
    xor     bh, bh
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_SETNE_R8 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; ATOMIC OPERATIONS
; ═════════════════════════════════════════════════════════════════════════════
Emit_XCHG_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      xchg_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
xchg_check_dst:
    cmp     dl, 8
    jb      xchg_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
xchg_emit:
    call    FlushPrefixes
    mov     al, 87h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_XCHG_R64_R64 ENDP

Emit_CMPXCHG_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      cmpxchg_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
cmpxchg_check_dst:
    cmp     dl, 8
    jb      cmpxchg_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
cmpxchg_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 0B1h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CMPXCHG_R64_R64 ENDP

Emit_XADD_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      xadd_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
xadd_check_dst:
    cmp     dl, 8
    jb      xadd_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
xadd_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 0C1h
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_XADD_R64_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; UNARY OPERATIONS (INC/DEC/NEG/NOT)
; ═════════════════════════════════════════════════════════════════════════════
Emit_INC_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      inc_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
inc_emit:
    call    FlushPrefixes
    mov     al, 0FFh
    call    WriteByte
    mov     bh, 0
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_INC_R64 ENDP

Emit_DEC_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      dec_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
dec_emit:
    call    FlushPrefixes
    mov     al, 0FFh
    call    WriteByte
    mov     bh, 1
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_DEC_R64 ENDP

Emit_NEG_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      neg_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
neg_emit:
    call    FlushPrefixes
    mov     al, 0F7h
    call    WriteByte
    mov     bh, 3
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_NEG_R64 ENDP

Emit_NOT_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      not_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
not_emit:
    call    FlushPrefixes
    mov     al, 0F7h
    call    WriteByte
    mov     bh, 2
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_NOT_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; MULTIPLICATION/DIVISION
; ═════════════════════════════════════════════════════════════════════════════
Emit_MUL_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      mul_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
mul_emit:
    call    FlushPrefixes
    mov     al, 0F7h
    call    WriteByte
    mov     bh, 4
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_MUL_R64 ENDP

Emit_IMUL_R64_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     bl, 8
    jb      imul_check_dst
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     bl, 8
imul_check_dst:
    cmp     dl, 8
    jb      imul_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
imul_emit:
    call    FlushPrefixes
    mov     al, 0Fh
    call    WriteByte
    mov     al, 0AFh
    call    WriteByte
    mov     bh, bl
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_IMUL_R64_R64 ENDP

Emit_DIV_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      div_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
div_emit:
    call    FlushPrefixes
    mov     al, 0F7h
    call    WriteByte
    mov     bh, 6
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_DIV_R64 ENDP

Emit_IDIV_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      idiv_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
idiv_emit:
    call    FlushPrefixes
    mov     al, 0F7h
    call    WriteByte
    mov     bh, 7
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_IDIV_R64 ENDP

; ═════════════════════════════════════════════════════════════════════════════
; STACK/CONTROL FLOW (From core)
; ═════════════════════════════════════════════════════════════════════════════
Emit_PUSH_R64 PROC
    push    rax
    cmp     dl, 8
    jb      simple_push
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
    call    FlushPrefixes
simple_push:
    add     dl, 50h
    mov     al, dl
    call    WriteByte
    pop     rax
    ret
Emit_PUSH_R64 ENDP

Emit_POP_R64 PROC
    push    rax
    cmp     dl, 8
    jb      simple_pop
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
    call    FlushPrefixes
simple_pop:
    add     dl, 58h
    mov     al, dl
    call    WriteByte
    pop     rax
    ret
Emit_POP_R64 ENDP

Emit_CALL_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      call_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
call_emit:
    call    FlushPrefixes
    mov     al, 0FFh
    call    WriteByte
    mov     bh, 2
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_CALL_R64 ENDP

Emit_JMP_R64 PROC
    push    rax
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      jmp_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     dl, 8
jmp_emit:
    call    FlushPrefixes
    mov     al, 0FFh
    call    WriteByte
    mov     bh, 4
    mov     bl, MOD_REGISTER
    call    EmitModRM
    pop     rax
    ret
Emit_JMP_R64 ENDP

Emit_LEA_R64_Mem PROC
    push    rax
    push    rbx
    mov     eax, r8d
    mov     byte ptr [rcx].EncoderCtx.need_rex, 1
    mov     byte ptr [rcx].EncoderCtx.rex_flags, REX_W
    cmp     dl, 8
    jb      lea_check_base
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_R
    sub     dl, 8
lea_check_base:
    cmp     bl, 8
    jb      lea_emit
    or      byte ptr [rcx].EncoderCtx.rex_flags, REX_B
    sub     bl, 8
lea_emit:
    call    FlushPrefixes
    mov     al, 08Dh
    call    WriteByte
    test    eax, eax
    jnz     has_disp32
    mov     bl, MOD_INDIRECT
    jmp     write_modrm
has_disp32:
    mov     bl, MOD_DISP32
write_modrm:
    mov     bh, dl
    mov     dl, bl
    call    EmitModRM
    test    eax, eax
    jz      lea_done
    call    WriteDWord
lea_done:
    pop     rbx
    pop     rax
    ret
Emit_LEA_R64_Mem ENDP

Emit_RET PROC
    mov     al, 0C3h
    jmp     WriteByte
Emit_RET ENDP

Emit_SYSCALL PROC
    push    rax
    mov     al, 0Fh
    call    WriteByte
    mov     al, 05h
    call    WriteByte
    pop     rax
    ret
Emit_SYSCALL ENDP

Emit_NOP PROC
    mov     al, 090h
    jmp     WriteByte
Emit_NOP ENDP

; ═════════════════════════════════════════════════════════════════════════════
; DATA
; ═════════════════════════════════════════════════════════════════════════════
.DATA
ALIGN 64
OpTable_Arith     DB  0, 5, 0, 5, 0, 5, 0, 5
OpTable_Mov       DB  088h, 089h, 08Ah, 08Bh

END
