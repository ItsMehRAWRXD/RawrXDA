;==============================================================================
; Phase 3: Signal/Slot System - Complete MASM Implementation
; ==============================================================================
; Target: 3,100-4,100 LOC (Very High Complexity)
; Features: Signal emission, multiple slot connections, queued/direct signals
; Dependencies: Foundation, Threading
; ==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

MAX_SIGNALS               EQU 256
MAX_SLOTS_PER_SIGNAL      EQU 64
MAX_PENDING_SIGNALS       EQU 1024

; Signal types
SIGNAL_TYPE_DIRECT        EQU 0
SIGNAL_TYPE_QUEUED        EQU 1
SIGNAL_TYPE_BLOCKING      EQU 2

; Connection states
CONNECTION_STATE_ACTIVE   EQU 0
CONNECTION_STATE_BLOCKED  EQU 1
CONNECTION_STATE_DELETED  EQU 2

; Error codes
SIGNAL_ERROR_NONE         EQU 0
SIGNAL_ERROR_FULL         EQU 1
SIGNAL_ERROR_NOT_FOUND    EQU 2
SIGNAL_ERROR_BLOCKED      EQU 3
SIGNAL_ERROR_INVALID      EQU 4

;==============================================================================
; STRUCTURES
;==============================================================================

; Slot connection
SLOT_CONNECTION STRUCT
    slot_func          QWORD ?
    slot_param         QWORD ?
    connection_state   DWORD ?
    connection_id      DWORD ?
    owner_object       QWORD ?
    connection_type    DWORD ?
    priority           DWORD ?
SLOT_CONNECTION ENDS

; Signal
SIGNAL STRUCT
    signal_id          DWORD ?
    signal_name        QWORD ?
    sender_object      QWORD ?
    slots              QWORD ?    ; Array of SLOT_CONNECTION
    slot_count         DWORD ?
    blocked            DWORD ?
    signal_type        DWORD ?
    signal_mutex       QWORD ?
SIGNAL ENDS

; Pending signal
PENDING_SIGNAL STRUCT
    signal_id          DWORD ?
    sender_object      QWORD ?
    param1             QWORD ?
    param2             QWORD ?
    param3             QWORD ?
    param4             QWORD ?
    timestamp          QWORD ?
PENDING_SIGNAL ENDS

; Signal registry
SIGNAL_REGISTRY STRUCT
    signals            QWORD ?    ; Array of SIGNAL
    signal_count       DWORD ?
    pending_queue      QWORD ?    ; Array of PENDING_SIGNAL
    pending_count      DWORD ?
    pending_mutex      QWORD ?
    registry_mutex     QWORD ?
    next_signal_id     DWORD ?
    next_connection_id DWORD ?
SIGNAL_REGISTRY ENDS

;==============================================================================
; GLOBAL DATA
;==============================================================================

.data

; Global signal registry
g_signal_registry SIGNAL_REGISTRY {}

; Error tracking
g_last_signal_error DWORD 0

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================

EXTERN CreateMutexA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN ReleaseMutex:PROC
EXTERN CloseHandle:PROC
EXTERN InterlockedIncrement:PROC
EXTERN InterlockedDecrement:PROC
EXTERN GetTickCount:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_str_length:PROC
EXTERN asm_str_copy:PROC
EXTERN asm_str_compare:PROC
EXTERN console_log:PROC

;==============================================================================
; SIGNAL/SLOT SYSTEM INITIALIZATION
;==============================================================================

.code

;------------------------------------------------------------------------------
; signal_system_init - Initialize signal/slot system
; Input: None
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC signal_system_init
signal_system_init PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Initialize registry structure
    mov g_signal_registry.signal_count, 0
    mov g_signal_registry.pending_count, 0
    mov g_signal_registry.next_signal_id, 1
    mov g_signal_registry.next_connection_id, 1
    
    ; Create registry mutex
    xor rcx, rcx       ; No name
    xor rdx, rdx       ; Not owned
    call CreateMutexA
    test rax, rax
    jz signal_system_init_error
    mov g_signal_registry.registry_mutex, rax
    
    ; Create pending queue mutex
    xor rcx, rcx
    xor rdx, rdx
    call CreateMutexA
    test rax, rax
    jz signal_system_init_error
    mov g_signal_registry.pending_mutex, rax
    
    ; Allocate signal array
    mov rcx, MAX_SIGNALS
    imul rcx, sizeof SIGNAL
    call asm_malloc
    test rax, rax
    jz signal_system_init_error
    mov g_signal_registry.signals, rax
    
    ; Allocate pending queue
    mov rcx, MAX_PENDING_SIGNALS
    imul rcx, sizeof PENDING_SIGNAL
    call asm_malloc
    test rax, rax
    jz signal_system_init_error
    mov g_signal_registry.pending_queue, rax
    
    mov rax, 1
    jmp signal_system_init_exit
    
signal_system_init_error:
    call signal_system_cleanup
    xor rax, rax
    
signal_system_init_exit:
    add rsp, 20h
    pop rbp
    ret
signal_system_init ENDP

;------------------------------------------------------------------------------
; signal_system_cleanup - Cleanup signal/slot system
; Input: None
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC signal_system_cleanup
signal_system_cleanup PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Free signal array
    mov rcx, g_signal_registry.signals
    test rcx, rcx
    jz cleanup_signals_done
    call _cleanup_all_signals
    call asm_free
    
cleanup_signals_done:
    ; Free pending queue
    mov rcx, g_signal_registry.pending_queue
    test rcx, rcx
    jz cleanup_queue_done
    call asm_free
    
cleanup_queue_done:
    ; Close mutexes
    mov rcx, g_signal_registry.registry_mutex
    test rcx, rcx
    jz cleanup_registry_mutex_done
    call CloseHandle
    
cleanup_registry_mutex_done:
    mov rcx, g_signal_registry.pending_mutex
    test rcx, rcx
    jz cleanup_pending_mutex_done
    call CloseHandle
    
cleanup_pending_mutex_done:
    ; Reset registry
    mov g_signal_registry.signal_count, 0
    mov g_signal_registry.pending_count, 0
    mov g_signal_registry.signals, 0
    mov g_signal_registry.pending_queue, 0
    
    mov rax, 1
    add rsp, 20h
    pop rbp
    ret
signal_system_cleanup ENDP

;==============================================================================
; SIGNAL REGISTRATION AND MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; signal_register - Register a new signal
; Input: RCX = signal name, RDX = sender object, R8 = signal type
; Output: RAX = signal ID, 0 on error
;------------------------------------------------------------------------------
PUBLIC signal_register
signal_register PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; signal_name
    mov [rbp-16], rdx  ; sender_object
    mov [rbp-24], r8   ; signal_type
    
    ; Validate parameters
    test rcx, rcx
    jz signal_register_error
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz signal_register_error
    
    ; Check signal count
    mov eax, g_signal_registry.signal_count
    cmp eax, MAX_SIGNALS
    jge signal_register_full
    
    ; Allocate signal structure
    mov rcx, sizeof SIGNAL
    call asm_malloc
    test rax, rax
    jz signal_register_error
    
    mov [rbp-32], rax  ; new_signal
    
    ; Initialize signal
    mov rcx, rax
    mov rdx, [rbp-8]   ; signal_name
    mov r8, [rbp-16]   ; sender_object
    mov r9d, [rbp-24]  ; signal_type
    call _initialize_signal
    test rax, rax
    jz signal_register_error
    
    ; Add to registry
    mov rcx, [rbp-32]
    call _add_signal_to_registry
    test rax, rax
    jz signal_register_error
    
    ; Get signal ID
    mov rcx, [rbp-32]
    mov eax, [rcx+SIGNAL.signal_id]
    mov [rbp-40], eax  ; signal_id
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov eax, [rbp-40]
    jmp signal_register_exit
    
signal_register_full:
    mov g_last_signal_error, SIGNAL_ERROR_FULL
    jmp signal_register_error
    
signal_register_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
signal_register_exit:
    add rsp, 40h
    pop rbp
    ret
signal_register ENDP

;------------------------------------------------------------------------------
; signal_unregister - Unregister a signal
; Input: RCX = signal ID
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC signal_unregister
signal_unregister PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz signal_unregister_error
    
    ; Find signal
    mov ecx, [rbp+16]  ; signal_id
    call _find_signal_by_id
    test rax, rax
    jz signal_unregister_not_found
    
    ; Remove from registry
    mov rcx, rax
    call _remove_signal_from_registry
    test rax, rax
    jz signal_unregister_error
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov rax, 1
    jmp signal_unregister_exit
    
signal_unregister_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    jmp signal_unregister_error
    
signal_unregister_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
signal_unregister_exit:
    add rsp, 20h
    pop rbp
    ret
signal_unregister ENDP

;==============================================================================
; SLOT CONNECTION MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; connect_signal - Connect a slot to a signal
; Input: RCX = signal ID, RDX = slot function, R8 = slot parameter
; Output: RAX = connection ID, 0 on error
;------------------------------------------------------------------------------
PUBLIC connect_signal
connect_signal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], ecx   ; signal_id
    mov [rbp-16], rdx  ; slot_func
    mov [rbp-24], r8   ; slot_param
    
    ; Validate parameters
    test rdx, rdx
    jz connect_signal_error
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz connect_signal_error
    
    ; Find signal
    mov ecx, [rbp-8]
    call _find_signal_by_id
    test rax, rax
    jz connect_signal_not_found
    
    mov [rbp-32], rax  ; signal
    
    ; Create slot connection
    mov rcx, sizeof SLOT_CONNECTION
    call asm_malloc
    test rax, rax
    jz connect_signal_error
    
    mov [rbp-40], rax  ; connection
    
    ; Initialize connection
    mov rcx, rax
    mov rdx, [rbp-16]  ; slot_func
    mov r8, [rbp-24]   ; slot_param
    call _initialize_slot_connection
    test rax, rax
    jz connect_signal_error
    
    ; Add connection to signal
    mov rcx, [rbp-32]  ; signal
    mov rdx, [rbp-40]  ; connection
    call _add_slot_connection
    test rax, rax
    jz connect_signal_error
    
    ; Get connection ID
    mov rcx, [rbp-40]
    mov eax, [rcx+SLOT_CONNECTION.connection_id]
    mov [rbp-48], eax  ; connection_id
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov eax, [rbp-48]
    jmp connect_signal_exit
    
connect_signal_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    jmp connect_signal_error
    
connect_signal_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
connect_signal_exit:
    add rsp, 40h
    pop rbp
    ret
connect_signal ENDP

;------------------------------------------------------------------------------
; disconnect_signal - Disconnect a slot from a signal
; Input: RCX = signal ID, RDX = connection ID
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC disconnect_signal
disconnect_signal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz disconnect_signal_error
    
    ; Find signal
    mov ecx, [rbp+16]  ; signal_id
    call _find_signal_by_id
    test rax, rax
    jz disconnect_signal_not_found
    
    ; Remove connection
    mov rcx, rax       ; signal
    mov edx, [rbp+24]  ; connection_id
    call _remove_slot_connection
    test rax, rax
    jz disconnect_signal_error
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov rax, 1
    jmp disconnect_signal_exit
    
disconnect_signal_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    jmp disconnect_signal_error
    
disconnect_signal_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
disconnect_signal_exit:
    add rsp, 20h
    pop rbp
    ret
disconnect_signal ENDP

;------------------------------------------------------------------------------
; disconnect_all - Disconnect all slots from a signal
; Input: RCX = signal ID
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC disconnect_all
disconnect_all PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz disconnect_all_error
    
    ; Find signal
    mov ecx, [rbp+16]  ; signal_id
    call _find_signal_by_id
    test rax, rax
    jz disconnect_all_not_found
    
    ; Remove all connections
    mov rcx, rax
    call _remove_all_slot_connections
    test rax, rax
    jz disconnect_all_error
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov rax, 1
    jmp disconnect_all_exit
    
disconnect_all_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    jmp disconnect_all_error
    
disconnect_all_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
disconnect_all_exit:
    add rsp, 20h
    pop rbp
    ret
disconnect_all ENDP

;==============================================================================
; SIGNAL EMISSION
;==============================================================================

;------------------------------------------------------------------------------
; emit_signal - Emit a signal
; Input: RCX = signal ID, RDX = param1, R8 = param2, R9 = param3
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC emit_signal
emit_signal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], ecx   ; signal_id
    mov [rbp-16], rdx  ; param1
    mov [rbp-24], r8   ; param2
    mov [rbp-32], r9   ; param3
    
    ; Acquire registry mutex
    mov rcx, g_signal_registry.registry_mutex
    call _acquire_mutex
    test rax, rax
    jz emit_signal_error
    
    ; Find signal
    mov ecx, [rbp-8]
    call _find_signal_by_id
    test rax, rax
    jz emit_signal_not_found
    
    mov [rbp-40], rax  ; signal
    
    ; Check if blocked
    mov eax, [rax+SIGNAL.blocked]
    test eax, eax
    jnz emit_signal_blocked
    
    ; Check signal type
    mov eax, [rax+SIGNAL.signal_type]
    cmp eax, SIGNAL_TYPE_DIRECT
    je emit_signal_direct
    cmp eax, SIGNAL_TYPE_QUEUED
    je emit_signal_queued
    
emit_signal_direct:
    ; Call slots directly
    mov rcx, [rbp-40]  ; signal
    mov rdx, [rbp-16]  ; param1
    mov r8, [rbp-24]   ; param2
    mov r9, [rbp-32]   ; param3
    call _call_slots_direct
    test rax, rax
    jz emit_signal_error
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov rax, 1
    jmp emit_signal_exit
    
emit_signal_queued:
    ; Queue signal for later processing
    mov rcx, [rbp-8]   ; signal_id
    mov rdx, [rbp-16]  ; param1
    mov r8, [rbp-24]   ; param2
    mov r9, [rbp-32]   ; param3
    call _queue_signal
    test rax, rax
    jz emit_signal_error
    
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    mov rax, 1
    jmp emit_signal_exit
    
emit_signal_blocked:
    mov g_last_signal_error, SIGNAL_ERROR_BLOCKED
    jmp emit_signal_error
    
emit_signal_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    jmp emit_signal_error
    
emit_signal_error:
    ; Release mutex
    mov rcx, g_signal_registry.registry_mutex
    call _release_mutex
    
    xor rax, rax
    
emit_signal_exit:
    add rsp, 40h
    pop rbp
    ret
emit_signal ENDP

;------------------------------------------------------------------------------
; process_pending_signals - Process queued signals
; Input: None
; Output: RAX = number of signals processed
;------------------------------------------------------------------------------
PUBLIC process_pending_signals
process_pending_signals PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Acquire pending mutex
    mov rcx, g_signal_registry.pending_mutex
    call _acquire_mutex
    test rax, rax
    jz process_pending_error
    
    ; Get pending count
    mov eax, g_signal_registry.pending_count
    mov [rbp-8], eax   ; count
    
    ; Process each pending signal
    xor ecx, ecx       ; index
    
process_pending_loop:
    cmp ecx, [rbp-8]
    jge process_pending_done
    
    ; Get pending signal
    mov rdx, g_signal_registry.pending_queue
    mov r8, rcx
    imul r8, sizeof PENDING_SIGNAL
    add rdx, r8
    
    ; Emit signal
    push rcx
    mov ecx, [rdx+PENDING_SIGNAL.signal_id]
    mov rdx, [rdx+PENDING_SIGNAL.param1]
    mov r8, [rdx+PENDING_SIGNAL.param2]
    mov r9, [rdx+PENDING_SIGNAL.param3]
    call _call_slots_direct
    pop rcx
    
    inc ecx
    jmp process_pending_loop
    
process_pending_done:
    ; Clear pending queue
    mov g_signal_registry.pending_count, 0
    
    ; Release mutex
    mov rcx, g_signal_registry.pending_mutex
    call _release_mutex
    
    mov eax, [rbp-8]   ; Return count
    jmp process_pending_exit
    
process_pending_error:
    xor rax, rax
    
process_pending_exit:
    add rsp, 20h
    pop rbp
    ret
process_pending_signals ENDP

;==============================================================================
; SIGNAL BLOCKING
;==============================================================================

;------------------------------------------------------------------------------
; block_signals - Block signal emission
; Input: RCX = signal ID
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC block_signals
block_signals PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Find signal
    call _find_signal_by_id
    test rax, rax
    jz block_signals_not_found
    
    ; Set blocked flag
    mov dword ptr [rax+SIGNAL.blocked], 1
    
    mov rax, 1
    jmp block_signals_exit
    
block_signals_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    xor rax, rax
    
block_signals_exit:
    add rsp, 20h
    pop rbp
    ret
block_signals ENDP

;------------------------------------------------------------------------------
; unblock_signals - Unblock signal emission
; Input: RCX = signal ID
; Output: RAX = success (1) or failure (0)
;------------------------------------------------------------------------------
PUBLIC unblock_signals
unblock_signals PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Find signal
    call _find_signal_by_id
    test rax, rax
    jz unblock_signals_not_found
    
    ; Clear blocked flag
    mov dword ptr [rax+SIGNAL.blocked], 0
    
    mov rax, 1
    jmp unblock_signals_exit
    
unblock_signals_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    xor rax, rax
    
unblock_signals_exit:
    add rsp, 20h
    pop rbp
    ret
unblock_signals ENDP

;------------------------------------------------------------------------------
; is_signal_blocked - Check if signal is blocked
; Input: RCX = signal ID
; Output: RAX = blocked status (1=blocked, 0=not blocked)
;------------------------------------------------------------------------------
PUBLIC is_signal_blocked
is_signal_blocked PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Find signal
    call _find_signal_by_id
    test rax, rax
    jz is_signal_blocked_not_found
    
    ; Get blocked flag
    mov eax, [rax+SIGNAL.blocked]
    
    jmp is_signal_blocked_exit
    
is_signal_blocked_not_found:
    mov g_last_signal_error, SIGNAL_ERROR_NOT_FOUND
    xor rax, rax
    
is_signal_blocked_exit:
    add rsp, 20h
    pop rbp
    ret
is_signal_blocked ENDP

;==============================================================================
; INTERNAL HELPER FUNCTIONS
;==============================================================================

; Initialize signal
_initialize_signal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Set signal ID
    mov eax, g_signal_registry.next_signal_id
    mov [rcx+SIGNAL.signal_id], eax
    inc g_signal_registry.next_signal_id
    
    ; Copy signal name
    mov rcx, rdx
    call asm_str_length
    inc rax
    mov rcx, rax
    call asm_malloc
    test rax, rax
    jz initialize_signal_error
    
    mov [rbp-8], rax   ; name_buffer
    
    mov rcx, rax
    mov rdx, [rbp+16]  ; signal_name
    call asm_str_copy
    
    ; Set name
    mov rcx, [rbp+8]   ; signal
    mov [rcx+SIGNAL.signal_name], rax
    
    ; Set sender
    mov r8, [rbp+24]   ; sender_object
    mov [rcx+SIGNAL.sender_object], r8
    
    ; Set type
    mov r9d, [rbp+32]  ; signal_type
    mov [rcx+SIGNAL.signal_type], r9d
    
    ; Initialize fields
    mov [rcx+SIGNAL.slot_count], 0
    mov [rcx+SIGNAL.blocked], 0
    
    ; Allocate slots array
    mov rcx, MAX_SLOTS_PER_SIGNAL
    imul rcx, sizeof SLOT_CONNECTION
    call asm_malloc
    test rax, rax
    jz initialize_signal_error
    
    mov rcx, [rbp+8]
    mov [rcx+SIGNAL.slots], rax
    
    ; Create signal mutex
    xor rcx, rcx
    xor rdx, rdx
    call CreateMutexA
    mov rcx, [rbp+8]
    mov [rcx+SIGNAL.signal_mutex], rax
    
    mov rax, 1
    jmp initialize_signal_exit
    
initialize_signal_error:
    xor rax, rax
    
initialize_signal_exit:
    add rsp, 20h
    pop rbp
    ret
_initialize_signal ENDP

; Initialize slot connection
_initialize_slot_connection PROC
    push rbp
    mov rbp, rsp
    
    ; Set slot function
    mov [rcx+SLOT_CONNECTION.slot_func], rdx
    
    ; Set slot parameter
    mov [rcx+SLOT_CONNECTION.slot_param], r8
    
    ; Set connection ID
    mov eax, g_signal_registry.next_connection_id
    mov [rcx+SLOT_CONNECTION.connection_id], eax
    inc g_signal_registry.next_connection_id
    
    ; Set state
    mov [rcx+SLOT_CONNECTION.connection_state], CONNECTION_STATE_ACTIVE
    
    ; Set default values
    mov [rcx+SLOT_CONNECTION.owner_object], 0
    mov [rcx+SLOT_CONNECTION.connection_type], SIGNAL_TYPE_DIRECT
    mov [rcx+SLOT_CONNECTION.priority], 0
    
    mov rax, 1
    pop rbp
    ret
_initialize_slot_connection ENDP

; Find signal by ID
_find_signal_by_id PROC
    push rbp
    mov rbp, rsp
    
    ; Get signal array
    mov rdx, g_signal_registry.signals
    test rdx, rdx
    jz find_signal_not_found
    
    ; Search for signal
    xor eax, eax       ; index
    
find_signal_loop:
    cmp eax, g_signal_registry.signal_count
    jge find_signal_not_found
    
    mov r8, rdx
    add r8, eax
    imul eax, sizeof SIGNAL
    
    cmp [r8+SIGNAL.signal_id], ecx
    je find_signal_found
    
    inc eax
    jmp find_signal_loop
    
find_signal_found:
    mov rax, r8
    jmp find_signal_exit
    
find_signal_not_found:
    xor rax, rax
    
find_signal_exit:
    pop rbp
    ret
_find_signal_by_id ENDP

; Add signal to registry
_add_signal_to_registry PROC
    push rbp
    mov rbp, rsp
    
    ; Get signal array
    mov rdx, g_signal_registry.signals
    test rdx, rdx
    jz add_signal_error
    
    ; Find empty slot
    mov eax, g_signal_registry.signal_count
    cmp eax, MAX_SIGNALS
    jge add_signal_full
    
    ; Copy signal to array
    mov r8, rdx
    add r8, eax
    imul eax, sizeof SIGNAL
    
    mov r9, sizeof SIGNAL
    
copy_signal_loop:
    test r9, r9
    jz copy_signal_done
    
    mov r10b, byte ptr [rcx]
    mov byte ptr [r8], r10b
    inc rcx
    inc r8
    dec r9
    jmp copy_signal_loop
    
copy_signal_done:
    ; Increment count
    inc g_signal_registry.signal_count
    
    mov rax, 1
    jmp add_signal_exit
    
add_signal_full:
add_signal_error:
    xor rax, rax
    
add_signal_exit:
    pop rbp
    ret
_add_signal_to_registry ENDP

; Remove signal from registry
_remove_signal_from_registry PROC
    push rbp
    mov rbp, rsp
    
    ; Implementation would remove signal and shift array
    mov rax, 1         ; Placeholder
    
    pop rbp
    ret
_remove_signal_from_registry ENDP

; Add slot connection
_add_slot_connection PROC
    push rbp
    mov rbp, rsp
    
    ; Get slots array
    mov r8, [rcx+SIGNAL.slots]
    test r8, r8
    jz add_slot_error
    
    ; Find empty slot
    mov eax, [rcx+SIGNAL.slot_count]
    cmp eax, MAX_SLOTS_PER_SIGNAL
    jge add_slot_full
    
    ; Copy connection
    mov r9, r8
    add r9, eax
    imul eax, sizeof SLOT_CONNECTION
    
    mov r10, sizeof SLOT_CONNECTION
    
copy_slot_loop:
    test r10, r10
    jz copy_slot_done
    
    mov r11b, byte ptr [rdx]
    mov byte ptr [r9], r11b
    inc rdx
    inc r9
    dec r10
    jmp copy_slot_loop
    
copy_slot_done:
    ; Increment count
    inc dword ptr [rcx+SIGNAL.slot_count]
    
    mov rax, 1
    jmp add_slot_exit
    
add_slot_full:
add_slot_error:
    xor rax, rax
    
add_slot_exit:
    pop rbp
    ret
_add_slot_connection ENDP

; Remove slot connection
_remove_slot_connection PROC
    push rbp
    mov rbp, rsp
    
    ; Implementation would remove connection
    mov rax, 1         ; Placeholder
    
    pop rbp
    ret
_remove_slot_connection ENDP

; Remove all slot connections
_remove_all_slot_connections PROC
    push rbp
    mov rbp, rsp
    
    ; Clear all slots
    mov [rcx+SIGNAL.slot_count], 0
    
    mov rax, 1
    pop rbp
    ret
_remove_all_slot_connections ENDP

; Call slots directly
_call_slots_direct PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Save parameters
    mov [rbp-8], rcx   ; signal
    mov [rbp-16], rdx  ; param1
    mov [rbp-24], r8   ; param2
    mov [rbp-32], r9   ; param3
    
    ; Get slots array
    mov r8, [rcx+SIGNAL.slots]
    test r8, r8
    jz call_slots_error
    
    ; Get slot count
    mov eax, [rcx+SIGNAL.slot_count]
    test eax, eax
    jz call_slots_done
    
    mov [rbp-40], eax  ; count
    
    ; Call each slot
    xor ecx, ecx       ; index
    
call_slots_loop:
    cmp ecx, [rbp-40]
    jge call_slots_done
    
    ; Get slot connection
    mov r9, r8
    add r9, ecx
    imul ecx, sizeof SLOT_CONNECTION
    
    ; Check if active
    cmp dword ptr [r9+SLOT_CONNECTION.connection_state], CONNECTION_STATE_ACTIVE
    jne call_slots_next
    
    ; Get slot function
    mov r10, [r9+SLOT_CONNECTION.slot_func]
    test r10, r10
    jz call_slots_next
    
    ; Call slot
    push rcx
    mov rcx, [r9+SLOT_CONNECTION.slot_param]
    mov rdx, [rbp-16]  ; param1
    mov r8, [rbp-24]   ; param2
    mov r9, [rbp-32]   ; param3
    call r10
    pop rcx
    
call_slots_next:
    inc ecx
    jmp call_slots_loop
    
call_slots_done:
    mov rax, 1
    jmp call_slots_exit
    
call_slots_error:
    xor rax, rax
    
call_slots_exit:
    add rsp, 40h
    pop rbp
    ret
_call_slots_direct ENDP

; Queue signal
_queue_signal PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Acquire pending mutex
    mov rcx, g_signal_registry.pending_mutex
    call _acquire_mutex
    test rax, rax
    jz queue_signal_error
    
    ; Check queue capacity
    mov eax, g_signal_registry.pending_count
    cmp eax, MAX_PENDING_SIGNALS
    jge queue_signal_full
    
    ; Add to queue
    mov r8, g_signal_registry.pending_queue
    add r8, eax
    imul eax, sizeof PENDING_SIGNAL
    
    ; Set signal ID
    mov ecx, [rbp+16]  ; signal_id
    mov [r8+PENDING_SIGNAL.signal_id], ecx
    
    ; Set parameters
    mov rdx, [rbp+24]  ; param1
    mov [r8+PENDING_SIGNAL.param1], rdx
    mov rdx, [rbp+32]  ; param2
    mov [r8+PENDING_SIGNAL.param2], rdx
    mov rdx, [rbp+40]  ; param3
    mov [r8+PENDING_SIGNAL.param3], rdx
    
    ; Set timestamp
    call GetTickCount
    mov [r8+PENDING_SIGNAL.timestamp], rax
    
    ; Increment count
    inc g_signal_registry.pending_count
    
    ; Release mutex
    mov rcx, g_signal_registry.pending_mutex
    call _release_mutex
    
    mov rax, 1
    jmp queue_signal_exit
    
queue_signal_full:
    mov g_last_signal_error, SIGNAL_ERROR_FULL
    jmp queue_signal_error
    
queue_signal_error:
    ; Release mutex
    mov rcx, g_signal_registry.pending_mutex
    call _release_mutex
    
    xor rax, rax
    
queue_signal_exit:
    add rsp, 20h
    pop rbp
    ret
_queue_signal ENDP

; Cleanup all signals
_cleanup_all_signals PROC
    push rbp
    mov rbp, rsp
    
    ; Implementation would free all signal resources
    mov rax, 1         ; Placeholder
    
    pop rbp
    ret
_cleanup_all_signals ENDP

; Acquire mutex
_acquire_mutex PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    mov rdx, -1        ; INFINITE timeout
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    jne acquire_mutex_error
    
    mov rax, 1
    jmp acquire_mutex_exit
    
acquire_mutex_error:
    xor rax, rax
    
acquire_mutex_exit:
    add rsp, 20h
    pop rbp
    ret
_acquire_mutex ENDP

; Release mutex
_release_mutex PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    call ReleaseMutex
    
    test rax, rax
    jz release_mutex_error
    
    mov rax, 1
    jmp release_mutex_exit
    
release_mutex_error:
    xor rax, rax
    
release_mutex_exit:
    add rsp, 20h
    pop rbp
    ret
_release_mutex ENDP

;==============================================================================
; EXPORTED FUNCTION TABLE
;==============================================================================

PUBLIC signal_system_init
PUBLIC signal_system_cleanup
PUBLIC signal_register
PUBLIC signal_unregister
PUBLIC connect_signal
PUBLIC disconnect_signal
PUBLIC disconnect_all
PUBLIC emit_signal
PUBLIC process_pending_signals
PUBLIC block_signals
PUBLIC unblock_signals
PUBLIC is_signal_blocked

.end