; UEC-X MASM64 Core - Public Header for C++ Interop
; This file defines the C-compatible interface to the MASM64 dispatch core

; =============================================================================
; Error Codes (must match C++ definitions)
; =============================================================================
UEC_SUCCESS             EQU 0
UEC_ERROR_INVALID_PARAM EQU 1
UEC_ERROR_NO_MEMORY     EQU 2
UEC_ERROR_NOT_FOUND     EQU 3
UEC_ERROR_BUSY          EQU 4
UEC_ERROR_PERMISSION    EQU 5

; =============================================================================
; Command Types
; =============================================================================
UEC_CMD_REGISTER        EQU 1
UEC_CMD_UNREGISTER      EQU 2
UEC_CMD_EXECUTE         EQU 3
UEC_CMD_EMIT_EVENT      EQU 4
UEC_CMD_QUERY_STATE     EQU 5
UEC_CMD_HOTPATCH        EQU 6

; =============================================================================
; IPC Transport Types
; =============================================================================
UEC_IPC_NAMED_PIPE      EQU 0
UEC_IPC_SHARED_MEMORY   EQU 1
UEC_IPC_SOCKET          EQU 2
UEC_IPC_LOOPBACK        EQU 3

; =============================================================================
; Constants
; =============================================================================
UEC_DISPATCH_TABLE_SIZE EQU 4096
UEC_RING_BUFFER_SIZE    EQU 65536
UEC_MAX_EXTENSIONS      EQU 256
UEC_CACHE_LINE_SIZE     EQU 64

; =============================================================================
; Structure Definitions (for reference - use C++ headers for actual structs)
; =============================================================================

; UECDispatchStats - 32 bytes
;   dispatch_count      QWORD
;   dispatch_errors     QWORD
;   ipc_bytes_sent      QWORD
;   ipc_bytes_recv      QWORD

; UECExtensionSlot - 32 bytes
;   status              DWORD
;   id                  DWORD
;   context_ptr         QWORD
;   reserved            QWORD (2)

; UECEventHeader - 16 bytes
;   event_type          DWORD
;   payload_length      DWORD
;   timestamp           QWORD

; =============================================================================
; Function Prototypes (for C++ extern "C" declarations)
; =============================================================================

; Core lifecycle
; uec_dispatch_init PROC
;   Parameter:  rcx = config flags
;   Returns:    rax = error code

; uec_dispatch_shutdown PROC
;   Returns:    rax = error code

; Command registration
; uec_dispatch_register PROC
;   Parameters: rcx = command_id, rdx = handler_ptr, r8 = user_data
;   Returns:    rax = error code

; uec_dispatch_unregister PROC
;   Parameters: rcx = command_id
;   Returns:    rax = error code

; Command execution
; uec_dispatch_execute PROC
;   Parameters: rcx = command_id, rdx = param_buffer, r8 = param_length
;   Returns:    rax = result code

; Ring buffer operations
; uec_ring_enqueue PROC
;   Parameters: rcx = data_ptr, rdx = data_length
;   Returns:    rax = error code

; uec_ring_dequeue PROC
;   Parameters: rcx = out_buffer, rdx = max_length, r8 = actual_length_ptr
;   Returns:    rax = error code

; IPC transport
; uec_ipc_init_named_pipe PROC
;   Parameters: rcx = pipe_name, rdx = buffer_size
;   Returns:    rax = error code

; uec_ipc_init_shared_memory PROC
;   Parameters: rcx = shm_name, rdx = size
;   Returns:    rax = error code

; uec_ipc_send PROC
;   Parameters: rcx = data_ptr, rdx = data_length
;   Returns:    rax = error code

; uec_ipc_recv PROC
;   Parameters: rcx = buffer_ptr, rdx = buffer_size, r8 = bytes_received_ptr
;   Returns:    rax = error code

; Extension slot management
; uec_extension_alloc_slot PROC
;   Parameters: rcx = slot_id_ptr
;   Returns:    rax = error code

; uec_extension_free_slot PROC
;   Parameters: rcx = slot_id
;   Returns:    rax = error code

; uec_extension_get_context PROC
;   Parameters: rcx = slot_id
;   Returns:    rax = context_ptr

; Fast syscall wrapper
; uec_fast_syscall PROC
;   Parameters: rcx = syscall_num, rdx-r9 = args
;   Returns:    rax = result

; Statistics
; uec_get_stats PROC
;   Parameters: rcx = stats_ptr
;   Returns:    rax = error code

; =============================================================================
; Export Decorations
; =============================================================================

; These symbols are exported from the MASM64 DLL
EXTERNDEF uec_dispatch_init:PROC
EXTERNDEF uec_dispatch_register:PROC
EXTERNDEF uec_dispatch_execute:PROC
EXTERNDEF uec_ring_enqueue:PROC
EXTERNDEF uec_ring_dequeue:PROC
EXTERNDEF uec_ipc_init_named_pipe:PROC
EXTERNDEF uec_ipc_send:PROC
EXTERNDEF uec_ipc_recv:PROC
EXTERNDEF uec_extension_alloc_slot:PROC
EXTERNDEF uec_extension_free_slot:PROC
EXTERNDEF uec_fast_syscall:PROC
EXTERNDEF uec_get_stats:PROC

