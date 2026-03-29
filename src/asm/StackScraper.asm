; =============================================================================
; StackScraper.asm — x64 MASM live stack / frame telemetry (Vector 1 primitive)
; =============================================================================
; Fixes vs naive version:
;   - RIP via call/next-instruction pop (not lea rax,[rip], which is invalid here)
;   - pOut preserved in R11 (volatile) until after RIP + stack peek + RSP snapshot
;   - RAX field: not recoverable for the true caller without CONTEXT; stored as 0
;   - StackPeek: 64 bytes at RSP immediately after RIP capture (caller's stack top)
;   - WalkRbpReturnAddresses: MSVC-style frame chain [rbp]->prev rbp, ret at [rbp+8]
;
; C++ should prefer RtlCaptureContext / GetThreadContext for full truth; this
; module is the fast path and RBP walk without CRT or DAP serialization.
; =============================================================================

INCLUDE RawrXD_Common.inc

; RawrxdCpuContextLite layout (must match Ship/StackScraper.hpp)
OFF_RIP         EQU 0
OFF_RSP         EQU 8
OFF_RBP         EQU 16
OFF_RAX         EQU 24
OFF_RBX         EQU 32
OFF_RCX         EQU 40
OFF_RDX         EQU 48
OFF_RSI         EQU 56
OFF_RDI         EQU 64
OFF_R8          EQU 72
OFF_R9          EQU 80
OFF_R10         EQU 88
OFF_R11         EQU 96
OFF_R12         EQU 104
OFF_R13         EQU 112
OFF_R14         EQU 120
OFF_R15         EQU 128
OFF_STACK_PEEK  EQU 136
CTX_LITE_SIZE   EQU 200

.code

; void __cdecl CaptureCpuContextLite(RawrxdCpuContextLite* pOut);
; RCX = pOut
PUBLIC CaptureCpuContextLite
CaptureCpuContextLite PROC
    mov     r11, rcx                    ; pOut

    call    capture_rip_point
capture_rip_point:
    pop     rax
    mov     [r11 + OFF_RIP], rax        ; RIP = capture_rip_point
    mov     [r11 + OFF_RSP], rsp        ; RSP after pop (aligned to caller stack)
    mov     [r11 + OFF_RBP], rbp

    ; Top 64 bytes at current RSP (includes return into caller of CaptureCpuContextLite)
    mov     rax, [rsp]
    mov     [r11 + OFF_STACK_PEEK], rax
    mov     rax, [rsp + 8]
    mov     [r11 + OFF_STACK_PEEK + 8], rax
    mov     rax, [rsp + 16]
    mov     [r11 + OFF_STACK_PEEK + 16], rax
    mov     rax, [rsp + 24]
    mov     [r11 + OFF_STACK_PEEK + 24], rax
    mov     rax, [rsp + 32]
    mov     [r11 + OFF_STACK_PEEK + 32], rax
    mov     rax, [rsp + 40]
    mov     [r11 + OFF_STACK_PEEK + 40], rax
    mov     rax, [rsp + 48]
    mov     [r11 + OFF_STACK_PEEK + 48], rax
    mov     rax, [rsp + 56]
    mov     [r11 + OFF_STACK_PEEK + 56], rax

    mov     qword ptr [r11 + OFF_RAX], 0 ; caller RAX not observable here

    mov     [r11 + OFF_RBX], rbx
    mov     [r11 + OFF_RCX], rcx        ; still original pOut from entry
    mov     [r11 + OFF_RDX], rdx
    mov     [r11 + OFF_RSI], rsi
    mov     [r11 + OFF_RDI], rdi
    mov     [r11 + OFF_R8], r8
    mov     [r11 + OFF_R9], r9          ; clobbered by peek base — acceptable snapshot
    mov     [r11 + OFF_R10], r10
    mov     [r11 + OFF_R11], r11        ; pOut
    mov     [r11 + OFF_R12], r12
    mov     [r11 + OFF_R13], r13
    mov     [r11 + OFF_R14], r14
    mov     [r11 + OFF_R15], r15

    ret
CaptureCpuContextLite ENDP

; size_t __cdecl WalkRbpReturnAddresses(uint64_t* outRips, uint32_t maxFrames, uint64_t rbpSeed);
; RCX = outRips, EDX = maxFrames, R8 = rbpSeed (0 => use RBP)
; Returns RAX = number of return addresses written.
PUBLIC WalkRbpReturnAddresses
WalkRbpReturnAddresses PROC
    mov     r10, rcx                    ; out
    mov     r11d, edx                   ; max
    xor     r15, r15                    ; count

    mov     rax, r8
    test    rax, rax
    jnz     walk_have_rbp
    mov     rax, rbp
walk_have_rbp:
    test    rax, rax
    jz      walk_done

walk_loop:
    cmp     r15d, r11d
    jae     walk_done
    mov     rcx, [rax + 8]              ; saved return IP
    mov     [r10 + r15 * 8], rcx
    inc     r15
    mov     rax, [rax]                  ; next frame's RBP
    test    rax, rax
    jz      walk_done
    jmp     walk_loop

walk_done:
    mov     rax, r15
    ret
WalkRbpReturnAddresses ENDP

END
