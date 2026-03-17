; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Core_Utils.asm  ─  Core Utilities (Memory, Strings, Debug)
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

.DATA
    g_hProcessHeap  QWORD 0

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_GetHeap
; Returns the process heap handle, caching it if necessary.
; Returns: RAX = Heap Handle
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_GetHeap PROC FRAME
    cmp g_hProcessHeap, 0
    jne @ret_cached
    
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    call GetProcessHeap
    mov g_hProcessHeap, rax
    
    add rsp, 28h
    ret

@ret_cached:
    mov rax, g_hProcessHeap
    ret
RawrXD_GetHeap ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_MemAlloc
; Allocates memory from the process heap.
; Parameters: RCX = Size in bytes
; Returns: RAX = Pointer to memory, or NULL
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_MemAlloc PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    mov rbx, rcx        ; Save size
    
    call RawrXD_GetHeap
    
    mov rcx, rax        ; hHeap
    mov rdx, 8          ; HEAP_ZERO_MEMORY
    mov r8, rbx         ; dwBytes
    call HeapAlloc
    
    add rsp, 20h
    pop rbx
    ret
RawrXD_MemAlloc ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_MemFree
; Frees memory allocated by RawrXD_MemAlloc.
; Parameters: RCX = Pointer to memory
; Returns: RAX = TRUE/FALSE
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_MemFree PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    mov rbx, rcx        ; Save ptr
    test rbx, rbx
    jz @ret_null
    
    call RawrXD_GetHeap
    
    mov rcx, rax        ; hHeap
    xor edx, edx        ; dwFlags = 0
    mov r8, rbx         ; lpMem
    call HeapFree
    
@ret_null:
    add rsp, 20h
    pop rbx
    ret
RawrXD_MemFree ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_StrLen
; Returns length of null-terminated ASCII string.
; Parameters: RCX = String
; Returns: RAX = Length
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_StrLen PROC FRAME
    test rcx, rcx
    jz @ret_zero
    
    xor rax, rax
@loop:
    cmp byte ptr [rcx + rax], 0
    je @done
    inc rax
    jmp @loop
@done:
    ret

@ret_zero:
    xor rax, rax
    ret
RawrXD_StrLen ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HexToString
; Converts DWORD to Upper Case Hex String.
; Parameters: RCX = Value (DWORD), RDX = Buffer (At least 9 bytes)
; Returns: RAX = Length
; ═══════════════════════════════════════════════════════════════════════════════
RawrXD_HexToString PROC FRAME
    push rbx
    push rsi
    .pushreg rbx
    .pushreg rsi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rsi, rdx    ; Buffer
    mov rbx, rcx    ; Value
    
    test rbx, rbx
    jnz @convert
    
    ; Case 0
    mov byte ptr [rsi], '0'
    mov byte ptr [rsi+1], 0
    mov rax, 1
    jmp @exit
    
@convert:
    ; Count digits
    mov rax, rbx
    xor r8, r8      ; Count
@count_loop:
    test rax, rax
    jz @count_done
    shr rax, 4
    inc r8
    jmp @count_loop
@count_done:

    mov rax, r8     ; Length
    mov byte ptr [rsi+rax], 0 ; Null terminate
    
    ; Fill backwards
    mov rcx, r8
    dec rcx         ; Index
    
@fill_loop:
    mov rdx, rbx
    and rdx, 0Fh    ; Low nibble
    
    cmp dl, 9
    jg @hex_char
    add dl, '0'
    jmp @store
@hex_char:
    add dl, 'A' - 10
@store:
    mov [rsi + rcx], dl
    
    shr rbx, 4
    dec rcx
    cmp rbx, 0
    jnz @fill_loop
    
    ; Return length
    mov rax, r8

@exit:
    add rsp, 20h
    pop rsi
    pop rbx
    ret
RawrXD_HexToString ENDP

END
