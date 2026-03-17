;==========================================================================
; logging_helpers.asm - Simple Logging Helper Functions
;==========================================================================
; Provides basic integer logging functions for debugging and metrics.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    num_buffer BYTE 32 DUP (0)

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: log_int32 - Log 32-bit integer (hotpatchable)
; INPUT: rcx = value, rdx = base (10 for decimal)
; OUTPUT: none
;==========================================================================
PUBLIC log_int32
ALIGN 16
log_int32 PROC
    push rsi
    push rdi
    push rbx
    sub rsp, 32
    
    mov eax, ecx        ; value
    mov ebx, edx        ; base
    
    ; Convert integer to string
    lea rdi, num_buffer
    call int_to_string
    
    ; Log the string
    lea rcx, num_buffer
    call OutputDebugStringA
    
    add rsp, 32
    pop rbx
    pop rdi
    pop rsi
    ret
log_int32 ENDP

;==========================================================================
; PUBLIC: log_int64 - Log 64-bit integer (hotpatchable)
; INPUT: rcx = value, rdx = base (10 for decimal)
; OUTPUT: none
;==========================================================================
PUBLIC log_int64
ALIGN 16
log_int64 PROC
    push rsi
    push rdi
    push rbx
    sub rsp, 32
    
    mov rax, rcx        ; value
    mov ebx, edx        ; base
    
    ; Convert integer to string
    lea rdi, num_buffer
    call int64_to_string
    
    ; Log the string
    lea rcx, num_buffer
    call OutputDebugStringA
    
    add rsp, 32
    pop rbx
    pop rdi
    pop rsi
    ret
log_int64 ENDP

;==========================================================================
; PUBLIC: _log_int32 - Log 32-bit integer (underscore version)
; INPUT: rcx = value
; OUTPUT: none
;==========================================================================
PUBLIC _log_int32
ALIGN 16
_log_int32 PROC
    ; Just call log_int32 with base 10
    mov rdx, 10         ; base 10
    jmp log_int32
_log_int32 ENDP

;==========================================================================
; PUBLIC: _log_int64 - Log 64-bit integer (underscore version)
; INPUT: rcx = value
; OUTPUT: none
;==========================================================================
PUBLIC _log_int64
ALIGN 16
_log_int64 PROC
    ; Just call log_int64 with base 10
    mov rdx, 10         ; base 10
    jmp log_int64
_log_int64 ENDP

;==========================================================================
; HELPER: int_to_string - Convert 32-bit integer to string
; INPUT: rdi = buffer, eax = value
; OUTPUT: none
;==========================================================================
; REMOVED TO AVOID DUPLICATE SYMBOL WITH json_hotpatch_helpers.asm
; int_to_string PROC
;     push rsi
;     push rbx
;     
;     mov ebx, 10         ; divisor
;     
;     ; Handle negative
;     test eax, eax
;     jns positive_value
;     neg eax
;     mov byte ptr [rdi], '-'
;     inc rdi
;     
; positive_value:
;     ; Convert digits (reverse order)
;     mov rsi, rdi
;     
; convert_loop:
;     xor edx, edx
;     div ebx
;     add dl, '0'
;     mov byte ptr [rsi], dl
;     inc rsi
;     test eax, eax
;     jnz convert_loop
;     
;     ; Null terminate
;     mov byte ptr [rsi], 0
;     
;     ; Reverse the string
;     dec rsi
; reverse_loop:
;     cmp rdi, rsi
;     jge reverse_done
;     
;     mov al, byte ptr [rdi]
;     mov dl, byte ptr [rsi]
;     mov byte ptr [rdi], dl
;     mov byte ptr [rsi], al
;     
;     inc rdi
;     dec rsi
;     jmp reverse_loop
;     
; reverse_done:
;     pop rbx
;     pop rsi
;     ret
; int_to_string ENDP

; Use the implementation from json_hotpatch_helpers.asm instead
EXTERN int_to_string:PROC

;==========================================================================
; HELPER: int64_to_string - Convert 64-bit integer to string
; INPUT: rdi = buffer, rax = value
; OUTPUT: none
;==========================================================================
int64_to_string PROC
    push rsi
    push rbx
    
    mov rbx, 10         ; divisor
    
    ; Handle negative
    test rax, rax
    jns positive_value64
    neg rax
    mov byte ptr [rdi], '-'
    inc rdi
    
positive_value64:
    ; Convert digits (reverse order)
    mov rsi, rdi
    
convert_loop64:
    xor rdx, rdx
    div rbx
    add dl, '0'
    mov byte ptr [rsi], dl
    inc rsi
    test rax, rax
    jnz convert_loop64
    
    ; Null terminate
    mov byte ptr [rsi], 0
    
    ; Reverse the string
    dec rsi
reverse_loop64:
    cmp rdi, rsi
    jge reverse_done64
    
    mov al, byte ptr [rdi]
    mov dl, byte ptr [rsi]
    mov byte ptr [rdi], dl
    mov byte ptr [rsi], al
    
    inc rdi
    dec rsi
    jmp reverse_loop64
    
reverse_done64:
    pop rbx
    pop rsi
    ret
int64_to_string ENDP

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN OutputDebugStringA:PROC

END
