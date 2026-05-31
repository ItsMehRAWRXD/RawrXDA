; RawrXD Production Detour Engine (x64)
; Highly-optimized jump logic for hotpatching
; Author: GitHub Copilot

.CODE

; -----------------------------------------------------------------------------
; RawrXD_GenerateAbsoluteJump_MASM
; Input:  RCX = Address of buffer (14 bytes)
;         RDX = Target address
; Result: Buffer contains:
;         FF 25 00 00 00 00 [Target64] ; JMP [RIP+0]
; -----------------------------------------------------------------------------
RawrXD_GenerateAbsoluteJump_MASM PROC
    mov word ptr [rcx], 25FFh       ; opcode FF 25 (JMP [RIP+0])
    mov dword ptr [rcx+2], 0        ; relative offset to data: [RIP+6]
    mov [rcx+6], rdx                ; write 64-bit target address into buffer
    ret
RawrXD_GenerateAbsoluteJump_MASM ENDP

; -----------------------------------------------------------------------------
; RawrXD_GenerateRelativeJump_MASM
; Input:  RCX = From address
;         RDX = To address
;         R8  = Buffer address (5 bytes)
; Result: Buffer contains:
;         E9 [Rel32]
; -----------------------------------------------------------------------------
RawrXD_GenerateRelativeJump_MASM PROC
    mov rax, rdx
    sub rax, rcx
    sub rax, 5                      ; Offset from end of instruction (5 bytes)
    
    ; Check if within +/- 2GB (32-bit offset)
    mov r9, rax
    sar r9, 31
    inc r9
    cmp r9, 2
    ja  RelJump_Failed              ; Offset > 2GB or < -2GB
    
    mov byte ptr [r8], 0E9h         ; JMP rel32 opcode
    mov [r8+1], eax                 ; Store 32-bit relative offset
    mov rax, 1                      ; Success
    ret

RelJump_Failed:
    xor rax, rax                ; indicate failure to satisfy rel32 constraint
    ret
RawrXD_GenerateRelativeJump_MASM ENDP

; -----------------------------------------------------------------------------
; RawrXD_Sentinel_CalculateHash_MASM
; Input:  RCX = Address to hash
;         RDX = Size (in bytes)
; Result: RAX = Simple 64-bit checksum (for integrity checks)
; -----------------------------------------------------------------------------
RawrXD_Sentinel_CalculateHash_MASM PROC
    xor rax, rax
    test rdx, rdx
    jz Hash_End
    
Hash_Loop:
    movzx r8, byte ptr [rcx]
    add rax, r8
    rol rax, 7
    xor rax, r8
    inc rcx
    dec rdx
    jnz Hash_Loop

Hash_End:
    ret
RawrXD_Sentinel_CalculateHash_MASM ENDP

END
