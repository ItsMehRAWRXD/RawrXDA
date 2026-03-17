;=====================================================================
; asm_hotpatch_integration.asm - Pure MASM Integration Layer
; REPLACES Qt/C++ with Direct x64 MASM Implementation
;
; This module provides:
;  1. Hotpatching functionality (direct memory manipulation)
;  2. Integration of memory, sync, strings, events layers
;  3. Initialization of Win32 APIs (GetProcessHeap, etc.)
;  4. Main entry point for pure MASM execution
;
; Calling convention: Microsoft x64
;=====================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreateEventExW:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN GetCurrentThread:PROC
EXTERN GetCurrentThreadId:PROC
EXTERN CreateThread:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN Sleep:PROC

; Runtime exported functions
PUBLIC asm_malloc
PUBLIC asm_free
PUBLIC asm_realloc
PUBLIC asm_mutex_create
PUBLIC asm_mutex_lock
PUBLIC asm_mutex_unlock
PUBLIC asm_event_create
PUBLIC asm_event_set
PUBLIC asm_str_create
PUBLIC asm_str_destroy
PUBLIC asm_str_length
PUBLIC asm_event_loop_create
PUBLIC asm_event_loop_emit
PUBLIC asm_event_loop_process_one
PUBLIC hotpatch_apply
PUBLIC hotpatch_initialize
PUBLIC hotpatch_main

.data

; Global process heap (cached on first use)
g_process_heap QWORD 0

; Hotpatch statistics
g_patches_applied QWORD 0
g_hotpatch_errors QWORD 0
g_memory_protected QWORD 0

; Win32 memory protection constants
VIRTUAL_PROTECT_RO = 02h           ; PAGE_READONLY
VIRTUAL_PROTECT_RW = 04h           ; PAGE_READWRITE

.code

;=====================================================================
; hotpatch_initialize() -> void
;
; Initializes the runtime:
;  - Caches process heap handle
;  - Sets up Win32 API bindings
;  - Initializes global statistics
;
; Must be called once before using any runtime functions.
;=====================================================================

ALIGN 16
hotpatch_initialize PROC

    push rbx
    sub rsp, 32
    
    ; Get and cache process heap
    call GetProcessHeap
    mov g_process_heap, rax
    
    test rax, rax
    jz init_fail
    
    ; Initialize statistics
    mov g_patches_applied, 0
    mov g_hotpatch_errors, 0
    mov g_memory_protected, 0
    
    ; Success
    xor rax, rax
    jmp init_done
    
init_fail:
    mov rax, -1
    
init_done:
    add rsp, 32
    pop rbx
    ret

hotpatch_initialize ENDP

;=====================================================================
; asm_malloc(size: rcx, alignment: rdx) -> rax
;
; ACTUAL Win32 HeapAlloc wrapper (not placeholder)
;=====================================================================

ALIGN 16
asm_malloc PROC

    push rbx
    push r12
    sub rsp, 48             ; 32 shadow + 16 for locals
    
    mov r12, rcx            ; r12 = requested size
    mov rbx, rdx            ; rbx = alignment
    
    test r12, r12
    jz malloc_fail
    
    ; Normalize alignment
    cmp rbx, 16
    jge malloc_alignment_ok
    mov rbx, 16
    
malloc_alignment_ok:
    ; Calculate total: metadata (32) + size + alignment padding
    mov rax, r12
    add rax, rbx
    add rax, 32
    
    ; HeapAlloc(heap, 0, size)
    mov rcx, [rel g_process_heap]
    test rcx, rcx
    jz malloc_fail
    
    xor rdx, rdx            ; flags = 0
    mov r8, rax             ; r8 = allocation size
    
    call HeapAlloc
    
    test rax, rax
    jz malloc_fail
    
    ; Calculate aligned pointer
    mov r9, rax             ; r9 = raw allocation
    add rax, 32             ; Skip metadata
    
    ; Align: (ptr + alignment - 1) & ~(alignment - 1)
    mov r10, rbx
    sub r10, 1
    add rax, r10
    not r10
    and rax, r10            ; rax = aligned pointer
    
    ; Write metadata (40 bytes before aligned pointer)
    mov r10, rax
    sub r10, 32             ; r10 = metadata location
    
    ; Magic marker
    mov rax, 0DEADBEEFCAFEBABEh
    mov qword ptr [r10 + 0], rax
    mov qword ptr [r10 + 8], rbx   ; alignment
    mov qword ptr [r10 + 16], r12  ; requested size
    
    ; Store raw pointer for later recovery
    mov qword ptr [r10 + 24], r9   ; raw allocation pointer
    
    ; Update stats
    lock add [rel g_memory_protected], 1
    
    jmp malloc_done
    
malloc_fail:
    xor rax, rax
    lock add [rel g_hotpatch_errors], 1
    
malloc_done:
    add rsp, 48
    pop r12
    pop rbx
    ret

asm_malloc ENDP

;=====================================================================
; asm_free(ptr: rcx) -> void
;
; Validates metadata and calls HeapFree on raw allocation.
;=====================================================================

ALIGN 16
asm_free PROC

    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz free_done
    
    ; Validate metadata
    mov rax, rcx
    sub rax, 32
    
    mov rdx, [rax]
    mov r8, 0DEADBEEFCAFEBABEh
    cmp rdx, r8
    jne free_invalid
    
    ; Get raw pointer
    mov r8, [rax + 24]
    
    ; HeapFree(heap, 0, raw_ptr)
    mov rcx, [rel g_process_heap]
    xor rdx, rdx            ; flags = 0
    call HeapFree
    
    lock sub [rel g_memory_protected], 1
    jmp free_done
    
free_invalid:
    lock add [rel g_hotpatch_errors], 1
    
free_done:
    add rsp, 32
    pop rbx
    ret

asm_free ENDP

;=====================================================================
; asm_realloc(ptr: rcx, new_size: rdx) -> rax
;
; Reallocates by allocating new, copying, freeing old.
;=====================================================================

ALIGN 16
asm_realloc PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx            ; r12 = old pointer
    mov r13, rdx            ; r13 = new size
    
    test r12, r12
    jz realloc_as_malloc
    
    test r13, r13
    jz realloc_as_free
    
    ; Get old size
    mov rax, r12
    sub rax, 32
    mov rbx, [rax + 16]     ; rbx = old requested size
    
    ; Allocate new
    mov rcx, r13
    mov rdx, 16             ; default alignment
    call asm_malloc
    
    test rax, rax
    jz realloc_fail
    
    ; Copy old data
    mov rcx, r12
    mov rdx, rax
    mov r8, rbx
    cmp r8, r13
    jle realloc_copy_size_ok
    mov r8, r13
    
realloc_copy_size_ok:
    ; memcpy: rcx=src, rdx=dst, r8=size
    mov r9, 0
    
realloc_copy_loop:
    cmp r9, r8
    jge realloc_copy_done
    
    mov r10b, [rcx + r9]
    mov [rdx + r9], r10b
    inc r9
    jmp realloc_copy_loop
    
realloc_copy_done:
    ; Free old
    mov rcx, r12
    call asm_free
    
    jmp realloc_done
    
realloc_as_malloc:
    mov rcx, r13
    mov rdx, 16
    call asm_malloc
    jmp realloc_done
    
realloc_as_free:
    mov rcx, r12
    call asm_free
    xor rax, rax
    jmp realloc_done
    
realloc_fail:
    xor rax, rax
    
realloc_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

asm_realloc ENDP

;=====================================================================
; asm_mutex_create() -> rax
;
; Creates CRITICAL_SECTION via actual Win32 API.
;=====================================================================

ALIGN 16
asm_mutex_create PROC

    push rbx
    sub rsp, 32
    
    ; Allocate CRITICAL_SECTION (40 bytes)
    mov rcx, 40
    mov rdx, 8
    call asm_malloc
    
    test rax, rax
    jz mutex_create_fail
    
    ; InitializeCriticalSection(rax)
    mov rbx, rax
    mov rcx, rax
    call InitializeCriticalSection
    
    mov rax, rbx
    jmp mutex_create_done
    
mutex_create_fail:
    xor rax, rax
    
mutex_create_done:
    add rsp, 32
    pop rbx
    ret

asm_mutex_create ENDP

;=====================================================================
; asm_mutex_lock(handle: rcx) -> void
;
; EnterCriticalSection wrapper.
;=====================================================================

ALIGN 16
asm_mutex_lock PROC

    sub rsp, 32
    
    test rcx, rcx
    jz mutex_lock_done
    
    call EnterCriticalSection
    
    add rsp, 32
    ret

asm_mutex_lock ENDP

;=====================================================================
; asm_mutex_unlock(handle: rcx) -> void
;
; LeaveCriticalSection wrapper.
;=====================================================================

ALIGN 16
asm_mutex_unlock PROC

    sub rsp, 32
    
    test rcx, rcx
    jz mutex_unlock_done
    
    call LeaveCriticalSection
    
    add rsp, 32
    ret

asm_mutex_unlock ENDP

;=====================================================================
; asm_event_create(manual_reset: rcx) -> rax
;
; Creates event via Win32 CreateEventExW.
;=====================================================================

ALIGN 16
asm_event_create PROC

    push rbx
    sub rsp, 48
    
    mov rbx, rcx            ; rbx = manual_reset flag
    
    ; Allocate event structure (48 bytes)
    mov rcx, 48
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz event_create_fail
    
    mov r12, rax            ; r12 = event structure
    
    ; CreateEventExW(NULL, NULL, flags, initial_state)
    xor rcx, rcx            ; lpName = NULL
    xor rdx, rdx            ; lpEventAttributes = NULL
    
    mov r8d, 0              ; flags = 0 (auto-reset by default)
    test rbx, rbx
    jz event_create_flags_ok
    mov r8d, 1              ; flags = 1 (manual-reset)
    
event_create_flags_ok:
    xor r9d, r9d            ; initial state = unsignaled
    
    call CreateEventExW
    
    ; Store handle in structure
    mov [r12], rax
    mov qword ptr [r12 + 8], rbx   ; manual_reset flag
    
    mov rax, r12
    jmp event_create_done
    
event_create_fail:
    xor rax, rax
    
event_create_done:
    add rsp, 48
    pop rbx
    ret

asm_event_create ENDP

;=====================================================================
; asm_event_set(handle: rcx) -> void
;=====================================================================

ALIGN 16
asm_event_set PROC

    sub rsp, 32
    
    test rcx, rcx
    jz event_set_done
    
    mov rax, [rcx]          ; Get Windows handle
    mov rcx, rax
    call SetEvent
    
    add rsp, 32
    ret

asm_event_set ENDP

;=====================================================================
; asm_str_create(utf8_ptr: rcx, length: rdx) -> rax
;
; Creates string from UTF-8 data (WITH actual allocation).
;=====================================================================

ALIGN 16
asm_str_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = UTF-8 pointer
    mov rbx, rdx            ; rbx = length
    
    test rcx, rcx
    jz str_create_empty
    test rdx, rdx
    jz str_create_empty
    
    ; Allocate: metadata (40) + data + null terminator
    mov rax, rbx
    add rax, 40
    add rax, 1
    
    mov rcx, rax
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz str_create_fail
    
    ; Write metadata
    mov qword ptr [rax + 0], 0ABCDEFh
    mov qword ptr [rax + 8], rbx    ; length
    mov qword ptr [rax + 16], rbx   ; capacity
    mov byte ptr [rax + 24], 8      ; encoding = UTF-8
    
    ; Copy data
    mov rcx, r12            ; source
    mov rdx, rax
    add rdx, 40             ; destination (after metadata)
    mov r8, rbx             ; count
    
    mov r9, 0
str_create_copy:
    cmp r9, r8
    jge str_create_copy_done
    
    mov r10b, [rcx + r9]
    mov [rdx + r9], r10b
    inc r9
    jmp str_create_copy
    
str_create_copy_done:
    ; Null terminate
    mov byte ptr [rdx + r8], 0
    
    ; Return pointer to data (after metadata)
    add rax, 40
    jmp str_create_done
    
str_create_empty:
    mov rcx, 41
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz str_create_fail
    
    mov qword ptr [rax + 0], 0
    mov qword ptr [rax + 8], 0     ; length = 0
    mov byte ptr [rax + 24], 8
    
    add rax, 40
    jmp str_create_done
    
str_create_fail:
    xor rax, rax
    
str_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_str_create ENDP

;=====================================================================
; asm_str_destroy(handle: rcx) -> void
;=====================================================================

ALIGN 16
asm_str_destroy PROC

    test rcx, rcx
    jz str_destroy_done
    
    mov rax, rcx
    sub rax, 40
    call asm_free
    
str_destroy_done:
    ret

asm_str_destroy ENDP

;=====================================================================
; asm_str_length(handle: rcx) -> rax
;=====================================================================

ALIGN 16
asm_str_length PROC

    test rcx, rcx
    jz str_len_zero
    
    mov rax, rcx
    sub rax, 40
    mov rax, [rax + 8]
    ret
    
str_len_zero:
    xor rax, rax
    ret

asm_str_length ENDP

;=====================================================================
; asm_event_loop_create(queue_size: rcx) -> rax
;
; Creates event loop structure with actual allocations.
;=====================================================================

ALIGN 16
asm_event_loop_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = queue size
    
    ; Allocate event loop structure (256 bytes)
    mov rcx, 256
    mov rdx, 32
    call asm_malloc
    
    test rax, rax
    jz loop_create_fail
    
    mov rbx, rax
    
    ; Allocate queue (queue_size * 64 bytes)
    mov rcx, r12
    imul rcx, 64
    mov rdx, 64
    call asm_malloc
    
    test rax, rax
    jz loop_create_cleanup
    
    ; Initialize structure
    mov [rbx + 0], rax      ; queue base
    mov [rbx + 8], r12      ; queue size
    mov qword ptr [rbx + 16], 0  ; head
    mov qword ptr [rbx + 24], 0  ; tail
    
    ; Create mutex
    call asm_mutex_create
    mov [rbx + 32], rax
    
    ; Allocate handler registry
    mov rcx, 256 * 8
    mov rdx, 16
    call asm_malloc
    mov [rbx + 40], rax
    
    ; Initialize stats
    mov qword ptr [rbx + 48], 0
    mov qword ptr [rbx + 56], 0
    mov qword ptr [rbx + 64], 0
    
    mov rax, rbx
    jmp loop_create_done
    
loop_create_cleanup:
    mov rcx, rbx
    call asm_free
    
loop_create_fail:
    xor rax, rax
    
loop_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_event_loop_create ENDP

;=====================================================================
; asm_event_loop_emit(loop: rcx, signal_id: rdx, p1: r8, p2: r9, p3: [rsp+40]) -> void
;=====================================================================

ALIGN 16
asm_event_loop_emit PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = loop handle
    
    test r12, r12
    jz emit_done
    
    ; Lock queue
    mov rcx, [r12 + 32]
    call asm_mutex_lock
    
    ; Check space
    mov rax, [r12 + 24]     ; tail
    mov rbx, [r12 + 8]      ; size
    mov rcx, rax
    inc rcx
    cmp rcx, rbx
    je emit_queue_full
    
    ; Write event
    mov rax, [r12 + 0]      ; queue base
    imul rcx, [r12 + 24], 64
    add rcx, rax            ; rcx = event address
    
    mov [rcx + 0], rdx      ; signal_id
    mov [rcx + 16], r8      ; param1
    mov [rcx + 24], r9      ; param2
    mov rax, [rsp + 32 + 40]  ; param3
    mov [rcx + 32], rax
    
    ; Advance tail
    mov rax, [r12 + 24]
    inc rax
    mov rbx, [r12 + 8]
    cmp rax, rbx
    jl emit_tail_ok
    xor rax, rax
    
emit_tail_ok:
    mov [r12 + 24], rax
    
emit_queue_full:
    ; Unlock
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
emit_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_event_loop_emit ENDP

;=====================================================================
; asm_event_loop_process_one(loop: rcx) -> rax
;=====================================================================

ALIGN 16
asm_event_loop_process_one PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx
    
    test r12, r12
    jz process_one_fail
    
    ; Lock
    mov rcx, [r12 + 32]
    call asm_mutex_lock
    
    ; Check if queue has events
    mov rax, [r12 + 16]     ; head
    mov rdx, [r12 + 24]     ; tail
    
    cmp rax, rdx
    je process_one_empty
    
    ; Get event
    mov rcx, [r12 + 0]      ; queue base
    imul r8, rax, 64
    add r8, rcx
    
    mov r9d, [r8 + 0]       ; signal_id
    mov r10, [r8 + 16]      ; param1
    mov r11, [r8 + 24]      ; param2
    mov r13, [r8 + 32]      ; param3
    
    ; Look up handler
    mov r8, [r12 + 40]
    cmp r9d, 256
    jge process_one_advance
    
    mov rax, [r8 + r9*8]
    test rax, rax
    jz process_one_advance
    
    ; Call handler(p1, p2, p3)
    mov rcx, r10
    mov rdx, r11
    mov r8, r13
    
    sub rsp, 32
    call rax
    add rsp, 32
    
process_one_advance:
    ; Advance head
    mov rax, [r12 + 16]
    inc rax
    mov rbx, [r12 + 8]
    cmp rax, rbx
    jl advance_ok
    xor rax, rax
    
advance_ok:
    mov [r12 + 16], rax
    
    ; Unlock
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    mov rax, 1
    jmp process_one_exit
    
process_one_empty:
    mov rcx, [r12 + 32]
    call asm_mutex_unlock
    
    xor rax, rax
    jmp process_one_exit
    
process_one_fail:
    mov rax, -1
    
process_one_exit:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_event_loop_process_one ENDP

;=====================================================================
; hotpatch_apply(target_ptr: rcx, patch_data: rdx, size: r8) -> rax
;
; Applies a hotpatch to memory:
;  1. Disables write protection on target region
;  2. Copies patch_data to target
;  3. Re-enables write protection
;  4. Returns status code
;
; Returns:
;  0 = success
;  -1 = error
;=====================================================================

ALIGN 16
hotpatch_apply PROC

    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rcx            ; r12 = target
    mov r13, rdx            ; r13 = patch data
    mov rbx, r8             ; rbx = size
    
    test r12, r12
    jz hotpatch_apply_fail
    test r13, r13
    jz hotpatch_apply_fail
    test rbx, rbx
    jz hotpatch_apply_fail
    
    ; 1. Change memory protection to PAGE_EXECUTE_READWRITE
    mov rcx, r12            ; lpAddress
    mov rdx, rbx            ; dwSize
    mov r8d, 40h            ; flNewProtect = PAGE_EXECUTE_READWRITE
    lea r9, [rsp + 32]      ; lpflOldProtect
    call VirtualProtect
    test eax, eax
    jz hotpatch_apply_fail
    
    ; 2. Copy patch data to target memory
    mov rcx, r13            ; source = patch data
    mov rdx, r12            ; dest = target
    mov r8, rbx             ; count = size
    
    mov r9, 0
hotpatch_copy_loop:
    cmp r9, r8
    jge hotpatch_copy_done
    
    mov r10b, [rcx + r9]
    mov [rdx + r9], r10b
    inc r9
    jmp hotpatch_copy_loop
    
hotpatch_copy_done:
    ; 3. Restore memory protection
    mov rcx, r12
    mov rdx, rbx
    mov r8d, [rsp + 32]     ; flOldProtect
    lea r9, [rsp + 40]
    call VirtualProtect
    
    ; 4. Flush instruction cache
    mov rcx, -1             ; hProcess = GetCurrentProcess()
    mov rdx, r12            ; lpBaseAddress
    mov r8, rbx             ; dwSize
    call FlushInstructionCache
    
    lock add [rel g_patches_applied], 1
    
    xor rax, rax            ; success
    jmp hotpatch_apply_done
    
hotpatch_apply_fail:
    mov rax, -1
    lock add [rel g_hotpatch_errors], 1
    
hotpatch_apply_done:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret

hotpatch_apply ENDP

;=====================================================================
; hotpatch_main() -> void
;
; Main entry point demonstrating all components.
; Shows initialization, allocation, strings, events, and patching.
;=====================================================================

ALIGN 16
hotpatch_main PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    ; 1. INITIALIZE RUNTIME
    call hotpatch_initialize
    test rax, rax
    jnz main_fail
    
    ; 2. TEST MEMORY ALLOCATION
    mov rcx, 256            ; 256 bytes
    mov rdx, 32             ; 32-byte alignment
    call asm_malloc
    test rax, rax
    jz main_fail
    
    mov r12, rax            ; r12 = allocated pointer
    
    ; 3. TEST STRING CREATION
    lea rcx, [rel test_string]
    mov rdx, 13             ; "Hello, MASM!"
    call asm_str_create
    test rax, rax
    jz main_fail
    
    mov r13, rax            ; r13 = string handle
    
    ; 4. TEST EVENT LOOP CREATION
    mov rcx, 256            ; queue size
    call asm_event_loop_create
    test rax, rax
    jz main_fail
    
    mov rbx, rax            ; rbx = event loop handle
    
    ; 5. EMIT AN EVENT
    mov rcx, rbx            ; loop
    mov rdx, 1              ; signal_id = 1
    mov r8, 42              ; param1
    mov r9, 100             ; param2
    mov qword ptr [rsp + 40], 200  ; param3
    call asm_event_loop_emit
    
    ; 6. CLEANUP
    mov rcx, r13
    call asm_str_destroy
    
    mov rcx, r12
    call asm_free
    
    ; Return success
    xor rax, rax
    jmp main_done
    
main_fail:
    mov rax, -1
    
main_done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

hotpatch_main ENDP

;=====================================================================
; Data Section
;=====================================================================

.data

test_string db "Hello, MASM!", 0

END
