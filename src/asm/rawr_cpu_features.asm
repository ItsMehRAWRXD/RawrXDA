; ============================================================
; FILE: rawr_cpu_features.asm
; PURPOSE: Canonical runtime feature gates for AVX2 / AVX-512
;          on Win64.  Pure MASM64.  No CRT dependencies.
;
; EXPORTS:
;   rawr_cpu_has_avx512()  → EAX=1 if safe, else 0
;   rawr_cpu_has_avx2()    → EAX=1 if safe, else 0
;
; GATE LOGIC (AVX-512):
;   1. CPUID(1): OSXSAVE (ECX.27) + AVX (ECX.28) present
;   2. XGETBV(XCR0): XMM (bit 1) + YMM (bit 2) enabled by OS
;   3. CPUID(7,0): AVX-512F (EBX.16) present
;   4. XGETBV(XCR0): opmask (bit 5) + ZMM_hi256 (bit 6) +
;      hi16_ZMM (bit 7) enabled by OS
;
; GATE LOGIC (AVX2):
;   1. CPUID(1): OSXSAVE (ECX.27) + AVX (ECX.28) present
;   2. XGETBV(XCR0): XMM (bit 1) + YMM (bit 2) enabled by OS
;   3. CPUID(7,0): AVX2 (EBX.5) present
;
; NOTES:
;   - CPUID clobbers EAX/EBX/ECX/EDX.  RBX is non-volatile on
;     Win64 → must be saved/restored.
;   - XGETBV result in EDX:EAX is consumed before the next
;     CPUID so the XCR0 state is not clobbered.
;   - Zen 2/3: no AVX-512 → gate returns 0.
;   - Zen 4 (7800X3D etc.): gate returns 1 if OS has enabled
;     XSTATE for ZMM.
;
; Build: ml64.exe /c /Zi /Zd rawr_cpu_features.asm
; ============================================================
option casemap:none

; XCR0 bit masks
XCR0_XMM_YMM       EQU 06h         ; bits 1+2: SSE + AVX state
XCR0_AVX512_STATE   EQU 0E0h       ; bits 5+6+7: opmask + ZMM_hi256 + hi16_ZMM

.code

; =============================================================================
; UINT32 rawr_cpu_has_avx512(void)
;
; Returns EAX=1 if all four gates pass, else EAX=0.
; Safe to call at any point — no side effects, no globals written.
; =============================================================================
PUBLIC rawr_cpu_has_avx512
rawr_cpu_has_avx512 PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; ── Gate 1: CPUID(1) — require OSXSAVE + AVX ──────────────────────
    mov     eax, 1
    xor     ecx, ecx
    cpuid                           ; EAX/EBX/ECX/EDX clobbered

    ; ECX bit 27 = OSXSAVE  (OS enabled XSAVE and set CR4.OSXSAVE)
    bt      ecx, 27
    jnc     @@fail

    ; ECX bit 28 = AVX      (CPU advertises AVX)
    bt      ecx, 28
    jnc     @@fail

    ; ── Gate 2: XGETBV(XCR0) — XMM + YMM enabled by OS ───────────────
    xor     ecx, ecx                ; XCR0 selector
    xgetbv                          ; EDX:EAX = XCR0
    mov     r8d, eax                ; *** save XCR0 low dword ***

    and     eax, XCR0_XMM_YMM
    cmp     eax, XCR0_XMM_YMM
    jne     @@fail

    ; ── Gate 3: CPUID(7,0) — AVX-512F feature bit ─────────────────────
    mov     eax, 7
    xor     ecx, ecx
    cpuid                           ; clobbers EAX — but we saved XCR0 in R8D

    ; EBX bit 16 = AVX-512F
    bt      ebx, 16
    jnc     @@fail

    ; ── Gate 4: XCR0 bits 5/6/7 — OS enabled AVX-512 XSTATE ──────────
    ; Use the saved XCR0 value (R8D), NOT eax (which is now CPUID output)
    mov     ecx, r8d
    and     ecx, XCR0_AVX512_STATE
    cmp     ecx, XCR0_AVX512_STATE
    jne     @@fail

    mov     eax, 1
    pop     rbx
    ret

@@fail:
    xor     eax, eax
    pop     rbx
    ret
rawr_cpu_has_avx512 ENDP

; =============================================================================
; UINT32 rawr_cpu_has_avx2(void)
;
; Returns EAX=1 if OS + CPU support AVX2 (YMM-safe), else EAX=0.
; =============================================================================
PUBLIC rawr_cpu_has_avx2
rawr_cpu_has_avx2 PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; ── CPUID(1): OSXSAVE + AVX ──
    mov     eax, 1
    xor     ecx, ecx
    cpuid

    bt      ecx, 27                ; OSXSAVE
    jnc     @@fail2

    bt      ecx, 28                ; AVX
    jnc     @@fail2

    ; ── XGETBV(XCR0): XMM + YMM ──
    xor     ecx, ecx
    xgetbv

    and     eax, XCR0_XMM_YMM
    cmp     eax, XCR0_XMM_YMM
    jne     @@fail2

    ; ── CPUID(7,0): AVX2 ──
    mov     eax, 7
    xor     ecx, ecx
    cpuid

    bt      ebx, 5                 ; AVX2
    jnc     @@fail2

    mov     eax, 1
    pop     rbx
    ret

@@fail2:
    xor     eax, eax
    pop     rbx
    ret
rawr_cpu_has_avx2 ENDP

END
