; ============================================================================
; INFERENCEBACKEND_INIT.ASM - Inference Backend Detection & Initialization
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

PUBLIC InferenceBackend_Init
PUBLIC InferenceBackend_SelectBackend
PUBLIC InferenceBackend_CreateInferenceContext

; Backend types
BACKEND_CPU         EQU 0
BACKEND_VULKAN      EQU 1
BACKEND_CUDA        EQU 2
BACKEND_ROCM        EQU 3
BACKEND_METAL       EQU 4

.data
    g_dwSelectedBackend     dd BACKEND_CPU
    g_dwAvailableBackends   dd 0
    g_szBackendName         db "CPU", 0

.code

; ============================================================================
; InferenceBackend_Init - Detect available backends
; Output: EAX = number of available backends
; ============================================================================
InferenceBackend_Init PROC
    push ebx
    push esi
    
    ; CPU backend is always available
    mov [g_dwAvailableBackends], 1
    
    ; In a full build we would probe Vulkan/CUDA/ROCm/Metal; keep CPU-only here
    
    mov eax, [g_dwAvailableBackends]
    pop esi
    pop ebx
    ret
InferenceBackend_Init ENDP

; ============================================================================
; InferenceBackend_SelectBackend - Select active inference backend
; Input: ECX = backend type (BACKEND_CPU, BACKEND_VULKAN, etc.)
; Output: EAX = 1 if successful, 0 if unavailable
; ============================================================================
InferenceBackend_SelectBackend PROC dwType:DWORD
    push ebx
    
    ; Validate backend type (only CPU supported in this minimal build)
    mov eax, dwType
    cmp eax, BACKEND_CPU
    jne @fallback
    mov [g_dwSelectedBackend], eax
    mov byte ptr g_szBackendName, 'C'
    mov byte ptr g_szBackendName+1, 'P'
    mov byte ptr g_szBackendName+2, 'U'
    mov byte ptr g_szBackendName+3, 0
    mov eax, 1
    pop ebx
    ret
@fallback:
    mov [g_dwSelectedBackend], BACKEND_CPU
    mov byte ptr g_szBackendName, 'C'
    mov byte ptr g_szBackendName+1, 'P'
    mov byte ptr g_szBackendName+2, 'U'
    mov byte ptr g_szBackendName+3, 0
    mov eax, 1
    pop ebx
    ret
InferenceBackend_SelectBackend ENDP

; ============================================================================
; InferenceBackend_CreateInferenceContext - Create context for inference
; Input: ECX = model handle
; Output: EAX = context handle (or 0 if failed)
; ============================================================================
InferenceBackend_CreateInferenceContext PROC hModel:DWORD
    push ebx
    
    ; Minimal context handle: echo the model handle if nonzero else fail
    mov eax, hModel
    test eax, eax
    jz @fail
    ; Success, return model handle as context token
    mov eax, hModel
    pop ebx
    ret
@fail:
    xor eax, eax
    pop ebx
    ret
InferenceBackend_CreateInferenceContext ENDP

END
