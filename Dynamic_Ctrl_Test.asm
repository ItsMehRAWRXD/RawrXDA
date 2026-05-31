; ==============================================================================
; Dynamic_Ctrl_Test.asm
; Test harness for Hardware Breakpoint Orchestrator
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN HOOK_THREAD : PROC
EXTERN UNHOOK_THREAD : PROC
EXTERN GET_THREAD_STATE : PROC
EXTERN SET_THREAD_STATE : PROC
EXTERN STEALTH_RESUME : PROC
EXTERN IS_HWBP_ACTIVE : PROC

.DATA
    ALIGN 16
    ; Test target function
    test_target:
        mov rax, 0DEADBEEFh
        ret
    
    ; Debug register state buffer (64 bytes)
    ALIGN 16
    dr_state dq 8 dup(0)
    
    ; Status output
    status_active dd 0
    
.CODE

PUBLIC main
main proc
    sub rsp, 40
    
    xor rbx, rbx                ; Test pass counter
    
    ; Test 1: Get thread state (current thread pseudo-handle = -2)
    mov rcx, -2                 ; GetCurrentThread()
    lea rdx, dr_state
    call GET_THREAD_STATE
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 2: Verify initial state (all DRs should be 0)
    mov rax, dr_state[0]        ; DR0
    test rax, rax
    jnz test_fail
    mov rax, dr_state[40]       ; DR7
    test rax, rax
    jnz test_fail
    inc rbx
    
    ; Test 3: Set hardware breakpoint on current thread
    ; Note: In production, use a real thread handle
    mov rcx, -2                 ; GetCurrentThread()
    lea rdx, test_target        ; Breakpoint address
    xor r8, r8                  ; BP index 0
    call HOOK_THREAD
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 4: Check if breakpoint is active
    mov rcx, -2
    xor rdx, rdx                ; BP index 0
    call IS_HWBP_ACTIVE
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 5: Get thread state (should show DR0 set)
    mov rcx, -2
    lea rdx, dr_state
    call GET_THREAD_STATE
    test eax, eax
    jz test_fail
    
    ; Verify DR0 is set
    lea rax, test_target
    mov rdx, dr_state[0]
    cmp rdx, rax
    jne test_fail
    inc rbx
    
    ; Test 6: Remove hardware breakpoint
    mov rcx, -2
    xor rdx, rdx                ; BP index 0
    call UNHOOK_THREAD
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 7: Verify breakpoint is removed
    mov rcx, -2
    xor rdx, rdx                ; BP index 0
    call IS_HWBP_ACTIVE
    test eax, eax
    jnz test_fail               ; Should be 0 (inactive)
    inc rbx
    
    ; Test 8: Get final state (should be clean)
    mov rcx, -2
    lea rdx, dr_state
    call GET_THREAD_STATE
    test eax, eax
    jz test_fail
    
    mov rax, dr_state[0]        ; DR0
    test rax, rax
    jnz test_fail
    mov rax, dr_state[40]       ; DR7
    test rax, rax
    jnz test_fail
    inc rbx
    
    ; All tests passed (8 tests)
    cmp rbx, 8
    jge test_success
    
test_fail:
    mov rcx, 1
    call ExitProcess
    
test_success:
    xor rcx, rcx
    call ExitProcess
    
main endp

end