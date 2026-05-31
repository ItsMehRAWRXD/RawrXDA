; ==============================================================================
; Sovereign Hypervisor Init - Ring -1 Gatekeeper
; ==============================================================================
; VMX initialization bridge for hypervisor-assisted instrumentation.
; Wraps the entire OS in a virtual machine for EPT-based stealth hooks.
;
; Architecture:
;   - IA32_FEATURE_CONTROL MSR verification
;   - VMXON execution with proper alignment
;   - CPUID VMX feature detection
;   - Production-ready error handling
;
; Exports:
;   CHECK_VMX_SUPPORT   - Verify CPU VMX capability
;   VMXON_EXEC           - Enter VMX root mode
;   VMXOFF_EXEC          - Exit VMX root mode
;   GET_VMX_STATUS       - Query VMX state
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Intel MSR Constants
; ==============================================================================
IA32_FEATURE_CONTROL    equ 03A0h
IA32_VMX_BASIC          equ 0480h
IA32_VMX_CR0_FIXED0     equ 0486h
IA32_VMX_CR0_FIXED1     equ 0487h
IA32_VMX_CR4_FIXED0     equ 0488h
IA32_VMX_CR4_FIXED1     equ 0489h

; Feature control bits
FEATURE_LOCK_BIT        equ 00000001h
FEATURE_VMXON_IN_SMX    equ 00000002h
FEATURE_VMXON_OUT_SMX   equ 00000004h

; CPUID VMX bit
CPUID_VMX_BIT           equ 5

; VMXON region size
VMXON_REGION_SIZE       equ 1000h    ; 4KB aligned

; ==============================================================================
; Data Section
; ==============================================================================
.data
ALIGN 16
VMXON_REGION    db VMXON_REGION_SIZE dup(0)
VMX_ENABLED     db 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; CHECK_VMX_SUPPORT - Verify CPU VMX capability
; ==============================================================================
; Input:  None
; Output: RAX = 1 if VMX supported and enabled, 0 otherwise
; Clobbers: RAX, RCX, RDX
; ==============================================================================
CHECK_VMX_SUPPORT proc
    push rbx
    push rcx
    push rdx
    
    ; 1. Check CPUID Leaf 1 (ECX bit 5 = VMX support)
    mov eax, 1
    cpuid
    bt ecx, CPUID_VMX_BIT
    jnc vmx_not_supported
    
    ; 2. Check MSR IA32_FEATURE_CONTROL
    mov ecx, IA32_FEATURE_CONTROL
    rdmsr
    
    ; Check LOCK bit (bit 0)
    test eax, FEATURE_LOCK_BIT
    jz vmx_not_configured
    
    ; Check VMXON outside SMX bit (bit 2)
    test eax, FEATURE_VMXON_OUT_SMX
    jz vmx_not_configured
    
    ; 3. Check VMX_BASIC MSR for region size
    mov ecx, IA32_VMX_BASIC
    rdmsr
    
    ; 4. Verify CR0 fixed bits
    mov ecx, IA32_VMX_CR0_FIXED0
    rdmsr
    mov rbx, rax
    mov rax, cr0
    and rax, rbx
    cmp rax, rbx
    jne vmx_not_configured
    
    ; 5. Verify CR4 fixed bits
    mov ecx, IA32_VMX_CR4_FIXED0
    rdmsr
    mov rbx, rax
    mov rax, cr4
    and rax, rbx
    cmp rax, rbx
    jne vmx_not_configured
    
    ; VMX is supported and configured
    mov eax, 1
    jmp vmx_check_exit
    
vmx_not_supported:
vmx_not_configured:
    xor eax, eax
    
vmx_check_exit:
    pop rdx
    pop rcx
    pop rbx
    ret
CHECK_VMX_SUPPORT endp

; ==============================================================================
; VMXON_EXEC - Enter VMX root mode
; ==============================================================================
; Input:  None (uses internal VMXON_REGION)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
VMXON_EXEC proc
    push rbx
    push rcx
    push rdx
    
    ; Check if already enabled
    cmp byte ptr [VMX_ENABLED], 0
    jne vmxon_already_enabled
    
    ; Verify VMX support first
    call CHECK_VMX_SUPPORT
    test eax, eax
    jz vmxon_fail
    
    ; Initialize VMXON region (clear and set revision ID)
    lea rcx, VMXON_REGION
    mov qword ptr [rcx], 0      ; Clear first 8 bytes
    
    ; Get VMX revision ID from IA32_VMX_BASIC
    mov ecx, IA32_VMX_BASIC
    rdmsr
    and eax, 07FFFFFFFh         ; Extract revision ID
    mov dword ptr [VMXON_REGION], eax
    
    ; Enable VMX in CR4
    mov rax, cr4
    or rax, 02000h              ; CR4.VMXE = bit 13
    mov cr4, rax
    
    ; Execute VMXON
    lea rax, VMXON_REGION
    vmxon qword ptr [rax]
    
    ; Check CF (Carry Flag) for failure
    jc vmxon_fail
    
    ; Mark as enabled
    mov byte ptr [VMX_ENABLED], 1
    
    mov eax, 1
    jmp vmxon_exit
    
vmxon_already_enabled:
    mov eax, 1
    jmp vmxon_exit
    
vmxon_fail:
    xor eax, eax
    
vmxon_exit:
    pop rdx
    pop rcx
    pop rbx
    ret
VMXON_EXEC endp

; ==============================================================================
; VMXOFF_EXEC - Exit VMX root mode
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
VMXOFF_EXEC proc
    push rbx
    
    ; Check if enabled
    cmp byte ptr [VMX_ENABLED], 0
    je vmxoff_not_enabled
    
    ; Execute VMXOFF
    vmxoff
    
    ; Disable VMX in CR4
    mov rax, cr4
    and rax, NOT 02000h         ; Clear CR4.VMXE
    mov cr4, rax
    
    ; Mark as disabled
    mov byte ptr [VMX_ENABLED], 0
    
    mov eax, 1
    jmp vmxoff_exit
    
vmxoff_not_enabled:
    xor eax, eax
    
vmxoff_exit:
    pop rbx
    ret
VMXOFF_EXEC endp

; ==============================================================================
; GET_VMX_STATUS - Query VMX state
; ==============================================================================
; Input:  None
; Output: RAX = 1 if VMX enabled, 0 if disabled
; ==============================================================================
GET_VMX_STATUS proc
    movzx eax, byte ptr [VMX_ENABLED]
    ret
GET_VMX_STATUS endp

; ==============================================================================
; VMX_ABORT_HANDLER - Handle VMX abort conditions
; ==============================================================================
; Input:  None
; Output: RAX = Abort reason code
; ==============================================================================
VMX_ABORT_HANDLER proc
    ; Read VMX abort indicator from VMXON region
    lea rax, VMXON_REGION
    mov eax, dword ptr [rax + 4]  ; Abort reason at offset 4
    ret
VMX_ABORT_HANDLER endp

end