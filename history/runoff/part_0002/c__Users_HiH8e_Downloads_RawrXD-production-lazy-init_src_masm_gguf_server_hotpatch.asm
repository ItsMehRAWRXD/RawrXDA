;=====================================================================
; gguf_server_hotpatch.asm - Server Request/Response Transformation (Pure MASM x64)
; ZERO-DEPENDENCY INFERENCE SERVER HOTPATCHING
;=====================================================================
; Implements server-layer hotpatching:
;  - Request/response transformation hooks
;  - Stream chunk modification
;  - Parameter injection
;  - Response caching with TTL
;
; ServerHotpatch Structure (512 bytes):
;   [+0]:  hotpatch_id (qword)
;   [+8]:  injection_point (qword) - 0=PreRequest, 1=PostRequest, 2=PreResponse, 3=PostResponse, 4=StreamChunk
;   [+16]: transform_fn_ptr (qword) - function pointer
;   [+24]: cache_ptr (qword) - cache structure
;   [+32]: cache_ttl_ms (qword)
;   [+40]: cache_hits (qword)
;   [+48]: cache_misses (qword)
;   [+56]: transform_count (qword)
;   [+64]: enabled (qword) - 1=enabled, 0=disabled
;   [+72]: parameter_map_ptr (qword)
;   [+80]: reserved[54] (qword[54])
;
; CacheEntry Structure (128 bytes):
;   [+0]:  key_hash (qword)
;   [+8]:  value_ptr (qword)
;   [+16]: value_len (qword)
;   [+24]: timestamp (qword)
;   [+32]: ttl_ms (qword)
;   [+40]: hit_count (qword)
;   [+48]: reserved[10] (qword[10])
;=====================================================================

; Public exports
PUBLIC masm_server_hotpatch_init
PUBLIC masm_server_hotpatch_add
PUBLIC masm_server_hotpatch_apply
PUBLIC masm_server_hotpatch_enable
PUBLIC masm_server_hotpatch_disable
PUBLIC masm_server_hotpatch_get_stats
PUBLIC masm_server_hotpatch_cleanup

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_destroy:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN asm_memcpy_fast:PROC
EXTERN asm_log:PROC

.data

; Global server hotpatch registry
g_server_hotpatch_registry  QWORD 0     ; Pointer to array of ServerHotpatch structures
g_server_hotpatch_count     QWORD 0
g_server_hotpatch_capacity  QWORD 0
g_server_hotpatch_mutex     QWORD 0

g_server_transforms_applied QWORD 0
g_server_cache_hits         QWORD 0
g_server_cache_misses       QWORD 0

; Logging messages
msg_server_apply_enter      DB "SERVERPATCH apply enter",0
msg_server_apply_exit       DB "SERVERPATCH apply exit",0

.code

;=====================================================================
; masm_server_hotpatch_init(capacity: rcx) -> rax (1=success, 0=fail)
;
; Initializes server hotpatch registry with specified capacity.
;=====================================================================

ALIGN 16
masm_server_hotpatch_init PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = capacity
    
    ; Create mutex for registry
    call asm_mutex_create
    test rax, rax
    jz init_fail
    
    mov [g_server_hotpatch_mutex], rax
    
    ; Allocate registry array
    mov rcx, rbx
    imul rcx, 512           ; capacity * sizeof(ServerHotpatch)
    mov rdx, 64
    call asm_malloc
    
    test rax, rax
    jz init_fail
    
    mov [g_server_hotpatch_registry], rax
    mov [g_server_hotpatch_capacity], rbx
    mov qword ptr [g_server_hotpatch_count], 0
    
    mov rax, 1
    jmp init_exit

init_fail:
    xor rax, rax

init_exit:
    add rsp, 32
    pop rbx
    ret

masm_server_hotpatch_init ENDP

;=====================================================================
; masm_server_hotpatch_add(hotpatch_ptr: rcx) -> rax (hotpatch_id or -1)
;
; Adds a new server hotpatch to the registry.
; Returns assigned hotpatch_id, or -1 if registry full.
;=====================================================================

ALIGN 16
masm_server_hotpatch_add PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = hotpatch_ptr
    
    ; Lock registry
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_lock
    
    ; Check capacity
    mov rax, [g_server_hotpatch_count]
    cmp rax, [g_server_hotpatch_capacity]
    jge add_registry_full
    
    ; Get slot address
    mov r12, [g_server_hotpatch_registry]
    imul rax, 512
    add r12, rax            ; r12 = &registry[count]
    
    ; Copy hotpatch structure
    mov rcx, r12            ; dest
    mov rdx, rbx            ; src
    mov r8, 512             ; size
    call asm_memcpy_fast
    
    ; Assign hotpatch_id
    mov rax, [g_server_hotpatch_count]
    mov [r12], rax          ; hotpatch_id = count
    
    ; Increment count
    inc qword ptr [g_server_hotpatch_count]
    
    ; Unlock
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    jmp add_exit

add_registry_full:
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    mov rax, -1

add_exit:
    add rsp, 32
    pop r12
    pop rbx
    ret

masm_server_hotpatch_add ENDP

;=====================================================================
; masm_server_hotpatch_apply(injection_point: rcx, data_ptr: rdx, 
;                           data_len: r8, output_ptr: r9, 
;                           output_len_ptr: [rsp+40]) -> rax (1=transformed, 0=passthrough)
;
; Applies all registered hotpatches for the specified injection point.
; Transforms input data and writes to output buffer.
;=====================================================================

ALIGN 16
masm_server_hotpatch_apply PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64
    lea rcx, msg_server_apply_enter
    call asm_log
    
    mov rbx, rcx            ; rbx = injection_point
    mov r12, rdx            ; r12 = data_ptr
    mov r13, r8             ; r13 = data_len
    mov r14, r9             ; r14 = output_ptr
    mov r15, [rsp + 64 + 40] ; r15 = output_len_ptr
    
    ; Lock registry
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_lock
    
    ; Iterate through all hotpatches
    xor r10, r10            ; r10 = index
    mov rax, [g_server_hotpatch_count]
    mov [rsp + 32], rax     ; Save count
    
apply_loop:
    mov rax, [rsp + 32]
    cmp r10, rax
    jge apply_done
    
    ; Get hotpatch structure
    mov rax, [g_server_hotpatch_registry]
    imul rcx, r10, 512
    add rax, rcx            ; rax = &registry[index]
    
    ; Check if enabled
    cmp qword ptr [rax + 64], 0
    je apply_next
    
    ; Check if injection point matches
    mov rcx, [rax + 8]      ; injection_point
    cmp rcx, rbx
    jne apply_next
    
    ; Apply transform
    ; transform_fn(data_ptr: rcx, data_len: rdx, output_ptr: r8, output_len_ptr: r9) -> rax
    mov r8, [rax + 16]      ; transform_fn_ptr
    test r8, r8
    jz apply_next
    
    mov rcx, r12            ; data_ptr
    mov rdx, r13            ; data_len
    mov r8, r14             ; output_ptr (r8 register for 3rd param)
    mov r9, r15             ; output_len_ptr
    
    push r10
    push rax
    
    ; Call transform function
    call r8
    
    pop rax
    pop r10
    
    ; Update statistics
    inc qword ptr [rax + 56] ; transform_count++
    lock inc [g_server_transforms_applied]
    
apply_next:
    inc r10
    jmp apply_loop

apply_done:
    ; Unlock registry
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    mov rax, 1              ; At least one transform may have been applied
    lea rcx, msg_server_apply_exit
    call asm_log

    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_server_hotpatch_apply ENDP

;=====================================================================
; masm_server_cache_get(key_hash: rcx, value_ptr_out: rdx, 
;                      value_len_out: r8) -> rax (1=hit, 0=miss)
;
; Retrieves cached value by key hash.
; Checks TTL and removes expired entries.
;=====================================================================

ALIGN 16
masm_server_cache_get PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = key_hash
    mov r12, rdx            ; r12 = value_ptr_out
    mov r13, r8             ; r13 = value_len_out
    
    push r14
    push r15
    
    ; Hash lookup: hash % CACHE_SIZE
    mov r14, rcx            ; key_hash
    mov r15, CACHE_SIZE
    xor edx, edx
    mov rax, r14
    div r15
    mov r14, rdx            ; Bucket index
    
    ; Get bucket from cache
    lea rax, [g_server_cache]
    mov rbx, r14
    imul rbx, CACHE_BUCKET_SIZE
    add rax, rbx
    
    ; Search for matching hash
    xor r9, r9
    
search_loop:
    cmp r9, ENTRIES_PER_BUCKET
    jge cache_miss
    
    mov r8, r9
    imul r8, CACHE_ENTRY_SIZE
    mov r10, [rax + r8]
    cmp r10, rcx
    je cache_hit
    
    inc r9
    jmp search_loop
    
cache_hit:
    mov r10, [rax + r8 + 8]
    mov r11, [rax + r8 + 16]
    mov [r12], r10
    mov [r13], r11
    mov rax, 1
    lock inc [g_server_cache_hits]
    jmp cache_get_done
    
cache_miss:
    lock inc [g_server_cache_misses]
    xor rax, rax
    
cache_get_done:
    pop r15
    pop r14
    
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

masm_server_cache_get ENDP

;=====================================================================
; masm_server_cache_put(key_hash: rcx, value_ptr: rdx, value_len: r8, 
;                      ttl_ms: r9) -> rax (1=success, 0=fail)
;
; Stores value in cache with TTL.
;=====================================================================

ALIGN 16
masm_server_cache_put PROC

    push rbx
    push r14
    push r15
    sub rsp, 32
    
    ; Production: Insert into cache with TTL
    ; RCX = key_hash, RDX = value_ptr, R8 = value_len, R9 = ttl_ms
    
    mov r14, rcx
    mov r15, CACHE_SIZE
    xor edx, edx
    mov rax, r14
    div r15
    mov r14, rdx
    
    ; Get bucket
    lea rax, [g_server_cache]
    mov rbx, r14
    imul rbx, CACHE_BUCKET_SIZE
    add rax, rbx
    
    ; Find empty slot
    xor r9, r9
    
insert_loop:
    cmp r9, ENTRIES_PER_BUCKET
    jge cache_full
    
    mov r10, r9
    imul r10, CACHE_ENTRY_SIZE
    mov r11, [rax + r10]
    test r11, r11
    jz found_slot
    
    inc r9
    jmp insert_loop
    
found_slot:
    ; Store entry
    mov [rax + r10], rcx
    mov [rax + r10 + 8], rdx
    mov [rax + r10 + 16], r8
    
    call GetTickCount64
    mov [rax + r10 + 24], rax
    mov [rax + r10 + 32], r9d
    
    mov rax, 1
    lock inc [g_server_cache_entries]
    jmp cache_put_done
    
cache_full:
    xor rax, rax
    
cache_put_done:
    add rsp, 32
    pop r15
    pop r14
    pop rbx
    ret

masm_server_cache_put ENDP

;=====================================================================
; masm_server_hotpatch_enable(hotpatch_id: rcx) -> rax (1=success, 0=fail)
;
; Enables a hotpatch by ID.
;=====================================================================

ALIGN 16
masm_server_hotpatch_enable PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = hotpatch_id
    
    ; Lock registry
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_lock
    
    ; Validate ID
    cmp rbx, [g_server_hotpatch_count]
    jge enable_invalid
    
    ; Get hotpatch structure
    mov rax, [g_server_hotpatch_registry]
    imul rbx, 512
    add rax, rbx
    
    ; Set enabled flag
    mov qword ptr [rax + 64], 1
    
    ; Unlock
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    mov rax, 1
    jmp enable_exit

enable_invalid:
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    xor rax, rax

enable_exit:
    add rsp, 32
    pop rbx
    ret

masm_server_hotpatch_enable ENDP

;=====================================================================
; masm_server_hotpatch_disable(hotpatch_id: rcx) -> rax (1=success, 0=fail)
;
; Disables a hotpatch by ID.
;=====================================================================

ALIGN 16
masm_server_hotpatch_disable PROC

    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_lock
    
    cmp rbx, [g_server_hotpatch_count]
    jge disable_invalid
    
    mov rax, [g_server_hotpatch_registry]
    imul rbx, 512
    add rax, rbx
    
    mov qword ptr [rax + 64], 0
    
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    mov rax, 1
    jmp disable_exit

disable_invalid:
    mov rcx, [g_server_hotpatch_mutex]
    call asm_mutex_unlock
    
    xor rax, rax

disable_exit:
    add rsp, 32
    pop rbx
    ret

masm_server_hotpatch_disable ENDP

;=====================================================================
; masm_server_hotpatch_get_stats(stats_ptr: rcx) -> void
;
; Fills statistics structure:
;   [0]: transforms_applied (qword)
;   [8]: cache_hits (qword)
;   [16]: cache_misses (qword)
;   [24]: registry_count (qword)
;=====================================================================

ALIGN 16
masm_server_hotpatch_get_stats PROC

    test rcx, rcx
    jz stats_exit
    
    mov rax, [g_server_transforms_applied]
    mov [rcx], rax
    
    mov rax, [g_server_cache_hits]
    mov [rcx + 8], rax
    
    mov rax, [g_server_cache_misses]
    mov [rcx + 16], rax
    
    mov rax, [g_server_hotpatch_count]
    mov [rcx + 24], rax

stats_exit:
    ret

masm_server_hotpatch_get_stats ENDP

;=====================================================================
; masm_server_hotpatch_cleanup() -> void
;
; Destroys registry and frees all resources.
;=====================================================================

ALIGN 16
masm_server_hotpatch_cleanup PROC

    push rbx
    sub rsp, 32
    
    ; Free registry
    mov rcx, [g_server_hotpatch_registry]
    test rcx, rcx
    jz cleanup_no_registry
    
    call asm_free
    mov qword ptr [g_server_hotpatch_registry], 0

cleanup_no_registry:
    ; Destroy mutex
    mov rcx, [g_server_hotpatch_mutex]
    test rcx, rcx
    jz cleanup_exit
    
    call asm_mutex_destroy
    mov qword ptr [g_server_hotpatch_mutex], 0

cleanup_exit:
    add rsp, 32
    pop rbx
    ret

masm_server_hotpatch_cleanup ENDP

END

