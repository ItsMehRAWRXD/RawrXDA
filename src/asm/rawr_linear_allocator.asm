; rawr_linear_allocator.asm
; MASM x64 Linear Pool Allocator — replaces xmemory / std::allocator
; Uses HeapAlloc directly to bypass CRT heap management
;
; Exports:
;   RawrLinearAlloc_Init      — initialize the process heap handle
;   RawrLinearAlloc_Alloc     — allocate zeroed memory from process heap
;   RawrLinearAlloc_Free      — free memory back to process heap
;   RawrLinearAlloc_Realloc   — resize an existing block
;
; Calling convention: Microsoft x64 (RCX, RDX, R8, R9)

extern GetProcessHeap : proc
extern HeapAlloc      : proc
extern HeapFree       : proc
extern HeapReAlloc    : proc

.data
    g_hHeap dq 0
    g_allocCount dq 0
    g_freeCount  dq 0

.code

; ---------------------------------------------------------------------------
; RawrLinearAlloc_Init
;   RCX = flags (reserved, pass 0)
;   Returns: RAX = 1 on success, 0 on failure
; ---------------------------------------------------------------------------
RawrLinearAlloc_Init proc public
    sub rsp, 40
    call GetProcessHeap
    test rax, rax
    jz @fail
    mov g_hHeap, rax
    mov rax, 1
    add rsp, 40
    ret
@fail:
    xor rax, rax
    add rsp, 40
    ret
RawrLinearAlloc_Init endp

; ---------------------------------------------------------------------------
; RawrLinearAlloc_Alloc
;   RCX = size in bytes
;   Returns: RAX = pointer to zeroed memory, or NULL
; ---------------------------------------------------------------------------
RawrLinearAlloc_Alloc proc public
    sub rsp, 40
    mov r8, rcx                 ; dwBytes = size
    mov rdx, 8                  ; HEAP_ZERO_MEMORY
    mov rcx, g_hHeap
    call HeapAlloc
    test rax, rax
    jz @alloc_done
    inc qword ptr [g_allocCount]
@alloc_done:
    add rsp, 40
    ret
RawrLinearAlloc_Alloc endp

; ---------------------------------------------------------------------------
; RawrLinearAlloc_Free
;   RCX = pointer to free
;   Returns: RAX = 1 on success, 0 on failure
; ---------------------------------------------------------------------------
RawrLinearAlloc_Free proc public
    sub rsp, 40
    test rcx, rcx
    jz @free_done               ; NULL free is a no-op
    mov r8, rcx                 ; lpMem
    xor rdx, rdx                ; dwFlags = 0
    mov rcx, g_hHeap
    call HeapFree
    test rax, rax
    jz @free_done
    inc qword ptr [g_freeCount]
@free_done:
    add rsp, 40
    ret
RawrLinearAlloc_Free endp

; ---------------------------------------------------------------------------
; RawrLinearAlloc_Realloc
;   RCX = old pointer (may be NULL)
;   RDX = new size in bytes
;   Returns: RAX = new pointer, or NULL
; ---------------------------------------------------------------------------
RawrLinearAlloc_Realloc proc public
    sub rsp, 40
    mov r9, rdx                 ; dwBytes = new size
    mov r8, rcx                 ; lpMem = old pointer
    mov rdx, 8                  ; HEAP_ZERO_MEMORY
    mov rcx, g_hHeap
    call HeapReAlloc
    add rsp, 40
    ret
RawrLinearAlloc_Realloc endp

; ---------------------------------------------------------------------------
; RawrLinearAlloc_GetStats
;   RCX = pointer to uint64_t[2] buffer {allocCount, freeCount}
; ---------------------------------------------------------------------------
RawrLinearAlloc_GetStats proc public
    mov rax, qword ptr [g_allocCount]
    mov qword ptr [rcx], rax
    mov rax, qword ptr [g_freeCount]
    mov qword ptr [rcx+8], rax
    ret
RawrLinearAlloc_GetStats endp

end
