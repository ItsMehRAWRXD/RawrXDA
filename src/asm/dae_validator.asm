; ============================================================================
; dae_validator.asm — DAE MASM64 State Transition Validator
; ============================================================================
; Provides a fast-path validation gate called before each Intent IR node
; is applied to the ShadowFilesystem.  The C++ fallback reference
; implementation in replay_engine.cpp must produce bit-identical results.
;
; Calling convention: Microsoft x64 ABI
;
; Exported functions:
;
;   DAE_ValidateTransition
;     RCX = ptr to TransitionContext (layout below)
;     Returns RAX = 0 on success, non-zero DAE ValidationError code on fail
;
;   DAE_HashChainStep
;     RCX = ptr to current 32-byte state hash (input)
;     RDX = ptr to 32-byte action digest  (input)
;     R8  = ptr to 32-byte output buffer  (output)
;     Mixes current hash with action digest via simple xor+rotate.
;     Production hash uses BCrypt in C++; this stub validates the ABI only.
;
; TransitionContext layout (matches dae_transition_context in replay_engine.h):
;   +0x00  QWORD  seq            ; monotonic sequence number
;   +0x08  BYTE[32] stateHash    ; 32-byte content hash of current state
;   +0x28  BYTE[32] actionDigest ; 32-byte digest of the operation
;   +0x48  DWORD  opType         ; OpType enum value
;   +0x4C  DWORD  reserved
; Total =  0x50 (80 bytes)
;
; NOTE: This is the P0 stub.  The full MASM hotpath (vectorised CRC lane
; comparisons, prefetch, VMOVNTDQ fence ordering) is a P1 deliverable.
; A runtime feature flag RAWRXD_DAE_ASM_HOTPATH controls which path executes.
; ============================================================================

OPTION DOTNAME

.CODE

; ============================================================================
; DAE_ValidateTransition
; ============================================================================
DAE_ValidateTransition PROC
    ; Save non-volatile registers
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32                 ; Shadow space

    ; RCX = TransitionContext*
    ; Basic null check
    test    rcx, rcx
    jz      .fail_null

    ; Load seq (offset 0)
    mov     rax, QWORD PTR [rcx + 0]
    test    rax, rax
    ; seq == 0 is valid only for the very first event; allow it

    ; Load opType (offset 0x48)
    mov     eax, DWORD PTR [rcx + 48]

    ; OpType range check: must be in [0, 10] or 0xFFFF (NoOp)
    cmp     eax, 10
    jbe     .valid_optype
    cmp     eax, 0FFFFh
    jne     .fail_optype

.valid_optype:
    ; Verify state hash is not all-zero (zero hash = uninitialised, seq > 0)
    ; For seq == 0 it is allowed — skip check
    mov     rax, QWORD PTR [rcx + 0]  ; seq
    test    rax, rax
    jz      .success                   ; seq == 0: first event, always valid

    ; Check first 8 bytes of stateHash (offset 0x08)
    mov     rax, QWORD PTR [rcx + 8]
    test    rax, rax
    jz      .fail_zero_hash

.success:
    xor     eax, eax                ; Return 0 = success
    jmp     .exit

.fail_null:
    mov     eax, 1                  ; NullContext
    jmp     .exit

.fail_optype:
    mov     eax, 2                  ; InvalidOpType
    jmp     .exit

.fail_zero_hash:
    mov     eax, 3                  ; UninitialisedStateHash

.exit:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
DAE_ValidateTransition ENDP

; ============================================================================
; DAE_HashChainStep
; ============================================================================
; RCX = ptr to current 32-byte state hash  (input, read-only)
; RDX = ptr to 32-byte action digest        (input, read-only)
; R8  = ptr to 32-byte output buffer        (output)
;
; Mixing: output[i] = ROL8( stateHash[i] XOR actionDigest[i] )
; This is the ABI stub; production uses BCryptHashData in C++.
; ============================================================================
DAE_HashChainStep PROC
    push    rbx
    sub     rsp, 32

    xor     ebx, ebx

.loop:
    cmp     ebx, 32
    jge     .done

    movzx   eax, BYTE PTR [rcx + rbx]   ; stateHash[i]
    xor     al, BYTE PTR [rdx + rbx]    ; XOR with actionDigest[i]
    rol     al, 1                        ; ROL by 1
    mov     BYTE PTR [r8 + rbx], al     ; write to output
    inc     ebx
    jmp     .loop

.done:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
DAE_HashChainStep ENDP

END
