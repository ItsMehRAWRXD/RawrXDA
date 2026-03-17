; ════════════════════════════════════════════════════════════════════════════════
; Analysis - Classify and Count
; ════════════════════════════════════════════════════════════════════════════════

.data
    ; Pattern strings (ASCII)
    pat_ffn             db 'ffn', 0
    pat_attn            db 'attn', 0
    pat_embed           db 'embed', 0
    pat_norm            db 'norm', 0

.code

; Normalize string handling to use ASCII for tensor names and patterns
CompareStrings PROC
    ; rcx = pointer to string 1
    ; rdx = pointer to string 2
    ; r8  = length of strings

    xor rax, rax          ; Clear result register

CompareLoop:
    test r8, r8           ; Check if length is zero
    jz CompareDone

    movzx r9, byte ptr [rcx] ; Load byte from string 1
    movzx r10, byte ptr [rdx] ; Load byte from string 2

    cmp r9, r10           ; Compare bytes
    jne CompareMismatch

    inc rcx               ; Move to next byte in string 1
    inc rdx               ; Move to next byte in string 2
    dec r8                ; Decrement length
    jmp CompareLoop

CompareMismatch:
    mov rax, 1            ; Set result to mismatch

CompareDone:
    ret
CompareStrings ENDP

AnalyzeTensors PROC
    ; Shadow space + local variables + alignment
    sub rsp, 40h

    ; Classify tensors
    ; ... (classification logic here) ...

    ; Count tensor types
    ; ... (counting logic here) ...

    add rsp, 40h
    ret
AnalyzeTensors ENDP

END AnalyzeTensors