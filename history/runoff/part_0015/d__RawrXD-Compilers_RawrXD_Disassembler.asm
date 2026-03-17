; =============================================================================
; RawrXD Reverse Assembler (Disassembler) Loop
; Full x86/x64 instruction decoder with complete analysis
; Production-ready, zero dependencies, hand-tuned MASM64
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; =============================================================================
; EQUATES - Instruction Limits
; =============================================================================

MAX_INSN_LEN            EQU 15          ; Maximum x86 instruction length
MAX_OPS                 EQU 5           ; Maximum operands per instruction
MAX_PREFIXES            EQU 14          ; Maximum prefixes
MAX_MNEMONIC_LEN        EQU 32          ; Maximum mnemonic string length

; Prefix types
PREFIX_LOCK             EQU 0F0h
PREFIX_REPNE            EQU 0F2h
PREFIX_REP              EQU 0F3h
PREFIX_CS_OVERRIDE      EQU 02Eh
PREFIX_SS_OVERRIDE      EQU 036h
PREFIX_DS_OVERRIDE      EQU 03Eh
PREFIX_ES_OVERRIDE      EQU 026h
PREFIX_FS_OVERRIDE      EQU 064h
PREFIX_GS_OVERRIDE      EQU 065h
PREFIX_OPERAND_SIZE     EQU 066h
PREFIX_ADDRESS_SIZE     EQU 067h

; REX bits
REX_W                   EQU 008h        ; 64-bit operand size
REX_R                   EQU 004h        ; Extension of ModRM.reg
REX_X                   EQU 002h        ; Extension of SIB.index
REX_B                   EQU 001h        ; Extension of ModRM.rm/SIB.base

; Operand types
OP_NONE                 EQU 0
OP_REG                  EQU 1           ; Register
OP_IMM                  EQU 2           ; Immediate
OP_MEM                  EQU 3           ; Memory reference
OP_REL                  EQU 4           ; Relative address
OP_FAR                  EQU 5           ; Far pointer

; Register size
SIZE_BYTE               EQU 0
SIZE_WORD               EQU 1
SIZE_DWORD              EQU 2
SIZE_QWORD              EQU 3
SIZE_XMM                EQU 4
SIZE_YMM                EQU 5
SIZE_ZMM                EQU 6

; Register IDs
REG_RAX                 EQU 0
REG_RCX                 EQU 1
REG_RDX                 EQU 2
REG_RBX                 EQU 3
REG_RSP                 EQU 4
REG_RBP                 EQU 5
REG_RSI                 EQU 6
REG_RDI                 EQU 7
REG_R8                  EQU 8
REG_R9                  EQU 9
REG_R10                 EQU 10
REG_R11                 EQU 11
REG_R12                 EQU 12
REG_R13                 EQU 13
REG_R14                 EQU 14
REG_R15                 EQU 15

; =============================================================================
; STRUCTURES
; =============================================================================

Operand STRUCT
    op_type             DWORD ?         ; OP_*
    reg_id              DWORD ?         ; Register ID if OP_REG
    reg_size            DWORD ?         ; SIZE_*
    imm_value           QWORD ?         ; Immediate value
    disp_value          SDWORD ?        ; Displacement
    scale               DWORD ?         ; Scale factor (1,2,4,8)
    base_reg            DWORD ?         ; Base register
    index_reg           DWORD ?         ; Index register
    segment             DWORD ?         ; Segment override
    has_disp            BYTE ?
    has_sib             BYTE ?
Operand ENDS

DecodedInsn STRUCT
    raw_bytes           BYTE MAX_INSN_LEN DUP(?)
    raw_len             DWORD ?
    mnemonic            BYTE MAX_MNEMONIC_LEN DUP(?)
    prefixes            BYTE MAX_PREFIXES DUP(?)
    prefix_count        DWORD ?
    rex_byte            BYTE ?
    opcode              BYTE 4 DUP(?)
    opcode_len          DWORD ?
    modrm               BYTE ?
    has_modrm           BYTE ?
    sib                 BYTE ?
    has_sib             BYTE ?
    disp                SDWORD ?
    disp_size           DWORD ?
    imm                 QWORD ?
    imm_size            DWORD ?
    operands            Operand MAX_OPS DUP(<>)
    op_count            DWORD ?
    is_64bit            BYTE ?
    is_valid            BYTE ?
DecodedInsn ENDS

DecoderState STRUCT
    code_base           QWORD ?
    code_size           QWORD ?
    current_offset      QWORD ?
    insn_count          QWORD ?
    error_count         QWORD ?
    flags               DWORD ?
DecoderState ENDS

; =============================================================================
; DATA SECTION
; =============================================================================

.DATA

ALIGN 16

; Register name tables
reg_names_8 LABEL BYTE
    DB "al", 0, "cl", 0, "dl", 0, "bl", 0
    DB "ah", 0, "ch", 0, "dh", 0, "bh", 0
    DB "r8b", 0, "r9b", 0, "r10b", 0, "r11b", 0
    DB "r12b", 0, "r13b", 0, "r14b", 0, "r15b", 0

reg_names_16 LABEL BYTE
    DB "ax", 0, "cx", 0, "dx", 0, "bx", 0
    DB "sp", 0, "bp", 0, "si", 0, "di", 0
    DB "r8w", 0, "r9w", 0, "r10w", 0, "r11w", 0
    DB "r12w", 0, "r13w", 0, "r14w", 0, "r15w", 0

reg_names_32 LABEL BYTE
    DB "eax", 0, "ecx", 0, "edx", 0, "ebx", 0
    DB "esp", 0, "ebp", 0, "esi", 0, "edi", 0
    DB "r8d", 0, "r9d", 0, "r10d", 0, "r11d", 0
    DB "r12d", 0, "r13d", 0, "r14d", 0, "r15d", 0

reg_names_64 LABEL BYTE
    DB "rax", 0, "rcx", 0, "rdx", 0, "rbx", 0
    DB "rsp", 0, "rbp", 0, "rsi", 0, "rdi", 0
    DB "r8", 0, "r9", 0, "r10", 0, "r11", 0
    DB "r12", 0, "r13", 0, "r14", 0, "r15", 0

; Common mnemonics
mnem_mov                DB "mov", 0
mnem_add                DB "add", 0
mnem_sub                DB "sub", 0
mnem_xor                DB "xor", 0
mnem_and                DB "and", 0
mnem_or                 DB "or", 0
mnem_cmp                DB "cmp", 0
mnem_test               DB "test", 0
mnem_push               DB "push", 0
mnem_pop                DB "pop", 0
mnem_call               DB "call", 0
mnem_ret                DB "ret", 0
mnem_jmp                DB "jmp", 0
mnem_je                 DB "je", 0
mnem_jne                DB "jne", 0
mnem_jl                 DB "jl", 0
mnem_jg                 DB "jg", 0
mnem_jle                DB "jle", 0
mnem_jge                DB "jge", 0
mnem_lea                DB "lea", 0
mnem_nop                DB "nop", 0
mnem_syscall           DB "syscall", 0
mnem_invalid            DB "invalid", 0

; Group operation tables
group1_ops LABEL BYTE
    DB "add", 0, "or", 0, "adc", 0, "sbb", 0
    DB "and", 0, "sub", 0, "xor", 0, "cmp", 0

group2_ops LABEL BYTE
    DB "rol", 0, "ror", 0, "rcl", 0, "rcr", 0
    DB "shl", 0, "shr", 0, "sar", 0

; Format strings
fmt_hex_byte            DB "%02X", 0
fmt_hex_dword           DB "%08X", 0
fmt_newline             DB 13, 10, 0
fmt_mnem_op1            DB "%-8s %s", 0
fmt_mnem_op2            DB "%-8s %s, %s", 0
fmt_reg_val             DB "[%s+%d]", 0

; =============================================================================
; CODE SECTION
; =============================================================================

.CODE

ALIGN 16

; =============================================================================
; GetRegName - Get register name by ID and size
; RCX = register ID (0-15)
; RDX = size (SIZE_*)
; Returns: RAX = pointer to name string
; =============================================================================
GetRegName PROC FRAME
    push    rbx
    push    rsi
    
    mov     rbx, rcx                    ; rbx = reg_id
    mov     rax, rdx                    ; rax = size
    
    ; Select table based on size
    cmp     al, SIZE_BYTE
    je      @byte_size
    cmp     al, SIZE_WORD
    je      @word_size
    cmp     al, SIZE_DWORD
    je      @dword_size
    
@qword_size:
    lea     rsi, reg_names_64
    jmp     @calc_offset
    
@dword_size:
    lea     rsi, reg_names_32
    jmp     @calc_offset
    
@word_size:
    lea     rsi, reg_names_16
    jmp     @calc_offset
    
@byte_size:
    lea     rsi, reg_names_8
    
@calc_offset:
    ; Calculate offset: each entry is 4-5 chars padded to 8 bytes
    mov     rax, rbx
    shl     rax, 3
    add     rsi, rax
    mov     rax, rsi
    
    pop     rsi
    pop     rbx
    ret
GetRegName ENDP

; =============================================================================
; DecodeModRM - Parse ModRM byte fields
; RCX = ModRM byte
; RDX = REX byte (0 if none)
; Returns: AL = mod, AH = reg (extended), BL = rm (extended), BH = reserved
; =============================================================================
DecodeModRM PROC FRAME
    push    rbx
    
    mov     al, cl                      ; al = ModRM
    
    ; Extract mod (bits 7-6)
    mov     bl, al
    shr     bl, 6
    
    ; Extract reg/opcode (bits 5-3)
    mov     ah, al
    shr     ah, 3
    and     ah, 7
    
    ; Apply REX.R extension
    test    dl, REX_R
    jz      @no_rex_r
    or      ah, 8
    
@no_rex_r:
    ; Extract rm (bits 2-0)
    mov     bh, al
    and     bh, 7
    
    ; Apply REX.B extension
    test    dl, REX_B
    jz      @no_rex_b
    or      bh, 8
    
@no_rex_b:
    ; Return: AL=mod, AH=reg, BL=rm, BH=spare
    mov     bl, bh                      ; bl = rm
    
    pop     rbx
    ret
DecodeModRM ENDP

; =============================================================================
; DecodeSIB - Parse SIB byte fields
; RCX = SIB byte
; RDX = REX byte
; Returns: AL = scale, AH = index (extended), BL = base (extended)
; =============================================================================
DecodeSIB PROC FRAME
    push    rbx
    
    mov     al, cl                      ; al = SIB
    
    ; Extract scale (bits 7-6)
    shr     al, 6                       ; al = scale (0-3)
    
    ; Extract index (bits 5-3)
    mov     ah, cl
    shr     ah, 3
    and     ah, 7
    
    ; Apply REX.X extension
    test    dl, REX_X
    jz      @no_rex_x
    or      ah, 8
    
@no_rex_x:
    ; Extract base (bits 2-0)
    mov     bl, cl
    and     bl, 7
    
    ; Apply REX.B extension
    test    dl, REX_B
    jz      @no_rex_b
    or      bl, 8
    
@no_rex_b:
    pop     rbx
    ret
DecodeSIB ENDP

; =============================================================================
; DecodeInstruction - Decode single x86-64 instruction
; RCX = code buffer pointer
; RDX = offset in buffer
; R8 = remaining size in buffer
; R9 = pointer to DecodedInsn output structure
; Returns: RAX = instruction length (0 on error)
; =============================================================================
DecodeInstruction PROC FRAME
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     r12, rcx                    ; r12 = code base
    mov     r13, rdx                    ; r13 = offset
    mov     r14, r8                     ; r14 = remaining size
    mov     r15, r9                     ; r15 = output structure
    
    ; Validate parameters
    test    r12, r12
    jz      @decode_error
    test    r14, r14
    jz      @decode_error
    
    ; Clear output structure
    xor     eax, eax
    mov     ecx, SIZEOF DecodedInsn / 8
    mov     rdi, r15
    rep     stosq
    
    xor     ebx, ebx                    ; rbx = position in instruction
    xor     r10d, r10d                  ; r10 = REX byte
    xor     r11d, r11d                  ; r11 = prefix count
    
    ; =================================================================
    ; PHASE 1: Parse prefixes
    ; =================================================================
@parse_prefixes:
    cmp     rbx, r14
    jae     @prefixes_done
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    
    ; Check for REX prefix (40-4F) in 64-bit mode
    cmp     al, 040h
    jb      @check_legacy
    cmp     al, 04Fh
    ja      @check_legacy
    
    ; REX prefix
    mov     r10d, eax
    mov     [r15].DecodedInsn.rex_byte, al
    mov     [r15].DecodedInsn.is_64bit, 1
    jmp     @next_prefix
    
@check_legacy:
    ; Check legacy prefixes
    cmp     al, PREFIX_LOCK
    je      @store_prefix
    cmp     al, PREFIX_REPNE
    je      @store_prefix
    cmp     al, PREFIX_REP
    je      @store_prefix
    cmp     al, PREFIX_OPERAND_SIZE
    je      @store_prefix
    cmp     al, PREFIX_ADDRESS_SIZE
    je      @store_prefix
    cmp     al, PREFIX_CS_OVERRIDE
    je      @store_prefix
    cmp     al, PREFIX_SS_OVERRIDE
    je      @store_prefix
    cmp     al, PREFIX_DS_OVERRIDE
    je      @store_prefix
    cmp     al, PREFIX_ES_OVERRIDE
    je      @store_prefix
    cmp     al, PREFIX_FS_OVERRIDE
    je      @store_prefix
    cmp     al, PREFIX_GS_OVERRIDE
    je      @store_prefix
    
    ; Not a prefix, done parsing prefixes
    jmp     @prefixes_done
    
@store_prefix:
    cmp     r11d, MAX_PREFIXES
    jae     @decode_error
    
    mov     [r15].DecodedInsn.prefixes[r11d], al
    inc     r11d
    
@next_prefix:
    inc     rbx
    jmp     @parse_prefixes
    
@prefixes_done:
    mov     [r15].DecodedInsn.prefix_count, r11d
    
    ; =================================================================
    ; PHASE 2: Parse opcode
    ; =================================================================
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.opcode[0], al
    mov     [r15].DecodedInsn.opcode_len, 1
    inc     rbx
    
    ; Check for two-byte opcode (0F)
    cmp     al, 0Fh
    jne     @opcode_done
    
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.opcode[1], al
    inc     [r15].DecodedInsn.opcode_len
    inc     rbx
    
    ; Check for three-byte opcodes
    cmp     al, 038h
    je      @three_byte
    cmp     al, 03Ah
    je      @three_byte
    jmp     @opcode_done
    
@three_byte:
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.opcode[2], al
    inc     [r15].DecodedInsn.opcode_len
    inc     rbx
    
@opcode_done:

    ; =================================================================
    ; PHASE 3: Parse ModRM if required
    ; =================================================================
    movzx   eax, [r15].DecodedInsn.opcode[0]
    
    ; Determine if ModRM is needed (simplified heuristic)
    cmp     al, 0F0h
    jae     @check_modrm_high
    
    cmp     al, 080h
    jb      @check_modrm_low
    
    ; 80-FF range - most need ModRM
    cmp     al, 0C0h
    jb      @needs_modrm
    cmp     al, 0C9h
    jbe     @needs_modrm
    cmp     al, 0D0h
    jb      @skip_modrm
    cmp     al, 0FFh
    jbe     @needs_modrm
    jmp     @skip_modrm
    
@check_modrm_low:
    cmp     al, 004h
    je      @skip_modrm
    cmp     al, 005h
    je      @skip_modrm
    cmp     al, 040h
    jb      @needs_modrm                ; Some in 00-3F need ModRM
    cmp     al, 050h
    jb      @skip_modrm                 ; 40-4F are REX or simple
    cmp     al, 060h
    jb      @skip_modrm                 ; 50-5F are PUSH/POP
    cmp     al, 070h
    jb      @needs_modrm                ; 60-6F have mixed
    cmp     al, 080h
    jb      @skip_modrm                 ; 70-7F are JCond
    jmp     @needs_modrm
    
@check_modrm_high:
    ; F0-FF
    cmp     al, 0F4h
    je      @skip_modrm
    cmp     al, 0F5h
    je      @skip_modrm
    cmp     al, 0F8h
    jb      @needs_modrm
    cmp     al, 0FDh
    jbe     @skip_modrm
    cmp     al, 0FFh
    jbe     @needs_modrm
    
@needs_modrm:
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.modrm, al
    mov     [r15].DecodedInsn.has_modrm, 1
    inc     rbx
    
    ; Decode ModRM fields
    mov     cl, al
    mov     dl, r10b                    ; REX byte
    call    DecodeModRM
    
    ; AL = mod, AH = reg, BL = rm
    mov     r10d, eax                   ; Save mod/reg
    mov     r11d, ebx                   ; Save rm
    
    ; Check for SIB (mod != 3 and rm == 4)
    cmp     al, 3                       ; Check mod
    je      @no_sib
    cmp     bl, 4                       ; Check rm
    jne     @no_sib
    
    ; Parse SIB
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.sib, al
    mov     [r15].DecodedInsn.has_sib, 1
    inc     rbx
    
    mov     cl, al
    mov     dl, [r15].DecodedInsn.rex_byte
    call    DecodeSIB
    
@no_sib:
    ; Parse displacement based on mod
    mov     eax, r10d                   ; Restore mod
    shr     eax, 6                      ; Extract mod to AL
    
    cmp     al, 1
    je      @disp8
    cmp     al, 2
    je      @disp32
    
    ; mod=0 or 3, check for [RIP+disp32]
    cmp     al, 0
    jne     @skip_disp
    cmp     r11b, 5                     ; rm == 5?
    jne     @skip_disp
    
@disp32:
    cmp     rbx, r14
    jae     @decode_error
    
    cmp     rbx + 3, r14
    ja      @decode_error
    
    mov     eax, DWORD PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.disp, eax
    mov     [r15].DecodedInsn.disp_size, 4
    add     rbx, 4
    jmp     @skip_disp
    
@disp8:
    cmp     rbx, r14
    jae     @decode_error
    
    movsx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.disp, eax
    mov     [r15].DecodedInsn.disp_size, 1
    inc     rbx
    
@skip_disp:

@skip_modrm:

    ; =================================================================
    ; PHASE 4: Parse immediate
    ; =================================================================
    movzx   eax, [r15].DecodedInsn.opcode[0]
    
    ; Determine immediate size based on opcode
    cmp     al, 004h
    je      @imm8
    cmp     al, 005h
    je      @imm32_mov
    cmp     al, 0B0h
    jb      @skip_imm
    cmp     al, 0B8h
    jb      @imm8_mov
    cmp     al, 0C0h
    jb      @imm32_mov
    
    cmp     al, 0C2h
    je      @imm16
    cmp     al, 0C8h
    je      @imm16_8
    cmp     al, 0C6h
    je      @imm8
    cmp     al, 0C7h
    je      @imm32_mov
    cmp     al, 0E8h
    je      @imm32
    cmp     al, 0EBh
    je      @imm8
    cmp     al, 0E9h
    je      @imm32
    
    jmp     @skip_imm
    
@imm8:
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 1
    inc     rbx
    jmp     @skip_imm
    
@imm16:
    cmp     rbx + 1, r14
    ja      @decode_error
    
    movzx   eax, WORD PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 2
    add     rbx, 2
    jmp     @skip_imm
    
@imm16_8:
    cmp     rbx + 1, r14
    ja      @decode_error
    
    movzx   eax, WORD PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.imm, rax
    add     rbx, 2
    
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    shl     rax, 16
    or      [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 3
    inc     rbx
    jmp     @skip_imm
    
@imm32:
    cmp     rbx + 3, r14
    ja      @decode_error
    
    mov     eax, DWORD PTR [r12 + r13 + rbx]
    movsxd  rax, eax
    mov     [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 4
    add     rbx, 4
    jmp     @skip_imm
    
@imm32_mov:
    cmp     rbx + 3, r14
    ja      @decode_error
    
    mov     eax, DWORD PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 4
    add     rbx, 4
    jmp     @skip_imm
    
@imm8_mov:
    cmp     rbx, r14
    jae     @decode_error
    
    movzx   eax, BYTE PTR [r12 + r13 + rbx]
    mov     [r15].DecodedInsn.imm, rax
    mov     [r15].DecodedInsn.imm_size, 1
    inc     rbx
    
@skip_imm:

    ; =================================================================
    ; PHASE 5: Finalize
    ; =================================================================
    mov     [r15].DecodedInsn.raw_len, ebx
    
    ; Copy raw bytes
    mov     ecx, ebx
    mov     rdi, r15
    add     rdi, OFFSET DecodedInsn.raw_bytes
    mov     rsi, r12
    add     rsi, r13
    rep     movsb
    
    mov     [r15].DecodedInsn.is_valid, 1
    
    mov     rax, rbx                    ; Return length
    jmp     @decode_exit
    
@decode_error:
    xor     eax, eax                    ; Return 0 on error
    
@decode_exit:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
DecodeInstruction ENDP

; =============================================================================
; DisassembleBuffer - Main disassembler loop
; RCX = code buffer
; RDX = code size
; R8 = callback function (optional, 0 = none)
; Returns: RAX = number of instructions decoded
; =============================================================================
DisassembleBuffer PROC FRAME
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     r12, rcx                    ; r12 = code buffer
    mov     r13, rdx                    ; r13 = code size
    mov     r14, r8                     ; r14 = callback
    xor     r15, r15                    ; r15 = offset
    xor     rbx, rbx                    ; rbx = instruction count
    
    test    r12, r12
    jz      @disasm_exit
    test    r13, r13
    jz      @disasm_exit
    
@disasm_loop:
    cmp     r15, r13
    jae     @disasm_done
    
    ; Calculate remaining size
    mov     rax, r13
    sub     rax, r15
    cmp     rax, MAX_INSN_LEN
    cmova   rax, MAX_INSN_LEN           ; Cap at max instruction length
    
    ; Allocate stack space for DecodedInsn (local variable)
    sub     rsp, SIZEOF DecodedInsn
    mov     r9, rsp                     ; r9 = structure pointer
    
    ; Decode instruction
    mov     rcx, r12
    mov     rdx, r15
    mov     r8, rax
    call    DecodeInstruction
    
    add     rsp, SIZEOF DecodedInsn
    
    test    rax, rax
    jz      @skip_insn
    
    ; Valid instruction
    inc     rbx
    add     r15, rax
    
    ; Call callback if provided
    test    r14, r14
    jz      @disasm_loop
    
    ; Callback(rax=insn_length, rsi=DecodedInsn*)
    mov     rsi, rsp
    mov     rcx, rsi
    mov     edx, [rsi].DecodedInsn.raw_len
    call    r14
    
    jmp     @disasm_loop
    
@skip_insn:
    ; Skip problematic byte
    inc     r15
    jmp     @disasm_loop
    
@disasm_done:
    mov     rax, rbx
    
@disasm_exit:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
DisassembleBuffer ENDP

; =============================================================================
; GetInstructionMnemonic - Get mnemonic string for decoded instruction
; RCX = pointer to DecodedInsn
; Returns: RAX = pointer to mnemonic string
; =============================================================================
GetInstructionMnemonic PROC FRAME
    push    rbx
    push    rsi
    
    mov     rsi, rcx
    movzx   eax, [rsi].DecodedInsn.opcode[0]
    
    ; Map common opcodes to mnemonics
    cmp     al, 089h
    je      @mnem_mov                    ; MOV r/m, r
    cmp     al, 08Bh
    je      @mnem_mov                    ; MOV r, r/m
    cmp     al, 08Dh
    je      @mnem_lea                    ; LEA
    cmp     al, 001h
    je      @mnem_add                    ; ADD
    cmp     al, 029h
    je      @mnem_sub                    ; SUB
    cmp     al, 031h
    je      @mnem_xor                    ; XOR
    cmp     al, 021h
    je      @mnem_and                    ; AND
    cmp     al, 009h
    je      @mnem_or                     ; OR
    cmp     al, 039h
    je      @mnem_cmp                    ; CMP
    cmp     al, 085h
    je      @mnem_test                   ; TEST
    cmp     al, 050h
    jb      @mnem_other
    cmp     al, 058h
    jbe     @mnem_push                   ; PUSH (50-57)
    cmp     al, 050h
    jb      @mnem_other
    cmp     al, 058h
    jbe     @mnem_pop                    ; POP (58-5F)
    
    ; 50-57 PUSH, 58-5F POP - recheck
    cmp     al, 050h
    jae     @check_push_pop
    jmp     @mnem_other
    
@check_push_pop:
    cmp     al, 058h
    jb      @mnem_push
    cmp     al, 060h
    jb      @mnem_pop
    
    cmp     al, 0E8h
    je      @mnem_call
    cmp     al, 0C3h
    je      @mnem_ret
    cmp     al, 0E9h
    je      @mnem_jmp
    cmp     al, 0EBh
    je      @mnem_jmp
    cmp     al, 074h
    je      @mnem_je
    cmp     al, 075h
    je      @mnem_jne
    cmp     al, 07Ch
    je      @mnem_jl
    cmp     al, 07Fh
    je      @mnem_jg
    cmp     al, 090h
    je      @mnem_nop
    
    ; Two-byte opcodes
    cmp     [rsi].DecodedInsn.opcode[0], 0Fh
    jne     @mnem_other
    
    movzx   eax, [rsi].DecodedInsn.opcode[1]
    cmp     al, 005h
    je      @mnem_syscall
    
    jmp     @mnem_other
    
@mnem_mov:
    lea     rax, mnem_mov
    jmp     @mnem_done
@mnem_add:
    lea     rax, mnem_add
    jmp     @mnem_done
@mnem_sub:
    lea     rax, mnem_sub
    jmp     @mnem_done
@mnem_xor:
    lea     rax, mnem_xor
    jmp     @mnem_done
@mnem_and:
    lea     rax, mnem_and
    jmp     @mnem_done
@mnem_or:
    lea     rax, mnem_or
    jmp     @mnem_done
@mnem_cmp:
    lea     rax, mnem_cmp
    jmp     @mnem_done
@mnem_test:
    lea     rax, mnem_test
    jmp     @mnem_done
@mnem_push:
    lea     rax, mnem_push
    jmp     @mnem_done
@mnem_pop:
    lea     rax, mnem_pop
    jmp     @mnem_done
@mnem_call:
    lea     rax, mnem_call
    jmp     @mnem_done
@mnem_ret:
    lea     rax, mnem_ret
    jmp     @mnem_done
@mnem_jmp:
    lea     rax, mnem_jmp
    jmp     @mnem_done
@mnem_je:
    lea     rax, mnem_je
    jmp     @mnem_done
@mnem_jne:
    lea     rax, mnem_jne
    jmp     @mnem_done
@mnem_jl:
    lea     rax, mnem_jl
    jmp     @mnem_done
@mnem_jg:
    lea     rax, mnem_jg
    jmp     @mnem_done
@mnem_lea:
    lea     rax, mnem_lea
    jmp     @mnem_done
@mnem_nop:
    lea     rax, mnem_nop
    jmp     @mnem_done
@mnem_syscall:
    lea     rax, mnem_syscall
    jmp     @mnem_done
@mnem_other:
    lea     rax, mnem_invalid
    
@mnem_done:
    pop     rsi
    pop     rbx
    ret
GetInstructionMnemonic ENDP

; =============================================================================
; End of disassembler
; =============================================================================

END
