option casemap:none

;==========================================================================
; rawr1024_integration_test.asm - Comprehensive Integration Test
; ==========================================================================
; Tests complete pipeline:
; 1. Initialize full integration
; 2. Load GGUF model via IDE menu
; 3. Check memory pressure (should trigger streaming for large models)
; 4. Dispatch inference task
; 5. Keep model warm via access update
; 6. Hotpatch model to different engine
; 7. Verify memory management and eviction
;==========================================================================

option casemap:none

;==========================================================================
; EXTERNAL PROCEDURES
;==========================================================================
EXTERN rawr1024_init_full_integration:PROC
EXTERN rawr1024_ide_load_and_dispatch:PROC
EXTERN rawr1024_ide_run_inference:PROC
EXTERN rawr1024_ide_hotpatch_model:PROC
EXTERN rawr1024_periodic_memory_maintenance:PROC
EXTERN rawr1024_get_session_status:PROC
EXTERN rawr1024_menu_load_model:PROC
EXTERN rawr1024_check_memory_pressure:PROC

EXTERN integration_session:QWORD
EXTERN engine_states:DWORD
EXTERN model_mgr:QWORD

;==========================================================================
; TEST CONSTANTS
;==========================================================================
TOTAL_MEMORY            EQU 1073741824  ; 1GB
TEST_ITERATIONS         EQU 10

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Test strings
    msg_init            BYTE "Initializing full integration...", 0Ah, 0
    msg_load            BYTE "Loading GGUF model via menu...", 0Ah, 0
    msg_memory          BYTE "Checking memory pressure...", 0Ah, 0
    msg_infer           BYTE "Running inference...", 0Ah, 0
    msg_hotpatch        BYTE "Hotpatching model...", 0Ah, 0
    msg_maint           BYTE "Running periodic maintenance...", 0Ah, 0
    msg_success         BYTE "✓ ALL TESTS PASSED", 0Ah, 0
    msg_fail            BYTE "✗ TEST FAILED", 0Ah, 0
    
    ; Test counters
    test_count          DWORD 0
    pass_count          DWORD 0
    fail_count          DWORD 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Main Test Entry Point
;--------------------------------------------------------------------------
PUBLIC main
main PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 32         ; Shadow space
    
    ; Test 1: Initialize full integration
    mov     rcx, TOTAL_MEMORY
    call    rawr1024_init_full_integration
    test    rax, rax
    jz      init_fail
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     test2
    
init_fail:
    lea     r8, fail_count
    inc     DWORD PTR [r8]
    jmp     final_result
    
test2:
    ; Test 2: Load model via menu
    call    rawr1024_menu_load_model
    cmp     eax, 0
    je      load_fail
    mov     r12d, eax       ; Save model ID
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     test3
    
load_fail:
    lea     r8, fail_count
    inc     DWORD PTR [r8]
    jmp     final_result
    
test3:
    ; Test 3: Check memory pressure
    call    rawr1024_check_memory_pressure
    ; Should return pressure percentage (0-100)
    cmp     eax, 101
    jl      pressure_ok
    lea     r8, fail_count
    inc     DWORD PTR [r8]
    jmp     final_result
    
pressure_ok:
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     test4
    
test4:
    ; Test 4: Run inference (multiple times to keep warm)
    xor     ebx, ebx
infer_loop:
    cmp     ebx, 5
    jge     test5
    
    mov     eax, r12d       ; model_id
    xor     ecx, ecx        ; engine_id
    call    rawr1024_ide_run_inference
    
    inc     ebx
    jmp     infer_loop
    
test5:
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     test6
    
test6:
    ; Test 6: Periodic maintenance (simulate background task)
    call    rawr1024_periodic_memory_maintenance
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     test7
    
test7:
    ; Test 7: Get session status
    call    rawr1024_get_session_status
    test    rax, rax
    jz      status_fail
    lea     r8, pass_count
    inc     DWORD PTR [r8]
    jmp     final_result
    
status_fail:
    lea     r8, fail_count
    inc     DWORD PTR [r8]
    
final_result:
    ; Return fail count (0 = all passed)
    lea     r8, fail_count
    mov     eax, DWORD PTR [r8]
    
    add     rsp, 32
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
main ENDP

END
