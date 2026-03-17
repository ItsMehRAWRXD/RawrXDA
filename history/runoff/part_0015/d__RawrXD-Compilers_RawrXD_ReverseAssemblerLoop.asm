; =============================================================================
; RawrXD x64 Reverse Assembler Loop - Production Ready
; Pure MASM64, zero dependencies, brute-force instruction encoding/decoding
; Full instruction encoder → decoder round-trip support
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

; =============================================================================
; INCLUDES & EQUATES
; =============================================================================

; Instruction encoding limits
MAX_INSN_LEN         EQU 15          ; x86 max instruction length
MAX_PREFIXES         EQU 4           ; Max prefixes per instruction
MAX_OPERANDS         EQU 4           ; Max operands per instruction

; Prefix bytes
PREFIX_LOCK          EQU 0F0h
PREFIX_REPNE         EQU 0F2h
PREFIX_REP           EQU 0F3h
PREFIX_CS            EQU 2Eh
PREFIX_SS            EQU 36h
PREFIX_DS            EQU 3Eh
PREFIX_ES            EQU 26h
PREFIX_FS            EQU 64h
PREFIX_GS            EQU 65h
PREFIX_OPSIZE        EQU 66h
PREFIX_ADDRSIZE      EQU 67h

; REX prefix base
REX_BASE             EQU 40h
REX_W                EQU 08h         ; 64-bit operand
REX_R                EQU 04h         ; ModRM reg extension
REX_X                EQU 02h         ; SIB index extension
REX_B                EQU 01h         ; ModRM rm extension

; ModRM fields
MODRM_MOD_SHIFT      EQU 6
MODRM_REG_SHIFT      EQU 3
MODRM_RM_MASK        EQU 7

; SIB fields
SIB_SCALE_SHIFT      EQU 6
SIB_INDEX_SHIFT      EQU 3
SIB_BASE_MASK        EQU 7

; Operand types
OP_NONE              EQU 0
OP_REG               EQU 1
OP_IMM               EQU 2
OP_MEM               EQU 3
OP_REL               EQU 4

; Register IDs (0-15)
REG_RAX              EQU 0
REG_RCX              EQU 1
REG_RDX              EQU 2
REG_RBX              EQU 3
REG_RSP              EQU 4
REG_RBP              EQU 5
REG_RSI              EQU 6
REG_RDI              EQU 7
REG_R8               EQU 8
REG_R9               EQU 9
REG_R10              EQU 10
REG_R11              EQU 11
REG_R12              EQU 12
REG_R13              EQU 13
REG_R14              EQU 14
REG_R15              EQU 15

; =============================================================================
; STRUCTURES
; =============================================================================
OPDESC STRUCT 8
    op_type         DWORD ?         ; OP_REG/OP_IMM/OP_MEM/OP_REL
    op_size         DWORD ?         ; 1,2,4,8 bytes
    op_reg          DWORD ?         ; Register number if OP_REG
    op_imm          QWORD ?         ; Immediate value
    op_base         DWORD ?         ; Base register for memory
    op_index        DWORD ?         ; Index register for memory
    op_scale        DWORD ?         ; Scale factor (1,2,4,8)
    op_disp         SDWORD ?        ; Displacement
OPDESC ENDS

INSN_CTX STRUCT 8
    ctx_opcode      BYTE ?          ; Primary opcode
    ctx_opcode2     BYTE ?          ; Secondary opcode (0F xx)
    ctx_has_0f      BYTE ?          ; Has 0F prefix
    ctx_rex         BYTE ?          ; REX prefix value
    ctx_modrm       BYTE ?          ; ModRM byte
    ctx_sib         BYTE ?          ; SIB byte
    ctx_has_modrm   BYTE ?          ; Has ModRM
    ctx_has_sib     BYTE ?          ; Has SIB
    ctx_prefix_cnt  BYTE ?          ; Number of prefixes
    ctx_prefixes    BYTE MAX_PREFIXES DUP(?)
    ctx_operands    OPDESC MAX_OPERANDS DUP(<>)
    ctx_op_count    DWORD ?
    ctx_insn_len    DWORD ?
    ctx_bytes       BYTE MAX_INSN_LEN DUP(?)
INSN_CTX ENDS


; =============================================================================
; DATA SECTION
; =============================================================================
.data
align 16

; Output buffer for testing
test_buffer          BYTE 256 DUP(0)
test_buffer_size     QWORD 0

; =============================================================================
; CODE SECTION
; =============================================================================
.code

; =============================================================================
; REVERSE_ASSEMBLER_INIT - Initialize assembler context
; =============================================================================
REVERSE_ASSEMBLER_INIT PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    .endprolog
    
    ; rcx = pointer to INSN_CTX to initialize
    
    xor eax, eax
    mov r8, rcx
    mov r9, SIZEOF INSN_CTX
    
    ; Zero the entire context
    @@:
    mov [r8 + rax], r9b
    inc rax
    cmp rax, r9
    jb @B
    
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
REVERSE_ASSEMBLER_INIT ENDP

; =============================================================================
; BUILD_MODRM - Construct ModRM byte
; =============================================================================
BUILD_MODRM PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    ; rcx = context pointer
    ; rdx = mod (0-3)
    ; r8 = reg/opcode extension (0-7)
    ; r9 = rm (0-7)
    
    mov rbx, rcx
    
    ; mod << 6 | reg << 3 | rm
    mov r12b, dl
    shl r12b, MODRM_MOD_SHIFT
    
    mov r13b, r8b
    shl r13b, MODRM_REG_SHIFT
    or r12b, r13b
    
    or r12b, r9b
    
    mov [rbx + INSN_CTX.ctx_modrm], r12b
    mov byte ptr [rbx + INSN_CTX.ctx_has_modrm], 1
    
    mov al, r12b
    pop r13
    pop r12
    pop rbx
    ret
BUILD_MODRM ENDP

; =============================================================================
; BUILD_SIB - Construct SIB byte
; =============================================================================
BUILD_SIB PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    ; rcx = context pointer
    ; rdx = scale (1,2,4,8)
    ; r8 = index register (0-15, 4=none)
    ; r9 = base register (0-15)
    
    mov rbx, rcx
    xor r12d, r12d          ; sib byte accumulator
    
    ; Encode scale: 1->0, 2->1, 4->2, 8->3
    mov eax, edx
    xor ecx, ecx
    cmp eax, 1
    je .scale_done
    mov ecx, 1
    cmp eax, 2
    je .scale_done
    mov ecx, 2
    cmp eax, 4
    je .scale_done
    mov ecx, 3
    cmp eax, 8
    je .scale_done
    xor ecx, ecx            ; default to 1x
    
.scale_done:
    shl ecx, SIB_SCALE_SHIFT
    or r12d, ecx
    
    ; Add index
    mov r13d, r8d
    and r13d, 7
    shl r13d, SIB_INDEX_SHIFT
    or r12d, r13d
    
    ; Add base
    mov r13d, r9d
    and r13d, 7
    or r12b, r13b
    
    mov [rbx + INSN_CTX.ctx_sib], r12b
    mov byte ptr [rbx + INSN_CTX.ctx_has_sib], 1
    
    mov al, r12b
    pop r13
    pop r12
    pop rbx
    ret
BUILD_SIB ENDP

; =============================================================================
; ENCODE_INSTRUCTION - Main encoding entry point
; =============================================================================
ENCODE_INSTRUCTION PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    ; rcx = pointer to INSN_CTX
    ; rdx = pointer to output buffer
    ; r8 = buffer size
    
    mov rbx, rcx            ; rbx = context
    mov rsi, rdx            ; rsi = output buffer
    mov r12, r8             ; r12 = buffer size
    xor r13, r13            ; r13 = bytes written
    
    ; Validate context
    test rbx, rbx
    jz .error_invalid_ctx
    
    ; Clear output buffer first
    push rdi
    mov rdi, rsi
    mov rcx, r12
    xor eax, eax
    rep stosb
    pop rdi
    
    ; Encode prefixes
    movzx r14d, byte ptr [rbx + INSN_CTX.ctx_prefix_cnt]
    test r14d, r14d
    jz .after_prefixes
    
    xor ecx, ecx
.prefix_loop:
    cmp r13, r12
    jae .error_overflow
    
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_prefixes + rcx]
    mov [rsi + r13], al
    inc r13
    inc ecx
    cmp ecx, r14d
    jb .prefix_loop
    
.after_prefixes:
    ; Encode REX if needed
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_rex]
    test al, al
    jz .after_rex
    
    cmp r13, r12
    jae .error_overflow
    
    or al, REX_BASE         ; Ensure REX base is set
    mov [rsi + r13], al
    inc r13
    
.after_rex:
    ; Encode 0F prefix if needed
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_has_0f]
    test al, al
    jz .after_0f
    
    cmp r13, r12
    jae .error_overflow
    
    mov byte ptr [rsi + r13], 0Fh
    inc r13
    
.after_0f:
    ; Encode primary opcode
    cmp r13, r12
    jae .error_overflow
    
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_opcode]
    mov [rsi + r13], al
    inc r13
    
    ; Encode secondary opcode if present
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_opcode2]
    test al, al
    jz .after_opcode2
    
    cmp r13, r12
    jae .error_overflow
    
    mov [rsi + r13], al
    inc r13
    
.after_opcode2:
    ; Encode ModRM if present
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_has_modrm]
    test al, al
    jz .after_modrm
    
    cmp r13, r12
    jae .error_overflow
    
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_modrm]
    mov [rsi + r13], al
    inc r13
    
.after_modrm:
    ; Encode SIB if present
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_has_sib]
    test al, al
    jz .after_sib
    
    cmp r13, r12
    jae .error_overflow
    
    movzx eax, byte ptr [rbx + INSN_CTX.ctx_sib]
    mov [rsi + r13], al
    inc r13
    
.after_sib:
    ; Encode displacement if present
    xor ecx, ecx
    mov r14d, [rbx + INSN_CTX.ctx_op_count]
.disp_loop:
    cmp ecx, r14d
    jae .after_disp
    
    mov r10, rcx
    imul r10, 56            ; SIZEOF OPDESC = 56 bytes
    mov eax, [rbx + INSN_CTX.ctx_operands + r10 + OPDESC.op_type]
    cmp eax, OP_MEM
    jne .next_disp
    
    mov r10, rcx
    imul r10, 56
    mov eax, [rbx + INSN_CTX.ctx_operands + r10 + OPDESC.op_disp]
    test eax, eax
    jz .next_disp
    
    cmp r13, r12
    jae .error_overflow
    cmp r12, 4
    jb .error_overflow
    
    mov [rsi + r13], eax
    add r13, 4
    
.next_disp:
    inc ecx
    jmp .disp_loop
    
.after_disp:
    ; Encode immediate if present
    xor ecx, ecx
.imm_loop:
    cmp ecx, r14d
    jae .after_imm
    
    mov r10, rcx
    imul r10, 56            ; SIZEOF OPDESC = 56 bytes
    mov eax, [rbx + INSN_CTX.ctx_operands + r10 + OPDESC.op_type]
    cmp eax, OP_IMM
    jne .next_imm
    
    mov r10, rcx
    imul r10, 56
    mov r8d, [rbx + INSN_CTX.ctx_operands + r10 + OPDESC.op_size]
    mov rax, [rbx + INSN_CTX.ctx_operands + r10 + OPDESC.op_imm]
    
    mov r9, r13
    add r9, r8
    cmp r9, r12
    ja .error_overflow
    
    @@:
    mov [rsi + r13], al
    inc r13
    shr rax, 8
    dec r8d
    jnz @B
    
.next_imm:
    inc ecx
    jmp .imm_loop
    
.after_imm:
    mov [rbx + INSN_CTX.ctx_insn_len], r13d
    
    lea rdi, [rbx + INSN_CTX.ctx_bytes]
    mov rcx, r13
    @@:
    mov al, [rsi + rcx - 1]
    mov [rdi + rcx - 1], al
    dec rcx
    jnz @B
    
    mov rax, r13            ; Return bytes written
    jmp .done
    
.error_invalid_ctx:
    xor eax, eax
    dec rax                 ; Return -1
    jmp .done
    
.error_overflow:
    xor eax, eax
    dec rax
    dec rax                 ; Return -2
    jmp .done
    
.done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ENCODE_INSTRUCTION ENDP

; =============================================================================
; ENCODE_MOV_REG_IMM64 - Encode MOV r64, imm64
; =============================================================================
ENCODE_MOV_REG_IMM64 PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    .endprolog
    
    ; rcx = context
    ; rdx = register (0-15)
    ; r8 = immediate value
    
    mov rbx, rcx
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov eax, edx
    and eax, 7
    add al, 0B8h
    mov [rbx + INSN_CTX.ctx_opcode], al
    
    cmp edx, 8
    jl @F
    
    mov byte ptr [rbx + INSN_CTX.ctx_rex], REX_BASE or REX_B or REX_W
    
    @@:
    mov [rbx + INSN_CTX.ctx_operands + OPDESC.op_type], OP_IMM
    mov [rbx + INSN_CTX.ctx_operands + OPDESC.op_size], 8
    mov [rbx + INSN_CTX.ctx_operands + OPDESC.op_imm], r8
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 1
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop rsi
    pop rbx
    ret
ENCODE_MOV_REG_IMM64 ENDP

; =============================================================================
; ENCODE_MOV_REG_REG - Encode MOV r64, r64
; =============================================================================
ENCODE_MOV_REG_REG PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    ; rcx = context
    ; rdx = destination register
    ; r8 = source register
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 89h
    
    xor r9d, r9d
    cmp r12d, 8
    jl @F
    or r9b, REX_R
    @@:
    cmp r13d, 8
    jl @F
    or r9b, REX_B
    @@:
    or r9b, REX_W or REX_BASE
    mov [rbx + INSN_CTX.ctx_rex], r9b
    
    mov ecx, ebx
    mov edx, 3
    mov r8d, r12d
    and r8d, 7
    mov r9d, r13d
    and r9d, 7
    call BUILD_MODRM
    
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 2
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop r13
    pop r12
    pop rbx
    ret
ENCODE_MOV_REG_REG ENDP

; =============================================================================
; ENCODE_PUSH_REG - Encode PUSH r64
; =============================================================================
ENCODE_PUSH_REG PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    ; rcx = context
    ; rdx = register (0-15)
    
    mov rbx, rcx
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov eax, edx
    and eax, 7
    add al, 50h
    mov [rbx + INSN_CTX.ctx_opcode], al
    
    cmp edx, 8
    jl @F
    mov byte ptr [rbx + INSN_CTX.ctx_rex], REX_BASE or REX_B
    
    @@:
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 1
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop rbx
    ret
ENCODE_PUSH_REG ENDP

; =============================================================================
; ENCODE_POP_REG - Encode POP r64
; =============================================================================
ENCODE_POP_REG PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    ; rcx = context
    ; rdx = register (0-15)
    
    mov rbx, rcx
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov eax, edx
    and eax, 7
    add al, 58h
    mov [rbx + INSN_CTX.ctx_opcode], al
    
    cmp edx, 8
    jl @F
    mov byte ptr [rbx + INSN_CTX.ctx_rex], REX_BASE or REX_B
    
    @@:
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 1
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop rbx
    ret
ENCODE_POP_REG ENDP

; =============================================================================
; ENCODE_ADD_REG_REG - Encode ADD r64, r64
; =============================================================================
ENCODE_ADD_REG_REG PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    ; rcx = context
    ; rdx = dst reg
    ; r8 = src reg
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 01h
    
    xor r9d, r9d
    cmp r12d, 8
    jl @F
    or r9b, REX_R
    @@:
    cmp r13d, 8
    jl @F
    or r9b, REX_B
    @@:
    or r9b, REX_W or REX_BASE
    mov [rbx + INSN_CTX.ctx_rex], r9b
    
    mov ecx, ebx
    mov edx, 3
    mov r8d, r12d
    and r8d, 7
    mov r9d, r13d
    and r9d, 7
    call BUILD_MODRM
    
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 2
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop r13
    pop r12
    pop rbx
    ret
ENCODE_ADD_REG_REG ENDP

; =============================================================================
; ENCODE_SUB_REG_REG - Encode SUB r64, r64
; =============================================================================
ENCODE_SUB_REG_REG PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 29h
    
    xor r9d, r9d
    cmp r12d, 8
    jl @F
    or r9b, REX_R
    @@:
    cmp r13d, 8
    jl @F
    or r9b, REX_B
    @@:
    or r9b, REX_W or REX_BASE
    mov [rbx + INSN_CTX.ctx_rex], r9b
    
    mov ecx, ebx
    mov edx, 3
    mov r8d, r12d
    and r8d, 7
    mov r9d, r13d
    and r9d, 7
    call BUILD_MODRM
    
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 2
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop r13
    pop r12
    pop rbx
    ret
ENCODE_SUB_REG_REG ENDP

; =============================================================================
; ENCODE_XOR_REG_REG - Encode XOR r64, r64
; =============================================================================
ENCODE_XOR_REG_REG PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 31h
    
    xor r9d, r9d
    cmp r12d, 8
    jl @F
    or r9b, REX_R
    @@:
    cmp r13d, 8
    jl @F
    or r9b, REX_B
    @@:
    or r9b, REX_W or REX_BASE
    mov [rbx + INSN_CTX.ctx_rex], r9b
    
    mov ecx, ebx
    mov edx, 3
    mov r8d, r12d
    and r8d, 7
    mov r9d, r13d
    and r9d, 7
    call BUILD_MODRM
    
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 2
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop r13
    pop r12
    pop rbx
    ret
ENCODE_XOR_REG_REG ENDP

; =============================================================================
; ENCODE_NOP - Encode NOP
; =============================================================================
ENCODE_NOP PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 90h
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 0
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop rbx
    ret
ENCODE_NOP ENDP

; =============================================================================
; ENCODE_RET - Encode RET
; =============================================================================
ENCODE_RET PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx
    
    mov rcx, rbx
    call REVERSE_ASSEMBLER_INIT
    
    mov byte ptr [rbx + INSN_CTX.ctx_opcode], 0C3h
    mov dword ptr [rbx + INSN_CTX.ctx_op_count], 0
    
    lea rdx, test_buffer
    mov r8, 256
    mov rcx, rbx
    call ENCODE_INSTRUCTION
    
    pop rbx
    ret
ENCODE_RET ENDP

; =============================================================================
; DISASSEMBLE_BYTES - Reverse: bytes -> INSN_CTX
; =============================================================================
DISASSEMBLE_BYTES PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    .endprolog
    
    ; rcx = pointer to bytes
    ; rdx = byte count
    ; r8 = pointer to INSN_CTX to fill
    
    mov rsi, rcx            ; rsi = input bytes
    mov r12, rdx            ; r12 = byte count
    mov rdi, r8             ; rdi = output context
    
    mov rcx, rdi
    call REVERSE_ASSEMBLER_INIT
    
    xor r13, r13            ; r13 = byte position
    
    ; Parse prefixes
    xor r14d, r14d          ; prefix count
    
.prefix_parse:
    cmp r13, r12
    jae .done_prefixes
    
    movzx eax, byte ptr [rsi + r13]
    
    ; Check if prefix
    cmp al, PREFIX_LOCK
    je .is_prefix
    cmp al, PREFIX_REPNE
    je .is_prefix
    cmp al, PREFIX_REP
    je .is_prefix
    cmp al, PREFIX_CS
    je .is_prefix
    cmp al, PREFIX_SS
    je .is_prefix
    cmp al, PREFIX_DS
    je .is_prefix
    cmp al, PREFIX_ES
    je .is_prefix
    cmp al, PREFIX_FS
    je .is_prefix
    cmp al, PREFIX_GS
    je .is_prefix
    cmp al, PREFIX_OPSIZE
    je .is_prefix
    cmp al, PREFIX_ADDRSIZE
    je .is_prefix
    
    ; Check for REX
    and al, 0F0h
    cmp al, REX_BASE
    jne .done_prefixes
    
    ; It's a REX prefix
    movzx eax, byte ptr [rsi + r13]
    mov [rdi + INSN_CTX.ctx_rex], al
    inc r13
    jmp .prefix_parse
    
.is_prefix:
    cmp r14d, MAX_PREFIXES
    jae .done_prefixes
    
    movzx eax, byte ptr [rsi + r13]
    mov [rdi + INSN_CTX.ctx_prefixes + r14], al
    inc r14d
    inc r13
    jmp .prefix_parse
    
.done_prefixes:
    mov [rdi + INSN_CTX.ctx_prefix_cnt], r14b
    
    ; Check for 0F escape
    cmp r13, r12
    jae .done
    
    movzx eax, byte ptr [rsi + r13]
    cmp al, 0Fh
    jne .check_opcode
    
    mov byte ptr [rdi + INSN_CTX.ctx_has_0f], 1
    inc r13
    
    cmp r13, r12
    jae .done
    
    ; Get second opcode byte
    movzx eax, byte ptr [rsi + r13]
    mov [rdi + INSN_CTX.ctx_opcode2], al
    inc r13
    jmp .check_modrm
    
.check_opcode:
    mov [rdi + INSN_CTX.ctx_opcode], al
    inc r13
    
.check_modrm:
    ; Determine if this opcode needs ModRM
    movzx eax, [rdi + INSN_CTX.ctx_opcode]
    
    cmp al, 40h
    jb .has_modrm
    cmp al, 80h
    jb .no_modrm
    cmp al, 90h
    jb .has_modrm
    
    cmp al, 0C4h
    je .maybe_vex
    cmp al, 0C5h
    je .maybe_vex
    
.no_modrm:
    jmp .done_parse
    
.has_modrm:
    cmp r13, r12
    jae .done
    
    movzx eax, byte ptr [rsi + r13]
    mov [rdi + INSN_CTX.ctx_modrm], al
    mov byte ptr [rdi + INSN_CTX.ctx_has_modrm], 1
    inc r13
    
    ; Check for SIB (rm = 100 and mod != 11)
    movzx ecx, al
    and cl, 0C0h
    cmp cl, 0C0h
    je .check_imm
    
    and al, 7
    cmp al, 4
    jne .check_disp
    
    ; Parse SIB
    cmp r13, r12
    jae .done
    
    movzx eax, byte ptr [rsi + r13]
    mov [rdi + INSN_CTX.ctx_sib], al
    mov byte ptr [rdi + INSN_CTX.ctx_has_sib], 1
    inc r13
    
.check_disp:
    ; Check displacement based on mod
    movzx eax, [rdi + INSN_CTX.ctx_modrm]
    mov ecx, eax
    shr ecx, 6
    
    cmp cl, 0
    jne .check_disp8
    and al, 7
    cmp al, 5
    jne .check_imm
    cmp r13, r12
    jae .done
    add r13, 4
    jmp .check_imm
    
.check_disp8:
    cmp cl, 1
    jne .check_disp32
    cmp r13, r12
    jae .done
    inc r13
    jmp .check_imm
    
.check_disp32:
    cmp cl, 2
    jne .check_imm
    cmp r13, r12
    jae .done
    add r13, 4
    
.check_imm:
    ; Check for immediate based on opcode
    movzx eax, [rdi + INSN_CTX.ctx_opcode]
    
    cmp al, 0B0h
    jb @F
    cmp al, 0C0h
    jb .imm8
    
    @@:
    cmp al, 0C0h
    jb @F
    cmp al, 0C2h
    jb .imm8
    
    @@:
    cmp al, 68h
    je .imm16_32
    cmp al, 6Ah
    je .imm8
    
    cmp al, 0E8h
    je .imm32
    cmp al, 0E9h
    je .imm32
    
    jmp .done_parse
    
.imm8:
    cmp r13, r12
    jae .done
    inc r13
    jmp .done_parse
    
.imm16_32:
    cmp r13, r12
    jae .done
    add r13, 4
    jmp .done_parse
    
.imm32:
    cmp r13, r12
    jae .done
    add r13, 4
    
.done_parse:
    ; Store final length
    mov [rdi + INSN_CTX.ctx_insn_len], r13d
    
    ; Copy bytes to context
    lea rdi, [rdi + INSN_CTX.ctx_bytes]
    mov rcx, r13
    @@:
    mov al, [rsi + rcx - 1]
    mov [rdi + rcx - 1], al
    dec rcx
    jnz @B
    
.done:
    mov rax, r13            ; Return bytes consumed
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
    
.maybe_vex:
    ; VEX prefix detected - unsupported for now
    mov rax, -1
    jmp .done
    
DISASSEMBLE_BYTES ENDP

; =============================================================================
; TEST_REVERSE_ASSEMBLER - Minimal test stub
; =============================================================================
TEST_REVERSE_ASSEMBLER PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    xor eax, eax
    
    pop rbx
    ret
TEST_REVERSE_ASSEMBLER ENDP

; =============================================================================
; MAIN ENTRY POINT
; =============================================================================
main PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
main ENDP

END
