; ==============================================================================
; Mem_Scanner_Test.asm
; Test harness for Memory Pattern Scanner
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

EXTERN ExitProcess : PROC
EXTERN FIND_PATTERN : PROC
EXTERN FIND_PATTERN_EX : PROC
EXTERN COMPARE_PATTERN : PROC
EXTERN CALC_PATTERN_SIZE : PROC
EXTERN COUNT_PATTERN_OCCURRENCES : PROC

.DATA
    ALIGN 16
    ; Test data buffer with known pattern
    test_data db 90h, 90h, 90h                    ; NOP sled
              db 48h, 89h, 5Ch, 24h, 08h        ; mov [rsp+8], rbx
              db 48h, 89h, 74h, 24h, 10h        ; mov [rsp+10], rsi
              db 55h                              ; push rbp
              db 90h, 90h, 90h                    ; NOP sled
              db 48h, 89h, 5Ch, 24h, 08h        ; Second occurrence
              db 48h, 89h, 74h, 24h, 10h
              db 55h
              db 0C3h                             ; ret
    test_data_end LABEL BYTE
    
    ; Pattern to find
    test_pattern db 48h, 89h, 5Ch, 24h, 08h, 48h, 89h, 74h, 24h, 10h, 55h
    test_mask    db "xxxxxxxxxxx", 0
    
    ; Pattern with wildcard
    wildcard_pattern db 48h, 89h, 5Ch, 24h, 0FFh, 48h, 89h, 74h, 24h, 10h, 55h
    wildcard_mask    db "xxxx?xxxxxx", 0
    
    ; Results
    found_addr dq 0
    pattern_len dd 0
    match_count dd 0

.CODE

PUBLIC main
main proc
    sub rsp, 40
    
    xor rbx, rbx                ; Test pass counter
    
    ; Test 1: Calculate pattern size
    lea rcx, test_mask
    call CALC_PATTERN_SIZE
    cmp eax, 11
    jne test_fail
    mov pattern_len, eax
    inc rbx
    
    ; Test 2: Compare exact pattern (should match)
    lea rcx, test_data + 3      ; Point to first pattern occurrence
    lea rdx, test_pattern
    lea r8, test_mask
    mov r9d, 11
    call COMPARE_PATTERN
    test eax, eax
    jz test_fail
    inc rbx
    
    ; Test 3: Compare pattern at wrong offset (should fail)
    lea rcx, test_data          ; Point to NOP sled
    lea rdx, test_pattern
    lea r8, test_mask
    mov r9d, 11
    call COMPARE_PATTERN
    test eax, eax
    jnz test_fail               ; Should NOT match
    inc rbx
    
    ; Test 4: Find pattern in buffer
    lea rcx, test_data
    lea rdx, test_data_end
    lea r8, test_pattern
    lea r9, test_mask
    call FIND_PATTERN
    test rax, rax
    jz test_fail
    mov found_addr, rax
    inc rbx
    
    ; Test 5: Verify found address
    lea rcx, test_data + 3
    cmp found_addr, rcx
    jne test_fail
    inc rbx
    
    ; Test 6: Find pattern with wildcard
    lea rcx, test_data
    lea rdx, test_data_end
    lea r8, wildcard_pattern
    lea r9, wildcard_mask
    call FIND_PATTERN
    test rax, rax
    jz test_fail
    inc rbx
    
    ; Test 7: Count occurrences
    lea rcx, test_data
    lea rdx, test_data_end
    lea r8, test_pattern
    lea r9, test_mask
    call COUNT_PATTERN_OCCURRENCES
    cmp eax, 2                  ; Should find 2 occurrences
    jne test_fail
    mov match_count, eax
    inc rbx
    
    ; Test 8: Find pattern with EX function
    lea rcx, test_data
    mov rdx, OFFSET test_data_end - OFFSET test_data
    lea r8, test_pattern
    lea r9, test_mask
    call FIND_PATTERN_EX
    test rax, rax
    jz test_fail
    inc rbx
    
    ; Test 9: Search for non-existent pattern (should return 0)
    lea rcx, test_data
    lea rdx, test_data_end
    lea r8, test_pattern
    lea r9, test_mask
    add rcx, 50                 ; Start past all data
    call FIND_PATTERN
    test rax, rax
    jnz test_fail               ; Should NOT find anything
    inc rbx
    
    ; All tests passed (9 tests)
    cmp rbx, 9
    jge test_success
    
test_fail:
    mov rcx, 1
    call ExitProcess
    
test_success:
    xor rcx, rcx
    call ExitProcess
    
main endp

end