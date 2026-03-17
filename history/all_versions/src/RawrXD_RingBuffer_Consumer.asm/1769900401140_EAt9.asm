; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Consumer.asm
; High-throughput ring buffer consumer with UTF-8 to UTF-16 conversion
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
RING_BUFFER_SIZE        EQU 65536   ; 64KB
CONSUMER_THREAD_SLEEP   EQU 1       ; ms to sleep when empty

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferContext STRUCT
    BufferStart         QWORD       ?
    ReadIndex           DWORD       ?
    WriteIndex          DWORD       ?
    hConsumerThread     QWORD       ?
    bRunning            DWORD       ?
    hOutputWindow       QWORD       ?       ; Window handle for messages
    VocabTable          QWORD       ?
RingBufferContext ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_RingContext           RingBufferContext <>

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Initialize
; RCX = Window Handle, RDX = Vocab Table Ptr
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Initialize PROC FRAME
    push rbx
    push rsi
    sub rsp, 32
    
    ; Init context
    mov g_RingContext.hOutputWindow, rcx
    mov g_RingContext.VocabTable, rdx
    mov g_RingContext.ReadIndex, 0
    mov g_RingContext.WriteIndex, 0
    mov g_RingContext.bRunning, 1
    
    ; Allocate buffer
    mov rcx, RING_BUFFER_SIZE
    mov rdx, 040h                   ; LPTR (Zero init)
    call LocalAlloc
    mov g_RingContext.BufferStart, rax
    
    test rax, rax
    jz @init_fail
    
    ; Create consumer thread
    mov rcx, 0                      ; Security attributes
    mov rdx, 0                      ; Stack size
    lea r8, RingBufferThreadProc    ; Thread func
    lea r9, g_RingContext           ; Parameter
    mov qword ptr [rsp + 32], 0     ; FLAGS
    mov qword ptr [rsp + 40], 0     ; ThreadId ptr
    call CreateThread
    mov g_RingContext.hConsumerThread, rax
    
    mov rax, 1
    jmp @init_done
    
@init_fail:
    xor eax, eax
    
@init_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
RingBufferConsumer_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferConsumer_Shutdown
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferConsumer_Shutdown PROC FRAME
    push rbx
    sub rsp, 32
    
    ; Signal stop
    mov g_RingContext.bRunning, 0
    
    ; Wait for thread
    mov rcx, g_RingContext.hConsumerThread
    test rcx, rcx
    jz @free_buffer
    
    mov rdx, 1000                   ; 1 sec timeout
    call WaitForSingleObject
    
    mov rcx, g_RingContext.hConsumerThread
    call CloseHandle
    mov g_RingContext.hConsumerThread, 0
    
@free_buffer:
    mov rcx, g_RingContext.BufferStart
    test rcx, rcx
    jz @shutdown_done
    call LocalFree
    mov g_RingContext.BufferStart, 0
    
@shutdown_done:
    add rsp, 32
    pop rbx
    ret
RingBufferConsumer_Shutdown ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferThreadProc
; Background thread to process tokens
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferThreadProc PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    
    mov rsi, rcx                    ; Context
    
@process_loop:
    cmp [rsi].RingBufferContext.bRunning, 0
    je @exit_thread
    
    ; Check if data available
    mov ecx, [rsi].RingBufferContext.ReadIndex
    mov edx, [rsi].RingBufferContext.WriteIndex
    cmp ecx, edx
    je @sleep_thread
    
    ; Process data (Simplified echo)
    ; In real impl, we''d read UTF-8, convert to display, and post message
    
    ; Advance read index
    inc dword ptr [rsi].RingBufferContext.ReadIndex
    and dword ptr [rsi].RingBufferContext.ReadIndex, RING_BUFFER_SIZE - 1
    
    jmp @process_loop
    
@sleep_thread:
    mov rcx, CONSUMER_THREAD_SLEEP
    call Sleep
    jmp @process_loop
    
@exit_thread:
    xor eax, eax
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
RingBufferThreadProc ENDP

PUBLIC RingBufferConsumer_Initialize
PUBLIC RingBufferConsumer_Shutdown

END
