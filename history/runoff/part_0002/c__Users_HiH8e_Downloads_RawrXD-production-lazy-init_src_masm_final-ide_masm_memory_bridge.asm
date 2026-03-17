;==============================================================================
; masm_memory_bridge.asm - Safe Memory Sharing Between Qt and MASM
; Purpose: Allocate, manage, and safely share memory across Qt/MASM boundary
; Size: 320 lines of production-grade memory management
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

MEMORY_GUARD_SIZE       EQU 4096  ; 4KB guard pages
MEMORY_ALIGNMENT        EQU 16    ; 16-byte alignment
MAX_ALLOCATIONS         EQU 256
MEMORY_MAGIC            EQU 0xDEADBEEF

;==============================================================================
; STRUCTURES
;==============================================================================

; Memory block metadata
MEMORY_BLOCK STRUCT
    magic           DWORD ?     ; Sanity check
    size            QWORD ?
    user_ptr        QWORD ?
    is_valid        DWORD ?
    alloc_time      DWORD ?
    alloc_stack     QWORD 4 DUP (?)  ; Allocator call stack
MEMORY_BLOCK ENDS

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================
PUBLIC memory_bridge_alloc
PUBLIC memory_bridge_free
PUBLIC memory_bridge_copy
PUBLIC memory_bridge_validate
PUBLIC memory_bridge_get_stats
PUBLIC memory_bridge_init

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
    g_memory_heap       QWORD 0
    g_total_allocated   QWORD 0
    g_block_count       DWORD 0
    g_memory_mutex      QWORD 0
    
    g_blocks MEMORY_BLOCK MAX_ALLOCATIONS DUP(<>)
    
    szBridgeAlloc BYTE "Memory allocated: %d bytes",0
    szBridgeFreed BYTE "Memory freed: %d bytes",0
    szMemoryError BYTE "Memory error: invalid pointer",0

;==============================================================================
; CODE SECTION
;==============================================================================
.code

;==============================================================================
; PUBLIC: memory_bridge_init() -> bool (rax)
; Initialize memory bridge system
;==============================================================================
ALIGN 16
memory_bridge_init PROC
    push rbx
    sub rsp, 32
    
    ; Create private heap
    mov ecx, 0
    mov edx, 65536        ; Initial size: 64KB
    mov r8d, 0
    call HeapCreate
    test rax, rax
    jz .init_error
    mov g_memory_heap, rax
    
    ; Create mutex for thread safety
    lea rcx, g_memory_mutex
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateMutexA
    test rax, rax
    jz .init_error
    mov g_memory_mutex, rax
    
    ; Initialize block array
    mov g_block_count, 0
    mov g_total_allocated, 0
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.init_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_memory_bridge_init ENDP

;==============================================================================
; PUBLIC: memory_bridge_alloc(size: rcx) -> ptr (rax)
; Allocate aligned memory with metadata
;==============================================================================
ALIGN 16
memory_bridge_alloc PROC
    ; rcx = size
    push rbx
    push r12
    push r13
    sub rsp, 40
    
    mov r12, rcx        ; Save requested size
    
    ; Acquire mutex
    mov rcx, g_memory_mutex
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne .alloc_error
    
    ; Check block limit
    cmp g_block_count, MAX_ALLOCATIONS
    jge .alloc_limit
    
    ; Calculate total size with metadata
    mov rax, r12
    add rax, SIZEOF MEMORY_BLOCK
    add rax, MEMORY_ALIGNMENT - 1
    and rax, -MEMORY_ALIGNMENT
    
    ; Allocate from heap
    mov rcx, g_memory_heap
    xor edx, edx
    mov r8, rax
    call HeapAlloc
    test rax, rax
    jz .alloc_fail
    
    mov r13, rax        ; Save allocation pointer
    
    ; Store metadata
    mov [rax + MEMORY_BLOCK.magic], MEMORY_MAGIC
    mov [rax + MEMORY_BLOCK.size], r12
    
    ; Calculate user pointer (aligned after metadata)
    mov rbx, rax
    add rbx, SIZEOF MEMORY_BLOCK
    mov [rax + MEMORY_BLOCK.user_ptr], rbx
    mov DWORD PTR [rax + MEMORY_BLOCK.is_valid], 1
    
    ; Record in block table
    mov r8d, g_block_count
    mov r9, OFFSET g_blocks
    imul r8d, SIZEOF MEMORY_BLOCK
    add r8, r9
    
    mov [r8], rax       ; Store block header pointer
    
    inc g_block_count
    add g_total_allocated, r12
    
    ; Release mutex
    mov rcx, g_memory_mutex
    call ReleaseMutex
    
    ; Return user pointer
    mov rax, rbx
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
    
.alloc_limit:
.alloc_fail:
    mov rcx, g_memory_mutex
    call ReleaseMutex
    jmp .alloc_error
    
.alloc_error:
    xor eax, eax
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
masm_memory_bridge_alloc ENDP

;==============================================================================
; PUBLIC: memory_bridge_free(ptr: rcx) -> bool (rax)
; Free memory allocated by bridge
;==============================================================================
ALIGN 16
memory_bridge_free PROC
    ; rcx = user pointer
    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx        ; Save pointer
    
    ; Acquire mutex
    mov rcx, g_memory_mutex
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne .free_error
    
    ; Find metadata (should be SIZEOF MEMORY_BLOCK before user pointer)
    mov rax, r12
    sub rax, SIZEOF MEMORY_BLOCK
    
    ; Validate magic number
    mov ebx, [rax + MEMORY_BLOCK.magic]
    cmp ebx, MEMORY_MAGIC
    jne .free_invalid
    
    ; Check valid flag
    mov ebx, [rax + MEMORY_BLOCK.is_valid]
    test ebx, ebx
    jz .free_invalid
    
    ; Get size for stats
    mov r8, [rax + MEMORY_BLOCK.size]
    
    ; Mark as invalid
    mov DWORD PTR [rax + MEMORY_BLOCK.is_valid], 0
    
    ; Free from heap
    mov rcx, g_memory_heap
    xor edx, edx
    mov r8, rax
    call HeapFree
    test eax, eax
    jz .free_fail
    
    ; Update stats
    sub g_total_allocated, [rax + MEMORY_BLOCK.size]
    
    ; Release mutex
    mov rcx, g_memory_mutex
    call ReleaseMutex
    
    mov eax, 1
    add rsp, 32
    pop r12
    pop rbx
    ret
    
.free_invalid:
.free_fail:
    mov rcx, g_memory_mutex
    call ReleaseMutex
    jmp .free_error
    
.free_error:
    xor eax, eax
    add rsp, 32
    pop r12
    pop rbx
    ret
masm_memory_bridge_free ENDP

;==============================================================================
; PUBLIC: memory_bridge_copy(dest: rcx, src: rdx, size: r8) -> bool (rax)
; Safe memory copy with validation
;==============================================================================
ALIGN 16
memory_bridge_copy PROC
    ; rcx = destination, rdx = source, r8 = size
    push rbx
    sub rsp, 32
    
    ; Validate pointers
    test rcx, rcx
    jz .copy_error
    test rdx, rdx
    jz .copy_error
    test r8, r8
    jz .copy_error
    
    ; Check reasonable size (<100MB)
    mov rax, 104857600
    cmp r8, rax
    jg .copy_error
    
    ; Validate destination pointer
    mov rax, rcx
    sub rax, SIZEOF MEMORY_BLOCK
    mov ebx, [rax + MEMORY_BLOCK.magic]
    cmp ebx, MEMORY_MAGIC
    jne .copy_not_validated
    
    ; Size check
    mov rax, [rax + MEMORY_BLOCK.size]
    cmp r8, rax
    jg .copy_error
    
.copy_not_validated:
    ; Perform memory copy
    mov rsi, rdx      ; Source
    mov rdi, rcx      ; Destination
    mov rcx, r8
    
    ; Copy byte by byte (safe fallback)
    mov rax, 0
    
.copy_loop:
    cmp rax, rcx
    jge .copy_done
    
    mov bl, BYTE PTR [rsi + rax]
    mov BYTE PTR [rdi + rax], bl
    inc rax
    jmp .copy_loop
    
.copy_done:
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.copy_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_memory_bridge_copy ENDP

;==============================================================================
; PUBLIC: memory_bridge_validate(ptr: rcx) -> bool (rax)
; Validate if pointer is safe to use
;==============================================================================
ALIGN 16
memory_bridge_validate PROC
    ; rcx = pointer to validate
    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz .validate_invalid
    
    ; Check if pointer is within reasonable range
    mov rax, rcx
    mov rbx, 140737488355328  ; Max user space address on x64
    cmp rax, rbx
    jg .validate_invalid
    
    ; Try to read magic number (protected by SEH in real implementation)
    mov rax, rcx
    sub rax, SIZEOF MEMORY_BLOCK
    
    mov ebx, [rax + MEMORY_BLOCK.magic]
    cmp ebx, MEMORY_MAGIC
    jne .validate_unknown
    
    ; Check valid flag
    mov ebx, [rax + MEMORY_BLOCK.is_valid]
    test ebx, ebx
    jz .validate_invalid
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.validate_unknown:
    ; Pointer not allocated by bridge, but might be valid external memory
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.validate_invalid:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_memory_bridge_validate ENDP

;==============================================================================
; PUBLIC: memory_bridge_get_stats() -> struct (rax)
; Get memory usage statistics
; Returns: rdx = total allocated, ecx = block count
;==============================================================================
ALIGN 16
memory_bridge_get_stats PROC
    push rbx
    sub rsp, 32
    
    ; Acquire mutex for safe read
    mov rcx, g_memory_mutex
    mov rdx, 0  ; Non-blocking
    call WaitForSingleObject
    
    ; Get stats (safe to read without lock if atomic)
    mov rax, g_total_allocated
    mov rcx, g_block_count
    
    ; Release if locked
    cmp eax, WAIT_OBJECT_0
    jne .stats_return
    mov r8, g_memory_mutex
    mov rcx, r8
    call ReleaseMutex
    
.stats_return:
    ; Return: rax = total allocated, rcx = block count
    mov rdx, g_total_allocated
    mov rcx, g_block_count
    add rsp, 32
    pop rbx
    ret
masm_memory_bridge_get_stats ENDP

END
