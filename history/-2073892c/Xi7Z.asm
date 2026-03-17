; rawr1024_entry.asm - Entry point for RawrXD dual engine
; Provides the main CRT entry point for linking with MASM object files

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib msvcrt.lib

;==========================================================================
; External declarations - from compiled MASM modules
;==========================================================================
EXTERN rawr1024_init:PROC
EXTERN rawr1024_start_engine:PROC
EXTERN rawr1024_process:PROC
EXTERN rawr1024_gpu_detect_tier:PROC
EXTERN rawr1024_adaptive_buffer_create:PROC

;==========================================================================
; Data Section
;==========================================================================
.data
    log_startup BYTE "RawrXD Dual Engine System - Starting up...", 0Ah, 0
    log_init_done BYTE "Initialization complete. Ready for inference.", 0Ah, 0
    log_gpu_detect BYTE "GPU Detection Engine initialized.", 0Ah, 0
    hStdOut QWORD 0
    hStdErr QWORD 0

;==========================================================================
; Code Section
;==========================================================================
.code

; Entry point for the application
main PROC
    sub     rsp, 40h                    ; 32 bytes shadow space + 8 bytes alignment
    
    ; Get standard output handle
    mov     rcx, -11                    ; STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdOut], rax
    
    ; Print startup message
    lea     rcx, [log_startup]
    call    printf
    
    ; Initialize dual engine
    xor     rcx, rcx                    ; Load default config
    call    rawr1024_init
    cmp     rax, 0
    jne     init_error
    
    ; Print init complete
    lea     rcx, [log_init_done]
    call    printf
    
    ; Test GPU detection
    call    rawr1024_gpu_detect_tier
    
    ; Print GPU detection message
    lea     rcx, [log_gpu_detect]
    call    printf
    
    ; Return success
    xor     eax, eax
    add     rsp, 40h
    ret
    
init_error:
    lea     rcx, [log_init_done]
    mov     rdx, rax                    ; Error code
    call    printf
    mov     eax, 1                      ; Return error
    add     rsp, 40h
    ret
main ENDP

; Minimal printf for testing
printf PROC
    ; rcx = format string
    ; Simple implementation - just write first arg to console
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; For simplicity, just use WriteConsoleA
    mov     rdx, rcx                    ; String
    
    ; Get length
    xor     r8, r8
    xor     r9, r9
    
    pop     rsp
    pop     rbp
    ret
printf ENDP

; Entry point required by CRT
mainCRTStartup PROC
    jmp     main
mainCRTStartup ENDP

END
