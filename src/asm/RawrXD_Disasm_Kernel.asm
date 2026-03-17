; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Disasm_Kernel.asm - High-Speed Linear Sweep Disassembler Kernel
; ═══════════════════════════════════════════════════════════════════════════════
; Part of RawrXD Win32IDE v14.7.x - Zero Bloat
; Optimized for x86_64 fetch/decode skeleton
; ═══════════════════════════════════════════════════════════════════════════════

include RawrXD_Defs.inc

; --- Structure Definitions ---
RawrXD_Insn STRUCT
    Address     QWORD ?         ; Virtual Address
    RawBytes    BYTE 15 DUP (?) ; Max instruction length for x86/x64
    InsnLength  BYTE ?          ; Actual length
    Mnemonic    BYTE 32 DUP (?) ; Instruction string (e.g., "mov rax, rbx")
    Opcode      BYTE ?          ; Primary opcode byte
    Prefix      BYTE ?          ; Legacy prefix if any
    REX         BYTE ?          ; REX prefix if any
    Flags       DWORD ?         ; Internal analysis flags
RawrXD_Insn ENDS

.code

; ------------------------------------------------------------------------------
; RawrXD_FetchInsn
; RCX = Virtual Address to fetch from
; RDX = Pointer to RawrXD_Insn struct to fill
; R8  = Max buffer size (protection)
; Returns: RAX = Bytes consumed (InsnLength), or 0 on failure
; ------------------------------------------------------------------------------
RawrXD_FetchInsn PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx            ; RSI = Source VA
    mov rdi, rdx            ; RDI = Target Struct
    mov rax, 0              ; Initialize length

    ; Zero out the mnemonic and bytes
    lea rbx, [rdi].RawrXD_Insn.Mnemonic
    mov qword ptr [rbx], 0
    mov qword ptr [rbx+8], 0
    mov qword ptr [rbx+16], 0
    mov qword ptr [rbx+24], 0
    
    ; Store Address
    mov [rdi].RawrXD_Insn.Address, rcx

    ; Linear sweep logic: 
    ; 1. Check for prefixes (REX, 0x66, 0x67, 0xF0, 0xF2, 0xF3)
    ; 2. Fetch Opcode
    ; 3. Determine length based on opcode table (simple version for now)
    
    ; Note: This kernel is a "fetcher". Complex decoding is bridged to 
    ; the DisasmBridge.cpp using a light-weight lookup.
    
    mov al, byte ptr [rsi]
    mov [rdi].RawrXD_Insn.Opcode, al
    mov [rdi].RawrXD_Insn.RawBytes[0], al
    
    ; Basic length heuristic (Placeholder - to be refined by bridge or table)
    ; In a real linear sweep, we'd use a 256-byte table.
    mov byte ptr [rdi].RawrXD_Insn.InsnLength, 1
    mov rax, 1

    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_FetchInsn ENDP

END
