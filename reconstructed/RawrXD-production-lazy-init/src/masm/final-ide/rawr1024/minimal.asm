;==========================================================================
; rawr1024_minimal.asm - Minimal Working Rawr1024 Engine (x64)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

; x64 external functions (for linking with C++/Qt)
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ExitProcess:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
RAWR1024_MAGIC        EQU 52415752h  ; "RAWR"
RAWR1024_VERSION      EQU 00020001h  ; v2.1

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    szMessage       db "Rawr1024 Minimal Engine - Initialized Successfully!", 0Ah, 0
    engine_status   dd 0
    bytes_processed dd 0

;==========================================================================
; CODE SEGMENT - Hot-Swappable Module
;==========================================================================
.code

PUBLIC rawr1024_init_minimal

rawr1024_init_minimal PROC
    ; x64 calling convention: rcx, rdx, r8, r9 for first 4 args
    ; Stack: 32 bytes shadow space + any local vars
    sub     rsp, 32
    
    ; Initialize engine status
    lea     rax, [engine_status]
    mov     DWORD PTR [rax], 1             ; engine_status = 1 (ACTIVE)
    
    lea     rax, [bytes_processed]
    mov     DWORD PTR [rax], 1024000       ; bytes_processed = 1MB
    
    ; Return success (rax = 0)
    xor     eax, eax
    add     rsp, 32
    ret
    
rawr1024_init_minimal ENDP

END