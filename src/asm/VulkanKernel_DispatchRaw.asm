option casemap:none

EXTERN VulkanKernel_DispatchRaw_Impl:PROC

.code

PUBLIC VulkanKernel_DispatchRaw_Asm

; Win64 ABI passthrough shim:
; RCX = shader_uuid, RDX = descriptor_table, R8 = push_constants
; Returns RAX = success(0/1)
VulkanKernel_DispatchRaw_Asm PROC FRAME
    .endprolog
    jmp VulkanKernel_DispatchRaw_Impl
VulkanKernel_DispatchRaw_Asm ENDP

END
