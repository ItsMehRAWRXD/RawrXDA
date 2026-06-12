;-------------------------------------------------------------------------------
; SymbolProcessor.asm - High-Speed AST Scanner
; Target: x64 Windows ABI (ml64.exe)
;-------------------------------------------------------------------------------
; Build: ml64 /c /W3 /nologo /Zi /Fo SymbolProcessor.obj SymbolProcessor.asm
;-------------------------------------------------------------------------------

.code

; Symbol POD Layout (48 bytes, 8-byte aligned)
; Offset 0:   pName       QWORD  (const char*)
; Offset 8:   pKind       QWORD  (const char*)
; Offset 16:  line        QWORD  (size_t)
; Offset 24:  column      QWORD  (size_t)
; Offset 32:  is_public   BYTE   (1 = public, 0 = private)
; Offset 33:  pad0        BYTE[7]
; Offset 40:  node_type   DWORD  (uint32_t)
; Offset 44:  pad1        BYTE[4]
; Total: 48 bytes

SYMBOL_SIZE equ 48
OFF_NAME    equ 0
OFF_KIND    equ 8
OFF_LINE    equ 16
OFF_COL     equ 24
OFF_PUBLIC  equ 32
OFF_TYPE    equ 40

;-------------------------------------------------------------------------------
; FilterPublicSymbols
;   rcx = symbolBuffer (POD_Symbol*)
;   rdx = count
; Returns:
;   rax = number of symbols with is_public == 1
;-------------------------------------------------------------------------------
FilterPublicSymbols proc public frame
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    .endprolog

    xor rax, rax            ; publicCount = 0
    test rcx, rcx
    jz L_filter_done
    test rdx, rdx
    jz L_filter_done

    mov rbx, rcx            ; rbx = base pointer
    mov rdi, rdx            ; rdi = remaining count
    xor r8, r8              ; r8 = byte offset

L_filter_loop:
    cmp byte ptr [rbx + r8 + OFF_PUBLIC], 1
    jne L_filter_next
    inc rax

L_filter_next:
    add r8, SYMBOL_SIZE
    dec rdi
    jnz L_filter_loop

L_filter_done:
    pop rdi
    pop rbx
    ret
FilterPublicSymbols endp

;-------------------------------------------------------------------------------
; FindSymbolByName
;   rcx = symbolBuffer (POD_Symbol*)
;   rdx = count
;   r8  = nameToFind (const char*, null-terminated)
; Returns:
;   rax = index of first matching symbol, or -1 if not found
;-------------------------------------------------------------------------------
FindSymbolByName proc public frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    .endprolog

    mov rax, -1             ; default: not found
    test rcx, rcx
    jz L_find_done
    test rdx, rdx
    jz L_find_done
    test r8, r8
    jz L_find_done

    mov rbx, rcx            ; rbx = symbol buffer
    mov r12, rdx            ; r12 = count (preserved)
    mov rsi, r8             ; rsi = needle (nameToFind)
    xor r9, r9              ; r9 = current index
    xor r10, r10            ; r10 = byte offset

L_find_loop:
    ; Load pName of current symbol
    mov rdi, [rbx + r10 + OFF_NAME]
    test rdi, rdi
    jz L_find_advance

    ; Byte-by-byte string compare (rdi = haystack, rsi = needle)
    push rsi
    mov rcx, rsi            ; rcx = needle
    mov rdx, rdi            ; rdx = haystack

L_compare_loop:
    mov al, [rcx]
    mov r11b, [rdx]
    cmp al, r11b
    jne L_compare_done       ; mismatch
    test al, al
    jz L_match               ; both reached null = match
    inc rcx
    inc rdx
    jmp L_compare_loop

L_compare_done:
    pop rsi
    jmp L_find_advance

L_match:
    pop rsi
    mov rax, r9
    jmp L_find_done

L_find_advance:
    add r10, SYMBOL_SIZE
    inc r9
    dec r12
    jnz L_find_loop

L_find_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FindSymbolByName endp

;-------------------------------------------------------------------------------
; CountSymbolsByKind
;   rcx = symbolBuffer (POD_Symbol*)
;   rdx = count
;   r8  = kindToFind (const char*, null-terminated)
; Returns:
;   rax = number of symbols with matching kind
;-------------------------------------------------------------------------------
CountSymbolsByKind proc public frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .endprolog

    xor rax, rax            ; matchCount = 0
    test rcx, rcx
    jz L_kind_done
    test rdx, rdx
    jz L_kind_done
    test r8, r8
    jz L_kind_done

    mov rbx, rcx            ; rbx = symbol buffer
    mov r12, rdx            ; r12 = count
    mov rsi, r8             ; rsi = needle (kindToFind)
    xor r13, r13            ; r13 = byte offset

L_kind_loop:
    mov rdi, [rbx + r13 + OFF_KIND]
    test rdi, rdi
    jz L_kind_next

    push rsi
    mov rcx, rsi
    mov rdx, rdi

L_compare_kind:
    mov al, [rcx]
    mov r11b, [rdx]
    cmp al, r11b
    jne L_compare_kind_done
    test al, al
    jz L_kind_match
    inc rcx
    inc rdx
    jmp L_compare_kind

L_compare_kind_done:
    pop rsi
    jmp L_kind_next

L_kind_match:
    pop rsi
    inc rax

L_kind_next:
    add r13, SYMBOL_SIZE
    dec r12
    jnz L_kind_loop

L_kind_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CountSymbolsByKind endp

end
