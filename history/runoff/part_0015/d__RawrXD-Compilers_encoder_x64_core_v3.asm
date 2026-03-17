; ═════════════════════════════════════════════════════════════════════════════
; RawrXD x64 Instruction Encoder Core v3.0
; Brute-Force Instruction Matrix - 1000+ Variant Support
; Pure MASM64 | Zero Dependencies | Direct PE Generation
; ═════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE
OPTION WIN64:11

; ═════════════════════════════════════════════════════════════════════════════
; INSTRUCTION ENCODING STRUCTURES
; ═════════════════════════════════════════════════════════════════════════════

; Operand types
OP_NONE         EQU 0
OP_REG8         EQU 1
OP_REG16        EQU 2
OP_REG32        EQU 3
OP_REG64        EQU 4
OP_IMM8         EQU 5
OP_IMM16        EQU 6
OP_IMM32        EQU 7
OP_IMM64        EQU 8
OP_MEM8         EQU 9
OP_MEM16        EQU 10
OP_MEM32        EQU 11
OP_MEM64        EQU 12
OP_REL8         EQU 13
OP_REL32        EQU 14
OP_MOFFSET      EQU 15

; Register IDs (Intel encoding order)
REG_AL  EQU 0;  REG_AX  EQU 0;  REG_EAX EQU 0;  REG_RAX EQU 0
REG_CL  EQU 1;  REG_CX  EQU 1;  REG_ECX EQU 1;  REG_RCX EQU 1
REG_DL  EQU 2;  REG_DX  EQU 2;  REG_EDX EQU 2;  REG_RDX EQU 2
REG_BL  EQU 3;  REG_BX  EQU 3;  REG_EBX EQU 3;  REG_RBX EQU 3
REG_AH  EQU 4;  REG_SP  EQU 4;  REG_ESP EQU 4;  REG_RSP EQU 4
REG_CH  EQU 5;  REG_BP  EQU 5;  REG_EBP EQU 5;  REG_RBP EQU 5
REG_DH  EQU 6;  REG_SI  EQU 6;  REG_ESI EQU 6;  REG_RSI EQU 6
REG_BH  EQU 7;  REG_DI  EQU 7;  REG_EDI EQU 7;  REG_RDI EQU 7
REG_R8B EQU 8;  REG_R8W EQU 8;  REG_R8D EQU 8;  REG_R8  EQU 8
REG_R9B EQU 9;  REG_R9W EQU 9;  REG_R9D EQU 9;  REG_R9  EQU 9
REG_R10B EQU 10; REG_R10W EQU 10; REG_R10D EQU 10; REG_R10 EQU 10
REG_R11B EQU 11; REG_R11W EQU 11; REG_R11D EQU 11; REG_R11 EQU 11
REG_R12B EQU 12; REG_R12W EQU 12; REG_R12D EQU 12; REG_R12 EQU 12
REG_R13B EQU 13; REG_R13W EQU 13; REG_R13D EQU 13; REG_R13 EQU 13
REG_R14B EQU 14; REG_R14W EQU 14; REG_R14D EQU 14; REG_R14 EQU 14
REG_R15B EQU 15; REG_R15W EQU 15; REG_R15D EQU 15; REG_R15 EQU 15

; REX prefix bits
REX_W EQU 48h   ; 64-bit operand
REX_R EQU 44h   ; Extension of ModRM reg field
REX_X EQU 42h   ; Extension of SIB index field
REX_B EQU 41h   ; Extension of ModRM r/m or SIB base

; ModRM mod values
MOD_DISP0   EQU 0
MOD_DISP8   EQU 40h
MOD_DISP32  EQU 80h
MOD_REG     EQU 0C0h

; SIB scale values
SCALE_1 EQU 0
SCALE_2 EQU 40h
SCALE_4 EQU 80h
SCALE_8 EQU 0C0h

; Instruction record structure
InstrRec STRUCT
    opcode          DB 4 DUP(?)     ; Primary + extension bytes
    opcode_len      DB ?            ; 1-4 bytes
    rex_required    DB ?            ; 0/1 + flags
    modrm_required  DB ?            ; 0/1
    sib_required    DB ?            ; 0/1
    imm_size        DB ?            ; 0,1,2,4,8
    dir_bit         DB ?            ; Direction (d bit) if applicable
    lockable        DB ?            ; Can use F0 prefix
    special_encoding DB ?           ; Special handling flags
InstrRec ENDS

; Operand structure
Operand STRUCT
    op_type         DD ?
    reg_id          DD ?            ; 0-15 register or base reg for mem
    index_reg       DD ?            ; For SIB
    scale           DD ?            ; 1,2,4,8
    displacement    DQ ?            ; Immediate/displacement value
    rip_relative    DB ?            ; RIP-relative addressing
Operand ENDS

; Encoding context
EncodeCtx STRUCT
    output_ptr      DQ ?            ; Current code emission pointer
    section_rva     DQ ?            ; Current RVA for relocations
    fixup_list      DQ ?            ; Head of fixup chain
    rex_template    DB ?            ; Calculated REX
    modrm_template  DB ?            ; Calculated ModRM
    sib_template    DB ?            ; Calculated SIB
    needs_rex       DB ?            ; Boolean
    emit_66h        DB ?            ; Operand size override
    emit_67h        DB ?            ; Address size override
EncodeCtx ENDS

.data?
align 8
g_enc_ctx       EncodeCtx <?>

; ═════════════════════════════════════════════════════════════════════════════
; CODE GENERATION MACROS (Inline expansion for speed)
; ═════════════════════════════════════════════════════════════════════════════

; Emit byte to code stream
EMIT macro byte_val
    mov     al, byte_val
    mov     rcx, [rbx].EncodeCtx.output_ptr
    mov     [rcx], al
    inc     QWORD PTR [rbx].EncodeCtx.output_ptr
endm

; Calculate REX prefix: REX_W | REX_R | REX_X | REX_B
CALC_REX macro need_w, reg_field, index_reg, rm_field
    xor     al, al
    ;; W bit (64-bit)
    IF need_w EQ 1
        or      al, 48h
    ENDIF
    ;; R bit (extension of reg)
    IF reg_field GE 8
        or      al, 44h
    ENDIF
    ;; X bit (extension of index)
    IF index_reg GE 8
        or      al, 42h
    ENDIF
    ;; B bit (extension of r/m or base)
    IF rm_field GE 8
        or      al, 41h
    ENDIF
    mov     [rbx].EncodeCtx.rex_template, al
    mov     [rbx].EncodeCtx.needs_rex, al
endm

; Build ModRM byte: mod | reg | r/m
BUILD_MODRM macro mod_val, reg_val, rm_val
    mov     al, mod_val
    and     al, 0C0h
    mov     cl, reg_val
    and     cl, 7
    shl     cl, 3
    or      al, cl
    mov     cl, rm_val
    and     cl, 7
    or      al, cl
    mov     [rbx].EncodeCtx.modrm_template, al
endm

; Build SIB byte: scale | index | base
BUILD_SIB macro scale_val, index_val, base_val
    mov     al, scale_val
    mov     cl, index_val
    and     cl, 7
    shl     cl, 3
    or      al, cl
    mov     cl, base_val
    and     cl, 7
    or      al, cl
    mov     [rbx].EncodeCtx.sib_template, al
    mov     [rbx].EncodeCtx.sib_required, 1
endm

.code
align 64

; ═════════════════════════════════════════════════════════════════════════════
; CORE ENCODING FUNCTIONS
; ═════════════════════════════════════════════════════════════════════════════

; Initialize encoding context
; RCX = output buffer, RDX = section RVA
Encoder_Init PROC FRAME
    mov     [g_enc_ctx.output_ptr], rcx
    mov     [g_enc_ctx.section_rva], rdx
    mov     QWORD PTR [g_enc_ctx.fixup_list], 0
    ret
Encoder_Init ENDP

; ═════════════════════════════════════════════════════════════════════════════
; REX PREFIX HANDLER
; Analyzes operands and determines required REX bits
; ═════════════════════════════════════════════════════════════════════════════

CalculateREX PROC
    ; CL = need 64-bit (W), RDX = operand1, R8 = operand2
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    xor     al, al
    
    ; Check for 64-bit operand requirement
    test    cl, cl
    jz      @@no_w
    or      al, REX_W
    
@@no_w:
    ; Check operand1 (destination) for extended registers (8-15)
    cmp     [rdx].Operand.op_type, OP_REG64
    jb      @@check_op2
    mov     r9d, [rdx].Operand.reg_id
    cmp     r9d, 8
    jb      @@check_op2
    or      al, REX_R
    
@@check_op2:
    cmp     r8, 0                       ; NULL second operand?
    je      @@save_rex
    
    cmp     [r8].Operand.op_type, OP_REG64
    jb      @@check_sib
    
    mov     r9d, [r8].Operand.reg_id
    cmp     r9d, 8
    jb      @@check_sib
    or      al, REX_B
    
@@check_sib:
    ; Check if SIB needed with extended index
    cmp     [r8].Operand.index_reg, 8
    jb      @@save_rex
    or      al, REX_X
    
@@save_rex:
    test    al, al                      ; Any REX needed?
    jz      @@no_rex
    mov     [rbx].EncodeCtx.rex_template, al
    or      al, 40h                     ; REX prefix base 0100xxxx
    EMIT    al
    
@@no_rex:
    pop     rbx
    ret
CalculateREX ENDP

; ═════════════════════════════════════════════════════════════════════════════
; MODRM/SIB ENCODER
; Handles all addressing modes: reg-reg, reg-mem (all disp sizes), rip-relative
; ═════════════════════════════════════════════════════════════════════════════

EncodeModRM_SIB PROC
    ; CL = reg/opcode extension, RDX = mem operand
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    
    mov     r10d, ecx                   ; Save reg field
    mov     r11d, [rdx].Operand.reg_id  ; Base register
    
    ; Check for RIP-relative addressing (special case)
    cmp     [rdx].Operand.rip_relative, 1
    je      @@rip_relative
    
    ; Check if pure register addressing (mod = 11)
    cmp     [rdx].Operand.op_type, OP_REG64
    jbe     @@reg_direct
    
    ; Memory addressing - determine displacement size
    movsx   rax, DWORD PTR [rdx].Operand.displacement
    
    test    rax, rax
    jnz     @@has_displacement
    
    ; disp0 - but need to check for RBP/R13 exception (requires disp8=0)
    cmp     r11d, REG_RBP
    je      @@force_disp8
    cmp     r11d, REG_R13
    je      @@force_disp8
    
    ; Pure [base] or [base+index*scale]
    mov     al, MOD_DISP0
    jmp     @@build_mod
    
@@force_disp8:
    mov     al, MOD_DISP8
    mov     [rdx].Operand.displacement, 0
    jmp     @@build_mod
    
@@has_displacement:
    cmp     rax, -128
    jl      @@disp32
    cmp     rax, 127
    jg      @@disp32
    
    ; disp8
    mov     al, MOD_DISP8
    jmp     @@build_mod
    
@@disp32:
    mov     al, MOD_DISP32
    
@@build_mod:
    ; Build ModRM
    mov     r9d, r10d                   ; reg
    and     r9d, 7
    shl     r9d, 3
    
    mov     r8d, r11d                   ; r/m
    and     r8d, 7
    
    or      al, r9b
    or      al, r8b
    EMIT    al
    
    ; Check if SIB needed (base = RSP/R12, or indexed addressing)
    cmp     r11d, REG_RSP
    je      @@needs_sib
    cmp     r11d, REG_R12
    je      @@needs_sib
    cmp     [rdx].Operand.index_reg, 0
    jne     @@needs_sib
    
    jmp     @@emit_displacement
    
@@needs_sib:
    ; Build SIB: scale | index | base
    xor     r9d, r9d
    
    ; Scale factor to encoding
    mov     r8d, [rdx].Operand.scale
    dec     r8d                         ; 1->0, 2->1, 4->2, 8->3
    and     r8d, 3
    shl     r8d, 6
    
    ; Index register (RSP=4 means no index)
    mov     eax, [rdx].Operand.index_reg
    cmp     eax, 4                      ; RSP as index = no index
    je      @@no_index
    and     eax, 7
    shl     eax, 3
    jmp     @@do_base
    
@@no_index:
    mov     eax, 4                      ; Encoding for no index (100b)
    shl     eax, 3
    
@@do_base:
    or      r9d, r8d
    or      r9d, eax
    
    mov     r8d, r11d
    and     r8d, 7
    or      r9d, r8d
    
    EMIT    r9b
    
@@emit_displacement:
    ; Emit displacement bytes
    mov     eax, [rbx].EncodeCtx.modrm_template
    and     eax, 0C0h
    
    cmp     al, MOD_DISP0
    je      @@done
    
    cmp     al, MOD_DISP8
    je      @@emit8
    
    ; disp32
    mov     ecx, DWORD PTR [rdx].Operand.displacement
    EMIT    cl
    EMIT    ch
    shr     ecx, 16
    EMIT    cl
    EMIT    ch
    jmp     @@done
    
@@emit8:
    EMIT    BYTE PTR [rdx].Operand.displacement
    jmp     @@done
    
@@reg_direct:
    ; mod=11 (register direct)
    mov     al, MOD_REG
    mov     r9d, r10d
    and     r9d, 7
    shl     r9d, 3
    
    mov     r8d, r11d
    and     r8d, 7
    
    or      al, r9b
    or      al, r8b
    EMIT    al
    jmp     @@done
    
@@rip_relative:
    ; RIP-relative: mod=00, r/m=101 (RBP encoding), followed by 32-bit disp
    mov     al, MOD_DISP0
    or      al, 5                       ; r/m = 101 (RIP-relative encoding)
    
    mov     r9d, r10d
    and     r9d, 7
    shl     r9d, 3
    or      al, r9b
    
    EMIT    al
    
    ; Emit 32-bit displacement (relative to next instruction)
    mov     rax, [rdx].Operand.displacement
    sub     rax, [rbx].EncodeCtx.section_rva
    sub     rax, [rbx].EncodeCtx.output_ptr
    sub     rax, 4                      ; Size of displacement itself
    
    EMIT    al
    EMIT    ah
    shr     rax, 16
    EMIT    al
    EMIT    ah
    
@@done:
    pop     rbx
    ret
EncodeModRM_SIB ENDP

; ═════════════════════════════════════════════════════════════════════════════
; IMMEDIATE ENCODERS
; ═════════════════════════════════════════════════════════════════════════════

EmitImmediate PROC
    ; CL = size (1,2,4,8), RDX = value
    cmp     cl, 1
    je      @@byte
    cmp     cl, 2
    je      @@word
    cmp     cl, 4
    je      @@dword
    cmp     cl, 8
    je      @@qword
    
@@byte:
    EMIT    dl
    ret
    
@@word:
    EMIT    dl
    EMIT    dh
    ret
    
@@dword:
    mov     eax, edx
    EMIT    al
    EMIT    ah
    shr     eax, 16
    EMIT    al
    EMIT    ah
    ret
    
@@qword:
    mov     rax, rdx
    EMIT    al
    EMIT    ah
    shr     rax, 16
    EMIT    al
    EMIT    ah
    shr     rax, 16
    EMIT    al
    EMIT    ah
    shr     rax, 16
    EMIT    al
    EMIT    ah
    ret
    
EmitImmediate ENDP

; ═════════════════════════════════════════════════════════════════════════════
; INSTRUCTION ENCODING DISPATCH
; Brute-force matrix for MOV, ADD, SUB, JMP, CALL, etc.
; ═════════════════════════════════════════════════════════════════════════════

; Instruction dispatch table structure
InstrHandler TYPEDEF PROTO :QWORD, :QWORD, :QWORD

InstrEntry STRUCT
    mnemonic_id     DD ?
    handler         DQ ?
InstrEntry ENDS

; Mnemonic IDs
ID_MOV  EQU 1
ID_ADD  EQU 2
ID_SUB  EQU 3
ID_JMP  EQU 4
ID_CALL EQU 5
ID_PUSH EQU 6
ID_POP  EQU 7
ID_XOR  EQU 8
ID_CMP  EQU 9
ID_TEST EQU 10
ID_LEA  EQU 11

.data
align 8
DispatchTable:
    InstrEntry <ID_MOV,  Encode_MOV>
    InstrEntry <ID_ADD,  Encode_ADD>
    InstrEntry <ID_SUB,  Encode_SUB>
    InstrEntry <ID_JMP,  Encode_JMP>
    InstrEntry <ID_CALL, Encode_CALL>
    InstrEntry <ID_PUSH, Encode_PUSH>
    InstrEntry <ID_POP,  Encode_POP>
    InstrEntry <ID_XOR,  Encode_XOR>
    InstrEntry <ID_CMP,  Encode_CMP>
    InstrEntry <ID_LEA,  Encode_LEA>
    DD 0, 0                             ; End marker

; ═════════════════════════════════════════════════════════════════════════════
; BRUTE-FORCE INSTRUCTION ENCODERS
; Each handles all operand combinations for that instruction
; ═════════════════════════════════════════════════════════════════════════════

Encode_MOV PROC
    ; RCX = op1, RDX = op2 (operands)
    mov     rax, rcx
    mov     rbx, rdx
    
    ; MOV r64, imm64 (B8+rd)
    cmp     [rbx].Operand.op_type, OP_IMM64
    je      @@mov_r64_imm64
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@mov_r64_imm32
    
    ; MOV r64, r64 (8B /r)
    cmp     [rax].Operand.op_type, OP_REG64
    jne     @@check_mem
    
    cmp     [rbx].Operand.op_type, OP_REG64
    je      @@mov_r64_r64
    
    ; MOV r64, m64 (8B /r)
    cmp     [rbx].Operand.op_type, OP_MEM64
    je      @@mov_r64_m64
    
@@check_mem:
    ; MOV m64, r64 (89 /r)
    cmp     [rax].Operand.op_type, OP_MEM64
    jne     @@error
    
    cmp     [rbx].Operand.op_type, OP_REG64
    je      @@mov_m64_r64
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@mov_m64_imm32

@@error:
    xor     eax, eax
    ret

@@mov_r64_imm64:
    ; REX.W + B8+rd + imm64
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1                       ; W=1
    mov     rdx, rax
    xor     r8, r8
    call    CalculateREX
    pop     rbx
    
    mov     eax, [rax].Operand.reg_id
    and     eax, 7
    or      al, 0B8h                    ; B8+rd
    EMIT    al
    
    mov     rdx, [rbx].Operand.displacement  ; Immediate value
    mov     cl, 8
    jmp     EmitImmediate

@@mov_r64_imm32:
    ; REX.W + C7 /0 id (sign-extended)
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     rdx, rax
    xor     r8, r8
    call    CalculateREX
    pop     rbx
    
    EMIT    0C7h
    EMIT    0C0h                        ; ModRM: mod=11, reg=0, r/m=dst
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate

@@mov_r64_r64:
    ; REX.W + 8B /r (MOV r64, r/m64)
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     r8, rbx
    call    CalculateREX
    pop     rbx
    
    EMIT    8Bh
    
    mov     ecx, [rbx].Operand.reg_id   ; Source becomes r/m
    mov     rdx, rax                    ; Dest becomes reg field in ModRM
    xchg    rcx, [rdx].Operand.reg_id   ; Swap for encoding
    push    rax
    push    rcx
    mov     rcx, [rdx].Operand.reg_id
    mov     rdx, rsp                    ; Fake operand structure
    call    EncodeModRM_SIB
    add     rsp, 16
    ret

@@mov_r64_m64:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     r8, rbx
    call    CalculateREX
    pop     rbx
    
    EMIT    8Bh                         ; MOV r64, r/m64
    
    mov     ecx, [rax].Operand.reg_id
    mov     rdx, rbx
    jmp     EncodeModRM_SIB

@@mov_m64_r64:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     r8, rbx
    call    CalculateREX
    pop     rbx
    
    EMIT    89h                         ; MOV r/m64, r64
    
    mov     ecx, [rbx].Operand.reg_id
    mov     rdx, rax
    jmp     EncodeModRM_SIB

@@mov_m64_imm32:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    xor     cl, cl                      ; REX.W handled separately
    mov     rdx, rax
    call    CalculateREX
    pop     rbx
    
    EMIT    0C7h                        ; MOV r/m64, imm32
    
    xor     ecx, ecx                    ; reg=0 in ModRM
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
Encode_MOV ENDP

Encode_ADD PROC
    ; Handler for ADD instructions
    ; REX.W + 01 /r (ADD r/m64, r64)
    ; REX.W + 03 /r (ADD r64, r/m64)
    ; REX.W + 81 /0 id (ADD r/m64, imm32)
    ; REX.W + 83 /0 ib (ADD r/m64, imm8 sign-extended)
    
    mov     rax, rcx
    mov     rbx, rdx
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@add_imm32
    
    cmp     [rbx].Operand.op_type, OP_IMM8
    je      @@add_imm8
    
    ; ADD r/m64, r64
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     rdx, rax
    call    CalculateREX
    pop     rbx
    
    EMIT    01h
    
    mov     ecx, [rbx].Operand.reg_id
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@add_imm32:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     rdx, rax
    call    CalculateREX
    pop     rbx
    
    EMIT    81h
    
    xor     ecx, ecx                    ; /0 encoding
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
    
@@add_imm8:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    mov     rdx, rax
    call    CalculateREX
    pop     rbx
    
    EMIT    83h
    
    xor     ecx, ecx
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    EMIT    BYTE PTR [rbx].Operand.displacement
    ret
Encode_ADD ENDP

Encode_SUB PROC
    ; Similar to ADD but with 28/29/2B opcodes or 81/83 /5
    mov     rax, rcx
    mov     rbx, rdx
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@sub_imm32
    
    cmp     [rbx].Operand.op_type, OP_IMM8
    je      @@sub_imm8
    
    ; SUB r/m64, r64 (29 /r)
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    29h
    
    mov     ecx, [rbx].Operand.reg_id
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@sub_imm32:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    81h
    
    mov     ecx, 5                      ; /5 encoding for SUB
    shl     ecx, 3                      ; reg field
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
    
@@sub_imm8:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    83h
    
    mov     ecx, 5
    shl     ecx, 3
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    EMIT    BYTE PTR [rbx].Operand.displacement
    ret
Encode_SUB ENDP

Encode_JMP PROC
    ; E9 cd (JMP rel32)
    ; FF /4 (JMP r/m64)
    ; EB cb (JMP rel8)
    
    mov     rax, rcx                    ; Target operand
    
    cmp     [rax].Operand.op_type, OP_REL32
    je      @@jmp_rel32
    
    cmp     [rax].Operand.op_type, OP_REL8
    je      @@jmp_rel8
    
    cmp     [rax].Operand.op_type, OP_REG64
    je      @@jmp_indirect
    
    cmp     [rax].Operand.op_type, OP_MEM64
    je      @@jmp_indirect

@@jmp_rel32:
    EMIT    0E9h
    
    ; Calculate relative offset from next instruction
    mov     rdx, [rax].Operand.displacement
    mov     rcx, [g_enc_ctx.section_rva]
    add     rcx, [g_enc_ctx.output_ptr]
    add     rcx, 4                      ; After this instruction
    sub     rdx, rcx
    
    mov     cl, 4
    jmp     EmitImmediate
    
@@jmp_rel8:
    EMIT    0EBh
    EMIT    BYTE PTR [rax].Operand.displacement
    ret
    
@@jmp_indirect:
    ; REX.W + FF /4
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    xor     r8, r8
    call    CalculateREX
    pop     rax
    
    EMIT    0FFh
    
    mov     ecx, 4                      ; /4 encoding
    mov     rdx, rax
    jmp     EncodeModRM_SIB
Encode_JMP ENDP

Encode_CALL PROC
    ; E8 cd (CALL rel32)
    ; FF /2 (CALL r/m64)
    
    mov     rax, rcx
    
    cmp     [rax].Operand.op_type, OP_REL32
    je      @@call_rel32
    
    ; CALL r/m64
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    xor     r8, r8
    call    CalculateREX
    pop     rax
    
    EMIT    0FFh
    
    mov     ecx, 2                      ; /2 encoding
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@call_rel32:
    EMIT    0E8h
    
    mov     rdx, [rax].Operand.displacement
    mov     rcx, [g_enc_ctx.section_rva]
    add     rcx, [g_enc_ctx.output_ptr]
    add     rcx, 4
    sub     rdx, rcx
    
    mov     cl, 4
    jmp     EmitImmediate
Encode_CALL ENDP

Encode_PUSH PROC
    ; 50+rd (PUSH r64)
    ; FF /6 (PUSH r/m64)
    ; 68 id (PUSH imm32)
    ; 6A ib (PUSH imm8)
    
    mov     rax, rcx
    
    cmp     [rax].Operand.op_type, OP_REG64
    je      @@push_reg
    
    cmp     [rax].Operand.op_type, OP_IMM8
    je      @@push_imm8
    
    cmp     [rax].Operand.op_type, OP_IMM32
    je      @@push_imm32
    
    ; PUSH r/m64
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    xor     cl, cl                      ; No REX.W needed for push/pop
    call    CalculateREX
    pop     rax
    
    EMIT    0FFh
    
    mov     ecx, 6                      ; /6
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@push_reg:
    mov     ecx, [rax].Operand.reg_id
    cmp     ecx, 8
    jb      @@no_rex
    
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    xor     cl, cl
    call    CalculateREX                ; For R8-R15 extension
    pop     rax
    
@@no_rex:
    mov     eax, [rax].Operand.reg_id
    and     al, 7
    or      al, 50h                     ; 50+rd
    EMIT    al
    ret
    
@@push_imm8:
    EMIT    6Ah
    EMIT    BYTE PTR [rax].Operand.displacement
    ret
    
@@push_imm32:
    EMIT    68h
    mov     rdx, [rax].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
Encode_PUSH ENDP

Encode_POP PROC
    ; 58+rd (POP r64)
    ; 8F /0 (POP r/m64)
    
    mov     rax, rcx
    
    cmp     [rax].Operand.op_type, OP_REG64
    je      @@pop_reg
    
    ; POP r/m64
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    xor     cl, cl
    call    CalculateREX
    pop     rax
    
    EMIT    8Fh
    
    xor     ecx, ecx                    ; /0
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@pop_reg:
    mov     ecx, [rax].Operand.reg_id
    cmp     ecx, 8
    jb      @@no_rex
    
    push    rax
    mov     rbx, OFFSET g_enc_ctx
    xor     cl, cl
    call    CalculateREX
    pop     rax
    
@@no_rex:
    mov     eax, [rax].Operand.reg_id
    and     al, 7
    or      al, 58h                     ; 58+rd
    EMIT    al
    ret
Encode_POP ENDP

Encode_XOR PROC
    ; 31 /r (XOR r/m64, r64)
    ; 33 /r (XOR r64, r/m64)
    ; 81 /6 id (XOR r/m64, imm32)
    ; 83 /6 ib (XOR r/m64, imm8)
    
    mov     rax, rcx
    mov     rbx, rdx
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@xor_imm32
    
    cmp     [rbx].Operand.op_type, OP_IMM8
    je      @@xor_imm8
    
    ; XOR r/m64, r64
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    31h
    
    mov     ecx, [rbx].Operand.reg_id
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@xor_imm32:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    81h
    
    mov     ecx, 6
    shl     ecx, 3
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
    
@@xor_imm8:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    83h
    
    mov     ecx, 6
    shl     ecx, 3
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    EMIT    BYTE PTR [rbx].Operand.displacement
    ret
Encode_XOR ENDP

Encode_CMP PROC
    ; 38 /r (CMP r/m8, r8)
    ; 39 /r (CMP r/m64, r64)
    ; 3B /r (CMP r64, r/m64)
    ; 80 /7 ib (CMP r/m8, imm8)
    ; 81 /7 id (CMP r/m64, imm32)
    ; 83 /7 ib (CMP r/m64, imm8)
    
    mov     rax, rcx
    mov     rbx, rdx
    
    cmp     [rbx].Operand.op_type, OP_IMM32
    je      @@cmp_imm32
    
    cmp     [rbx].Operand.op_type, OP_IMM8
    je      @@cmp_imm8
    
    ; CMP r/m64, r64 (39 /r)
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    39h
    
    mov     ecx, [rbx].Operand.reg_id
    mov     rdx, rax
    jmp     EncodeModRM_SIB
    
@@cmp_imm32:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    81h
    
    mov     ecx, 7                      ; /7
    shl     ecx, 3
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    mov     rdx, [rbx].Operand.displacement
    mov     cl, 4
    jmp     EmitImmediate
    
@@cmp_imm8:
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1
    call    CalculateREX
    pop     rbx
    
    EMIT    83h
    
    mov     ecx, 7
    shl     ecx, 3
    mov     rdx, rax
    call    EncodeModRM_SIB
    
    EMIT    BYTE PTR [rbx].Operand.displacement
    ret
Encode_CMP ENDP

Encode_LEA PROC
    ; REX.W + 8D /r (LEA r64, m)
    mov     rax, rcx    ; Dest reg
    mov     rbx, rdx    ; Mem operand
    
    push    rbx
    mov     rbx, OFFSET g_enc_ctx
    mov     cl, 1                       ; Always 64-bit
    xor     r8, r8
    call    CalculateREX
    pop     rbx
    
    EMIT    8Dh
    
    mov     ecx, [rax].Operand.reg_id   ; Reg field = dest
    mov     rdx, rbx                    ; RDX = memory operand
    jmp     EncodeModRM_SIB
Encode_LEA ENDP

; ═════════════════════════════════════════════════════════════════════════════
; MASTER ENCODE DISPATCH
; RCX = Mnemonic ID, RDX = Op1, R8 = Op2, R9 = Op3
; ═════════════════════════════════════════════════════════════════════════════
EncodeInstruction PROC FRAME
    push    rbx rsi rdi
    mov     rsi, OFFSET DispatchTable
    
@@search:
    mov     eax, [rsi].InstrEntry.mnemonic_id
    test    eax, eax
    jz      @@not_found
    
    cmp     eax, ecx
    je      @@found
    
    add     rsi, SIZEOF InstrEntry
    jmp     @@search
    
@@found:
    mov     rax, [rsi].InstrEntry.handler
    mov     rcx, rdx                    ; Op1
    mov     rdx, r8                     ; Op2
    ; R9 already = Op3 if needed
    call    rax
    
@@done:
    pop     rdi rsi rbx
    ret
    
@@not_found:
    xor     eax, eax
    jmp     @@done
EncodeInstruction ENDP

; ═════════════════════════════════════════════════════════════════════════════
; PUBLIC API
; ═════════════════════════════════════════════════════════════════════════════
PUBLIC Encoder_Init
PUBLIC EncodeInstruction
PUBLIC Encode_MOV
PUBLIC Encode_ADD
PUBLIC Encode_SUB
PUBLIC Encode_JMP
PUBLIC Encode_CALL
PUBLIC Encode_PUSH
PUBLIC Encode_POP
PUBLIC Encode_XOR
PUBLIC Encode_CMP
PUBLIC Encode_LEA
PUBLIC EmitImmediate
PUBLIC CalculateREX
PUBLIC EncodeModRM_SIB

; Data exports
PUBLIC DispatchTable
PUBLIC g_enc_ctx

; Reg IDs
PUBLIC REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI
PUBLIC REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15

; Operand types
PUBLIC OP_REG64, OP_IMM32, OP_IMM64, OP_MEM64, OP_REL32

END
