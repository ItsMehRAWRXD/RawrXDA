; ============================================================================
; asm_memory_x64.asm - Memory Management for x64
; ============================================================================

option casemap:none

extern HeapCreate:proc
extern HeapAlloc:proc
extern HeapFree:proc
extern GetProcessHeap:proc

.data
    heap_handle dq 0

.code

; ============================================================================
; memory_init - Initialize memory system
; ============================================================================
public memory_init
memory_init proc
    push rbx
    sub rsp, 32
    
    ; Get process heap
    call GetProcessHeap
    
    lea rbx, [heap_handle]
    mov [rbx], rax
    
    add rsp, 32
    pop rbx
    ret
memory_init endp

; ============================================================================
; asm_malloc - Allocate memory
; rcx = size
; Returns: pointer in rax
; ============================================================================
public asm_malloc
asm_malloc proc
    push rbx
    sub rsp, 32
    
    mov r8, rcx         ; r8 = size
    
    ; Get heap handle
    lea rbx, [heap_handle]
    mov rcx, [rbx]
    cmp rcx, 0
    jne malloc_have_heap
    
    ; Initialize if not done
    call memory_init
    lea rbx, [heap_handle]
    mov rcx, [rbx]

malloc_have_heap:
    ; HeapAlloc(heap, 0, size)
    mov rdx, 0          ; flags = 0
    mov r8, r8          ; size
    call HeapAlloc
    
    add rsp, 32
    pop rbx
    ret
asm_malloc endp

; ============================================================================
; asm_free - Free memory
; rcx = pointer
; ============================================================================
public asm_free
asm_free proc
    push rbx
    sub rsp, 32
    
    mov r8, rcx         ; r8 = pointer
    
    ; Get heap handle
    lea rbx, [heap_handle]
    mov rcx, [rbx]
    mov rdx, 0          ; flags = 0
    call HeapFree
    
    add rsp, 32
    pop rbx
    ret
asm_free endp

; ============================================================================
; asm_memcpy - Copy memory
; rcx = destination
; rdx = source
; r8 = size
; ============================================================================
public asm_memcpy
asm_memcpy proc
    push rdi
    push rsi
    
    mov rdi, rcx        ; rdi = destination
    mov rsi, rdx        ; rsi = source
    mov rcx, r8         ; rcx = size
    
    cmp rcx, 0
    je memcpy_done
    
memcpy_loop:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jnz memcpy_loop

memcpy_done:
    pop rsi
    pop rdi
    ret
asm_memcpy endp

; ============================================================================
; asm_memset - Fill memory
; rcx = pointer
; rdx = value
; r8 = size
; ============================================================================
public asm_memset
asm_memset proc
    push rdi
    
    mov rdi, rcx        ; rdi = pointer
    mov rax, rdx        ; rax = value
    mov rcx, r8         ; rcx = size
    
    cmp rcx, 0
    je memset_done
    
memset_loop:
    mov [rdi], al
    inc rdi
    dec rcx
    jnz memset_loop

memset_done:
    pop rdi
    ret
asm_memset endp

end
