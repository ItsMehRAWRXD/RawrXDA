; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_RingBuffer_Producer.asm  ─  High-Performance Producer for Ring Buffer
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

EXTERNDEF RawrXD_DMA_RingBuffer_Base:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ConsumerOffset:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ProducerOffset:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Lock:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Semaphore:QWORD

; 64MB Buffer
RING_BUFFER_SIZE        EQU 67108864
RING_BUFFER_MASK        EQU RING_BUFFER_SIZE - 1

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; RingBufferProducer_Write
; Thread-safe write to the ring buffer.
; Parameters:
;   RCX = Pointer to data
;   RDX = Length in bytes
; Returns:
;   RAX = 1 (Success), 0 (Failure/Full)
; ═══════════════════════════════════════════════════════════════════════════════
RingBufferProducer_Write PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .endprolog

    mov rsi, rcx    ; Source data
    mov rdi, rdx    ; Length
    
    ; Validate inputs
    test rsi, rsi
    jz @fail
    test rdi, rdi
    jz @success     ; Length 0 is success

    ; Acquire Spinlock
    mov rbx, RawrXD_DMA_RingBuffer_Lock
@spin_wait:
    call EnterCriticalSection   ; Using CS for robustness as per Shared_Data
    
    ; Check space
    mov r8, RawrXD_DMA_RingBuffer_ConsumerOffset
    mov r9, RawrXD_DMA_RingBuffer_ProducerOffset
    
    mov r10, r9
    sub r10, r8     ; Used = Producer - Consumer
    
    mov r11, RING_BUFFER_SIZE
    sub r11, r10    ; Available = Size - Used
    
    cmp r11, rdi
    jl @full_unlock ; Not enough space
    
    ; We have space.
    ; Write data.
    ; Logical Write Pos = ProducerOffset % SIZE
    ; Handle wrap around split write.
    
    mov rax, r9             ; Current Producer Offset
    and rax, RING_BUFFER_MASK ; Wrapped Index
    
    mov r12, RING_BUFFER_SIZE
    sub r12, rax            ; Space until end of buffer
    
    mov r13, RawrXD_DMA_RingBuffer_Base
    lea r13, [r13 + rax]    ; Dest ptr
    
    cmp rdi, r12
    jg @split_write
    
    ; Single write
    mov rcx, r13
    mov rdx, rsi
    mov r8, rdi
    call RtlCopyMemory
    jmp @update_offset
    
@split_write:
    ; Part 1: Write until end
    mov rcx, r13
    mov rdx, rsi
    mov r8, r12
    call RtlCopyMemory
    
    ; Part 2: Write remainder at start
    mov r13, RawrXD_DMA_RingBuffer_Base ; Base
    
    mov rdx, rsi
    add rdx, r12            ; Source + Part1
    
    mov r8, rdi
    sub r8, r12             ; Remainder
    
    mov rcx, r13            ; Dest
    call RtlCopyMemory

@update_offset:
    ; Update Producer Offset
    add RawrXD_DMA_RingBuffer_ProducerOffset, rdi
    
    ; Signal Semaphore
    mov rcx, RawrXD_DMA_RingBuffer_Semaphore
    mov edx, 1          ; Release 1 count
    xor r8, r8          ; Prev count
    call ReleaseSemaphore

    ; Release Lock
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call LeaveCriticalSection
    
@success:
    mov rax, 1
    jmp @exit
    
@full_unlock:
    mov rcx, RawrXD_DMA_RingBuffer_Lock
    call LeaveCriticalSection
    xor rax, rax            ; Fail
    jmp @exit

@fail:
    xor rax, rax

@exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RingBufferProducer_Write ENDP

END
