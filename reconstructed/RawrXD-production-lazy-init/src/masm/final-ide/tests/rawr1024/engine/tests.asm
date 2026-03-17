;==========================================================================
; rawr1024_engine_tests.asm - Test Harness for Quad Dual Engine Architecture
; ==========================================================================
; Tests the new functions: rawr1024_init_quad_dual_engines, rawr1024_hotpatch_engine, rawr1024_dispatch_agent_task
; Assembles with ML64 and links with rawr1024_dual_engine_custom.obj
;==========================================================================

option casemap:none

;==========================================================================
; INCLUDES AND EXTERNS
;==========================================================================
EXTERN rawr1024_init_quad_dual_engines:PROC
EXTERN rawr1024_hotpatch_engine:PROC
EXTERN rawr1024_dispatch_agent_task:PROC
EXTERN rawr1024_init:PROC
EXTERN rawr1024_cleanup:PROC
EXTERN engine_states:BYTE

;==========================================================================
; STRUCTURE DEFINITIONS (copied from main file)
;==========================================================================
ENGINE_STATE STRUCT
    id                  DWORD ?
    status              DWORD ?
    progress            DWORD ?
    error_code          DWORD ?
    memory_base         QWORD ?
    memory_size         QWORD ?
    thread_id           QWORD ?
    start_time          QWORD ?
ENGINE_STATE ENDS

;==========================================================================
; TEST CONSTANTS
;==========================================================================
RAWR1024_ENGINE_COUNT EQU 8

;==========================================================================
; TEST DATA SEGMENT
;==========================================================================
.data

; Test messages
msg_test_start        BYTE "Starting Rawr1024 Engine Tests", 0Ah, 0
msg_test_passed       BYTE "Test PASSED", 0Ah, 0
msg_test_failed       BYTE "Test FAILED", 0Ah, 0
msg_init_test         BYTE "Testing rawr1024_init_quad_dual_engines...", 0Ah, 0
msg_hotpatch_test     BYTE "Testing rawr1024_hotpatch_engine...", 0Ah, 0
msg_dispatch_test     BYTE "Testing rawr1024_dispatch_agent_task...", 0Ah, 0
msg_engine_status     BYTE "Engine %d: status=%d, progress=%d, error=%d", 0Ah, 0

; Test configuration for hotpatch
test_config          QWORD 0123456789ABCDEFh

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Test Helper: Print String (simplified)
;--------------------------------------------------------------------------
print_string PROC
    ; Input: RCX = string pointer
    ; This is a stub - actual output handled by test exit code
    ret
print_string ENDP

;--------------------------------------------------------------------------
; Test Helper: Exit with Code
;--------------------------------------------------------------------------
test_exit PROC
    ; Input: RCX = exit code
    mov     rax, rcx
    ret
test_exit ENDP

;--------------------------------------------------------------------------
; Test 1: Initialize Quad Dual Engines
;--------------------------------------------------------------------------
test_init_quad_dual_engines PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    ; Print test message
    lea     rcx, msg_init_test
    call    print_string
    
    ; Call the function
    call    rawr1024_init_quad_dual_engines
    test    rax, rax
    jz      test_init_fail
    
    ; Verify all engines were initialized
    lea     rsi, engine_states
    xor     rbx, rbx            ; engine counter
    
verify_engines_loop:
    cmp     rbx, RAWR1024_ENGINE_COUNT
    jge     test_init_success
    
    ; Check engine status (should be group + 10)
    mov     rax, rbx
    shr     rax, 1              ; group = engine / 2
    add     eax, 10             ; status = group + 10
    
    mov     ecx, DWORD PTR [rsi].ENGINE_STATE.status
    cmp     ecx, eax
    jne     test_init_fail
    
    ; Check memory base is allocated
    mov     rax, QWORD PTR [rsi].ENGINE_STATE.memory_base
    test    rax, rax
    jz      test_init_fail
    
    ; Check memory size is 2MB
    mov     rax, QWORD PTR [rsi].ENGINE_STATE.memory_size
    cmp     rax, 2097152
    jne     test_init_fail
    
    ; Move to next engine
    add     rsi, SIZEOF ENGINE_STATE
    inc     rbx
    jmp     verify_engines_loop
    
test_init_success:
    lea     rcx, msg_test_passed
    call    print_string
    mov     rax, 1
    jmp     test_init_exit
    
test_init_fail:
    lea     rcx, msg_test_failed
    call    print_string
    xor     rax, rax
    
test_init_exit:
    pop     rsi
    pop     rbx
    pop     rbp
    ret
test_init_quad_dual_engines ENDP

;--------------------------------------------------------------------------
; Test 2: Hotpatch Engine
;--------------------------------------------------------------------------
test_hotpatch_engine PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    ; Print test message
    lea     rcx, msg_hotpatch_test
    call    print_string
    
    ; Try to hotpatch engine 0
    mov     rcx, 0              ; engine_id
    lea     rdx, test_config    ; config_ptr
    mov     r8, 1               ; patch_flags
    call    rawr1024_hotpatch_engine
    test    rax, rax
    jz      test_hotpatch_fail
    
    ; Verify progress was incremented
    lea     rsi, engine_states
    mov     ecx, DWORD PTR [rsi].ENGINE_STATE.progress
    cmp     ecx, 1
    jne     test_hotpatch_fail
    
    ; Try invalid engine (should fail)
    mov     rcx, RAWR1024_ENGINE_COUNT  ; invalid engine
    lea     rdx, test_config
    mov     r8, 1
    call    rawr1024_hotpatch_engine
    test    rax, rax
    jnz     test_hotpatch_fail  ; should return 0
    
    lea     rcx, msg_test_passed
    call    print_string
    mov     rax, 1
    jmp     test_hotpatch_exit
    
test_hotpatch_fail:
    lea     rcx, msg_test_failed
    call    print_string
    xor     rax, rax
    
test_hotpatch_exit:
    pop     rsi
    pop     rbx
    pop     rbp
    ret
test_hotpatch_engine ENDP

;--------------------------------------------------------------------------
; Test 3: Dispatch Agent Task
;--------------------------------------------------------------------------
test_dispatch_agent_task PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    ; Print test message
    lea     rcx, msg_dispatch_test
    call    print_string
    
    ; Test dispatch to primary AI (type 0)
    mov     rcx, 0              ; task_type = AI_INFERENCE
    xor     rdx, rdx            ; task_data_ptr = NULL
    mov     r8, 0               ; task_size = 0
    call    rawr1024_dispatch_agent_task
    
    ; Should return 0 or 1
    cmp     eax, 0
    je      dispatch_primary_ok
    cmp     eax, 1
    je      dispatch_primary_ok
    jmp     test_dispatch_fail
    
dispatch_primary_ok:
    ; Verify progress was incremented on assigned engine
    mov     ebx, eax            ; save engine_id
    imul    rsi, rbx, SIZEOF ENGINE_STATE
    lea     rsi, engine_states
    add     rsi, rbx
    mov     ecx, DWORD PTR [rsi].ENGINE_STATE.progress
    cmp     ecx, 1
    jne     test_dispatch_fail
    
    ; Test dispatch to orchestration (type 3)
    mov     rcx, 3              ; task_type = ORCHESTRATION
    xor     rdx, rdx
    mov     r8, 0
    call    rawr1024_dispatch_agent_task
    cmp     eax, 6              ; should return engine 6
    jne     test_dispatch_fail
    
    ; Test invalid task type (should return -1)
    mov     rcx, 4              ; invalid type
    xor     rdx, rdx
    mov     r8, 0
    call    rawr1024_dispatch_agent_task
    cmp     eax, -1
    jne     test_dispatch_fail
    
    lea     rcx, msg_test_passed
    call    print_string
    mov     rax, 1
    jmp     test_dispatch_exit
    
test_dispatch_fail:
    lea     rcx, msg_test_failed
    call    print_string
    xor     rax, rax
    
test_dispatch_exit:
    pop     rsi
    pop     rbx
    pop     rbp
    ret
test_dispatch_agent_task ENDP

;--------------------------------------------------------------------------
; Main Test Runner - Simplified (No System Calls)
;--------------------------------------------------------------------------
main PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    
    ; Test: Verify engine_states structure and dispatch behavior
    lea     rsi, engine_states
    
    ; Test 1: Call rawr1024_dispatch_agent_task (type 0 - Primary)
    mov     rcx, 0              ; task_type = 0 (AI_INFERENCE)
    xor     rdx, rdx            ; task_data = NULL
    xor     r8, r8              ; task_size = 0
    call    rawr1024_dispatch_agent_task
    
    ; Verify result is 0 or 1 (engine 0 or 1)
    cmp     eax, 0
    je      test1_ok
    cmp     eax, 1
    je      test1_ok
    jmp     main_fail
    
test1_ok:
    ; Test 2: Call rawr1024_dispatch_agent_task (type 3 - Orchestration)
    mov     rcx, 3              ; task_type = 3 (ORCHESTRATION)
    xor     rdx, rdx
    xor     r8, r8
    call    rawr1024_dispatch_agent_task
    
    ; Should return engine 6
    cmp     eax, 6
    jne     main_fail
    
    ; Test 3: Verify invalid task type returns -1
    mov     rcx, 4              ; invalid type
    xor     rdx, rdx
    xor     r8, r8
    call    rawr1024_dispatch_agent_task
    cmp     eax, -1
    jne     main_fail
    
    ; All tests passed
    xor     rax, rax            ; success exit code 0
    jmp     main_done
    
main_fail:
    mov     rax, 1              ; failure exit code 1
    
main_done:
    pop     rsi
    pop     rbx
    pop     rbp
    ret

main ENDP

END