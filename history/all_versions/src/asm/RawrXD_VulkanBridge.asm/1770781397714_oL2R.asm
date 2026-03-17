; =============================================================================
; RawrXD_VulkanBridge.asm — MASM64 Bridge to VulkanCompute C++ Engine
; Provides assembly-callable wrappers for VulkanKernel C exports.
; Allows inference_core.asm and other ASM kernels to dispatch GPU work.
;
; Build: Included in CMakeLists.txt ASM_KERNEL_SOURCES
; Pattern: RAX = 1 success, 0 failure (matches C exports)
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; =============================================================================
; C-callable Vulkan exports (defined in vulkan_compute.cpp)
; =============================================================================
EXTERNDEF VulkanKernel_Init:PROC
EXTERNDEF VulkanKernel_LoadShader:PROC
EXTERNDEF VulkanKernel_CreatePipeline:PROC
EXTERNDEF VulkanKernel_AllocBuffer:PROC
EXTERNDEF VulkanKernel_CopyToDevice:PROC
EXTERNDEF VulkanKernel_CopyToHost:PROC
EXTERNDEF VulkanKernel_DispatchMatMul:PROC
EXTERNDEF VulkanKernel_DispatchFlashAttn:PROC
EXTERNDEF VulkanKernel_HotswapShader:PROC
EXTERNDEF VulkanKernel_GetStats:PROC
EXTERNDEF VulkanKernel_Cleanup:PROC

; =============================================================================
; .data — Constants and string literals
; =============================================================================
.data
    szVkInit         db "[VulkanBridge] Initializing GPU compute...",13,10,0
    szVkInitOk       db "[VulkanBridge] GPU ready.",13,10,0
    szVkInitFail     db "[VulkanBridge] GPU init failed — CPU fallback.",13,10,0
    szVkCleanup      db "[VulkanBridge] GPU resources released.",13,10,0

    ; Shader paths (relative to binary directory)
    szMatMulShader   db "shaders\matmul.spv",0
    szFlashAttnShader db "shaders\flash_attn_v2.spv",0
    szFusedMLPShader db "shaders\fused_mlp.spv",0

    ; State
    align 8
    g_VkReady        dq 0      ; 1 = Vulkan initialized, 0 = not

; =============================================================================
; .code
; =============================================================================
.code

; =============================================================================
; VkBridge_Init — Initialize Vulkan compute subsystem
; Returns: RAX = 1 success, 0 failure
; Clobbers: all volatile regs
; =============================================================================
PUBLIC VkBridge_Init
VkBridge_Init PROC
    sub  rsp, 40

    ; Call C++ VulkanKernel_Init
    call VulkanKernel_Init
    test eax, eax
    jz   vbi_fail

    ; Mark as ready
    mov  qword ptr [g_VkReady], 1
    mov  eax, 1
    jmp  vbi_exit

vbi_fail:
    mov  qword ptr [g_VkReady], 0
    xor  eax, eax

vbi_exit:
    add  rsp, 40
    ret
VkBridge_Init ENDP

; =============================================================================
; VkBridge_IsReady — Check if Vulkan is initialized
; Returns: RAX = 1 ready, 0 not ready
; =============================================================================
PUBLIC VkBridge_IsReady
VkBridge_IsReady PROC
    mov  rax, qword ptr [g_VkReady]
    ret
VkBridge_IsReady ENDP

; =============================================================================
; VkBridge_LoadMatMulShader — Load the default MatMul SPIR-V shader
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_LoadMatMulShader
VkBridge_LoadMatMulShader PROC
    sub  rsp, 40

    cmp  qword ptr [g_VkReady], 0
    je   vlm_fail

    ; VulkanKernel_LoadShader("matmul", "shaders\\matmul.spv")
    lea  rcx, szMatMulLabel
    lea  rdx, szMatMulShader
    call VulkanKernel_LoadShader
    test eax, eax
    jz   vlm_fail

    ; VulkanKernel_CreatePipeline("matmul")
    lea  rcx, szMatMulLabel
    call VulkanKernel_CreatePipeline
    test eax, eax
    jz   vlm_fail

    mov  eax, 1
    jmp  vlm_exit

vlm_fail:
    xor  eax, eax

vlm_exit:
    add  rsp, 40
    ret

szMatMulLabel db "matmul",0

VkBridge_LoadMatMulShader ENDP

; =============================================================================
; VkBridge_DispatchMatMul — GPU matrix multiply
; RCX = buffer index A
; RDX = buffer index B
; R8  = buffer index Output
; R9  = M (rows of A)
; [RSP+40] = K (cols of A / rows of B)
; [RSP+48] = N (cols of B)
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_DispatchMatMul
VkBridge_DispatchMatMul PROC
    ; Stack layout: shadow space (32) + 2 extra args at [rsp+40] and [rsp+48]
    ; Forward directly to C export — same calling convention
    cmp  qword ptr [g_VkReady], 0
    je   vdm_fail

    jmp  VulkanKernel_DispatchMatMul  ; Tail call — same ABI

vdm_fail:
    xor  eax, eax
    ret
VkBridge_DispatchMatMul ENDP

; =============================================================================
; VkBridge_AllocBuffer — Allocate a GPU buffer
; RCX = size in bytes (QWORD)
; RDX = ptr to DWORD receiving buffer index
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_AllocBuffer
VkBridge_AllocBuffer PROC
    cmp  qword ptr [g_VkReady], 0
    je   vab_fail

    jmp  VulkanKernel_AllocBuffer     ; Tail call

vab_fail:
    xor  eax, eax
    ret
VkBridge_AllocBuffer ENDP

; =============================================================================
; VkBridge_CopyToDevice — Upload host data to GPU buffer
; RCX = buffer index (DWORD)
; RDX = ptr to host data
; R8  = size in bytes (QWORD)
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_CopyToDevice
VkBridge_CopyToDevice PROC
    cmp  qword ptr [g_VkReady], 0
    je   vcd_fail

    jmp  VulkanKernel_CopyToDevice

vcd_fail:
    xor  eax, eax
    ret
VkBridge_CopyToDevice ENDP

; =============================================================================
; VkBridge_CopyToHost — Download GPU buffer to host memory
; RCX = buffer index (DWORD)
; RDX = ptr to host buffer
; R8  = size in bytes (QWORD)
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_CopyToHost
VkBridge_CopyToHost PROC
    cmp  qword ptr [g_VkReady], 0
    je   vch_fail

    jmp  VulkanKernel_CopyToHost

vch_fail:
    xor  eax, eax
    ret
VkBridge_CopyToHost ENDP

; =============================================================================
; VkBridge_DispatchFlashAttn — GPU Flash Attention v2
; RCX = Q buffer index
; RDX = K buffer index
; R8  = V buffer index
; R9  = Output buffer index
; [RSP+40] = seq_len
; [RSP+48] = head_dim
; [RSP+56] = num_heads
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_DispatchFlashAttn
VkBridge_DispatchFlashAttn PROC
    cmp  qword ptr [g_VkReady], 0
    je   vfa_fail

    jmp  VulkanKernel_DispatchFlashAttn

vfa_fail:
    xor  eax, eax
    ret
VkBridge_DispatchFlashAttn ENDP

; =============================================================================
; VkBridge_HotswapShader — Live-replace a GPU shader
; RCX = shader name (ptr to null-terminated string)
; RDX = ptr to SPIR-V uint32_t array
; R8  = size in bytes of SPIR-V data
; Returns: RAX = 1 success, 0 failure
; =============================================================================
PUBLIC VkBridge_HotswapShader
VkBridge_HotswapShader PROC
    cmp  qword ptr [g_VkReady], 0
    je   vhs_fail

    jmp  VulkanKernel_HotswapShader

vhs_fail:
    xor  eax, eax
    ret
VkBridge_HotswapShader ENDP

; =============================================================================
; VkBridge_GetStats — Retrieve GPU performance counters
; RCX = ptr to QWORD receiving dispatch count
; RDX = ptr to QWORD receiving matmul count
; R8  = ptr to QWORD receiving attention count
; R9  = ptr to QWORD receiving error count
; =============================================================================
PUBLIC VkBridge_GetStats
VkBridge_GetStats PROC
    cmp  qword ptr [g_VkReady], 0
    je   vgs_zero

    jmp  VulkanKernel_GetStats

vgs_zero:
    ; Zero all outputs if Vulkan not ready
    test rcx, rcx
    jz   vgs_s1
    mov  qword ptr [rcx], 0
vgs_s1:
    test rdx, rdx
    jz   vgs_s2
    mov  qword ptr [rdx], 0
vgs_s2:
    test r8, r8
    jz   vgs_s3
    mov  qword ptr [r8], 0
vgs_s3:
    test r9, r9
    jz   vgs_done
    mov  qword ptr [r9], 0
vgs_done:
    ret
VkBridge_GetStats ENDP

; =============================================================================
; VkBridge_Cleanup — Shutdown Vulkan and release all GPU resources
; =============================================================================
PUBLIC VkBridge_Cleanup
VkBridge_Cleanup PROC
    sub  rsp, 40

    cmp  qword ptr [g_VkReady], 0
    je   vcl_done

    call VulkanKernel_Cleanup
    mov  qword ptr [g_VkReady], 0

vcl_done:
    add  rsp, 40
    ret
VkBridge_Cleanup ENDP

END
