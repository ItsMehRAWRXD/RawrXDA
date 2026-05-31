; ==============================================================================
; VEH_Core_Test.asm
; Test harness for Vectored Exception Handler
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN INSTALL_VEH : PROC
EXTERN REMOVE_VEH : PROC
EXTERN GET_LAST_EXCEPTION : PROC
EXTERN GET_EXCEPTION_STATS : PROC

.DATA
    ALIGN 16
    ; Exception info output buffer
    exc_info db 256 dup(0)
    
    ; Stats output
    stats_total dd 0
    stats_single dd 0
    stats_av dd 0
    
    ; Test counter
    test_passed dd 0

.CODE

PUBLIC main
main proc
    sub rsp, 40
    
    xor rbx, rbx                ; Test pass counter
    
    ; Test 1: Install VEH
    call INSTALL_VEH
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 2: Get stats (should be 0 initially)
    lea rcx, stats_total
    call GET_EXCEPTION_STATS
    test eax, eax
    jz test_fail
    
    cmp stats_total, 0
    jne test_fail
    inc rbx
    
    ; Test 3: Trigger INT 3 (software breakpoint)
    int 3
    
    ; If we get here, VEH caught it and continued
    inc rbx
    
    ; Test 4: Get stats (should show 1 exception)
    lea rcx, stats_total
    call GET_EXCEPTION_STATS
    test eax, eax
    jz test_fail
    
    cmp stats_total, 1
    jl test_fail                ; At least 1 exception
    inc rbx
    
    ; Test 5: Get last exception info
    lea rcx, exc_info
    call GET_LAST_EXCEPTION
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 6: Remove VEH
    call REMOVE_VEH
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 7: Re-install VEH (should succeed)
    call INSTALL_VEH
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 8: Remove VEH again
    call REMOVE_VEH
    test eax, eax
    jz test_fail
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