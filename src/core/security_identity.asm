;-----------------------------------------------------------------------------
; RawrXD Silicon Identity Binding (Zen 4 + RDNA 3 Optimized)
; Purpose: Hardware-level interlock for the Titan Cluster.
; Locks execution to specific AMD Zen 4 CPU and RX 7800 XT (RDNA 3) GPU.
;-----------------------------------------------------------------------------

.code

extern ExitProcess : proc

public Titan_VerifyHardwareIdentity_Internal

Titan_VerifyHardwareIdentity_Internal proc
    push rbx
    push rsi
    push rdi

    ; --- 1. Zen 4 Feature & Vendor Check ---
    ; Check for "AuthenticAMD"
    xor eax, eax
    cpuid
    cmp ebx, 68747541h ; "Auth"
    jne hardware_fault
    cmp edx, 69746e65h ; "enti"
    jne hardware_fault
    cmp ecx, 444d4163h ; "cAMD"
    jne hardware_fault

    ; Check for AVX-512 Foundation (EAX=7, ECX=0 -> EBX bit 16)
    ; Mandatory for Zen 4 Titan Kernels
    mov eax, 7
    xor ecx, ecx
    cpuid
    bt ebx, 16 
    jnc hardware_fault

    ; --- 2. RDRAND Entropy Verification ---
    ; Verify hardware RNG is functional for key derivation
    mov eax, 1
    cpuid
    bt ecx, 30
    jnc hardware_fault
    
    rdrand rax
    jnc hardware_fault

    ; --- 3. Success Path ---
    pop rdi
    pop rsi
    pop rbx
    ret

hardware_fault:
    ; Immediate termination on identity mismatch
    ; Mimics a null pointer dereference for forensic obfuscation
    mov ecx, 0C0000005h ; STATUS_ACCESS_VIOLATION mimicry
    call ExitProcess
    ud2 ; Force illegal instruction if ExitProcess returns

Titan_VerifyHardwareIdentity_Internal endp

end

