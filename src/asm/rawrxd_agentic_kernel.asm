; rawrxd_agentic_kernel.asm
; AVX-512 register-hot agent context kernel surface.
; Exports:
;   agent_context_load
;   agent_step_execute
;   agent_context_store
;
; Calling convention (Win64):
;   RCX = pointer to AgentRegisterHotState

OPTION CASEMAP:NONE

PUBLIC agent_context_load
PUBLIC agent_step_execute
PUBLIC agent_context_store

.code

ALIGN 16
agent_context_load PROC
    ; ZMM0-ZMM7 <- workingState (512 bytes)
    vmovdqu64 zmm0, zmmword ptr [rcx + 0]
    vmovdqu64 zmm1, zmmword ptr [rcx + 64]
    vmovdqu64 zmm2, zmmword ptr [rcx + 128]
    vmovdqu64 zmm3, zmmword ptr [rcx + 192]
    vmovdqu64 zmm4, zmmword ptr [rcx + 256]
    vmovdqu64 zmm5, zmmword ptr [rcx + 320]
    vmovdqu64 zmm6, zmmword ptr [rcx + 384]
    vmovdqu64 zmm7, zmmword ptr [rcx + 448]
    ret
agent_context_load ENDP

ALIGN 16
agent_step_execute PROC
    ; ZMM8-ZMM15 <- toolScratch (512 bytes)
    vmovdqu64 zmm8,  zmmword ptr [rcx + 512]
    vmovdqu64 zmm9,  zmmword ptr [rcx + 576]
    vmovdqu64 zmm10, zmmword ptr [rcx + 640]
    vmovdqu64 zmm11, zmmword ptr [rcx + 704]
    vmovdqu64 zmm12, zmmword ptr [rcx + 768]
    vmovdqu64 zmm13, zmmword ptr [rcx + 832]
    vmovdqu64 zmm14, zmmword ptr [rcx + 896]
    vmovdqu64 zmm15, zmmword ptr [rcx + 960]

    ; Minimal branchless activity marker.
    ; lastStepSuccess (offset 1032) = 1
    mov dword ptr [rcx + 1032], 1
    ret
agent_step_execute ENDP

ALIGN 16
agent_context_store PROC
    ; Store back workingState from zmm0-zmm7.
    vmovdqu64 zmmword ptr [rcx + 0], zmm0
    vmovdqu64 zmmword ptr [rcx + 64], zmm1
    vmovdqu64 zmmword ptr [rcx + 128], zmm2
    vmovdqu64 zmmword ptr [rcx + 192], zmm3
    vmovdqu64 zmmword ptr [rcx + 256], zmm4
    vmovdqu64 zmmword ptr [rcx + 320], zmm5
    vmovdqu64 zmmword ptr [rcx + 384], zmm6
    vmovdqu64 zmmword ptr [rcx + 448], zmm7
    ret
agent_context_store ENDP

END
