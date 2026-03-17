; Caching Layer Header File
; Phase D Component 1: Redis-compatible caching

; Cache error codes
CACHE_SUCCESS equ 0
CACHE_ERROR_INVALID equ 1
CACHE_ERROR_OOM equ 2
CACHE_ERROR_NOT_FOUND equ 3
CACHE_ERROR_EXISTS equ 4
CACHE_ERROR_EXPIRED equ 5
CACHE_ERROR_SIZE equ 6

; Cache structure (for external reference)
CACHE_STRUCT struct
    cache_mutex dword ?
    cache_size dword ?
    max_size dword ?
    item_count dword ?
    head_ptr dword ?
    tail_ptr dword ?
    hit_count dword ?
    miss_count dword ?
    eviction_count dword ?
    stats_enabled dword ?
    hash_table_ptr dword ?
    hash_table_size dword ?
    allocator_ptr dword ?
    deallocator_ptr dword ?
    reserved dword 16 dup(?)
CACHE_STRUCT ends

; Cache item structure
CACHE_ITEM struct
    key_ptr dword ?
    key_len dword ?
    value_ptr dword ?
    value_len dword ?
    expires_at dword ?
    access_count dword ?
    last_access dword ?
    prev_ptr dword ?
    next_ptr dword ?
    hash_next_ptr dword ?
    flags dword ?
    reserved dword 4 dup(?)
CACHE_ITEM ends

; Constants
CACHE_MAX_KEY_SIZE equ 1024
CACHE_MAX_VALUE_SIZE equ 1048576
CACHE_DEFAULT_SIZE equ 67108864
CACHE_HASH_TABLE_SIZE equ 8191

; Function declarations
EXTERN cache_init:proc
EXTERN cache_shutdown:proc
EXTERN cache_get:proc
EXTERN cache_set:proc
EXTERN cache_delete:proc
EXTERN cache_exists:proc
EXTERN cache_clear:proc
EXTERN cache_stats:proc