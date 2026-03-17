;=====================================================================
; asm_memory.asm - x64 MASM Memory Manager (Heap Allocation/Deallocation)
; ZERO-DEPENDENCY DYNAMIC MEMORY MANAGEMENT
;=====================================================================
; Implements efficient heap allocation with:
;  - Hidden metadata (size, alignment, magic marker)
;  - Configurable alignment (16, 32, 64 bytes for SIMD)
;  - Fragmentation tracking
;  - Thread-safe operations (via mutex in calling code)
;
; Metadata Layout:
;   [Offset -32] Magic       (qword) = 0xDEADBEEFCAFEBABE
;   [Offset -24] Alignment   (qword)
;   [Offset -16] Requested   (qword) - user-requested size
;   [Offset -8]  Total       (qword) - total allocated (metadata + data + padding)
;   [Offset  0]  USER DATA   <- pointer returned to caller
;
;=====================================================================

.data

; Global allocations tracking (statistics)
; These are data section variables for thread-safe atomic updates
; In production, protect with a mutex
PUBLIC g_total_allocations, g_total_bytes, g_alloc_count, g_magic_failed, g_process_heap_handle

g_total_allocations QWORD 0    ; Total allocation count
g_total_bytes       QWORD 0    ; Total bytes allocated (including metadata)
g_alloc_count       QWORD 0    ; Current live allocation count
g_magic_failed      QWORD 0    ; Magic marker validation failures
g_process_heap_handle QWORD 0  ; Storage for heap handle (initialized at startup)

; Logging messages
msg_malloc_enter    DB "ALLOC enter",0
msg_malloc_fail     DB "ALLOC fail",0
msg_malloc_exit     DB "ALLOC exit",0
msg_free_enter      DB "FREE enter",0
msg_free_invalid    DB "FREE invalid magic",0
msg_free_exit       DB "FREE exit",0
msg_realloc_enter   DB "REALLOC enter",0
msg_realloc_fail    DB "REALLOC fail",0
msg_realloc_exit    DB "REALLOC exit",0
dbg_heapfree_fail   DB "[dbg] HeapFree failed",13,10,0

; New logging strings for function calls
szMallocStart       BYTE "Memory allocation starting.", 0
szMallocSuccess     BYTE "Memory allocation successful.", 0
szMallocFailed      BYTE "Memory allocation failed.", 0


; Logging
EXTERN asm_log:PROC

.code

; External Win32 heap APIs
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; ============================================================================
; Integration with Zero-Day Agentic Engine (master include)
; Provides access to:
;   - All ZeroDayAgenticEngine_* functions
;   - All mission state constants (MISSION_STATE_*)
;   - All signal types (SIGNAL_TYPE_*)
;   - Structured logging framework (LOG_LEVEL_*)
;   - Complexity levels (COMPLEXITY_*)
; ============================================================================
include d:\RawrXD-production-lazy-init\masm\masm_master_include.asm

; Export all public allocator functions
PUBLIC asm_malloc, asm_free, asm_realloc, asm_memcpy
PUBLIC asm_get_process_heap, asm_heap_alloc, asm_heap_free
PUBLIC asm_memory_stats

;=====================================================================
; asm_malloc(size: rcx, alignment: rdx) -> rax
;
; Allocates memory with specified alignment and metadata.
; If alignment < 16, defaults to 16.
; If size == 0, returns NULL.
;
; Returns:
;   rax = pointer to user data (aligned)
;   NULL if allocation failed
;=====================================================================

ALIGN 16
asm_malloc PROC

    ; Prologue: preserve non-volatile registers
    push rbx
    push r12
    sub rsp, 40             ; 32-byte shadow space + 8 for alignment (Total = 56, aligned)

    ; ============================================================================
    ; Log memory allocation start
    ; ============================================================================
    LEA rcx, [rel szMallocStart]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured

    ; Cache inputs (commenting out logging for now to avoid crashes)
    mov r12, rcx            ; Save original size
    mov rbx, rdx            ; Save alignment
    ; lea rcx, msg_malloc_enter
    ; call asm_log
    
    ; Input validation
    mov rcx, r12
    test rcx, rcx           ; size == 0?
    jz malloc_fail
    
    ; Normalize alignment
    mov rdx, rbx
    cmp rdx, 16
    jge malloc_alignment_ok
    mov rdx, 16             ; Minimum 16-byte alignment
    mov rbx, rdx
    
malloc_alignment_ok:
    ; Ensure alignment is power of 2 (2, 4, 8, 16, 32, 64...)
    ; For now, assume caller provides valid alignment
    
    ; Calculate total allocation:
    ; total = 32 (metadata) + size + (alignment - 1) for alignment padding
    ; = 32 + size + alignment - 1
    mov rax, rdx
    sub rax, 1
    add rax, rcx            ; rax = size + alignment - 1
    add rax, 32             ; rax = 32 + size + alignment - 1
    
    ; Call HeapAlloc(GetProcessHeap(), 0, size)
    ; GetProcessHeap() returns handle in rax
    call asm_get_process_heap  ; rax = heap handle
    mov rcx, rax            ; rcx = heap handle
    
    ; Calculate total bytes: size + alignment + 31 (includes metadata padding)
    mov rax, r12            ; Restore size
    add rax, rbx            ; size + alignment
    add rax, 31             ; size + alignment + 31
    mov rdx, rax            ; rdx = total bytes to allocate (passed to wrapper)
    
    ; HeapAlloc(heap, 0, bytes)
    call asm_heap_alloc     ; rax = allocated pointer or NULL
    test rax, rax
    jz malloc_fail
    
    ; rax now points to raw allocation (not yet aligned)
    mov rcx, rax            ; rcx = raw pointer
    
    ; Align the user data pointer
    ; Calculate: aligned_ptr = (raw + 32 + (alignment - 1)) & ~(alignment - 1)
    ; This ensures metadata is before the aligned user data
    
    mov rax, rcx            ; rax = raw pointer
    add rax, 32             ; Skip metadata
    
    ; Alignment calculation: (ptr + alignment - 1) & ~(alignment - 1)
    mov r8, rbx             ; r8 = alignment
    sub r8, 1               ; r8 = alignment - 1 (mask)
    add rax, r8             ; rax += alignment - 1
    not r8                  ; r8 = ~(alignment - 1)
    and rax, r8             ; Clear low bits to align
    
    ; Now rax points to aligned user data
    ; Write metadata at [rax - 32]
    
    mov r8, rax             ; r8 = aligned user data pointer
    sub r8, 32              ; r8 = metadata start
    
    ; Write magic marker (use movabs for 64-bit immediate)
    mov r9, 0CAFEBABEh
    shl r9, 32
    or r9, 0DEADBEEFh
    mov [r8], r9
    
    ; Write alignment
    mov [r8 + 8], rbx
    
    ; Write requested size
    mov [r8 + 16], r12
    
    ; Store raw pointer for accurate free
    mov [r8 + 24], rcx
    
    ; ============================================================================
    ; PHASE 3: Update statistics only in development mode
    ; ============================================================================
    PUSH rax                ; Save aligned pointer
    CALL Config_IsProduction
    TEST rax, rax
    POP rax                 ; Restore aligned pointer
    JNZ skip_memory_stats   ; Skip stats in production
    
    ; Update global statistics (NOT thread-safe, requires external mutex)
    lock add [g_total_allocations], 1
    mov r9, rax
    sub r9, rcx             ; r9 = aligned_user - raw
    add r9, r12             ; r9 = total bytes
    lock add [g_total_bytes], r9
    lock add [g_alloc_count], 1
    
skip_memory_stats:
    
    ; ============================================================================
    ; Log memory allocation success
    ; ============================================================================
    LEA rcx, [rel szMallocSuccess]
    MOV rdx, LOG_LEVEL_SUCCESS
    CALL Logger_LogStructured
    
    ; Return aligned pointer
    mov rax, r8
    add rax, 32             ; Return pointer to user data
    ; lea rcx, msg_malloc_exit
    ; call asm_log
    jmp malloc_done
    
malloc_fail:
    ; ============================================================================
    ; Log memory allocation failure
    ; ============================================================================
    LEA rcx, [rel szMallocFailed]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    
    xor rax, rax            ; Return NULL
    ; lea rcx, msg_malloc_fail
    ; call asm_log
    
malloc_done:
    add rsp, 40
    pop r12
    pop rbx
    ret

asm_malloc ENDP

;=====================================================================
; asm_free(ptr: rcx) -> void
;
; Frees allocated memory, validating magic marker.
; Safe to call with NULL pointer.
;=====================================================================

ALIGN 16
asm_free PROC

    ; Prologue
    push rbx
    push r12
    sub rsp, 40             ; align stack for WinAPI calls (Total = 56)

    mov r11, rcx            ; Preserve user pointer
    ; lea rcx, msg_free_enter
    ; call asm_log
    
    ; Check for NULL
    test r11, r11
    jz free_done
    
    ; Retrieve metadata (32 bytes before user data)
    mov rax, r11
    sub rax, 32
    
    ; Validate magic marker
    mov rdx, [rax]
    mov r10, 0CAFEBABEh
    shl r10, 32
    or r10, 0DEADBEEFh
    cmp rdx, r10
    jne free_invalid_magic
    
    ; Valid allocation, extract info
    mov rdx, [rax + 24]     ; rdx = raw allocation pointer
    mov r12, [rax + 16]     ; r12 = requested size
    mov rbx, [rax + 8]      ; rbx = alignment
    
    ; Acquire heap handle
    call asm_get_process_heap
    mov rbx, rax            ; rbx = heap handle
    
    ; Raw pointer stored in metadata at +24
    mov r9, rdx             ; r9 = raw allocation start
    
    ; HeapFree(heap, 0, raw_ptr)
    mov rcx, rbx            ; rcx = heap handle
    xor rdx, rdx            ; rdx = flags
    mov r8, r9              ; r8 = pointer to free
    call asm_heap_free
    
    ; Update statistics
    ; Update statistics: total = (user_ptr - raw_ptr) + requested_size
    mov r10, r11            ; r10 = user data pointer
    sub r10, r9             ; offset between raw and user
    add r10, r12            ; total bytes
    lock sub [g_total_bytes], r10
    lock sub [g_alloc_count], 1
    ; lea rcx, msg_free_exit
    ; call asm_log
    
     ; Check HeapFree result
     test rax, rax
     jnz free_heap_ok
     lea rcx, dbg_heapfree_fail
     call asm_log

free_heap_ok:
    jmp free_done
    
free_invalid_magic:
    ; Magic marker validation failed
    lock add [g_magic_failed], 1
    ; lea rcx, msg_free_invalid
    ; call asm_log
    ; In production, log error or raise exception
    ; For now, silently ignore (or assert)
    
free_done:
    add rsp, 40
    pop r12
    pop rbx
    ret

asm_free ENDP

;=====================================================================
; asm_realloc(ptr: rcx, new_size: rdx) -> rax
;
; Reallocates existing memory to new size.
; If ptr is NULL, behaves like malloc.
; If new_size is 0, behaves like free and returns NULL.
;
; Returns:
;   rax = pointer to new allocation (may be different from input)
;   NULL if realloc failed (old pointer still valid)
;=====================================================================

ALIGN 16
asm_realloc PROC

    ; Prologue
    push rbx
    push r12
    sub rsp, 40             ; align stack for WinAPI calls (Total = 56)
    
    mov r12, rcx            ; r12 = original pointer
    mov rbx, rdx            ; rbx = new_size
     ; lea rcx, msg_realloc_enter
     ; call asm_log
    
    ; Handle special cases
    test r12, r12           ; ptr == NULL?
    jz realloc_as_malloc
    
    test rbx, rbx           ; new_size == 0?
    jz realloc_as_free
    
    ; Normal reallocation: allocate new, copy, free old
    
    ; Get metadata from old allocation
    mov rcx, r12
    sub rcx, 32
    
    ; Validate magic
    mov rax, [rcx]
    mov r10, 0CAFEBABEh
    shl r10, 32
    or r10, 0DEADBEEFh
    cmp rax, r10
    jne realloc_invalid
    
    ; Get old size
    mov rax, [rcx + 16]     ; rax = old requested size
    
    ; Allocate new block (use same alignment as before)
    mov r8, [rcx + 8]       ; r8 = old alignment
    mov rcx, rbx            ; rcx = new_size
    mov rdx, r8             ; rdx = alignment
    
    call asm_malloc         ; rax = new pointer
    test rax, rax
    jz realloc_nomem
    
    ; Copy old data to new (min of old and new size)
    mov rcx, r12            ; rcx = source (old data)
    mov rdx, rax            ; rdx = destination (new data)
    mov r8, [g_total_bytes]  ; Use conservative copy size
    
    ; Actually use the old requested size
    mov r9, [r12 - 32 + 16]  ; Old requested size
    cmp r9, rbx
    jle realloc_copy_size_ok
    mov r9, rbx
    
realloc_copy_size_ok:
    mov r8, r9              ; r8 = bytes to copy
    
    ; memcpy: copy r8 bytes from rcx to rdx
    push rax                ; Save new pointer
    mov rax, rcx            ; rax = source
    mov rcx, rdx            ; rcx = destination
    mov rdx, r8             ; rdx = count
    call asm_memcpy
    pop rax                 ; rax = new pointer
    
    ; Free old allocation
    push rax                ; preserve new pointer across free
    mov rcx, r12
    call asm_free
    pop rax                 ; restore new pointer
    
    ; Return new pointer
    jmp realloc_done
    
realloc_as_malloc:
    mov rcx, rbx            ; rcx = size
    mov rdx, 16             ; rdx = default alignment
    call asm_malloc
    jmp realloc_done
    
realloc_as_free:
    mov rcx, r12
    call asm_free
    xor rax, rax
    jmp realloc_done
    
realloc_invalid:
realloc_nomem:
    xor rax, rax
     ; lea rcx, msg_realloc_fail
     ; call asm_log
    
realloc_done:
     ; lea rcx, msg_realloc_exit
     ; call asm_log
    add rsp, 40
    pop r12
    pop rbx
    ret

asm_realloc ENDP

;=====================================================================
; asm_memcpy(rcx = src, rdx = dst, r8 = count) -> void
;
; Copies r8 bytes from rcx to rdx.
; Assumes non-overlapping regions.
;=====================================================================

ALIGN 16
asm_memcpy PROC

    ; Simple implementation: copy 8 bytes at a time
    test r8, r8
    jz memcpy_done
    
    mov r9, r8
    shr r9, 3               ; r9 = count / 8 (qwords)
    
memcpy_loop:
    test r9, r9
    jz memcpy_remainder
    
    mov r10, [rcx]          ; Load qword from source
    mov [rdx], r10          ; Store to destination
    
    add rcx, 8
    add rdx, 8
    dec r9
    jnz memcpy_loop
    
memcpy_remainder:
    ; Handle remaining bytes (0-7)
    mov r9, r8
    and r9, 7               ; r9 = count % 8
    jz memcpy_done
    
memcpy_byte_loop:
    mov r10b, [rcx]
    mov [rdx], r10b
    inc rcx
    inc rdx
    dec r9
    jnz memcpy_byte_loop
    
memcpy_done:
    ret

asm_memcpy ENDP

;=====================================================================
; asm_get_process_heap() -> rax
;
; Returns the current process heap handle.
; Calls Windows API GetProcessHeap().
;=====================================================================

ALIGN 16
asm_get_process_heap PROC

    ; GetProcessHeap is in kernel32.dll
    ; For x64 calling convention, we need to set up the call
    
    ; This is a simple wrapper around the Win32 API
    ; In a real implementation, we'd use the IAT or load it dynamically
    
    ; For MVP, assume kernel32.GetProcessHeap is already loaded
    ; In production, you'd use LoadLibraryA + GetProcAddress
    
    ; Return cached heap handle if initialized
    mov rax, [g_process_heap_handle]
    test rax, rax
    jnz heap_ok

    ; Initialize by calling Win32 GetProcessHeap
    sub rsp, 40             ; 32-byte shadow space + 8 for alignment
    call GetProcessHeap
    add rsp, 40
    mov [g_process_heap_handle], rax

heap_ok:
    ret

asm_get_process_heap ENDP

;=====================================================================
; asm_heap_alloc(rcx = heap, rdx = flags, r8 = size) -> rax
;
; Wrapper around Win32 HeapAlloc API.
; Returns pointer to allocated block or NULL.
;=====================================================================

ALIGN 16
asm_heap_alloc PROC

    ; Call HeapAlloc(heap_handle, flags, size)
    ; This would be hooked to actual Win32 API
    ; For MVP, use placeholder
    
    ; In real implementation:
    ; extern "C" { void* __stdcall HeapAlloc(void* heap, unsigned long flags, size_t size); }
    
    ; rcx = heap_handle, rdx = size (from caller), use flags=0
    sub rsp, 40             ; ensure 16-byte alignment (8 + 40 = 48)
    mov r8, rdx             ; r8 = size
    xor rdx, rdx            ; rdx = flags
    call HeapAlloc
    add rsp, 40
    ret

asm_heap_alloc ENDP

;=====================================================================
; asm_heap_free(rcx = heap, rdx = flags, r8 = ptr) -> void
;
; Wrapper around Win32 HeapFree API.
;=====================================================================

ALIGN 16
asm_heap_free PROC

    ; rcx = heap_handle, rdx = flags (0), r8 = ptr
    sub rsp, 40             ; ensure 16-byte alignment (8 + 40 = 48)
    call HeapFree
    add rsp, 40
    ret

asm_heap_free ENDP

;=====================================================================
; Exported Statistics Functions
;=====================================================================

ALIGN 16
asm_memory_stats PROC

    ; Return structure address with statistics
    ; In production, allocate and return stats struct
    
    ; For MVP, return aggregate count in rax
    mov rax, [g_alloc_count]
    ret

asm_memory_stats ENDP

;=====================================================================
; asm_memcpy_avx512(rcx = dest, rdx = src, r8 = size) -> rax (dest)
;
; Ultra-fast memory copy using AVX-512 (64-byte blocks)
;=====================================================================
PUBLIC asm_memcpy_avx512
ALIGN 16
asm_memcpy_avx512 PROC
    push rdi
    push rsi
    
    mov rdi, rcx        ; dest
    mov rsi, rdx        ; src
    mov rcx, r8         ; size
    mov rax, rdi        ; save dest for return
    
    ; Check if size >= 64
    cmp rcx, 64
    jb @small_copy
    
@avx512_loop:
    cmp rcx, 64
    jb @remainder
    
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    
    add rsi, 64
    add rdi, 64
    sub rcx, 64
    jmp @avx512_loop
    
@remainder:
    test rcx, rcx
    jz @done
    
@small_copy:
    rep movsb
    
@done:
    pop rsi
    pop rdi
    ret
asm_memcpy_avx512 ENDP

;=====================================================================
; asm_memcpy_fast(rcx = dest, rdx = src, r8 = size) -> rax (dest)
;
; Fast memory copy using SIMD when possible
;=====================================================================
PUBLIC asm_memcpy_fast
ALIGN 16
asm_memcpy_fast PROC
    push rdi
    push rsi
    
    mov rdi, rcx        ; dest
    mov rsi, rdx        ; src
    mov rcx, r8         ; size
    mov rax, rdi        ; save dest for return
    
    ; Check if size >= 16 for SIMD
    cmp rcx, 16
    jb small_copy
    
    ; Align check for SSE (16-byte)
    test rdi, 0Fh
    jnz small_copy
    test rsi, 0Fh
    jnz small_copy
    
simd_copy:
    ; Copy 16 bytes at a time
    cmp rcx, 16
    jb copy_remainder
    
    movdqu xmm0, [rsi]
    movdqu [rdi], xmm0
    add rsi, 16
    add rdi, 16
    sub rcx, 16
    jmp simd_copy
    
copy_remainder:
    test rcx, rcx
    jz done
    
small_copy:
    ; Byte-by-byte copy
    rep movsb
    
done:
    pop rsi
    pop rdi
    ret
asm_memcpy_fast ENDP

END
