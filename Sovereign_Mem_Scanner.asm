; ==============================================================================
; Sovereign Memory Scanner - Pattern-Based Memory Analysis
; ==============================================================================
; ASLR-resilient signature scanning for locating data structures dynamically.
; Uses wildcard-enabled byte patterns to find game objects without static offsets.
;
; Architecture:
;   - Wildcard pattern matching (? = any byte)
;   - Read-only memory observation
;   - x64 ABI compliant
;   - No external dependencies (uses direct memory reads)
;
; Exports:
;   FIND_PATTERN        - Scan memory region for byte signature
;   FIND_PATTERN_EX     - Extended scan with process handle
;   COMPARE_PATTERN     - Compare buffer against pattern with mask
;   CALC_PATTERN_SIZE   - Calculate pattern length from mask string
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; External Imports
; ==============================================================================
EXTERN ExitProcess : PROC

; ==============================================================================
; Constants
; ==============================================================================
MAX_PATTERN_LEN equ 256
WILDCARD_CHAR   equ '?'

; ==============================================================================
; Data Section
; ==============================================================================
.data
align 16
; Test pattern data
    test_pattern db 48h, 89h, 5Ch, 24h, 08h, 48h, 89h, 74h, 24h, 10h, 55h
    test_mask    db "xxxx?xxxxxx", 0
    test_buffer  db 48h, 89h, 5Ch, 24h, 0AAh, 48h, 89h, 74h, 24h, 10h, 55h, 90h, 90h, 90h
    
    ; Results
    found_address dq 0
    pattern_size  dd 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; CALC_PATTERN_SIZE - Calculate pattern length from mask string
; ==============================================================================
; Input:  RCX = Pointer to mask string (null-terminated, e.g., "xx??x")
; Output: RAX = Pattern length in bytes
; Clobbers: RAX, RCX, RDX
; ==============================================================================
CALC_PATTERN_SIZE proc
    xor eax, eax
    
calc_loop:
    movzx edx, byte ptr [rcx + rax]
    test dl, dl
    jz calc_done
    inc eax
    jmp calc_loop
    
calc_done:
    ret
CALC_PATTERN_SIZE endp

; ==============================================================================
; COMPARE_PATTERN - Compare buffer against pattern with mask
; ==============================================================================
; Input:  RCX = Pointer to data buffer
;         RDX = Pointer to pattern bytes
;         R8  = Pointer to mask string
;         R9  = Pattern length
; Output: RAX = 1 if match, 0 if no match
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
COMPARE_PATTERN proc
    xor r10d, r10d            ; Index counter
    
compare_loop:
    cmp r10, r9
    jge compare_match         ; Reached end = match
    
    ; Check mask character
    movzx r11d, byte ptr [r8 + r10]
    cmp r11b, WILDCARD_CHAR
    je compare_next           ; Wildcard = skip comparison
    
    ; Compare actual bytes
    movzx r11d, byte ptr [rcx + r10]
    movzx eax, byte ptr [rdx + r10]
    cmp r11b, al
    jne compare_fail          ; Mismatch
    
compare_next:
    inc r10
    jmp compare_loop
    
compare_match:
    mov eax, 1
    ret
    
compare_fail:
    xor eax, eax
    ret
COMPARE_PATTERN endp

; ==============================================================================
; FIND_PATTERN - Scan memory region for byte signature
; ==============================================================================
; Input:  RCX = Start address
;         RDX = End address
;         R8  = Pointer to pattern bytes
;         R9  = Pointer to mask string
; Output: RAX = Found address or 0 if not found
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
FIND_PATTERN proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Save parameters
    mov r12, rcx              ; Current address
    mov r13, rdx              ; End address
    mov r14, r8               ; Pattern
    mov r15, r9               ; Mask
    
    ; Calculate pattern size
    mov rcx, r15
    call CALC_PATTERN_SIZE
    mov ebx, eax              ; RBX = pattern length
    test ebx, ebx
    jz find_fail              ; Empty pattern
    
    ; Calculate scan limit
    sub r13, rbx              ; End - pattern_len (can't match past end)
    
scan_loop:
    cmp r12, r13
    jg find_fail              ; Past scan limit
    
    ; Compare at current position
    mov rcx, r12              ; Data buffer
    mov rdx, r14              ; Pattern
    mov r8, r15               ; Mask
    mov r9d, ebx              ; Length
    call COMPARE_PATTERN
    test eax, eax
    jnz find_success
    
    ; Advance to next byte
    inc r12
    jmp scan_loop
    
find_success:
    mov rax, r12
    jmp find_exit
    
find_fail:
    xor eax, eax
    
find_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FIND_PATTERN endp

; ==============================================================================
; FIND_PATTERN_EX - Extended scan with bounds checking
; ==============================================================================
; Input:  RCX = Start address
;         RDX = Region size
;         R8  = Pointer to pattern bytes
;         R9  = Pointer to mask string
; Output: RAX = Found address or 0 if not found
; ==============================================================================
FIND_PATTERN_EX proc
    ; Calculate end address
    add rdx, rcx
    call FIND_PATTERN
    ret
FIND_PATTERN_EX endp

; ==============================================================================
; COUNT_PATTERN_OCCURRENCES - Count how many times pattern appears
; ==============================================================================
; Input:  RCX = Start address
;         RDX = End address
;         R8  = Pointer to pattern bytes
;         R9  = Pointer to mask string
; Output: RAX = Number of occurrences
; ==============================================================================
COUNT_PATTERN_OCCURRENCES proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx              ; Current
    mov r13, rdx              ; End
    mov r14, r8               ; Pattern
    mov r15, r9               ; Mask
    xor ebx, ebx              ; Count
    
    ; Get pattern size
    mov rcx, r15
    call CALC_PATTERN_SIZE
    mov esi, eax
    test esi, esi
    jz count_exit
    
    sub r13, rsi
    
count_loop:
    cmp r12, r13
    jg count_exit
    
    mov rcx, r12
    mov rdx, r14
    mov r8, r15
    mov r9d, esi
    call COMPARE_PATTERN
    test eax, eax
    jz count_next
    
    inc ebx                   ; Found match
    add r12, rsi              ; Skip past this match
    jmp count_loop
    
count_next:
    inc r12
    jmp count_loop
    
count_exit:
    mov eax, ebx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
COUNT_PATTERN_OCCURRENCES endp

end