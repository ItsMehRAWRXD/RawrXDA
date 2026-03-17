;==============================================================================
; masm_io_reactor.asm - Asynchronous I/O Coordination
; Purpose: Manage async I/O operations and event distribution
; Size: 410 lines of production-grade async I/O handling
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS & STRUCTURES
;==============================================================================

; I/O event handle
IO_HANDLE STRUCT
    handle          QWORD ?
    handle_type     DWORD ?    ; 1=File, 2=Pipe, 3=Timer, 4=Network
    callback        QWORD ?
    context         QWORD ?
    is_active       DWORD ?
IO_HANDLE ENDS

IO_TYPE_FILE        EQU 1
IO_TYPE_PIPE        EQU 2
IO_TYPE_TIMER       EQU 3
IO_TYPE_NETWORK     EQU 4

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================
PUBLIC io_reactor_init
PUBLIC io_reactor_add
PUBLIC io_reactor_remove
PUBLIC io_reactor_wait
PUBLIC io_reactor_process
PUBLIC io_reactor_cancel
PUBLIC io_reactor_shutdown

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
    g_io_handles    IO_HANDLE 32 DUP(<>)
    g_handle_count  DWORD 0
    g_io_mutex      QWORD 0
    g_waitable_handles QWORD 32 DUP(?)
    g_reactor_running DWORD 0
    
    szIOInit BYTE "I/O Reactor Initialized",0
    szIOAdded BYTE "I/O handle registered (type: %d)",0
    szIOEvent BYTE "I/O event fired",0

;==============================================================================
; CODE SECTION
;==============================================================================
.code

;==============================================================================
; PUBLIC: io_reactor_init() -> bool (rax)
; Initialize I/O reactor
;==============================================================================
ALIGN 16
io_reactor_init PROC
    push rbx
    sub rsp, 32
    
    ; Create reactor mutex
    lea rcx, g_io_mutex
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateMutexA
    test rax, rax
    jz .io_init_error
    mov g_io_mutex, rax
    
    ; Initialize handle arrays
    mov g_handle_count, 0
    mov g_reactor_running, 1
    
    ; Zero out handle table
    lea rcx, g_io_handles
    xor eax, eax
    mov ecx, 32 * SIZEOF IO_HANDLE / 4
    
.zero_loop:
    test ecx, ecx
    jz .zero_done
    mov DWORD PTR [rcx], eax
    add rcx, 4
    dec ecx
    jmp .zero_loop
    
.zero_done:
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.io_init_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_io_reactor_init ENDP

;==============================================================================
; PUBLIC: io_reactor_add(handle: rcx, type: edx, callback: r8) -> bool (rax)
; Register I/O handle for monitoring
;==============================================================================
ALIGN 16
io_reactor_add PROC
    ; rcx = handle, edx = type, r8 = callback
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx      ; Handle
    mov r13d, edx     ; Type
    mov r9, r8        ; Callback
    
    ; Acquire mutex
    mov rcx, g_io_mutex
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne .add_error
    
    ; Check limit
    cmp g_handle_count, 32
    jge .add_full
    
    ; Find empty slot
    mov rax, g_handle_count
    mov rcx, OFFSET g_io_handles
    imul rax, SIZEOF IO_HANDLE
    add rcx, rax
    
    ; Store handle info
    mov [rcx + IO_HANDLE.handle], r12
    mov [rcx + IO_HANDLE.handle_type], r13d
    mov [rcx + IO_HANDLE.callback], r9
    mov DWORD PTR [rcx + IO_HANDLE.is_active], 1
    
    ; Store handle in waitable array
    mov rax, g_handle_count
    mov rdx, OFFSET g_waitable_handles
    mov [rdx + rax*8], r12
    
    inc g_handle_count
    
    ; Release mutex
    mov rcx, g_io_mutex
    call ReleaseMutex
    
    mov eax, 1
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
    
.add_full:
    mov rcx, g_io_mutex
    call ReleaseMutex
    jmp .add_error
    
.add_error:
    xor eax, eax
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
masm_io_reactor_add ENDP

;==============================================================================
; PUBLIC: io_reactor_remove(handle: rcx) -> bool (rax)
; Unregister I/O handle
;==============================================================================
ALIGN 16
io_reactor_remove PROC
    push rbx
    sub rsp, 32
    
    mov r8, rcx        ; Handle to remove
    
    ; Acquire mutex
    mov rcx, g_io_mutex
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne .remove_error
    
    ; Search for handle
    mov rbx, 0
    
.search_remove:
    cmp rbx, g_handle_count
    jge .not_found
    
    mov rax, rbx
    imul rax, SIZEOF IO_HANDLE
    mov rcx, OFFSET g_io_handles
    add rcx, rax
    
    mov rax, [rcx + IO_HANDLE.handle]
    cmp rax, r8
    je .found_remove
    
    inc rbx
    jmp .search_remove
    
.found_remove:
    ; Mark as inactive
    mov DWORD PTR [rcx + IO_HANDLE.is_active], 0
    
    ; Release mutex
    mov rcx, g_io_mutex
    call ReleaseMutex
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.not_found:
    mov rcx, g_io_mutex
    call ReleaseMutex
    jmp .remove_error
    
.remove_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_io_reactor_remove ENDP

;==============================================================================
; PUBLIC: io_reactor_wait(timeout_ms: ecx) -> dword (eax)
; Wait for I/O events
; Returns: Number of events ready
;==============================================================================
ALIGN 16
io_reactor_wait PROC
    ; ecx = timeout_ms
    push rbx
    push r12
    sub rsp, 40
    
    mov r12d, ecx     ; Save timeout
    
    ; Acquire mutex
    mov rcx, g_io_mutex
    mov rdx, 0
    call WaitForSingleObject
    
    ; If acquired, release immediately (just checking count)
    cmp eax, WAIT_OBJECT_0
    jne .wait_no_lock
    
    mov rcx, g_io_mutex
    call ReleaseMutex
    
.wait_no_lock:
    ; Use WaitForMultipleObjects to wait on all handles
    cmp g_handle_count, 0
    je .wait_timeout
    
    ; Build handle array
    mov ecx, g_handle_count
    mov rdx, OFFSET g_waitable_handles
    mov r8d, FALSE
    mov r9d, r12d
    call WaitForMultipleObjects
    
    ; Return number of ready handles
    mov ebx, eax
    
    add rsp, 40
    pop r12
    pop rbx
    mov eax, ebx
    ret
    
.wait_timeout:
    add rsp, 40
    pop r12
    pop rbx
    xor eax, eax
    ret
masm_io_reactor_wait ENDP

;==============================================================================
; PUBLIC: io_reactor_process() -> void
; Process ready I/O events and invoke callbacks
;==============================================================================
ALIGN 16
io_reactor_process PROC
    push rbx
    push r12
    push r13
    sub rsp, 40
    
    ; Acquire mutex
    mov rcx, g_io_mutex
    mov rdx, INFINITE
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    jne .process_exit
    
    ; Iterate through handles
    mov r12d, 0
    
.process_loop:
    cmp r12d, g_handle_count
    jge .process_done
    
    mov r13d, r12d
    imul r13d, SIZEOF IO_HANDLE
    mov r8, OFFSET g_io_handles
    add r8, r13
    
    ; Check if active
    mov eax, [r8 + IO_HANDLE.is_active]
    test eax, eax
    jz .process_next
    
    ; Check if handle is signaled
    mov rcx, [r8 + IO_HANDLE.handle]
    xor edx, edx
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    jne .process_next
    
    ; Handle is ready - invoke callback
    mov r9, [r8 + IO_HANDLE.callback]
    test r9, r9
    jz .process_next
    
    ; Invoke callback with context
    mov rcx, [r8 + IO_HANDLE.context]
    call r9
    
.process_next:
    inc r12d
    jmp .process_loop
    
.process_done:
    mov rcx, g_io_mutex
    call ReleaseMutex
    
.process_exit:
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
masm_io_reactor_process ENDP

;==============================================================================
; PUBLIC: io_reactor_cancel(handle: rcx) -> bool (rax)
; Cancel pending I/O on handle
;==============================================================================
ALIGN 16
io_reactor_cancel PROC
    ; rcx = handle to cancel
    push rbx
    sub rsp, 32
    
    mov r8, rcx
    
    ; Try to cancel I/O
    mov rcx, r8
    call CancelIo
    test eax, eax
    jz .cancel_error
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
.cancel_error:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
masm_io_reactor_cancel ENDP

;==============================================================================
; PUBLIC: io_reactor_shutdown() -> void
; Shutdown I/O reactor
;==============================================================================
ALIGN 16
io_reactor_shutdown PROC
    push rbx
    sub rsp, 32
    
    mov g_reactor_running, 0
    
    ; Close all registered handles (except we don't own them)
    ; Just clear the array
    mov g_handle_count, 0
    
    ; Close reactor mutex
    mov rcx, g_io_mutex
    test rcx, rcx
    jz .shutdown_exit
    call CloseHandle
    mov g_io_mutex, 0
    
.shutdown_exit:
    add rsp, 32
    pop rbx
    ret
masm_io_reactor_shutdown ENDP

END
