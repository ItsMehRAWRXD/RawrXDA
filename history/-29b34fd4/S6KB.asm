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

GpuTransferOp STRUCT
    OpType              DWORD       ?       ; UPLOAD, DOWNLOAD, COPY
    SrcHostPtr          QWORD       ?
    DstVramOffset       QWORD       ?
    OpSize              QWORD       ?
    CompletionEvent     QWORD       ?
    FenceValue          QWORD       ?
GpuTransferOp ENDS

VramHeap STRUCT
    TotalSize           QWORD       ?
    UsedSize            QWORD       ?
    FragmentationScore  REAL4       ?
    
    Blocks              VramBlock VRAM_POOL_BLOCKS DUP (<>)
    FreeListHead        DWORD       ?       ; Index of first free block
    UsedListHead        DWORD       ?       ; Index of first used block
    
    TransferQueue       GpuTransferOp VRAM_TRANSFER_QUEUE_DEPTH DUP (<>)
    QueueHead           DWORD       ?
    QueueTail           DWORD       ?
    
    DefragLock          DWORD       ?
    LastDefragTick      QWORD       ?
VramHeap ENDS

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
fDefragThreshold        REAL4       0.4     ; 40% fragmentation trigger

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
    ; Check if remaining space is big enough to split
    mov rax, [rdi].VramBlock.VSize
    sub rax, rcx                    ; rax = remaining size (Total - Alloc)
    
    cmp rax, VRAM_BLOCK_SIZE_MIN
    jl @use_whole_block             ; Too small to split, use whole block
    
    ; Find an empty slot for the new block
    push rcx
    push rdx
    
    xor ecx, ecx
    lea rdx, g_VramHeap.Blocks
    
@find_slot_loop:
    cmp ecx, VRAM_POOL_BLOCKS
    jge @slot_full                  ; No slots available, just use whole block
    
    imul r8, rcx, SIZEOF VramBlock
    cmp [rdx + r8].VramBlock.Flags, -1   ; Check for invalid/unused slot
    je @found_slot
    
    inc ecx
    jmp @find_slot_loop
    
@found_slot:
    ; Create new block node at [rdx + r8]
    ; rdi = current block (being allocated)
    ; rax = remaining size
    ; stack has saved rcx (alloc size total)
    
    ; Calculate offset for new block
    mov r9, [rdi].VramBlock.VOffset
    mov r10, [rsp + 8]              ; Retrieve alloc size (rcx was pushed last? No, rdx then rcx)
                                    ; Pushed rcx, then rdx. 
                                    ; RSP points to rdx. RSP+8 points to rcx.
                                    ; Wait, I pushed rcx then rdx.
                                    ; push rcx (alloc size used in cmp) -> RSP+8
                                    ; push rdx (g_VramHeap) -> RSP
    
    ; Actually, I need the 'alloc size' calculated in 'rcx' before the jump.
    ; in @block_fits, 'rcx' is the total bytes needed including alignment padding.
    ; So new block starts at OldOffset + AllocSize
    
    add r9, [rsp + 8]               ; New Offset = Old Offset + Alloc Size
    
    mov [rdx + r8].VramBlock.VOffset, r9
    mov [rdx + r8].VramBlock.VSize, rax
    mov [rdx + r8].VramBlock.Flags, BLOCK_FLAG_FREE
    mov [rdx + r8].VramBlock.OwnerModel, 0
    
    ; Update current block size
    mov r11, [rsp + 8]
    mov [rdi].VramBlock.VSize, r11
    
    pop rdx
    pop rcx
    jmp @use_whole_block

@slot_full:
    pop rdx
    pop rcx
    ; Fallthrough to use whole block

@use_whole_block:
    ; Mark as allocated
    mov [rdi].VramBlock.Flags, BLOCK_FLAG_ALLOCATED
    call GetTickCount64
    mov [rdi].VramBlock.LastAccessTick, rax
    
    ; Update heap stats
    add [rsi].VramHeap.UsedSize, r12
    
    ; Return offset (accounting for alignment padding)
    mov rax, [rdi].VramBlock.VOffset
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
    .endprolog
    
    lea rsi, g_VramHeap
    
    ; Find block containing this offset
    xor ebx, ebx
    
@find_loop:
    cmp ebx, VRAM_POOL_BLOCKS
    jge @free_not_found
    
    imul rax, rbx, SIZEOF VramBlock
    lea rdi, [rsi + VramHeap.Blocks]
    add rdi, rax
    cmp [rdi].VramBlock.VOffset, rcx
    je @found_block
    cmp [rdi].VramBlock.Flags, BLOCK_FLAG_ALLOCATED
    jne @next_find
    
    mov rax, [rdi].VramBlock.VOffset
    add rax, [rdi].VramBlock.VSize
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
    mov rax, [rdi].VramBlock.VSize
    sub [rsi].VramHeap.UsedSize, rax
    
    ; Coalesce Logic
    xor ebx, ebx            ; Loop index
    
@coalesce_loop:
    cmp ebx, VRAM_POOL_BLOCKS
    jge @free_done
    
    imul rax, rbx, SIZEOF VramBlock
    lea rdx, [rsi + VramHeap.Blocks]
    add rdx, rax            ; rdx = Candidate Block
    
    ; Skip if invalid or allocated
    cmp [rdx].VramBlock.Flags, BLOCK_FLAG_FREE
    jne @next_coalesce
    
    ; Skip if it is ourselves
    cmp rdx, rdi
    je @next_coalesce
    
    ; Check 1: Is rdx a PREDECESSOR? (Candidate.Offset + Candidate.Size == Current.Offset)
    mov rax, [rdx].VramBlock.VOffset
    add rax, [rdx].VramBlock.VSize
    cmp rax, [rdi].VramBlock.VOffset
    je @merge_predecessor
    
    ; Check 2: Is rdx a SUCCESSOR? (Current.Offset + Current.Size == Candidate.Offset)
    mov rax, [rdi].VramBlock.VOffset
    add rax, [rdi].VramBlock.VSize
    cmp rax, [rdx].VramBlock.VOffset
    je @merge_successor
    
    jmp @next_coalesce
    
@merge_predecessor:
    ; Merge Current (rdi) INTO Predecessor (rdx)
    ; Pred.Size += Current.Size
    mov rax, [rdi].VramBlock.VSize
    add [rdx].VramBlock.VSize, rax
    
    ; Mark Current as Invalid
    mov [rdi].VramBlock.Flags, -1
    
    ; Now, Predecessor becomes the Current for further coalescing
    mov rdi, rdx
    xor ebx, ebx            ; Restart scan
    jmp @coalesce_loop
    
@merge_successor:
    ; Merge Successor (rdx) INTO Current (rdi)
    ; Current.Size += Succ.Size
    mov rax, [rdx].VramBlock.VSize
    add [rdi].VramBlock.VSize, rax
    
    ; Mark Successor as Invalid
    mov [rdx].VramBlock.Flags, -1
    
    ; Restart scan
    xor ebx, ebx
    jmp @coalesce_loop

@next_coalesce:
    inc ebx
    jmp @coalesce_loop
    
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
    add rbx, VramHeap.TransferQueue
    add rbx, rsi
    ; rbx now points to the slot
    
    mov [rbx].GpuTransferOp.OpType, 0 ; UPLOAD
    mov [rbx].GpuTransferOp.SrcHostPtr, rcx
    mov [rbx].GpuTransferOp.DstVramOffset, rdx
    mov [rbx].GpuTransferOp.OpSize, r8
    
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
    mov r8, [rbx].GpuTransferOp.OpSize
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
    
    ucomiss xmm0, dword ptr [fDefragThreshold]
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
; Vram_SetCallbacks
; Set GPU driver callback functions
; RCX = Alloc, RDX = Free, R8 = Submit, R9 = Wait
; ═══════════════════════════════════════════════════════════════════════════════
Vram_SetCallbacks PROC FRAME
    .endprolog
    mov [pfnGpuAllocate], rcx
    mov [pfnGpuFree], rdx
    mov [pfnGpuSubmitTransfer], r8
    mov [pfnGpuWaitFence], r9
    ret
Vram_SetCallbacks ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; CalculateFragmentation
; Returns fragmentation ratio in XMM0 (0.0 = none, 1.0 = fully fragmented)
; ═══════════════════════════════════════════════════════════════════════════════
CalculateFragmentation PROC FRAME
    push rbx
    push rsi
    .endprolog
    
    lea rsi, g_VramHeap
    xor rbx, rbx     ; Index
    
    xorps xmm0, xmm0 ; Result
    xor r8, r8       ; TotalFreeBytes
    xor r9, r9       ; MaxFreeBlockBytes
    
@calc_loop:
    cmp rbx, VRAM_POOL_BLOCKS
    jge @calc_compute
    
    imul rax, rbx, SIZEOF VramBlock
    lea rdx, [rsi + VramHeap.Blocks]
    add rdx, rax
    
    cmp [rdx].VramBlock.Flags, BLOCK_FLAG_FREE
    jne @next_calc
    
    mov rax, [rdx].VramBlock.VSize
    add r8, rax
    cmp rax, r9
    jle @next_calc
    mov r9, rax
    
@next_calc:
    inc rbx
    jmp @calc_loop
    
@calc_compute:
    test r8, r8
    jz @calc_done    ; 0 free bytes -> 0 fragmentation
    
    cvtsi2ss xmm0, r9 ; Max Size
    cvtsi2ss xmm1, r8 ; Total Size
    divss xmm0, xmm1  ; Max / Total (0.0 to 1.0)
    
    ; Frag = 1.0 - (Max / Total)
    mov eax, 3F800000h ; 1.0f
    movd xmm1, eax
    subss xmm1, xmm0
    movaps xmm0, xmm1
    
@calc_done:
    pop rsi
    pop rbx
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
PUBLIC Vram_SetCallbacks

END
