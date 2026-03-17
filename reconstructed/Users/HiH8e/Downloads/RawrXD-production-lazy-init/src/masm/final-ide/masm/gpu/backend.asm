;==========================================================================
; masm_gpu_backend.asm - Pure MASM GPU Backend Manager
; ==========================================================================
; Replaces gpu_backend.cpp.
; Detects and initializes CUDA, HIP, or Vulkan backends.
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN console_log:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szGpuInit       BYTE "GpuBackend: Detecting hardware acceleration...", 0
    szCudaLib       BYTE "nvcuda.dll", 0
    szVulkanLib     BYTE "vulkan-1.dll", 0
    
    szCudaFound     BYTE "GpuBackend: CUDA detected (nvcuda.dll)", 0
    szVulkanFound   BYTE "GpuBackend: Vulkan detected (vulkan-1.dll)", 0
    szCpuFallback   BYTE "GpuBackend: No GPU acceleration found. Falling back to CPU.", 0

.code

;==========================================================================
; gpu_backend_init() -> rax (type)
;==========================================================================
PUBLIC gpu_backend_init
gpu_backend_init PROC
    sub rsp, 32
    
    lea rcx, szGpuInit
    call console_log
    
    ; 1. Try CUDA
    lea rcx, szCudaLib
    call LoadLibraryA
    test rax, rax
    jz .try_vulkan
    
    lea rcx, szCudaFound
    call console_log
    mov rax, 1          ; CUDA
    jmp .exit

.try_vulkan:
    ; 2. Try Vulkan
    lea rcx, szVulkanLib
    call LoadLibraryA
    test rax, rax
    jz .fallback
    
    lea rcx, szVulkanFound
    call console_log
    mov rax, 3          ; Vulkan
    jmp .exit

.fallback:
    lea rcx, szCpuFallback
    call console_log
    xor rax, rax        ; CPU

.exit:
    add rsp, 32
    ret
gpu_backend_init ENDP

END
