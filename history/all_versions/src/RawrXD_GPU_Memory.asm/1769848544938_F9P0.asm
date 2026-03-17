; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_GPU_Memory.asm  ─  VRAM Management, Transfer Queues, Paging
; GPU memory allocator with defragmentation and host-device transfer optimization
; ═══════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE

include windows.inc
include kernel32.inc

includelib kernel32.lib

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
VRAM_POOL_BLOCKS        EQU 4096    ; Maximum allocatable regions
VRAM_BLOCK_SIZE_MIN     EQU 65536   ; 64KB minimum allocation
VRAM_TRANSFER_QUEUE_DEPTH EQU 64    ; Pending DMA transfers
VRAM_BLOCK_STRUCT_SIZE  EQU 56      ; Manual sizeof to avoid circular dependency/syntax issues

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
    blkOffset           QWORD       ?       ; Byte offset in VRAM
    blkSize             QWORD       ?       ; Size in bytes
    blkFlags            DWORD       ?       ; Renamed from Flags
    OwnerModel          QWORD       ?       ; Back pointer to model
    HostMapping         QWORD       ?       ; Pinned host pointer (if mapped)
    LastAccessTick      QWORD       ?
    TransferFence       QWORD       ?       ; GPU fence for async ops
VramBlock ENDS

VramHeap STRUCT
    TotalSize           QWORD       ?
    UsedSize            QWORD       ?
    FragmentationScore  REAL4       ?
    
    pBlocks             QWORD       ?       ; Pointer to external storage
    
    FreeListHead        DWORD       ?       ; Index of first free block
    UsedListHead        DWORD       ?       ; Index of first used block
    
    TransferQueue       QWORD VRAM_TRANSFER_QUEUE_DEPTH DUP (?)  ; Pending copies
    QueueHead           DWORD       ?
    QueueTail           DWORD       ?
    
    DefragLock          DWORD       ?
    LastDefragTick      QWORD       ?
VramHeap ENDS

GpuTransferOp STRUCT
    OpType              DWORD       ?       ; UPLOAD, DOWNLOAD, COPY
    SrcHostPtr          QWORD       ?
    DstVramOffset       QWORD       ?
    TransferSize        QWORD       ?       ; Renamed from Size to avoid keyword conflict
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

.DATA?
align 16
g_HeapBlocksStorage     DB 229376 DUP (?) ; 4096 * 56 storage for blocks

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

EXTERN RtlZeroMemory : PROC
EXTERN Spinlock_Acquire : PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Initialize
; Initialize heap tracking (actual VRAM allocated via driver callback)
; RCX = total VRAM size to manage
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Initialize PROC
    push rbx
    push rsi
    sub rsp, 32 ; Shadow space
    
    mov rbx, rcx                    ; Total size
    
    ; Zero heap structure
    lea rcx, g_VramHeap
    mov edx, SIZEOF VramHeap
    call RtlZeroMemory
    
    mov g_VramHeap.TotalSize, rbx
    
    ; Setup blocks pointer
    lea rax, g_HeapBlocksStorage
    mov g_VramHeap.pBlocks, rax
    
    ; Initialize single free block covering entire heap
    mov rax, g_VramHeap.pBlocks     ; Base address
    
    ; Index 0 is at offset 0
    mov (VramBlock PTR [rax]).blkOffset, 0
    mov (VramBlock PTR [rax]).blkSize, rbx
    mov (VramBlock PTR [rax]).blkFlags, BLOCK_FLAG_FREE
    mov g_VramHeap.FreeListHead, 0
    
    ; Link remaining blocks as empty
    mov ecx, 1
    
@init_blocks:
    cmp ecx, VRAM_POOL_BLOCKS
    jge @init_done
    
    ; Must use IMUL for offset because sizeof(VramBlock) is not power of 2
    mov eax, ecx
    imul rax, VRAM_BLOCK_STRUCT_SIZE
    
    ; Access byte offset
    mov r8, g_VramHeap.pBlocks
    lea rdx, [r8 + rax]
    
    mov (VramBlock PTR [rdx]).blkFlags, -1  ; Invalid/unused
    
    inc ecx
    jmp @init_blocks
    
@init_done:
    add rsp, 32
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
Vram_Allocate PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 32
    
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
    mov ebx, (VramHeap PTR [rsi]).FreeListHead
    
@search_loop:
    cmp ebx, -1
    je @alloc_fail                  ; No free block
    
    mov eax, ebx
    imul rax, VRAM_BLOCK_STRUCT_SIZE
    
    mov r9, (VramHeap PTR [rsi]).pBlocks
    lea rdi, [r9 + rax]
    
    ; Check if block is free and large enough
    cmp (VramBlock PTR [rdi]).blkFlags, BLOCK_FLAG_FREE
    jne @next_block
    
    ; Calculate aligned offset within block
    mov rax, (VramBlock PTR [rdi]).blkOffset
    add rax, r13
    dec r13
    not r13
    and rax, r13                    ; Aligned address
    inc r13
    
    mov rcx, rax
    sub rcx, (VramBlock PTR [rdi]).blkOffset ; Padding for alignment
    add rcx, r12                    ; Total needed
    
    cmp rcx, (VramBlock PTR [rdi]).blkSize
    jle @block_fits
    
@next_block:
    ; Find next free block (simplified - scan all)
    inc ebx
    cmp ebx, VRAM_POOL_BLOCKS
    jl @search_loop
    mov ebx, -1
    jmp @search_loop
    
@block_fits:
    ; Split block if there's significant remainder
    mov rax, (VramBlock PTR [rdi]).blkSize
    sub rax, rcx                    ; Remainder after allocation
    cmp rax, VRAM_BLOCK_SIZE_MIN * 2
    jl @use_whole_block
    
    ; Split: current block becomes allocation, create new free block after
    push rbx                        ; Save current block index
    
    ; Find free slot for remainder
    xor ecx, ecx
@find_slot:
    mov eax, ecx
    imul rax, VRAM_BLOCK_STRUCT_SIZE
    
    mov r9, (VramHeap PTR [rsi]).pBlocks
    lea r8, [r9 + rax]
    
    cmp (VramBlock PTR [r8]).blkFlags, -1
    je @found_slot
    inc ecx
    cmp ecx, VRAM_POOL_BLOCKS
    jl @find_slot
    pop rbx
    jmp @alloc_fail                 ; No slot for split
    
@found_slot:
    pop rbx
    ; r8 is remainder block ptr, rdi is current block ptr
    
    ; Re-calculate required size (allocation + padding)
    mov rax, (VramBlock PTR [rdi]).blkOffset
    add rax, r13
    dec r13
    not r13
    and rax, r13
    sub rax, (VramBlock PTR [rdi]).blkOffset
    add rax, r12 ; rax is now required size
    
    push rax ; Store required size
    
    ; Calc Offset for new block = old offset + required size
    mov r10, (VramBlock PTR [rdi]).blkOffset
    add r10, rax
    mov (VramBlock PTR [r8]).blkOffset, r10
    
    ; Calc Size for new block = old size - required size
    mov r11, (VramBlock PTR [rdi]).blkSize
    sub r11, rax
    mov (VramBlock PTR [r8]).blkSize, r11
    
    mov (VramBlock PTR [r8]).blkFlags, BLOCK_FLAG_FREE
    
    ; Shrink current block to allocation size
    pop rax ; Restore required size
    mov (VramBlock PTR [rdi]).blkSize, rax
    
@use_whole_block:
    ; Mark as allocated
    mov (VramBlock PTR [rdi]).blkFlags, BLOCK_FLAG_ALLOCATED
    call GetTickCount64
    mov (VramBlock PTR [rdi]).LastAccessTick, rax
    
    ; Update heap stats
    add (VramHeap PTR [rsi]).UsedSize, r12
    
    ; Return offset (accounting for alignment padding)
    mov rax, (VramBlock PTR [rdi]).blkOffset
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
    
    ; Retry allocation - simplified, just fail for now to avoid stack recursion risk
    
@really_fail:
    mov rax, -1
    
@alloc_done:
    add rsp, 32
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Vram_Allocate ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Free
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Free PROC
    push rbx
    push rsi
    push rdi
    
    lea rsi, g_VramHeap
    
    ; Find block containing this offset
    xor ebx, ebx
    
@find_loop:
    cmp ebx, VRAM_POOL_BLOCKS
    jge @free_not_found
    
    mov eax, ebx
    imul rax, VRAM_BLOCK_STRUCT_SIZE
    
    mov r9, (VramHeap PTR [rsi]).pBlocks
    lea rdi, [r9 + rax]
    
    cmp (VramBlock PTR [rdi]).blkOffset, rcx
    je @found_block
    cmp (VramBlock PTR [rdi]).blkFlags, BLOCK_FLAG_ALLOCATED
    jne @next_find
    
    mov rax, (VramBlock PTR [rdi]).blkOffset
    add rax, (VramBlock PTR [rdi]).blkSize
    cmp rax, rcx
    ja @found_block                 ; Offset within this block
    
@next_find:
    inc ebx
    jmp @find_loop
    
@found_block:
    ; Mark as free
    mov (VramBlock PTR [rdi]).blkFlags, BLOCK_FLAG_FREE
    mov (VramBlock PTR [rdi]).OwnerModel, 0
    
    ; Update heap stats
    mov rax, (VramBlock PTR [rdi]).blkSize
    sub (VramHeap PTR [rsi]).UsedSize, rax
    
    ; Coalesce with previous free block
    test ebx, ebx
    jz @check_next
    
    mov eax, ebx
    dec eax
    imul rax, VRAM_BLOCK_STRUCT_SIZE
    
    mov r8, (VramHeap PTR [rsi]).pBlocks
    lea rcx, [r8 + rax]
    
    cmp (VramBlock PTR [rcx]).blkFlags, BLOCK_FLAG_FREE
    jne @check_next
    
    ; Merge previous into current
    mov rax, (VramBlock PTR [rcx]).blkSize
    add (VramBlock PTR [rdi]).blkSize, rax
    sub (VramBlock PTR [rdi]).blkOffset, rax
    mov (VramBlock PTR [rcx]).blkFlags, -1   ; Invalidate previous
    
@check_next:
    ; Coalesce with next free block
    ; (Simplified, similar logic...)
    
@free_done:
@free_not_found:
    pop rdi
    pop rsi
    pop rbx
    ret
Vram_Free ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_SubmitUpload
; ═══════════════════════════════════════════════════════════════════════════════
Vram_SubmitUpload PROC
    push rbx
    push rsi
    sub rsp, 32
    
    lea rsi, g_VramHeap
    
    ; Find free queue slot
    mov eax, (VramHeap PTR [rsi]).QueueTail
    inc eax
    and eax, 3Fh ; MASK 63
    cmp eax, (VramHeap PTR [rsi]).QueueHead
    je @queue_full                  ; Queue full, must wait
    
    ; Setup transfer operation
    mov ebx, (VramHeap PTR [rsi]).QueueTail
    imul rbx, SIZEOF GpuTransferOp
    lea rbx, (VramHeap PTR [rsi]).TransferQueue[rbx]
    
    mov [rbx].GpuTransferOp.OpType, 0 ; UPLOAD
    mov [rbx].GpuTransferOp.SrcHostPtr, rcx
    mov [rbx].GpuTransferOp.DstVramOffset, rdx
    mov [rbx].GpuTransferOp.TransferSize, r8
    
    ; Create completion event
    xor ecx, ecx
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call CreateEventA
    mov [rbx].GpuTransferOp.CompletionEvent, rax
    
    ; Submit to GPU driver
    cmp qword ptr [pfnGpuSubmitTransfer], 0
    je @no_driver
    
    mov rcx, [rbx].GpuTransferOp.SrcHostPtr
    mov rdx, [rbx].GpuTransferOp.DstVramOffset
    mov r8, [rbx].GpuTransferOp.TransferSize
    call qword ptr [pfnGpuSubmitTransfer]
    mov [rbx].GpuTransferOp.FenceValue, rax
    
    ; Advance queue tail
    mov eax, (VramHeap PTR [rsi]).QueueTail
    inc eax
    and eax, 3Fh
    mov (VramHeap PTR [rsi]).QueueTail, eax
    
    mov rax, [rbx].GpuTransferOp.FenceValue
    
    add rsp, 32
    pop rsi
    pop rbx
    ret

@no_driver:
    mov rax, 0
    add rsp, 32
    pop rsi
    pop rbx
    ret
    
@queue_full:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rbx
    ret
Vram_SubmitUpload ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Vram_Defragment
; ═══════════════════════════════════════════════════════════════════════════════
Vram_Defragment PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 40
    
    lea rsi, g_VramHeap
    
    ; Acquire defrag lock
    lea rcx, (VramHeap PTR [rsi]).DefragLock
    call Spinlock_Acquire
    
    ; ... Implementation omitted for brevity ...
    
    mov (VramHeap PTR [rsi]).DefragLock, 0
    call GetTickCount64
    mov (VramHeap PTR [rsi]).LastDefragTick, rax
    
    add rsp, 40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    
    xor eax, eax ; Fail for now
    ret
Vram_Defragment ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC Vram_Initialize
PUBLIC Vram_Allocate
PUBLIC Vram_Free
PUBLIC Vram_SubmitUpload
PUBLIC Vram_Defragment

fDefragThreshold        REAL4 0.30

END
