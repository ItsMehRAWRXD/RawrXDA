; =============================================================================
; RawrXD_NLShell.asm - Phase 46: Intent Validation & Sandbox Guard
; Fingerprint-based command risk classification (4-byte LE token match).
;
; Exports:
;   NLShell_ValidateCommand(RCX=CmdPtr, RDX=CmdLen) -> RAX = risk score
;   NLShell_MatchTable(RCX=fp, RDX=table, R8=count) -> RAX = 1 if match
;
; Risk score constants:
;   RISK_READONLY=0  RISK_WRITE=1  RISK_DESTRUCTIVE=2  RISK_NETWORK=3
;   RISK_BLOCK=100  (requires quorum before execution)

RISK_READONLY    EQU 0
RISK_WRITE       EQU 1
RISK_DESTRUCTIVE EQU 2
RISK_NETWORK     EQU 3
RISK_BLOCK       EQU 100

.DATA

; Forbidden command fingerprints (first 4 bytes, lowercase LE)
; "rm -" "del " "rd /", "rmdi", "mkfs", "dd i", "fdis", "form", "> /d"
ForbiddenFP DWORD 2D206D72h, 206C6564h, 2F206472h, 69646D72h, \
                  73666B6Dh, 69206464h, 73696466h, 6D726F66h, 642F203Eh
ForbiddenCount EQU 9

; Network command fingerprints
; "curl" "wget" "Invo" "fetc"
NetworkFP DWORD 6C727563h, 74656777h, 6F766E69h, 63746566h
NetworkCount EQU 4

; Write command fingerprints
; "touc" "mkdi" "echo" "New-" "copy" "move"
WriteFP DWORD 63756F74h, 69646B6Dh, 6F686365h, 2D77656Eh, 79706F63h, 65766F6Dh
WriteCount EQU 6

.CODE

; ============================================================================
; NLShell_MatchTable
; RCX = fingerprint DWORD to find
; RDX = pointer to DWORD table
; R8  = entry count
; RAX = 1 if found, 0 otherwise
; ============================================================================
NLShell_MatchTable PROC
    test r8, r8
    jz   MT_Miss
    xor  r9, r9
MT_Loop:
    cmp  ecx, dword ptr [rdx + r9*4]
    je   MT_Hit
    inc  r9
    cmp  r9, r8
    jl   MT_Loop
MT_Miss:
    xor  rax, rax
    ret
MT_Hit:
    mov  rax, 1
    ret
NLShell_MatchTable ENDP

; ============================================================================
; NLShell_ValidateCommand
; RCX = pointer to ASCII/UTF-8 command string
; RDX = length (unused; string is NUL-terminated)
; RAX = 0 (readonly) | 1 (write) | 2 (destructive) | 3 (network) | 100 (block)
; ============================================================================
NLShell_ValidateCommand PROC FRAME
    .PUSHREG rbx
    .PUSHREG rsi
    push rbx
    push rsi
    .ENDPROLOG

    test rcx, rcx
    jz   VC_Safe

    mov  rsi, rcx

    ; --- Skip leading whitespace ---
VC_SkipWS:
    movzx eax, byte ptr [rsi]
    cmp   al, 20h
    je    VC_SkipWS_Next
    cmp   al, 09h
    je    VC_SkipWS_Next
    jmp   VC_BuildFP
VC_SkipWS_Next:
    inc   rsi
    jmp   VC_SkipWS

    ; --- Build 4-byte little-endian fingerprint (case-folded to lower) ---
VC_BuildFP:
    xor ebx, ebx

    ; Byte 0
    movzx eax, byte ptr [rsi]
    test  al, al
    jz    VC_Check
    cmp al, 41h
    jl @F
    cmp al, 5Ah
    jg @F
    or al, 20h
@@: mov bl, al

    ; Byte 1
    movzx eax, byte ptr [rsi+1]
    test  al, al
    jz    VC_Check
    cmp al, 41h
    jl @F
    cmp al, 5Ah
    jg @F
    or al, 20h
@@: shl eax, 8
    or  ebx, eax

    ; Byte 2
    movzx eax, byte ptr [rsi+2]
    test  al, al
    jz    VC_Check
    cmp al, 41h
    jl @F
    cmp al, 5Ah
    jg @F
    or al, 20h
@@: shl eax, 16
    or  ebx, eax

    ; Byte 3
    movzx eax, byte ptr [rsi+3]
    test  al, al
    jz    VC_Check
    cmp al, 41h
    jl @F
    cmp al, 5Ah
    jg @F
    or al, 20h
@@: shl eax, 24
    or  ebx, eax

VC_Check:
    ; --- Forbidden? -> RISK_BLOCK ---
    mov  ecx, ebx
    lea  rdx, [ForbiddenFP]
    mov  r8,  ForbiddenCount
    call NLShell_MatchTable
    test rax, rax
    jnz  VC_Block

    ; --- Network? -> RISK_NETWORK ---
    mov  ecx, ebx
    lea  rdx, [NetworkFP]
    mov  r8,  NetworkCount
    call NLShell_MatchTable
    test rax, rax
    jnz  VC_Network

    ; --- Write? -> RISK_WRITE ---
    mov  ecx, ebx
    lea  rdx, [WriteFP]
    mov  r8,  WriteCount
    call NLShell_MatchTable
    test rax, rax
    jnz  VC_Write

VC_Safe:
    mov  rax, RISK_READONLY
    jmp  VC_Done
VC_Write:
    mov  rax, RISK_WRITE
    jmp  VC_Done
VC_Network:
    mov  rax, RISK_NETWORK
    jmp  VC_Done
VC_Block:
    mov  rax, RISK_BLOCK
VC_Done:
    pop  rsi
    pop  rbx
    ret
NLShell_ValidateCommand ENDP

END
