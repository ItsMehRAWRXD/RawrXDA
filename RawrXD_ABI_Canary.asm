; ============================================================
; FILE: RawrXD_ABI_Canary.asm  (FIXED)
; PURPOSE: ABI preservation canary for Win64
; FIXES vs prior drop:
;  1) Preserves GPR-failure bitmask across helper calls
;  2) Does NOT use uninitialized stack scratch for masks
;  3) Keeps masks in regs (r11d = GPR, r10d = XMM) then merges
; ============================================================
option casemap:none

RAWR_SHADOWSPACE equ 20h

RAWR_PROLOGUE macro locals_bytes:req
    sub     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
endm

RAWR_EPILOGUE macro locals_bytes:req
    add     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
    ret
endm

RAWR_SAVE_NONVOL macro
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
endm

RAWR_RESTORE_NONVOL macro
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
endm

.data
xmm_s6  dq 0123456789ABCDEFh, 0FEDCBA987654321h
xmm_s7  dq 1111111111111111h, 2222222222222222h
xmm_s8  dq 3333333333333333h, 4444444444444444h
xmm_s9  dq 5555555555555555h, 6666666666666666h
xmm_s10 dq 7777777777777777h, 8888888888888888h
xmm_s11 dq 9999999999999999h, 0AAAAAAAAAAAAAAAh
xmm_s12 dq 0BBBBBBBBBBBBBBBh, 0CCCCCCCCCCCCCCCCh
xmm_s13 dq 0DDDDDDDDDDDDDDDDh, 0EEEEEEEEEEEEEEEEh
xmm_s14 dq 0F0F0F0F0F0F0F0F0h, 00F0F0F0F0F0F0F0Fh
xmm_s15 dq 13579BDF2468ACE0h, 0ECA8642FDB97531h

.code

; ------------------------------------------------------------
; UINT32 abi_canary_call0(void* fn)
; RCX = fn pointer
; Returns EAX bitmask of failures (0 = pass)
;
; Bit layout:
; 0 RBX  1 RBP  2 RSI  3 RDI  4 R12  5 R13  6 R14  7 R15
; 8 XMM6 ... 17 XMM15
; ------------------------------------------------------------
public abi_canary_call0
abi_canary_call0 proc
    ; locals:
    ;  - save original XMM6..XMM15: 10*16 = A0h
    ;  - post-call dump XMM6..XMM15: A0h
    ; total locals = 140h (space + alignment slack)
    RAWR_PROLOGUE 140h
    RAWR_SAVE_NONVOL

    ; Save original XMM6..XMM15
    movdqu  xmmword ptr [rsp+020h], xmm6
    movdqu  xmmword ptr [rsp+030h], xmm7
    movdqu  xmmword ptr [rsp+040h], xmm8
    movdqu  xmmword ptr [rsp+050h], xmm9
    movdqu  xmmword ptr [rsp+060h], xmm10
    movdqu  xmmword ptr [rsp+070h], xmm11
    movdqu  xmmword ptr [rsp+080h], xmm12
    movdqu  xmmword ptr [rsp+090h], xmm13
    movdqu  xmmword ptr [rsp+0A0h], xmm14
    movdqu  xmmword ptr [rsp+0B0h], xmm15

    ; failure masks
    xor     r11d, r11d     ; GPR failures
    xor     r10d, r10d     ; XMM failures

    ; Inject nonvolatile GPR sentinels
    mov     rbx, 0B0B0B0B0B0B0B0h
    mov     rbp, 0B1B1B1B1B1B1B1B1h
    mov     rsi, 0B2B2B2B2B2B2B2B2h
    mov     rdi, 0B3B3B3B3B3B3B3B3h
    mov     r12, 0B4B4B4B4B4B4B4B4h
    mov     r13, 0B5B5B5B5B5B5B5B5h
    mov     r14, 0B6B6B6B6B6B6B6B6h
    mov     r15, 0B7B7B7B7B7B7B7B7h

    ; Inject XMM sentinels
    movdqu  xmm6,  xmmword ptr [xmm_s6]
    movdqu  xmm7,  xmmword ptr [xmm_s7]
    movdqu  xmm8,  xmmword ptr [xmm_s8]
    movdqu  xmm9,  xmmword ptr [xmm_s9]
    movdqu  xmm10, xmmword ptr [xmm_s10]
    movdqu  xmm11, xmmword ptr [xmm_s11]
    movdqu  xmm12, xmmword ptr [xmm_s12]
    movdqu  xmm13, xmmword ptr [xmm_s13]
    movdqu  xmm14, xmmword ptr [xmm_s14]
    movdqu  xmm15, xmmword ptr [xmm_s15]

    ; Call target fn()
    mov     rax, rcx
    call    rax

    ; Check nonvolatile GPRs
    mov     r10, 0B0B0B0B0B0B0B0h
    cmp     rbx, r10
    je      @f
    or      r11d, 1 shl 0
@@:
    mov     r10, 0B1B1B1B1B1B1B1B1h
    cmp     rbp, r10
    je      @f
    or      r11d, 1 shl 1
@@:
    mov     r10, 0B2B2B2B2B2B2B2B2h
    cmp     rsi, r10
    je      @f
    or      r11d, 1 shl 2
@@:
    mov     r10, 0B3B3B3B3B3B3B3B3h
    cmp     rdi, r10
    je      @f
    or      r11d, 1 shl 3
@@:
    mov     r10, 0B4B4B4B4B4B4B4B4h
    cmp     r12, r10
    je      @f
    or      r11d, 1 shl 4
@@:
    mov     r10, 0B5B5B5B5B5B5B5B5h
    cmp     r13, r10
    je      @f
    or      r11d, 1 shl 5
@@:
    mov     r10, 0B6B6B6B6B6B6B6B6h
    cmp     r14, r10
    je      @f
    or      r11d, 1 shl 6
@@:
    mov     r10, 0B7B7B7B7B7B7B7B7h
    cmp     r15, r10
    je      @f
    or      r11d, 1 shl 7
@@:

    ; Dump XMM6..XMM15 post-call to stack
    movdqu  xmmword ptr [rsp+0C0h], xmm6
    movdqu  xmmword ptr [rsp+0D0h], xmm7
    movdqu  xmmword ptr [rsp+0E0h], xmm8
    movdqu  xmmword ptr [rsp+0F0h], xmm9
    movdqu  xmmword ptr [rsp+100h], xmm10
    movdqu  xmmword ptr [rsp+110h], xmm11
    movdqu  xmmword ptr [rsp+120h], xmm12
    movdqu  xmmword ptr [rsp+130h], xmm13
    movdqu  xmmword ptr [rsp+140h], xmm14
    movdqu  xmmword ptr [rsp+150h], xmm15

    ; Compare post-call dumps against sentinel blocks

    ; XMM6
    lea     rdx, [rsp+0C0h]
    lea     rcx, xmm_s6
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 8
@@:
    ; XMM7
    lea     rdx, [rsp+0D0h]
    lea     rcx, xmm_s7
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 9
@@:
    ; XMM8
    lea     rdx, [rsp+0E0h]
    lea     rcx, xmm_s8
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 10
@@:
    ; XMM9
    lea     rdx, [rsp+0F0h]
    lea     rcx, xmm_s9
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 11
@@:
    ; XMM10
    lea     rdx, [rsp+100h]
    lea     rcx, xmm_s10
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 12
@@:
    ; XMM11
    lea     rdx, [rsp+110h]
    lea     rcx, xmm_s11
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 13
@@:
    ; XMM12
    lea     rdx, [rsp+120h]
    lea     rcx, xmm_s12
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 14
@@:
    ; XMM13
    lea     rdx, [rsp+130h]
    lea     rcx, xmm_s13
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 15
@@:
    ; XMM14
    lea     rdx, [rsp+140h]
    lea     rcx, xmm_s14
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 16
@@:
    ; XMM15
    lea     rdx, [rsp+150h]
    lea     rcx, xmm_s15
    call    abi_cmp16
    test    eax, eax
    jz      @f
    or      r10d, 1 shl 17
@@:

    ; Restore original XMM6..XMM15
    movdqu  xmm6,  xmmword ptr [rsp+020h]
    movdqu  xmm7,  xmmword ptr [rsp+030h]
    movdqu  xmm8,  xmmword ptr [rsp+040h]
    movdqu  xmm9,  xmmword ptr [rsp+050h]
    movdqu  xmm10, xmmword ptr [rsp+060h]
    movdqu  xmm11, xmmword ptr [rsp+070h]
    movdqu  xmm12, xmmword ptr [rsp+080h]
    movdqu  xmm13, xmmword ptr [rsp+090h]
    movdqu  xmm14, xmmword ptr [rsp+0A0h]
    movdqu  xmm15, xmmword ptr [rsp+0B0h]

    ; Return merged failure mask
    mov     eax, r11d
    or      eax, r10d

    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 140h
abi_canary_call0 endp


; ------------------------------------------------------------
; UINT32 abi_cmp16(const void* b, const void* a)
; RCX = b (sentinel), RDX = a (dump)
; Returns EAX=0 if equal, 1 if different
; ------------------------------------------------------------
public abi_cmp16
abi_cmp16 proc
    mov     r8,  qword ptr [rdx]
    cmp     r8,  qword ptr [rcx]
    jne     _diff
    mov     r8,  qword ptr [rdx+8]
    cmp     r8,  qword ptr [rcx+8]
    jne     _diff
    xor     eax, eax
    ret
_diff:
    mov     eax, 1
    ret
abi_cmp16 endp

end
