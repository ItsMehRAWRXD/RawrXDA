;================================================================================
; RawrXD_IPC_Bridge.asm - Shared Memory Bridge for CLI/GUI Integration
; Allows CLI and GUI to share the same Vulkan Compute Queue
;================================================================================

option casemap:none
option frame:auto

include \masm64\include64\masm64rt.inc

;================================================================================
; CONSTANTS - IPC Configuration
;================================================================================
IPC_SHARED_MEM_SIZE     equ 16777216  ; 16MB shared memory
IPC_MAX_MESSAGES        equ 1024      ; Ring buffer size
IPC_MAX_MESSAGE_SIZE    equ 4096      ; Max message size

; Message types
MSG_NONE                equ 0
MSG_INFERENCE_REQUEST   equ 1
MSG_INFERENCE_RESPONSE  equ 2
MSG_TOKEN_STREAM        equ 3
MSG_FILE_OPEN           equ 4
MSG_FILE_EDIT           equ 5
MSG_CURSOR_MOVE         equ 6
MSG_COMPLETION_REQUEST  equ 7
MSG_COMPLETION_RESPONSE equ 8
MSG_SHUTDOWN            equ 255

; Sync primitives
IPC_MUTEX_NAME          db "RawrXD_IPC_Mutex_v1", 0
IPC_EVENT_GUI           db "RawrXD_GUI_Event_v1", 0
IPC_EVENT_CLI           db "RawrXD_CLI_Event_v1", 0
IPC_SHARED_MEM_NAME     db "RawrXD_SharedMemory_v1", 0

;================================================================================
; STRUCTURES
;================================================================================
IPC_MESSAGE struct
    msg_type        dd ?
    msg_id          dd ?
    timestamp       dq ?
    payload_size    dd ?
    sender_pid      dd ?
    payload         db IPC_MAX_MESSAGE_SIZE dup(?)
IPC_MESSAGE ends

IPC_RING_BUFFER struct
    head            dd ?        ; Write position
    tail            dd ?        ; Read position
    count           dd ?        ; Current message count
    capacity        dd ?        ; Max messages
    messages        IPC_MESSAGE IPC_MAX_MESSAGES dup(<>)
IPC_RING_BUFFER ends

IPC_SHARED_STATE struct
    version         dd ?        ; Protocol version
    flags           dd ?        ; Status flags
    gui_active      db ?        ; GUI is running
    cli_active      db ?        ; CLI is running
    gpu_busy        db ?        ; GPU compute in progress
    _padding        db ?
    
    ; Statistics
    messages_sent   dq ?
    messages_recv   dq ?
    bytes_transferred dq ?
    
    ; Ring buffers (double buffered)
    gui_to_cli      IPC_RING_BUFFER <>
    cli_to_gui      IPC_RING_BUFFER <>
    
    ; Shared GPU resources
    vulkan_device   dq ?        ; Handle to shared device
    compute_queue   dq ?        ; Shared compute queue
    model_loaded    db ?        ; Model status
    model_path      db 260 dup(?)
    
    ; Token streaming buffer (circular)
    token_buffer    dq ?        ; Pointer to token ring
    token_head      dd ?
    token_tail      dd ?
    token_count     dd ?
IPC_SHARED_STATE ends

IPC_CONTEXT struct
    h_map_file      dq ?        ; File mapping handle
    h_mutex         dq ?        ; Cross-process mutex
    h_event_gui     dq ?        ; GUI notification event
    h_event_cli     dq ?        ; CLI notification event
    
    mapped_view     dq ?        ; Pointer to shared memory
    shared_state    dq ?        ; Pointer to IPC_SHARED_STATE
    
    is_gui          db ?        ; TRUE if GUI process
    is_initialized  db ?
    _padding        db 6 dup(?)
    
    ; Local buffers
    recv_buffer     db IPC_MAX_MESSAGE_SIZE dup(?)
    send_buffer     db IPC_MAX_MESSAGE_SIZE dup(?)
IPC_CONTEXT ends

;================================================================================
; DATA SECTION
;================================================================================
.data

align 8
g_ipc_context       IPC_CONTEXT <>
g_ipc_initialized   db 0

;================================================================================
; CODE SECTION
;================================================================================
.code

PUBLIC IPC_Initialize
PUBLIC IPC_Shutdown
PUBLIC IPC_SendMessage
PUBLIC IPC_RecvMessage
PUBLIC IPC_PeekMessage
PUBLIC IPC_WaitForMessage
PUBLIC IPC_AttachGPU
PUBLIC IPC_DetachGPU
PUBLIC IPC_ShareVulkanContext

;================================================================================
; INITIALIZATION - Create/join shared memory
;================================================================================
IPC_Initialize PROC FRAME
    ; ecx = is_gui (0=CLI, 1=GUI)
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 48
    .allocstack 48
    .ENDPROLOG
    
    mov r12b, cl            ; Save is_gui
    
    ; Check if already initialized
    cmp g_ipc_initialized, 1
    je ipc_already_init
    
    ; Allocate context
    xor ecx, ecx                       ; lpAddress = NULL
    mov edx, sizeof IPC_CONTEXT        ; dwSize
    mov r8d, MEM_COMMIT or MEM_RESERVE  ; flAllocationType
    mov r9d, PAGE_READWRITE             ; flProtect
    call VirtualAlloc
    test rax, rax
    jz ipc_fail
    mov rbx, rax
    mov g_ipc_context, rax
    
    mov [rbx].IPC_CONTEXT.is_gui, r12b
    
    ; Create or open mutex
    xor ecx, ecx            ; Security attributes
    mov edx, 1              ; Initial owner: TRUE
    lea r8, IPC_MUTEX_NAME
    call CreateMutexA
    test rax, rax
    jz ipc_cleanup
    mov [rbx].IPC_CONTEXT.h_mutex, rax
    
    ; Create events
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d            ; bInitialState = FALSE (full dword clear)
    lea r9, IPC_EVENT_GUI
    call CreateEventA
    mov [rbx].IPC_CONTEXT.h_event_gui, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d            ; bInitialState = FALSE (full dword clear)
    lea r9, IPC_EVENT_CLI
    call CreateEventA
    mov [rbx].IPC_CONTEXT.h_event_cli, rax
    
    ; Create or open file mapping (x64 calling convention)
    ; CreateFileMappingA(hFile, lpAttributes, flProtect, dwMaxHigh, dwMaxLow, lpName)
    mov  rcx, INVALID_HANDLE_VALUE  ; hFile — page file
    xor  edx, edx                   ; lpFileMappingAttributes = NULL
    mov  r8d, PAGE_READWRITE        ; flProtect
    xor  r9d, r9d                   ; dwMaximumSizeHigh = 0
    mov  dword ptr [rsp+20h], IPC_SHARED_MEM_SIZE  ; dwMaximumSizeLow
    lea  rax, IPC_SHARED_MEM_NAME
    mov  qword ptr [rsp+28h], rax   ; lpName
    call CreateFileMappingA
    
    test rax, rax
    jz ipc_cleanup
    mov [rbx].IPC_CONTEXT.h_map_file, rax
    
    ; Map view (x64 calling convention)
    ; MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap)
    mov  rcx, rax                    ; hFileMappingObject
    mov  edx, FILE_MAP_ALL_ACCESS    ; dwDesiredAccess
    xor  r8d, r8d                    ; dwFileOffsetHigh = 0
    xor  r9d, r9d                    ; dwFileOffsetLow = 0
    mov  qword ptr [rsp+20h], 0      ; dwNumberOfBytesToMap = 0 (map all)
    call MapViewOfFile
    
    test rax, rax
    jz ipc_cleanup_map
    mov [rbx].IPC_CONTEXT.mapped_view, rax
    mov [rbx].IPC_CONTEXT.shared_state, rax
    mov r13, rax            ; Shared state pointer
    
    ; Check if we're first (GUI creates, CLI attaches)
    cmp r12b, 1
    jne cli_attach
    
    ; GUI: Initialize shared state
    mov [r13].IPC_SHARED_STATE.version, 1
    mov [r13].IPC_SHARED_STATE.flags, 0
    mov [r13].IPC_SHARED_STATE.gui_active, 1
    mov [r13].IPC_SHARED_STATE.cli_active, 0
    mov [r13].IPC_SHARED_STATE.gpu_busy, 0
    
    ; Initialize ring buffers
    mov [r13].IPC_SHARED_STATE.gui_to_cli.head, 0
    mov [r13].IPC_SHARED_STATE.gui_to_cli.tail, 0
    mov [r13].IPC_SHARED_STATE.gui_to_cli.count, 0
    mov [r13].IPC_SHARED_STATE.gui_to_cli.capacity, IPC_MAX_MESSAGES
    
    mov [r13].IPC_SHARED_STATE.cli_to_gui.head, 0
    mov [r13].IPC_SHARED_STATE.cli_to_gui.tail, 0
    mov [r13].IPC_SHARED_STATE.cli_to_gui.count, 0
    mov [r13].IPC_SHARED_STATE.cli_to_gui.capacity, IPC_MAX_MESSAGES
    
    jmp ipc_init_done
    
cli_attach:
    ; CLI: Signal that we're active
    mov [r13].IPC_SHARED_STATE.cli_active, 1
    
    ; Wait for GUI to be ready
    mov ecx, 5000           ; 5 second timeout
    call Sleep
    
ipc_init_done:
    mov [rbx].IPC_CONTEXT.is_initialized, 1
    mov g_ipc_initialized, 1
    
    mov eax, 1
    jmp ipc_done
    
ipc_already_init:
    mov eax, 1
    jmp ipc_done
    
ipc_cleanup_map:
    mov rcx, [rbx].IPC_CONTEXT.h_map_file
    call CloseHandle
    
ipc_cleanup:
    ; Cleanup partial initialization
    mov rcx, rbx
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov g_ipc_context, 0
    
ipc_fail:
    xor eax, eax
    
ipc_done:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
IPC_Initialize ENDP

;================================================================================
; MESSAGE SENDING - Thread-safe ring buffer
;================================================================================
IPC_SendMessage PROC FRAME
    ; ecx = msg_type
    ; rdx = payload pointer
    ; r8d = payload_size
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .ENDPROLOG
    
    mov r12d, ecx           ; msg_type
    mov r13, rdx            ; payload
    mov r14d, r8d           ; payload_size
    
    ; ---- Clamp payload_size to IPC_MAX_MESSAGE_SIZE to prevent buffer overflow ----
    cmp  r14d, IPC_MAX_MESSAGE_SIZE
    jbe  @ipc_size_ok
    mov  r14d, IPC_MAX_MESSAGE_SIZE
@ipc_size_ok:
    
    mov rbx, g_ipc_context
    
    ; Acquire mutex
    mov rcx, [rbx].IPC_CONTEXT.h_mutex
    mov edx, INFINITE
    call WaitForSingleObject
    
    ; Get appropriate ring buffer
    cmp [rbx].IPC_CONTEXT.is_gui, 1
    je gui_sending
    
    ; CLI sending to GUI: use cli_to_gui buffer
    mov r15, [rbx].IPC_CONTEXT.shared_state
    add r15, OFFSET IPC_SHARED_STATE.cli_to_gui
    jmp check_space
    
gui_sending:
    ; GUI sending to CLI: use gui_to_cli buffer
    mov r15, [rbx].IPC_CONTEXT.shared_state
    add r15, OFFSET IPC_SHARED_STATE.gui_to_cli
    
check_space:
    ; Check if buffer full
    mov eax, [r15].IPC_RING_BUFFER.count
    cmp eax, IPC_MAX_MESSAGES
    jae buffer_full
    
    ; Calculate write position
    mov ecx, [r15].IPC_RING_BUFFER.head
    imul ecx, sizeof IPC_MESSAGE
    lea rax, [r15].IPC_RING_BUFFER.messages
    add rax, rcx
    
    ; Fill message
    mov [rax].IPC_MESSAGE.msg_type, r12d
    mov r12, rax            ; Save message pointer in non-volatile r12 (msg_type already written)
    mov [r12].IPC_MESSAGE.payload_size, r14d
    
    call GetCurrentProcessId
    mov [r12].IPC_MESSAGE.sender_pid, eax
    
    call GetTickCount64
    mov [r12].IPC_MESSAGE.timestamp, rax
    
    ; Copy payload with bounds-checked length
    lea rcx, [r12].IPC_MESSAGE.payload
    mov rdx, r13
    ; Clamp copy size to prevent overflow
    mov r8d, r14d
    cmp r8d, IPC_MAX_MESSAGE_SIZE
    jbe @F
    mov r8d, IPC_MAX_MESSAGE_SIZE
@@:
    call memcpy
    
    ; Update head and count
    inc [r15].IPC_RING_BUFFER.head
    mov eax, [r15].IPC_RING_BUFFER.head
    cmp eax, IPC_MAX_MESSAGES
    jb @F
    mov [r15].IPC_RING_BUFFER.head, 0
    
@@: inc [r15].IPC_RING_BUFFER.count
    
    ; Signal other process
    cmp [rbx].IPC_CONTEXT.is_gui, 1
    je signal_cli
    mov rcx, [rbx].IPC_CONTEXT.h_event_gui
    jmp do_signal
    
signal_cli:
    mov rcx, [rbx].IPC_CONTEXT.h_event_cli
    
do_signal:
    call SetEvent
    
    mov eax, 1
    jmp send_done
    
buffer_full:
    xor eax, eax
    
send_done:
    ; Release mutex
    mov rcx, [rbx].IPC_CONTEXT.h_mutex
    call ReleaseMutex
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
IPC_SendMessage ENDP

;================================================================================
; MESSAGE PEEKING - Non-blocking, does NOT consume the message
;================================================================================
IPC_PeekMessage PROC FRAME
    ; rcx = output buffer (IPC_MESSAGE pointer)
    ; Returns: eax = 1 if message available (copied but NOT consumed), 0 if empty
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .ENDPROLOG
    
    mov  r12, rcx            ; Output buffer
    mov  rbx, g_ipc_context
    
    ; Acquire mutex
    mov  rcx, [rbx].IPC_CONTEXT.h_mutex
    mov  edx, INFINITE
    call WaitForSingleObject
    
    ; Get appropriate ring buffer (opposite of send — same as recv)
    cmp  [rbx].IPC_CONTEXT.is_gui, 1
    je   peek_gui_receiving
    
    ; CLI peeking from GUI: read from gui_to_cli
    mov  r13, [rbx].IPC_CONTEXT.shared_state
    add  r13, OFFSET IPC_SHARED_STATE.gui_to_cli
    jmp  peek_check
    
peek_gui_receiving:
    ; GUI peeking from CLI: read from cli_to_gui
    mov  r13, [rbx].IPC_CONTEXT.shared_state
    add  r13, OFFSET IPC_SHARED_STATE.cli_to_gui
    
peek_check:
    ; Check if any messages
    cmp  [r13].IPC_RING_BUFFER.count, 0
    je   peek_empty
    
    ; Calculate read position (tail)
    mov  ecx, [r13].IPC_RING_BUFFER.tail
    imul ecx, sizeof IPC_MESSAGE
    lea  rax, [r13].IPC_RING_BUFFER.messages
    add  rax, rcx
    
    ; Copy message to output — but do NOT advance tail or decrement count
    mov  rcx, r12
    mov  rdx, rax
    mov  r8d, sizeof IPC_MESSAGE
    call memcpy
    
    mov  eax, 1
    jmp  peek_done
    
peek_empty:
    xor  eax, eax
    
peek_done:
    ; Release mutex
    mov  rcx, [rbx].IPC_CONTEXT.h_mutex
    call ReleaseMutex
    
    pop  r13
    pop  r12
    pop  rbx
    ret
IPC_PeekMessage ENDP

;================================================================================
; MESSAGE RECEIVING - Non-blocking
;================================================================================
IPC_RecvMessage PROC FRAME
    ; rcx = output buffer (IPC_MESSAGE pointer)
    ; Returns: eax = 1 if message received, 0 if empty
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .ENDPROLOG
    
    mov r12, rcx            ; Output buffer
    mov rbx, g_ipc_context
    test rbx, rbx
    jz recv_null_ctx        ; NULL check - bypass mutex release
    
    ; Acquire mutex
    mov rcx, [rbx].IPC_CONTEXT.h_mutex
    mov edx, INFINITE
    call WaitForSingleObject
    
    ; Get appropriate ring buffer (opposite of send)
    cmp [rbx].IPC_CONTEXT.is_gui, 1
    je gui_receiving
    
    ; CLI receiving from GUI: read from gui_to_cli
    mov r13, [rbx].IPC_CONTEXT.shared_state
    add r13, OFFSET IPC_SHARED_STATE.gui_to_cli
    jmp check_messages
    
gui_receiving:
    ; GUI receiving from CLI: read from cli_to_gui
    mov r13, [rbx].IPC_CONTEXT.shared_state
    add r13, OFFSET IPC_SHARED_STATE.cli_to_gui
    
check_messages:
    ; Check if any messages
    cmp [r13].IPC_RING_BUFFER.count, 0
    je no_messages
    
    ; Calculate read position
    mov ecx, [r13].IPC_RING_BUFFER.tail
    imul ecx, sizeof IPC_MESSAGE
    lea rax, [r13].IPC_RING_BUFFER.messages
    add rax, rcx
    
    ; Copy message to output
    mov rcx, r12
    mov rdx, rax
    mov r8d, sizeof IPC_MESSAGE
    call memcpy
    
    ; Update tail and count
    inc [r13].IPC_RING_BUFFER.tail
    mov eax, [r13].IPC_RING_BUFFER.tail
    cmp eax, IPC_MAX_MESSAGES
    jb @F
    mov [r13].IPC_RING_BUFFER.tail, 0
    
@@: dec [r13].IPC_RING_BUFFER.count
    
    mov eax, 1
    jmp recv_done
    
no_messages:
    xor eax, eax
    
recv_done:
    ; Release mutex
    mov rcx, [rbx].IPC_CONTEXT.h_mutex
    call ReleaseMutex
    
    pop r13
    pop r12
    pop rbx
    ret

recv_null_ctx:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret
IPC_RecvMessage ENDP

;================================================================================
; BLOCKING RECEIVE - Wait for message
;================================================================================
IPC_WaitForMessage PROC FRAME
    ; ecx = output buffer
    ; edx = timeout_ms
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .ENDPROLOG
    
    mov r12, rcx
    mov r13d, edx
    
    mov rbx, g_ipc_context
    
wait_loop:
    ; Try non-blocking receive first
    mov rcx, r12
    call IPC_RecvMessage
    test eax, eax
    jnz wait_done
    
    ; Wait for event
    cmp [rbx].IPC_CONTEXT.is_gui, 1
    je wait_gui_event
    mov rcx, [rbx].IPC_CONTEXT.h_event_cli
    jmp do_wait
    
wait_gui_event:
    mov rcx, [rbx].IPC_CONTEXT.h_event_gui
    
do_wait:
    mov edx, 100              ; 100ms timeout for polling
    call WaitForSingleObject
    
    ; Check for timeout
    cmp eax, WAIT_TIMEOUT
    jne wait_loop
    
    ; Decrement timeout
    sub r13d, 100
    jns wait_loop
    
    xor eax, eax
    
wait_done:
    pop r13
    pop r12
    pop rbx
    ret
IPC_WaitForMessage ENDP

;================================================================================
; GPU SHARING - Vulkan context sharing
;================================================================================
IPC_AttachGPU PROC FRAME
    ; Attach to shared GPU context
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    .ENDPROLOG
    mov rbx, g_ipc_context
    
    ; Wait for GPU to be free
    mov r12, [rbx].IPC_CONTEXT.shared_state
    xor r13d, r13d          ; Initialize timeout counter
    
gpu_wait_loop:
    cmp [r12].IPC_SHARED_STATE.gpu_busy, 0
    je gpu_available
    mov ecx, 1
    call Sleep
    ; Timeout after 30 seconds to prevent infinite spin
    inc r13d
    cmp r13d, 30000
    jae gpu_timeout
    jmp gpu_wait_loop
    
gpu_timeout:
    xor rax, rax            ; Return NULL on timeout
    pop r13
    pop r12
    pop rbx
    ret
    
gpu_available:
    ; Atomic compare-and-swap for gpu_busy flag
    xor eax, eax            ; Expected: 0 (not busy)
    mov cl, 1               ; Desired: 1 (busy)
    lock cmpxchg [r12].IPC_SHARED_STATE.gpu_busy, cl
    jnz gpu_wait_loop       ; CAS failed, another process got it first
    
    ; Return shared queue handle
    mov rax, [r12].IPC_SHARED_STATE.compute_queue
    
    pop r13
    pop r12
    pop rbx
    ret
IPC_AttachGPU ENDP

IPC_DetachGPU PROC FRAME
    ; Release GPU
    push rbx
    .pushreg rbx
    .ENDPROLOG
    mov rbx, g_ipc_context
    mov r8, [rbx].IPC_CONTEXT.shared_state
    mov [r8].IPC_SHARED_STATE.gpu_busy, 0
    pop rbx
    ret
IPC_DetachGPU ENDP

IPC_ShareVulkanContext PROC FRAME
    ; rcx = VkDevice
    ; rdx = VkQueue (compute)
    push rbx
    .pushreg rbx
    .ENDPROLOG
    mov rbx, g_ipc_context
    mov r8, [rbx].IPC_CONTEXT.shared_state
    
    mov [r8].IPC_SHARED_STATE.vulkan_device, rcx
    mov [r8].IPC_SHARED_STATE.compute_queue, rdx
    
    pop rbx
    ret
IPC_ShareVulkanContext ENDP

;================================================================================
; SHUTDOWN - Cleanup
;================================================================================
IPC_Shutdown PROC FRAME
    push rbx
    .pushreg rbx
    .ENDPROLOG
    mov rbx, g_ipc_context
    
    test rbx, rbx
    jz ipc_shutdown_done
    
    ; Signal shutdown
    mov r8, [rbx].IPC_CONTEXT.shared_state
    cmp [rbx].IPC_CONTEXT.is_gui, 1
    jne cli_shutdown
    mov [r8].IPC_SHARED_STATE.gui_active, 0
    jmp @F
cli_shutdown:
    mov [r8].IPC_SHARED_STATE.cli_active, 0
    
@@: ; Unmap view
    mov rcx, [rbx].IPC_CONTEXT.mapped_view
    test rcx, rcx
    jz @F
    call UnmapViewOfFile
    
@@: ; Close handles
    mov rcx, [rbx].IPC_CONTEXT.h_map_file
    call CloseHandle
    
    mov rcx, [rbx].IPC_CONTEXT.h_mutex
    call CloseHandle
    
    mov rcx, [rbx].IPC_CONTEXT.h_event_gui
    call CloseHandle
    
    mov rcx, [rbx].IPC_CONTEXT.h_event_cli
    call CloseHandle
    
    ; Free context
    mov rcx, rbx
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
    mov g_ipc_context, 0
    mov g_ipc_initialized, 0
    
ipc_shutdown_done:
    pop rbx
    ret
IPC_Shutdown ENDP

;================================================================================
; UTILITY
;================================================================================
memcpy PROC FRAME
    ; rcx = dest, rdx = src, r8d = count
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .ENDPROLOG
    
    mov rdi, rcx
    mov rsi, rdx
    mov ecx, r8d
    
    rep movsb
    
    pop rdi
    pop rsi
    ret
memcpy ENDP

;================================================================================
; END
;================================================================================
END
