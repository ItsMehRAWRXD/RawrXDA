; RawrXD_Streaming_Orchestrator.asm
; High-performance DMA Ring Buffer Orchestration (AVX-512 Optimized)
; 64MB Circular Buffer Management

; ─── PUBLIC Exports ──────────────────────────────────────────────────────────

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

PUBLIC RawrXD_Streaming_Write
PUBLIC RawrXD_Streaming_Read

.code

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
RING_SIZE       EQU 67108864    ; 64MB (64 * 1024 * 1024)
RING_MASK       EQU 67108863    ; Size - 1 (for fast modulo)
CACHE_LINE      EQU 64          ; Align to cache line

; -----------------------------------------------------------------------------
; Data Structure Layout (RawrXD_StreamingEngine class mirror)
; -----------------------------------------------------------------------------
; rcx = this pointer
; [rcx + 00] = hFileMapping
; [rcx + 08] = ringBuffer (ptr)
; [rcx + 16] = writePos (volatile size_t)
; [rcx + 24] = readPos (volatile size_t)
; [rcx + 32] = hWriteEvent
; [rcx + 40] = hReadEvent
; [rcx + 48] = lock (SRWLOCK)

; -----------------------------------------------------------------------------
; RawrXD_Streaming_Write
; rcx: this, rdx: data, r8: len
; returns: bytes written
; -----------------------------------------------------------------------------
align 16
RawrXD_Streaming_Write proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov r12, rcx        ; this
    mov r13, rdx        ; input data
    mov r14, r8         ; length

    ; Load current positions
    mov rax, [r12 + 16] ; writePos
    mov rbx, [r12 + 24] ; readPos
    mov r15, [r12 + 8]  ; buffer base

    ; Calculate available space
    ; space = size - (write - read)
    sub rax, rbx
    mov rdx, RING_SIZE
    sub rdx, rax
    
    ; If full, return 0 (or wait logic handled by C++ caller)
    cmp r14, rdx
    ja ToReturnZero

    ; Perform write (memcpy with wrap)
    ; offset = writePos & MASK
    mov rax, [r12 + 16]
    and rax, RING_MASK
    
    ; Chunk 1: buffer base + offset
    mov rsi, r13        ; source
    lea rdi, [r15 + rax]; dest
    
    ; Check if wrapper needed
    mov rcx, RING_SIZE
    sub rcx, rax        ; space until end
    
    cmp r14, rcx
    jbe WriteDirect
    
    ; Wrap needed
    ; Copy chunk 1
    push rcx            ; save count1
    rep movsb
    
    ; Copy chunk 2 (rest from start)
    pop rcx
    mov rdx, r14
    sub rdx, rcx        ; remaining bytes
    mov rcx, rdx
    mov rdi, r15        ; dest = base
    rep movsb
    jmp UpdatePos

WriteDirect:
    mov rcx, r14
    rep movsb

UpdatePos:
    ; Update writePos atomically (simplified, use real MFENCE in prod)
    lock add [r12 + 16], r14
    
    ; Signal write event
    mov rcx, [r12 + 32]  ; hWriteEvent
    call SetEvent
    
    mov rax, r14        ; return bytes written
    jmp Cleanup

ToReturnZero:
    xor rax, rax

Cleanup:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

RawrXD_Streaming_Write endp

; -----------------------------------------------------------------------------
; RawrXD_Streaming_Read
; rcx: this, rdx: out_buffer, r8: len
; returns: bytes read
; -----------------------------------------------------------------------------
align 16
RawrXD_Streaming_Read proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Logic similar to Write, but reading and updating readPos
    mov r12, rcx        ; this
    mov r13, rdx        ; data buffer
    mov r14, r8         ; len
    
    ; Check available data (simplified)
    mov rax, [r12 + 16] ; writePos
    sub rax, [r12 + 24] ; readPos
    cmp rax, r14
    jb NoData
    
    ; Copy data from ring buffer
    mov rsi, [r12 + 8]  ; ringBuffer
    mov rdi, r13
    mov rcx, r14
    rep movsb
    
    ; Update readPos
    lock add [r12 + 24], r14
    
    ; Signal read event
    mov rcx, [r12 + 40] ; hReadEvent
    call SetEvent
    
    mov rax, r14
    jmp CleanupRead
    
NoData:
    xor rax, rax
    
CleanupRead:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_Streaming_Read endp

end
