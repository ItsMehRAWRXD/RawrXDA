; ============================================================================
; asm_string_x64.asm - String Operations for x64
; ============================================================================

option casemap:none

.code

; ============================================================================
; asm_strlen - Get string length
; rcx = pointer to string
; Returns: length in rax
; ============================================================================
public asm_strlen
asm_strlen proc
    xor eax, eax
    cmp rcx, 0
    je strlen_done
    
strlen_loop:
    cmp byte ptr [rcx], 0
    je strlen_done
    inc eax
    inc rcx
    jmp strlen_loop

strlen_done:
    ret
asm_strlen endp

; ============================================================================
; asm_strcpy - Copy string
; rcx = destination
; rdx = source
; ============================================================================
public asm_strcpy
asm_strcpy proc
    push rdi
    push rsi
    
    mov rdi, rcx        ; rdi = destination
    mov rsi, rdx        ; rsi = source

strcpy_loop:
    mov al, [rsi]
    mov [rdi], al
    cmp al, 0
    je strcpy_done
    inc rsi
    inc rdi
    jmp strcpy_loop

strcpy_done:
    mov rax, rcx        ; return destination
    pop rsi
    pop rdi
    ret
asm_strcpy endp

; ============================================================================
; asm_strcat - Concatenate strings
; rcx = destination
; rdx = source
; ============================================================================
public asm_strcat
asm_strcat proc
    push rdi
    push rsi
    
    mov rdi, rcx        ; rdi = destination
    mov rsi, rdx        ; rsi = source
    
    ; Find end of destination
strcat_find_end:
    cmp byte ptr [rdi], 0
    je strcat_copy
    inc rdi
    jmp strcat_find_end

strcat_copy:
    mov al, [rsi]
    mov [rdi], al
    cmp al, 0
    je strcat_done
    inc rsi
    inc rdi
    jmp strcat_copy

strcat_done:
    mov rax, rcx        ; return destination
    pop rsi
    pop rdi
    ret
asm_strcat endp

; ============================================================================
; asm_strcmp - Compare strings
; rcx = string 1
; rdx = string 2
; Returns: 0 if equal, non-zero otherwise
; ============================================================================
public asm_strcmp
asm_strcmp proc
    xor eax, eax
    
strcmp_loop:
    mov al, byte ptr [rcx]
    mov bl, byte ptr [rdx]
    cmp al, bl
    jne strcmp_not_equal
    cmp al, 0
    je strcmp_done
    inc rcx
    inc rdx
    jmp strcmp_loop

strcmp_not_equal:
    movsx eax, al
    movsx ebx, bl
    sub eax, ebx

strcmp_done:
    ret
asm_strcmp endp

; ============================================================================
; StringFind - Find substring
; rcx = haystack
; rdx = needle
; Returns: pointer to found substring or NULL
; ============================================================================
public StringFind
StringFind proc
    push rdi
    push rsi
    
    mov rsi, rcx        ; rsi = haystack
    mov rdi, rdx        ; rdi = needle
    
    cmp rsi, 0
    je find_not_found
    cmp rdi, 0
    je find_not_found

find_loop:
    mov al, [rsi]
    cmp al, 0
    je find_not_found
    
    ; Check if substring matches
    mov rcx, rsi
    mov rdx, rdi
    
find_check:
    mov al, [rcx]
    mov bl, [rdx]
    cmp al, bl
    jne find_next
    cmp bl, 0
    je find_found
    inc rcx
    inc rdx
    jmp find_check

find_next:
    inc rsi
    jmp find_loop

find_found:
    mov rax, rsi
    pop rsi
    pop rdi
    ret

find_not_found:
    xor eax, eax
    pop rsi
    pop rdi
    ret
StringFind endp

; ============================================================================
; StringCompare - Compare strings (alias for strcmp)
; ============================================================================
public StringCompare
StringCompare proc
    jmp asm_strcmp
StringCompare endp

end
