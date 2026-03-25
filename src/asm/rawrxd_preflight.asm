; rawrxd_preflight.asm
; Stone MASM64 hardware preflight for RawrXD.
;
; Exported functions:
;   rawrxd_has_avx512     — CPUID leaf 7 + XGETBV OS-save probe
;   rawrxd_has_vulkan     — LoadLibraryA("vulkan-1.dll") probe
;   rawrxd_preflight_lock — VirtualLock 64 KB dry-run
;
; All functions follow the Microsoft x64 ABI (RCX/RDX/R8/R9 args,
; RAX return, 32-byte shadow space on the callee's frame, 16-byte RSP
; alignment before any CALL instruction).
;
; PROC FRAME + unwind directives are used throughout so the MSVC exception
; dispatcher can unwind through these frames reliably.

OPTION CASEMAP:NONE

; ---------------------------------------------------------------------------
; External Win32 imports — resolved by linker from kernel32.lib
; ---------------------------------------------------------------------------
EXTRN   LoadLibraryA:PROC
EXTRN   FreeLibrary:PROC
EXTRN   VirtualAlloc:PROC
EXTRN   VirtualLock:PROC
EXTRN   VirtualUnlock:PROC
EXTRN   VirtualFree:PROC

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
MEM_COMMIT      EQU 01000h
MEM_RESERVE     EQU 02000h
MEM_RELEASE     EQU 08000h
PAGE_READWRITE  EQU 4
PROBE_SIZE      EQU 010000h    ; 64 KB

; CPUID bit positions (used as immediate masks)
OSXSAVE_BIT     EQU 008000000h ; ECX bit 27 from leaf 1
XSAVE_BIT       EQU 004000000h ; ECX bit 26 from leaf 1
AVX512F_BIT     EQU 000010000h ; EBX bit 16 from leaf 7 sub 0
XCR0_ZMM_MASK   EQU 0E6h       ; bits 7:5 (ZMM hi) + bits 2:1 (YMM) = E0h|06h

; ---------------------------------------------------------------------------
; Read-only data
; ---------------------------------------------------------------------------
.const
    szVulkanICD DB "vulkan-1.dll", 0

.code

PUBLIC rawrxd_has_avx512
PUBLIC rawrxd_has_vulkan
PUBLIC rawrxd_preflight_lock

; ===========================================================================
; int rawrxd_has_avx512(void)
;
; Returns 1 if AVX-512F is present AND the OS has enabled ZMM save/restore
; (via XGETBV XCR0 check). Returns 0 otherwise.
;
; Register use:
;   CPUID clobbers RBX — saved/restored via PROC FRAME.
;   RAX/RCX/RDX are caller-save; we hold our own copy in EBX.
; ===========================================================================
rawrxd_has_avx512 PROC FRAME
    push    rbx
    .PUSHREG rbx
    .ENDPROLOG

    ; --- Check max supported CPUID leaf ---
    xor     eax, eax
    cpuid
    cmp     eax, 7
    jl      has_avx512_no           ; leaf 7 unavailable — AVX-512 not present

    ; --- CPUID leaf 1: verify OSXSAVE (bit 27) and XSAVE (bit 26) ---
    mov     eax, 1
    xor     ecx, ecx
    cpuid
    test    ecx, OSXSAVE_BIT
    jz      has_avx512_no
    test    ecx, XSAVE_BIT
    jz      has_avx512_no

    ; --- XGETBV(0): confirm OS saves both YMM (bits 2:1) and ZMM (bits 7:5) ---
    xor     ecx, ecx
    xgetbv                          ; EDX:EAX = XCR0; we need low 8 bits of EAX
    and     eax, XCR0_ZMM_MASK      ; keep only the state bits we care about
    cmp     eax, XCR0_ZMM_MASK
    jne     has_avx512_no

    ; --- CPUID leaf 7 sub 0: AVX-512F = EBX bit 16 ---
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    test    ebx, AVX512F_BIT
    jz      has_avx512_no

    ; Success path
    mov     eax, 1
    pop     rbx
    ret

has_avx512_no:
    xor     eax, eax
    pop     rbx
    ret

rawrxd_has_avx512 ENDP


; ===========================================================================
; int rawrxd_has_vulkan(void)
;
; Probes for Vulkan ICD availability by attempting LoadLibraryA("vulkan-1.dll").
; If the handle is non-NULL, FreeLibrary is called and 1 is returned.
; The probe loads and unloads the DLL; no Vulkan instance is created.
;
; Frame layout (in stack offset from RSP after sub rsp,32):
;   [rsp+0 .. rsp+31]  = shadow home space for callee
;   [rsp+32]           = saved rbx (via push rbx — below return address)
; ===========================================================================
rawrxd_has_vulkan PROC FRAME
    push    rbx
    .PUSHREG rbx
    sub     rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG

    ; LoadLibraryA("vulkan-1.dll")
    lea     rcx, szVulkanICD
    call    LoadLibraryA
    test    rax, rax
    jz      has_vulkan_no

    ; DLL found — release it; note result in EBX
    mov     rbx, rax
    mov     rcx, rbx
    call    FreeLibrary

    mov     eax, 1
    jmp     has_vulkan_done

has_vulkan_no:
    xor     eax, eax

has_vulkan_done:
    add     rsp, 32
    pop     rbx
    ret

rawrxd_has_vulkan ENDP


; ===========================================================================
; int rawrxd_preflight_lock(void)
;
; Allocates a 64 KB probe page with VirtualAlloc, attempts VirtualLock,
; then unconditionally frees the allocation. Returns 1 on lock success.
;
; This validates that the process working-set policy allows page pinning
; (required for latency-sensitive inference arena allocation).
;
; RBX holds the allocation base throughout.
; ===========================================================================
rawrxd_preflight_lock PROC FRAME
    push    rbx
    .PUSHREG rbx
    sub     rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG

    ; VirtualAlloc(NULL, PROBE_SIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)
    xor     rcx, rcx
    mov     rdx, PROBE_SIZE
    mov     r8d, MEM_RESERVE OR MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      lock_alloc_fail

    mov     rbx, rax                ; save allocation base

    ; VirtualLock(rbx, PROBE_SIZE)
    mov     rcx, rbx
    mov     rdx, PROBE_SIZE
    call    VirtualLock
    test    eax, eax
    jz      lock_lock_fail

    ; Lock succeeded — VirtualUnlock and free
    mov     rcx, rbx
    mov     rdx, PROBE_SIZE
    call    VirtualUnlock

    mov     rcx, rbx
    xor     rdx, rdx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

    mov     eax, 1
    jmp     lock_done

lock_lock_fail:
    ; Locking denied — still free the committed pages
    mov     rcx, rbx
    xor     rdx, rdx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

lock_alloc_fail:
    xor     eax, eax

lock_done:
    add     rsp, 32
    pop     rbx
    ret

rawrxd_preflight_lock ENDP

END
