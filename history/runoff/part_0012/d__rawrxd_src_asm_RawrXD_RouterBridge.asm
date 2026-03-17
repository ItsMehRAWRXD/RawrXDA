; =============================================================================
; RawrXD_RouterBridge.asm — Phase 30: MASM Fast-Path Accelerator Router Bridge
; =============================================================================
; Hot-loop inference fast path: bypasses C++ vtable dispatch for the
; router's most critical path (submit inference task → get result).
;
; This bridge provides:
;   1. Zero-overhead C calling convention entry points
;   2. Register-based parameter passing for hot-loop dispatch
;   3. Atomic backend selection cache (avoids mutex on every submit)
;   4. Fast backend type validation (lookup table, no branches)
;
; Build: ml64 /c /Fo RawrXD_RouterBridge.obj RawrXD_RouterBridge.asm
; Link:  Automatically linked via CMakeLists.txt MASM_OBJECTS
;
; Calling Convention: Microsoft x64 (RCX, RDX, R8, R9 + stack)
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
; External C functions (implemented in accelerator_router.cpp)
; =============================================================================
EXTERN AccelRouter_Create:PROC
EXTERN AccelRouter_Init:PROC
EXTERN AccelRouter_Shutdown:PROC
EXTERN AccelRouter_Submit:PROC
EXTERN AccelRouter_GetActiveBackend:PROC
EXTERN AccelRouter_ForceBackend:PROC
EXTERN AccelRouter_IsBackendAvailable:PROC
EXTERN AccelRouter_GetStatsJson:PROC

; =============================================================================
; Constants — Backend Type IDs (must match RouterBackendType enum)
; =============================================================================
BACKEND_NONE         EQU 0
BACKEND_AMD_XDNA     EQU 1
BACKEND_INTEL_XE     EQU 2
BACKEND_ARM64_ADRENO EQU 3
BACKEND_ARM64_NPU    EQU 4
BACKEND_CEREBRAS_WSE EQU 5
BACKEND_CPU_FALLBACK EQU 6
BACKEND_AUTO         EQU 255
BACKEND_MAX          EQU 7

; =============================================================================
; Data Section
; =============================================================================
.data

; Cached router handle (set on first Router_FastInit call)
g_routerHandle    DQ 0

; Cached active backend (updated atomically on each dispatch)
g_cachedBackend   DD BACKEND_NONE

; Backend validity lookup table (1 = valid, 0 = invalid)
; Index by backend type for O(1) validation
ALIGN 16
g_backendValid    DB 0    ; 0 = None (invalid for dispatch)
                  DB 1    ; 1 = AMD_XDNA
                  DB 1    ; 2 = Intel_Xe
                  DB 1    ; 3 = ARM64_Adreno
                  DB 1    ; 4 = ARM64_NPU
                  DB 1    ; 5 = Cerebras_WSE
                  DB 1    ; 6 = CPU_Fallback
                  DB 0    ; 7 = padding

; Stats: dispatch counter (64-bit atomic)
g_fastPathDispatches DQ 0

; =============================================================================
; Code Section
; =============================================================================
.code

; =============================================================================
; Router_FastInit — One-time initialization of the fast-path bridge
; =============================================================================
; Returns: RAX = router handle (void*), or 0 on failure
; Clobbers: RCX, RDX, R8, R9
; =============================================================================
Router_FastInit PROC
    ; Check if already initialized
    mov     rax, QWORD PTR [g_routerHandle]
    test    rax, rax
    jnz     @already_init

    ; Create router instance
    sub     rsp, 28h          ; Shadow space + alignment
    call    AccelRouter_Create
    mov     QWORD PTR [g_routerHandle], rax
    test    rax, rax
    jz      @create_failed

    ; Initialize the router
    mov     rcx, rax           ; handle
    call    AccelRouter_Init
    test    eax, eax
    jnz     @init_failed

    ; Cache the active backend
    mov     rcx, QWORD PTR [g_routerHandle]
    call    AccelRouter_GetActiveBackend
    mov     DWORD PTR [g_cachedBackend], eax

    mov     rax, QWORD PTR [g_routerHandle]
    add     rsp, 28h
    ret

@init_failed:
    ; Init failed — clear handle
    xor     rax, rax
    mov     QWORD PTR [g_routerHandle], rax
    add     rsp, 28h
    ret

@create_failed:
    xor     rax, rax
    add     rsp, 28h
    ret

@already_init:
    ; Already initialized — return cached handle
    ret
Router_FastInit ENDP

; =============================================================================
; Router_FastShutdown — Shutdown the router and clear cached state
; =============================================================================
; Returns: void
; =============================================================================
Router_FastShutdown PROC
    mov     rcx, QWORD PTR [g_routerHandle]
    test    rcx, rcx
    jz      @not_init

    sub     rsp, 28h
    call    AccelRouter_Shutdown
    add     rsp, 28h

    xor     rax, rax
    mov     QWORD PTR [g_routerHandle], rax
    mov     DWORD PTR [g_cachedBackend], BACKEND_NONE
    mov     QWORD PTR [g_fastPathDispatches], rax

@not_init:
    ret
Router_FastShutdown ENDP

; =============================================================================
; Router_FastSubmit — Hot-path inference dispatch
; =============================================================================
; RCX = pointer to RouterInferenceTask
; RDX = pointer to RouterResult (output)
; Returns: EAX = 0 on success, error code on failure
; =============================================================================
Router_FastSubmit PROC
    ; Validate inputs
    test    rcx, rcx
    jz      @null_task
    test    rdx, rdx
    jz      @null_result

    ; Save task and result pointers
    push    rbx
    push    rsi
    sub     rsp, 28h

    mov     rsi, rcx            ; RSI = task pointer
    mov     rbx, rdx            ; RBX = result pointer

    ; Check router handle
    mov     rcx, QWORD PTR [g_routerHandle]
    test    rcx, rcx
    jz      @not_initialized

    ; Dispatch: AccelRouter_Submit(handle, task, result)
    ; RCX = handle (already loaded)
    mov     rdx, rsi            ; task
    mov     r8, rbx             ; result
    call    AccelRouter_Submit

    ; Increment fast-path dispatch counter (atomic)
    lock inc QWORD PTR [g_fastPathDispatches]

    ; Update cached backend from result
    ; RouterResult.executedOn is at offset 16 (after success:1 + pad:3 + detail:8 + errorCode:4)
    ; Actually offset depends on struct layout — use the C bridge instead
    mov     rcx, QWORD PTR [g_routerHandle]
    push    rax                 ; save return code
    call    AccelRouter_GetActiveBackend
    mov     DWORD PTR [g_cachedBackend], eax
    pop     rax                 ; restore return code

    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret

@null_task:
    mov     eax, -1
    ret

@null_result:
    mov     eax, -2
    ret

@not_initialized:
    mov     eax, -3
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
Router_FastSubmit ENDP

; =============================================================================
; Router_FastGetBackend — Get cached active backend (no mutex, no call overhead)
; =============================================================================
; Returns: EAX = backend type (RouterBackendType)
; =============================================================================
Router_FastGetBackend PROC
    mov     eax, DWORD PTR [g_cachedBackend]
    ret
Router_FastGetBackend ENDP

; =============================================================================
; Router_FastForceBackend — Force a specific backend via fast path
; =============================================================================
; ECX = backend type (RouterBackendType)
; Returns: void
; =============================================================================
Router_FastForceBackend PROC
    ; Validate backend type
    cmp     ecx, BACKEND_MAX
    jae     @invalid_backend

    ; Check validity table
    lea     rax, [g_backendValid]
    movzx   edx, BYTE PTR [rax + rcx]
    test    edx, edx
    jz      @invalid_backend

    ; Update cache immediately
    mov     DWORD PTR [g_cachedBackend], ecx

    ; Call through to C implementation
    push    rcx
    sub     rsp, 28h
    mov     rcx, QWORD PTR [g_routerHandle]
    test    rcx, rcx
    jz      @no_handle

    pop     rdx                 ; backend type (was pushed RCX)
    push    rdx                 ; re-push for cleanup
    ; AccelRouter_ForceBackend(handle, backendType)
    ; RCX = handle (already set)
    ; RDX = backendType
    call    AccelRouter_ForceBackend

@no_handle:
    add     rsp, 28h
    pop     rcx
    ret

@invalid_backend:
    ret
Router_FastForceBackend ENDP

; =============================================================================
; Router_FastIsAvailable — O(1) backend availability check
; =============================================================================
; ECX = backend type
; Returns: EAX = 1 if available, 0 if not
; =============================================================================
Router_FastIsAvailable PROC
    cmp     ecx, BACKEND_MAX
    jae     @out_of_range

    ; Quick check via C bridge
    sub     rsp, 28h
    mov     rdx, rcx            ; backend type → RDX (second param)
    mov     rcx, QWORD PTR [g_routerHandle]
    test    rcx, rcx
    jz      @no_handle_avail

    call    AccelRouter_IsBackendAvailable
    add     rsp, 28h
    ret

@no_handle_avail:
    xor     eax, eax
    add     rsp, 28h
    ret

@out_of_range:
    xor     eax, eax
    ret
Router_FastIsAvailable ENDP

; =============================================================================
; Router_FastGetDispatchCount — Get total fast-path dispatch count
; =============================================================================
; Returns: RAX = 64-bit dispatch count
; =============================================================================
Router_FastGetDispatchCount PROC
    mov     rax, QWORD PTR [g_fastPathDispatches]
    ret
Router_FastGetDispatchCount ENDP

END
