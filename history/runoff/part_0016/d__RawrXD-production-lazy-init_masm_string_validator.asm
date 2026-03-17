; ValidateStringLengthASM
; RCX = len (uint64), RDX = max (uint64)
; Returns 1 if valid, 0 if corrupt/overflow
.code
ValidateStringLengthASM PROC
    cmp rcx, rdx                    ; len > max ?
    ja Invalid
    
    test rcx, 8000000000000000h     ; Check high bit (signed corruption)
    jnz Invalid
    
    mov rax, 1
    ret

Invalid:
    xor rax, rax
    ret
ValidateStringLengthASM ENDP
END
