; =============================================================================
; RawrXD Backend Selector — Production GPU Path
; Zero CPU fallbacks. Real backend probing.
; =============================================================================
        .code
        option casemap:none

; -----------------------------------------------------------------------------n; C++ mangled names for external linkage
; -----------------------------------------------------------------------------n        EXTERN VulkanInferenceEngine_Initialize:PROC
        EXTERN VulkanInferenceEngine_Shutdown:PROC
        EXTERN HIPInferenceEngine_Initialize:PROC
        EXTERN HIPInferenceEngine_Shutdown:PROC
        EXTERN CUDAInferenceEngine_Initialize:PROC
        EXTERN CUDAInferenceEngine_Shutdown:PROC
        EXTERN TitanInferenceEngine_Initialize:PROC
        EXTERN TitanInferenceEngine_Shutdown:PROC
        EXTERN VulkanResolveImports:PROC

; -----------------------------------------------------------------------------n; Backend capability flags
; -----------------------------------------------------------------------------nBACKEND_VULKAN          EQU 1
BACKEND_HIP             EQU 2
BACKEND_CUDA            EQU 4
BACKEND_TITAN           EQU 8
BACKEND_CPU             EQU 0x80000000              ; Last resort, never used in production

; -----------------------------------------------------------------------------n; Global state
; -----------------------------------------------------------------------------n        .data
ALIGN 64
g_activeBackend         DWORD   0
g_availableBackends     DWORD   0
g_backendHandles        QWORD   4 DUP (0)             ; 0=Vulkan, 1=HIP, 2=CUDA, 3=Titan

szHipDll                BYTE    "amdhip64.dll", 0
szCudaDll               BYTE    "nvcuda.dll", 0
szTitanDevice           BYTE    "\\\\.\\TitanDevice", 0

; -----------------------------------------------------------------------------n; SECTION .code
; -----------------------------------------------------------------------------n        .code

; =============================================================================
; BackendSelector_ProbeAll
;   Detects available GPU backends. Returns bitmask in EAX.
; =============================================================================
BackendSelector_ProbeAll PROC FRAME
        push    rbx
        .pushreg rbx
        push    rsi
        .pushreg rsi
        push    rdi
        .pushreg rdi
        sub     rsp, 40
        .allocstack 40
        .endprolog

        xor     esi, esi                                ; available mask

        ; === Probe Vulkan ===
        call    VulkanResolveImports
        test    rax, rax
        jnz     @@skip_vulkan
        or      esi, BACKEND_VULKAN
@@skip_vulkan:

        ; === Probe HIP ===
        ; Check for amdhip64.dll
        lea     rcx, [szHipDll]
        call    LoadLibraryA
        test    rax, rax
        jz      @@skip_hip
        mov     rdi, rax                                ; hHip
        or      esi, BACKEND_HIP
        ; Don't leak — we keep handle for runtime use
        mov     [g_backendHandles + 8], rax
@@skip_hip:

        ; === Probe CUDA ===
        lea     rcx, [szCudaDll]
        call    LoadLibraryA
        test    rax, rax
        jz      @@skip_cuda
        mov     [g_backendHandles + 16], rax
        or      esi, BACKEND_CUDA
@@skip_cuda:

        ; === Probe Titan ===
        lea     rcx, [szTitanDevice]
        mov     edx, 0xC0000000
        xor     r8d, r8d
        xor     r9d, r9d
        mov     QWORD PTR [rsp+32], 3
        mov     QWORD PTR [rsp+40], 0x80
        mov     QWORD PTR [rsp+48], 0
        call    CreateFileA
        cmp     rax, -1
        je      @@skip_titan
        ; Valid device found
        mov     [g_backendHandles + 24], rax
        or      esi, BACKEND_TITAN
        ; Don't close — we keep handle
@@skip_titan:

        mov     [g_availableBackends], esi
        mov     eax, esi

        add     rsp, 40
        pop     rdi
        pop     rsi
        pop     rbx
        ret
BackendSelector_ProbeAll ENDP

; =============================================================================
; BackendSelector_CreateBest
;   Creates the best available backend. Priority: Titan > Vulkan > HIP > CUDA
;   Returns: RAX = 0 (success), backend type in g_activeBackend
; =============================================================================
BackendSelector_CreateBest PROC FRAME
        push    rbx
        .pushreg rbx
        push    rsi
        .pushreg rsi
        .endprolog

        call    BackendSelector_ProbeAll
        mov     esi, eax

        ; Priority 1: Titan
        test    esi, BACKEND_TITAN
        jz      @@try_vulkan
        lea     rcx, [szTitanDevice]
        call    TitanInferenceEngine_Initialize
        test    rax, rax
        jnz     @@try_vulkan
        mov     DWORD PTR [g_activeBackend], BACKEND_TITAN
        xor     eax, eax
        pop     rsi
        pop     rbx
        ret

@@try_vulkan:
        ; Priority 2: Vulkan
        test    esi, BACKEND_VULKAN
        jz      @@try_hip
        call    VulkanInferenceEngine_Initialize
        test    rax, rax
        jnz     @@try_hip
        mov     DWORD PTR [g_activeBackend], BACKEND_VULKAN
        xor     eax, eax
        pop     rsi
        pop     rbx
        ret

@@try_hip:
        ; Priority 3: HIP
        test    esi, BACKEND_HIP
        jz      @@try_cuda
        call    HIPInferenceEngine_Initialize
        test    rax, rax
        jnz     @@try_cuda
        mov     DWORD PTR [g_activeBackend], BACKEND_HIP
        xor     eax, eax
        pop     rsi
        pop     rbx
        ret

@@try_cuda:
        ; Priority 4: CUDA
        test    esi, BACKEND_CUDA
        jz      @@fail
        call    CUDAInferenceEngine_Initialize
        test    rax, rax
        jnz     @@fail
        mov     DWORD PTR [g_activeBackend], BACKEND_CUDA
        xor     eax, eax
        pop     rsi
        pop     rbx
        ret

@@fail:
        ; No GPU backend available — return error, do NOT fall back to CPU
        mov     eax, 1
        pop     rsi
        pop     rbx
        ret
BackendSelector_CreateBest ENDP

; =============================================================================
; BackendSelector_Shutdown
;   Cleans up the active backend and releases all handles.
; =============================================================================
BackendSelector_Shutdown PROC FRAME
        push    rbx
        .pushreg rbx
        .endprolog

        mov     ebx, DWORD PTR [g_activeBackend]

        cmp     ebx, BACKEND_TITAN
        jne     @@check_vulkan
        call    TitanInferenceEngine_Shutdown
        jmp     @@cleanup_handles

@@check_vulkan:
        cmp     ebx, BACKEND_VULKAN
        jne     @@check_hip
        call    VulkanInferenceEngine_Shutdown
        jmp     @@cleanup_handles

@@check_hip:
        cmp     ebx, BACKEND_HIP
        jne     @@check_cuda
        call    HIPInferenceEngine_Shutdown
        jmp     @@cleanup_handles

@@check_cuda:
        cmp     ebx, BACKEND_CUDA
        jne     @@cleanup_handles
        call    CUDAInferenceEngine_Shutdown

@@cleanup_handles:
        ; Close all backend library handles
        mov     rcx, [g_backendHandles + 8]             ; HIP
        test    rcx, rcx
        jz      @@skip_close_hip
        call    FreeLibrary
@@skip_close_hip:

        mov     rcx, [g_backendHandles + 16]            ; CUDA
        test    rcx, rcx
        jz      @@skip_close_cuda
        call    FreeLibrary
@@skip_close_cuda:

        mov     rcx, [g_backendHandles + 24]            ; Titan
        cmp     rcx, -1
        je      @@skip_close_titan
        call    CloseHandle
@@skip_close_titan:

        mov     DWORD PTR [g_activeBackend], 0
        mov     DWORD PTR [g_availableBackends], 0
        xor     eax, eax
        pop     rbx
        ret
BackendSelector_Shutdown ENDP

; =============================================================================
; BackendSelector_GetActive
;   Returns the currently active backend type in EAX.
; =============================================================================
BackendSelector_GetActive PROC
        mov     eax, DWORD PTR [g_activeBackend]
        ret
BackendSelector_GetActive ENDP

; =============================================================================
; BackendSelector_GetAvailable
;   Returns the bitmask of available backends in EAX.
; =============================================================================
BackendSelector_GetAvailable PROC
        mov     eax, DWORD PTR [g_availableBackends]
        ret
BackendSelector_GetAvailable ENDP

; =============================================================================
; BackendSelector_IsBackendAvailable
;   RCX = backend flag to check
;   Returns: RAX = 1 if available, 0 if not
; =============================================================================
BackendSelector_IsBackendAvailable PROC
        mov     eax, DWORD PTR [g_availableBackends]
        and     eax, ecx
        setnz   al
        movzx   eax, al
        ret
BackendSelector_IsBackendAvailable ENDP

        END
