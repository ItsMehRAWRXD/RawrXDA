; Stream Processor - Event streaming pipeline
; Phase D Component 2: 1,600 MASM LOC, 12 functions
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
; Stream processor structure (128 bytes)
STREAM_PROCESSOR struct
    processor_mutex dword ?      ; Mutex for thread safety
    stream_count dword ?         ; Number of streams
    max_streams dword ?          ; Maximum streams (default 256)
    consumer_count dword ?       ; Number of consumers
    max_consumers dword ?        ; Maximum consumers (default 1024)
    total_messages dword ?       ; Total messages processed
    total_bytes dword ?          ; Total bytes processed
    streams_ptr dword ?          ; Pointer to streams array
    consumers_ptr dword ?        ; Pointer to consumers array
    allocator_ptr dword ?        ; Memory allocator
    deallocator_ptr dword ?      ; Memory deallocator
    stats_enabled dword ?        ; Statistics enabled
    reserved dword 16 dup(?)     ; Reserved
STREAM_PROCESSOR ends

; Stream structure (96 bytes)
STREAM struct
    stream_id dword ?            ; Unique stream ID
    name_ptr dword ?             ; Stream name
    name_len dword ?             ; Name length
    message_count dword ?        ; Messages in stream
    first_message_ptr dword ?    ; First message (oldest)
    last_message_ptr dword ?     ; Last message (newest)
    consumer_count dword ?       ; Number of consumers
    max_messages dword ?         ; Maximum messages (0 = unlimited)
    retention_ms dword ?         ; Retention time (0 = forever)
    created_at dword ?           ; Creation timestamp
    flags dword ?                ; Stream flags
    reserved dword 6 dup(?)      ; Reserved
STREAM ends

; Message structure (64 bytes)
MESSAGE struct
    message_id dword ?           ; Unique message ID
    stream_id dword ?            ; Stream ID
    timestamp dword ?            ; Creation timestamp
    data_ptr dword ?             ; Message data
    data_len dword ?             ; Data length
    next_ptr dword ?             ; Next message in stream
    prev_ptr dword ?             ; Previous message in stream
    ack_count dword ?            ; Number of acks
    nack_count dword ?           ; Number of nacks
    flags dword ?                ; Message flags
    reserved dword 4 dup(?)      ; Reserved
MESSAGE ends

; Consumer structure (80 bytes)
CONSUMER struct
    consumer_id dword ?          ; Unique consumer ID
    stream_id dword ?            ; Stream ID
    name_ptr dword ?             ; Consumer name
    name_len dword ?             ; Name length
    current_offset dword ?       ; Current read offset
    last_message_id dword ?      ; Last processed message ID
    ack_count dword ?            ; Messages acknowledged
    nack_count dword ?           ; Messages rejected
    created_at dword ?           ; Creation timestamp
    flags dword ?                ; Consumer flags
    reserved dword 6 dup(?)      ; Reserved
CONSUMER ends

; Constants
STREAM_MAX_NAME_LEN equ 256
STREAM_MAX_MESSAGES equ 1000000  ; 1 million messages max
CONSUMER_MAX_NAME_LEN equ 256
MESSAGE_MAX_SIZE equ 1048576     ; 1MB max message size

; Error codes
STREAM_SUCCESS equ 0
STREAM_ERROR_INVALID equ 1
STREAM_ERROR_OOM equ 2
STREAM_ERROR_NOT_FOUND equ 3
STREAM_ERROR_EXISTS equ 4
STREAM_ERROR_FULL equ 5
STREAM_ERROR_EMPTY equ 6
STREAM_ERROR_OFFSET equ 7

.code

; Initialize stream processor
stream_processor_init proc uses ebx esi edi, max_streams:dword, max_consumers:dword, stats:dword
    local processor_ptr:dword
    
    ; Validate parameters
    mov eax, max_streams
    test eax, eax
    jnz streams_ok
    mov eax, 256
streams_ok:
    mov max_streams, eax
    
    mov eax, max_consumers
    test eax, eax
    jnz consumers_ok
    mov eax, 1024
consumers_ok:
    mov max_consumers, eax
    
    ; Allocate processor structure
    invoke crt_malloc, sizeof STREAM_PROCESSOR
    test eax, eax
    jz error_oom
    mov processor_ptr, eax
    
    ; Initialize processor structure
    mov ebx, processor_ptr
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    test eax, eax
    jz cleanup_error
    mov [ebx].processor_mutex, eax
    
    ; Initialize fields
    mov [ebx].stream_count, 0
    mov eax, max_streams
    mov [ebx].max_streams, eax
    mov [ebx].consumer_count, 0
    mov eax, max_consumers
    mov [ebx].max_consumers, eax
    mov [ebx].total_messages, 0
    mov [ebx].total_bytes, 0
    mov eax, stats
    mov [ebx].stats_enabled, eax
    
    ; Allocate streams array
    mov eax, max_streams
    imul eax, sizeof STREAM
    invoke crt_malloc, eax
    test eax, eax
    jz cleanup_mutex
    mov [ebx].streams_ptr, eax
    
    ; Initialize streams array to zeros
    mov edi, eax
    mov ecx, max_streams
    imul ecx, sizeof STREAM
    shr ecx, 2
    xor eax, eax
    rep stosd
    
    ; Allocate consumers array
    mov eax, max_consumers
    imul eax, sizeof CONSUMER
    invoke crt_malloc, eax
    test eax, eax
    jz cleanup_streams
    mov [ebx].consumers_ptr, eax
    
    ; Initialize consumers array
    mov edi, eax
    mov ecx, max_consumers
    imul ecx, sizeof CONSUMER
    shr ecx, 2
    xor eax, eax
    rep stosd
    
    ; Set default allocators
    mov [ebx].allocator_ptr, offset crt_malloc
    mov [ebx].deallocator_ptr, offset crt_free
    
    assume ebx:nothing
    mov eax, processor_ptr
    ret
    
cleanup_streams:
    invoke crt_free, [ebx].streams_ptr
cleanup_mutex:
    invoke CloseHandle, [ebx].processor_mutex
cleanup_error:
    invoke crt_free, processor_ptr
error_oom:
    xor eax, eax
    ret
stream_processor_init endp

; Shutdown stream processor
stream_processor_shutdown proc uses ebx esi edi, processor_ptr:dword
    local i:dword, stream_ptr:dword, message_ptr:dword, next_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz done
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Free all streams and messages
    mov i, 0
free_streams_loop:
    mov eax, i
    cmp eax, [ebx].stream_count
    jae free_streams_done
    
    ; Get stream pointer
    mov edx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add edx, eax
    mov stream_ptr, edx
    assume edx:ptr STREAM
    
    ; Free stream name
    invoke crt_free, [edx].name_ptr
    
    ; Free all messages in stream
    mov esi, [edx].first_message_ptr
free_messages_loop:
    test esi, esi
    jz free_messages_done
    assume esi:ptr MESSAGE
    mov next_ptr, [esi].next_ptr
    
    ; Free message data
    invoke crt_free, [esi].data_ptr
    invoke crt_free, esi
    
    mov esi, next_ptr
    jmp free_messages_loop
    
free_messages_done:
    assume esi:nothing
    assume edx:nothing
    inc i
    jmp free_streams_loop
    
free_streams_done:
    ; Free consumers
    mov i, 0
free_consumers_loop:
    mov eax, i
    cmp eax, [ebx].consumer_count
    jae free_consumers_done
    
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    assume edx:ptr CONSUMER
    
    invoke crt_free, [edx].name_ptr
    assume edx:nothing
    inc i
    jmp free_consumers_loop
    
free_consumers_done:
    ; Free arrays
    invoke crt_free, [ebx].streams_ptr
    invoke crt_free, [ebx].consumers_ptr
    
    ; Release mutex and close handle
    invoke ReleaseMutex, [ebx].processor_mutex
    invoke CloseHandle, [ebx].processor_mutex
    
    ; Free processor structure
    invoke crt_free, ebx
    
    assume ebx:nothing
done:
    mov eax, STREAM_SUCCESS
    ret
stream_processor_shutdown endp

; Create new stream
stream_create proc uses ebx esi edi, processor_ptr:dword, name_ptr:dword, name_len:dword, 
              max_messages:dword, retention_ms:dword
    local stream_id:dword, stream_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, name_ptr
    test eax, eax
    jz error_invalid
    mov eax, name_len
    test eax, eax
    jz error_invalid
    cmp eax, STREAM_MAX_NAME_LEN
    ja error_size
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Check if stream limit reached
    mov eax, [ebx].stream_count
    cmp eax, [ebx].max_streams
    jae error_full
    
    ; Check if stream already exists
    mov ecx, [ebx].stream_count
    test ecx, ecx
    jz create_new
    
    mov esi, [ebx].streams_ptr
    mov edi, 0
    
check_existing:
    cmp edi, ecx
    jae create_new
    assume esi:ptr STREAM
    
    ; Check name match
    mov eax, [esi].name_len
    cmp eax, name_len
    jne next_stream
    
    push esi
    mov esi, [esi].name_ptr
    mov edi, name_ptr
    mov ecx, name_len
    repe cmpsb
    pop esi
    jne next_stream
    
    ; Stream already exists
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_EXISTS
    ret
    
next_stream:
    add esi, sizeof STREAM
    inc edi
    jmp check_existing
    
create_new:
    ; Get next stream ID
    mov eax, [ebx].stream_count
    mov stream_id, eax
    
    ; Get stream pointer
    mov edx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add edx, eax
    mov stream_ptr, edx
    assume edx:ptr STREAM
    
    ; Allocate and copy name
    invoke crt_malloc, name_len
    test eax, eax
    jz error_oom_locked
    mov edi, eax
    mov esi, name_ptr
    mov ecx, name_len
    rep movsb
    mov [edx].name_ptr, eax
    mov [edx].name_len, name_len
    
    ; Initialize stream fields
    mov eax, stream_id
    mov [edx].stream_id, eax
    mov [edx].message_count, 0
    mov [edx].first_message_ptr, 0
    mov [edx].last_message_ptr, 0
    mov [edx].consumer_count, 0
    mov eax, max_messages
    mov [edx].max_messages, eax
    mov eax, retention_ms
    mov [edx].retention_ms, eax
    invoke GetTickCount
    mov [edx].created_at, eax
    mov [edx].flags, 0
    
    ; Update processor statistics
    inc [ebx].stream_count
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_oom_locked:
    invoke ReleaseMutex, [ebx].processor_mutex
error_oom:
    mov eax, STREAM_ERROR_OOM
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_size:
    mov eax, STREAM_ERROR_SIZE
    ret
    
error_full:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_FULL
    ret
    
    assume edx:nothing
    assume ebx:nothing
stream_create endp

; Subscribe consumer to stream
stream_subscribe proc uses ebx esi edi, processor_ptr:dword, stream_id:dword, name_ptr:dword, name_len:dword
    local consumer_id:dword, consumer_ptr:dword, stream_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, name_ptr
    test eax, eax
    jz error_invalid
    mov eax, name_len
    test eax, eax
    jz error_invalid
    cmp eax, CONSUMER_MAX_NAME_LEN
    ja error_size
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Check if consumer limit reached
    mov eax, [ebx].consumer_count
    cmp eax, [ebx].max_consumers
    jae error_full
    
    ; Validate stream ID
    mov eax, stream_id
    cmp eax, [ebx].stream_count
    jae error_not_found
    
    ; Get stream pointer
    mov edx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add edx, eax
    mov stream_ptr, edx
    assume edx:ptr STREAM
    
    ; Get next consumer ID
    mov eax, [ebx].consumer_count
    mov consumer_id, eax
    
    ; Get consumer pointer
    mov ecx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add ecx, eax
    mov consumer_ptr, ecx
    assume ecx:ptr CONSUMER
    
    ; Allocate and copy name
    invoke crt_malloc, name_len
    test eax, eax
    jz error_oom_locked
    mov edi, eax
    mov esi, name_ptr
    mov ecx, name_len
    rep movsb
    mov [ecx].name_ptr, eax
    mov [ecx].name_len, name_len
    
    ; Initialize consumer fields
    mov eax, consumer_id
    mov [ecx].consumer_id, eax
    mov eax, stream_id
    mov [ecx].stream_id, eax
    mov [ecx].current_offset, 0
    mov [ecx].last_message_id, 0
    mov [ecx].ack_count, 0
    mov [ecx].nack_count, 0
    invoke GetTickCount
    mov [ecx].created_at, eax
    mov [ecx].flags, 0
    
    ; Update stream consumer count
    inc [edx].consumer_count
    
    ; Update processor statistics
    inc [ebx].consumer_count
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_oom_locked:
    invoke ReleaseMutex, [ebx].processor_mutex
error_oom:
    mov eax, STREAM_ERROR_OOM
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_size:
    mov eax, STREAM_ERROR_SIZE
    ret
    
error_full:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_FULL
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume ecx:nothing
    assume edx:nothing
    assume ebx:nothing
stream_subscribe endp

; Publish message to stream
stream_publish proc uses ebx esi edi, processor_ptr:dword, stream_id:dword, data_ptr:dword, data_len:dword
    local message_ptr:dword, stream_ptr:dword, message_id:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, data_ptr
    test eax, eax
    jz error_invalid
    mov eax, data_len
    test eax, eax
    jz error_invalid
    cmp eax, MESSAGE_MAX_SIZE
    ja error_size
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate stream ID
    mov eax, stream_id
    cmp eax, [ebx].stream_count
    jae error_not_found
    
    ; Get stream pointer
    mov edx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add edx, eax
    mov stream_ptr, edx
    assume edx:ptr STREAM
    
    ; Check if stream is full
    mov eax, [edx].max_messages
    test eax, eax
    jz size_ok
    mov ecx, [edx].message_count
    cmp ecx, eax
    jae error_full
    
size_ok:
    ; Allocate message structure
    invoke crt_malloc, sizeof MESSAGE
    test eax, eax
    jz error_oom_locked
    mov message_ptr, eax
    
    ; Allocate and copy message data
    invoke crt_malloc, data_len
    test eax, eax
    jz free_message_error
    mov edi, eax
    mov esi, data_ptr
    mov ecx, data_len
    rep movsb
    
    mov ebx, processor_ptr
    assume ebx:ptr STREAM_PROCESSOR
    mov edx, stream_ptr
    assume edx:ptr STREAM
    mov esi, message_ptr
    assume esi:ptr MESSAGE
    
    ; Initialize message fields
    mov eax, [ebx].total_messages
    mov message_id, eax
    mov [esi].message_id, eax
    mov eax, stream_id
    mov [esi].stream_id, eax
    invoke GetTickCount
    mov [esi].timestamp, eax
    mov [esi].data_ptr, eax
    mov [esi].data_len, data_len
    mov [esi].next_ptr, 0
    mov [esi].prev_ptr, 0
    mov [esi].ack_count, 0
    mov [esi].nack_count, 0
    mov [esi].flags, 0
    
    ; Add to stream
    mov eax, [edx].last_message_ptr
    test eax, eax
    jz first_message
    
    ; Add to end of list
    mov [eax].next_ptr, esi
    mov [esi].prev_ptr, eax
    mov [edx].last_message_ptr, esi
    jmp add_done
    
first_message:
    ; First message in stream
    mov [edx].first_message_ptr, esi
    mov [edx].last_message_ptr, esi
    
add_done:
    ; Update stream statistics
    inc [edx].message_count
    
    ; Update processor statistics
    inc [ebx].total_messages
    mov eax, data_len
    add [ebx].total_bytes, eax
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
free_message_error:
    invoke crt_free, message_ptr
error_oom_locked:
    invoke ReleaseMutex, [ebx].processor_mutex
error_oom:
    mov eax, STREAM_ERROR_OOM
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_size:
    mov eax, STREAM_ERROR_SIZE
    ret
    
error_full:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_FULL
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume esi:nothing
    assume edx:nothing
    assume ebx:nothing
stream_publish endp

; Consume messages from stream
stream_consume proc uses ebx esi edi, processor_ptr:dword, consumer_id:dword, max_messages:dword, 
               messages_ptr:dword, messages_count_ptr:dword
    local message_count:dword, current_offset:dword, stream_ptr:dword, consumer_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, messages_ptr
    test eax, eax
    jz error_invalid
    mov eax, messages_count_ptr
    test eax, eax
    jz error_invalid
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate consumer ID
    mov eax, consumer_id
    cmp eax, [ebx].consumer_count
    jae error_not_found
    
    ; Get consumer pointer
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    mov consumer_ptr, edx
    assume edx:ptr CONSUMER
    
    ; Get stream pointer
    mov eax, [edx].stream_id
    mov ecx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add ecx, eax
    mov stream_ptr, ecx
    assume ecx:ptr STREAM
    
    ; Get current offset
    mov eax, [edx].current_offset
    mov current_offset, eax
    
    ; Find message at offset
    mov esi, [ecx].first_message_ptr
    test esi, esi
    jz error_empty
    
    mov message_count, 0
    mov edi, messages_ptr
    
consume_loop:
    test esi, esi
    jz consume_done
    cmp message_count, max_messages
    jae consume_done
    
    assume esi:ptr MESSAGE
    
    ; Check if we've reached our offset
    mov eax, [esi].message_id
    cmp eax, current_offset
    jbe next_message
    
    ; Add message to output
    mov [edi], esi
    add edi, 4
    inc message_count
    
next_message:
    mov esi, [esi].next_ptr
    jmp consume_loop
    
consume_done:
    ; Update consumer offset if we found messages
    cmp message_count, 0
    je no_messages
    
    ; Get last message ID
    mov esi, messages_ptr
    mov eax, message_count
    dec eax
    mov esi, [esi + eax * 4]
    assume esi:ptr MESSAGE
    mov eax, [esi].message_id
    mov [edx].current_offset, eax
    assume esi:nothing
    
no_messages:
    ; Return message count
    mov eax, messages_count_ptr
    mov ecx, message_count
    mov [eax], ecx
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_empty:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_EMPTY
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume edx:nothing
    assume ecx:nothing
    assume ebx:nothing
stream_consume endp

; Acknowledge message
stream_ack proc uses ebx esi edi, processor_ptr:dword, consumer_id:dword, message_id:dword
    local consumer_ptr:dword, stream_ptr:dword, message_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate consumer ID
    mov eax, consumer_id
    cmp eax, [ebx].consumer_count
    jae error_not_found
    
    ; Get consumer pointer
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    mov consumer_ptr, edx
    assume edx:ptr CONSUMER
    
    ; Get stream pointer
    mov eax, [edx].stream_id
    mov ecx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add ecx, eax
    mov stream_ptr, ecx
    assume ecx:ptr STREAM
    
    ; Find message
    mov esi, [ecx].first_message_ptr
    test esi, esi
    jz error_not_found
    
find_message:
    test esi, esi
    jz error_not_found
    assume esi:ptr MESSAGE
    
    mov eax, [esi].message_id
    cmp eax, message_id
    je found_message
    
    mov esi, [esi].next_ptr
    jmp find_message
    
found_message:
    ; Update message ack count
    inc [esi].ack_count
    
    ; Update consumer ack count
    inc [edx].ack_count
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume esi:nothing
    assume edx:nothing
    assume ecx:nothing
    assume ebx:nothing
stream_ack endp

; Negative acknowledge message
stream_nack proc uses ebx esi edi, processor_ptr:dword, consumer_id:dword, message_id:dword
    local consumer_ptr:dword, stream_ptr:dword, message_ptr:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate consumer ID
    mov eax, consumer_id
    cmp eax, [ebx].consumer_count
    jae error_not_found
    
    ; Get consumer pointer
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    mov consumer_ptr, edx
    assume edx:ptr CONSUMER
    
    ; Get stream pointer
    mov eax, [edx].stream_id
    mov ecx, [ebx].streams_ptr
    imul eax, sizeof STREAM
    add ecx, eax
    mov stream_ptr, ecx
    assume ecx:ptr STREAM
    
    ; Find message
    mov esi, [ecx].first_message_ptr
    test esi, esi
    jz error_not_found
    
find_message_nack:
    test esi, esi
    jz error_not_found
    assume esi:ptr MESSAGE
    
    mov eax, [esi].message_id
    cmp eax, message_id
    je found_message_nack
    
    mov esi, [esi].next_ptr
    jmp find_message_nack
    
found_message_nack:
    ; Update message nack count
    inc [esi].nack_count
    
    ; Update consumer nack count
    inc [edx].nack_count
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume esi:nothing
    assume edx:nothing
    assume ecx:nothing
    assume ebx:nothing
stream_nack endp

; Get consumer offset
stream_get_offset proc uses ebx esi edi, processor_ptr:dword, consumer_id:dword, offset_ptr:dword
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, offset_ptr
    test eax, eax
    jz error_invalid
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate consumer ID
    mov eax, consumer_id
    cmp eax, [ebx].consumer_count
    jae error_not_found
    
    ; Get consumer pointer
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    assume edx:ptr CONSUMER
    
    ; Return offset
    mov eax, offset_ptr
    mov ecx, [edx].current_offset
    mov [eax], ecx
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
    assume edx:nothing
    assume ebx:nothing
stream_get_offset endp

; Seek to specific offset
stream_seek proc uses ebx esi edi, processor_ptr:dword, consumer_id:dword, offset:dword
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Validate consumer ID
    mov eax, consumer_id
    cmp eax, [ebx].consumer_count
    jae error_not_found
    
    ; Get consumer pointer
    mov edx, [ebx].consumers_ptr
    imul eax, sizeof CONSUMER
    add edx, eax
    assume edx:ptr CONSUMER
    
    ; Validate offset
    mov eax, offset
    cmp eax, [ebx].total_messages
    jae error_offset
    
    ; Set offset
    mov [edx].current_offset, eax
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_NOT_FOUND
    ret
    
error_offset:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_ERROR_OFFSET
    ret
    
    assume edx:nothing
    assume ebx:nothing
stream_seek endp

; Get stream statistics
stream_stats proc uses ebx esi edi, processor_ptr:dword, stats_ptr:dword
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    mov esi, stats_ptr
    test esi, esi
    jz error_invalid
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Copy statistics
    mov eax, [ebx].stream_count
    mov [esi], eax
    mov eax, [ebx].consumer_count
    mov [esi+4], eax
    mov eax, [ebx].total_messages
    mov [esi+8], eax
    mov eax, [ebx].total_bytes
    mov [esi+12], eax
    
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
    assume ebx:nothing
stream_stats endp

; List all streams
stream_list proc uses ebx esi edi, processor_ptr:dword, streams_ptr:dword, count_ptr:dword
    local i:dword
    
    mov ebx, processor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr STREAM_PROCESSOR
    
    ; Validate parameters
    mov eax, streams_ptr
    test eax, eax
    jz error_invalid
    mov eax, count_ptr
    test eax, eax
    jz error_invalid
    
    ; Lock processor
    invoke WaitForSingleObject, [ebx].processor_mutex, INFINITE
    
    ; Copy stream pointers
    mov ecx, [ebx].stream_count
    mov eax, count_ptr
    mov [eax], ecx
    
    test ecx, ecx
    jz list_done
    
    mov esi, [ebx].streams_ptr
    mov edi, streams_ptr
    mov i, 0
    
list_loop:
    mov eax, i
    cmp eax, ecx
    jae list_done
    
    mov [edi], esi
    add edi, 4
    add esi, sizeof STREAM
    inc i
    jmp list_loop
    
list_done:
    invoke ReleaseMutex, [ebx].processor_mutex
    mov eax, STREAM_SUCCESS
    ret
    
error_invalid:
    mov eax, STREAM_ERROR_INVALID
    ret
    
    assume ebx:nothing
stream_list endp

end