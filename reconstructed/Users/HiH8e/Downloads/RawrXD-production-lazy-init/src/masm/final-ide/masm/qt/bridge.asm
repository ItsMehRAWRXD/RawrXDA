;==============================================================================
; masm_qt_bridge.asm - Qt/MASM Signal and Function Bridge
; Purpose: Marshal Qt signals/slots and invoke callbacks from MASM
; Author: RawrXD CI/CD
; Date: Dec 28, 2025
;==============================================================================

option casemap:none

include windows.inc
include masm_master_defs.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS & STRUCTURES
;==============================================================================

; Signal callback entry structure
SIGNAL_CALLBACK STRUCT
    signal_id       DWORD ?
    callback_addr   QWORD ?
    context         QWORD ?
    is_active       DWORD ?
    param_count     DWORD ?
SIGNAL_CALLBACK ENDS

; Qt parameter wrapper
QT_PARAM STRUCT
    param_type      DWORD ?  ; 0=int, 1=string, 2=double, 3=void*
    param_value     QWORD ?
    param_len       DWORD ?
QT_PARAM ENDS

; Signal ID constants (mapped from Qt signals)
SIG_CHAT_MESSAGE_RECEIVED    EQU 1001h
SIG_FILE_OPENED              EQU 1002h
SIG_TERMINAL_OUTPUT          EQU 1003h
SIG_HOTPATCH_APPLIED         EQU 1004h
SIG_EDITOR_TEXT_CHANGED      EQU 1005h
SIG_PANE_RESIZED             EQU 1006h
SIG_FAILURE_DETECTED         EQU 1007h
SIG_CORRECTION_APPLIED       EQU 1008h

; Event queue size constant
MAX_PENDING_EVENTS           EQU 256
SIGNAL_CALLBACK_SIZE         EQU SIZEOF SIGNAL_CALLBACK
MAX_CALLBACK_SLOTS           EQU 32

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================
PUBLIC masm_qt_bridge_init
PUBLIC masm_signal_connect
PUBLIC masm_signal_disconnect
PUBLIC masm_signal_emit
PUBLIC masm_callback_invoke
PUBLIC masm_event_pump
PUBLIC masm_thread_safe_call

;==============================================================================
; GLOBAL DATA SECTION
;==============================================================================
.data
    ; Callback array (32 slots, 32 bytes each = 1KB)
    g_signal_callbacks SIGNAL_CALLBACK 32 DUP(<>)
    
    ; Global state variables
    g_callback_count   DWORD 0              ; Number of active callbacks
    g_bridge_mutex     QWORD 0              ; HANDLE to bridge synchronization mutex
    g_pending_events   QWORD 0              ; Pointer to event queue
    g_event_count      DWORD 0              ; Number of pending events
    
    ; Debug strings (for logging/diagnostics)
    szBridgeInitMsg    BYTE "Qt/MASM Bridge Initialized",0
    szSignalConnected  BYTE "Signal connected",0
    szCallbackInvoked  BYTE "Callback invoked",0
    szErrorInit        BYTE "Bridge initialization failed",0

.data?
    ; Uninitialized globals
    g_tls_slot         DWORD ?              ; Thread-local storage slot
    g_heap_handle      QWORD ?              ; Process heap handle

;==============================================================================
; CODE SECTION
;==============================================================================
.code
ALIGN 16

;==============================================================================
; PUBLIC: masm_qt_bridge_init() -> bool (rax)
; Initialize the Qt/MASM bridge system
; Returns: 1 = success, 0 = failure
;==============================================================================
masm_qt_bridge_init PROC
    push rbx
    sub rsp, 40h
    
    ; Get process heap handle
    call GetProcessHeap
    mov g_heap_handle, rax
    test rax, rax
    jz init_error
    
    ; Create mutex for thread safety
    xor rcx, rcx                    ; lpMutexAttributes = NULL
    mov rdx, 0                      ; bInitialOwner = FALSE
    lea r8, [szBridgeInitMsg]
    call CreateMutexA
    test rax, rax
    jz init_error
    mov g_bridge_mutex, rax
    
    ; Initialize callback count to 0
    mov g_callback_count, 0
    mov g_event_count, 0
    
    ; Allocate pending event queue (65536 bytes)
    mov rcx, g_heap_handle          ; hHeap
    xor rdx, rdx                    ; dwFlags = 0
    mov r8, 65536                   ; dwBytes
    call HeapAlloc
    test rax, rax
    jz init_error
    mov g_pending_events, rax
    
    ; Success return
    mov eax, 1
    add rsp, 40h
    pop rbx
    ret
    
init_error:
    xor eax, eax                    ; Return 0 (failure)
    add rsp, 40h
    pop rbx
    ret
masm_qt_bridge_init ENDP

;==============================================================================
; PUBLIC: masm_signal_connect(ecx=signal_id, rdx=callback_addr) -> bool (rax)
; Register a signal handler callback
; Args: RCX = signal_id, RDX = callback address
; Returns: 1 = success, 0 = failure
;==============================================================================
ALIGN 16
masm_signal_connect PROC
    push rbx
    push r12
    sub rsp, 32h
    
    mov r12d, ecx                   ; Save signal_id
    mov r10, rdx                    ; Save callback_addr
    
    ; Acquire bridge mutex for thread safety
    mov r8, g_bridge_mutex
    mov rcx, r8
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne connect_failed
    
    ; Check if we have space (max 32 callbacks)
    mov eax, g_callback_count
    cmp eax, MAX_CALLBACK_SLOTS
    jge connect_full
    
    ; Find an empty slot in the callback array
    xor r11d, r11d                  ; slot_index = 0
    mov rbx, OFFSET g_signal_callbacks
    
search_empty_slot:
    cmp r11d, MAX_CALLBACK_SLOTS
    jge add_new_callback_entry
    
    mov eax, r11d
    imul eax, SIGNAL_CALLBACK_SIZE
    mov r9d, [rbx + rax]            ; Check signal_id field
    test r9d, r9d
    jz found_empty_slot
    
    inc r11d
    jmp search_empty_slot
    
found_empty_slot:
    ; Store signal and callback in the empty slot
    mov eax, r11d
    jmp store_callback_data
    
add_new_callback_entry:
    mov eax, g_callback_count
    mov r11d, eax
    inc g_callback_count
    
store_callback_data:
    ; RBX = base of g_signal_callbacks, EAX = slot index
    imul eax, SIGNAL_CALLBACK_SIZE
    add rax, rbx
    
    ; Fill SIGNAL_CALLBACK structure at RBX + offset
    mov [rax + 0],  r12d            ; signal_id
    mov [rax + 8],  r10             ; callback_addr
    mov QWORD PTR [rax + 16], 0     ; context
    mov DWORD PTR [rax + 24], 1     ; is_active
    mov DWORD PTR [rax + 28], 0     ; param_count
    
    ; Release mutex
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    
    ; Success
    mov eax, 1
    add rsp, 32h
    pop r12
    pop rbx
    ret
    
connect_full:
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    jmp connect_failed
    
connect_failed:
    xor eax, eax                    ; Return 0 (failure)
    add rsp, 32h
    pop r12
    pop rbx
    ret
masm_signal_connect ENDP

;==============================================================================
; PUBLIC: masm_signal_disconnect(ecx=signal_id) -> bool (rax)
; Unregister a signal handler callback
; Args: RCX = signal_id
; Returns: 1 = success, 0 = not found/failure
;==============================================================================
ALIGN 16
masm_signal_disconnect PROC
    push rbx
    push r12
    sub rsp, 32h
    
    mov r12d, ecx                   ; Save signal_id
    
    ; Acquire mutex
    mov r8, g_bridge_mutex
    mov rcx, r8
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne disconnect_failed
    
    ; Search for matching signal_id
    xor r11d, r11d                  ; index = 0
    mov rbx, OFFSET g_signal_callbacks
    
search_signal_id:
    cmp r11d, g_callback_count
    jge signal_not_found
    
    mov eax, r11d
    imul eax, SIGNAL_CALLBACK_SIZE
    mov r10d, [rbx + rax]           ; Check signal_id
    cmp r10d, r12d
    je found_signal_to_remove
    
    inc r11d
    jmp search_signal_id
    
found_signal_to_remove:
    ; Clear the callback entry (zero out structure)
    mov eax, r11d
    imul eax, SIGNAL_CALLBACK_SIZE
    add rax, rbx
    
    mov DWORD PTR [rax + 0], 0      ; signal_id = 0
    mov QWORD PTR [rax + 8], 0      ; callback_addr = 0
    mov QWORD PTR [rax + 16], 0     ; context = 0
    mov DWORD PTR [rax + 24], 0     ; is_active = 0
    mov DWORD PTR [rax + 28], 0     ; param_count = 0
    
    ; Release mutex
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    
    mov eax, 1                      ; Success
    add rsp, 32h
    pop r12
    pop rbx
    ret
    
signal_not_found:
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    
disconnect_failed:
    xor eax, eax                    ; Return 0 (failure)
    add rsp, 32h
    pop r12
    pop rbx
    ret
masm_signal_disconnect ENDP

;==============================================================================
; PUBLIC: masm_signal_emit(ecx=signal_id, rdx=param_count, r8=params_ptr) -> bool (rax)
; Emit a signal and invoke all registered callbacks
; Args: RCX = signal_id, RDX = parameter count, R8 = parameter array
; Returns: 1 = success, 0 = failure
;==============================================================================
ALIGN 16
masm_signal_emit PROC
    push rbx
    push r12
    push r13
    sub rsp, 32h
    
    mov r12d, ecx                   ; Save signal_id
    mov r13d, edx                   ; Save param_count
    mov r10, r8                     ; Save params_ptr
    
    ; Acquire mutex
    mov r8, g_bridge_mutex
    mov rcx, r8
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne emit_failed
    
    ; Iterate through callbacks and find matching signal_id
    xor r11d, r11d                  ; callback_index = 0
    mov rbx, OFFSET g_signal_callbacks
    
emit_callback_loop:
    cmp r11d, g_callback_count
    jge emit_complete
    
    mov eax, r11d
    imul eax, SIGNAL_CALLBACK_SIZE
    mov r9d, [rbx + rax]            ; signal_id field
    test r9d, r9d
    jz emit_next_callback
    
    cmp r9d, r12d
    jne emit_next_callback
    
    ; Found matching callback - invoke it
    ; RBX + EAX = pointer to SIGNAL_CALLBACK structure
    mov r9, [rbx + rax + 8]         ; callback_addr (qword at offset 8)
    test r9, r9
    jz emit_next_callback
    
    ; Call the callback with parameters
    ; Parameters: RCX = signal_id, RDX = param_count, R8 = params_ptr
    mov ecx, r12d
    mov edx, r13d
    mov r8, r10
    call r9
    
emit_next_callback:
    inc r11d
    jmp emit_callback_loop
    
emit_complete:
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    
    mov eax, 1                      ; Success
    add rsp, 32h
    pop r13
    pop r12
    pop rbx
    ret
    
emit_failed:
    xor eax, eax
    add rsp, 32h
    pop r13
    pop r12
    pop rbx
    ret
masm_signal_emit ENDP

;==============================================================================
; PUBLIC: masm_callback_invoke(rcx=callback_addr, rdx=param) -> QWORD (rax)
; Invoke a callback directly with a single parameter
; Args: RCX = callback address, RDX = parameter
; Returns: return value from callback
;==============================================================================
ALIGN 16
masm_callback_invoke PROC
    sub rsp, 32h
    
    mov rax, rcx
    test rax, rax
    jz invoke_failed
    
    ; Call with single parameter in RDX
    mov rcx, rdx
    call rax
    
    add rsp, 32h
    ret
    
invoke_failed:
    xor eax, eax
    add rsp, 32h
    ret
masm_callback_invoke ENDP

;==============================================================================
; PUBLIC: masm_event_pump() -> DWORD (rax)
; Process all pending events in the event queue
; Returns: number of events processed
;==============================================================================
ALIGN 16
masm_event_pump PROC
    push rbx
    push r12
    sub rsp, 32h
    
    ; Get event count
    mov eax, g_event_count
    test eax, eax
    jz event_pump_empty
    
    xor r12d, r12d                  ; processed_count = 0
    mov rbx, g_pending_events
    mov r10d, g_event_count
    
process_event_loop:
    cmp r12d, r10d
    jge event_pump_done
    
    ; Process event at g_pending_events[r12d]
    ; (Event processing would happen here)
    
    inc r12d
    jmp process_event_loop
    
event_pump_done:
    ; Clear event count
    mov g_event_count, 0
    mov eax, r10d                   ; Return count of processed events
    add rsp, 32h
    pop r12
    pop rbx
    ret
    
event_pump_empty:
    xor eax, eax
    add rsp, 32h
    pop r12
    pop rbx
    ret
masm_event_pump ENDP

;==============================================================================
; PUBLIC: masm_thread_safe_call(rcx=func_ptr, rdx=arg1, r8=arg2) -> QWORD (rax)
; Invoke a function with mutex protection
; Args: RCX = function pointer, RDX = arg1, R8 = arg2
; Returns: function return value
;==============================================================================
ALIGN 16
masm_thread_safe_call PROC
    push rbx
    sub rsp, 32h
    
    mov rax, rcx
    test rax, rax
    jz tsc_failed
    
    ; Acquire mutex
    mov r8, g_bridge_mutex
    mov rcx, r8
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne tsc_failed
    
    ; Call function with arguments
    mov rax, [rsp + 40h]            ; Get func_ptr from stack
    mov rcx, [rsp + 48h]            ; Get arg1
    mov rdx, [rsp + 50h]            ; Get arg2
    call rax
    mov rbx, rax                    ; Save return value
    
    ; Release mutex
    mov rcx, g_bridge_mutex
    call ReleaseMutex
    
    mov rax, rbx                    ; Restore return value
    add rsp, 32h
    pop rbx
    ret
    
tsc_failed:
    xor eax, eax
    add rsp, 32h
    pop rbx
    ret
masm_thread_safe_call ENDP

;==============================================================================
; END OF FILE
;==============================================================================
END
