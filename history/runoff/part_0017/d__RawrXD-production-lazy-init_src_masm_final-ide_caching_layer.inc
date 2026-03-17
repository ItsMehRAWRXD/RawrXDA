; Caching Layer Header (x64)
; Redis-compatible in-memory cache API

IFNDEF CACHING_LAYER_INC
CACHING_LAYER_INC equ 1

; Cache error codes
CACHE_SUCCESS       equ 0
CACHE_ERROR_INVALID equ 1
CACHE_ERROR_OOM     equ 2
CACHE_ERROR_NOT_FOUND equ 3
CACHE_ERROR_EXISTS  equ 4
CACHE_ERROR_EXPIRED equ 5
CACHE_ERROR_SIZE    equ 6

; Cache structure (64-bit pointers)
CACHE_STRUCT STRUCT
    cache_mutex        QWORD ?
    cache_size         QWORD ?
    max_size           QWORD ?
    item_count         QWORD ?
    head_ptr           QWORD ?
    tail_ptr           QWORD ?
    hit_count          QWORD ?
    miss_count         QWORD ?
    eviction_count     QWORD ?
    stats_enabled      QWORD ?
    hash_table_ptr     QWORD ?
    hash_table_size    QWORD ?
    allocator_ptr      QWORD ?
    deallocator_ptr    QWORD ?
    reserved           QWORD 16 dup (?)
CACHE_STRUCT ENDS

; Cache item structure (64-bit pointers)
CACHE_ITEM STRUCT
    key_ptr        QWORD ?
    key_len        QWORD ?
    value_ptr      QWORD ?
    value_len      QWORD ?
    expires_at     QWORD ?
    access_count   QWORD ?
    last_access    QWORD ?
    prev_ptr       QWORD ?
    next_ptr       QWORD ?
    hash_next_ptr  QWORD ?
    flags          QWORD ?
    reserved       QWORD 4 dup (?)
CACHE_ITEM ENDS

; Constants
CACHE_MAX_KEY_SIZE    equ 1024
CACHE_MAX_VALUE_SIZE  equ 1048576
CACHE_DEFAULT_SIZE    equ 67108864
CACHE_HASH_TABLE_SIZE equ 8191

; Function declarations
IFNDEF CACHING_LAYER_IMPLEMENTATION
EXTERN cache_init:proc
EXTERN cache_shutdown:proc
EXTERN cache_get:proc
EXTERN cache_set:proc
EXTERN cache_delete:proc
EXTERN cache_exists:proc
EXTERN cache_clear:proc
EXTERN cache_stats:proc
ENDIF

