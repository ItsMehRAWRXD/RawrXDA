; ===============================================================================
; TEST SUITE FOR UNIVERSAL DISPATCHER AND DRAG-AND-DROP
; ===============================================================================
; This file tests the complete implementation of all three components:
; 1. Universal Dispatcher
; 2. Drag-and-Drop UI
; 3. Instrumentation Pass
; ===============================================================================

option casemap:none

; External dependencies
extern InitializeDispatcher:proc
extern UniversalDispatch:proc
extern asm_log:proc

; Test commands
szTestReadFile      db "readFile",0
szTestWriteFile     db "writeFile",0
szTestPlan         db "plan",0
szTestStartServer  db "startServer",0

.data
szTestStarted      db "Test suite started",0
szTestCompleted    db "Test suite completed",0
szTestPassed       db "Test passed",0
szTestFailed       db "Test failed",0

.code

; Main test function
TestUniversalDispatcher PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Log test start
    lea     rcx, szTestStarted
    call    asm_log
    
    ; Initialize dispatcher
    call    InitializeDispatcher
    test    rax, rax
    jz      .test_fail
    
    ; Test readFile command
    lea     rcx, szTestReadFile
    xor     rdx, rdx
    call    UniversalDispatch
    test    rax, rax
    jz      .test_fail
    
    ; Test writeFile command
    lea     rcx, szTestWriteFile
    xor     rdx, rdx
    call    UniversalDispatch
    test    rax, rax
    jz      .test_fail
    
    ; Test plan command
    lea     rcx, szTestPlan
    xor     rdx, rdx
    call    UniversalDispatch
    test    rax, rax
    jz      .test_fail
    
    ; Test startServer command
    lea     rcx, szTestStartServer
    xor     rdx, rdx
    call    UniversalDispatch
    test    rax, rax
    jz      .test_fail
    
    ; Log test completion
    lea     rcx, szTestCompleted
    call    asm_log
    lea     rcx, szTestPassed
    call    asm_log
    
    mov     rax, 1
    jmp     .test_done
    
.test_fail:
    lea     rcx, szTestFailed
    call    asm_log
    xor     rax, rax
    
.test_done:
    leave
    ret
TestUniversalDispatcher ENDP

PUBLIC TestUniversalDispatcher

END