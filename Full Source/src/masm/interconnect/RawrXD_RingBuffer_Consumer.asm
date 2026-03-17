; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Consumer.asm
; Stub implementation for ring buffer consumption
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

RingBufferConsumer_Initialize PROC FRAME
    ; RCX = hWnd (target window for WM_APP messages)
    ; RDX = Vocab Table (pointer to token→string lookup)
    ; Returns: RAX = 1 on success, 0 on failure
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx                     ; hWnd
    mov r12, rdx                     ; vocab table
    
    ; Store config
    lea rcx, [g_ConsumerHwnd]
    mov [rcx], rbx
    lea rcx, [g_VocabTable]
    mov [rcx], r12
    
    ; Allocate ring buffer: 4096 entries x 8 bytes (token IDs)
    mov rcx, 0
    mov edx, 32768                   ; 4096 * 8
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@rbc_fail
    lea rcx, [g_RingBuffer]
    mov [rcx], rax
    
    ; Initialize head/tail indices
    lea rcx, [g_RingHead]
    mov DWORD PTR [rcx], 0
    lea rcx, [g_RingTail]
    mov DWORD PTR [rcx], 0
    
    ; Create consumer event (auto-reset)
    xor ecx, ecx
    xor edx, edx                     ; auto-reset
    xor r8d, r8d                     ; non-signaled
    xor r9d, r9d
    call CreateEventA
    lea rcx, [g_ConsumerEvent]
    mov [rcx], rax
    
    lea rcx, [g_ConsumerReady]
    mov DWORD PTR [rcx], 1
    
    mov rax, 1
    add rsp, 48
    pop r12
    pop rbx
    ret
    
@@rbc_fail:
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
RingBufferConsumer_Initialize ENDP

RingBufferConsumer_Shutdown PROC FRAME
    ; Gracefully shut down the ring buffer consumer
    ; Free ring buffer, close event handle, zero state
    ; Returns: EAX = 0 on success
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Mark not ready
    lea rcx, [g_ConsumerReady]
    mov DWORD PTR [rcx], 0
    
    ; Free ring buffer
    lea rcx, [g_RingBuffer]
    mov rcx, [rcx]
    test rcx, rcx
    jz @@rbs_skip_free
    xor edx, edx
    mov r8d, 8000h                   ; MEM_RELEASE
    call VirtualFree
    lea rcx, [g_RingBuffer]
    mov QWORD PTR [rcx], 0
@@rbs_skip_free:
    
    ; Close event handle
    lea rcx, [g_ConsumerEvent]
    mov rcx, [rcx]
    test rcx, rcx
    jz @@rbs_skip_event
    call CloseHandle
    lea rcx, [g_ConsumerEvent]
    mov QWORD PTR [rcx], 0
@@rbs_skip_event:
    
    ; Zero indices
    lea rcx, [g_RingHead]
    mov DWORD PTR [rcx], 0
    lea rcx, [g_RingTail]
    mov DWORD PTR [rcx], 0
    
    xor eax, eax
    add rsp, 40
    ret
RingBufferConsumer_Shutdown ENDP

PUBLIC RingBufferConsumer_Initialize
PUBLIC RingBufferConsumer_Shutdown

END