; Stream Processor Header File
; Phase D Component 2: Event streaming pipeline

; Stream error codes
STREAM_SUCCESS equ 0
STREAM_ERROR_INVALID equ 1
STREAM_ERROR_OOM equ 2
STREAM_ERROR_NOT_FOUND equ 3
STREAM_ERROR_EXISTS equ 4
STREAM_ERROR_FULL equ 5
STREAM_ERROR_EMPTY equ 6
STREAM_ERROR_OFFSET equ 7

; Stream processor structure
STREAM_PROCESSOR struct
    processor_mutex dword ?
    stream_count dword ?
    max_streams dword ?
    consumer_count dword ?
    max_consumers dword ?
    total_messages dword ?
    total_bytes dword ?
    streams_ptr dword ?
    consumers_ptr dword ?
    allocator_ptr dword ?
    deallocator_ptr dword ?
    stats_enabled dword ?
    reserved dword 16 dup(?)
STREAM_PROCESSOR ends

; Stream structure
STREAM struct
    stream_id dword ?
    name_ptr dword ?
    name_len dword ?
    message_count dword ?
    first_message_ptr dword ?
    last_message_ptr dword ?
    consumer_count dword ?
    max_messages dword ?
    retention_ms dword ?
    created_at dword ?
    flags dword ?
    reserved dword 6 dup(?)
STREAM ends

; Message structure
MESSAGE struct
    message_id dword ?
    stream_id dword ?
    timestamp dword ?
    data_ptr dword ?
    data_len dword ?
    next_ptr dword ?
    prev_ptr dword ?
    ack_count dword ?
    nack_count dword ?
    flags dword ?
    reserved dword 4 dup(?)
MESSAGE ends

; Consumer structure
CONSUMER struct
    consumer_id dword ?
    stream_id dword ?
    name_ptr dword ?
    name_len dword ?
    current_offset dword ?
    last_message_id dword ?
    ack_count dword ?
    nack_count dword ?
    created_at dword ?
    flags dword ?
    reserved dword 6 dup(?)
CONSUMER ends

; Constants
STREAM_MAX_NAME_LEN equ 256
STREAM_MAX_MESSAGES equ 1000000
CONSUMER_MAX_NAME_LEN equ 256
MESSAGE_MAX_SIZE equ 1048576

; Function declarations
EXTERN stream_processor_init:proc
EXTERN stream_processor_shutdown:proc
EXTERN stream_create:proc
EXTERN stream_subscribe:proc
EXTERN stream_publish:proc
EXTERN stream_consume:proc
EXTERN stream_ack:proc
EXTERN stream_nack:proc
EXTERN stream_get_offset:proc
EXTERN stream_seek:proc
EXTERN stream_stats:proc
EXTERN stream_list:proc