; ==============================================================================
; Sovereign_Hyper_Detector.asm - VMX Capability Probe (Ring 3 Safe)
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
CPUID_VMX_BIT           equ 5
CPUID_HYPERVISOR_BIT    equ 31
IA32_VMX_BASIC          equ 480h
IA32_FEATURE_CONTROL    equ 3Ah

; ==============================================================================
; External APIs
; ==============================================================================
EXTERN ExitProcess : PROC

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16

; Detection results
ALIGN 16
VmxSupported            dq 0
VmxLocked               dq 0
RunningUnderHV          dq 0
VmxonRegionNeeded       dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; CHECK_VMX_SUPPORT: Safe CPUID probe for VMX capabilities
; Returns: RAX = 1 (supported), 0 (not supported)
; ==============================================================================
CHECK_VMX_SUPPORT PROC
    push rbx
    push rcx
    push rdx
    
    ; Check if already running under a hypervisor
    mov eax, 1
    cpuid
    test ecx, (1 shl 31)
    jz no_hypervisor
    
    ; Running under hypervisor — nested VT-x may not be available
    mov RunningUnderHV, 1
    
no_hypervisor:
    ; Check VMX bit in ECX
    test ecx, (1 shl 5)
    jz vmx_not_supported
    
    ; VMX is supported by CPU
    mov VmxSupported, 1
    
    ; Get VMXON region size from IA32_VMX_BASIC MSR
    ; NOTE: Cannot read MSR from Ring 3 — would #GP fault
    ; In a real driver, you would:
    ;   mov ecx, IA32_VMX_BASIC
    ;   rdmsr
    ;   and eax, 01FFh    ; Bits 0-8 = VMCS revision identifier
    ;   mov VmxonRegionNeeded, rax
    
    ; For this probe, we assume standard 4KB (4096 bytes)
    mov VmxonRegionNeeded, 4096
    
    ; Check if IA32_FEATURE_CONTROL is locked
    ; NOTE: Also requires Ring 0. In a driver:
    ;   mov ecx, IA32_FEATURE_CONTROL
    ;   rdmsr
    ;   test eax, 1       ; Bit 0 = Lock bit
    ;   jz not_locked
    ;   test eax, 4       ; Bit 2 = Enable VMX outside SMX
    ;   jz vmx_disabled
    
    ; For this probe, we report "unknown" for lock state
    ; (would need kernel driver to verify)
    mov VmxLocked, 0FFFFFFFFFFFFFFFFh   ; -1 = unknown (need Ring 0)
    
    mov rax, 1
    jmp vmx_exit
    
vmx_not_supported:
    xor rax, rax
    
vmx_exit:
    pop rdx
    pop rcx
    pop rbx
    ret
CHECK_VMX_SUPPORT ENDP

; ==============================================================================
; main - VMX Detection Entry Point
; ==============================================================================
main PROC
    sub rsp, 40
    
    ; Run detection
    call CHECK_VMX_SUPPORT
    
    ; Return 0 if VMX supported, 1 if not
    test eax, eax
    jz no_vmx
    
    ; VMX supported — exit 0
    xor ecx, ecx
    call ExitProcess
    
no_vmx:
    mov ecx, 1
    call ExitProcess
    
main ENDP

; ==============================================================================
; End
; ==============================================================================
end
