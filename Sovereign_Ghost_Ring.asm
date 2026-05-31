; ==============================================================================
; Sovereign Ghost Ring - Lock-Free SPSC Ring Buffer
; ==============================================================================
; Zero-latency communication between AI inference thread (producer) and
; UI render thread (consumer). Uses atomic memory barriers without kernel
; transitions. Designed for 8,259+ TPS throughput.
;
; Architecture:
;   - Single-Producer/Single-Consumer (SPSC)
;   - Power-of-2 masking (no modulo operator)
;   - sfence/lfence memory barriers
;   - Cache-line alignment (64 bytes) for head/tail pointers
;   - 1024-entry default capacity
;
; Exports:
;   RING_INIT           - Initialize ring buffer
;   PUSH_PREDICTION     - Producer: AI thread pushes token
;   POP_PREDICTION      - Consumer: UI thread reads token
;   GET_RING_STATS      - Query occupancy and throughput
;   RING_RESET          - Clear buffer state
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
RING_SIZE       equ 1024
RING_MASK       equ 1023
GHOST_DATA_SIZE equ 32        ; Must match GHOST_DATA structure

; ==============================================================================
; Ghost Data Structure (32 bytes - cache line friendly)
; ==============================================================================
GHOST_DATA struc
    BufferPtr   dq ?        ; String content pointer
    Length      dq ?        ; Length of ghost text
    Timestamp   dq ?        ; Arrival time (rdtsc cycles)
    Confidence  dd ?        ; AI model confidence score (0.0-1.0)
    Reserved    dd ?        ; Padding
GHOST_DATA ends

; ==============================================================================
; Ring Buffer State (Cache-line aligned)
; ==============================================================================
.data
align 64
; Producer state (AI thread writes here)
g_WriteHead     dq 0
align 64
; Consumer state (UI thread writes here)
g_ReadHead      dq 0
align 64
; Buffer storage
g_RingBuffer    GHOST_DATA RING_SIZE dup(<>)

; Statistics
g_TotalPushed   dq 0
g_TotalPopped   dq 0
g_DroppedCount  dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; RING_INIT - Initialize ring buffer state
; ==============================================================================
; Input:  None
; Output: RAX = 1 always
; ==============================================================================
RING_INIT proc
    mov qword ptr [g_WriteHead], 0
    mov qword ptr [g_ReadHead], 0
    mov qword ptr [g_TotalPushed], 0
    mov qword ptr [g_TotalPopped], 0
    mov qword ptr [g_DroppedCount], 0
    
    ; Clear buffer memory
    lea rcx, g_RingBuffer
    mov rdx, RING_SIZE * GHOST_DATA_SIZE
    xor r8, r8
    
clear_loop:
    test rdx, rdx
    jz clear_done
    mov byte ptr [rcx], r8b
    inc rcx
    dec rdx
    jmp clear_loop
    
clear_done:
    mov eax, 1
    ret
RING_INIT endp

; ==============================================================================
; PUSH_PREDICTION - Producer: AI thread pushes ghost text
; ==============================================================================
; Input:  RCX = Pointer to GHOST_DATA to push
; Output: RAX = 1 on success, 0 on failure (buffer full)
; Clobbers: RAX, RDX, R8, R9, R10
; ==============================================================================
PUSH_PREDICTION proc
    ; Load current write head
    mov rax, [g_WriteHead]
    
    ; Calculate next position (power-of-2 masking)
    lea r8, [rax + 1]
    and r8, RING_MASK
    
    ; Check if buffer is full: (WriteHead + 1) == ReadHead
    cmp r8, [g_ReadHead]
    je push_full
    
    ; Calculate buffer slot address
    imul r9, rax, GHOST_DATA_SIZE
    lea r10, [g_RingBuffer + r9]
    
    ; Copy GHOST_DATA structure (32 bytes)
    mov rdx, [rcx + 0]          ; BufferPtr
    mov [r10 + 0], rdx
    mov rdx, [rcx + 8]          ; Length
    mov [r10 + 8], rdx
    mov rdx, [rcx + 16]         ; Timestamp
    mov [r10 + 16], rdx
    mov edx, [rcx + 24]         ; Confidence
    mov [r10 + 24], edx
    
    ; Memory barrier: ensure data is written before updating head
    sfence
    
    ; Update write head
    mov [g_WriteHead], r8
    
    ; Increment statistics
    inc qword ptr [g_TotalPushed]
    
    mov eax, 1
    ret
    
push_full:
    ; Increment drop counter
    inc qword ptr [g_DroppedCount]
    xor eax, eax
    ret
PUSH_PREDICTION endp

; ==============================================================================
; POP_PREDICTION - Consumer: UI thread reads ghost text
; ==============================================================================
; Input:  RCX = Pointer to output GHOST_DATA buffer
; Output: RAX = 1 on success, 0 on failure (buffer empty)
; Clobbers: RAX, RDX, R8, R9, R10
; ==============================================================================
POP_PREDICTION proc
    ; Load current read head
    mov rax, [g_ReadHead]
    
    ; Check if buffer is empty: ReadHead == WriteHead
    cmp rax, [g_WriteHead]
    je pop_empty
    
    ; Calculate buffer slot address
    imul r9, rax, GHOST_DATA_SIZE
    lea r10, [g_RingBuffer + r9]
    
    ; Copy GHOST_DATA structure (32 bytes)
    mov rdx, [r10 + 0]          ; BufferPtr
    mov [rcx + 0], rdx
    mov rdx, [r10 + 8]          ; Length
    mov [rcx + 8], rdx
    mov rdx, [r10 + 16]         ; Timestamp
    mov [rcx + 16], rdx
    mov edx, [r10 + 24]         ; Confidence
    mov [rcx + 24], edx
    
    ; Memory barrier: ensure data is read before updating head
    lfence
    
    ; Calculate next position
    lea r8, [rax + 1]
    and r8, RING_MASK
    
    ; Update read head
    mov [g_ReadHead], r8
    
    ; Increment statistics
    inc qword ptr [g_TotalPopped]
    
    mov eax, 1
    ret
    
pop_empty:
    xor eax, eax
    ret
POP_PREDICTION endp

; ==============================================================================
; GET_RING_STATS - Query ring buffer statistics
; ==============================================================================
; Input:  RCX = Pointer to output buffer (32 bytes)
;         [RCX+0]  = Total pushed
;         [RCX+8]  = Total popped
;         [RCX+16] = Dropped count
;         [RCX+24] = Current occupancy
; Output: RAX = 1 always
; ==============================================================================
GET_RING_STATS proc
    mov rax, [g_TotalPushed]
    mov [rcx + 0], rax
    mov rax, [g_TotalPopped]
    mov [rcx + 8], rax
    mov rax, [g_DroppedCount]
    mov [rcx + 16], rax
    
    ; Calculate occupancy
    mov rax, [g_WriteHead]
    sub rax, [g_ReadHead]
    and rax, RING_MASK
    mov [rcx + 24], rax
    
    mov eax, 1
    ret
GET_RING_STATS endp

; ==============================================================================
; RING_RESET - Clear ring buffer state
; ==============================================================================
; Input:  None
; Output: RAX = 1 always
; ==============================================================================
RING_RESET proc
    mov qword ptr [g_WriteHead], 0
    mov qword ptr [g_ReadHead], 0
    mov qword ptr [g_TotalPushed], 0
    mov qword ptr [g_TotalPopped], 0
    mov qword ptr [g_DroppedCount], 0
    mov eax, 1
    ret
RING_RESET endp

; ==============================================================================
; GET_OCCUPANCY - Quick occupancy check
; ==============================================================================
; Input:  None
; Output: RAX = Current occupancy (0 to RING_SIZE-1)
; ==============================================================================
GET_OCCUPANCY proc
    mov rax, [g_WriteHead]
    sub rax, [g_ReadHead]
    and rax, RING_MASK
    ret
GET_OCCUPANCY endp

; ==============================================================================
; IS_RING_EMPTY - Check if buffer is empty
; ==============================================================================
; Input:  None
; Output: RAX = 1 if empty, 0 if not empty
; ==============================================================================
IS_RING_EMPTY proc
    mov rax, [g_WriteHead]
    cmp rax, [g_ReadHead]
    je ring_empty
    xor eax, eax
    ret
ring_empty:
    mov eax, 1
    ret
IS_RING_EMPTY endp

; ==============================================================================
; IS_RING_FULL - Check if buffer is full
; ==============================================================================
; Input:  None
; Output: RAX = 1 if full, 0 if not full
; ==============================================================================
IS_RING_FULL proc
    mov rax, [g_WriteHead]
    inc rax
    and rax, RING_MASK
    cmp rax, [g_ReadHead]
    je ring_full
    xor eax, eax
    ret
ring_full:
    mov eax, 1
    ret
IS_RING_FULL endp

end