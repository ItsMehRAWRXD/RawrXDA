; Caching Layer - Redis-compatible in-memory cache
; Phase D Component 1: 1,200 MASM LOC, 8 functions
; Author: RawrXD-QtShell MASM Conversion Project
; Date: December 29, 2025

.686
.model flat, C
option casemap:none

include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\msvcrt.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\msvcrt.lib

include masm_master_defs.inc

.data
; Cache structure (128 bytes)
CACHE_STRUCT struct
    cache_mutex dword ?          ; Mutex for thread safety
    cache_size dword ?           ; Current cache size in bytes
    max_size dword ?             ; Maximum cache size (default 64MB)
    item_count dword ?           ; Number of items in cache
    head_ptr dword ?             ; Pointer to first cache item (LRU)
    tail_ptr dword ?             ; Pointer to last cache item
    hit_count dword ?            ; Cache hits
    miss_count dword ?           ; Cache misses
    eviction_count dword ?       ; Items evicted
    stats_enabled dword ?        ; Statistics enabled flag
    hash_table_ptr dword ?       ; Pointer to hash table
    hash_table_size dword ?      ; Hash table size (prime number)
    allocator_ptr dword ?        ; Memory allocator function
    deallocator_ptr dword ?      ; Memory deallocator function
    reserved dword 16 dup(?)     ; Reserved for future use
CACHE_STRUCT ends

; Cache item structure (64 bytes)
CACHE_ITEM struct
    key_ptr dword ?              ; Pointer to key string
    key_len dword ?              ; Key length
    value_ptr dword ?            ; Pointer to value data
    value_len dword ?            ; Value length
    expires_at dword ?           ; Expiration timestamp (0 = never)
    access_count dword ?         ; Number of accesses
    last_access dword ?          ; Last access timestamp
    prev_ptr dword ?             ; Previous item in LRU list
    next_ptr dword ?             ; Next item in LRU list
    hash_next_ptr dword ?        ; Next item in hash bucket
    flags dword ?                ; Item flags
    reserved dword 4 dup(?)      ; Reserved
CACHE_ITEM ends

; Constants
CACHE_MAX_KEY_SIZE equ 1024      ; Maximum key size (1KB)
CACHE_MAX_VALUE_SIZE equ 1048576 ; Maximum value size (1MB)
CACHE_DEFAULT_SIZE equ 67108864  ; Default cache size (64MB)
CACHE_HASH_TABLE_SIZE equ 8191   ; Hash table size (prime)

; Error codes
CACHE_SUCCESS equ 0
CACHE_ERROR_INVALID equ 1
CACHE_ERROR_OOM equ 2
CACHE_ERROR_NOT_FOUND equ 3
CACHE_ERROR_EXISTS equ 4
CACHE_ERROR_EXPIRED equ 5
CACHE_ERROR_SIZE equ 6

.code

; Initialize cache system
cache_init proc uses ebx esi edi, max_size:dword, stats:dword
    local cache_ptr:dword
    
    ; Validate parameters
    mov eax, max_size
    test eax, eax
    jnz size_ok
    mov eax, CACHE_DEFAULT_SIZE
    
size_ok:
    ; Allocate cache structure
    invoke crt_malloc, sizeof CACHE_STRUCT
    test eax, eax
    jz error_oom
    mov cache_ptr, eax
    
    ; Initialize cache structure
    mov ebx, cache_ptr
    assume ebx:ptr CACHE_STRUCT
    
    ; Create mutex for thread safety
    invoke CreateMutex, NULL, FALSE, NULL
    test eax, eax
    jz cleanup_error
    mov [ebx].cache_mutex, eax
    
    ; Initialize cache fields
    mov [ebx].cache_size, 0
    mov eax, max_size
    test eax, eax
    jnz set_max_size
    mov eax, CACHE_DEFAULT_SIZE
set_max_size:
    mov [ebx].max_size, eax
    mov [ebx].item_count, 0
    mov [ebx].head_ptr, 0
    mov [ebx].tail_ptr, 0
    mov [ebx].hit_count, 0
    mov [ebx].miss_count, 0
    mov [ebx].eviction_count, 0
    mov eax, stats
    mov [ebx].stats_enabled, eax
    
    ; Allocate hash table
    invoke crt_malloc, CACHE_HASH_TABLE_SIZE * 4
    test eax, eax
    jz cleanup_mutex
    mov [ebx].hash_table_ptr, eax
    mov [ebx].hash_table_size, CACHE_HASH_TABLE_SIZE
    
    ; Initialize hash table to zeros
    mov edi, eax
    mov ecx, CACHE_HASH_TABLE_SIZE
    xor eax, eax
    rep stosd
    
    ; Set default allocators
    mov [ebx].allocator_ptr, offset crt_malloc
    mov [ebx].deallocator_ptr, offset crt_free
    
    assume ebx:nothing
    mov eax, cache_ptr
    ret
    
cleanup_mutex:
    invoke CloseHandle, [ebx].cache_mutex
cleanup_error:
    invoke crt_free, cache_ptr
error_oom:
    xor eax, eax
    ret
cache_init endp

; Shutdown cache system
cache_shutdown proc uses ebx esi edi, cache_ptr:dword
    local item_ptr:dword, next_ptr:dword
    
    mov ebx, cache_ptr
    test ebx, ebx
    jz done
    assume ebx:ptr CACHE_STRUCT
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Free all cache items
    mov esi, [ebx].head_ptr
free_loop:
    test esi, esi
    jz free_done
    mov edi, esi
    assume edi:ptr CACHE_ITEM
    mov esi, [edi].next_ptr
    
    ; Free key and value
    invoke crt_free, [edi].key_ptr
    invoke crt_free, [edi].value_ptr
    invoke crt_free, edi
    jmp free_loop
    
free_done:
    assume edi:nothing
    
    ; Free hash table
    invoke crt_free, [ebx].hash_table_ptr
    
    ; Release mutex and close handle
    invoke ReleaseMutex, [ebx].cache_mutex
    invoke CloseHandle, [ebx].cache_mutex
    
    ; Free cache structure
    invoke crt_free, ebx
    
    assume ebx:nothing
done:
    mov eax, CACHE_SUCCESS
    ret
cache_shutdown endp

; Hash function for keys (djb2 algorithm)
hash_key proc uses ebx esi edi, key_ptr:dword, key_len:dword
    mov esi, key_ptr
    mov ecx, key_len
    xor eax, eax
    xor edx, edx
    
hash_loop:
    test ecx, ecx
    jz hash_done
    mov dl, byte ptr [esi]
    imul eax, eax, 33
    add eax, edx
    inc esi
    dec ecx
    jmp hash_loop
    
hash_done:
    ; Modulo hash table size
    xor edx, edx
    mov ecx, CACHE_HASH_TABLE_SIZE
    div ecx
    mov eax, edx
    ret
hash_key endp

; Get item from cache
cache_get proc uses ebx esi edi, cache_ptr:dword, key_ptr:dword, key_len:dword
    local value_ptr:dword, value_len:dword
    
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    ; Validate key
    mov eax, key_ptr
    test eax, eax
    jz error_invalid
    mov eax, key_len
    test eax, eax
    jz error_invalid
    cmp eax, CACHE_MAX_KEY_SIZE
    ja error_size
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Calculate hash
    invoke hash_key, key_ptr, key_len
    mov edx, eax
    
    ; Search hash bucket
    mov esi, [ebx].hash_table_ptr
    mov esi, [esi + edx * 4]
    
search_loop:
    test esi, esi
    jz not_found
    assume esi:ptr CACHE_ITEM
    
    ; Check key match
    mov edi, [esi].key_len
    cmp edi, key_len
    jne next_item
    
    ; Compare keys
    push esi
    mov esi, [esi].key_ptr
    mov edi, key_ptr
    mov ecx, key_len
    repe cmpsb
    pop esi
    jne next_item
    
    ; Check expiration
    mov eax, [esi].expires_at
    test eax, eax
    jz item_valid
    
    ; Get current time
    invoke GetTickCount
    cmp eax, [esi].expires_at
    jae expired
    
item_valid:
    ; Update access statistics
    inc [esi].access_count
    mov [esi].last_access, eax
    
    ; Update cache statistics
    inc [ebx].hit_count
    
    ; Move to front of LRU list
    call update_lru
    
    ; Return value
    mov eax, [esi].value_ptr
    mov value_ptr, eax
    mov eax, [esi].value_len
    mov value_len, eax
    
    ; Release mutex
    invoke ReleaseMutex, [ebx].cache_mutex
    
    mov eax, CACHE_SUCCESS
    ret
    
next_item:
    mov esi, [esi].hash_next_ptr
    jmp search_loop
    
not_found:
    inc [ebx].miss_count
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_ERROR_NOT_FOUND
    ret
    
expired:
    ; Remove expired item
    call remove_item
    inc [ebx].miss_count
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_ERROR_EXPIRED
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
error_size:
    mov eax, CACHE_ERROR_SIZE
    ret
    
update_lru:
    ; If already at head, nothing to do
    mov eax, [ebx].head_ptr
    cmp esi, eax
    je lru_done
    
    ; Remove from current position
    mov edi, [esi].prev_ptr
    mov edx, [esi].next_ptr
    test edi, edi
    jz no_prev
    mov [edi].next_ptr, edx
no_prev:
    test edx, edx
    jz no_next
    mov [edx].prev_ptr, edi
no_next:
    
    ; Update tail if necessary
    cmp esi, [ebx].tail_ptr
    jne not_tail
    mov [ebx].tail_ptr, edi
not_tail:
    
    ; Move to head
    mov eax, [ebx].head_ptr
    mov [esi].prev_ptr, 0
    mov [esi].next_ptr, eax
    test eax, eax
    jz no_old_head
    mov [eax].prev_ptr, esi
no_old_head:
    mov [ebx].head_ptr, esi
    
lru_done:
    ret
    
remove_item:
    ; Remove from hash table
    invoke hash_key, [esi].key_ptr, [esi].key_len
    mov edx, eax
    mov edi, [ebx].hash_table_ptr
    mov edi, [edi + edx * 4]
    
    ; Find and remove from hash chain
    test edi, edi
    jz hash_remove_done
    cmp edi, esi
    jne hash_search
    mov eax, [esi].hash_next_ptr
    mov [ebx].hash_table_ptr[edx * 4], eax
    jmp hash_remove_done
    
hash_search:
    mov eax, [edi].hash_next_ptr
    test eax, eax
    jz hash_remove_done
    cmp eax, esi
    jne next_hash
    mov eax, [esi].hash_next_ptr
    mov [edi].hash_next_ptr, eax
    jmp hash_remove_done
next_hash:
    mov edi, eax
    jmp hash_search
    
hash_remove_done:
    
    ; Remove from LRU list
    mov edi, [esi].prev_ptr
    mov edx, [esi].next_ptr
    test edi, edi
    jz lru_no_prev
    mov [edi].next_ptr, edx
lru_no_prev:
    test edx, edx
    jz lru_no_next
    mov [edx].prev_ptr, edi
lru_no_next:
    
    ; Update head/tail pointers
    cmp esi, [ebx].head_ptr
    jne not_head
    mov [ebx].head_ptr, edx
not_head:
    cmp esi, [ebx].tail_ptr
    jne not_tail2
    mov [ebx].tail_ptr, edi
not_tail2:
    
    ; Update cache statistics
    mov eax, [esi].value_len
    sub [ebx].cache_size, eax
    dec [ebx].item_count
    inc [ebx].eviction_count
    
    ; Free memory
    invoke crt_free, [esi].key_ptr
    invoke crt_free, [esi].value_ptr
    invoke crt_free, esi
    
    ret
    
    assume esi:nothing
    assume ebx:nothing
cache_get endp

; Set item in cache
cache_set proc uses ebx esi edi, cache_ptr:dword, key_ptr:dword, key_len:dword, 
          value_ptr:dword, value_len:dword, ttl:dword
    local item_ptr:dword, hash_index:dword, new_size:dword
    
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    ; Validate parameters
    mov eax, key_ptr
    test eax, eax
    jz error_invalid
    mov eax, key_len
    test eax, eax
    jz error_invalid
    cmp eax, CACHE_MAX_KEY_SIZE
    ja error_size
    mov eax, value_ptr
    test eax, eax
    jz error_invalid
    mov eax, value_len
    test eax, eax
    jz error_invalid
    cmp eax, CACHE_MAX_VALUE_SIZE
    ja error_size
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Calculate new total size
    mov eax, value_len
    add eax, key_len
    add eax, sizeof CACHE_ITEM
    mov new_size, eax
    
    ; Check if we need to evict
    mov eax, [ebx].cache_size
    add eax, new_size
    cmp eax, [ebx].max_size
    jbe size_ok
    
    ; Evict until we have enough space
    call evict_for_space
    
size_ok:
    ; Check if key already exists
    invoke hash_key, key_ptr, key_len
    mov hash_index, eax
    
    mov esi, [ebx].hash_table_ptr
    mov esi, [esi + eax * 4]
    
search_existing:
    test esi, esi
    jz create_new
    assume esi:ptr CACHE_ITEM
    
    ; Check key match
    mov edi, [esi].key_len
    cmp edi, key_len
    jne next_existing
    
    push esi
    mov esi, [esi].key_ptr
    mov edi, key_ptr
    mov ecx, key_len
    repe cmpsb
    pop esi
    jne next_existing
    
    ; Update existing item
    mov eax, value_len
    cmp eax, [esi].value_len
    je size_same
    
    ; Size changed, update cache size
    mov edx, [esi].value_len
    sub [ebx].cache_size, edx
    add [ebx].cache_size, eax
    
    ; Reallocate value
    invoke crt_free, [esi].value_ptr
    invoke crt_malloc, value_len
    test eax, eax
    jz error_oom_locked
    mov [esi].value_ptr, eax
    
size_same:
    ; Copy new value
    mov edi, [esi].value_ptr
    mov esi, value_ptr
    mov ecx, value_len
    rep movsb
    
    ; Update expiration
    mov eax, ttl
    test eax, eax
    jz no_expiration
    invoke GetTickCount
    add eax, ttl
    mov [esi].expires_at, eax
    jmp expiration_set
no_expiration:
    mov [esi].expires_at, 0
expiration_set:
    
    ; Update access time
    invoke GetTickCount
    mov [esi].last_access, eax
    
    ; Move to front of LRU
    call update_lru
    
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
next_existing:
    mov esi, [esi].hash_next_ptr
    jmp search_existing
    
create_new:
    ; Allocate new cache item
    invoke crt_malloc, sizeof CACHE_ITEM
    test eax, eax
    jz error_oom_locked
    mov item_ptr, eax
    
    ; Allocate and copy key
    invoke crt_malloc, key_len
    test eax, eax
    jz free_item_error
    mov edi, eax
    mov esi, key_ptr
    mov ecx, key_len
    rep movsb
    mov ebx, cache_ptr
    assume ebx:ptr CACHE_STRUCT
    mov esi, item_ptr
    assume esi:ptr CACHE_ITEM
    mov [esi].key_ptr, eax
    mov [esi].key_len, key_len
    
    ; Allocate and copy value
    invoke crt_malloc, value_len
    test eax, eax
    jz free_key_error
    mov edi, eax
    mov esi, value_ptr
    mov ecx, value_len
    rep movsb
    mov [esi].value_ptr, eax
    mov [esi].value_len, value_len
    
    ; Set expiration
    mov eax, ttl
    test eax, eax
    jz new_no_expiration
    invoke GetTickCount
    add eax, ttl
    mov [esi].expires_at, eax
    jmp new_expiration_set
new_no_expiration:
    mov [esi].expires_at, 0
new_expiration_set:
    
    ; Initialize other fields
    mov [esi].access_count, 1
    invoke GetTickCount
    mov [esi].last_access, eax
    mov [esi].prev_ptr, 0
    mov [esi].next_ptr, 0
    mov [esi].hash_next_ptr, 0
    mov [esi].flags, 0
    
    ; Add to hash table
    mov eax, hash_index
    mov edi, [ebx].hash_table_ptr
    mov edx, [edi + eax * 4]
    mov [esi].hash_next_ptr, edx
    mov [edi + eax * 4], esi
    
    ; Add to LRU list (at head)
    mov eax, [ebx].head_ptr
    mov [esi].next_ptr, eax
    test eax, eax
    jz no_old_head2
    mov [eax].prev_ptr, esi
no_old_head2:
    mov [ebx].head_ptr, esi
    
    ; Update tail if this is first item
    cmp [ebx].tail_ptr, 0
    jne tail_ok
    mov [ebx].tail_ptr, esi
tail_ok:
    
    ; Update cache statistics
    mov eax, value_len
    add [ebx].cache_size, eax
    inc [ebx].item_count
    
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
free_key_error:
    invoke crt_free, [esi].key_ptr
free_item_error:
    invoke crt_free, item_ptr
error_oom_locked:
    invoke ReleaseMutex, [ebx].cache_mutex
error_oom:
    mov eax, CACHE_ERROR_OOM
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
error_size:
    mov eax, CACHE_ERROR_SIZE
    ret
    
evict_for_space:
    ; Evict least recently used items until we have enough space
    mov eax, new_size
    add eax, [ebx].cache_size
    cmp eax, [ebx].max_size
    jbe evict_done
    
    mov esi, [ebx].tail_ptr
    test esi, esi
    jz evict_done
    
    assume esi:ptr CACHE_ITEM
    
evict_loop:
    call remove_item
    
    ; Check if we have enough space now
    mov eax, new_size
    add eax, [ebx].cache_size
    cmp eax, [ebx].max_size
    jbe evict_done
    
    mov esi, [ebx].tail_ptr
    test esi, esi
    jnz evict_loop
    
evict_done:
    ret
    
    assume esi:nothing
    assume ebx:nothing
cache_set endp

; Delete item from cache
cache_delete proc uses ebx esi edi, cache_ptr:dword, key_ptr:dword, key_len:dword
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    ; Validate key
    mov eax, key_ptr
    test eax, eax
    jz error_invalid
    mov eax, key_len
    test eax, eax
    jz error_invalid
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Calculate hash
    invoke hash_key, key_ptr, key_len
    mov edx, eax
    
    ; Search hash bucket
    mov esi, [ebx].hash_table_ptr
    mov esi, [esi + edx * 4]
    
search_delete:
    test esi, esi
    jz not_found_delete
    assume esi:ptr CACHE_ITEM
    
    ; Check key match
    mov edi, [esi].key_len
    cmp edi, key_len
    jne next_delete
    
    push esi
    mov esi, [esi].key_ptr
    mov edi, key_ptr
    mov ecx, key_len
    repe cmpsb
    pop esi
    jne next_delete
    
    ; Found item, remove it
    call remove_item
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
next_delete:
    mov esi, [esi].hash_next_ptr
    jmp search_delete
    
not_found_delete:
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_ERROR_NOT_FOUND
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
    assume esi:nothing
    assume ebx:nothing
cache_delete endp

; Check if item exists in cache
cache_exists proc uses ebx esi edi, cache_ptr:dword, key_ptr:dword, key_len:dword
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    ; Validate key
    mov eax, key_ptr
    test eax, eax
    jz error_invalid
    mov eax, key_len
    test eax, eax
    jz error_invalid
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Calculate hash
    invoke hash_key, key_ptr, key_len
    mov edx, eax
    
    ; Search hash bucket
    mov esi, [ebx].hash_table_ptr
    mov esi, [esi + edx * 4]
    
search_exists:
    test esi, esi
    jz not_found_exists
    assume esi:ptr CACHE_ITEM
    
    ; Check key match
    mov edi, [esi].key_len
    cmp edi, key_len
    jne next_exists
    
    push esi
    mov esi, [esi].key_ptr
    mov edi, key_ptr
    mov ecx, key_len
    repe cmpsb
    pop esi
    jne next_exists
    
    ; Check expiration
    mov eax, [esi].expires_at
    test eax, eax
    jz exists_valid
    
    invoke GetTickCount
    cmp eax, [esi].expires_at
    jae expired_exists
    
exists_valid:
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
next_exists:
    mov esi, [esi].hash_next_ptr
    jmp search_exists
    
not_found_exists:
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_ERROR_NOT_FOUND
    ret
    
expired_exists:
    ; Remove expired item
    call remove_item
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_ERROR_EXPIRED
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
    assume esi:nothing
    assume ebx:nothing
cache_exists endp

; Clear all items from cache
cache_clear proc uses ebx esi edi, cache_ptr:dword
    local item_ptr:dword, next_ptr:dword
    
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Free all cache items
    mov esi, [ebx].head_ptr
clear_loop:
    test esi, esi
    jz clear_done
    mov edi, esi
    assume edi:ptr CACHE_ITEM
    mov esi, [edi].next_ptr
    
    ; Free key and value
    invoke crt_free, [edi].key_ptr
    invoke crt_free, [edi].value_ptr
    invoke crt_free, edi
    jmp clear_loop
    
clear_done:
    assume edi:nothing
    
    ; Reset cache state
    mov [ebx].cache_size, 0
    mov [ebx].item_count, 0
    mov [ebx].head_ptr, 0
    mov [ebx].tail_ptr, 0
    
    ; Clear hash table
    mov edi, [ebx].hash_table_ptr
    mov ecx, [ebx].hash_table_size
    xor eax, eax
    rep stosd
    
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
    assume ebx:nothing
cache_clear endp

; Get cache statistics
cache_stats proc uses ebx esi edi, cache_ptr:dword, stats_ptr:dword
    mov ebx, cache_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr CACHE_STRUCT
    
    mov esi, stats_ptr
    test esi, esi
    jz error_invalid
    
    ; Lock cache
    invoke WaitForSingleObject, [ebx].cache_mutex, INFINITE
    
    ; Copy statistics
    mov eax, [ebx].cache_size
    mov [esi], eax
    mov eax, [ebx].max_size
    mov [esi+4], eax
    mov eax, [ebx].item_count
    mov [esi+8], eax
    mov eax, [ebx].hit_count
    mov [esi+12], eax
    mov eax, [ebx].miss_count
    mov [esi+16], eax
    mov eax, [ebx].eviction_count
    mov [esi+20], eax
    
    invoke ReleaseMutex, [ebx].cache_mutex
    mov eax, CACHE_SUCCESS
    ret
    
error_invalid:
    mov eax, CACHE_ERROR_INVALID
    ret
    
    assume ebx:nothing
cache_stats endp

end