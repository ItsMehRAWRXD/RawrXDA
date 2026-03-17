; Caching Layer - Redis-compatible in-memory cache (x64)
; Ported from 32-bit MASM to ML64 with hash table + LRU semantics intact.

CACHING_LAYER_IMPLEMENTATION equ 1

include windows.inc
include masm_master_defs.inc
include caching_layer.inc

CACHE_STRUCT_size equ SIZEOF CACHE_STRUCT
CACHE_ITEM_size   equ SIZEOF CACHE_ITEM

OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

.code

; ---------------------------------------------------------------------------
; Local helpers
; allocate_zero(size: rcx) -> rax
allocate_zero PROC PRIVATE
    push rbx
    sub rsp, 32
    mov rbx, rcx
    call GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, rbx
    call HeapAlloc
    add rsp, 32
    pop rbx
    ret
allocate_zero ENDP

; free_heap(ptr: rcx)
free_heap PROC PRIVATE
    push rbx
    sub rsp, 32
    mov rbx, rcx
    test rbx, rbx
    jz short fh_done
    call GetProcessHeap
    mov rcx, rax
    xor rdx, rdx
    mov r8, rbx
    call HeapFree
fh_done:
    add rsp, 32
    pop rbx
    ret
free_heap ENDP

; djb2 hash (key_ptr: rcx, key_len: rdx) -> rax (bucket index)
hash_key PROC PUBLIC
    push rsi
    push rdi
    sub rsp, 40
    mov rsi, rcx
    mov rdi, rdx
    xor eax, eax
    xor edx, edx
hk_loop:
    test rdi, rdi
    jz hk_mod
    mov dl, byte ptr [rsi]
    imul eax, eax, 33
    add eax, edx
    inc rsi
    dec rdi
    jmp hk_loop
hk_mod:
    xor edx, edx
    mov ecx, CACHE_HASH_TABLE_SIZE
    div ecx
    mov eax, edx
    add rsp, 40
    pop rdi
    pop rsi
    ret
hash_key ENDP

; Move item to head of LRU: rcx=cache*, rdx=item*
update_lru PROC PRIVATE
    push rbx
    mov rbx, rcx
    mov rcx, rdx
    cmp rcx, [rbx + CACHE_STRUCT.head_ptr]
    je  ul_done

    mov r8, [rcx + CACHE_ITEM.prev_ptr]
    mov r9, [rcx + CACHE_ITEM.next_ptr]
    test r8, r8
    jz  ul_no_prev
    mov [r8 + CACHE_ITEM.next_ptr], r9
ul_no_prev:
    test r9, r9
    jz  ul_no_next
    mov [r9 + CACHE_ITEM.prev_ptr], r8
ul_no_next:

    cmp rcx, [rbx + CACHE_STRUCT.tail_ptr]
    jne ul_not_tail
    mov [rbx + CACHE_STRUCT.tail_ptr], r8
ul_not_tail:

    mov r8, [rbx + CACHE_STRUCT.head_ptr]
    mov [rcx + CACHE_ITEM.prev_ptr], 0
    mov [rcx + CACHE_ITEM.next_ptr], r8
    test r8, r8
    jz  ul_no_old_head
    mov [r8 + CACHE_ITEM.prev_ptr], rcx
ul_no_old_head:
    mov [rbx + CACHE_STRUCT.head_ptr], rcx
ul_done:
    pop rbx
    ret
update_lru ENDP

; Remove item: rcx=cache*, rdx=item*
remove_item PROC PRIVATE
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    mov rbx, rcx
    mov rsi, rdx

    mov rcx, [rsi + CACHE_ITEM.key_ptr]
    mov rdx, [rsi + CACHE_ITEM.key_len]
    call hash_key
    mov r8, rax
    mov r9, [rbx + CACHE_STRUCT.hash_table_ptr]
    lea r9, [r9 + r8*8]
    mov r10, [r9]
    test r10, r10
    jz  ri_hash_done
    cmp r10, rsi
    jne ri_hash_seek
    mov r10, [rsi + CACHE_ITEM.hash_next_ptr]
    mov [r9], r10
    jmp ri_hash_done
ri_hash_seek:
    mov r11, r10
ri_hash_loop:
    mov r10, [r11 + CACHE_ITEM.hash_next_ptr]
    test r10, r10
    jz  ri_hash_done
    cmp r10, rsi
    jne ri_hash_next
    mov r10, [r10 + CACHE_ITEM.hash_next_ptr]
    mov [r11 + CACHE_ITEM.hash_next_ptr], r10
    jmp ri_hash_done
ri_hash_next:
    mov r11, r10
    jmp ri_hash_loop
ri_hash_done:

    mov r8, [rsi + CACHE_ITEM.prev_ptr]
    mov r9, [rsi + CACHE_ITEM.next_ptr]
    test r8, r8
    jz  ri_no_prev
    mov [r8 + CACHE_ITEM.next_ptr], r9
ri_no_prev:
    test r9, r9
    jz  ri_no_next
    mov [r9 + CACHE_ITEM.prev_ptr], r8
ri_no_next:
    cmp rsi, [rbx + CACHE_STRUCT.head_ptr]
    jne ri_not_head
    mov [rbx + CACHE_STRUCT.head_ptr], r9
ri_not_head:
    cmp rsi, [rbx + CACHE_STRUCT.tail_ptr]
    jne ri_not_tail
    mov [rbx + CACHE_STRUCT.tail_ptr], r8
ri_not_tail:

    mov r8, [rsi + CACHE_ITEM.value_len]
    sub [rbx + CACHE_STRUCT.cache_size], r8
    dec qword ptr [rbx + CACHE_STRUCT.item_count]
    inc qword ptr [rbx + CACHE_STRUCT.eviction_count]

    mov rcx, [rsi + CACHE_ITEM.key_ptr]
    call free_heap
    mov rcx, [rsi + CACHE_ITEM.value_ptr]
    call free_heap
    mov rcx, rsi
    call free_heap

    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
remove_item ENDP

; ---------------------------------------------------------------------------
; cache_init(max_size: rcx, stats_enabled: rdx) -> rax (cache* or 0)
cache_init PROC PUBLIC
    push rbx
    push rsi
    sub rsp, 40

    mov rbx, rcx
    test rbx, rbx
    jnz ci_has_size
    mov rbx, CACHE_DEFAULT_SIZE
ci_has_size:

    mov rcx, CACHE_STRUCT_size
    call allocate_zero
    test rax, rax
    jz  ci_fail
    mov rsi, rax

    mov rcx, 0
    mov rdx, 0
    mov r8, 0
    call CreateMutexA
    test rax, rax
    jz  ci_free_struct
    mov [rsi + CACHE_STRUCT.cache_mutex], rax

    mov rcx, CACHE_HASH_TABLE_SIZE*8
    call allocate_zero
    test rax, rax
    jz  ci_free_mutex
    mov [rsi + CACHE_STRUCT.hash_table_ptr], rax
    mov qword ptr [rsi + CACHE_STRUCT.hash_table_size], CACHE_HASH_TABLE_SIZE

    mov [rsi + CACHE_STRUCT.max_size], rbx
    mov [rsi + CACHE_STRUCT.stats_enabled], rdx
    
    lea rax, HeapAlloc
    mov [rsi + CACHE_STRUCT.allocator_ptr], rax
    lea rax, HeapFree
    mov [rsi + CACHE_STRUCT.deallocator_ptr], rax

    mov rax, rsi
    add rsp, 40
    pop rsi
    pop rbx
    ret

ci_free_mutex:
    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    call CloseHandle
ci_free_struct:
    mov rcx, rsi
    call free_heap
ci_fail:
    xor rax, rax
    add rsp, 40
    pop rsi
    pop rbx
    ret
cache_init ENDP

; cache_shutdown(cache*: rcx) -> eax
cache_shutdown PROC PUBLIC
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    mov rsi, rcx
    test rsi, rsi
    jz cs_done

    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rbx, [rsi + CACHE_STRUCT.head_ptr]
cs_loop:
    test rbx, rbx
    jz cs_items_done
    mov rdi, [rbx + CACHE_ITEM.next_ptr]
    mov rcx, [rbx + CACHE_ITEM.key_ptr]
    call free_heap
    mov rcx, [rbx + CACHE_ITEM.value_ptr]
    call free_heap
    mov rcx, rbx
    call free_heap
    mov rbx, rdi
    jmp cs_loop
cs_items_done:
    mov rcx, [rsi + CACHE_STRUCT.hash_table_ptr]
    call free_heap

    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    call CloseHandle
    mov rcx, rsi
    call free_heap
cs_done:
    xor eax, eax
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
cache_shutdown ENDP

; cache_get(cache*, key_ptr, key_len) -> eax
cache_get PROC PUBLIC
    push rbx
    push rsi
    push rdi
    push r14
    sub rsp, 40

    mov r14, rcx              ; cache
    mov rbx, rdx              ; key_ptr
    mov rdi, r8               ; key_len

    test r14, r14
    jz cg_invalid
    test rbx, rbx
    jz cg_invalid
    test rdi, rdi
    jz cg_invalid
    cmp rdi, CACHE_MAX_KEY_SIZE
    ja cg_size

    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rcx, rbx
    mov rdx, rdi
    call hash_key
    mov r10, rax
    mov r11, [r14 + CACHE_STRUCT.hash_table_ptr]
    mov r11, [r11 + r10*8]

cg_search:
    test r11, r11
    jz cg_not_found
    mov rax, [r11 + CACHE_ITEM.key_len]
    cmp rax, rdi
    jne cg_next
    mov rcx, rax
    mov rsi, [r11 + CACHE_ITEM.key_ptr]
    mov rdi, rbx
    repe cmpsb
    jnz cg_next

    mov rax, [r11 + CACHE_ITEM.expires_at]
    test rax, rax
    jz cg_valid
    call GetTickCount
    cmp eax, dword ptr [r11 + CACHE_ITEM.expires_at]
    jae cg_expired
cg_valid:
    inc qword ptr [r11 + CACHE_ITEM.access_count]
    call GetTickCount
    mov [r11 + CACHE_ITEM.last_access], rax
    inc qword ptr [r14 + CACHE_STRUCT.hit_count]
    mov rcx, r14
    mov rdx, r11
    call update_lru
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cg_exit

cg_expired:
    mov rcx, r14
    mov rdx, r11
    call remove_item
    inc qword ptr [r14 + CACHE_STRUCT.miss_count]
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_EXPIRED
    jmp cg_exit

cg_next:
    mov r11, [r11 + CACHE_ITEM.hash_next_ptr]
    jmp cg_search

cg_not_found:
    inc qword ptr [r14 + CACHE_STRUCT.miss_count]
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_NOT_FOUND
    jmp cg_exit
cg_invalid:
    mov eax, CACHE_ERROR_INVALID
    jmp cg_exit
cg_size:
    mov eax, CACHE_ERROR_SIZE
cg_exit:
    add rsp, 40
    pop r14
    pop rdi
    pop rsi
    pop rbx
    ret
cache_get ENDP

; cache_set(cache*, key_ptr, key_len, value_ptr, value_len, ttl_ms) -> eax
cache_set PROC PUBLIC
    mov r13, [rsp + 40]       ; value_len (5th arg)
    mov r15, [rsp + 48]       ; ttl_ms  (6th arg)
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32

    mov r14, rcx              ; cache
    mov rbx, rdx              ; key_ptr
    mov rdi, r8               ; key_len
    mov r12, r9               ; value_ptr
    mov [rsp + 16], rdi       ; stash key_len

    test r14, r14
    jz cs_invalid
    test rbx, rbx
    jz cs_invalid
    test rdi, rdi
    jz cs_invalid
    test r12, r12
    jz cs_invalid
    test r13, r13
    jz cs_invalid
    cmp rdi, CACHE_MAX_KEY_SIZE
    ja cs_size
    cmp r13, CACHE_MAX_VALUE_SIZE
    ja cs_size

    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rax, r13
    add rax, rdi
    add rax, CACHE_ITEM_size
    mov [rsp + 8], rax            ; spill new_size

cs_evict:
    mov rax, [rsp + 8]
    add rax, [r14 + CACHE_STRUCT.cache_size]
    cmp rax, [r14 + CACHE_STRUCT.max_size]
    jbe cs_evict_done
    mov rdx, [r14 + CACHE_STRUCT.tail_ptr]
    test rdx, rdx
    jz cs_evict_done
    mov rcx, r14
    call remove_item
    jmp cs_evict
cs_evict_done:

    mov rcx, rbx
    mov rdx, rdi
    call hash_key
    mov r10, rax
    mov r11, [r14 + CACHE_STRUCT.hash_table_ptr]
    lea r11, [r11 + r10*8]
    mov r9, [r11]

cs_search:
    test r9, r9
    jz cs_new
    mov rax, [r9 + CACHE_ITEM.key_len]
    cmp rax, rdi
    jne cs_next
    mov rcx, rax
    mov rsi, [r9 + CACHE_ITEM.key_ptr]
    mov rdi, rbx
    repe cmpsb
    jnz cs_next

    mov rax, [r9 + CACHE_ITEM.value_len]
    cmp rax, r13
    je cs_same_size
    sub [r14 + CACHE_STRUCT.cache_size], rax
    add [r14 + CACHE_STRUCT.cache_size], r13
    mov rcx, [r9 + CACHE_ITEM.value_ptr]
    call free_heap
    mov rcx, r13
    call allocate_zero
    test rax, rax
    jz cs_oom_locked
    mov [r9 + CACHE_ITEM.value_ptr], rax
cs_same_size:
    mov rdi, [r9 + CACHE_ITEM.value_ptr]
    mov rsi, r12
    mov rcx, r13
    rep movsb
    mov eax, r15d
    test eax, eax
    jz cs_no_exp
    call GetTickCount
    add eax, r15d
    mov [r9 + CACHE_ITEM.expires_at], rax
    jmp cs_exp_set
cs_no_exp:
    mov qword ptr [r9 + CACHE_ITEM.expires_at], 0
cs_exp_set:
    call GetTickCount
    mov [r9 + CACHE_ITEM.last_access], rax
    mov rcx, r14
    mov rdx, r9
    call update_lru
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cs_exit
cs_next:
    mov r9, [r9 + CACHE_ITEM.hash_next_ptr]
    jmp cs_search

cs_new:
    mov rcx, CACHE_ITEM_size
    call allocate_zero
    test rax, rax
    jz cs_oom_locked
    mov r9, rax

    mov rcx, [rsp + 16]        ; key_len
    call allocate_zero
    test rax, rax
    jz cs_oom_free_item
    mov [r9 + CACHE_ITEM.key_ptr], rax
    mov rax, [rsp + 16]
    mov [r9 + CACHE_ITEM.key_len], rax
    mov rdi, [r9 + CACHE_ITEM.key_ptr]
    mov rsi, rbx
    mov rcx, [rsp + 16]
    rep movsb

    mov rcx, r13
    call allocate_zero
    test rax, rax
    jz cs_oom_free_key
    mov [r9 + CACHE_ITEM.value_ptr], rax
    mov [r9 + CACHE_ITEM.value_len], r13
    mov rdi, rax
    mov rsi, r12
    mov rcx, r13
    rep movsb

    mov eax, r15d
    test eax, eax
    jz cs_new_no_exp
    call GetTickCount
    add eax, r15d
    mov [r9 + CACHE_ITEM.expires_at], rax
    jmp cs_new_exp_set
cs_new_no_exp:
    mov qword ptr [r9 + CACHE_ITEM.expires_at], 0
cs_new_exp_set:
    mov qword ptr [r9 + CACHE_ITEM.access_count], 1
    call GetTickCount
    mov [r9 + CACHE_ITEM.last_access], rax

    mov rax, [r11]
    mov [r9 + CACHE_ITEM.hash_next_ptr], rax
    mov [r11], r9

    mov rax, [r14 + CACHE_STRUCT.head_ptr]
    mov [r9 + CACHE_ITEM.next_ptr], rax
    mov qword ptr [r9 + CACHE_ITEM.prev_ptr], 0
    test rax, rax
    jz cs_no_old_head
    mov [rax + CACHE_ITEM.prev_ptr], r9
cs_no_old_head:
    mov [r14 + CACHE_STRUCT.head_ptr], r9
    cmp qword ptr [r14 + CACHE_STRUCT.tail_ptr], 0
    jne cs_tail_ok
    mov [r14 + CACHE_STRUCT.tail_ptr], r9
cs_tail_ok:
    add [r14 + CACHE_STRUCT.cache_size], r13
    inc qword ptr [r14 + CACHE_STRUCT.item_count]
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cs_exit

cs_oom_free_key:
    mov rcx, [r9 + CACHE_ITEM.key_ptr]
    call free_heap
cs_oom_free_item:
    mov rcx, r9
    call free_heap
cs_oom_locked:
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_OOM
    jmp cs_exit
cs_invalid:
    mov eax, CACHE_ERROR_INVALID
    jmp cs_exit
cs_size:
    mov eax, CACHE_ERROR_SIZE
cs_exit:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
cache_set ENDP

; cache_delete(cache*, key_ptr, key_len)
cache_delete PROC PUBLIC
    push rbx
    push rsi
    push rdi
    push r14
    sub rsp, 40

    mov r14, rcx
    mov rbx, rdx
    mov rdi, r8

    test r14, r14
    jz cd_invalid
    test rbx, rbx
    jz cd_invalid
    test rdi, rdi
    jz cd_invalid

    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rcx, rbx
    mov rdx, rdi
    call hash_key
    mov r10, rax
    mov r11, [r14 + CACHE_STRUCT.hash_table_ptr]
    lea r11, [r11 + r10*8]
    mov r9, [r11]
cd_search:
    test r9, r9
    jz cd_not_found
    mov rax, [r9 + CACHE_ITEM.key_len]
    cmp rax, rdi
    jne cd_next
    mov rcx, rax
    mov rsi, [r9 + CACHE_ITEM.key_ptr]
    mov rdi, rbx
    repe cmpsb
    jnz cd_next
    mov rcx, r14
    mov rdx, r9
    call remove_item
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cd_exit
cd_next:
    mov r9, [r9 + CACHE_ITEM.hash_next_ptr]
    jmp cd_search
cd_not_found:
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_NOT_FOUND
    jmp cd_exit
cd_invalid:
    mov eax, CACHE_ERROR_INVALID
cd_exit:
    add rsp, 40
    pop r14
    pop rdi
    pop rsi
    pop rbx
    ret
cache_delete ENDP

; cache_exists(cache*, key_ptr, key_len)
cache_exists PROC PUBLIC
    push rbx
    push rsi
    push rdi
    push r14
    sub rsp, 40

    mov r14, rcx
    mov rbx, rdx
    mov rdi, r8
    test r14, r14
    jz ce_invalid
    test rbx, rbx
    jz ce_invalid
    test rdi, rdi
    jz ce_invalid

    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rcx, rbx
    mov rdx, rdi
    call hash_key
    mov r10, rax
    mov r11, [r14 + CACHE_STRUCT.hash_table_ptr]
    lea r11, [r11 + r10*8]
    mov r9, [r11]
ce_search:
    test r9, r9
    jz ce_not_found
    mov rax, [r9 + CACHE_ITEM.key_len]
    cmp rax, rdi
    jne ce_next
    mov rcx, rax
    mov rsi, [r9 + CACHE_ITEM.key_ptr]
    mov rdi, rbx
    repe cmpsb
    jnz ce_next
    mov rax, [r9 + CACHE_ITEM.expires_at]
    test rax, rax
    jz ce_valid
    call GetTickCount
    cmp eax, dword ptr [r9 + CACHE_ITEM.expires_at]
    jae ce_expired
ce_valid:
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp ce_exit
ce_next:
    mov r9, [r9 + CACHE_ITEM.hash_next_ptr]
    jmp ce_search
ce_not_found:
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_NOT_FOUND
    jmp ce_exit
ce_expired:
    mov rcx, r14
    mov rdx, r9
    call remove_item
    mov rcx, [r14 + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    mov eax, CACHE_ERROR_EXPIRED
    jmp ce_exit
ce_invalid:
    mov eax, CACHE_ERROR_INVALID
ce_exit:
    add rsp, 40
    pop r14
    pop rdi
    pop rsi
    pop rbx
    ret
cache_exists ENDP

; cache_clear(cache*)
cache_clear PROC PUBLIC
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    mov rsi, rcx
    test rsi, rsi
    jz cc_invalid

    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rbx, [rsi + CACHE_STRUCT.head_ptr]
cc_loop:
    test rbx, rbx
    jz cc_done_clear
    mov rdi, [rbx + CACHE_ITEM.next_ptr]
    mov rcx, [rbx + CACHE_ITEM.key_ptr]
    call free_heap
    mov rcx, [rbx + CACHE_ITEM.value_ptr]
    call free_heap
    mov rcx, rbx
    call free_heap
    mov rbx, rdi
    jmp cc_loop
cc_done_clear:
    mov qword ptr [rsi + CACHE_STRUCT.cache_size], 0
    mov qword ptr [rsi + CACHE_STRUCT.item_count], 0
    mov qword ptr [rsi + CACHE_STRUCT.head_ptr], 0
    mov qword ptr [rsi + CACHE_STRUCT.tail_ptr], 0
    mov rcx, [rsi + CACHE_STRUCT.hash_table_ptr]
    mov rdx, CACHE_HASH_TABLE_SIZE*8
    mov rdi, rcx
    mov rcx, rdx
    xor eax, eax
    rep stosb
    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cc_exit
cc_invalid:
    mov eax, CACHE_ERROR_INVALID
cc_exit:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
cache_clear ENDP

; cache_stats(cache*, stats_ptr)
cache_stats PROC PUBLIC
    push rbx
    push rsi
    sub rsp, 40

    mov rsi, rcx
    mov rbx, rdx
    test rsi, rsi
    jz cst_invalid
    test rbx, rbx
    jz cst_invalid

    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    mov rdx, INFINITE
    call WaitForSingleObject

    mov rax, [rsi + CACHE_STRUCT.cache_size]
    mov [rbx + 0], rax
    mov rax, [rsi + CACHE_STRUCT.max_size]
    mov [rbx + 8], rax
    mov rax, [rsi + CACHE_STRUCT.item_count]
    mov [rbx + 16], rax
    mov rax, [rsi + CACHE_STRUCT.hit_count]
    mov [rbx + 24], rax
    mov rax, [rsi + CACHE_STRUCT.miss_count]
    mov [rbx + 32], rax
    mov rax, [rsi + CACHE_STRUCT.eviction_count]
    mov [rbx + 40], rax

    mov rcx, [rsi + CACHE_STRUCT.cache_mutex]
    call ReleaseMutex
    xor eax, eax
    jmp cst_exit
cst_invalid:
    mov eax, CACHE_ERROR_INVALID
cst_exit:
    add rsp, 40
    pop rsi
    pop rbx
    ret
cache_stats ENDP

END