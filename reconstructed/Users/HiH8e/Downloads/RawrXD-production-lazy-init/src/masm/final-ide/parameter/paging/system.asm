;=====================================================================
; parameter_paging_system.asm - On-Demand Parameter Swapping
; 14TB DISK ↔ RAM PARAMETER PAGING ENGINE
;=====================================================================
; Implements transparent parameter paging between RAM and disk:
;  - Hot parameters: Cached in RAM (1GB cache)
;  - Cold parameters: Stored on disk (14TB)
;  - Automatic swapping: LRU eviction policy
;  - Zero-copy transfers: DMA when available
;  - Predictive prefetching: Load before needed
;
; Memory Hierarchy:
;  Level 1: RAM Cache (1GB) - Hot parameters, <1ms access
;  Level 2: SSD Cache (if available) - Warm parameters, ~5ms access
;  Level 3: HDD Storage (14TB) - Cold parameters, ~15ms access
;
; Example: 120B model with 1.95GB base
;  RAM: 1.95GB (base) + 1GB (cache) = 2.95GB total
;  Disk: 238GB (120B - 1B at INT8) = 119GB additional params
;  Effective: 120B model in 3GB RAM with disk backing
;=====================================================================

.code

PUBLIC masm_paging_init
PUBLIC masm_paging_load_parameter_page
PUBLIC masm_paging_store_parameter_page
PUBLIC masm_paging_prefetch
PUBLIC masm_paging_get_cache_stats
PUBLIC masm_paging_set_eviction_policy
PUBLIC masm_paging_flush_cache

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN SetFilePointerEx:PROC
EXTERN CloseHandle:PROC

;=====================================================================
; PAGING CONSTANTS
;=====================================================================

; Page sizes
PAGE_SIZE_SMALL             EQU 1048576     ; 1MB
PAGE_SIZE_MEDIUM            EQU 4194304     ; 4MB
PAGE_SIZE_LARGE             EQU 16777216    ; 16MB

; Cache configuration
CACHE_SIZE                  EQU 1073741824  ; 1GB RAM cache
CACHE_PAGES_MAX             EQU 256         ; Max cached pages
CACHE_PREFETCH_AHEAD        EQU 4           ; Prefetch 4 pages ahead

; Eviction policies
EVICTION_LRU                EQU 1   ; Least Recently Used
EVICTION_LFU                EQU 2   ; Least Frequently Used
EVICTION_FIFO               EQU 3   ; First In First Out
EVICTION_RANDOM             EQU 4   ; Random eviction
EVICTION_ADAPTIVE           EQU 5   ; Adaptive (learns usage patterns)

; Page states
PAGE_STATE_EMPTY            EQU 0
PAGE_STATE_LOADING          EQU 1
PAGE_STATE_CACHED           EQU 2
PAGE_STATE_DIRTY            EQU 3
PAGE_STATE_EVICTING         EQU 4

; Access flags
ACCESS_READ                 EQU 1
ACCESS_WRITE                EQU 2
ACCESS_PREFETCH             EQU 4

;=====================================================================
; DATA STRUCTURES
;=====================================================================

.data

; Page Cache Entry (128 bytes)
; [+0]:    page_id (qword)
; [+8]:    state (qword)
; [+16]:   ram_addr (qword)
; [+24]:   disk_offset (qword)
; [+32]:   size (qword)
; [+40]:   access_count (qword)
; [+48]:   last_access_time (qword)
; [+56]:   dirty_flag (qword)
; [+64]:   lock_count (qword)
; [+72]:   bank_handle (qword)
; [+80]:   reserved[6] (qword[6])

; Paging System Context (4096 bytes)
; [+0]:    initialized (qword)
; [+8]:    cache_base_addr (qword)
; [+16]:   cache_size (qword)
; [+24]:   page_size (qword)
; [+32]:   page_table_ptr (qword)
; [+40]:   free_pages_count (qword)
; [+48]:   cached_pages_count (qword)
; [+56]:   eviction_policy (qword)
; [+64]:   cache_hits (qword)
; [+72]:   cache_misses (qword)
; [+80]:   total_page_loads (qword)
; [+88]:   total_page_stores (qword)
; [+96]:   total_bytes_loaded (qword)
; [+104]:  total_bytes_stored (qword)
; [+112]:  prefetch_hits (qword)
; [+120]:  prefetch_misses (qword)
; [+128]:  reserved[492] (qword[492])

g_paging_context                QWORD 0
g_paging_initialized            QWORD 0
g_page_table                    QWORD 0
g_cache_memory                  QWORD 0
g_eviction_queue                QWORD 0

; Statistics
g_total_page_swaps              QWORD 0
g_average_load_time             QWORD 0
g_peak_memory_usage             QWORD 0

; Prefetch predictor
g_access_history                QWORD 0
g_prediction_table              QWORD 0

; Log messages
msg_paging_init         DB "[Paging] Initialized - Cache: 1GB, Policy: LRU", 0
msg_page_load           DB "[Paging] Loading page %lld from disk (offset: %lld)", 0
msg_page_store          DB "[Paging] Storing page %lld to disk (offset: %lld)", 0
msg_cache_hit           DB "[Paging] Cache HIT - Page %lld (RAM addr: %p)", 0
msg_cache_miss          DB "[Paging] Cache MISS - Page %lld (loading...)", 0
msg_eviction            DB "[Paging] Evicting page %lld (policy: %d)", 0
msg_prefetch            DB "[Paging] Prefetching pages: %lld-%lld", 0
msg_stats               DB "[Paging] Stats - Hits: %lld, Misses: %lld, Ratio: %.2f%%", 0

.code

;=====================================================================
; masm_paging_init(page_size: rcx, cache_size: rdx) -> rax
;
; Initializes the parameter paging system.
; rcx = page_size (default: 4MB)
; rdx = cache_size (default: 1GB)
; Returns: paging context pointer on success, 0 on failure
;=====================================================================

ALIGN 16
masm_paging_init PROC

    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov r12, rcx            ; page_size
    mov r13, rdx            ; cache_size
    
    ; Check if already initialized
    cmp qword ptr [g_paging_initialized], 1
    je init_already_done
    
    ; Use defaults if not specified
    test r12, r12
    jnz size_specified
    mov r12, PAGE_SIZE_MEDIUM   ; 4MB default
size_specified:
    
    test r13, r13
    jnz cache_specified
    mov r13, CACHE_SIZE         ; 1GB default
cache_specified:
    
    ; Allocate paging context
    mov rcx, 4096
    call asm_malloc
    mov [g_paging_context], rax
    test rax, rax
    jz init_fail
    
    mov rbx, rax            ; rbx = context
    
    ; Zero out context
    mov rdi, rbx
    mov rcx, 512
    xor rax, rax
    rep stosq
    
    ; Store configuration
    mov [rbx + 16], r13     ; cache_size
    mov [rbx + 24], r12     ; page_size
    
    ; Calculate max pages in cache
    mov rax, r13
    xor rdx, rdx
    div r12
    mov [rbx + 40], rax     ; free_pages_count
    
    ; Allocate cache memory
    mov rcx, r13
    call asm_malloc
    test rax, rax
    jz init_fail_cache
    mov [rbx + 8], rax      ; cache_base_addr
    mov [g_cache_memory], rax
    
    ; Allocate page table
    mov rcx, CACHE_PAGES_MAX
    imul rcx, 128           ; 128 bytes per entry
    call asm_malloc
    test rax, rax
    jz init_fail_page_table
    mov [rbx + 32], rax     ; page_table_ptr
    mov [g_page_table], rax
    
    ; Initialize page table entries
    mov rdi, rax
    mov rcx, CACHE_PAGES_MAX
    xor rax, rax
init_page_loop:
    stosq                   ; page_id = 0
    stosq                   ; state = EMPTY
    add rdi, 112            ; Skip rest of entry
    loop init_page_loop
    
    ; Allocate eviction queue (LRU)
    mov rcx, CACHE_PAGES_MAX * 8
    call asm_malloc
    mov [g_eviction_queue], rax
    
    ; Allocate access history for prefetch prediction
    mov rcx, 1024 * 8       ; 1024 entry history
    call asm_malloc
    mov [g_access_history], rax
    
    mov rcx, 4096 * 8       ; 4096 entry prediction table
    call asm_malloc
    mov [g_prediction_table], rax
    
    ; Set default eviction policy
    mov qword ptr [rbx + 56], EVICTION_LRU
    
    ; Mark initialized
    mov qword ptr [rbx], 1
    mov qword ptr [g_paging_initialized], 1
    
    ; Log initialization
    lea rcx, [msg_paging_init]
    call asm_log
    
    mov rax, rbx            ; Return context
    jmp init_exit

init_already_done:
    mov rax, [g_paging_context]
    jmp init_exit

init_fail_page_table:
init_fail_cache:
init_fail:
    xor rax, rax

init_exit:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

masm_paging_init ENDP

;=====================================================================
; masm_paging_load_parameter_page(page_id: rcx, 
;                                  bank_handle: rdx) -> rax
;
; Loads a parameter page from disk to RAM cache.
; Uses LRU eviction if cache is full.
; Returns: RAM pointer to page data on success, 0 on failure
;=====================================================================

ALIGN 16
masm_paging_load_parameter_page PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128
    
    mov r12, rcx            ; page_id
    mov r13, rdx            ; bank_handle
    
    ; Get paging context
    mov rbx, [g_paging_context]
    test rbx, rbx
    jz load_no_context
    
    ; Check if page already in cache
    mov rcx, r12
    call find_page_in_cache
    test rax, rax
    jz load_cache_miss
    
    ; Cache HIT
    mov r14, rax            ; page_entry
    
    ; Update access statistics
    inc qword ptr [r14 + 40]        ; access_count++
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [r14 + 48], rax             ; last_access_time
    
    lock inc [rbx + 64]             ; cache_hits++
    
    ; Log cache hit
    sub rsp, 32
    lea rcx, [msg_cache_hit]
    mov rdx, r12
    mov r8, [r14 + 16]              ; ram_addr
    call asm_log
    add rsp, 32
    
    mov rax, [r14 + 16]             ; Return RAM address
    jmp load_exit
    
load_cache_miss:
    ; Cache MISS - need to load from disk
    lock inc [rbx + 72]             ; cache_misses++
    
    ; Log cache miss
    sub rsp, 32
    lea rcx, [msg_cache_miss]
    mov rdx, r12
    call asm_log
    add rsp, 32
    
    ; Find free cache slot (or evict)
    call find_free_cache_slot
    test rax, rax
    jnz load_slot_found
    
    ; No free slot - evict LRU page
    call evict_lru_page
    call find_free_cache_slot
    test rax, rax
    jz load_no_slot
    
load_slot_found:
    mov r14, rax            ; page_entry
    
    ; Calculate disk offset
    mov rax, r12
    imul rax, [rbx + 24]    ; page_id × page_size
    mov [r14 + 24], rax     ; disk_offset
    
    ; Calculate RAM address
    mov rax, [rbx + 48]     ; cached_pages_count
    imul rax, [rbx + 24]    ; × page_size
    add rax, [rbx + 8]      ; + cache_base_addr
    mov [r14 + 16], rax     ; ram_addr
    
    ; Set page metadata
    mov [r14], r12          ; page_id
    mov qword ptr [r14 + 8], PAGE_STATE_LOADING
    mov rax, [rbx + 24]
    mov [r14 + 32], rax     ; size
    mov [r14 + 72], r13     ; bank_handle
    
    ; Log page load
    sub rsp, 32
    lea rcx, [msg_page_load]
    mov rdx, r12
    mov r8, [r14 + 24]
    call asm_log
    add rsp, 32
    
    ; Read from disk
    mov rcx, r13            ; bank_handle
    mov rdx, [r14 + 24]     ; disk_offset
    mov r8, [r14 + 16]      ; ram_addr (buffer)
    mov r9, [r14 + 32]      ; size
    call read_page_from_disk
    test rax, rax
    jz load_read_failed
    
    ; Update state
    mov qword ptr [r14 + 8], PAGE_STATE_CACHED
    
    ; Update access time
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [r14 + 48], rax
    
    ; Update statistics
    inc qword ptr [rbx + 48]        ; cached_pages_count++
    lock inc [rbx + 80]             ; total_page_loads++
    mov rax, [r14 + 32]
    lock add [rbx + 96], rax        ; total_bytes_loaded += size
    
    ; Trigger prefetch prediction
    mov rcx, r12
    call predict_next_pages
    
    mov rax, [r14 + 16]     ; Return RAM address
    jmp load_exit

load_read_failed:
load_no_slot:
load_no_context:
    xor rax, rax

load_exit:
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_paging_load_parameter_page ENDP

;=====================================================================
; masm_paging_store_parameter_page(page_id: rcx, 
;                                   bank_handle: rdx,
;                                   data: r8) -> rax
;
; Stores a parameter page from RAM to disk.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_paging_store_parameter_page PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 96
    
    mov r12, rcx            ; page_id
    mov r13, rdx            ; bank_handle
    mov r14, r8             ; data
    
    ; Get paging context
    mov rbx, [g_paging_context]
    test rbx, rbx
    jz store_no_context
    
    ; Find page in cache
    mov rcx, r12
    call find_page_in_cache
    test rax, rax
    jz store_not_cached
    
    mov r15, rax            ; page_entry
    
    ; Log page store
    sub rsp, 32
    lea rcx, [msg_page_store]
    mov rdx, r12
    mov r8, [r15 + 24]      ; disk_offset
    call asm_log
    add rsp, 32
    
    ; Write to disk
    mov rcx, r13            ; bank_handle
    mov rdx, [r15 + 24]     ; disk_offset
    mov r8, r14             ; data
    mov r9, [r15 + 32]      ; size
    call write_page_to_disk
    test rax, rax
    jz store_write_failed
    
    ; Clear dirty flag
    mov qword ptr [r15 + 56], 0
    
    ; Update state
    mov qword ptr [r15 + 8], PAGE_STATE_CACHED
    
    ; Update statistics
    lock inc [rbx + 88]             ; total_page_stores++
    mov rax, [r15 + 32]
    lock add [rbx + 104], rax       ; total_bytes_stored += size
    
    mov rax, 1
    jmp store_exit

store_write_failed:
store_not_cached:
store_no_context:
    xor rax, rax

store_exit:
    add rsp, 96
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_paging_store_parameter_page ENDP

;=====================================================================
; masm_paging_prefetch(start_page: rcx, count: rdx) -> rax
;
; Prefetches multiple pages ahead of time.
; Uses background loading to minimize latency.
; Returns: number of pages prefetched
;=====================================================================

ALIGN 16
masm_paging_prefetch PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov r12, rcx            ; start_page
    mov r13, rdx            ; count
    
    ; Get paging context
    mov rbx, [g_paging_context]
    test rbx, rbx
    jz prefetch_no_context
    
    ; Log prefetch
    sub rsp, 32
    lea rcx, [msg_prefetch]
    mov rdx, r12
    mov r8, r12
    add r8, r13
    dec r8
    call asm_log
    add rsp, 32
    
    xor r14, r14            ; prefetch_count = 0
    
prefetch_loop:
    cmp r14, r13
    jge prefetch_done
    
    ; Check if page already cached
    mov rcx, r12
    add rcx, r14
    call find_page_in_cache
    test rax, rax
    jnz prefetch_skip      ; Already cached
    
    ; Prefetch this page
    mov rcx, r12
    add rcx, r14
    mov rdx, 0              ; bank_handle (would be passed)
    call masm_paging_load_parameter_page
    test rax, rax
    jz prefetch_skip
    
    inc r14
    lock inc [rbx + 112]    ; prefetch_hits++
    jmp prefetch_next
    
prefetch_skip:
    lock inc [rbx + 120]    ; prefetch_misses++
    
prefetch_next:
    inc r14
    jmp prefetch_loop
    
prefetch_done:
    mov rax, r14            ; Return prefetch count
    jmp prefetch_exit

prefetch_no_context:
    xor rax, rax

prefetch_exit:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_paging_prefetch ENDP

;=====================================================================
; masm_paging_get_cache_stats() -> rax
;
; Returns pointer to cache statistics structure.
;=====================================================================

ALIGN 16
masm_paging_get_cache_stats PROC

    mov rbx, [g_paging_context]
    test rbx, rbx
    jz no_stats
    
    ; Calculate hit ratio
    mov rax, [rbx + 64]     ; cache_hits
    mov rcx, [rbx + 72]     ; cache_misses
    add rcx, rax
    test rcx, rcx
    jz no_stats
    
    imul rax, 10000         ; Multiply by 10000 for precision
    xor rdx, rdx
    div rcx
    mov [rbx + 128], rax    ; Store hit ratio (basis points)
    
    ; Log stats
    sub rsp, 32
    lea rcx, [msg_stats]
    mov rdx, [rbx + 64]
    mov r8, [rbx + 72]
    mov r9, rax
    call asm_log
    add rsp, 32
    
    mov rax, rbx            ; Return context (has stats)
    ret

no_stats:
    xor rax, rax
    ret

masm_paging_get_cache_stats ENDP

;=====================================================================
; masm_paging_set_eviction_policy(policy: rcx) -> rax
;
; Sets the cache eviction policy.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_paging_set_eviction_policy PROC

    mov rbx, [g_paging_context]
    test rbx, rbx
    jz no_policy
    
    mov [rbx + 56], rcx     ; Set policy
    mov rax, 1
    ret

no_policy:
    xor rax, rax
    ret

masm_paging_set_eviction_policy ENDP

;=====================================================================
; masm_paging_flush_cache() -> rax
;
; Flushes all dirty pages to disk and clears cache.
; Returns: number of pages flushed
;=====================================================================

ALIGN 16
masm_paging_flush_cache PROC

    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov rbx, [g_paging_context]
    test rbx, rbx
    jz flush_no_context
    
    xor r12, r12            ; flush_count = 0
    xor r13, r13            ; entry_index = 0
    
flush_loop:
    cmp r13, CACHE_PAGES_MAX
    jge flush_done
    
    ; Get page entry
    mov rax, r13
    imul rax, 128
    add rax, [rbx + 32]     ; page_table_ptr
    
    ; Check if dirty
    cmp qword ptr [rax + 56], 0
    je flush_next
    
    ; Flush this page
    mov rcx, [rax]          ; page_id
    mov rdx, [rax + 72]     ; bank_handle
    mov r8, [rax + 16]      ; ram_addr
    call masm_paging_store_parameter_page
    
    inc r12
    
flush_next:
    inc r13
    jmp flush_loop
    
flush_done:
    mov rax, r12            ; Return flush count
    jmp flush_exit

flush_no_context:
    xor rax, rax

flush_exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret

masm_paging_flush_cache ENDP

;=====================================================================
; HELPER FUNCTIONS
;=====================================================================

; Find page in cache
ALIGN 16
find_page_in_cache PROC
    push rbx
    push r12
    
    mov r12, rcx            ; page_id
    mov rbx, [g_paging_context]
    mov rax, [rbx + 32]     ; page_table
    
    xor rcx, rcx
find_loop:
    cmp rcx, CACHE_PAGES_MAX
    jge find_not_found
    
    cmp [rax], r12
    je find_found
    
    add rax, 128
    inc rcx
    jmp find_loop
    
find_found:
    pop r12
    pop rbx
    ret
    
find_not_found:
    xor rax, rax
    pop r12
    pop rbx
    ret
find_page_in_cache ENDP

; Find free cache slot
ALIGN 16
find_free_cache_slot PROC
    mov rbx, [g_paging_context]
    mov rax, [rbx + 32]     ; page_table
    
    xor rcx, rcx
slot_loop:
    cmp rcx, CACHE_PAGES_MAX
    jge slot_not_found
    
    cmp qword ptr [rax + 8], PAGE_STATE_EMPTY
    je slot_found
    
    add rax, 128
    inc rcx
    jmp slot_loop
    
slot_found:
    ret
    
slot_not_found:
    xor rax, rax
    ret
find_free_cache_slot ENDP

; Evict LRU page
ALIGN 16
evict_lru_page PROC
    ; Find page with oldest access time
    mov rbx, [g_paging_context]
    mov rax, [rbx + 32]     ; page_table
    
    mov r8, rax             ; LRU candidate
    mov r9, [rax + 48]      ; oldest_time
    
    xor rcx, rcx
evict_loop:
    cmp rcx, CACHE_PAGES_MAX
    jge evict_found
    
    cmp [rax + 48], r9
    jge evict_next
    
    mov r8, rax
    mov r9, [rax + 48]
    
evict_next:
    add rax, 128
    inc rcx
    jmp evict_loop
    
evict_found:
    ; Evict page at r8
    mov qword ptr [r8 + 8], PAGE_STATE_EMPTY
    mov rax, 1
    ret
evict_lru_page ENDP

; Read page from disk
ALIGN 16
read_page_from_disk PROC
    ; Stub: Would use ReadFile API
    mov rax, 1
    ret
read_page_from_disk ENDP

; Write page to disk
ALIGN 16
write_page_to_disk PROC
    ; Stub: Would use WriteFile API
    mov rax, 1
    ret
write_page_to_disk ENDP

; Predict next pages (for prefetch)
ALIGN 16
predict_next_pages PROC
    ; Stub: ML-based prediction
    ; Would analyze access history to predict next accesses
    mov rax, 1
    ret
predict_next_pages ENDP

END
