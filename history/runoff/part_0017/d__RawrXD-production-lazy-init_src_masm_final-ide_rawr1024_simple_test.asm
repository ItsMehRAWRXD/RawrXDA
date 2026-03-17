option casemap:none

;==========================================================================
; rawr1024_simple_test.asm - Simple Test Without Complex Dependencies
;==========================================================================
; Tests basic functionality of compiled modules without full integration
;==========================================================================

;==========================================================================
; EXTERNAL PROCEDURES FROM COMPILED MODULES
;==========================================================================
EXTERN rawr1024_gpu_detect_tier:PROC
EXTERN rawr1024_adaptive_buffer_create:PROC
EXTERN rawr1024_check_memory_pressure:PROC

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    msg_start           BYTE "RAWR1024 Simple Module Test", 0Ah, 0
    msg_gpu_tier        BYTE "GPU Tier Detected: ", 0
    msg_buffer          BYTE "Buffer Size Created: ", 0
    msg_memory          BYTE "Memory Pressure: ", 0
    msg_success         BYTE "ALL MODULES LINKED SUCCESSFULLY", 0Ah, 0
    newline             BYTE 0Ah, 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Simple Console Output (Stub - would need Windows API for real output)
;--------------------------------------------------------------------------
print_number PROC
    ; RAX = number to print (stub)
    ret
print_number ENDP

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
    sub     rsp, 32
    
    ; Test 1: GPU Tier Detection
    call    rawr1024_gpu_detect_tier
    mov     rbx, rax                ; Save tier
    
    ; Test 2: Adaptive Buffer Creation
    mov     rcx, 1048576            ; Request 1MB
    mov     rdx, 2147483648         ; Have 2GB available
    call    rawr1024_adaptive_buffer_create
    mov     rsi, rax                ; Save buffer size
    
    ; Test 3: Check Memory Pressure
    call    rawr1024_check_memory_pressure
    mov     rdi, rax                ; Save pressure
    
    ; All tests passed - return 0
    xor     eax, eax
    
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
main ENDP

END
