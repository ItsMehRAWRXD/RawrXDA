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

END
