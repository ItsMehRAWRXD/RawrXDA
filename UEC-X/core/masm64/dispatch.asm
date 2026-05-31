; UEC-X MASM64 Dispatch Core
; Pure x64 assembly - no C runtime dependencies
; Responsible for: IPC transport, dispatch jump tables, fast syscall wrappers, lock-free queues

; =============================================================================
; UEC-X MASM64 Core - Dispatch Engine
; Architecture: x86-64 (AMD64)
; Calling Convention: Microsoft x64 ABI
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

; =============================================================================
; External Imports (Windows API only)
; =============================================================================
EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_CreateNamedPipeA:QWORD
EXTERNDEF __imp_ConnectNamedPipe:QWORD
EXTERNDEF __imp_ReadFile:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
EXTERNDEF __imp_CreateMutexA:QWORD
EXTERNDEF __imp_ReleaseMutex:QWORD
EXTERNDEF __imp_WaitForSingleObject:QWORD
EXTERNDEF __imp_VirtualAlloc:QWORD
EXTERNDEF __imp_VirtualFree:QWORD
EXTERNDEF __imp_GetLastError:QWORD
EXTERNDEF __imp_SetEvent:QWORD
EXTERNDEF __imp_CreateEventA:QWORD
EXTERNDEF __imp_ResetEvent:QWORD

; =============================================================================
; Constants
; =============================================================================
UEC_DISPATCH_TABLE_SIZE EQU 4096
UEC_RING_BUFFER_SIZE    EQU 65536
UEC_MAX_EXTENSIONS      EQU 256
UEC_CACHE_LINE_SIZE     EQU 64
UEC_PAGE_SIZE          EQU 4096

; IPC Transport Types
UEC_IPC_NAMED_PIPE     EQU 0
UEC_IPC_SHARED_MEMORY  EQU 1
UEC_IPC_SOCKET         EQU 2
UEC_IPC_LOOPBACK       EQU 3

; Dispatch Command Types
UEC_CMD_REGISTER        EQU 1
UEC_CMD_UNREGISTER      EQU 2
UEC_CMD_EXECUTE         EQU 3
UEC_CMD_EMIT_EVENT      EQU 4
UEC_CMD_QUERY_STATE     EQU 5
UEC_CMD_HOTPATCH        EQU 6

; Error Codes
UEC_SUCCESS             EQU 0
UEC_ERROR_INVALID_PARAM EQU 1
UEC_ERROR_NO_MEMORY     EQU 2
UEC_ERROR_NOT_FOUND     EQU 3
UEC_ERROR_BUSY          EQU 4
UEC_ERROR_PERMISSION    EQU 5

; =============================================================================
; Data Section
; =============================================================================
.data
ALIGN 64

; Global dispatch table - 4096 entries, each 8 bytes (function pointer)
g_dispatch_table LABEL QWORD
    REPEAT UEC_DISPATCH_TABLE_SIZE
        DQ 0
    ENDM

; Ring buffer structure for lock-free IPC
g_ring_buffer LABEL BYTE
    ; Head index (producer)
    g_ring_head     DQ 0
    ; Tail index (consumer)  
    g_ring_tail     DQ 0
    ; Buffer data
    g_ring_data     DB UEC_RING_BUFFER_SIZE DUP(0)

; Extension slot registry
g_extension_slots LABEL BYTE
    g_slot_count    DD 0
    g_slot_reserved DD 0
    ; Slot metadata: 32 bytes per slot (status, id, ptr, reserved)
    g_slot_data     DB UEC_MAX_EXTENSIONS * 32 DUP(0)

; IPC transport handles
g_ipc_transport LABEL BYTE
    g_ipc_type      DD 0
    g_ipc_handle    DQ 0
    g_ipc_buffer    DQ 0
    g_ipc_size      DQ 0

; Statistics counters (cache-line aligned)
ALIGN 64
g_stats_dispatch_count      DQ 0
g_stats_dispatch_errors     DQ 0
g_stats_ipc_bytes_sent      DQ 0
g_stats_ipc_bytes_recv      DQ 0

; =============================================================================
; Code Section
; =============================================================================
.code
ALIGN 16

; =============================================================================
; uec_dispatch_init - Initialize dispatch core
; Parameters: rcx = config flags
; Returns: rax = error code (0 = success)
; =============================================================================
uec_dispatch_init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    ; Clear dispatch table
    xor     eax, eax
    mov     rdi, OFFSET g_dispatch_table
    mov     rcx, UEC_DISPATCH_TABLE_SIZE
    rep     stosq

    ; Initialize ring buffer
    mov     QWORD PTR [g_ring_head], 0
    mov     QWORD PTR [g_ring_tail], 0

    ; Clear extension slots
    mov     rdi, OFFSET g_extension_slots
    mov     rcx, (UEC_MAX_EXTENSIONS * 32) / 8
    rep     stosq

    ; Initialize statistics
    mov     QWORD PTR [g_stats_dispatch_count], 0
    mov     QWORD PTR [g_stats_dispatch_errors], 0

    mov     rax, UEC_SUCCESS
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_dispatch_init ENDP

; =============================================================================
; uec_dispatch_register - Register a command handler
; Parameters: 
;   rcx = command id (1-4095)
;   rdx = handler function pointer
;   r8  = user data
; Returns: rax = error code
; =============================================================================
uec_dispatch_register PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Validate command id
    test    rcx, rcx
    jz      invalid_param
    cmp     rcx, UEC_DISPATCH_TABLE_SIZE
    jae     invalid_param

    ; Calculate table offset
    mov     rbx, rcx
    shl     rbx, 3              ; Multiply by 8 (QWORD size)
    lea     rax, [g_dispatch_table + rbx]

    ; Store handler (atomic write)
    mov     [rax], rdx

    mov     rax, UEC_SUCCESS
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rbx
    ret
uec_dispatch_register ENDP

; =============================================================================
; uec_dispatch_execute - Execute a registered command by ID
; Parameters:
;   rcx = command id
;   rdx = parameter buffer
;   r8  = parameter length
; Returns: rax = result code
; =============================================================================
uec_dispatch_execute PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    ; Validate command id
    test    rcx, rcx
    jz      not_found
    cmp     rcx, UEC_DISPATCH_TABLE_SIZE
    jae     not_found

    ; Load handler from table
    mov     rbx, rcx
    shl     rbx, 3
    mov     rax, [g_dispatch_table + rbx]
    test    rax, rax
    jz      not_found

    ; Increment dispatch counter
    lock inc QWORD PTR [g_stats_dispatch_count]

    ; Call handler (Microsoft x64 ABI)
    ; rcx = command id (already set)
    ; rdx = param buffer (already set)
    ; r8 = param length (already set)
    ; r9 = reserved for future use
    xor     r9, r9
    sub     rsp, 32             ; Shadow space
    call    rax
    add     rsp, 32

    pop     rsi
    pop     rdi
    pop     rbx
    ret

not_found:
    lock inc QWORD PTR [g_stats_dispatch_errors]
    mov     rax, UEC_ERROR_NOT_FOUND
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_dispatch_execute ENDP

; =============================================================================
; uec_ring_enqueue - Lock-free enqueue to ring buffer
; Parameters:
;   rcx = data pointer
;   rdx = data length
; Returns: rax = error code
; =============================================================================
uec_ring_enqueue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog

    ; Validate parameters
    test    rcx, rcx
    jz      invalid_param
    test    rdx, rdx
    jz      invalid_param
    cmp     rdx, UEC_RING_BUFFER_SIZE
    ja      invalid_param

    ; Load current head
    mov     rax, [g_ring_head]

enqueue_retry:
    ; Calculate available space
    mov     rbx, [g_ring_tail]
    sub     rbx, rax
    add     rbx, UEC_RING_BUFFER_SIZE
    and     rbx, (UEC_RING_BUFFER_SIZE - 1)
    sub     rbx, UEC_RING_BUFFER_SIZE
    neg     rbx                 ; rbx = available space
    sub     rbx, 1              ; Leave 1 byte gap

    ; Check if enough space
    cmp     rbx, rdx
    jb      busy

    ; Calculate write position
    mov     rdi, rax
    and     rdi, (UEC_RING_BUFFER_SIZE - 1)
    lea     rdi, [g_ring_data + rdi]

    ; Copy data
    mov     rsi, rcx
    mov     rcx, rdx
    rep     movsb

    ; Update head (atomic)
    mov     rbx, [g_ring_head]
    add     rbx, rdx
    and     rbx, (UEC_RING_BUFFER_SIZE - 1)
    lock cmpxchg [g_ring_head], rbx
    jne     enqueue_retry

    mov     rax, UEC_SUCCESS
    pop     rdi
    pop     rbx
    ret

busy:
    mov     rax, UEC_ERROR_BUSY
    pop     rdi
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rdi
    pop     rbx
    ret
uec_ring_enqueue ENDP

; =============================================================================
; uec_ring_dequeue - Lock-free dequeue from ring buffer
; Parameters:
;   rcx = output buffer
;   rdx = max length
;   r8  = actual length (out)
; Returns: rax = error code
; =============================================================================
uec_ring_dequeue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    ; Validate parameters
    test    rcx, rcx
    jz      invalid_param
    test    rdx, rdx
    jz      invalid_param
    test    r8, r8
    jz      invalid_param

    ; Load current tail
    mov     rax, [g_ring_tail]

dequeue_retry:
    ; Check if data available
    cmp     rax, [g_ring_head]
    je      empty

    ; Calculate read position
    mov     rsi, rax
    and     rsi, (UEC_RING_BUFFER_SIZE - 1)
    lea     rsi, [g_ring_data + rsi]

    ; Read message header (first 4 bytes = length)
    mov     ebx, DWORD PTR [rsi]
    test    ebx, ebx
    jz      empty

    ; Check if enough output space
    cmp     rbx, rdx
    ja      buffer_too_small

    ; Copy data
    mov     rdi, rcx
    mov     rcx, rbx
    rep     movsb

    ; Update tail (atomic)
    mov     rbx, [g_ring_tail]
    add     rbx, rax
    lock cmpxchg [g_ring_tail], rbx
    jne     dequeue_retry

    ; Return actual length
    mov     [r8], eax
    mov     rax, UEC_SUCCESS
    pop     rsi
    pop     rdi
    pop     rbx
    ret

empty:
    mov     rax, UEC_ERROR_NOT_FOUND
    pop     rsi
    pop     rdi
    pop     rbx
    ret

buffer_too_small:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rsi
    pop     rdi
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_ring_dequeue ENDP

; =============================================================================
; uec_ipc_init_named_pipe - Initialize named pipe transport
; Parameters:
;   rcx = pipe name (ASCII)
;   rdx = buffer size
; Returns: rax = error code
; =============================================================================
uec_ipc_init_named_pipe PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    .endprolog

    ; Store parameters
    mov     rbx, rcx            ; pipe name
    mov     r12, rdx            ; buffer size

    ; Create named pipe
    ; Parameters for CreateNamedPipeA:
    ; rcx = lpName
    ; rdx = dwOpenMode (PIPE_ACCESS_DUPLEX)
    ; r8  = dwPipeMode (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE)
    ; r9  = nMaxInstances (1)
    ; [rsp+32] = nOutBufferSize
    ; [rsp+40] = nInBufferSize
    ; [rsp+48] = nDefaultTimeOut
    ; [rsp+56] = lpSecurityAttributes

    mov     rcx, rbx
    mov     edx, 00000003h      ; PIPE_ACCESS_DUPLEX
    mov     r8d, 00000006h      ; PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE
    mov     r9d, 1              ; nMaxInstances

    sub     rsp, 64             ; Shadow space + parameters
    mov     QWORD PTR [rsp+32], r12
    mov     QWORD PTR [rsp+40], r12
    mov     DWORD PTR [rsp+48], 0
    mov     QWORD PTR [rsp+56], 0

    mov     rax, __imp_CreateNamedPipeA
    call    QWORD PTR [rax]
    add     rsp, 64

    cmp     rax, INVALID_HANDLE_VALUE
    je      ipc_error

    ; Store handle
    mov     [g_ipc_handle], rax
    mov     DWORD PTR [g_ipc_type], UEC_IPC_NAMED_PIPE
    mov     [g_ipc_size], r12

    mov     rax, UEC_SUCCESS
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret

ipc_error:
    mov     rax, UEC_ERROR_BUSY
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_ipc_init_named_pipe ENDP

; =============================================================================
; uec_ipc_send - Send data over IPC transport
; Parameters:
;   rcx = data pointer
;   rdx = data length
; Returns: rax = error code
; =============================================================================
uec_ipc_send PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    ; Validate parameters
    test    rcx, rcx
    jz      invalid_param
    test    rdx, rdx
    jz      invalid_param

    ; Check transport type
    cmp     DWORD PTR [g_ipc_type], UEC_IPC_NAMED_PIPE
    jne     invalid_param

    ; Get handle
    mov     rbx, [g_ipc_handle]
    test    rbx, rbx
    jz      invalid_param

    ; Write to pipe
    sub     rsp, 48             ; Shadow space + locals
    mov     rdi, rsp            ; lpNumberOfBytesWritten
    mov     QWORD PTR [rdi], 0

    mov     r8, rdx             ; nNumberOfBytesToWrite
    mov     rdx, rcx            ; lpBuffer
    mov     rcx, rbx            ; hFile
    mov     r9, rdi             ; lpNumberOfBytesWritten
    mov     QWORD PTR [rsp+32], 0  ; lpOverlapped

    mov     rax, __imp_WriteFile
    call    QWORD PTR [rax]
    add     rsp, 48

    test    rax, rax
    jz      ipc_error

    ; Update stats
    mov     rax, [rsp-48]       ; bytes written
    lock add [g_stats_ipc_bytes_sent], rax

    mov     rax, UEC_SUCCESS
    pop     rsi
    pop     rdi
    pop     rbx
    ret

ipc_error:
    mov     rax, UEC_ERROR_BUSY
    pop     rsi
    pop     rdi
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_ipc_send ENDP

; =============================================================================
; uec_ipc_recv - Receive data from IPC transport
; Parameters:
;   rcx = buffer pointer
;   rdx = buffer size
;   r8  = bytes received (out)
; Returns: rax = error code
; =============================================================================
uec_ipc_recv PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    .endprolog

    ; Validate parameters
    test    rcx, rcx
    jz      invalid_param
    test    rdx, rdx
    jz      invalid_param
    test    r8, r8
    jz      invalid_param

    ; Check transport type
    cmp     DWORD PTR [g_ipc_type], UEC_IPC_NAMED_PIPE
    jne     invalid_param

    ; Get handle
    mov     rbx, [g_ipc_handle]
    test    rbx, rbx
    jz      invalid_param

    ; Read from pipe
    sub     rsp, 48
    mov     r12, r8             ; Save output pointer
    mov     rdi, rsp            ; lpNumberOfBytesRead

    mov     r8, rdx             ; nNumberOfBytesToRead
    mov     rdx, rcx            ; lpBuffer
    mov     rcx, rbx            ; hFile
    mov     r9, rdi             ; lpNumberOfBytesRead
    mov     QWORD PTR [rsp+32], 0  ; lpOverlapped

    mov     rax, __imp_ReadFile
    call    QWORD PTR [rax]
    add     rsp, 48

    test    rax, rax
    jz      ipc_error

    ; Return bytes read
    mov     rax, [rsp-48]       ; bytes read
    mov     [r12], rax
    lock add [g_stats_ipc_bytes_recv], rax

    mov     rax, UEC_SUCCESS
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret

ipc_error:
    mov     rax, UEC_ERROR_NOT_FOUND
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret
uec_ipc_recv ENDP

; =============================================================================
; uec_extension_alloc_slot - Allocate an extension slot
; Parameters: rcx = extension id (out)
; Returns: rax = error code
; =============================================================================
uec_extension_alloc_slot PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    .endprolog

    test    rcx, rcx
    jz      invalid_param

    ; Search for free slot
    xor     ebx, ebx
    mov     rdi, OFFSET g_slot_data

slot_loop:
    cmp     ebx, UEC_MAX_EXTENSIONS
    jae     no_slots

    ; Check if slot is free (first DWORD = status)
    mov     eax, DWORD PTR [rdi]
    test    eax, eax
    jz      found_slot

    add     rdi, 32
    inc     ebx
    jmp     slot_loop

found_slot:
    ; Mark slot as allocated
    mov     DWORD PTR [rdi], 1
    inc     DWORD PTR [g_slot_count]

    ; Return slot index
    mov     [rcx], ebx
    mov     rax, UEC_SUCCESS
    pop     rdi
    pop     rbx
    ret

no_slots:
    mov     rax, UEC_ERROR_BUSY
    pop     rdi
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rdi
    pop     rbx
    ret
uec_extension_alloc_slot ENDP

; =============================================================================
; uec_extension_free_slot - Free an extension slot
; Parameters: rcx = slot index
; Returns: rax = error code
; =============================================================================
uec_extension_free_slot PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Validate slot index
    cmp     ecx, UEC_MAX_EXTENSIONS
    jae     invalid_param

    ; Calculate slot address
    mov     rbx, rcx
    shl     rbx, 5              ; Multiply by 32
    lea     rax, [g_slot_data + rbx]

    ; Mark slot as free
    mov     DWORD PTR [rax], 0
    dec     DWORD PTR [g_slot_count]

    mov     rax, UEC_SUCCESS
    pop     rbx
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rbx
    ret
uec_extension_free_slot ENDP

; =============================================================================
; uec_fast_syscall - Fast system call wrapper
; Parameters: rcx = syscall number, rdx-r9 = args
; Returns: rax = result
; =============================================================================
uec_fast_syscall PROC FRAME
    ; Direct Windows syscall invocation
    ; rcx = syscall number
    ; rdx, r8, r9 = args 1-3
    ; stack = args 4-6

    push    rbx
    .pushreg rbx
    .endprolog

    ; Move syscall number to eax
    mov     eax, ecx

    ; Arguments already in correct registers for Windows x64
    ; rcx = arg1 (was rdx)
    ; rdx = arg2 (was r8)
    ; r8  = arg3 (was r9)
    ; r9  = arg4 (from stack)

    mov     rcx, rdx
    mov     rdx, r8
    mov     r8, r9
    mov     r9, QWORD PTR [rsp+40]

    ; Execute syscall
    syscall

    pop     rbx
    ret
uec_fast_syscall ENDP

; =============================================================================
; uec_get_stats - Get dispatch statistics
; Parameters: rcx = stats structure pointer
; Returns: rax = error code
; =============================================================================
uec_get_stats PROC FRAME
    push    rdi
    .pushreg rdi
    .endprolog

    test    rcx, rcx
    jz      invalid_param

    mov     rdi, rcx

    ; Copy statistics
    mov     rax, [g_stats_dispatch_count]
    mov     [rdi], rax
    mov     rax, [g_stats_dispatch_errors]
    mov     [rdi+8], rax
    mov     rax, [g_stats_ipc_bytes_sent]
    mov     [rdi+16], rax
    mov     rax, [g_stats_ipc_bytes_recv]
    mov     [rdi+24], rax

    mov     rax, UEC_SUCCESS
    pop     rdi
    ret

invalid_param:
    mov     rax, UEC_ERROR_INVALID_PARAM
    pop     rdi
    ret
uec_get_stats ENDP

; =============================================================================
; Export Table
; =============================================================================
PUBLIC uec_dispatch_init
PUBLIC uec_dispatch_register
PUBLIC uec_dispatch_execute
PUBLIC uec_ring_enqueue
PUBLIC uec_ring_dequeue
PUBLIC uec_ipc_init_named_pipe
PUBLIC uec_ipc_send
PUBLIC uec_ipc_recv
PUBLIC uec_extension_alloc_slot
PUBLIC uec_extension_free_slot
PUBLIC uec_fast_syscall
PUBLIC uec_get_stats

END
