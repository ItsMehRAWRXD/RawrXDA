; ============================================================================
; RESOURCE GUARD STUB - Minimal implementation for extended harness
; ============================================================================
; Provides ResourceGuard functions needed by extended harness linking
; ============================================================================

OPTION CASEMAP:NONE

public ResourceGuard_Initialize
public ResourceGuard_RegisterHandle
public ResourceGuard_UnregisterHandle
public ResourceGuard_CleanupAll

.data
align 8
dwHandleCount dd 0
dwReserved dd 0

.code

; ============================================================================
; ResourceGuard_Initialize
; RCX = pointer to resource guard structure
; Returns: EAX = success (0)
; ============================================================================
ResourceGuard_Initialize PROC
    lea rax, [dwHandleCount]
    mov dword ptr [rax], 0
    xor eax, eax
    ret
ResourceGuard_Initialize ENDP

; ============================================================================
; ResourceGuard_RegisterHandle
; RCX = pointer to resource guard
; RDX = handle to register
; R8 = handle type
; Returns: EAX = success (0)
; ============================================================================
ResourceGuard_RegisterHandle PROC
    lea rax, [dwHandleCount]
    add dword ptr [rax], 1
    xor eax, eax
    ret
ResourceGuard_RegisterHandle ENDP

; ============================================================================
; ResourceGuard_UnregisterHandle
; RCX = pointer to resource guard
; RDX = handle to unregister
; Returns: EAX = success (0)
; ============================================================================
ResourceGuard_UnregisterHandle PROC
    lea rax, [dwHandleCount]
    sub dword ptr [rax], 1
    js fix_count
    xor eax, eax
    ret
fix_count:
    mov dword ptr [rax], 0
    xor eax, eax
    ret
ResourceGuard_UnregisterHandle ENDP

; ============================================================================
; ResourceGuard_CleanupAll
; RCX = pointer to resource guard
; Returns: EAX = success (0)
; ============================================================================
ResourceGuard_CleanupAll PROC
    lea rax, [dwHandleCount]
    mov dword ptr [rax], 0
    xor eax, eax
    ret
ResourceGuard_CleanupAll ENDP

end
