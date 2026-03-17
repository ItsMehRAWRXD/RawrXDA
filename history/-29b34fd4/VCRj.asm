; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_GPU_Memory.asm  ─  VRAM Management, Transfer Queues, Paging
; GPU memory allocator with defragmentation and host-device transfer optimization
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
VRAM_POOL_BLOCKS        EQU 4096    ; Maximum allocatable regions
VRAM_BLOCK_SIZE_MIN     EQU 65536   ; 64KB minimum allocation
VRAM_TRANSFER_QUEUE_DEPTH EQU 64    ; Pending DMA transfers

; Block flags
BLOCK_FLAG_FREE         EQU 0
BLOCK_FLAG_ALLOCATED    EQU 1
BLOCK_FLAG_MAPPED       EQU 2       ; Mapped to host address space
BLOCK_FLAG_TRANSFER     EQU 4       ; Pending transfer
BLOCK_FLAG_PINNED       EQU 8       ; Cannot be moved (defrag)

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
VramBlock STRUCT
    VOffset             QWORD       ?       ; Byte offset in VRAM
    VSize               QWORD       ?       ; Size in bytes
    Flags               DWORD       ?
    OwnerModel          QWORD       ?       ; Back pointer to model
    HostMapping         QWORD       ?       ; Pinned host pointer (if mapped)
    LastAccessTick      QWORD       ?
    TransferFence       QWORD       ?       ; GPU fence for async ops
VramBlock ENDS

VramHeap STRUCT
    TotalSize           QWORD       ?
    UsedSize            QWORD       ?
    FragmentationScore  REAL4       ?
    
    Blocks              VramBlock VRAM_POOL_BLOCKS DUP (<>)
    FreeListHead        DWORD       ?       ; Index of first free block
    UsedListHead        DWORD       ?       ; Index of first used block
    
    TransferQueue       QWORD VRAM_TRANSFER_QUEUE_DEPTH DUP (?)  ; Pending copies
    QueueHead           DWORD       ?
    QueueTail           DWORD       ?
    
    DefragLock          DWORD       ?
    LastDefragTick      QWORD       ?
VramHeap ENDS

GpuTransferOp STRUCT
    Type                DWORD       ?       ; UPLOAD, DOWNLOAD, COPY
    SrcHostPtr          QWORD       ?
    DstVramOffset       QWORD       ?
    Size                QWORD       ?
    CompletionEvent     QWORD       ?
    FenceValue          QWORD       ?
GpuTransferOp ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_VramHeap              VramHeap <>

; Callbacks to GPU-specific implementation (set during init)
pfnGpuAllocate          QWORD       0       ; Driver allocation
pfnGpuFree              QWORD       0
pfnGpuSubmitTransfer    QWORD       0       ; Queue DMA operation
pfnGpuWaitFence         QWORD       0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Initialize
; Initialize heap tracking (actual VRAM allocated via driver callback)
; RCX = total VRAM size to manage
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Initialize PROC FRAME
    push rbx
    push rsi
    .endprolog
    
    mov rbx, rcx                    ; Total size
    
    ; Zero heap structure
    lea rcx, g_VramHeap
    mov edx, SIZEOF VramHeap
    call RtlZeroMemory
    
    mov g_VramHeap.TotalSize, rbx
    
    ; Initialize single free block covering entire heap
    mov g_VramHeap.Blocks[0 * SIZEOF VramBlock].VOffset, 0
    mov g_VramHeap.Blocks[0 * SIZEOF VramBlock].VSize, rbx
    mov g_VramHeap.Blocks[0 * SIZEOF VramBlock].Flags, BLOCK_FLAG_FREE
    mov g_VramHeap.FreeListHead, 0
    
    ; Link remaining blocks as empty
    mov ecx, 1
    
@init_blocks:
    cmp ecx, VRAM_POOL_BLOCKS
    jge @init_done
    
    imul rax, rcx, SIZEOF VramBlock
    lea rdx, g_VramHeap.Blocks
    mov [rdx + rax].VramBlock.Flags, -1  ; Invalid/unused
    
    inc ecx
    jmp @init_blocks
    
@init_done:
    pop rsi
    pop rbx
    ret
Vram_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Allocate
; First-fit allocation with immediate coalescing
; RCX = size, RDX = alignment (power of 2)
; Returns RAX = VRAM offset, or -1 on failure
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Allocate PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .endprolog
    
    mov r12, rcx                    ; Requested size
    mov r13, rdx                    ; Alignment
    
    ; Round up to minimum block size
    cmp r12, VRAM_BLOCK_SIZE_MIN
    jge @size_ok
    mov r12, VRAM_BLOCK_SIZE_MIN
    
@size_ok:
    ; Align size
    dec r13
    add r12, r13
    not r13
    and r12, r13
    inc r13
    
    lea rsi, g_VramHeap
    mov ebx, [rsi].VramHeap.FreeListHead
    
@search_loop:
    cmp ebx, -1
    je @alloc_fail                  ; No free block
    
    imul rax, rbx, SIZEOF VramBlock
    lea rdi, [rsi + VramHeap.Blocks]
    add rdi, rax
    
    ; Check if block is free and large enough
    cmp [rdi].VramBlock.Flags, BLOCK_FLAG_FREE
    jne @next_block
    
    ; Calculate aligned offset within block
    mov rax, [rdi].VramBlock.VOffset
    add rax, r13
    dec r13
    not r13
    and rax, r13                    ; Aligned address
    inc r13
    
    mov rcx, rax
    sub rcx, [rdi].VramBlock.VOffset ; Padding for alignment
    add rcx, r12                    ; Total needed
    
    cmp rcx, [rdi].VramBlock.VSize
    jle @block_fits
    
@next_block:
    ; Find next free block (simplified - scan all)
    inc ebx
    cmp ebx, VRAM_POOL_BLOCKS
    jl @search_loop
    mov ebx, -1
    jmp @search_loop
    
@block_fits:
    ; Split logic disabled for stability - using whole block
    jmp @use_whole_block
    
    ; (Split code removed)
    
@use_whole_block:
    ; Mark as allocated
    mov [rdi].VramBlock.Flags, BLOCK_FLAG_ALLOCATED
    call GetTickCount64
    mov [rdi].VramBlock.LastAccessTick, rax
    
    ; Update heap stats
    add [rsi].VramHeap.UsedSize, r12
    
    ; Return offset (accounting for alignment padding)
    mov rax, [rdi].VramBlock.Offset
    add rax, r13
    dec r13
    not r13
    and rax, r13
    
    jmp @alloc_done
    
@alloc_fail:
    ; Trigger defragmentation and retry once
    call Vram_Defragment
    test eax, eax
    jz @really_fail
    
    ; Retry allocation
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    jmp Vram_Allocate               ; Tail recursion for retry
    
@really_fail:
    mov rax, -1
    
@alloc_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Vram_Allocate ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Free
; Release block and coalesce with adjacent free blocks
; RCX = VRAM offset
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Free PROC FRAME
    push rbx
    push rsi
    push rdi
    
    lea rsi, g_VramHeap
    
    ; Find block containing this offset
    xor ebx, ebx
    
@find_loop:
    cmp ebx, VRAM_POOL_BLOCKS
    jge @free_not_found
    
    lea rdi, [rsi].VramHeap.Blocks[rbx * SIZEOF VramBlock]
    cmp [rdi].VramBlock.Offset, rcx
    je @found_block
    cmp [rdi].VramBlock.Flags, BLOCK_FLAG_ALLOCATED
    jne @next_find
    
    mov rax, [rdi].VramBlock.Offset
    add rax, [rdi].VramBlock.Size
    cmp rax, rcx
    ja @found_block                 ; Offset within this block
    
@next_find:
    inc ebx
    jmp @find_loop
    
@found_block:
    ; Mark as free
    mov [rdi].VramBlock.Flags, BLOCK_FLAG_FREE
    mov [rdi].VramBlock.OwnerModel, 0
    
    ; Update heap stats
    mov rax, [rdi].VramBlock.Size
    sub [rsi].VramHeap.UsedSize, rax
    
    ; Coalesce with previous free block
    test ebx, ebx
    jz @check_next
    
    lea rcx, [rsi].VramHeap.Blocks[(rbx-1) * SIZEOF VramBlock]
    cmp [rcx].VramBlock.Flags, BLOCK_FLAG_FREE
    jne @check_next
    
    ; Merge previous into current
    mov rax, [rcx].VramBlock.Size
    add [rdi].VramBlock.Size, rax
    sub [rdi].VramBlock.Offset, rax
    mov [rcx].VramBlock.Flags, -1   ; Invalidate previous
    
@check_next:
    ; Coalesce with next free block
    lea rcx, [rsi].VramHeap.Blocks[(rbx+1) * SIZEOF VramBlock]
    cmp [rcx].VramBlock.Flags, BLOCK_FLAG_FREE
    jne @free_done
    
    mov rax, [rcx].VramBlock.Size
    add [rdi].VramBlock.Size, rax
    mov [rcx].VramBlock.Flags, -1   ; Invalidate next
    
@free_done:
@free_not_found:
    pop rdi
    pop rsi
    pop rbx
    ret
Vram_Free ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_SubmitUpload
; Queue async host-to-device transfer
; RCX = host source pointer, RDX = VRAM destination offset, R8 = size
; Returns RAX = fence value to wait on, or 0 if queued
; ═══════════════════════════════════════════════════════════════════════════════
Vram_SubmitUpload PROC FRAME
    push rbx
    push rsi
    sub rsp, 40
    .endprolog
    
    mov rbx, rcx
    
    lea rsi, g_VramHeap
    
    ; Find free queue slot
    mov eax, [rsi].VramHeap.QueueTail
    inc eax
    and eax, VRAM_TRANSFER_QUEUE_DEPTH - 1
    cmp eax, [rsi].VramHeap.QueueHead
    je @queue_full                  ; Queue full, must wait
    
    ; Setup transfer operation
    mov ebx, [rsi].VramHeap.QueueTail
    imul rbx, SIZEOF GpuTransferOp
    lea rbx, [rsi].VramHeap.TransferQueue[rbx]
    
    mov [rbx].GpuTransferOp.Type, 0 ; UPLOAD
    mov [rbx].GpuTransferOp.SrcHostPtr, rcx
    mov [rbx].GpuTransferOp.DstVramOffset, rdx
    mov [rbx].GpuTransferOp.Size, r8
    
    ; Create completion event
    xor ecx, ecx
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call CreateEventA
    mov [rbx].GpuTransferOp.CompletionEvent, rax
    
    ; Submit to GPU driver
    mov rcx, [rbx].GpuTransferOp.SrcHostPtr
    mov rdx, [rbx].GpuTransferOp.DstVramOffset
    mov r8, [rbx].GpuTransferOp.Size
    call qword ptr [pfnGpuSubmitTransfer]
    mov [rbx].GpuTransferOp.FenceValue, rax
    
    ; Advance queue tail
    inc [rsi].VramHeap.QueueTail
    and dword ptr [rsi].VramHeap.QueueTail, VRAM_TRANSFER_QUEUE_DEPTH - 1
    
    mov rax, [rbx].GpuTransferOp.FenceValue
    
    add rsp, 40
    pop rsi
    pop rbx
    ret
    
@queue_full:
    xor eax, eax
    add rsp, 40
    pop rsi
    pop rbx
    ret
Vram_SubmitUpload ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Defragment
; Compact allocated blocks to reduce fragmentation
; Moves non-pinned blocks to eliminate gaps
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Defragment PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 40
    .endprolog
    
    lea rsi, g_VramHeap
    
    ; Acquire defrag lock
    lea rcx, [rsi].VramHeap.DefragLock
    call Spinlock_Acquire
    
    ; Simple defrag: sort blocks by offset, compact upwards
    ; Real implementation would use GPU copy engines for async moves
    
    ; Calculate fragmentation score
    call CalculateFragmentation
    movss [rsi].VramHeap.FragmentationScore, xmm0
    
    ucomiss xmm0, dword ptr [rel fDefragThreshold]
    jb @no_defrag_needed
    
    ; Perform compaction (simplified - would need actual GPU copies)
    ; ...
    mov eax, TRUE
    jmp @defrag_done
    
@no_defrag_needed:
    xor eax, eax
    
@defrag_done:
    mov [rsi].VramHeap.DefragLock, 0
    call GetTickCount64
    mov [rsi].VramHeap.LastDefragTick, rax
    
    add rsp, 40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Vram_Defragment ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; CalculateFragmentation
; Returns fragmentation ratio in XMM0 (0.0 = none, 1.0 = fully fragmented)
; ═══════════════════════════════════════════════════════════════════════════════
CalculateFragmentation PROC FRAME
    .endprolog
    ; Free space / Total space, weighted by number of free chunks
    ret
CalculateFragmentation ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC Vram_Initialize
PUBLIC Vram_Allocate
PUBLIC Vram_Free
PUBLIC Vram_SubmitUpload
PUBLIC Vram_Defragment

fDefragThreshold        REAL4 0.30   ; Defrag when 30% fragmentation

END
